#include "Bridge/DreamShaderPreviewWebSocketServer.h"

#include "DreamShaderModule.h"
#include "Diagnostics/DreamShaderTextWireUtils.h"
#include "Preview/DreamShaderPreviewRenderer.h"
#include "Preview/DreamShaderPreviewSession.h"

#include "Dom/JsonObject.h"
#include "IWebSocketNetworkingModule.h"
#include "IWebSocketServer.h"
#include "INetworkingWebSocket.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "WebSocketNetworkingDelegates.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		static constexpr uint32 DefaultPreviewWebSocketPort = 17864;

		// Wire-level type tags -- must match the decoders in the VS Code extension's webview and the
		// legacy Node client exactly (see the class comment in the header for the framing).
		static constexpr uint8 PreviewWireTypeJson = 1;
		static constexpr uint8 PreviewWireTypeBinary = 2;
		static constexpr uint8 PreviewWireTypeRawFrame = 3;

		static constexpr uint8 RawFrameHeaderVersion = 1;
		static constexpr uint8 RawFrameFormatRGBA8 = 1;
		static constexpr int32 RawFrameHeaderSize = 24;

		// Bound on the extra server services per Tick used to drain queued packets (a raw frame is
		// one packet; a legacy PNG frame is two; a probe-state change adds one).
		static constexpr int32 MaxExtraServiceTicks = 4;

		FString GetStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
		{
			FString Value;
			if (Object.IsValid())
			{
				Object->TryGetStringField(FieldName, Value);
			}
			return Value;
		}

		void SetOptionalStringField(const TSharedRef<FJsonObject>& Object, const TCHAR* FieldName, const FString& Value)
		{
			if (!Value.IsEmpty())
			{
				Object->SetStringField(FieldName, Value);
			}
		}

		double GetFrameIntervalSeconds(const TSharedPtr<FJsonObject>& Object, const double DefaultFrameRate)
		{
			double FrameRate = DefaultFrameRate;
			if (Object.IsValid())
			{
				Object->TryGetNumberField(TEXT("frameRate"), FrameRate);
			}
			if (FrameRate <= 0.0)
			{
				return 0.0;
			}
			return 1.0 / FMath::Clamp(FrameRate, 0.25, 60.0);
		}

		EDreamShaderPreviewFrameEncoding ParseEncoding(const TSharedPtr<FJsonObject>& Object)
		{
			// Absent = a client that predates the raw path; it gets what it always got.
			const FString Encoding = GetStringField(Object, TEXT("encoding"));
			return Encoding.Equals(TEXT("raw"), ESearchCase::IgnoreCase)
				? EDreamShaderPreviewFrameEncoding::RawRGBA8
				: EDreamShaderPreviewFrameEncoding::Png;
		}

		const TCHAR* EncodingToWireName(const EDreamShaderPreviewFrameEncoding Encoding)
		{
			return Encoding == EDreamShaderPreviewFrameEncoding::RawRGBA8 ? TEXT("raw") : TEXT("png");
		}

		void WriteLE16(uint8* Dst, const uint16 Value)
		{
			Dst[0] = static_cast<uint8>(Value & 0xFF);
			Dst[1] = static_cast<uint8>((Value >> 8) & 0xFF);
		}

		void WriteLE32(uint8* Dst, const uint32 Value)
		{
			Dst[0] = static_cast<uint8>(Value & 0xFF);
			Dst[1] = static_cast<uint8>((Value >> 8) & 0xFF);
			Dst[2] = static_cast<uint8>((Value >> 16) & 0xFF);
			Dst[3] = static_cast<uint8>((Value >> 24) & 0xFF);
		}

		void WriteLEFloat(uint8* Dst, const float Value)
		{
			uint32 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(uint32));
			WriteLE32(Dst, Bits);
		}
	}

	FDreamShaderPreviewWebSocketServer::FDreamShaderPreviewWebSocketServer() = default;

	FDreamShaderPreviewWebSocketServer::~FDreamShaderPreviewWebSocketServer()
	{
		Shutdown();
	}

	bool FDreamShaderPreviewWebSocketServer::Startup(uint32 InPort)
	{
		if (bStarted)
		{
			return true;
		}

		Port = InPort > 0 ? InPort : DefaultPreviewWebSocketPort;
		IWebSocketNetworkingModule* WebSocketModule = FModuleManager::LoadModulePtr<IWebSocketNetworkingModule>(TEXT("WebSocketNetworking"));
		if (!WebSocketModule)
		{
			UE_LOG(LogDreamShader, Warning, TEXT("DreamShader preview WebSocket server could not load WebSocketNetworking."));
			return false;
		}

		Server = WebSocketModule->CreateServer();
		if (!Server)
		{
			UE_LOG(LogDreamShader, Warning, TEXT("DreamShader preview WebSocket server could not be created."));
			return false;
		}

		Server->SetFilterConnectionCallback(FWebSocketFilterConnectionCallback::CreateLambda([](FString Origin, FString ClientIP)
		{
			// The Origin is deliberately not checked: the VS Code webview connects with a
			// vscode-webview:// origin, browsers with theirs. Loopback is the boundary.
			(void)Origin;
			return ClientIP == TEXT("127.0.0.1") || ClientIP == TEXT("localhost")
				? EWebsocketConnectionFilterResult::ConnectionAccepted
				: EWebsocketConnectionFilterResult::ConnectionRefused;
		}));

		if (!Server->Init(Port, FWebSocketClientConnectedCallBack::CreateRaw(this, &FDreamShaderPreviewWebSocketServer::HandleClientConnected), TEXT("127.0.0.1")))
		{
			UE_LOG(LogDreamShader, Warning, TEXT("DreamShader preview WebSocket server failed to listen on 127.0.0.1:%u."), Port);
			Server.Reset();
			return false;
		}

		bStarted = true;
		UE_LOG(LogDreamShader, Display, TEXT("DreamShader preview WebSocket server listening on 127.0.0.1:%u."), Port);
		return true;
	}

	void FDreamShaderPreviewWebSocketServer::Shutdown()
	{
		Clients.Reset();
		ClientStates.Reset();
		Server.Reset();
		bStarted = false;
	}

	void FDreamShaderPreviewWebSocketServer::Tick()
	{
		if (!bStarted || !Server)
		{
			return;
		}

		Server->Tick();
		TArray<INetworkingWebSocket*> ClientSnapshot = Clients.Array();
		const double NowSeconds = FPlatformTime::Seconds();
		int32 MaxPacketsQueued = 0;
		for (INetworkingWebSocket* Client : ClientSnapshot)
		{
			if (!Client || !Clients.Contains(Client))
			{
				continue;
			}
			Client->Tick();
			FClientState* State = ClientStates.Find(Client);
			if (!State || !State->Session.IsValid())
			{
				continue;
			}
			State->PacketsQueuedThisTick = 0;

			FDreamShaderPreviewFrame Frame;
			FString Error;
			if (State->Session->Tick(NowSeconds, Frame, Error))
			{
				SendFrame(Client, *State, Frame);
			}
			else if (!Error.IsEmpty())
			{
				SendSessionError(Client, *State, Error);
			}
			SendProbeStateIfChanged(Client, *State);
			MaxPacketsQueued = FMath::Max(MaxPacketsQueued, State->PacketsQueuedThisTick);
		}

		// Drain what the sessions just queued (see the header on why one Tick moves one packet).
		for (int32 Extra = 0; Extra < FMath::Min(MaxPacketsQueued, MaxExtraServiceTicks); ++Extra)
		{
			Server->Tick();
		}
	}

	void FDreamShaderPreviewWebSocketServer::HandleClientConnected(INetworkingWebSocket* Socket)
	{
		if (!Socket)
		{
			return;
		}

		Clients.Add(Socket);
		ClientStates.FindOrAdd(Socket);
		Socket->SetReceiveCallBack(FWebSocketPacketReceivedCallBack::CreateLambda([this, Socket](void* Data, int32 Size)
		{
			HandlePacket(Socket, Data, Size);
		}));
		Socket->SetSocketClosedCallBack(FWebSocketInfoCallBack::CreateRaw(this, &FDreamShaderPreviewWebSocketServer::HandleSocketClosed, Socket));
		UE_LOG(LogDreamShader, Display, TEXT("DreamShader preview WebSocket client connected: %s"), *Socket->RemoteEndPoint(true));
	}

	void FDreamShaderPreviewWebSocketServer::HandleSocketClosed(INetworkingWebSocket* Socket)
	{
		Clients.Remove(Socket);
		ClientStates.Remove(Socket);
	}

	void FDreamShaderPreviewWebSocketServer::HandlePacket(INetworkingWebSocket* Socket, void* Data, int32 Size)
	{
		if (!Socket || !Data || Size <= 0)
		{
			return;
		}

		FClientState* State = ClientStates.Find(Socket);
		if (!State)
		{
			return;
		}

		FUTF8ToTCHAR TextConverter(reinterpret_cast<const ANSICHAR*>(Data), Size);
		const FString Text(TextConverter.Length(), TextConverter.Get());
		TSharedPtr<FJsonObject> RequestObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, RequestObject) || !RequestObject.IsValid())
		{
			TSharedRef<FJsonObject> ErrorObject = MakeShared<FJsonObject>();
			ErrorObject->SetStringField(TEXT("type"), TEXT("previewResult"));
			ErrorObject->SetStringField(TEXT("status"), TEXT("error"));
			ErrorObject->SetStringField(TEXT("message"), TEXT("Invalid DreamShader preview request JSON.")); // I18N-EXEMPT: wire literal
			ErrorObject->SetStringField(TEXT("updatedAtUtc"), FDateTime::UtcNow().ToIso8601());
			SendJson(Socket, State, ErrorObject);
			return;
		}

		const FString Type = GetStringField(RequestObject, TEXT("type"));
		const FString Action = GetStringField(RequestObject, TEXT("action"));
		if (Type.Equals(TEXT("previewMaterial"), ESearchCase::IgnoreCase) || Action.Equals(TEXT("previewMaterial"), ESearchCase::IgnoreCase))
		{
			HandlePreviewRequest(Socket, *State, RequestObject);
		}
		else if (Type.Equals(TEXT("previewControl"), ESearchCase::IgnoreCase))
		{
			HandlePreviewControl(Socket, *State, RequestObject);
		}
		else if (Type.Equals(TEXT("setProbe"), ESearchCase::IgnoreCase))
		{
			HandleSetProbe(Socket, *State, RequestObject);
		}
		else if (Type.Equals(TEXT("clearProbe"), ESearchCase::IgnoreCase))
		{
			HandleClearProbe(Socket, *State, RequestObject);
		}
	}

	void FDreamShaderPreviewWebSocketServer::HandlePreviewRequest(INetworkingWebSocket* Socket, FClientState& State, const TSharedPtr<FJsonObject>& RequestObject)
	{
		FDreamShaderPreviewRequest PreviewRequest;
		PreviewRequest.SourceFilePath = GetStringField(RequestObject, TEXT("sourceFile"));
		PreviewRequest.Mesh = GetStringField(RequestObject, TEXT("mesh"));

		double Width = PreviewRequest.Width;
		double Height = PreviewRequest.Height;
		RequestObject->TryGetNumberField(TEXT("width"), Width);
		RequestObject->TryGetNumberField(TEXT("height"), Height);
		PreviewRequest.Width = FMath::Clamp(FMath::RoundToInt(Width), 64, 2048);
		PreviewRequest.Height = FMath::Clamp(FMath::RoundToInt(Height), 64, 2048);

		// Optional -- absent (a legacy client, or a fresh session) falls back to the request
		// defaults, which match USceneThumbnailInfo's baseline framing.
		double OrbitYaw = PreviewRequest.OrbitYaw;
		double OrbitPitch = PreviewRequest.OrbitPitch;
		RequestObject->TryGetNumberField(TEXT("orbitYaw"), OrbitYaw);
		RequestObject->TryGetNumberField(TEXT("orbitPitch"), OrbitPitch);
		PreviewRequest.OrbitYaw = static_cast<float>(OrbitYaw);
		PreviewRequest.OrbitPitch = static_cast<float>(OrbitPitch);

		// A re-request for a mesh/camera change, or a save the bridge's auto-compile already picked
		// up, does not need another generation; only an explicit refresh asks for one.
		bool bForce = false;
		RequestObject->TryGetBoolField(TEXT("force"), bForce);
		PreviewRequest.bForceRecompile = bForce;

		bool bStream = true;
		RequestObject->TryGetBoolField(TEXT("stream"), bStream);
		const EDreamShaderPreviewFrameEncoding Encoding = ParseEncoding(RequestObject);
		const double FrameIntervalSeconds = GetFrameIntervalSeconds(RequestObject, 2.0);
		const FString RequestId = GetStringField(RequestObject, TEXT("requestId"));

		if (!State.Session.IsValid())
		{
			State.Session = MakeUnique<FDreamShaderPreviewSession>();
		}

		FDreamShaderPreviewResult PreviewResult;
		const bool bReady = State.Session->BeginPreview(PreviewRequest, RequestId, Encoding, FrameIntervalSeconds, bStream, PreviewResult);
		if (bReady && !PreviewResult.Message.IsEmpty())
		{
			PreviewResult.Message = FText::FromString(FString::Printf(TEXT("Streaming preview for %s."), *PreviewResult.AssetPath)); // I18N-EXEMPT: wire literal
		}

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetStringField(TEXT("type"), TEXT("previewResult"));
		SetOptionalStringField(ResultObject, TEXT("requestId"), RequestId);
		ResultObject->SetStringField(TEXT("status"), bReady ? TEXT("ready") : TEXT("error"));
		ResultObject->SetStringField(TEXT("sourceFile"), PreviewResult.SourceFilePath);
		ResultObject->SetStringField(TEXT("assetPath"), PreviewResult.AssetPath);
		ResultObject->SetStringField(TEXT("imagePath"), PreviewResult.ImagePath);
		ResultObject->SetStringField(TEXT("mesh"), PreviewResult.Mesh);
		ResultObject->SetStringField(TEXT("encoding"), EncodingToWireName(Encoding));
		ResultObject->SetStringField(TEXT("message"), ToInvariantWireString(PreviewResult.Message));
		ResultObject->SetStringField(TEXT("updatedAtUtc"), FDateTime::UtcNow().ToIso8601());
		SendJson(Socket, &State, ResultObject);

		if (bReady)
		{
			// The raw path streams its first frame from Tick like every other one; only the legacy
			// path has a file to hand over immediately.
			if (Encoding == EDreamShaderPreviewFrameEncoding::Png && !PreviewResult.ImagePath.IsEmpty())
			{
				TArray<uint8> FirstFramePngData;
				if (FFileHelper::LoadFileToArray(FirstFramePngData, *PreviewResult.ImagePath) && !FirstFramePngData.IsEmpty())
				{
					SendTagged(Socket, &State, PreviewWireTypeBinary, FirstFramePngData.GetData(), FirstFramePngData.Num());
				}
			}
			// The probe may have re-attached (or moved) during the compile; tell the client now
			// rather than on the next tick's poll.
			State.LastProbeSignature.Reset();
			SendProbeStateIfChanged(Socket, State);
			UE_LOG(LogDreamShader, Display, TEXT("DreamShader preview WebSocket: %s"), *ToInvariantWireString(PreviewResult.Message));
		}
		else
		{
			UE_LOG(LogDreamShader, Error, TEXT("DreamShader preview WebSocket: %s"), *ToInvariantWireString(PreviewResult.Message));
		}
	}

	void FDreamShaderPreviewWebSocketServer::HandlePreviewControl(INetworkingWebSocket* Socket, FClientState& State, const TSharedPtr<FJsonObject>& RequestObject)
	{
		(void)Socket;
		if (!State.Session.IsValid())
		{
			return;
		}
		FDreamShaderPreviewSession& Session = *State.Session;

		const FString RequestId = GetStringField(RequestObject, TEXT("requestId"));
		if (!RequestId.IsEmpty() && !Session.GetRequestId().IsEmpty() && RequestId != Session.GetRequestId())
		{
			return;
		}

		bool bStream = Session.IsStreaming();
		RequestObject->TryGetBoolField(TEXT("stream"), bStream);
		// A control message without frameRate keeps the session's current rate.
		const double CurrentInterval = Session.GetFrameIntervalSeconds();
		const double CurrentRate = CurrentInterval > 0.0 ? 1.0 / CurrentInterval : 0.0;
		Session.SetStreaming(bStream, GetFrameIntervalSeconds(RequestObject, CurrentRate));

		// Missing fields keep the current values, so a plain frame acknowledgement cannot snap the
		// camera or the size back.
		double OrbitYaw = Session.GetOrbitYaw();
		double OrbitPitch = Session.GetOrbitPitch();
		RequestObject->TryGetNumberField(TEXT("orbitYaw"), OrbitYaw);
		RequestObject->TryGetNumberField(TEXT("orbitPitch"), OrbitPitch);
		Session.SetOrbit(static_cast<float>(OrbitYaw), static_cast<float>(OrbitPitch));

		double Width = Session.GetWidth();
		double Height = Session.GetHeight();
		RequestObject->TryGetNumberField(TEXT("width"), Width);
		RequestObject->TryGetNumberField(TEXT("height"), Height);
		Session.SetViewportSize(FMath::RoundToInt(Width), FMath::RoundToInt(Height));

		FString Mesh;
		if (RequestObject->TryGetStringField(TEXT("mesh"), Mesh))
		{
			Session.SetMesh(Mesh);
		}

		double AckFrameIndex = -1.0;
		if (RequestObject->TryGetNumberField(TEXT("ackFrameIndex"), AckFrameIndex))
		{
			Session.AckFrame(FMath::RoundToInt(AckFrameIndex));
		}
		else if (Session.IsStreaming())
		{
			// A control ping without an ack (rate/camera change) still counts as "the client is
			// alive and consuming" -- matches the pre-raw behaviour.
			Session.AckFrame(-1);
		}
	}

	void FDreamShaderPreviewWebSocketServer::HandleSetProbe(INetworkingWebSocket* Socket, FClientState& State, const TSharedPtr<FJsonObject>& RequestObject)
	{
		if (!State.Session.IsValid())
		{
			return;
		}
		double Line = 0.0;
		RequestObject->TryGetNumberField(TEXT("line"), Line);
		const FString Name = GetStringField(RequestObject, TEXT("name"));

		FString Error;
		State.Session->SetProbe(FMath::RoundToInt(Line), Name, Error);
		// Success or not, the client learns the outcome through probeState (pending carries the
		// reason), so a breakpoint set before the first compile is not reported as a failure.
		State.LastProbeSignature.Reset();
		SendProbeStateIfChanged(Socket, State);
	}

	void FDreamShaderPreviewWebSocketServer::HandleClearProbe(INetworkingWebSocket* Socket, FClientState& State, const TSharedPtr<FJsonObject>& RequestObject)
	{
		(void)RequestObject;
		if (!State.Session.IsValid())
		{
			return;
		}
		State.Session->ClearProbe();
		State.LastProbeSignature.Reset();
		SendProbeStateIfChanged(Socket, State);
	}

	void FDreamShaderPreviewWebSocketServer::SendProbeStateIfChanged(INetworkingWebSocket* Socket, FClientState& State)
	{
		if (!State.Session.IsValid())
		{
			return;
		}
		const FDreamShaderProbePreview& Probe = State.Session->GetProbePreview();

		FString Status;
		int32 Line = 0;
		int32 Column = 0;
		FString Name;
		int32 ComponentCount = 0;
		FString Message;
		if (Probe.IsActive())
		{
			const FDreamShaderProbePreview::FResolvedProbe& Resolved = Probe.GetResolvedProbe().GetValue();
			Status = TEXT("attached");
			Line = Resolved.Line;
			Column = Resolved.Column;
			Name = Resolved.Name;
			ComponentCount = Resolved.ComponentCount;
		}
		else if (Probe.IsRequested())
		{
			Status = TEXT("pending");
			Message = Probe.GetLastError();
		}
		else
		{
			Status = TEXT("cleared");
		}

		const FString Signature = FString::Printf(TEXT("%s|%d|%d|%s|%d|%s"), *Status, Line, Column, *Name, ComponentCount, *Message);
		if (Signature == State.LastProbeSignature)
		{
			return;
		}
		State.LastProbeSignature = Signature;

		TSharedRef<FJsonObject> ProbeObject = MakeShared<FJsonObject>();
		ProbeObject->SetStringField(TEXT("type"), TEXT("probeState"));
		SetOptionalStringField(ProbeObject, TEXT("requestId"), State.Session->GetRequestId());
		ProbeObject->SetStringField(TEXT("sourceFile"), State.Session->GetSourceFilePath());
		ProbeObject->SetStringField(TEXT("status"), Status);
		if (Line > 0)
		{
			ProbeObject->SetNumberField(TEXT("line"), Line);
			ProbeObject->SetNumberField(TEXT("column"), Column);
			ProbeObject->SetStringField(TEXT("name"), Name);
			ProbeObject->SetNumberField(TEXT("componentCount"), ComponentCount);
		}
		SetOptionalStringField(ProbeObject, TEXT("message"), Message);
		SendJson(Socket, &State, ProbeObject);
	}

	void FDreamShaderPreviewWebSocketServer::SendSessionError(INetworkingWebSocket* Socket, FClientState& State, const FString& Message)
	{
		TSharedRef<FJsonObject> ErrorObject = MakeShared<FJsonObject>();
		ErrorObject->SetStringField(TEXT("type"), TEXT("previewResult"));
		if (State.Session.IsValid())
		{
			SetOptionalStringField(ErrorObject, TEXT("requestId"), State.Session->GetRequestId());
			ErrorObject->SetStringField(TEXT("sourceFile"), State.Session->GetSourceFilePath());
			ErrorObject->SetStringField(TEXT("assetPath"), State.Session->GetAssetPath());
			ErrorObject->SetStringField(TEXT("mesh"), State.Session->GetMesh());
		}
		ErrorObject->SetStringField(TEXT("status"), TEXT("error"));
		ErrorObject->SetStringField(TEXT("message"), Message); // already invariant: the session's error channel is FString
		ErrorObject->SetStringField(TEXT("updatedAtUtc"), FDateTime::UtcNow().ToIso8601());
		SendJson(Socket, &State, ErrorObject);
	}

	void FDreamShaderPreviewWebSocketServer::SendFrame(INetworkingWebSocket* Socket, FClientState& State, const FDreamShaderPreviewFrame& Frame)
	{
		if (Frame.Encoding == EDreamShaderPreviewFrameEncoding::Png)
		{
			// Legacy pair: metadata JSON, then the PNG as the next tagged message; the client
			// correlates them by arrival order on this one connection.
			TSharedRef<FJsonObject> FrameObject = MakeShared<FJsonObject>();
			FrameObject->SetStringField(TEXT("type"), TEXT("previewFrame"));
			SetOptionalStringField(FrameObject, TEXT("requestId"), State.Session->GetRequestId());
			FrameObject->SetStringField(TEXT("sourceFile"), State.Session->GetSourceFilePath());
			FrameObject->SetStringField(TEXT("assetPath"), State.Session->GetAssetPath());
			FrameObject->SetStringField(TEXT("mesh"), State.Session->GetMesh());
			FrameObject->SetNumberField(TEXT("frameIndex"), Frame.FrameIndex);
			FrameObject->SetNumberField(TEXT("flags"), Frame.Flags);
			FrameObject->SetStringField(TEXT("updatedAtUtc"), FDateTime::UtcNow().ToIso8601());
			SendJson(Socket, &State, FrameObject);
			SendTagged(Socket, &State, PreviewWireTypeBinary, Frame.Payload.GetData(), Frame.Payload.Num());
			return;
		}

		uint8 Header[RawFrameHeaderSize];
		Header[0] = RawFrameHeaderVersion;
		Header[1] = RawFrameFormatRGBA8;
		WriteLE16(Header + 2, static_cast<uint16>(FMath::Clamp(Frame.Width, 0, 65535)));
		WriteLE16(Header + 4, static_cast<uint16>(FMath::Clamp(Frame.Height, 0, 65535)));
		WriteLE16(Header + 6, static_cast<uint16>(Frame.Flags & 0xFFFF));
		WriteLE32(Header + 8, static_cast<uint32>(Frame.FrameIndex));
		WriteLEFloat(Header + 12, Frame.OrbitYaw);
		WriteLEFloat(Header + 16, Frame.OrbitPitch);
		WriteLE32(Header + 20, static_cast<uint32>(FMath::Max(0, Frame.ProbeLine)));
		SendTagged(Socket, &State, PreviewWireTypeRawFrame, Header, RawFrameHeaderSize, Frame.Payload.GetData(), Frame.Payload.Num());
	}

	void FDreamShaderPreviewWebSocketServer::SendJson(INetworkingWebSocket* Socket, FClientState* State, const TSharedRef<FJsonObject>& JsonObject)
	{
		if (!Socket)
		{
			return;
		}

		FString OutputText;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputText);
		if (!FJsonSerializer::Serialize(JsonObject, Writer))
		{
			return;
		}

		FTCHARToUTF8 Converter(*OutputText);
		SendTagged(Socket, State, PreviewWireTypeJson, reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
	}

	void FDreamShaderPreviewWebSocketServer::SendTagged(INetworkingWebSocket* Socket, FClientState* State, const uint8 TypeTag, const uint8* Data, const int64 Length)
	{
		SendTagged(Socket, State, TypeTag, nullptr, 0, Data, Length);
	}

	void FDreamShaderPreviewWebSocketServer::SendTagged(INetworkingWebSocket* Socket, FClientState* State, const uint8 TypeTag, const uint8* HeaderData, const int64 HeaderLength, const uint8* Data, const int64 Length)
	{
		if (!Socket)
		{
			return;
		}

		// One buffer, one Send: [tag][header][payload]. Send() itself copies once more (LWS_PRE +
		// optional size prefix), which is the module's doing; nothing here copies the pixels twice.
		TArray<uint8> Tagged;
		Tagged.Reserve(static_cast<int32>(1 + HeaderLength + Length));
		Tagged.Add(TypeTag);
		if (HeaderData && HeaderLength > 0)
		{
			Tagged.Append(HeaderData, static_cast<int32>(HeaderLength));
		}
		if (Data && Length > 0)
		{
			Tagged.Append(Data, static_cast<int32>(Length));
		}

		// bPrependSize=true: the WS opcode is always binary on this module, so the explicit length +
		// tag is how the client tells message kinds apart (see the header comment).
		Socket->Send(Tagged.GetData(), Tagged.Num(), true);
		if (State)
		{
			++State->PacketsQueuedThisTick;
		}
	}
}
