// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The DreamMCP endpoint: DreamShader's half of the DreamMCP protocol (protocol 1). An MCP server reaches
// DreamShader in a running engine through it -- the user's own editor, or the DreamShaderHost commandlet the
// server starts when no editor has the project open.
//
//   GET  <route>/hello              who is answering: protocol, plugin, engine, mode, render, busy, actions
//   POST <route>/call               run an action: { "id", "action", "args" }
//   GET  <route>/jobs/<job>         a long action's state, and its result once it has one
//   POST <route>/jobs/<job>/cancel  ask a long action to stop
//   POST <route>/shutdown           a host only: exit cleanly
//
// What it promises, and where each promise is kept:
//
//   * Loopback only. The engine's HTTPServer binds where [HTTPServer.Listeners] says, and a project may say
//     `any`. Start() installs a per-port override naming 127.0.0.1 for the one bind it makes, removes it again,
//     and then proves the result: a connection to the port through every non-loopback address of the machine
//     must be refused. An endpoint that fails the proof unbinds its routes and does not publish itself.
//   * Every request carries the token in X-Dream-Token, or it is 401. A request with an Origin header is a
//     browser and is 403 before anything else is looked at.
//   * The game thread answers. Requests and jobs both run there -- the compiler creates UObjects -- so a long
//     action is a job: the request answers 202 at once and the job runs one step per tick afterwards, which is
//     what keeps `hello` answerable while a `check` of the whole tree is under way.
//   * The manifest (Resources/DreamMCP.json) is the one list of actions: hello reports it, /call validates the
//     arguments against its JSON Schema, and an action with no handler here is not offered at all.
//
// Transport-agnostic at the core: Handle() takes an FHttpExchange and returns an FHttpReply, and the HTTP routes
// are a thin adapter over it, so the automation tests drive the whole protocol without a socket and one test
// drives it through one.

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "HttpRouteHandle.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

#include <atomic>

#include "MCP/DreamShaderMCPResult.h"

class IHttpRouter;

namespace UE::DreamShader::Editor::MCP
{
	/** The protocol version this endpoint speaks. */
	inline constexpr int32 GDreamMCPProtocol = 1;

	/** `editor`: the user's editor. `host`: the DreamShaderHost commandlet. */
	enum class EEndpointMode : uint8
	{
		Editor,
		Host,
	};

	const TCHAR* LexEndpointMode(EEndpointMode Mode);

	/** One request, as the core sees it. Header names are lower case. */
	struct FHttpExchange
	{
		FString Verb;
		/** The full request path: `/dream/shader/jobs/abc`. */
		FString Path;
		TMap<FString, FString> Headers;
		FString Body;
	};

	struct FHttpReply
	{
		int32 Status = 200;
		FString Body;
	};

	/** `queued` / `running` / `done` / `failed` / `cancelled`. */
	enum class EJobState : uint8
	{
		Queued,
		Running,
		/** Finished with a result: `ok`, `source-error` or `refused`. */
		Done,
		/** Finished with an `engine-error` result. */
		Failed,
		/** Stopped on request before it finished. */
		Cancelled,
	};

	const TCHAR* LexJobState(EJobState State);

	/** What a job step sees. */
	class FJobContext
	{
	public:
		const FString& GetJobId() const { return JobId; }
		bool IsCancelRequested() const { return bCancelRequested; }

		/** `{ stage, done, total }` on the wire; total 0 means "not countable". */
		void SetProgress(const FString& InStage, int32 InDone, int32 InTotal)
		{
			Stage = InStage;
			Done = InDone;
			Total = InTotal;
		}

		/**
		 * This step only looked whether something outside the job is done yet (a shader compiling on a worker): the
		 * endpoint stops stepping the job for this frame instead of asking again at once.
		 */
		void SetWaiting() { bWaiting = true; }

	private:
		friend class FEndpoint;
		FString JobId;
		bool bCancelRequested = false;
		bool bWaiting = false;
		FString Stage;
		int32 Done = 0;
		int32 Total = 0;
	};

	/** One long action, stepped by the endpoint on the game thread, one Step per tick. */
	class IJob
	{
	public:
		virtual ~IJob() = default;

		/**
		 * One bounded piece of work. Returns true when the job is finished, with OutResult its result. A job that sees
		 * IsCancelRequested() and stops answers true with a result whose Reason is `cancelled`.
		 */
		virtual bool Step(FJobContext& Context, FResult& OutResult) = 0;
	};

	/** A job written as one function, called once per tick until it answers true. */
	class FFunctionJob final : public IJob
	{
	public:
		explicit FFunctionJob(TFunction<bool(FJobContext&, FResult&)> InStep) : StepFunction(MoveTemp(InStep)) {}
		virtual bool Step(FJobContext& Context, FResult& OutResult) override { return StepFunction(Context, OutResult); }

	private:
		TFunction<bool(FJobContext&, FResult&)> StepFunction;
	};

	/** One call, its arguments already validated against the manifest. */
	struct FActionCall
	{
		FString CallId;
		FString Action;
		TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		EEndpointMode Mode = EEndpointMode::Editor;
		bool bCanRender = false;

		FString GetString(const TCHAR* Name) const;
		bool GetBool(const TCHAR* Name, bool bDefault) const;
		double GetNumber(const TCHAR* Name, double Default) const;
		bool Has(const TCHAR* Name) const;
	};

	/** What the endpoint calls for an action: exactly one of the two is set. */
	struct FActionHandler
	{
		/** A short action: runs inside the request, which answers 200 with the result. */
		TFunction<FResult(const FActionCall&)> Run;
		/** A long action: makes the job, and the request answers 202. */
		TFunction<TUniquePtr<IJob>(const FActionCall&)> Start;
	};

	/** One action of the manifest. */
	struct FManifestAction
	{
		FString Name;
		/** `engine` / `render` / `none`. */
		FString Needs;
		bool bWrites = false;
		bool bLong = false;
		/** The JSON Schema of `args`. */
		TSharedPtr<FJsonObject> Args;
	};

	/** The parts of Resources/DreamMCP.json the endpoint reads. */
	struct FManifest
	{
		int32 Protocol = 0;
		FString Id;
		FString Plugin;
		FString Route;
		TArray<FManifestAction> Actions;

		const FManifestAction* FindAction(const FString& Name) const;

		/** False, with OutError, when Text is not a manifest this endpoint can serve. */
		static bool Parse(const FString& Text, FManifest& OutManifest, FString& OutError);

		/** `<Plugin>/Resources/DreamMCP.json`. */
		static FString GetPluginManifestPath();
	};

	/**
	 * Checks Args against an action's `args` schema: an object; every `required` property present; each property of the
	 * type its schema names (`string`, `boolean`, `integer`, `number`), inside `enum`, `minimum` and `maximum` when
	 * given; and no property the schema does not declare. False, with OutError naming the first problem.
	 */
	bool ValidateActionArgs(const FJsonObject& Schema, const FJsonObject& Args, FString& OutError);

	struct FEndpointOptions
	{
		EEndpointMode Mode = EEndpointMode::Editor;

		/** The first port tried; a port that cannot be bound is skipped for the next one. */
		int32 Port = 47800;
		int32 PortAttempts = 16;

		/** Empty: a random one. */
		FString Token;

		/** False: no socket at all, for the tests that drive Handle() directly. */
		bool bListen = true;
		/**
		 * Writes <DiscoveryDirectory>/<id>.json once the endpoint has answered a hello of its own over the socket (see
		 * UpdateDiscovery), and deletes it on Stop. Without a listener there is nothing to ask, and it is written at Start.
		 */
		bool bPublishDiscovery = true;
		/** Steps the jobs from the core ticker. False: the owner calls TickJobs(). */
		bool bRegisterTicker = true;

		/** Empty: <Project>/Saved/DreamMCP. */
		FString DiscoveryDirectory;
		/** Empty: the plugin's Resources/DreamMCP.json. */
		FString ManifestText;
		/** False under -nullrhi and in any commandlet; `needs: render` actions refuse then. */
		bool bCanRender = false;

		/** How long one frame may spend stepping jobs: one step at least, more while they are quick. 0: one step a frame. */
		double StepBudgetSeconds = 0.03;
	};

	class FEndpoint
	{
	public:
		FEndpoint();
		~FEndpoint();

		FEndpoint(const FEndpoint&) = delete;
		FEndpoint& operator=(const FEndpoint&) = delete;

		/** Before Start. An action the manifest names but nobody registered is left out of hello and answers 404. */
		void RegisterAction(const FString& Name, FActionHandler Handler);

		/**
		 * Loads the manifest, listens and proves the bind is loopback-only. The discovery file comes later, from
		 * UpdateDiscovery: a server that reads it must find an endpoint that answers.
		 */
		bool Start(const FEndpointOptions& InOptions, FString& OutError);

		/**
		 * Publishes the discovery file once the endpoint answers: asks its own /hello over the socket from a worker
		 * thread, and writes the file on the first tick after a 200. Until the engine's main loop runs, nothing ticks the
		 * HTTP server and the hello goes unanswered, so the file never names an endpoint that cannot answer yet -- an
		 * editor builds for seconds after its modules start. A hello that fails is asked again half a second later.
		 * The ticker calls it; an endpoint without one (bRegisterTicker false) has its owner call it. Game thread only.
		 */
		void UpdateDiscovery();
		bool IsDiscoveryPublished() const { return bPublishedDiscovery; }

		/** Unbinds the routes, deletes the discovery file if it is still this process's, drops every job. */
		void Stop();

		bool IsStarted() const { return bStarted; }
		bool IsListening() const { return Router.IsValid(); }

		/** The whole protocol, one request at a time. Game thread only. */
		FHttpReply Handle(const FHttpExchange& Exchange);

		/**
		 * Steps the running job -- starting the next queued one when none runs -- for up to StepBudgetSeconds of this
		 * frame, and less when a step only waited. Game thread only.
		 */
		void TickJobs();


		/** A job queued or running. */
		bool IsBusy() const;

		int32 GetPort() const { return Port; }
		const FString& GetToken() const { return Options.Token; }
		const FManifest& GetManifest() const { return Manifest; }
		EEndpointMode GetMode() const { return Options.Mode; }
		bool CanRender() const { return Options.bCanRender; }

		/** <DiscoveryDirectory>/<id>.json. */
		FString GetDiscoveryFilePath() const;

		/** `/shutdown` was accepted. The host commandlet's loop watches this. */
		bool IsShutdownRequested() const { return bShutdownRequested; }

		/** The action names offered: the manifest's, those with a handler. */
		TArray<FString> GetOfferedActions() const;

		/** <Project>/Saved/DreamMCP/artifacts, where actions put the files they hand back. */
		static FString GetArtifactDirectory();

		/**
		 * Opens a TCP connection to Port through every non-loopback IPv4 address this machine has, and answers true when
		 * every one is refused -- the listener is not reachable from the network. OutExposedAddress names the first that
		 * was accepted. A machine with no such address has nothing to prove and answers true.
		 */
		static bool ProveLoopbackOnly(int32 Port, FString& OutExposedAddress);

	private:
		struct FJob
		{
			FString JobId;
			FString CallId;
			FString Action;
			EJobState State = EJobState::Queued;
			TUniquePtr<IJob> Runner;
			FJobContext Context;
			TSharedPtr<FJsonObject> Result;
			double QueuedAt = 0.0;
			double StartedAt = 0.0;
			double FinishedAt = 0.0;
		};

		FHttpReply HandleHello() const;
		FHttpReply HandleCall(const FHttpExchange& Exchange);
		FHttpReply HandleJob(const FString& JobId) const;
		FHttpReply HandleCancel(const FString& JobId);
		FHttpReply HandleShutdown();

		static FHttpReply MakeReply(int32 Status, const TSharedRef<FJsonObject>& Body);
		static FHttpReply MakeResultReply(int32 Status, const FResult& Result);
		TSharedRef<FJsonObject> DescribeJob(const FJob& Job) const;
		TSharedPtr<FJob> FindJob(const FString& JobId) const;
		void FinishJob(FJob& Job, const FResult& Result);
		void PruneFinishedJobs();

		bool Listen(FString& OutError);
		void BindRoutes();
		void UnbindRoutes();
		bool PublishDiscovery(FString& OutError);
		void RetractDiscovery();
		bool HandleTicker(float DeltaSeconds);

		FEndpointOptions Options;
		FManifest Manifest;
		TMap<FString, FActionHandler> Handlers;
		int32 Port = 0;
		bool bStarted = false;
		bool bShutdownRequested = false;
		bool bPublishedDiscovery = false;
		double NextPublishAttempt = 0.0;
		FString StartedAtIso;
		double StartedAtSeconds = 0.0;

		/** The hello UpdateDiscovery asks of its own endpoint: 0 asking, 1 answered 200, 2 not answered. */
		struct FSelfHello
		{
			std::atomic<int32> State{ 0 };
		};
		TSharedPtr<FSelfHello, ESPMode::ThreadSafe> SelfHello;
		TFuture<void> SelfHelloTask;
		int32 SelfHelloAttempts = 0;

		TSharedPtr<IHttpRouter> Router;
		TArray<FHttpRouteHandle> RouteHandles;
		/** Kept alive by the route lambdas' weak copies: a request that lands after Stop finds it expired. */
		TSharedPtr<FEndpoint*, ESPMode::ThreadSafe> Self;

		TArray<TSharedPtr<FJob>> Jobs;
		int32 JobCounter = 0;
		FTSTicker::FDelegateHandle TickerHandle;
	};
}
