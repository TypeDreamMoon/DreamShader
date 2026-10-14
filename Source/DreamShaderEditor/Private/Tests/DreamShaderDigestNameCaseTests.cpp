// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Provenance.DigestNameCase -- the digest compares names without case (DSD5).
//
// FName compares without case and a package keeps one spelling per name, so a function output generated as `result`
// in a package that also holds a `Result` loads back as `Result`. Under DSD4 that was a different digest, and the next
// session's compile refused the asset as hand-edited. The spelling of a name in the digest must not matter; what the
// name is still must.

#include "Tests/DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderGeneratedAssetDigest.h"

#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialFunction.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDigestNameCaseTest,
	"DreamShader.Provenance.DigestNameCase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDigestNameCaseTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;

	UMaterialFunction* Function = NewObject<UMaterialFunction>(GetTransientPackage(), NAME_None, RF_Transient);
	auto* Input = Cast<UMaterialExpressionFunctionInput>(
		UMaterialEditingLibrary::CreateMaterialExpressionInFunction(Function, UMaterialExpressionFunctionInput::StaticClass()));
	auto* Custom = Cast<UMaterialExpressionCustom>(
		UMaterialEditingLibrary::CreateMaterialExpressionInFunction(Function, UMaterialExpressionCustom::StaticClass()));
	auto* Output = Cast<UMaterialExpressionFunctionOutput>(
		UMaterialEditingLibrary::CreateMaterialExpressionInFunction(Function, UMaterialExpressionFunctionOutput::StaticClass()));
	if (!TestTrue(TEXT("the graph was made"), Input && Custom && Output))
	{
		return false;
	}

	Input->InputName = FName(TEXT("value"));
	Custom->AdditionalOutputs.AddDefaulted_GetRef().OutputName = FName(TEXT("color"));
	Output->OutputName = FName(TEXT("result"));
	const FString Lower = BuildOutputDigest(Function);

	Input->InputName = FName(TEXT("Value"));
	Custom->AdditionalOutputs[0].OutputName = FName(TEXT("Color"));
	Output->OutputName = FName(TEXT("Result"));
	const FString Upper = BuildOutputDigest(Function);

	TestEqual(TEXT("a name spelled in another case is the same digest"), Upper, Lower);
	TestTrue(FString::Printf(TEXT("and the digest is DSD5 (%s)"), *Lower), Lower.StartsWith(TEXT("DSD5-")));

	Output->OutputName = FName(TEXT("other"));
	TestNotEqual(TEXT("another name is another digest"), BuildOutputDigest(Function), Lower);

	Function->MarkAsGarbage();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
