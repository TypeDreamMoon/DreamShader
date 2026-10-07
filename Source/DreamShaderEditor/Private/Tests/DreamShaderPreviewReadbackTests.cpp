// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Preview/DreamShaderPreviewRenderer.h"
#include "HAL/PlatformProcess.h"
#include "Materials/Material.h"
#include "Misc/ScopeLock.h"
#include "RenderingThread.h"

namespace UE::DreamShader::Editor::Private::PreviewReadbackTests
{
	struct FReadbackObservation
	{
		EPreviewReadbackTestEvent Event;
		const FRHIGPUTextureReadback* Readback;
		bool bRenderThread;
	};

	class FScopedReadbackObserver
	{
	public:
		FScopedReadbackObserver()
		{
			FlushRenderingCommands();
			Saved = MoveTemp(GetPreviewReadbackTestObserver());
			GetPreviewReadbackTestObserver() = [this](EPreviewReadbackTestEvent Event, const FRHIGPUTextureReadback* Readback)
			{
				FScopeLock Lock(&Mutex);
				Events.Add({ Event, Readback, IsInRenderingThread() });
			};
		}

		~FScopedReadbackObserver()
		{
			FlushRenderingCommands();
			GetPreviewReadbackTestObserver() = MoveTemp(Saved);
		}

		TArray<FReadbackObservation> Snapshot()
		{
			FScopeLock Lock(&Mutex);
			return Events;
		}

	private:
		FCriticalSection Mutex;
		TArray<FReadbackObservation> Events;
		FPreviewReadbackTestObserver Saved;
	};

	static bool ReadFrame(FAutomationTestBase& Test, FDreamShaderPreviewRenderContext& Context, UMaterial* Material)
	{
		FString Error;
		if (!Test.TestTrue(TEXT("the asynchronous capture starts"), Context.KickoffFrame(Material, 64, 64, TEXT("sphere"), -157.5f, -11.25f, Error)))
		{
			Test.AddError(Error);
			return false;
		}
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		while (FPlatformTime::Seconds() < Deadline)
		{
			TArray<uint8> Pixels;
			int32 Width = 0;
			int32 Height = 0;
			if (Context.TryConsumeReadyFramePixels(Pixels, Width, Height, Error))
			{
				Test.TestEqual(TEXT("frame width"), Width, 64);
				Test.TestEqual(TEXT("frame height"), Height, 64);
				Test.TestEqual(TEXT("complete RGBA frame"), Pixels.Num(), 64 * 64 * 4);
				Test.TestFalse(TEXT("the completed capture releases its in-flight state"), Context.IsReadbackInFlight());
				return true;
			}
			if (!Error.IsEmpty())
			{
				Test.AddError(Error);
				return false;
			}
			FlushRenderingCommands();
			FPlatformProcess::Sleep(0.001f);
		}
		Test.AddError(TEXT("Preview readback did not complete."));
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderPreviewReadbackThreadTest,
	"DreamShader.Preview.Readback.ReuseThreadContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPreviewReadbackThreadTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::PreviewReadbackTests;
	using namespace UE::DreamShader::Editor::Private::Tests;
	FScopedDreamShaderGraphBackendPin Backend;
	FDreamShaderCompile2Fixture Fixture(TEXT("PreviewReadbackThreadContract"));
	const FString Path = Fixture.MakeObjectPath(TEXT("M_Readback"));
	Fixture.TrackObjectPath(Path);
	if (!Fixture.WriteSource(*this, TEXT(
		"#pragma material(Backend = Graph, ShadingModel = Unlit, BlendMode = Opaque)\n"
		"export void M_Readback(inout material m) { m.EmissiveColor = float3(0, 1, 0); }\n")))
	{
		return false;
	}
	UE::DreamShader::FDreamShaderError Error;
	if (!ExpectDreamShaderTestCompile(*this, TEXT("readback fixture compiles"),
		CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, true, false), Error))
	{
		return false;
	}
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *Path);
	if (!TestNotNull(TEXT("readback material exists"), Material)) { return false; }
	Material->EnsureIsComplete();
	FlushRenderingCommands();

	FScopedReadbackObserver Observer;
	FDreamShaderPreviewRenderContext Context;
	const FRHIGPUTextureReadback* FirstReadback = nullptr;
	for (int32 Frame = 0; Frame < 3; ++Frame)
	{
		const int32 FirstEvent = Observer.Snapshot().Num();
		if (!ReadFrame(*this, Context, Material)) { return false; }
		const TArray<FReadbackObservation> Events = Observer.Snapshot();
		bool bSawCopy = false;
		bool bSawPoll = false;
		for (int32 Index = FirstEvent; Index < Events.Num(); ++Index)
		{
			const FReadbackObservation& Event = Events[Index];
			if (Event.Event == EPreviewReadbackTestEvent::CopyEnqueued)
			{
				bSawCopy = true;
				if (!FirstReadback) { FirstReadback = Event.Readback; }
				TestTrue(TEXT("same-size captures reuse the readback object"), FirstReadback == Event.Readback);
			}
			else
			{
				bSawPoll = true;
				TestTrue(TEXT("readiness is checked on the render thread, serialized with fence Clear/EnqueueCopy"), Event.bRenderThread);
				TestTrue(TEXT("each capture's readiness is polled only after its own copy was enqueued"), bSawCopy);
			}
		}
		TestTrue(TEXT("the frame enqueues a GPU copy"), bSawCopy);
		TestTrue(TEXT("the frame checks GPU readiness"), bSawPoll);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderPreviewReadbackCancellationTest,
	"DreamShader.Preview.Readback.CancelPending",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPreviewReadbackCancellationTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::PreviewReadbackTests;
	UMaterial* Material = UMaterial::GetDefaultMaterial(MD_Surface);
	if (!TestNotNull(TEXT("the engine default material exists"), Material)) { return false; }
	Material->EnsureIsComplete();
	FlushRenderingCommands();
	FScopedReadbackObserver Observer;
	for (const bool bPollBeforeDisconnect : { false, true })
	{
		TUniquePtr<FDreamShaderPreviewRenderContext> Context = MakeUnique<FDreamShaderPreviewRenderContext>();
		if (!ReadFrame(*this, *Context, Material)) { return false; }
		FString Error;
		if (!TestTrue(TEXT("the frame to cancel starts"), Context->KickoffFrame(Material, 64, 64, TEXT("sphere"), -157.5f, -11.25f, Error)))
		{
			AddError(Error);
			return false;
		}
		if (bPollBeforeDisconnect)
		{
			TArray<uint8> Pixels;
			int32 Width = 0;
			int32 Height = 0;
			Context->TryConsumeReadyFramePixels(Pixels, Width, Height, Error);
			TestTrue(TEXT("the initial poll has no readback error"), Error.IsEmpty());
		}
		// A disconnected client releases its context without another Tick. The copy/poll commands
		// must retain everything they need and fulfill or cancel their promise before its destructor.
		Context.Reset();
		FlushRenderingCommands();
	}
	int32 CopyCount = 0;
	for (const FReadbackObservation& Event : Observer.Snapshot())
	{
		CopyCount += Event.Event == EPreviewReadbackTestEvent::CopyEnqueued ? 1 : 0;
	}
	TestEqual(TEXT("both cancellation paths finish their queued copy commands"), CopyCount, 4);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
