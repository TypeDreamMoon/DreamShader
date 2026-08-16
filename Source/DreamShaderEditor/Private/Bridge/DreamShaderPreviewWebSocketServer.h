#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"

class INetworkingWebSocket;
class IWebSocketServer;
class FJsonObject;

namespace UE::DreamShader::Editor::Private
{
	class FDreamShaderPreviewSession;
	struct FDreamShaderPreviewFrame;

	// The streaming preview's transport: one WebSocket per client, one FDreamShaderPreviewSession per
	// socket. Everything about WHAT to render lives in the session; this class only parses JSON in
	// and frames bytes out.
	//
	// Wire format (unchanged outer framing since 1.5.0, so older clients keep working):
	//   every server->client message is one WS binary frame of [u32 length][u8 tag][payload], length
	//   counting the tag. Tags: 1 = UTF-8 JSON, 2 = PNG (legacy `encoding: "png"` sessions, always
	//   preceded by a JSON `previewFrame`), 3 = raw frame (`encoding: "raw"`): a 24-byte little-endian
	//   header {u8 version, u8 format, u16 width, u16 height, u16 flags, u32 frameIndex, f32 orbitYaw,
	//   f32 orbitPitch, u32 probeLine} followed by width*height*4 bytes of RGBA8.
	//   Client->server messages are WS text frames carrying one JSON object each.
	//
	// The engine's WebSocketNetworking module sends at most ONE queued packet per client per
	// IWebSocketServer::Tick (each Tick services lws once and asks for one writable callback), and its
	// server-side Flush() is a no-op. Tick() therefore services the server again for every packet a
	// session queued this tick, so a frame is on the wire the tick it was produced instead of N
	// editor ticks later.
	class FDreamShaderPreviewWebSocketServer
	{
	public:
		FDreamShaderPreviewWebSocketServer();
		~FDreamShaderPreviewWebSocketServer();

		bool Startup(uint32 InPort = 17864);
		void Shutdown();
		void Tick();

	private:
		struct FClientState
		{
			TUniquePtr<FDreamShaderPreviewSession> Session;
			// Last `probeState` the client was told about; re-sent whenever it changes (a probe
			// re-attaches after every regeneration, possibly to a different line).
			FString LastProbeSignature;
			int32 PacketsQueuedThisTick = 0;
		};

		void HandleClientConnected(INetworkingWebSocket* Socket);
		void HandleSocketClosed(INetworkingWebSocket* Socket);
		void HandlePacket(INetworkingWebSocket* Socket, void* Data, int32 Size);
		void HandlePreviewRequest(INetworkingWebSocket* Socket, FClientState& State, const TSharedPtr<FJsonObject>& RequestObject);
		void HandlePreviewControl(INetworkingWebSocket* Socket, FClientState& State, const TSharedPtr<FJsonObject>& RequestObject);
		void HandleSetProbe(INetworkingWebSocket* Socket, FClientState& State, const TSharedPtr<FJsonObject>& RequestObject);
		void HandleClearProbe(INetworkingWebSocket* Socket, FClientState& State, const TSharedPtr<FJsonObject>& RequestObject);

		void SendFrame(INetworkingWebSocket* Socket, FClientState& State, const FDreamShaderPreviewFrame& Frame);
		void SendProbeStateIfChanged(INetworkingWebSocket* Socket, FClientState& State);
		void SendSessionError(INetworkingWebSocket* Socket, FClientState& State, const FString& Message);
		void SendJson(INetworkingWebSocket* Socket, FClientState* State, const TSharedRef<FJsonObject>& JsonObject);
		void SendTagged(INetworkingWebSocket* Socket, FClientState* State, uint8 TypeTag, const uint8* Data, int64 Length);
		void SendTagged(INetworkingWebSocket* Socket, FClientState* State, uint8 TypeTag, const uint8* HeaderData, int64 HeaderLength, const uint8* Data, int64 Length);

		TUniquePtr<IWebSocketServer> Server;
		TSet<INetworkingWebSocket*> Clients;
		TMap<INetworkingWebSocket*, FClientState> ClientStates;
		uint32 Port = 17864;
		bool bStarted = false;
	};
}
