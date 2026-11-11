// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.MCP.*: the DreamMCP endpoint's contract (protocol 1).
//
// Every test but one drives FEndpoint::Handle() with no socket: the real manifest, the real actions, the real
// compiler, and TickJobs() stepped by hand so a long action's states can be watched one at a time.
// DreamShader.MCP.Socket.Loopback is the exception: it binds a real listener and talks HTTP to it over a TCP
// socket, which is also where the loopback-only promise is checked. DreamShader.MCP.Render.Preview renders and
// needs a real RHI.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "MCP/DreamShaderMCPActions.h"
#include "MCP/DreamShaderMCPEndpoint.h"
#include "MCP/DreamShaderMCPResult.h"

#include "AssetCompilingManager.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#include "HttpServerModule.h"
#include "IPAddress.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace UE::DreamShader::Editor::MCP::Tests
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	static const TCHAR* const TestToken = TEXT("dreammcp-test-token-0123456789abcdef");

	/** The real manifest and actions, no socket, no discovery file, no ticker: Handle() and TickJobs() by hand. */
	struct FTestEndpoint
	{
		FEndpoint Endpoint;

		FTestEndpoint(FAutomationTestBase& Test, const EEndpointMode Mode, const bool bCanRender = false)
		{
			RegisterDreamShaderMCPActions(Endpoint);
			FEndpointOptions Options;
			Options.Mode = Mode;
			Options.Token = TestToken;
			Options.bListen = false;
			Options.bPublishDiscovery = false;
			Options.bRegisterTicker = false;
			Options.bCanRender = bCanRender;
			// One step per TickJobs, so the tests can watch a job's states one at a time.
			Options.StepBudgetSeconds = 0.0;
			FString Error;
			Test.TestTrue(FString::Printf(TEXT("the endpoint starts (%s)"), *Error), Endpoint.Start(Options, Error));
		}

		~FTestEndpoint()
		{
			Endpoint.Stop();
			ResetDreamShaderMCPActionState();
		}
	};

	static FHttpReply Send(FEndpoint& Endpoint, const TCHAR* Verb, const FString& Route, const FString& Body = FString(),
		const TCHAR* Token = TestToken, const bool bOrigin = false)
	{
		FHttpExchange Exchange;
		Exchange.Verb = Verb;
		Exchange.Path = FString(TEXT("/dream/shader")) + Route;
		if (Token)
		{
			Exchange.Headers.Add(TEXT("x-dream-token"), Token);
		}
		if (bOrigin)
		{
			Exchange.Headers.Add(TEXT("origin"), TEXT("http://localhost:3000"));
		}
		Exchange.Body = Body;
		return Endpoint.Handle(Exchange);
	}

	static FString CallBody(const FString& Action, const FString& ArgsJson = TEXT("{}"), const FString& Id = TEXT("t-1"))
	{
		return FString::Printf(TEXT("{\"id\":\"%s\",\"action\":\"%s\",\"args\":%s}"), *Id, *Action, *ArgsJson);
	}

	static FString JsonPath(const FString& Path)
	{
		return Path.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
	}

	static TSharedPtr<FJsonObject> ParseReply(FAutomationTestBase& Test, const FHttpReply& Reply)
	{
		TSharedPtr<FJsonObject> Object = ParseJsonObject(Reply.Body);
		Test.TestTrue(FString::Printf(TEXT("the reply is a JSON object: %s"), *Reply.Body.Left(300)), Object.IsValid());
		return Object.IsValid() ? Object : MakeShared<FJsonObject>();
	}

	static FString Field(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
	{
		FString Value;
		if (Object.IsValid())
		{
			Object->TryGetStringField(Name, Value);
		}
		return Value;
	}

	static TSharedPtr<FJsonObject> ObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
	{
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (Object.IsValid() && Object->TryGetObjectField(Name, Child) && Child)
		{
			return *Child;
		}
		return MakeShared<FJsonObject>();
	}

	static TArray<TSharedPtr<FJsonValue>> ArrayField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Object.IsValid() && Object->TryGetArrayField(Name, Values) && Values)
		{
			return *Values;
		}
		return TArray<TSharedPtr<FJsonValue>>();
	}

	/** A refusal body: the status, `ok: false`, `outcome: refused` and the reason. */
	static void ExpectRefused(FAutomationTestBase& Test, const TCHAR* What, const FHttpReply& Reply, const int32 Status, const TCHAR* Reason)
	{
		Test.TestEqual(FString::Printf(TEXT("%s: status"), What), Reply.Status, Status);
		const TSharedPtr<FJsonObject> Body = ParseReply(Test, Reply);
		bool bOk = true;
		Body->TryGetBoolField(TEXT("ok"), bOk);
		Test.TestFalse(FString::Printf(TEXT("%s: ok is false"), What), bOk);
		Test.TestEqual(FString::Printf(TEXT("%s: outcome"), What), Field(Body, TEXT("outcome")), FString(TEXT("refused")));
		Test.TestEqual(FString::Printf(TEXT("%s: reason"), What), Field(Body, TEXT("reason")), FString(Reason));
		Test.TestFalse(FString::Printf(TEXT("%s: has a summary"), What), Field(Body, TEXT("summary")).IsEmpty());
	}

	/** A long call run to its end: the 202, then TickJobs and a poll per tick until the job finishes. The job, or null. */
	static TSharedPtr<FJsonObject> RunToEnd(FAutomationTestBase& Test, FEndpoint& Endpoint, const FString& Body, TArray<FString>* OutStatesSeen = nullptr)
	{
		const FHttpReply Accepted = Send(Endpoint, TEXT("POST"), TEXT("/call"), Body);
		if (!Test.TestEqual(FString::Printf(TEXT("a long action answers 202 (%s)"), *Accepted.Body.Left(300)), Accepted.Status, 202))
		{
			return nullptr;
		}
		const FString Job = Field(ParseReply(Test, Accepted), TEXT("job"));
		Test.TestFalse(TEXT("the 202 names a job"), Job.IsEmpty());

		const double Deadline = FPlatformTime::Seconds() + 180.0;
		while (FPlatformTime::Seconds() < Deadline)
		{
			const TSharedPtr<FJsonObject> Polled = ParseJsonObject(Send(Endpoint, TEXT("GET"), TEXT("/jobs/") + Job).Body);
			const FString State = Field(Polled, TEXT("state"));
			if (OutStatesSeen && (OutStatesSeen->IsEmpty() || OutStatesSeen->Last() != State))
			{
				OutStatesSeen->Add(State);
			}
			if (State == TEXT("done") || State == TEXT("failed") || State == TEXT("cancelled"))
			{
				return Polled;
			}
			Endpoint.TickJobs();
			// A preview waits for shaders that compile on worker threads: let their results land, as a frame would.
			FAssetCompilingManager::Get().ProcessAsyncTasks(true);
			FPlatformProcess::Sleep(0.0f);
		}
		Test.AddError(FString::Printf(TEXT("The job %s never finished."), *Job));
		return nullptr;
	}

	static TSharedPtr<FJsonObject> ResultOf(const TSharedPtr<FJsonObject>& Job)
	{
		return ObjectField(Job, TEXT("result"));
	}

	static FString Rel(const FString& Path)
	{
		return MakeProjectRelativePath(Path);
	}

	static FString OkMaterial(const TCHAR* Name)
	{
		return FString::Printf(TEXT(
			"#pragma material(ShadingModel = Unlit, BlendMode = Opaque)\n"
			"export void %s(inout material m) { m.EmissiveColor = float3(0.9, 0.3, 0.1); }\n"), Name);
	}

	static FString BrokenMaterial(const TCHAR* Name)
	{
		return FString::Printf(TEXT(
			"#pragma material(ShadingModel = Unlit, BlendMode = Opaque)\n"
			"export void %s(inout material m) { m.EmissiveColor = NotDeclaredAnywhere; }\n"), Name);
	}

	/** A test that compiles sources which are meant to fail: their errors reach the log, and must not fail the test. */
	class FQuietTestBase : public FAutomationTestBase
	{
	public:
		FQuietTestBase(const FString& InName, const bool bInComplexTask) : FAutomationTestBase(InName, bInComplexTask) {}
		virtual bool SuppressLogErrors() override { return true; }
		virtual bool SuppressLogWarnings() override { return true; }
	};

	// ------------------------------------------------------------------------------------- a raw HTTP client

	struct FRawReply
	{
		int32 Status = 0;
		FString Body;
	};

	/**
	 * One HTTP/1.1 request over a real TCP connection to 127.0.0.1:Port. The listener ticks on this thread, so the read
	 * loop ticks the HTTP server module itself between reads.
	 */
	static bool SendRaw(const int32 Port, const FString& RequestText, FRawReply& OutReply, FString& OutError)
	{
		ISocketSubsystem* const Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr();
		bool bValid = false;
		Address->SetIp(TEXT("127.0.0.1"), bValid);
		Address->SetPort(Port);
		FSocket* const Socket = Sockets->CreateSocket(NAME_Stream, TEXT("DreamMCPTestClient"), Address->GetProtocolType());
		if (!Socket || !Socket->Connect(*Address))
		{
			OutError = TEXT("could not connect to 127.0.0.1");
			if (Socket) { Sockets->DestroySocket(Socket); }
			return false;
		}
		Socket->SetNonBlocking(true);

		const FTCHARToUTF8 Utf8(*RequestText);
		int32 Sent = 0;
		Socket->Send(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), Sent);

		TArray<uint8> Received;
		bool bComplete = false;
		const double Deadline = FPlatformTime::Seconds() + 10.0;
		while (!bComplete && FPlatformTime::Seconds() < Deadline)
		{
			static_cast<FTSTickerObjectBase&>(FHttpServerModule::Get()).Tick(0.01f);
			uint8 Buffer[4096];
			int32 Read = 0;
			if (Socket->Recv(Buffer, sizeof(Buffer), Read) && Read > 0)
			{
				Received.Append(Buffer, Read);
			}

			const FUTF8ToTCHAR Decoded(reinterpret_cast<const ANSICHAR*>(Received.GetData()), Received.Num());
			const FString Text(Decoded.Length(), Decoded.Get());
			int32 HeaderEnd = Text.Find(TEXT("\r\n\r\n"));
			if (HeaderEnd != INDEX_NONE)
			{
				const FString Head = Text.Left(HeaderEnd);
				TArray<FString> Lines;
				Head.ParseIntoArrayLines(Lines);
				int32 ContentLength = -1;
				for (const FString& Line : Lines)
				{
					if (Line.StartsWith(TEXT("content-length:"), ESearchCase::IgnoreCase))
					{
						ContentLength = FCString::Atoi(*Line.RightChop(15).TrimStartAndEnd());
					}
				}
				const FString Body = Text.RightChop(HeaderEnd + 4);
				if (ContentLength >= 0 && FTCHARToUTF8(*Body).Length() >= ContentLength)
				{
					TArray<FString> StatusParts;
					Lines[0].ParseIntoArrayWS(StatusParts);
					OutReply.Status = StatusParts.Num() > 1 ? FCString::Atoi(*StatusParts[1]) : 0;
					OutReply.Body = Body;
					bComplete = true;
				}
			}
			FPlatformProcess::Sleep(0.001f);
		}
		Socket->Close();
		Sockets->DestroySocket(Socket);
		if (!bComplete)
		{
			OutError = TEXT("no complete answer within 10 s");
		}
		return bComplete;
	}
}

using namespace UE::DreamShader::Editor::MCP;
using namespace UE::DreamShader::Editor::MCP::Tests;

// ------------------------------------------------------------------------------------------------ protocol

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderMCPHelloTest,
	"DreamShader.MCP.Protocol.Hello",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMCPHelloTest::RunTest(const FString& Parameters)
{
	FTestEndpoint Test(*this, EEndpointMode::Editor);
	const FHttpReply Reply = Send(Test.Endpoint, TEXT("GET"), TEXT("/hello"));
	TestEqual(TEXT("hello answers 200"), Reply.Status, 200);
	const TSharedPtr<FJsonObject> Body = ParseReply(*this, Reply);

	double Protocol = 0.0;
	Body->TryGetNumberField(TEXT("protocol"), Protocol);
	TestEqual(TEXT("protocol 1"), static_cast<int32>(Protocol), 1);
	TestEqual(TEXT("id"), Field(Body, TEXT("id")), FString(TEXT("shader")));
	TestEqual(TEXT("mode"), Field(Body, TEXT("mode")), FString(TEXT("editor")));
	TestFalse(TEXT("plugin version"), Field(Body, TEXT("plugin")).IsEmpty());
	TestFalse(TEXT("engine version"), Field(Body, TEXT("engine")).IsEmpty());
	bool bBusy = true;
	Body->TryGetBoolField(TEXT("busy"), bBusy);
	TestFalse(TEXT("not busy"), bBusy);
	TestTrue(TEXT("ws is null without a WebSocket"), Body->HasTypedField<EJson::Null>(TEXT("ws")));

	// The manifest is the one list: every action it names has a handler here, and hello offers exactly those.
	TArray<FString> Offered;
	for (const TSharedPtr<FJsonValue>& Value : ArrayField(Body, TEXT("actions")))
	{
		Offered.Add(Value->AsString());
	}
	TArray<FString> Declared;
	for (const FManifestAction& Action : Test.Endpoint.GetManifest().Actions)
	{
		Declared.Add(Action.Name);
	}
	TestEqual(TEXT("hello offers every action of the manifest"), FString::Join(Offered, TEXT(",")), FString::Join(Declared, TEXT(",")));
	for (const TCHAR* Name : { TEXT("check"), TEXT("compile"), TEXT("diagnostics"), TEXT("lookup"), TEXT("preview"), TEXT("info"), TEXT("decompile") })
	{
		TestTrue(FString::Printf(TEXT("the manifest has %s"), Name), Declared.Contains(Name));
	}

	// `long` in the manifest and the shape of the answer agree: a short action answers 200, a long one 202.
	for (const FManifestAction& Action : Test.Endpoint.GetManifest().Actions)
	{
		// preview included: this endpoint does not render, and a long action is still answered with a job.
		const FString Args = Action.Name == TEXT("lookup") ? TEXT("{\"name\":\"BaseColor\"}")
			: Action.Name == TEXT("decompile") ? TEXT("{\"asset\":\"/Game/DoesNotExist\"}")
			: (Action.Name == TEXT("compile") || Action.Name == TEXT("preview")) ? TEXT("{\"file\":\"DShader/DoesNotExist.dss\"}")
			: TEXT("{}");
		const FHttpReply Answer = Send(Test.Endpoint, TEXT("POST"), TEXT("/call"), CallBody(Action.Name, Args, Action.Name));
		TestEqual(FString::Printf(TEXT("%s answers %s"), *Action.Name, Action.bLong ? TEXT("202") : TEXT("200")), Answer.Status, Action.bLong ? 202 : 200);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderMCPAuthTest,
	"DreamShader.MCP.Protocol.Auth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMCPAuthTest::RunTest(const FString& Parameters)
{
	FTestEndpoint Test(*this, EEndpointMode::Editor);
	ExpectRefused(*this, TEXT("no token"), Send(Test.Endpoint, TEXT("GET"), TEXT("/hello"), FString(), nullptr), 401, TEXT("bad-token"));
	ExpectRefused(*this, TEXT("a wrong token"), Send(Test.Endpoint, TEXT("GET"), TEXT("/hello"), FString(), TEXT("dreammcp-test-token-0123456789abcdeX")), 401, TEXT("bad-token"));
	ExpectRefused(*this, TEXT("a shorter token"), Send(Test.Endpoint, TEXT("GET"), TEXT("/hello"), FString(), TEXT("dreammcp")), 401, TEXT("bad-token"));
	ExpectRefused(*this, TEXT("a call without a token"), Send(Test.Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("info")), nullptr), 401, TEXT("bad-token"));
	ExpectRefused(*this, TEXT("an Origin with the right token"), Send(Test.Endpoint, TEXT("GET"), TEXT("/hello"), FString(), TestToken, true), 403, TEXT("origin-not-allowed"));
	ExpectRefused(*this, TEXT("an Origin without a token"), Send(Test.Endpoint, TEXT("GET"), TEXT("/hello"), FString(), nullptr, true), 403, TEXT("origin-not-allowed"));
	TestEqual(TEXT("the right token passes"), Send(Test.Endpoint, TEXT("GET"), TEXT("/hello")).Status, 200);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderMCPRefusalTest,
	"DreamShader.MCP.Protocol.Refusals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMCPRefusalTest::RunTest(const FString& Parameters)
{
	FTestEndpoint Editor(*this, EEndpointMode::Editor);
	FEndpoint& Endpoint = Editor.Endpoint;

	const FHttpReply Unknown = Send(Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("explode")));
	ExpectRefused(*this, TEXT("an unknown action"), Unknown, 404, TEXT("unknown-action"));
	TArray<FString> Listed;
	for (const TSharedPtr<FJsonValue>& Value : ArrayField(ObjectField(ParseReply(*this, Unknown), TEXT("data")), TEXT("actions")))
	{
		Listed.Add(Value->AsString());
	}
	TestTrue(TEXT("the unknown-action answer lists the actions"), Listed.Contains(TEXT("check")));
	TestFalse(TEXT("and says what to do next"), ArrayField(ParseReply(*this, Unknown), TEXT("next")).IsEmpty());

	ExpectRefused(*this, TEXT("a body that is not JSON"), Send(Endpoint, TEXT("POST"), TEXT("/call"), TEXT("check please")), 400, TEXT("bad-request"));
	ExpectRefused(*this, TEXT("args that are not an object"), Send(Endpoint, TEXT("POST"), TEXT("/call"), TEXT("{\"action\":\"check\",\"args\":[1]}")), 400, TEXT("bad-args"));
	ExpectRefused(*this, TEXT("an argument the action does not take"), Send(Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("check"), TEXT("{\"path\":\"x.dss\"}"))), 400, TEXT("bad-args"));
	ExpectRefused(*this, TEXT("an argument of the wrong type"), Send(Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("check"), TEXT("{\"file\":3}"))), 400, TEXT("bad-args"));
	ExpectRefused(*this, TEXT("a required argument missing"), Send(Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("compile"))), 400, TEXT("bad-args"));
	ExpectRefused(*this, TEXT("a value outside its enum"), Send(Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("preview"), TEXT("{\"file\":\"a.dss\",\"mesh\":\"teapot\"}"))), 400, TEXT("bad-args"));
	ExpectRefused(*this, TEXT("an integer that is not one"), Send(Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("lookup"), TEXT("{\"name\":\"x\",\"limit\":2.5}"))), 400, TEXT("bad-args"));
	ExpectRefused(*this, TEXT("a number below its minimum"), Send(Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("lookup"), TEXT("{\"name\":\"x\",\"limit\":0}"))), 400, TEXT("bad-args"));
	ExpectRefused(*this, TEXT("an unknown route"), Send(Endpoint, TEXT("GET"), TEXT("/status")), 404, TEXT("unknown-route"));
	ExpectRefused(*this, TEXT("a route under another prefix"), [&]() { FHttpExchange E; E.Verb = TEXT("GET"); E.Path = TEXT("/dream/fx/hello"); E.Headers.Add(TEXT("x-dream-token"), TestToken); return Endpoint.Handle(E); }(), 404, TEXT("unknown-route"));
	ExpectRefused(*this, TEXT("the wrong verb"), Send(Endpoint, TEXT("GET"), TEXT("/call")), 405, TEXT("bad-method"));
	ExpectRefused(*this, TEXT("an unknown job"), Send(Endpoint, TEXT("GET"), TEXT("/jobs/nope")), 404, TEXT("unknown-job"));
	ExpectRefused(*this, TEXT("cancelling an unknown job"), Send(Endpoint, TEXT("POST"), TEXT("/jobs/nope/cancel")), 404, TEXT("unknown-job"));
	ExpectRefused(*this, TEXT("shutdown of an editor"), Send(Endpoint, TEXT("POST"), TEXT("/shutdown")), 409, TEXT("not-a-host"));
	TestFalse(TEXT("an editor does not shut down"), Endpoint.IsShutdownRequested());
	// A long action stays long when it is refused before it starts: 202, and a job whose result is the refusal.
	const TSharedPtr<FJsonObject> NoRenderer = ResultOf(RunToEnd(*this, Endpoint, CallBody(TEXT("preview"), TEXT("{\"file\":\"DShader/a.dss\"}"), TEXT("r-1"))));
	TestEqual(TEXT("preview without a renderer is refused"), Field(NoRenderer, TEXT("reason")), FString(TEXT("render-unavailable")));
	TestEqual(TEXT("as a refusal"), Field(NoRenderer, TEXT("outcome")), FString(TEXT("refused")));

	FTestEndpoint Host(*this, EEndpointMode::Host);
	const FHttpReply Shutdown = Send(Host.Endpoint, TEXT("POST"), TEXT("/shutdown"));
	TestEqual(TEXT("a host accepts shutdown"), Shutdown.Status, 200);
	TestEqual(TEXT("with an ok result"), Field(ParseReply(*this, Shutdown), TEXT("outcome")), FString(TEXT("ok")));
	TestTrue(TEXT("and says so to its loop"), Host.Endpoint.IsShutdownRequested());
	return true;
}

// ------------------------------------------------------------------------------------------------- actions

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FDreamShaderMCPCheckTest,
	FQuietTestBase,
	"DreamShader.MCP.Actions.Check",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMCPCheckTest::RunTest(const FString& Parameters)
{
	FTestEndpoint Test(*this, EEndpointMode::Host);
	FDreamShaderCompile2Fixture Good(TEXT("M_McpCheckOk"), TEXT("MCP"));
	FDreamShaderCompile2Fixture Bad(TEXT("M_McpCheckBad"), TEXT("MCP"));
	if (!Good.WriteSource(*this, OkMaterial(TEXT("M_McpCheckOk"))) || !Bad.WriteSource(*this, BrokenMaterial(TEXT("M_McpCheckBad"))))
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Passed = RunToEnd(*this, Test.Endpoint,
		CallBody(TEXT("check"), FString::Printf(TEXT("{\"file\":\"%s\"}"), *JsonPath(Rel(Good.GetSourceFilePath())))));
	const TSharedPtr<FJsonObject> PassedResult = ResultOf(Passed);
	TestEqual(TEXT("a clean source: job done"), Field(Passed, TEXT("state")), FString(TEXT("done")));
	TestEqual(TEXT("a clean source: outcome ok"), Field(PassedResult, TEXT("outcome")), FString(TEXT("ok")));
	TestFalse(TEXT("check wrote nothing"), FPackageName::DoesPackageExist(Good.GetPackagePath() + TEXT("/M_McpCheckOk")));

	const TSharedPtr<FJsonObject> Failed = RunToEnd(*this, Test.Endpoint,
		CallBody(TEXT("check"), FString::Printf(TEXT("{\"file\":\"%s\"}"), *JsonPath(Rel(Bad.GetSourceFilePath()))), TEXT("t-2")));
	const TSharedPtr<FJsonObject> FailedResult = ResultOf(Failed);
	TestEqual(TEXT("a broken source: job done"), Field(Failed, TEXT("state")), FString(TEXT("done")));
	TestEqual(TEXT("a broken source: outcome source-error"), Field(FailedResult, TEXT("outcome")), FString(TEXT("source-error")));
	const TArray<TSharedPtr<FJsonValue>> Diagnostics = ArrayField(FailedResult, TEXT("diagnostics"));
	if (TestTrue(TEXT("a broken source has diagnostics"), Diagnostics.Num() > 0))
	{
		const TSharedPtr<FJsonObject> First = Diagnostics[0]->AsObject();
		TestTrue(TEXT("the diagnostic has a DSH code"), Field(First, TEXT("code")).StartsWith(TEXT("DSH")));
		TestEqual(TEXT("the diagnostic's severity"), Field(First, TEXT("severity")), FString(TEXT("error")));
		TestEqual(TEXT("the diagnostic's file is project-relative"), Field(First, TEXT("file")), Rel(Bad.GetSourceFilePath()));
		double Line = 0.0;
		First->TryGetNumberField(TEXT("line"), Line);
		TestEqual(TEXT("the diagnostic's line"), static_cast<int32>(Line), 2);
		TestTrue(TEXT("the diagnostic links its documentation"), Field(First, TEXT("doc")).StartsWith(TEXT("https://")));
		TestFalse(TEXT("no absolute path in the answer"), Failed.IsValid() && SerializeJson(Failed.ToSharedRef()).Contains(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())));
	}

	const TSharedPtr<FJsonObject> Missing = ResultOf(RunToEnd(*this, Test.Endpoint, CallBody(TEXT("check"), TEXT("{\"file\":\"DShader/NoSuchFile.dss\"}"), TEXT("t-3"))));
	TestEqual(TEXT("a missing file is refused"), Field(Missing, TEXT("reason")), FString(TEXT("file-not-found")));

	FString HeaderPath;
	Good.WriteSiblingSource(*this, TEXT("Shared.dsh"), TEXT("float Half(float x) { return x * 0.5; }\n"), HeaderPath);
	const TSharedPtr<FJsonObject> Header = ResultOf(RunToEnd(*this, Test.Endpoint,
		CallBody(TEXT("check"), FString::Printf(TEXT("{\"file\":\"%s\"}"), *JsonPath(Rel(HeaderPath))), TEXT("t-4"))));
	TestEqual(TEXT("a header is refused"), Field(Header, TEXT("reason")), FString(TEXT("not-a-source")));

	const TSharedPtr<FJsonObject> Neither = ResultOf(RunToEnd(*this, Test.Endpoint, CallBody(TEXT("check"), TEXT("{}"), TEXT("t-5"))));
	TestEqual(TEXT("check with neither file nor all is refused"), Field(Neither, TEXT("reason")), FString(TEXT("bad-args")));
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FDreamShaderMCPCompileTest,
	FQuietTestBase,
	"DreamShader.MCP.Actions.CompileAndDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMCPCompileTest::RunTest(const FString& Parameters)
{
	FTestEndpoint Test(*this, EEndpointMode::Host);
	FDreamShaderCompile2Fixture Good(TEXT("M_McpCompileOk"), TEXT("MCP"));
	FDreamShaderCompile2Fixture Bad(TEXT("M_McpCompileBad"), TEXT("MCP"));
	if (!Good.WriteSource(*this, OkMaterial(TEXT("M_McpCompileOk"))) || !Bad.WriteSource(*this, BrokenMaterial(TEXT("M_McpCompileBad"))))
	{
		return false;
	}
	const FString GoodArgs = FString::Printf(TEXT("{\"file\":\"%s\"}"), *JsonPath(Rel(Good.GetSourceFilePath())));
	const FString BadArgs = FString::Printf(TEXT("{\"file\":\"%s\"}"), *JsonPath(Rel(Bad.GetSourceFilePath())));

	const TSharedPtr<FJsonObject> Built = ResultOf(RunToEnd(*this, Test.Endpoint, CallBody(TEXT("compile"), GoodArgs, TEXT("c-1"))));
	TestEqual(TEXT("a clean source compiles"), Field(Built, TEXT("outcome")), FString(TEXT("ok")));
	const TArray<TSharedPtr<FJsonValue>> Writes = ArrayField(Built, TEXT("writes"));
	if (TestTrue(TEXT("the compile reports what it wrote"), Writes.Num() > 0))
	{
		TestTrue(TEXT("a write names a /Game package"), Field(Writes[0]->AsObject(), TEXT("asset")).StartsWith(Good.GetPackagePath()));
		TestEqual(TEXT("the host saves a new asset as created"), Field(Writes[0]->AsObject(), TEXT("state")), FString(TEXT("created")));
	}
	const TArray<TSharedPtr<FJsonValue>> Products = ArrayField(ObjectField(Built, TEXT("data")), TEXT("products"));
	if (TestEqual(TEXT("one product"), Products.Num(), 1))
	{
		TestTrue(TEXT("the product is the material"), Field(Products[0]->AsObject(), TEXT("asset")).EndsWith(TEXT("M_McpCompileOk.M_McpCompileOk")));
		Good.TrackObjectPath(Field(Products[0]->AsObject(), TEXT("asset")));
	}

	const TSharedPtr<FJsonObject> Again = ResultOf(RunToEnd(*this, Test.Endpoint, CallBody(TEXT("compile"), GoodArgs, TEXT("c-2"))));
	TestEqual(TEXT("an unchanged source compiles again"), Field(Again, TEXT("outcome")), FString(TEXT("ok")));
	TestEqual(TEXT("and writes nothing"), ArrayField(Again, TEXT("writes")).Num(), 0);

	const TSharedPtr<FJsonObject> Forced = ResultOf(RunToEnd(*this, Test.Endpoint,
		CallBody(TEXT("compile"), FString::Printf(TEXT("{\"file\":\"%s\",\"force\":true}"), *JsonPath(Rel(Good.GetSourceFilePath()))), TEXT("c-3"))));
	if (TestTrue(TEXT("force rebuilds"), ArrayField(Forced, TEXT("writes")).Num() > 0))
	{
		TestEqual(TEXT("a rebuilt asset is saved over the old one"), Field(ArrayField(Forced, TEXT("writes"))[0]->AsObject(), TEXT("state")), FString(TEXT("saved")));
	}

	const TSharedPtr<FJsonObject> Broken = ResultOf(RunToEnd(*this, Test.Endpoint, CallBody(TEXT("compile"), BadArgs, TEXT("c-4"))));
	TestEqual(TEXT("a broken source is a source error"), Field(Broken, TEXT("outcome")), FString(TEXT("source-error")));
	TestTrue(TEXT("with its diagnostics"), ArrayField(Broken, TEXT("diagnostics")).Num() > 0);
	TestEqual(TEXT("and no writes"), ArrayField(Broken, TEXT("writes")).Num(), 0);

	// diagnostics: what the last compile left, without compiling.
	const FHttpReply Held = Send(Test.Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("diagnostics"), BadArgs, TEXT("d-1")));
	TestEqual(TEXT("diagnostics is short: 200"), Held.Status, 200);
	const TSharedPtr<FJsonObject> HeldBody = ParseReply(*this, Held);
	TestEqual(TEXT("the broken file's diagnostics are a source error"), Field(HeldBody, TEXT("outcome")), FString(TEXT("source-error")));
	TestTrue(TEXT("and carry the errors"), ArrayField(HeldBody, TEXT("diagnostics")).Num() > 0);

	const TSharedPtr<FJsonObject> Clean = ParseReply(*this, Send(Test.Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("diagnostics"), GoodArgs, TEXT("d-2"))));
	TestEqual(TEXT("the clean file's diagnostics are ok"), Field(Clean, TEXT("outcome")), FString(TEXT("ok")));

	const TSharedPtr<FJsonObject> Unknown = ParseReply(*this, Send(Test.Endpoint, TEXT("POST"), TEXT("/call"),
		CallBody(TEXT("diagnostics"), TEXT("{\"file\":\"DShader/NeverCompiled.dss\"}"), TEXT("d-3"))));
	bool bKnown = true;
	ObjectField(Unknown, TEXT("data"))->TryGetBoolField(TEXT("known"), bKnown);
	TestFalse(TEXT("a file nothing compiled has no known diagnostics"), bKnown);

	const TSharedPtr<FJsonObject> All = ParseReply(*this, Send(Test.Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("diagnostics"), TEXT("{}"), TEXT("d-4"))));
	TestTrue(TEXT("diagnostics without a file lists the broken one"), SerializeJson(All.ToSharedRef()).Contains(TEXT("M_McpCompileBad.dss")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderMCPLookupInfoTest,
	"DreamShader.MCP.Actions.LookupAndInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMCPLookupInfoTest::RunTest(const FString& Parameters)
{
	FTestEndpoint Test(*this, EEndpointMode::Host);
	const auto Lookup = [this, &Test](const TCHAR* Args) { return ParseReply(*this, Send(Test.Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("lookup"), Args))); };

	const TSharedPtr<FJsonObject> Exact = Lookup(TEXT("{\"name\":\"UE.TexCoord\"}"));
	TestEqual(TEXT("an alias is an exact match"), Field(Exact, TEXT("outcome")), FString(TEXT("ok")));
	bool bExact = false;
	ObjectField(Exact, TEXT("data"))->TryGetBoolField(TEXT("exact"), bExact);
	TestTrue(TEXT("exact"), bExact);
	const TArray<TSharedPtr<FJsonValue>> Entries = ArrayField(ObjectField(Exact, TEXT("data")), TEXT("expressions"));
	if (TestEqual(TEXT("one entry"), Entries.Num(), 1))
	{
		TestEqual(TEXT("the full entry"), Field(Entries[0]->AsObject(), TEXT("className")), FString(TEXT("MaterialExpressionTextureCoordinate")));
	}

	const TSharedPtr<FJsonObject> Search = Lookup(TEXT("{\"name\":\"texture\",\"limit\":5}"));
	double Total = 0.0;
	ObjectField(Search, TEXT("data"))->TryGetNumberField(TEXT("total"), Total);
	TestTrue(TEXT("a part of a name finds several"), Total > 1.0);
	TestTrue(TEXT("no more than the limit listed"), ArrayField(ObjectField(Search, TEXT("data")), TEXT("expressions")).Num() <= 5);

	const TSharedPtr<FJsonObject> Attribute = Lookup(TEXT("{\"name\":\"m.BaseColor\"}"));
	TestEqual(TEXT("a material attribute"), ArrayField(ObjectField(Attribute, TEXT("data")), TEXT("attributes")).Num(), 1);

	const TSharedPtr<FJsonObject> Nothing = Lookup(TEXT("{\"name\":\"Zzqxvw\"}"));
	TestEqual(TEXT("nothing found is still ok"), Field(Nothing, TEXT("outcome")), FString(TEXT("ok")));
	TestFalse(TEXT("and says what to try"), ArrayField(Nothing, TEXT("next")).IsEmpty());

	const TSharedPtr<FJsonObject> Info = ParseReply(*this, Send(Test.Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("info"))));
	TestEqual(TEXT("info is ok"), Field(Info, TEXT("outcome")), FString(TEXT("ok")));
	const TSharedPtr<FJsonObject> Data = ObjectField(Info, TEXT("data"));
	TestFalse(TEXT("info names the plugin version"), Field(Data, TEXT("plugin")).IsEmpty());
	TestEqual(TEXT("info names the mode"), Field(Data, TEXT("mode")), FString(TEXT("host")));
	TestTrue(TEXT("info lists the source roots"), ArrayField(Data, TEXT("roots")).Num() > 0);
	TestFalse(TEXT("no absolute path in info"), SerializeJson(Info.ToSharedRef()).Contains(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())));
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FDreamShaderMCPDecompileTest,
	FQuietTestBase,
	"DreamShader.MCP.Actions.Decompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMCPDecompileTest::RunTest(const FString& Parameters)
{
	FScopedDreamShaderGraphBackendPin GraphBackend;
	FTestEndpoint Test(*this, EEndpointMode::Host);
	FDreamShaderCompile2Fixture Fixture(TEXT("M_McpDecompile"), TEXT("MCP"));
	if (!Fixture.WriteSource(*this, OkMaterial(TEXT("M_McpDecompile"))))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Built = ResultOf(RunToEnd(*this, Test.Endpoint,
		CallBody(TEXT("compile"), FString::Printf(TEXT("{\"file\":\"%s\"}"), *JsonPath(Rel(Fixture.GetSourceFilePath()))))));
	if (!TestEqual(TEXT("the material to decompile compiles"), Field(Built, TEXT("outcome")), FString(TEXT("ok"))))
	{
		return false;
	}
	Fixture.TrackObjectPath(Fixture.MakeObjectPath(TEXT("M_McpDecompile")));

	const FString Out = Rel(FPaths::Combine(FPaths::GetPath(Fixture.GetSourceFilePath()), TEXT("M_McpDecompile_Text.dss")));
	const FString Args = FString::Printf(TEXT("{\"asset\":\"%s\",\"out\":\"%s\"}"), *(Fixture.GetPackagePath() + TEXT("/M_McpDecompile")), *JsonPath(Out));
	const TSharedPtr<FJsonObject> Decompiled = ResultOf(RunToEnd(*this, Test.Endpoint, CallBody(TEXT("decompile"), Args, TEXT("x-1"))));
	TestEqual(TEXT("decompile is ok"), Field(Decompiled, TEXT("outcome")), FString(TEXT("ok")));
	const TArray<TSharedPtr<FJsonValue>> Artifacts = ArrayField(Decompiled, TEXT("artifacts"));
	if (TestEqual(TEXT("one artifact"), Artifacts.Num(), 1))
	{
		TestEqual(TEXT("the source it wrote"), Field(Artifacts[0]->AsObject(), TEXT("path")), Out);
		TestTrue(TEXT("which exists"), IFileManager::Get().FileExists(*FPaths::Combine(FPaths::ProjectDir(), Out)));
	}
	TestEqual(TEXT("decompile writes no asset"), ArrayField(Decompiled, TEXT("writes")).Num(), 0);

	const TSharedPtr<FJsonObject> Exists = ResultOf(RunToEnd(*this, Test.Endpoint, CallBody(TEXT("decompile"), Args, TEXT("x-2"))));
	TestEqual(TEXT("an existing destination is refused"), Field(Exists, TEXT("reason")), FString(TEXT("file-exists")));

	const FString Overwrite = FString::Printf(TEXT("{\"asset\":\"%s\",\"out\":\"%s\",\"overwrite\":true}"), *(Fixture.GetPackagePath() + TEXT("/M_McpDecompile")), *JsonPath(Out));
	TestEqual(TEXT("overwrite replaces it"), Field(ResultOf(RunToEnd(*this, Test.Endpoint, CallBody(TEXT("decompile"), Overwrite, TEXT("x-3")))), TEXT("outcome")), FString(TEXT("ok")));

	const TSharedPtr<FJsonObject> Missing = ResultOf(RunToEnd(*this, Test.Endpoint, CallBody(TEXT("decompile"), TEXT("{\"asset\":\"/Game/DreamShaderTests/MCP/Nothing/M_Nothing\"}"), TEXT("x-4"))));
	TestEqual(TEXT("an asset that does not exist is refused"), Field(Missing, TEXT("reason")), FString(TEXT("asset-not-found")));
	return true;
}

// ---------------------------------------------------------------------------------------------------- jobs

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FDreamShaderMCPJobsTest,
	FQuietTestBase,
	"DreamShader.MCP.Jobs.PollAndCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMCPJobsTest::RunTest(const FString& Parameters)
{
	FTestEndpoint Test(*this, EEndpointMode::Host);
	FEndpoint& Endpoint = Test.Endpoint;
	FDreamShaderCompile2Fixture Fixture(TEXT("M_McpJobs"), TEXT("MCP"));
	if (!Fixture.WriteSource(*this, OkMaterial(TEXT("M_McpJobs"))))
	{
		return false;
	}
	const FString Args = FString::Printf(TEXT("{\"file\":\"%s\"}"), *JsonPath(Rel(Fixture.GetSourceFilePath())));
	const auto Job = [this](const FHttpReply& Reply) { return Field(ParseReply(*this, Reply), TEXT("job")); };
	const auto State = [this, &Endpoint](const FString& Id) { return Field(ParseReply(*this, Send(Endpoint, TEXT("GET"), TEXT("/jobs/") + Id)), TEXT("state")); };
	const auto Busy = [this, &Endpoint]() { bool bBusy = false; ParseReply(*this, Send(Endpoint, TEXT("GET"), TEXT("/hello")))->TryGetBoolField(TEXT("busy"), bBusy); return bBusy; };

	// The same call id twice is one job.
	const FString A = Job(Send(Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("check"), Args, TEXT("job-a"))));
	TestEqual(TEXT("a repeated call id answers with the same job"), Job(Send(Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("check"), Args, TEXT("job-a")))), A);
	const FString B = Job(Send(Endpoint, TEXT("POST"), TEXT("/call"), CallBody(TEXT("check"), Args, TEXT("job-b"))));
	TestFalse(TEXT("another call id is another job"), A == B);
	TestEqual(TEXT("a new job is queued"), State(A), FString(TEXT("queued")));
	TestTrue(TEXT("hello is busy while jobs wait"), Busy());

	// A queued job cancels at once.
	const TSharedPtr<FJsonObject> CancelB = ParseReply(*this, Send(Endpoint, TEXT("POST"), TEXT("/jobs/") + B + TEXT("/cancel")));
	bool bCancelled = false;
	CancelB->TryGetBoolField(TEXT("cancelled"), bCancelled);
	TestTrue(TEXT("a queued job is cancelled"), bCancelled);
	TestEqual(TEXT("and stays cancelled"), State(B), FString(TEXT("cancelled")));
	TestEqual(TEXT("its result is the cancellation"), Field(ObjectField(ParseReply(*this, Send(Endpoint, TEXT("GET"), TEXT("/jobs/") + B)), TEXT("result")), TEXT("reason")), FString(TEXT("cancelled")));

	// A running job stops at its next step.
	Endpoint.TickJobs();
	TestEqual(TEXT("one tick starts the job"), State(A), FString(TEXT("running")));
	const TSharedPtr<FJsonObject> Running = ParseReply(*this, Send(Endpoint, TEXT("GET"), TEXT("/jobs/") + A));
	const TSharedPtr<FJsonObject> Progress = ObjectField(Running, TEXT("progress"));
	TestEqual(TEXT("a running job reports its stage as the progress message"), Field(Progress, TEXT("message")), FString(TEXT("check")));
	double ProgressTotal = 0.0;
	Progress->TryGetNumberField(TEXT("total"), ProgressTotal);
	TestEqual(TEXT("and its total"), static_cast<int32>(ProgressTotal), 1);
	TestTrue(TEXT("and where it is"), Progress->HasTypedField<EJson::Number>(TEXT("current")));
	const TSharedPtr<FJsonObject> CancelA = ParseReply(*this, Send(Endpoint, TEXT("POST"), TEXT("/jobs/") + A + TEXT("/cancel")));
	bool bPending = false;
	CancelA->TryGetBoolField(TEXT("pending"), bPending);
	TestTrue(TEXT("cancelling a running job is pending"), bPending);
	Endpoint.TickJobs();
	TestEqual(TEXT("the next step stops it"), State(A), FString(TEXT("cancelled")));
	TestFalse(TEXT("hello is idle again"), Busy());

	bool bCancelledAgain = true;
	ParseReply(*this, Send(Endpoint, TEXT("POST"), TEXT("/jobs/") + A + TEXT("/cancel")))->TryGetBoolField(TEXT("cancelled"), bCancelledAgain);
	TestFalse(TEXT("a finished job is not cancelled again"), bCancelledAgain);

	// Polled to the end: queued, then running, then done.
	TArray<FString> Seen;
	const TSharedPtr<FJsonObject> Done = RunToEnd(*this, Endpoint, CallBody(TEXT("check"), Args, TEXT("job-c")), &Seen);
	TestEqual(TEXT("a polled job passes through queued, running and done"), FString::Join(Seen, TEXT(",")), FString(TEXT("queued,running,done")));
	TestEqual(TEXT("its result is the check's"), Field(ResultOf(Done), TEXT("outcome")), FString(TEXT("ok")));
	TestEqual(TEXT("a job description echoes the call id"), Field(Done, TEXT("id")), FString(TEXT("job-c")));
	TestEqual(TEXT("and names its action"), Field(Done, TEXT("action")), FString(TEXT("check")));
	return true;
}

// -------------------------------------------------------------------------------------------------- socket

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderMCPSocketTest,
	"DreamShader.MCP.Socket.Loopback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMCPSocketTest::RunTest(const FString& Parameters)
{
	const FString DiscoveryDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShaderTests"), TEXT("MCPDiscovery")));
	FEndpoint Endpoint;
	RegisterDreamShaderMCPActions(Endpoint);
	FEndpointOptions Options;
	Options.Mode = EEndpointMode::Host;
	Options.Port = 47950;
	Options.PortAttempts = 30;
	Options.Token = TestToken;
	Options.bRegisterTicker = false;
	Options.DiscoveryDirectory = DiscoveryDirectory;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("a real listener starts (%s)"), *Error), Endpoint.Start(Options, Error)))
	{
		return false;
	}
	ON_SCOPE_EXIT { Endpoint.Stop(); IFileManager::Get().DeleteDirectory(*DiscoveryDirectory, false, true); };
	const int32 Port = Endpoint.GetPort();
	TestTrue(TEXT("it listens"), Endpoint.IsListening());

	// The discovery file: what a server reads to find it.
	FString DiscoveryText;
	TestTrue(TEXT("the discovery file is written"), FFileHelper::LoadFileToString(DiscoveryText, *Endpoint.GetDiscoveryFilePath()));
	TestEqual(TEXT("as <id>.json"), FPaths::GetCleanFilename(Endpoint.GetDiscoveryFilePath()), FString(TEXT("shader.json")));
	const TSharedPtr<FJsonObject> Discovery = ParseJsonObject(DiscoveryText);
	if (TestTrue(TEXT("the discovery file is JSON"), Discovery.IsValid()))
	{
		double Pid = 0.0, DiscoveredPort = 0.0, Protocol = 0.0;
		Discovery->TryGetNumberField(TEXT("pid"), Pid);
		Discovery->TryGetNumberField(TEXT("port"), DiscoveredPort);
		Discovery->TryGetNumberField(TEXT("protocol"), Protocol);
		TestEqual(TEXT("its pid"), static_cast<int64>(Pid), static_cast<int64>(FPlatformProcess::GetCurrentProcessId()));
		TestEqual(TEXT("its port"), static_cast<int32>(DiscoveredPort), Port);
		TestEqual(TEXT("its protocol"), static_cast<int32>(Protocol), 1);
		TestEqual(TEXT("its token"), Field(Discovery, TEXT("token")), FString(TestToken));
		TestEqual(TEXT("its route"), Field(Discovery, TEXT("route")), FString(TEXT("/dream/shader")));
		TestEqual(TEXT("its mode"), Field(Discovery, TEXT("mode")), FString(TEXT("host")));
		TestTrue(TEXT("its ws"), Discovery->HasTypedField<EJson::Null>(TEXT("ws")));
		TestFalse(TEXT("its startedAt"), Field(Discovery, TEXT("startedAt")).IsEmpty());
	}

	FRawReply Reply;
	TestTrue(FString::Printf(TEXT("hello over HTTP (%s)"), *Error),
		SendRaw(Port, FString::Printf(TEXT("GET /dream/shader/hello HTTP/1.1\r\nHost: 127.0.0.1\r\nX-Dream-Token: %s\r\nConnection: close\r\n\r\n"), TestToken), Reply, Error));
	TestEqual(TEXT("answers 200"), Reply.Status, 200);
	TestEqual(TEXT("with the protocol"), Field(ParseJsonObject(Reply.Body), TEXT("id")), FString(TEXT("shader")));

	FRawReply Denied;
	SendRaw(Port, TEXT("GET /dream/shader/hello HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"), Denied, Error);
	TestEqual(TEXT("a request without the token over HTTP is 401"), Denied.Status, 401);

	FRawReply Called;
	const FString Body = CallBody(TEXT("info"));
	SendRaw(Port, FString::Printf(TEXT("POST /dream/shader/call HTTP/1.1\r\nHost: 127.0.0.1\r\nX-Dream-Token: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s"),
		TestToken, FTCHARToUTF8(*Body).Length(), *Body), Called, Error);
	TestEqual(TEXT("a short call over HTTP is 200"), Called.Status, 200);
	TestEqual(TEXT("with its result"), Field(ParseJsonObject(Called.Body), TEXT("outcome")), FString(TEXT("ok")));

	FString Exposed;
	TestTrue(FString::Printf(TEXT("nothing answers on a non-loopback address (%s)"), *Exposed), FEndpoint::ProveLoopbackOnly(Port, Exposed));

	const FString DiscoveryPath = Endpoint.GetDiscoveryFilePath();
	Endpoint.Stop();
	TestFalse(TEXT("stopping deletes the discovery file"), IFileManager::Get().FileExists(*DiscoveryPath));
	return true;
}

// -------------------------------------------------------------------------------------------------- render

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FDreamShaderMCPPreviewTest,
	FQuietTestBase,
	"DreamShader.MCP.Render.Preview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderMCPPreviewTest::RunTest(const FString& Parameters)
{
	FTestEndpoint Test(*this, EEndpointMode::Host, /*bCanRender*/ true);
	FDreamShaderCompile2Fixture Good(TEXT("M_McpPreviewOk"), TEXT("MCP"));
	FDreamShaderCompile2Fixture Bad(TEXT("M_McpPreviewBad"), TEXT("MCP"));
	if (!Good.WriteSource(*this, OkMaterial(TEXT("M_McpPreviewOk"))) || !Bad.WriteSource(*this, BrokenMaterial(TEXT("M_McpPreviewBad"))))
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Rendered = ResultOf(RunToEnd(*this, Test.Endpoint,
		CallBody(TEXT("preview"), FString::Printf(TEXT("{\"file\":\"%s\",\"width\":128,\"height\":128,\"mesh\":\"cube\"}"), *JsonPath(Rel(Good.GetSourceFilePath()))))));
	TestEqual(TEXT("a clean material previews"), Field(Rendered, TEXT("outcome")), FString(TEXT("ok")));
	const TArray<TSharedPtr<FJsonValue>> Artifacts = ArrayField(Rendered, TEXT("artifacts"));
	if (TestEqual(TEXT("one image"), Artifacts.Num(), 1))
	{
		const FString Path = Field(Artifacts[0]->AsObject(), TEXT("path"));
		TestEqual(TEXT("an image artifact"), Field(Artifacts[0]->AsObject(), TEXT("kind")), FString(TEXT("image")));
		TestTrue(TEXT("under Saved/DreamMCP/artifacts"), Path.StartsWith(TEXT("Saved/DreamMCP/artifacts/")));
		const FString Absolute = FPaths::Combine(FPaths::ProjectDir(), Path);
		TestTrue(TEXT("a PNG on disk"), IFileManager::Get().FileSize(*Absolute) > 100);
		IFileManager::Get().Delete(*Absolute, false, true, true);
	}
	Good.TrackObjectPath(Good.MakeObjectPath(TEXT("M_McpPreviewOk")));

	const TSharedPtr<FJsonObject> Broken = ResultOf(RunToEnd(*this, Test.Endpoint,
		CallBody(TEXT("preview"), FString::Printf(TEXT("{\"file\":\"%s\"}"), *JsonPath(Rel(Bad.GetSourceFilePath()))), TEXT("p-2"))));
	TestEqual(TEXT("a broken material is a source error"), Field(Broken, TEXT("outcome")), FString(TEXT("source-error")));
	TestEqual(TEXT("and renders nothing"), ArrayField(Broken, TEXT("artifacts")).Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
