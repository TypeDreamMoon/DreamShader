// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Preview/DreamShaderPreviewSession.h"
#include "HAL/PlatformProcess.h"
#include "Materials/Material.h"
#include "RenderingThread.h"

namespace UE::DreamShader::Editor::Private::PreviewSessionTests
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	struct FScopedManifest
	{
		FString Path = FDreamShaderPreviewRenderer::GetPreviewManifestPath();
		TArray<uint8> Contents;
		bool bExisted = IFileManager::Get().FileExists(*Path);
		FScopedManifest()
		{
			if (bExisted) { verify(FFileHelper::LoadFileToArray(Contents, *Path)); }
		}
		~FScopedManifest()
		{
			if (bExisted) { FFileHelper::SaveArrayToFile(Contents, *Path); }
			else { IFileManager::Get().Delete(*Path); }
		}
	};

	static bool Prepare(FAutomationTestBase& Test, FDreamShaderCompile2Fixture& Fixture, const TCHAR* Color)
	{
		const FString Source = FString::Printf(TEXT(
			"#pragma material(ShadingModel = Unlit, BlendMode = Opaque)\n"
			"export void M_Preview(inout material m) { m.EmissiveColor = float3(%s); }\n"), Color);
		if (!Fixture.WriteSource(Test, Source)) { return false; }
		UE::DreamShader::FDreamShaderError Error;
		if (!ExpectDreamShaderTestCompile(Test, TEXT("preview fixture compiles"),
			CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, true, false), Error)) { return false; }
		const FString Path = Fixture.MakeObjectPath(TEXT("M_Preview"));
		Fixture.TrackObjectPath(Path);
		UMaterial* Material = LoadObject<UMaterial>(nullptr, *Path);
		if (!Test.TestNotNull(TEXT("preview fixture material exists"), Material)) { return false; }
		Material->EnsureIsComplete();
		FlushRenderingCommands();
		return true;
	}

	static FDreamShaderPreviewRequest Request(const FDreamShaderCompile2Fixture& Fixture, int32 Width = 64)
	{
		FDreamShaderPreviewRequest Result;
		Result.SourceFilePath = Fixture.GetSourceFilePath();
		Result.Width = Width;
		Result.Height = 64;
		Result.bForceRecompile = false;
		return Result;
	}

	static bool Begin(FAutomationTestBase& Test, FDreamShaderPreviewSession& Session,
		const FDreamShaderPreviewRequest& Request, const TCHAR* Id, bool bStream)
	{
		FDreamShaderPreviewResult Result;
		const bool bReady = Session.BeginPreview(Request, Id, EDreamShaderPreviewFrameEncoding::RawRGBA8, 1.0 / 60.0, bStream, Result);
		return Test.TestTrue(FString::Printf(TEXT("preview begins: %s"), *Result.Message.ToString()), bReady);
	}

	static bool NextFrame(FAutomationTestBase& Test, FDreamShaderPreviewSession& Session, FDreamShaderPreviewFrame& Frame)
	{
		const double Deadline = FPlatformTime::Seconds() + 3.0;
		while (FPlatformTime::Seconds() < Deadline)
		{
			FString Error;
			if (Session.Tick(FPlatformTime::Seconds(), Frame, Error)) { return true; }
			if (!Error.IsEmpty()) { Test.AddError(Error); return false; }
			FlushRenderingCommands();
			FPlatformProcess::Sleep(0.001f);
		}
		Test.AddError(TEXT("A requested preview frame was never delivered."));
		return false;
	}

	static void ExpectGreen(FAutomationTestBase& Test, const FDreamShaderPreviewFrame& Frame)
	{
		if (!Test.TestEqual(TEXT("raw payload matches dimensions"), Frame.Payload.Num(), Frame.Width * Frame.Height * 4)) { return; }
		const int32 Center = ((Frame.Height / 2) * Frame.Width + Frame.Width / 2) * 4;
		const uint8* Pixel = Frame.Payload.GetData() + Center;
		Test.TestTrue(FString::Printf(TEXT("current green material, not a stale/fallback frame: %d,%d,%d"), Pixel[0], Pixel[1], Pixel[2]),
			Pixel[1] > 150 && Pixel[0] < 80 && Pixel[2] < 80);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderPreviewRawOneShotTest,
	"DreamShader.Preview.Session.RawOneShot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPreviewRawOneShotTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::PreviewSessionTests;
	FScopedDreamShaderGraphBackendPin Backend;
	FScopedManifest Manifest;
	FDreamShaderCompile2Fixture Fixture(TEXT("PreviewRawOneShot"));
	if (!Prepare(*this, Fixture, TEXT("0, 1, 0"))) { return false; }
	FDreamShaderPreviewSession Session;
	if (!Begin(*this, Session, Request(Fixture), TEXT("one-shot"), false)) { return false; }
	TestFalse(TEXT("one-shot does not enable streaming"), Session.IsStreaming());
	FDreamShaderPreviewFrame Frame;
	if (!NextFrame(*this, Session, Frame)) { return false; }
	ExpectGreen(*this, Frame);
	Session.AckFrame(Frame.FrameIndex);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		FString Error;
		TestFalse(TEXT("one-shot sends exactly one frame"), Session.Tick(FPlatformTime::Seconds() + 10.0 + Index, Frame, Error));
		TestTrue(TEXT("completed one-shot stays healthy"), Error.IsEmpty());
		FlushRenderingCommands();
	}
	FDreamShaderPreviewResult Result;
	if (!TestTrue(TEXT("zero-rate streaming request falls back to one frame"), Session.BeginPreview(
		Request(Fixture), TEXT("zero-rate"), EDreamShaderPreviewFrameEncoding::RawRGBA8, 0.0, true, Result))) { return false; }
	TestFalse(TEXT("zero frame rate does not stream"), Session.IsStreaming());
	if (!NextFrame(*this, Session, Frame)) { return false; }
	ExpectGreen(*this, Frame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderPreviewSwitchPendingTest,
	"DreamShader.Preview.Session.SwitchWithPendingFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPreviewSwitchPendingTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::PreviewSessionTests;
	FScopedDreamShaderGraphBackendPin Backend;
	FScopedManifest Manifest;
	FDreamShaderCompile2Fixture Red(TEXT("PreviewSwitchRed"));
	FDreamShaderCompile2Fixture Green(TEXT("PreviewSwitchGreen"));
	if (!Prepare(*this, Red, TEXT("1, 0, 0")) || !Prepare(*this, Green, TEXT("0, 1, 0"))) { return false; }
	FDreamShaderPreviewSession Session;
	if (!Begin(*this, Session, Request(Red), TEXT("old-request"), true)) { return false; }
	FDreamShaderPreviewFrame Frame;
	FString Error;
	TestFalse(TEXT("kickoff leaves the old frame pending"), Session.Tick(FPlatformTime::Seconds(), Frame, Error));
	if (!TestTrue(TEXT("old frame kickoff succeeded"), Error.IsEmpty())) { return false; }
	if (!Begin(*this, Session, Request(Green, 96), TEXT("new-request"), true)) { return false; }
	if (!NextFrame(*this, Session, Frame)) { return false; }
	TestEqual(TEXT("only the new request's dimensions are delivered"), Frame.Width, 96);
	TestEqual(TEXT("session belongs to the new request"), Session.GetRequestId(), FString(TEXT("new-request")));
	ExpectGreen(*this, Frame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderPreviewCapturedMetadataTest,
	"DreamShader.Preview.Session.CapturedFrameMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderPreviewCapturedMetadataTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::PreviewSessionTests;
	FScopedDreamShaderGraphBackendPin Backend;
	FScopedManifest Manifest;
	FDreamShaderCompile2Fixture Fixture(TEXT("PreviewCapturedMetadata"));
	if (!Prepare(*this, Fixture, TEXT("0, 1, 0"))) { return false; }
	FDreamShaderPreviewSession Session;
	const FDreamShaderPreviewRequest Initial = Request(Fixture);
	if (!Begin(*this, Session, Initial, TEXT("camera"), true)) { return false; }
	FDreamShaderPreviewFrame Frame;
	FString Error;
	Session.Tick(FPlatformTime::Seconds(), Frame, Error);
	if (!TestTrue(TEXT("initial capture starts"), Error.IsEmpty())) { return false; }
	Session.SetOrbit(-90.0f, -20.0f);
	Session.SetViewportSize(96, 64);
	if (!NextFrame(*this, Session, Frame)) { return false; }
	TestEqual(TEXT("in-flight pixels retain their capture yaw"), Frame.OrbitYaw, Initial.OrbitYaw);
	TestEqual(TEXT("in-flight pixels retain their capture pitch"), Frame.OrbitPitch, Initial.OrbitPitch);
	TestEqual(TEXT("in-flight dimensions describe captured pixels"), Frame.Width, Initial.Width);
	Session.AckFrame(Frame.FrameIndex);
	if (!NextFrame(*this, Session, Frame)) { return false; }
	TestEqual(TEXT("next capture uses new yaw"), Frame.OrbitYaw, -90.0f);
	TestEqual(TEXT("next capture uses new size"), Frame.Width, 96);
	TestTrue(TEXT("controls applied during readback still request a keyframe"), (Frame.Flags & EDreamShaderPreviewFrameFlags::Keyframe) != 0);
	ExpectGreen(*this, Frame);
	Session.AckFrame(Frame.FrameIndex);
	Session.SetViewportSize(64, 64);
	if (!NextFrame(*this, Session, Frame)) { return false; }
	TestEqual(TEXT("readback can shrink again"), Frame.Width, 64);
	ExpectGreen(*this, Frame);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
