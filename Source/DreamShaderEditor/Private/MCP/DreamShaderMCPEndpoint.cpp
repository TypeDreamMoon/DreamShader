// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "MCP/DreamShaderMCPEndpoint.h"

#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HttpPath.h"
#include "HttpRequestHandler.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "IPAddress.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace UE::DreamShader::Editor::MCP
{
	namespace
	{
		// Every string below reaches the wire, never Slate: invariant English, literal on purpose.

		FString MakeRandomHex(const int32 Guids)
		{
			FString Text;
			for (int32 Index = 0; Index < Guids; ++Index)
			{
				Text += FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
			}
			return Text;
		}

		/** A comparison whose time does not depend on where the strings first differ. */
		bool TokensMatch(const FString& Given, const FString& Expected)
		{
			if (Expected.IsEmpty() || Given.Len() != Expected.Len())
			{
				return false;
			}
			uint32 Difference = 0;
			for (int32 Index = 0; Index < Expected.Len(); ++Index)
			{
				Difference |= static_cast<uint32>(Given[Index] ^ Expected[Index]);
			}
			return Difference == 0;
		}

		FString GetPluginVersionName()
		{
			if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader")))
			{
				return Plugin->GetDescriptor().VersionName;
			}
			return FString();
		}

		FString GetEngineVersionName()
		{
			return FEngineVersion::Current().ToString(EVersionComponent::Patch);
		}

		/**
		 * Puts `(Port=<p>,BindAddress=127.0.0.1,ReuseAddressAndPort=false)` at the front of
		 * [HTTPServer.Listeners] ListenerOverrides for the one bind it scopes, and puts the section back as it was
		 * afterwards.
		 *
		 * FHttpListener::StartListening reads its bind address from that section and nowhere else -- there is no
		 * parameter for it -- and a project may set DefaultBindAddress=any for a web interface of its own. The first
		 * override that names the port wins, so ours goes first. Put back at once so that nothing of it can be saved
		 * into the user's config: the listener read it when it bound and never reads it again. The broadcast is what
		 * 5.7+ needs, where FHttpServerConfig caches the section until it hears that it changed.
		 */
		class FScopedLoopbackListenerConfig
		{
		public:
			explicit FScopedLoopbackListenerConfig(const int32 Port)
			{
				bHadPrevious = GConfig->GetArray(Section, Key, Previous, GEngineIni) > 0;
				TArray<FString> Overrides;
				Overrides.Add(FString::Printf(TEXT("(Port=%d,BindAddress=127.0.0.1,ReuseAddressAndPort=false)"), Port)); // I18N-EXEMPT: config value
				Overrides.Append(Previous);
				GConfig->SetArray(Section, Key, Overrides, GEngineIni);
				Broadcast();
			}

			~FScopedLoopbackListenerConfig()
			{
				if (bHadPrevious)
				{
					GConfig->SetArray(Section, Key, Previous, GEngineIni);
				}
				else
				{
					GConfig->RemoveKey(Section, Key, GEngineIni);
				}
				Broadcast();
			}

		private:
			static void Broadcast()
			{
				TSet<FString> Sections;
				Sections.Add(Section);
				FCoreDelegates::TSOnConfigSectionsChanged().Broadcast(GEngineIni, Sections);
			}

			static constexpr const TCHAR* Section = TEXT("HTTPServer.Listeners");
			static constexpr const TCHAR* Key = TEXT("ListenerOverrides");
			TArray<FString> Previous;
			bool bHadPrevious = false;
		};

		const TCHAR* LexVerb(const EHttpServerRequestVerbs Verb)
		{
			switch (Verb)
			{
			case EHttpServerRequestVerbs::VERB_GET:     return TEXT("GET");
			case EHttpServerRequestVerbs::VERB_POST:    return TEXT("POST");
			case EHttpServerRequestVerbs::VERB_PUT:     return TEXT("PUT");
			case EHttpServerRequestVerbs::VERB_PATCH:   return TEXT("PATCH");
			case EHttpServerRequestVerbs::VERB_DELETE:  return TEXT("DELETE");
			case EHttpServerRequestVerbs::VERB_OPTIONS: return TEXT("OPTIONS");
			default:                                    return TEXT("OTHER");
			}
		}

		FHttpExchange MakeExchange(const FHttpServerRequest& Request, const FString& Path)
		{
			FHttpExchange Exchange;
			Exchange.Verb = LexVerb(Request.Verb);
			Exchange.Path = Path;
			for (const TPair<FString, TArray<FString>>& Header : Request.Headers)
			{
				Exchange.Headers.Add(Header.Key.ToLower(), FString::Join(Header.Value, TEXT(", ")));
			}
			if (Request.Body.Num() > 0)
			{
				const FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
				Exchange.Body = FString(Converter.Length(), Converter.Get());
			}
			return Exchange;
		}

		TUniquePtr<FHttpServerResponse> MakeResponse(const FHttpReply& Reply)
		{
			const FTCHARToUTF8 Utf8(*Reply.Body);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(MoveTemp(Bytes), TEXT("application/json"));
			Response->Code = static_cast<EHttpServerResponseCodes>(Reply.Status);
			return Response;
		}

		/** Which of the routes a request came in on; the path is rebuilt from it, because the router hands the handler a path relative to its route. */
		enum class ERouteKind : uint8 { Hello, Call, Job, Cancel, Shutdown };

		uint32 ReadDiscoveryPid(const FJsonObject& Discovery)
		{
			double Pid = 0.0;
			Discovery.TryGetNumberField(TEXT("pid"), Pid);
			return Pid > 0.0 ? static_cast<uint32>(Pid) : 0u;
		}
	}

	const TCHAR* LexEndpointMode(const EEndpointMode Mode)
	{
		return Mode == EEndpointMode::Host ? TEXT("host") : TEXT("editor");
	}

	const TCHAR* LexJobState(const EJobState State)
	{
		switch (State)
		{
		case EJobState::Queued:    return TEXT("queued");
		case EJobState::Running:   return TEXT("running");
		case EJobState::Done:      return TEXT("done");
		case EJobState::Failed:    return TEXT("failed");
		case EJobState::Cancelled: return TEXT("cancelled");
		}
		return TEXT("failed");
	}

	// ------------------------------------------------------------------------------------------ the call

	FString FActionCall::GetString(const TCHAR* Name) const
	{
		FString Value;
		Args->TryGetStringField(Name, Value);
		return Value;
	}

	bool FActionCall::GetBool(const TCHAR* Name, const bool bDefault) const
	{
		bool bValue = bDefault;
		return Args->TryGetBoolField(Name, bValue) ? bValue : bDefault;
	}

	double FActionCall::GetNumber(const TCHAR* Name, const double Default) const
	{
		double Value = Default;
		return Args->TryGetNumberField(Name, Value) ? Value : Default;
	}

	bool FActionCall::Has(const TCHAR* Name) const
	{
		return Args->HasField(Name);
	}

	// -------------------------------------------------------------------------------------- the manifest

	const FManifestAction* FManifest::FindAction(const FString& Name) const
	{
		return Actions.FindByPredicate([&Name](const FManifestAction& Action) { return Action.Name.Equals(Name, ESearchCase::CaseSensitive); });
	}

	FString FManifest::GetPluginManifestPath()
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader")))
		{
			return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("DreamMCP.json"));
		}
		return FString();
	}

	bool FManifest::Parse(const FString& Text, FManifest& OutManifest, FString& OutError)
	{
		OutManifest = FManifest();
		const TSharedPtr<FJsonObject> Root = ParseJsonObject(Text);
		if (!Root.IsValid())
		{
			OutError = TEXT("the manifest is not a JSON object");
			return false;
		}

		double Protocol = 0.0;
		Root->TryGetNumberField(TEXT("protocol"), Protocol);
		OutManifest.Protocol = static_cast<int32>(Protocol);
		Root->TryGetStringField(TEXT("id"), OutManifest.Id);
		Root->TryGetStringField(TEXT("plugin"), OutManifest.Plugin);
		Root->TryGetStringField(TEXT("route"), OutManifest.Route);

		if (OutManifest.Protocol != GDreamMCPProtocol)
		{
			OutError = FString::Printf(TEXT("the manifest speaks protocol %d; this endpoint speaks %d"), OutManifest.Protocol, GDreamMCPProtocol); // I18N-EXEMPT: wire and log text
			return false;
		}
		if (OutManifest.Id.IsEmpty() || !OutManifest.Route.StartsWith(TEXT("/")) || OutManifest.Route.EndsWith(TEXT("/")))
		{
			OutError = TEXT("the manifest needs an id and a route that starts with '/' and does not end with one");
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* ActionValues = nullptr;
		if (!Root->TryGetArrayField(TEXT("actions"), ActionValues) || !ActionValues)
		{
			OutError = TEXT("the manifest has no actions array");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *ActionValues)
		{
			const TSharedPtr<FJsonObject>* ActionObject = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(ActionObject) || !ActionObject)
			{
				OutError = TEXT("an entry of actions is not an object");
				return false;
			}
			FManifestAction Action;
			(*ActionObject)->TryGetStringField(TEXT("name"), Action.Name);
			(*ActionObject)->TryGetStringField(TEXT("needs"), Action.Needs);
			(*ActionObject)->TryGetBoolField(TEXT("writes"), Action.bWrites);
			(*ActionObject)->TryGetBoolField(TEXT("long"), Action.bLong);
			const TSharedPtr<FJsonObject>* ArgsObject = nullptr;
			if ((*ActionObject)->TryGetObjectField(TEXT("args"), ArgsObject) && ArgsObject)
			{
				Action.Args = *ArgsObject;
			}
			if (Action.Name.IsEmpty() || !Action.Args.IsValid())
			{
				OutError = FString::Printf(TEXT("the action '%s' has no name or no args schema"), *Action.Name); // I18N-EXEMPT: wire and log text
				return false;
			}
			OutManifest.Actions.Add(MoveTemp(Action));
		}
		return true;
	}

	bool ValidateActionArgs(const FJsonObject& Schema, const FJsonObject& Args, FString& OutError)
	{
		const TSharedPtr<FJsonObject>* PropertiesObject = nullptr;
		const TSharedPtr<FJsonObject> NoProperties = MakeShared<FJsonObject>();
		const FJsonObject& Properties = Schema.TryGetObjectField(TEXT("properties"), PropertiesObject) && PropertiesObject
			? **PropertiesObject
			: *NoProperties;

		// FJsonObject's keys are FString up to 5.7 and a shared string from 5.8; both spell their characters with `*`.
		TArray<FString> Known;
		for (const auto& Pair : Properties.Values)
		{
			Known.Add(FString(*Pair.Key));
		}
		Known.Sort();
		TArray<TPair<FString, TSharedPtr<FJsonValue>>> Given;
		for (const auto& Pair : Args.Values)
		{
			Given.Emplace(FString(*Pair.Key), Pair.Value);
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Given)
		{
			if (!Properties.HasField(Pair.Key))
			{
				OutError = Known.IsEmpty()
					? FString::Printf(TEXT("'%s' is not an argument of this action, which takes none"), *Pair.Key) // I18N-EXEMPT: wire text
					: FString::Printf(TEXT("'%s' is not an argument of this action; it takes %s"), *Pair.Key, *FString::Join(Known, TEXT(", "))); // I18N-EXEMPT: wire text
				return false;
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* Required = nullptr;
		if (Schema.TryGetArrayField(TEXT("required"), Required) && Required)
		{
			for (const TSharedPtr<FJsonValue>& Name : *Required)
			{
				const FString RequiredName = Name.IsValid() ? Name->AsString() : FString();
				if (!RequiredName.IsEmpty() && !Args.HasField(RequiredName))
				{
					OutError = FString::Printf(TEXT("the argument '%s' is required"), *RequiredName); // I18N-EXEMPT: wire text
					return false;
				}
			}
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Given)
		{
			const TSharedPtr<FJsonObject>* PropertySchema = nullptr;
			if (!Properties.TryGetObjectField(Pair.Key, PropertySchema) || !PropertySchema)
			{
				continue;
			}
			FString Type;
			(*PropertySchema)->TryGetStringField(TEXT("type"), Type);
			const TSharedPtr<FJsonValue>& Value = Pair.Value;
			const EJson Kind = Value.IsValid() ? Value->Type : EJson::None;

			bool bTypeOk = true;
			if (Type == TEXT("string"))       { bTypeOk = Kind == EJson::String; }
			else if (Type == TEXT("boolean")) { bTypeOk = Kind == EJson::Boolean; }
			else if (Type == TEXT("number"))  { bTypeOk = Kind == EJson::Number; }
			else if (Type == TEXT("integer")) { bTypeOk = Kind == EJson::Number && FMath::IsNearlyEqual(Value->AsNumber(), FMath::RoundToDouble(Value->AsNumber())); }
			else if (Type == TEXT("object"))  { bTypeOk = Kind == EJson::Object; }
			else if (Type == TEXT("array"))   { bTypeOk = Kind == EJson::Array; }
			if (!bTypeOk)
			{
				OutError = FString::Printf(TEXT("the argument '%s' must be %s %s"), *Pair.Key, // I18N-EXEMPT: wire text
					(Type == TEXT("integer") || Type == TEXT("array") || Type == TEXT("object")) ? TEXT("an") : TEXT("a"), *Type);
				return false;
			}

			const TArray<TSharedPtr<FJsonValue>>* Enum = nullptr;
			if (Kind == EJson::String && (*PropertySchema)->TryGetArrayField(TEXT("enum"), Enum) && Enum)
			{
				TArray<FString> Allowed;
				for (const TSharedPtr<FJsonValue>& Entry : *Enum)
				{
					Allowed.Add(Entry.IsValid() ? Entry->AsString() : FString());
				}
				if (!Allowed.Contains(Value->AsString()))
				{
					OutError = FString::Printf(TEXT("the argument '%s' must be one of %s; got '%s'"), *Pair.Key, *FString::Join(Allowed, TEXT(", ")), *Value->AsString()); // I18N-EXEMPT: wire text
					return false;
				}
			}

			if (Kind == EJson::Number)
			{
				double Bound = 0.0;
				if ((*PropertySchema)->TryGetNumberField(TEXT("minimum"), Bound) && Value->AsNumber() < Bound)
				{
					OutError = FString::Printf(TEXT("the argument '%s' must be at least %g"), *Pair.Key, Bound); // I18N-EXEMPT: wire text
					return false;
				}
				if ((*PropertySchema)->TryGetNumberField(TEXT("maximum"), Bound) && Value->AsNumber() > Bound)
				{
					OutError = FString::Printf(TEXT("the argument '%s' must be at most %g"), *Pair.Key, Bound); // I18N-EXEMPT: wire text
					return false;
				}
			}
		}
		return true;
	}

	// -------------------------------------------------------------------------------------- the endpoint

	FEndpoint::FEndpoint() = default;

	FEndpoint::~FEndpoint()
	{
		Stop();
	}

	void FEndpoint::RegisterAction(const FString& Name, FActionHandler Handler)
	{
		check((Handler.Run != nullptr) != (Handler.Start != nullptr));
		Handlers.Add(Name, MoveTemp(Handler));
	}

	FString FEndpoint::GetArtifactDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamMCP"), TEXT("artifacts")));
	}

	FString FEndpoint::GetDiscoveryFilePath() const
	{
		const FString Directory = Options.DiscoveryDirectory.IsEmpty()
			? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamMCP"))
			: Options.DiscoveryDirectory;
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(Directory, Manifest.Id + TEXT(".json")));
	}

	TArray<FString> FEndpoint::GetOfferedActions() const
	{
		TArray<FString> Names;
		for (const FManifestAction& Action : Manifest.Actions)
		{
			if (Handlers.Contains(Action.Name))
			{
				Names.Add(Action.Name);
			}
		}
		return Names;
	}

	bool FEndpoint::Start(const FEndpointOptions& InOptions, FString& OutError)
	{
		check(IsInGameThread());
		Stop();
		Options = InOptions;
		if (Options.Token.IsEmpty())
		{
			Options.Token = MakeRandomHex(2);
		}

		FString ManifestText = Options.ManifestText;
		if (ManifestText.IsEmpty())
		{
			const FString ManifestPath = FManifest::GetPluginManifestPath();
			if (ManifestPath.IsEmpty() || !FFileHelper::LoadFileToString(ManifestText, *ManifestPath))
			{
				OutError = FString::Printf(TEXT("the manifest '%s' could not be read"), *ManifestPath); // I18N-EXEMPT: log text
				return false;
			}
		}
		if (!FManifest::Parse(ManifestText, Manifest, OutError))
		{
			return false;
		}
		for (const FManifestAction& Action : Manifest.Actions)
		{
			if (!Handlers.Contains(Action.Name))
			{
				UE_LOG(LogDreamShader, Warning, TEXT("DreamMCP: the manifest names the action '%s', which this build does not implement; it is not offered."), *Action.Name);
			}
		}

		Self = MakeShared<FEndpoint*, ESPMode::ThreadSafe>(this);
		StartedAtIso = FDateTime::UtcNow().ToIso8601();

		if (Options.bListen)
		{
			if (!Listen(OutError))
			{
				Self.Reset();
				return false;
			}
			BindRoutes();
		}
		else
		{
			Port = Options.Port;
		}

		if (Options.bPublishDiscovery)
		{
			FString PublishError;
			if (!PublishDiscovery(PublishError))
			{
				if (Options.Mode == EEndpointMode::Host)
				{
					OutError = PublishError;
					UnbindRoutes();
					Self.Reset();
					return false;
				}
				// An editor waits its turn: the other one may close, and the ticker tries again.
				UE_LOG(LogDreamShader, Warning, TEXT("DreamMCP: not published yet: %s. Trying again every few seconds."), *PublishError);
			}
		}

		if (Options.bRegisterTicker)
		{
			TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FEndpoint::HandleTicker), 0.0f);
		}

		bStarted = true;
		bShutdownRequested = false;
		return true;
	}

	void FEndpoint::Stop()
	{
		if (TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle.Reset();
		}
		UnbindRoutes();
		RetractDiscovery();
		Self.Reset();
		Jobs.Reset();
		bStarted = false;
	}

	bool FEndpoint::HandleTicker(float)
	{
		TickJobs();

		if (Options.bPublishDiscovery && !bPublishedDiscovery && bStarted)
		{
			const double Now = FPlatformTime::Seconds();
			if (Now >= NextPublishAttempt)
			{
				NextPublishAttempt = Now + 5.0;
				FString PublishError;
				if (PublishDiscovery(PublishError))
				{
					UE_LOG(LogDreamShader, Display, TEXT("DreamMCP: published %s."), *GetDiscoveryFilePath());
				}
			}
		}
		return true;
	}

	// ----------------------------------------------------------------------------------------- listening

	bool FEndpoint::Listen(FString& OutError)
	{
		FHttpServerModule& HttpServer = FHttpServerModule::Get();
		// A listener made while the module's switch is off is only made, not started. Turning the switch on is
		// idempotent, and is what every other user of the module does before asking for a router.
		HttpServer.StartAllListeners();

		const int32 Attempts = FMath::Max(1, Options.PortAttempts);
		for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
		{
			const int32 Candidate = Options.Port + Attempt;
			if (Candidate <= 0 || Candidate > 65535)
			{
				break;
			}

			TSharedPtr<IHttpRouter> CandidateRouter;
			{
				const FScopedLoopbackListenerConfig LoopbackOnly(Candidate);
				CandidateRouter = HttpServer.GetHttpRouter(static_cast<uint32>(Candidate), /*bFailOnBindFailure*/ true);
			}
			if (!CandidateRouter.IsValid())
			{
				continue;
			}

			// Proven, not assumed: a listener another user of the module made on this port earlier is handed back as
			// it is, bound wherever that user's config said.
			FString Exposed;
			if (!ProveLoopbackOnly(Candidate, Exposed))
			{
				UE_LOG(LogDreamShader, Error, TEXT("DreamMCP: port %d answers on %s, not only on 127.0.0.1; it is not used. Check [HTTPServer.Listeners] in the project's Engine config."), Candidate, *Exposed);
				continue;
			}

			Router = CandidateRouter;
			Port = Candidate;
			return true;
		}

		OutError = FString::Printf(TEXT("no port from %d to %d could be bound on 127.0.0.1"), Options.Port, Options.Port + Attempts - 1); // I18N-EXEMPT: log text
		return false;
	}

	bool FEndpoint::ProveLoopbackOnly(const int32 InPort, FString& OutExposedAddress)
	{
		ISocketSubsystem* const Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!Sockets)
		{
			return true;
		}

		TArray<TSharedPtr<FInternetAddr>> Adapters;
		Sockets->GetLocalAdapterAddresses(Adapters);

		struct FProbe
		{
			FSocket* Socket = nullptr;
			TSharedPtr<FInternetAddr> Target;
		};
		TArray<FProbe> Probes;
		for (const TSharedPtr<FInternetAddr>& Adapter : Adapters)
		{
			if (!Adapter.IsValid() || Adapter->GetProtocolType() != FNetworkProtocolTypes::IPv4)
			{
				continue;
			}
			uint32 Ip = 0;
			Adapter->GetIp(Ip);
			if (Ip == 0 || (Ip >> 24) == 127)
			{
				continue;
			}

			FProbe Probe;
			Probe.Target = Adapter->Clone();
			Probe.Target->SetPort(InPort);
			Probe.Socket = Sockets->CreateSocket(NAME_Stream, TEXT("DreamMCPLoopbackProof"), Probe.Target->GetProtocolType());
			if (!Probe.Socket)
			{
				continue;
			}
			Probe.Socket->SetNonBlocking(true);
			Probe.Socket->Connect(*Probe.Target);
			Probes.Add(Probe);
		}

		// A bound-to-everything listener on this machine accepts within a millisecond or two; a refused connection is
		// either reported or (Windows, which retries a refused SYN) never completes inside the window. Either way only
		// a socket that became writable AND has a peer was accepted.
		bool bExposed = false;
		const double Deadline = FPlatformTime::Seconds() + 0.3;
		while (!bExposed && Probes.Num() > 0 && FPlatformTime::Seconds() < Deadline)
		{
			for (const FProbe& Probe : Probes)
			{
				if (Probe.Socket->Wait(ESocketWaitConditions::WaitForWrite, FTimespan::FromMilliseconds(5)))
				{
					TSharedRef<FInternetAddr> Peer = Sockets->CreateInternetAddr();
					if (Probe.Socket->GetPeerAddress(*Peer))
					{
						OutExposedAddress = Probe.Target->ToString(true);
						bExposed = true;
						break;
					}
				}
			}
		}

		for (FProbe& Probe : Probes)
		{
			Probe.Socket->Close();
			Sockets->DestroySocket(Probe.Socket);
		}
		return !bExposed;
	}

	void FEndpoint::BindRoutes()
	{
		if (!Router.IsValid())
		{
			return;
		}

		const TWeakPtr<FEndpoint*, ESPMode::ThreadSafe> Weak = Self;
		const FString Route = Manifest.Route;
		const auto Bind = [this, Weak, &Route](const FString& Suffix, const ERouteKind Kind)
		{
			const FHttpRequestHandler Handler = DREAMSHADER_HTTP_REQUEST_HANDLER(
				[Weak, Route, Kind](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool
				{
					FHttpReply Reply;
					const TSharedPtr<FEndpoint*, ESPMode::ThreadSafe> Pinned = Weak.Pin();
					if (!Pinned.IsValid() || *Pinned == nullptr)
					{
						FResult Gone = FResult::MakeEngineError(TEXT("endpoint-stopped"), TEXT("The DreamShader endpoint has stopped."));
						Reply.Status = 503;
						Reply.Body = SerializeJson(Gone.ToJson());
						OnComplete(MakeResponse(Reply));
						return true;
					}

					// The router hands the handler a path relative to the route it matched; the core wants the full one.
					// Below a fixed route, anything left over is an unknown path, and the core says so. A route with a
					// parameter is matched whole -- what the router leaves over there is the parameter itself.
					FString Remainder = Request.RelativePath.GetPath();
					if (Remainder == TEXT("/"))
					{
						Remainder.Reset();
					}
					const FString* JobId = Request.PathParams.Find(TEXT("job"));
					FString Path;
					switch (Kind)
					{
					case ERouteKind::Hello:    Path = Route + TEXT("/hello") + Remainder; break;
					case ERouteKind::Call:     Path = Route + TEXT("/call") + Remainder; break;
					case ERouteKind::Shutdown: Path = Route + TEXT("/shutdown") + Remainder; break;
					case ERouteKind::Job:      Path = Route + TEXT("/jobs/") + (JobId ? *JobId : FString()); break;
					case ERouteKind::Cancel:   Path = Route + TEXT("/jobs/") + (JobId ? *JobId : FString()) + TEXT("/cancel"); break;
					}
					Reply = (*Pinned)->Handle(MakeExchange(Request, Path));
					OnComplete(MakeResponse(Reply));
					return true;
				});

			// Both verbs on every route, so a wrong one gets this protocol's 405 instead of the router's bare 404.
			const FHttpRouteHandle Handle = Router->BindRoute(
				FHttpPath(Route + Suffix),
				EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_POST,
				Handler);
			if (Handle.IsValid())
			{
				RouteHandles.Add(Handle);
			}
			else
			{
				UE_LOG(LogDreamShader, Error, TEXT("DreamMCP: the route %s%s could not be bound; another plugin may hold it."), *Route, *Suffix);
			}
		};

		Bind(TEXT("/hello"), ERouteKind::Hello);
		Bind(TEXT("/call"), ERouteKind::Call);
		Bind(TEXT("/jobs/:job"), ERouteKind::Job);
		Bind(TEXT("/jobs/:job/cancel"), ERouteKind::Cancel);
		Bind(TEXT("/shutdown"), ERouteKind::Shutdown);
	}

	void FEndpoint::UnbindRoutes()
	{
		if (Router.IsValid())
		{
			for (const FHttpRouteHandle& Handle : RouteHandles)
			{
				Router->UnbindRoute(Handle);
			}
		}
		RouteHandles.Reset();
		Router.Reset();
	}

	// ------------------------------------------------------------------------------------------ discovery

	bool FEndpoint::PublishDiscovery(FString& OutError)
	{
		const FString Path = GetDiscoveryFilePath();
		const uint32 OwnPid = FPlatformProcess::GetCurrentProcessId();

		// Written over whatever is there: the newest process for the project is the one to talk to. An editor the
		// user opens while a host runs replaces the host's file, and the server moves to the editor once the host's
		// current job is done. Deleting is the careful half -- see RetractDiscovery.
		TSharedRef<FJsonObject> Discovery = MakeShared<FJsonObject>();
		Discovery->SetNumberField(TEXT("protocol"), GDreamMCPProtocol);
		Discovery->SetStringField(TEXT("id"), Manifest.Id);
		Discovery->SetNumberField(TEXT("pid"), OwnPid);
		Discovery->SetNumberField(TEXT("port"), Port);
		Discovery->SetStringField(TEXT("token"), Options.Token);
		Discovery->SetStringField(TEXT("route"), Manifest.Route);
		Discovery->SetField(TEXT("ws"), MakeShared<FJsonValueNull>());
		Discovery->SetStringField(TEXT("mode"), LexEndpointMode(Options.Mode));
		Discovery->SetBoolField(TEXT("render"), Options.bCanRender);
		Discovery->SetStringField(TEXT("engine"), GetEngineVersionName());
		Discovery->SetStringField(TEXT("plugin"), GetPluginVersionName());
		Discovery->SetStringField(TEXT("startedAt"), StartedAtIso);

		// Beside the target, then moved over it, so a reader never sees half a file.
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		const FString Temporary = FString::Printf(TEXT("%s.%u.tmp"), *Path, OwnPid);
		if (!FFileHelper::SaveStringToFile(SerializeJson(Discovery), *Temporary, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
			|| !IFileManager::Get().Move(*Path, *Temporary, /*bReplace*/ true))
		{
			IFileManager::Get().Delete(*Temporary, false, true, true);
			OutError = FString::Printf(TEXT("%s could not be written"), *Path); // I18N-EXEMPT: log text
			return false;
		}
		bPublishedDiscovery = true;
		return true;
	}

	void FEndpoint::RetractDiscovery()
	{
		if (!bPublishedDiscovery)
		{
			return;
		}
		bPublishedDiscovery = false;

		// Only while it still names this process: an editor may have taken it over since.
		const FString Path = GetDiscoveryFilePath();
		FString Existing;
		if (FFileHelper::LoadFileToString(Existing, *Path))
		{
			const TSharedPtr<FJsonObject> Holder = ParseJsonObject(Existing);
			if (Holder.IsValid() && ReadDiscoveryPid(*Holder) == FPlatformProcess::GetCurrentProcessId())
			{
				IFileManager::Get().Delete(*Path, false, true, true);
			}
		}
	}

	// -------------------------------------------------------------------------------------------- requests

	FHttpReply FEndpoint::MakeReply(const int32 Status, const TSharedRef<FJsonObject>& Body)
	{
		FHttpReply Reply;
		Reply.Status = Status;
		Reply.Body = SerializeJson(Body);
		return Reply;
	}

	FHttpReply FEndpoint::MakeResultReply(const int32 Status, const FResult& Result)
	{
		return MakeReply(Status, Result.ToJson());
	}

	FHttpReply FEndpoint::Handle(const FHttpExchange& Exchange)
	{
		check(IsInGameThread());

		// A browser first, whatever else the request carries: a page must never reach a compile, token or not.
		if (Exchange.Headers.Contains(TEXT("origin")))
		{
			return MakeResultReply(403, FResult::MakeRefused(TEXT("origin-not-allowed"),
				TEXT("Requests that carry an Origin header are refused; the DreamShader endpoint does not serve browsers.")));
		}

		const FString* Token = Exchange.Headers.Find(TEXT("x-dream-token"));
		if (!Token || !TokensMatch(*Token, Options.Token))
		{
			return MakeResultReply(401, FResult::MakeRefused(TEXT("bad-token"),
				TEXT("The X-Dream-Token header is missing or wrong."),
				TEXT("Read the token from Saved/DreamMCP/shader.json again; the engine may have restarted with a new one.")));
		}

		FString Path = Exchange.Path;
		int32 QueryStart = INDEX_NONE;
		if (Path.FindChar(TEXT('?'), QueryStart))
		{
			Path.LeftInline(QueryStart);
		}
		const FString Prefix = Manifest.Route + TEXT("/");
		if (!Path.StartsWith(Prefix, ESearchCase::CaseSensitive))
		{
			return MakeResultReply(404, FResult::MakeRefused(TEXT("unknown-route"), FString::Printf(TEXT("%s is not a route of this endpoint."), *Path))); // I18N-EXEMPT: wire text
		}
		TArray<FString> Segments;
		Path.RightChop(Prefix.Len()).ParseIntoArray(Segments, TEXT("/"), /*bCullEmpty*/ true);

		const auto WrongVerb = [&Exchange](const TCHAR* Wanted)
		{
			return MakeResultReply(405, FResult::MakeRefused(TEXT("bad-method"),
				FString::Printf(TEXT("This route takes %s, not %s."), Wanted, *Exchange.Verb))); // I18N-EXEMPT: wire text
		};

		if (Segments.Num() == 1 && Segments[0] == TEXT("hello"))
		{
			return Exchange.Verb == TEXT("GET") ? HandleHello() : WrongVerb(TEXT("GET"));
		}
		if (Segments.Num() == 1 && Segments[0] == TEXT("call"))
		{
			return Exchange.Verb == TEXT("POST") ? HandleCall(Exchange) : WrongVerb(TEXT("POST"));
		}
		if (Segments.Num() == 1 && Segments[0] == TEXT("shutdown"))
		{
			return Exchange.Verb == TEXT("POST") ? HandleShutdown() : WrongVerb(TEXT("POST"));
		}
		if (Segments.Num() == 2 && Segments[0] == TEXT("jobs"))
		{
			return Exchange.Verb == TEXT("GET") ? HandleJob(Segments[1]) : WrongVerb(TEXT("GET"));
		}
		if (Segments.Num() == 3 && Segments[0] == TEXT("jobs") && Segments[2] == TEXT("cancel"))
		{
			return Exchange.Verb == TEXT("POST") ? HandleCancel(Segments[1]) : WrongVerb(TEXT("POST"));
		}
		return MakeResultReply(404, FResult::MakeRefused(TEXT("unknown-route"), FString::Printf(TEXT("%s is not a route of this endpoint."), *Path))); // I18N-EXEMPT: wire text
	}

	FHttpReply FEndpoint::HandleHello() const
	{
		TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetNumberField(TEXT("protocol"), GDreamMCPProtocol);
		Body->SetStringField(TEXT("id"), Manifest.Id);
		Body->SetStringField(TEXT("plugin"), GetPluginVersionName());
		Body->SetStringField(TEXT("engine"), GetEngineVersionName());
		Body->SetStringField(TEXT("mode"), LexEndpointMode(Options.Mode));
		Body->SetBoolField(TEXT("render"), Options.bCanRender);
		Body->SetBoolField(TEXT("busy"), IsBusy());
		Body->SetField(TEXT("ws"), MakeShared<FJsonValueNull>());
		Body->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
		TArray<TSharedPtr<FJsonValue>> Names;
		for (const FString& Name : GetOfferedActions())
		{
			Names.Add(MakeShared<FJsonValueString>(Name));
		}
		Body->SetArrayField(TEXT("actions"), Names);
		return MakeReply(200, Body);
	}

	FHttpReply FEndpoint::HandleCall(const FHttpExchange& Exchange)
	{
		const TSharedPtr<FJsonObject> Request = ParseJsonObject(Exchange.Body);
		if (!Request.IsValid())
		{
			return MakeResultReply(400, FResult::MakeRefused(TEXT("bad-request"),
				TEXT("The request body is not a JSON object."),
				TEXT("Send {\"id\": \"...\", \"action\": \"...\", \"args\": {...}}.")));
		}

		FString CallId;
		FString ActionName;
		Request->TryGetStringField(TEXT("id"), CallId);
		Request->TryGetStringField(TEXT("action"), ActionName);

		TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		if (Request->HasField(TEXT("args")))
		{
			const TSharedPtr<FJsonObject>* ArgsObject = nullptr;
			if (!Request->TryGetObjectField(TEXT("args"), ArgsObject) || !ArgsObject || !ArgsObject->IsValid())
			{
				return MakeResultReply(400, FResult::MakeRefused(TEXT("bad-args"), TEXT("args must be a JSON object.")));
			}
			Args = ArgsObject->ToSharedRef();
		}

		const FManifestAction* Definition = Manifest.FindAction(ActionName);
		const FActionHandler* Handler = Handlers.Find(ActionName);
		if (!Definition || !Handler)
		{
			const TArray<FString> Offered = GetOfferedActions();
			FResult Unknown = FResult::MakeRefused(TEXT("unknown-action"),
				ActionName.IsEmpty()
					? FString(TEXT("The request names no action."))
					: FString::Printf(TEXT("'%s' is not an action of DreamShader."), *ActionName), // I18N-EXEMPT: wire text
				FString::Printf(TEXT("Use one of: %s."), *FString::Join(Offered, TEXT(", ")))); // I18N-EXEMPT: wire text
			Unknown.Data = MakeShared<FJsonObject>();
			TArray<TSharedPtr<FJsonValue>> Names;
			for (const FString& Name : Offered)
			{
				Names.Add(MakeShared<FJsonValueString>(Name));
			}
			Unknown.Data->SetArrayField(TEXT("actions"), Names);
			return MakeResultReply(404, Unknown);
		}

		FString ArgsError;
		if (!ValidateActionArgs(*Definition->Args, *Args, ArgsError))
		{
			return MakeResultReply(400, FResult::MakeRefused(TEXT("bad-args"),
				FString::Printf(TEXT("%s: %s."), *ActionName, *ArgsError), // I18N-EXEMPT: wire text
				FString::Printf(TEXT("Call help to see the arguments of %s."), *ActionName))); // I18N-EXEMPT: wire text
		}

		// An action that renders, in a process that cannot: refused, in the shape the manifest promises -- a short
		// action answers with the refusal, a long one with a job that has nothing to do but be refused.
		const bool bCannotRender = Definition->Needs == TEXT("render") && !Options.bCanRender;
		const FResult RenderRefusal = FResult::MakeRefused(TEXT("render-unavailable"),
			FString::Printf(TEXT("%s renders, and this engine process does not (a host started without a renderer, or -nullrhi)."), *ActionName), // I18N-EXEMPT: wire text
			TEXT("Run it in the user's editor or in a host started in render mode (the windowless editor)."));
		if (bCannotRender && Handler->Run)
		{
			return MakeResultReply(200, RenderRefusal);
		}

		FActionCall Call;
		Call.CallId = CallId;
		Call.Action = ActionName;
		Call.Args = Args;
		Call.Mode = Options.Mode;
		Call.bCanRender = Options.bCanRender;

		if (Handler->Run)
		{
			return MakeResultReply(200, Handler->Run(Call));
		}

		// The same call id twice is a retry, not a second job.
		if (!CallId.IsEmpty())
		{
			for (const TSharedPtr<FJob>& Existing : Jobs)
			{
				if (Existing->CallId == CallId && Existing->Action == ActionName)
				{
					TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
					Body->SetStringField(TEXT("id"), CallId);
					Body->SetStringField(TEXT("job"), Existing->JobId);
					Body->SetStringField(TEXT("state"), LexJobState(Existing->State));
					return MakeReply(202, Body);
				}
			}
		}

		TUniquePtr<IJob> Runner = bCannotRender
			? TUniquePtr<IJob>(MakeUnique<FFunctionJob>([RenderRefusal](FJobContext&, FResult& OutResult) { OutResult = RenderRefusal; return true; }))
			: Handler->Start(Call);
		if (!Runner.IsValid())
		{
			return MakeResultReply(200, FResult::MakeEngineError(TEXT("job-not-created"),
				FString::Printf(TEXT("%s could not be started."), *ActionName))); // I18N-EXEMPT: wire text
		}

		TSharedPtr<FJob> Job = MakeShared<FJob>();
		Job->JobId = FString::Printf(TEXT("%s-%d"), *MakeRandomHex(1).Left(10), ++JobCounter); // I18N-EXEMPT: an id
		Job->CallId = CallId;
		Job->Action = ActionName;
		Job->Runner = MoveTemp(Runner);
		Job->Context.JobId = Job->JobId;
		Job->QueuedAt = FPlatformTime::Seconds();
		Jobs.Add(Job);

		TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("id"), CallId);
		Body->SetStringField(TEXT("job"), Job->JobId);
		Body->SetStringField(TEXT("state"), LexJobState(Job->State));
		return MakeReply(202, Body);
	}

	TSharedPtr<FEndpoint::FJob> FEndpoint::FindJob(const FString& JobId) const
	{
		for (const TSharedPtr<FJob>& Job : Jobs)
		{
			if (Job->JobId == JobId)
			{
				return Job;
			}
		}
		return nullptr;
	}

	TSharedRef<FJsonObject> FEndpoint::DescribeJob(const FJob& Job) const
	{
		TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("id"), Job.CallId);
		Body->SetStringField(TEXT("job"), Job.JobId);
		Body->SetStringField(TEXT("action"), Job.Action);
		Body->SetStringField(TEXT("state"), LexJobState(Job.State));
		if (Job.State == EJobState::Running && !Job.Context.Stage.IsEmpty())
		{
			// `{ current, total?, message? }`: what the server forwards as MCP progress.
			TSharedRef<FJsonObject> Progress = MakeShared<FJsonObject>();
			Progress->SetNumberField(TEXT("current"), Job.Context.Done);
			if (Job.Context.Total > 0)
			{
				Progress->SetNumberField(TEXT("total"), Job.Context.Total);
			}
			Progress->SetStringField(TEXT("message"), Job.Context.Stage);
			Body->SetObjectField(TEXT("progress"), Progress);
		}
		if (Job.Result.IsValid())
		{
			Body->SetObjectField(TEXT("result"), Job.Result);
		}
		return Body;
	}

	FHttpReply FEndpoint::HandleJob(const FString& JobId) const
	{
		const TSharedPtr<FJob> Job = FindJob(JobId);
		if (!Job.IsValid())
		{
			return MakeResultReply(404, FResult::MakeRefused(TEXT("unknown-job"),
				FString::Printf(TEXT("There is no job '%s' in this process."), *JobId), // I18N-EXEMPT: wire text
				TEXT("A job does not survive an engine restart; run the action again.")));
		}
		return MakeReply(200, DescribeJob(*Job));
	}

	FHttpReply FEndpoint::HandleCancel(const FString& JobId)
	{
		const TSharedPtr<FJob> Job = FindJob(JobId);
		if (!Job.IsValid())
		{
			return MakeResultReply(404, FResult::MakeRefused(TEXT("unknown-job"),
				FString::Printf(TEXT("There is no job '%s' in this process."), *JobId))); // I18N-EXEMPT: wire text
		}

		TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("job"), Job->JobId);
		if (Job->State == EJobState::Queued)
		{
			FinishJob(*Job, FResult::MakeRefused(TEXT("cancelled"), TEXT("Cancelled before it started.")));
			Body->SetBoolField(TEXT("cancelled"), true);
			Body->SetStringField(TEXT("summary"), TEXT("The job was cancelled before it started."));
		}
		else if (Job->State == EJobState::Running)
		{
			Job->Context.bCancelRequested = true;
			Body->SetBoolField(TEXT("cancelled"), false);
			Body->SetBoolField(TEXT("pending"), true);
			Body->SetStringField(TEXT("summary"), TEXT("The job stops at its next step; poll it to see the final state. A step already under way finishes first."));
		}
		else
		{
			Body->SetBoolField(TEXT("cancelled"), false);
			Body->SetStringField(TEXT("summary"), TEXT("The job had already finished; nothing was cancelled."));
		}
		Body->SetStringField(TEXT("state"), LexJobState(Job->State));
		return MakeReply(200, Body);
	}

	FHttpReply FEndpoint::HandleShutdown()
	{
		if (Options.Mode != EEndpointMode::Host)
		{
			return MakeResultReply(409, FResult::MakeRefused(TEXT("not-a-host"),
				TEXT("This endpoint is the user's editor; only a host started for DreamMCP shuts down on request.")));
		}
		bShutdownRequested = true;
		UE_LOG(LogDreamShader, Display, TEXT("DreamMCP: shutdown requested."));
		return MakeResultReply(200, FResult::MakeOk(TEXT("The host is shutting down.")));
	}

	// ------------------------------------------------------------------------------------------------ jobs

	bool FEndpoint::IsBusy() const
	{
		return Jobs.ContainsByPredicate([](const TSharedPtr<FJob>& Job)
		{
			return Job->State == EJobState::Queued || Job->State == EJobState::Running;
		});
	}

	void FEndpoint::TickJobs()
	{
		check(IsInGameThread());

		TSharedPtr<FJob> Running;
		for (const TSharedPtr<FJob>& Job : Jobs)
		{
			if (Job->State == EJobState::Running)
			{
				Running = Job;
				break;
			}
		}
		if (!Running.IsValid())
		{
			for (const TSharedPtr<FJob>& Job : Jobs)
			{
				if (Job->State == EJobState::Queued)
				{
					Running = Job;
					Running->State = EJobState::Running;
					Running->StartedAt = FPlatformTime::Seconds();
					break;
				}
			}
		}
		if (!Running.IsValid() || !Running->Runner.IsValid())
		{
			return;
		}

		// One step at least; more while they are quick and the frame's budget lasts. A windowless editor host runs at
		// a capped frame rate, and a `check` of a whole tree is one step per file.
		const double FrameStart = FPlatformTime::Seconds();
		do
		{
			Running->Context.bWaiting = false;
			FResult Result;
			if (Running->Runner->Step(Running->Context, Result))
			{
				FinishJob(*Running, Result);
				return;
			}
		}
		while (!Running->Context.bWaiting && FPlatformTime::Seconds() - FrameStart < Options.StepBudgetSeconds);
	}

	void FEndpoint::FinishJob(FJob& Job, const FResult& Result)
	{
		if (Job.Context.bCancelRequested && Result.Reason == TEXT("cancelled"))
		{
			Job.State = EJobState::Cancelled;
		}
		else if (Job.State == EJobState::Queued && Result.Reason == TEXT("cancelled"))
		{
			Job.State = EJobState::Cancelled;
		}
		else
		{
			Job.State = Result.Outcome == EOutcome::EngineError ? EJobState::Failed : EJobState::Done;
		}
		Job.Result = Result.ToJson();
		Job.Runner.Reset();
		Job.FinishedAt = FPlatformTime::Seconds();
		UE_LOG(LogDreamShader, Display, TEXT("DreamMCP: %s %s %s (%s) in %.2f s: %s"),
			*Job.Action, *Job.JobId, LexJobState(Job.State), LexOutcome(Result.Outcome),
			Job.StartedAt > 0.0 ? Job.FinishedAt - Job.StartedAt : 0.0, *Result.Summary);
		PruneFinishedJobs();
	}

	void FEndpoint::PruneFinishedJobs()
	{
		// The last 64 finished jobs stay readable; a client polls within seconds, not hours.
		constexpr int32 KeepFinished = 64;
		int32 Finished = 0;
		for (const TSharedPtr<FJob>& Job : Jobs)
		{
			Finished += (Job->State == EJobState::Done || Job->State == EJobState::Failed || Job->State == EJobState::Cancelled) ? 1 : 0;
		}
		for (int32 Index = 0; Index < Jobs.Num() && Finished > KeepFinished;)
		{
			const EJobState State = Jobs[Index]->State;
			if (State == EJobState::Done || State == EJobState::Failed || State == EJobState::Cancelled)
			{
				Jobs.RemoveAt(Index);
				--Finished;
				continue;
			}
			++Index;
		}
	}
}
