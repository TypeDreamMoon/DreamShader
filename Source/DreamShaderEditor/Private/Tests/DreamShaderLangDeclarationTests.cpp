// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Lang2.Declarations.* -- the 2.0 front end's file-scope declarations, through the
// public ParseDreamShaderLang entry point. Assertions are on tree shape and diagnostic codes,
// never on message text (the editor is localised).

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangParser.h"
#include "Lang/LangSource.h"

#include "Misc/AutomationTest.h"

namespace UE::DreamShader::Editor::Private::Tests::LangDeclarations
{
	using namespace UE::DreamShader::Lang;

	FLangParseResult Parse(const TCHAR* Text, const TCHAR* Path = TEXT("Test.dss"))
	{
		return ParseDreamShaderLang(FLangSourceText(Path, Text), FLangParseOptions());
	}

	inline FString GetDeclName(const FVariableDecl& Decl) { return Decl.Declarator.Name; }
	inline FString GetDeclName(const FFunctionDecl& Decl) { return Decl.Name; }
	inline FString GetDeclName(const FStructDecl& Decl) { return Decl.Name; }

	template <typename T>
	const T* FindDecl(const FModule& Module, const TCHAR* Name)
	{
		for (const FDeclPtr& Decl : Module.Declarations)
		{
			if (const T* Typed = Decl ? Decl->template As<T>() : nullptr)
			{
				if (GetDeclName(*Typed).Equals(Name, ESearchCase::CaseSensitive))
				{
					return Typed;
				}
			}
		}
		return nullptr;
	}

	int32 CountUniforms(const FModule& Module)
	{
		int32 Count = 0;
		Module.ForEachDecl(ENodeKind::VariableDecl, [&Count](const FDecl& Decl)
		{
			if (static_cast<const FVariableDecl&>(Decl).Storage == EStorageClass::Uniform)
			{
				++Count;
			}
		});
		return Count;
	}

	const FFunctionDecl* FindEntry(const FModule& Module)
	{
		const FFunctionDecl* Entry = nullptr;
		Module.ForEachDecl(ENodeKind::FunctionDecl, [&Entry](const FDecl& Decl)
		{
			const FFunctionDecl& Function = static_cast<const FFunctionDecl&>(Decl);
			if (!Entry && Function.IsMaterialEntry())
			{
				Entry = &Function;
			}
		});
		return Entry;
	}

	bool ExpectFirstErrorCode(FAutomationTestBase& Test, const FLangParseResult& Result, const TCHAR* Code, const TCHAR* What)
	{
		const FLangDiagnostic* Error = Result.Diagnostics.FirstError();
		if (!Test.TestNotNull(FString::Printf(TEXT("%s: an error was reported"), What), Error))
		{
			return false;
		}
		Test.AddInfo(FString::Printf(TEXT("%s: %s"), What, *FLangDiagnosticSink::ToWireString(*Error)));
		return Test.TestTrue(
			FString::Printf(TEXT("%s: first error is %s"), What, Code),
			Error->Code.Equals(Code, ESearchCase::CaseSensitive));
	}

	const TCHAR* GMaterialExample = TEXT(
		"// Soft radial glow with a tighter hot core and a slow breathing pulse.\n"
		"\n"
		"#pragma material(ShadingModel = Unlit, BlendMode = Additive, bUsedWithNiagaraSprites = true)\n"
		"\n"
		"/// @group Glow|Look   @desc Multiplied on top of the particle colour\n"
		"uniform float4 Tint = float4(1, 1, 1, 1);\n"
		"\n"
		"/// @group Glow|Look   @desc Overall emissive gain\n"
		"uniform float Intensity = 0.7;\n"
		"\n"
		"/// @group Glow|Motion @desc Amplitude of the slow breathing pulse\n"
		"uniform float Breathe = 0.10;\n"
		"\n"
		"float GlowMask(float2 UV, float Time, float Amount)\n"
		"{\n"
		"    float2 P  = UV * 2.0 - 1.0;\n"
		"    float R2  = dot(P, P);\n"
		"    float Glow  = pow(saturate(1.0 - R2), 2.4);\n"
		"    float Hot   = pow(saturate(1.0 - R2 * 2.6), 5.0) * 0.9;\n"
		"    float Pulse = 1.0 - Amount + Amount * sin(Time * 2.1);\n"
		"    return (Glow + Hot) * Pulse;\n"
		"}\n"
		"\n"
		"export void M_TeleportGlow(inout material m)\n"
		"{\n"
		"    float2 UV = UE.TexCoord(Index = 0);\n"
		"    float4 VC = UE.VertexColor();\n"
		"\n"
		"    m.EmissiveColor = Tint.rgb * VC.rgb * GlowMask(UV, UE.Time(), Breathe) * Intensity;\n"
		"}\n");

	const TCHAR* GFunctionExample = TEXT(
		"/// @asset /MoonToon/MaterialFunctions/Utils/MF_UVChannelSwitch\n"
		"extern float2 MF_UVChannelSwitch(float UVChannelIndex);\n"
		"\n"
		"/// @library MoonToon|Shared\n"
		"/// @desc UV channel select, then scale and offset. ScaleOffset is xy = scale, zw = offset.\n"
		"/// @param UVChannel    Which UV channel to read.\n"
		"/// @param ScaleOffset  xy scales the UV, zw is added afterwards.\n"
		"export float2 MF_ToonUV(float UVChannel = 0.0, float4 ScaleOffset = float4(1, 1, 0, 0))\n"
		"{\n"
		"    float2 Selected = MF_UVChannelSwitch(UVChannel);\n"
		"    return Selected * ScaleOffset.xy + ScaleOffset.zw;\n"
		"}\n"
		"\n"
		"export void SampleTinted(Texture2D Tex, float2 UV, float3 InTint,\n"
		"                         out float3 RGB, out float Alpha)\n"
		"{\n"
		"    float4 Texel = Tex.Sample(UV);\n"
		"    RGB   = Texel.rgb * InTint;\n"
		"    Alpha = Texel.a;\n"
		"}\n");
}

// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2DeclarationsMaterialExampleTest,
	"DreamShader.Lang2.Declarations.MaterialExample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2DeclarationsMaterialExampleTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests::LangDeclarations;
	using namespace UE::DreamShader::Lang;

	const FLangParseResult Result = Parse(GMaterialExample, TEXT("M_TeleportGlow.dss"));
	for (const FLangDiagnostic& Diagnostic : Result.Diagnostics.GetDiagnostics())
	{
		AddInfo(FLangDiagnosticSink::ToWireString(Diagnostic));
	}
	if (!TestTrue(TEXT("the material example parses without errors"), Result.Succeeded()))
	{
		return false;
	}

	const FModule& Module = *Result.Module;
	TestEqual(TEXT("file kind"), static_cast<int32>(Module.FileKind), static_cast<int32>(ELangFileKind::Dss));
	TestEqual(TEXT("six declarations: pragma, three uniforms, two functions"), Module.Declarations.Num(), 6);
	TestEqual(TEXT("one pragma"), Module.CountDecls(ENodeKind::PragmaDecl), 1);
	TestEqual(TEXT("three uniforms"), CountUniforms(Module), 3);
	TestEqual(TEXT("two functions"), Module.CountDecls(ENodeKind::FunctionDecl), 2);

	// The pragma.
	const FPragmaDecl* Pragma = Module.Declarations[0] ? Module.Declarations[0]->As<FPragmaDecl>() : nullptr;
	if (TestNotNull(TEXT("first declaration is the pragma"), Pragma))
	{
		TestEqual(TEXT("pragma kind"), static_cast<int32>(Pragma->PragmaKind), static_cast<int32>(EPragmaKind::Material));
		TestEqual(TEXT("three pragma arguments"), Pragma->Arguments.Num(), 3);
		const FPragmaArgument* ShadingModel = Pragma->Find(TEXT("ShadingModel"));
		if (TestNotNull(TEXT("ShadingModel argument"), ShadingModel))
		{
			TestTrue(TEXT("ShadingModel = Unlit"), ShadingModel->Value.Equals(TEXT("Unlit"), ESearchCase::CaseSensitive));
			TestFalse(TEXT("ShadingModel is not quoted"), ShadingModel->bQuoted);
		}
		const FPragmaArgument* Niagara = Pragma->Find(TEXT("bUsedWithNiagaraSprites"));
		if (TestNotNull(TEXT("bUsedWithNiagaraSprites argument"), Niagara))
		{
			TestTrue(TEXT("bUsedWithNiagaraSprites = true"), Niagara->Value.Equals(TEXT("true"), ESearchCase::CaseSensitive));
		}
	}

	// A uniform with its doc block.
	const FVariableDecl* Intensity = FindDecl<FVariableDecl>(Module, TEXT("Intensity"));
	if (TestNotNull(TEXT("uniform Intensity"), Intensity))
	{
		TestEqual(TEXT("Intensity is uniform"), static_cast<int32>(Intensity->Storage), static_cast<int32>(EStorageClass::Uniform));
		TestEqual(TEXT("Intensity is a scalar"), static_cast<int32>(Intensity->Type.Category), static_cast<int32>(ETypeCategory::Scalar));
		TestEqual(TEXT("Intensity is float"), static_cast<int32>(Intensity->Type.Scalar), static_cast<int32>(EScalarKind::Float));
		TestNotNull(TEXT("Intensity has an initializer"), Intensity->Declarator.Initializer.Get());
		TestEqual(TEXT("two doc directives on Intensity"), Intensity->Doc.Directives.Num(), 2);
		const FDocDirective* Group = Intensity->Doc.Find(TEXT("group"));
		if (TestNotNull(TEXT("@group"), Group))
		{
			TestTrue(TEXT("@group value"), Group->Value.Equals(TEXT("Glow|Look"), ESearchCase::CaseSensitive));
		}
		const FDocDirective* Desc = Intensity->Doc.Find(TEXT("desc"));
		if (TestNotNull(TEXT("@desc"), Desc))
		{
			TestTrue(TEXT("@desc value runs to the end of the line"), Desc->Value.Equals(TEXT("Overall emissive gain"), ESearchCase::CaseSensitive));
		}
	}

	const FVariableDecl* Tint = FindDecl<FVariableDecl>(Module, TEXT("Tint"));
	if (TestNotNull(TEXT("uniform Tint"), Tint))
	{
		TestEqual(TEXT("Tint is a vector"), static_cast<int32>(Tint->Type.Category), static_cast<int32>(ETypeCategory::Vector));
		TestEqual(TEXT("Tint has four components"), Tint->Type.Rows, 4);
		const FExpr* Init = Tint->Declarator.Initializer.Get();
		if (TestNotNull(TEXT("Tint initializer"), Init))
		{
			// `float4(1, 1, 1, 1)` is a call whose callee is a type.
			const FCallExpr* Call = Init->As<FCallExpr>();
			if (TestNotNull(TEXT("Tint initializer is a call"), Call))
			{
				TestTrue(TEXT("callee is a type expression"), Call->Callee && Call->Callee->Is<FTypeExpr>());
				TestEqual(TEXT("four constructor arguments"), Call->Arguments.Num(), 4);
			}
		}
	}

	// The helper and the entry.
	const FFunctionDecl* Helper = FindDecl<FFunctionDecl>(Module, TEXT("GlowMask"));
	if (TestNotNull(TEXT("helper GlowMask"), Helper))
	{
		TestEqual(TEXT("helper is internal"), static_cast<int32>(Helper->Linkage), static_cast<int32>(EFunctionLinkage::Internal));
		TestEqual(TEXT("helper has three params"), Helper->Params.Num(), 3);
		TestNotNull(TEXT("helper has a parsed body"), Helper->Body.Get());
		TestFalse(TEXT("helper body is not opaque"), Helper->bOpaqueBody);
		TestFalse(TEXT("helper is not the entry"), Helper->IsMaterialEntry());
	}

	const FFunctionDecl* Entry = FindEntry(Module);
	if (TestNotNull(TEXT("material entry"), Entry))
	{
		TestTrue(TEXT("entry is M_TeleportGlow"), Entry->Name.Equals(TEXT("M_TeleportGlow"), ESearchCase::CaseSensitive));
		TestEqual(TEXT("entry is exported"), static_cast<int32>(Entry->Linkage), static_cast<int32>(EFunctionLinkage::Export));
		TestTrue(TEXT("entry returns void"), Entry->ReturnType.IsVoid());
		TestEqual(TEXT("entry parameter is inout"), static_cast<int32>(Entry->Params[0].Direction), static_cast<int32>(EParamDirection::InOut));
		TestTrue(TEXT("entry parameter is material"), Entry->Params[0].Type.IsMaterial());
		if (TestNotNull(TEXT("entry body"), Entry->Body.Get()))
		{
			TestEqual(TEXT("entry body has three statements"), Entry->Body->Statements.Num(), 3);
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2DeclarationsFunctionExampleTest,
	"DreamShader.Lang2.Declarations.FunctionExample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2DeclarationsFunctionExampleTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests::LangDeclarations;
	using namespace UE::DreamShader::Lang;

	const FLangParseResult Result = Parse(GFunctionExample, TEXT("MF_ToonUV.dss"));
	for (const FLangDiagnostic& Diagnostic : Result.Diagnostics.GetDiagnostics())
	{
		AddInfo(FLangDiagnosticSink::ToWireString(Diagnostic));
	}
	if (!TestTrue(TEXT("the function example parses without errors"), Result.Succeeded()))
	{
		return false;
	}

	const FModule& Module = *Result.Module;
	TestEqual(TEXT("three functions"), Module.CountDecls(ENodeKind::FunctionDecl), 3);
	TestNull(TEXT("no material entry"), FindEntry(Module));

	const FFunctionDecl* Extern = FindDecl<FFunctionDecl>(Module, TEXT("MF_UVChannelSwitch"));
	if (TestNotNull(TEXT("extern prototype"), Extern))
	{
		TestEqual(TEXT("extern linkage"), static_cast<int32>(Extern->Linkage), static_cast<int32>(EFunctionLinkage::Extern));
		TestTrue(TEXT("extern is a prototype"), Extern->IsPrototype());
		TestNull(TEXT("extern has no body"), Extern->Body.Get());
		const FDocDirective* Asset = Extern->Doc.Find(TEXT("asset"));
		if (TestNotNull(TEXT("@asset"), Asset))
		{
			TestTrue(TEXT("@asset value"), Asset->Value.Equals(TEXT("/MoonToon/MaterialFunctions/Utils/MF_UVChannelSwitch"), ESearchCase::CaseSensitive));
		}
		TestEqual(TEXT("extern returns float2"), Extern->ReturnType.Rows, 2);
	}

	const FFunctionDecl* ToonUV = FindDecl<FFunctionDecl>(Module, TEXT("MF_ToonUV"));
	if (TestNotNull(TEXT("MF_ToonUV"), ToonUV))
	{
		TestEqual(TEXT("MF_ToonUV is exported"), static_cast<int32>(ToonUV->Linkage), static_cast<int32>(EFunctionLinkage::Export));
		TestEqual(TEXT("two params"), ToonUV->Params.Num(), 2);
		if (ToonUV->Params.Num() == 2)
		{
			TestNotNull(TEXT("UVChannel has a default"), ToonUV->Params[0].Default.Get());
			TestNotNull(TEXT("ScaleOffset has a default"), ToonUV->Params[1].Default.Get());
			TestEqual(TEXT("params default to in"), static_cast<int32>(ToonUV->Params[0].Direction), static_cast<int32>(EParamDirection::In));
		}
		TestEqual(TEXT("four doc directives"), ToonUV->Doc.Directives.Num(), 4);
		int32 ParamDirectives = 0;
		for (const FDocDirective& Directive : ToonUV->Doc.Directives)
		{
			if (Directive.Key.Equals(TEXT("param"), ESearchCase::CaseSensitive))
			{
				++ParamDirectives;
				TestTrue(TEXT("@param value starts with the parameter name"),
					Directive.Value.StartsWith(TEXT("UVChannel"), ESearchCase::CaseSensitive) || Directive.Value.StartsWith(TEXT("ScaleOffset"), ESearchCase::CaseSensitive));
			}
		}
		TestEqual(TEXT("two @param directives"), ParamDirectives, 2);
		const FDocDirective* Library = ToonUV->Doc.Find(TEXT("library"));
		if (TestNotNull(TEXT("@library"), Library))
		{
			TestTrue(TEXT("@library value"), Library->Value.Equals(TEXT("MoonToon|Shared"), ESearchCase::CaseSensitive));
		}
	}

	const FFunctionDecl* Sample = FindDecl<FFunctionDecl>(Module, TEXT("SampleTinted"));
	if (TestNotNull(TEXT("SampleTinted"), Sample))
	{
		TestEqual(TEXT("five params"), Sample->Params.Num(), 5);
		if (Sample->Params.Num() == 5)
		{
			TestEqual(TEXT("Tex is a texture"), static_cast<int32>(Sample->Params[0].Type.Category), static_cast<int32>(ETypeCategory::Texture));
			TestEqual(TEXT("RGB is out"), static_cast<int32>(Sample->Params[3].Direction), static_cast<int32>(EParamDirection::Out));
			TestEqual(TEXT("Alpha is out"), static_cast<int32>(Sample->Params[4].Direction), static_cast<int32>(EParamDirection::Out));
		}
		TestFalse(TEXT("SampleTinted is not the material entry"), Sample->IsMaterialEntry());
	}

	return true;
}

// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2DeclarationsStructAndCustomTest,
	"DreamShader.Lang2.Declarations.StructAndCustomBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2DeclarationsStructAndCustomTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests::LangDeclarations;
	using namespace UE::DreamShader::Lang;

	const FLangParseResult Result = Parse(TEXT(
		"/// Inputs a toon ramp needs.\n"
		"struct ToonInputs\n"
		"{\n"
		"    float3 Albedo;\n"
		"    /// @desc Cosine of the light angle\n"
		"    float NdotL;\n"
		"    float Weights[4];\n"
		"};\n"
		"\n"
		"/// @custom\n"
		"float3 AtmosphereLightDir()\n"
		"{\n"
		"    #include \"/Engine/Private/Common.ush\"\n"
		"    // a comment with a } brace\n"
		"    const char* s = \"}\";\n"
		"    { return View.AtmosphereLightDirection[0].xyz; }\n"
		"}\n"
		"\n"
		"static const float K = 1.0;\n"));
	for (const FLangDiagnostic& Diagnostic : Result.Diagnostics.GetDiagnostics())
	{
		AddInfo(FLangDiagnosticSink::ToWireString(Diagnostic));
	}
	if (!TestTrue(TEXT("struct + custom + constant parse without errors"), Result.Succeeded()))
	{
		return false;
	}

	const FModule& Module = *Result.Module;
	TestEqual(TEXT("three declarations"), Module.Declarations.Num(), 3);

	const FStructDecl* Struct = FindDecl<FStructDecl>(Module, TEXT("ToonInputs"));
	if (TestNotNull(TEXT("struct ToonInputs"), Struct))
	{
		TestEqual(TEXT("struct doc free text"), Struct->Doc.FreeText.Num(), 1);
		TestEqual(TEXT("three fields"), Struct->Fields.Num(), 3);
		if (Struct->Fields.Num() == 3)
		{
			TestTrue(TEXT("field 0 name"), Struct->Fields[0].Name.Equals(TEXT("Albedo"), ESearchCase::CaseSensitive));
			TestEqual(TEXT("field 0 is float3"), Struct->Fields[0].Type.Rows, 3);
			TestNotNull(TEXT("field 1 has @desc"), Struct->Fields[1].Doc.Find(TEXT("desc")));
			TestEqual(TEXT("field 2 has one array dimension"), Struct->Fields[2].ArrayDimensions.Num(), 1);
		}
	}

	const FFunctionDecl* Custom = FindDecl<FFunctionDecl>(Module, TEXT("AtmosphereLightDir"));
	if (TestNotNull(TEXT("@custom function"), Custom))
	{
		TestTrue(TEXT("body is opaque"), Custom->bOpaqueBody);
		TestNull(TEXT("no parsed body"), Custom->Body.Get());
		TestTrue(TEXT("raw body keeps the #include line"), Custom->RawBody.Contains(TEXT("#include \"/Engine/Private/Common.ush\""), ESearchCase::CaseSensitive));
		TestTrue(TEXT("raw body keeps the comment"), Custom->RawBody.Contains(TEXT("// a comment with a } brace"), ESearchCase::CaseSensitive));
		TestTrue(TEXT("raw body keeps the string"), Custom->RawBody.Contains(TEXT("\"}\""), ESearchCase::CaseSensitive));
		TestTrue(TEXT("raw body keeps the nested block"), Custom->RawBody.Contains(TEXT("{ return View.AtmosphereLightDirection[0].xyz; }"), ESearchCase::CaseSensitive));
		TestTrue(TEXT("body span starts at the opening brace"), Custom->BodySpan.Length > 0);
	}

	const FVariableDecl* K = FindDecl<FVariableDecl>(Module, TEXT("K"));
	if (TestNotNull(TEXT("static const K"), K))
	{
		TestEqual(TEXT("K storage"), static_cast<int32>(K->Storage), static_cast<int32>(EStorageClass::StaticConst));
	}

	return true;
}

// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2DeclarationsOpaqueBodyTest,
	"DreamShader.Lang2.Declarations.OpaqueBodyIsNotLexed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2DeclarationsOpaqueBodyTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests::LangDeclarations;
	using namespace UE::DreamShader::Lang;

	// Four characters here start no DreamShaderLang token and the `#` is not the first thing on
	// its line, so lexing this as DreamShaderLang would raise DSH2101 five times (`'` appears twice)
	// and DSH2106 once. None of that is a fact about the program: the body is opaque HLSL, sliced
	// out of the source and handed to the shader compiler, so this front end has no opinion about it.
	const FLangParseResult Result = Parse(TEXT(
		"/// @custom\n"
		"float3 HostileButOpaque(float2 UV)\n"
		"{\n"
		"    float3 Result = $Globals.Tint @ UV.x # 2 `;\n"
		"    int Marker = 'x';\n"
		"    return Result;\n"
		"}\n"
		"\n"
		"uniform float Ok = 1.0;\n"));

	for (const FLangDiagnostic& Diagnostic : Result.Diagnostics.GetDiagnostics())
	{
		AddError(FString::Printf(
			TEXT("nothing should be reported about an opaque body, got %s"),
			*FLangDiagnosticSink::ToWireString(Diagnostic)));
	}
	TestEqual(TEXT("no diagnostics at all, warnings included"), Result.Diagnostics.Num(), 0);

	if (!TestNotNull(TEXT("a module was returned"), Result.Module.Get()))
	{
		return false;
	}

	const FFunctionDecl* Custom = FindDecl<FFunctionDecl>(*Result.Module, TEXT("HostileButOpaque"));
	if (TestNotNull(TEXT("the function parsed"), Custom))
	{
		TestTrue(TEXT("its body is opaque"), Custom->bOpaqueBody);
		TestNull(TEXT("its body was not parsed into statements"), Custom->Body.Get());
		TestTrue(
			TEXT("the raw body keeps every character verbatim"),
			Custom->RawBody.Contains(TEXT("$Globals.Tint @ UV.x # 2 `"), ESearchCase::CaseSensitive));
		TestTrue(
			TEXT("the raw body keeps the single quotes"),
			Custom->RawBody.Contains(TEXT("'x'"), ESearchCase::CaseSensitive));
	}

	// The capture stopped at the right brace, so the file goes on being DreamShaderLang after it.
	TestNotNull(
		TEXT("the declaration after the opaque body"),
		FindDecl<FVariableDecl>(*Result.Module, TEXT("Ok")));

	return true;
}

// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2DeclarationsIncludesAndPragmasTest,
	"DreamShader.Lang2.Declarations.IncludesAndPragmas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2DeclarationsIncludesAndPragmasTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests::LangDeclarations;
	using namespace UE::DreamShader::Lang;

	const FLangParseResult Result = Parse(TEXT(
		"#include \"/Game/Shared/Common.dsh\"\n"
		"import \"Hash.dsh\";\n"
		"#pragma region Sampling // the sampling block\n"
		"#pragma layout(Node, Var = UV, X = -1100, Y = -120)\n"
		"#pragma layout(Comment, Name = \"Sampling\", X = 10, Y = 20, W = 900, H = 400)\n"
		"#pragma endregion\n"
		"#pragma once_upon_a_time whatever text\n"
		"uniform float a, b = 2.0, c[2];\n"));
	for (const FLangDiagnostic& Diagnostic : Result.Diagnostics.GetDiagnostics())
	{
		AddInfo(FLangDiagnosticSink::ToWireString(Diagnostic));
	}
	if (!TestTrue(TEXT("includes, pragmas and declarators parse without errors"), Result.Succeeded()))
	{
		return false;
	}

	const FModule& Module = *Result.Module;
	TestEqual(TEXT("two includes"), Module.CountDecls(ENodeKind::IncludeDecl), 2);
	TestEqual(TEXT("five pragmas"), Module.CountDecls(ENodeKind::PragmaDecl), 5);
	TestEqual(TEXT("three uniforms from one declaration"), CountUniforms(Module), 3);
	TestEqual(TEXT("ten declarations in all"), Module.Declarations.Num(), 10);

	if (Module.Declarations.Num() >= 8)
	{
		const FIncludeDecl* Include = Module.Declarations[0]->As<FIncludeDecl>();
		if (TestNotNull(TEXT("#include"), Include))
		{
			TestTrue(TEXT("#include path"), Include->Path.Equals(TEXT("/Game/Shared/Common.dsh"), ESearchCase::CaseSensitive));
			TestFalse(TEXT("#include spelling"), Include->bImportSpelling);
		}
		const FIncludeDecl* Import = Module.Declarations[1]->As<FIncludeDecl>();
		if (TestNotNull(TEXT("import"), Import))
		{
			TestTrue(TEXT("import path"), Import->Path.Equals(TEXT("Hash.dsh"), ESearchCase::CaseSensitive));
			TestTrue(TEXT("import spelling"), Import->bImportSpelling);
		}
		const FPragmaDecl* Region = Module.Declarations[2]->As<FPragmaDecl>();
		if (TestNotNull(TEXT("#pragma region"), Region))
		{
			TestEqual(TEXT("region kind"), static_cast<int32>(Region->PragmaKind), static_cast<int32>(EPragmaKind::Region));
			TestTrue(TEXT("region title, trailing comment stripped"), Region->Text.Equals(TEXT("Sampling"), ESearchCase::CaseSensitive));
		}
		const FPragmaDecl* Layout = Module.Declarations[3]->As<FPragmaDecl>();
		if (TestNotNull(TEXT("#pragma layout"), Layout))
		{
			TestEqual(TEXT("layout kind"), static_cast<int32>(Layout->PragmaKind), static_cast<int32>(EPragmaKind::Layout));
			TestEqual(TEXT("layout has four arguments"), Layout->Arguments.Num(), 4);
			if (Layout->Arguments.Num() == 4)
			{
				TestTrue(TEXT("first argument is positional"), Layout->Arguments[0].Key.IsEmpty());
				TestTrue(TEXT("positional value is Node"), Layout->Arguments[0].Value.Equals(TEXT("Node"), ESearchCase::CaseSensitive));
			}
			const FPragmaArgument* X = Layout->Find(TEXT("X"));
			if (TestNotNull(TEXT("X argument"), X))
			{
				TestTrue(TEXT("negative number value"), X->Value.Equals(TEXT("-1100"), ESearchCase::CaseSensitive));
			}
		}
		const FPragmaDecl* Comment = Module.Declarations[4]->As<FPragmaDecl>();
		if (TestNotNull(TEXT("#pragma layout(Comment)"), Comment))
		{
			const FPragmaArgument* Name = Comment->Find(TEXT("Name"));
			if (TestNotNull(TEXT("Name argument"), Name))
			{
				TestTrue(TEXT("quoted value stripped"), Name->Value.Equals(TEXT("Sampling"), ESearchCase::CaseSensitive));
				TestTrue(TEXT("quoted flag"), Name->bQuoted);
			}
		}
		const FPragmaDecl* EndRegion = Module.Declarations[5]->As<FPragmaDecl>();
		if (TestNotNull(TEXT("#pragma endregion"), EndRegion))
		{
			TestEqual(TEXT("endregion kind"), static_cast<int32>(EndRegion->PragmaKind), static_cast<int32>(EPragmaKind::EndRegion));
		}
		const FPragmaDecl* Unknown = Module.Declarations[6]->As<FPragmaDecl>();
		if (TestNotNull(TEXT("unknown pragma"), Unknown))
		{
			TestEqual(TEXT("unknown kind"), static_cast<int32>(Unknown->PragmaKind), static_cast<int32>(EPragmaKind::Unknown));
			TestTrue(TEXT("unknown pragma name"), Unknown->Name.Equals(TEXT("once_upon_a_time"), ESearchCase::CaseSensitive));
			TestTrue(TEXT("unknown pragma text"), Unknown->Text.Equals(TEXT("whatever text"), ESearchCase::CaseSensitive));
		}
	}

	const FVariableDecl* B = FindDecl<FVariableDecl>(Module, TEXT("b"));
	if (TestNotNull(TEXT("second declarator b"), B))
	{
		TestEqual(TEXT("b keeps the storage"), static_cast<int32>(B->Storage), static_cast<int32>(EStorageClass::Uniform));
		TestNotNull(TEXT("b has its initializer"), B->Declarator.Initializer.Get());
	}
	const FVariableDecl* C = FindDecl<FVariableDecl>(Module, TEXT("c"));
	if (TestNotNull(TEXT("third declarator c"), C))
	{
		TestEqual(TEXT("c has one array dimension"), C->Declarator.ArrayDimensions.Num(), 1);
		TestNull(TEXT("c has no initializer"), C->Declarator.Initializer.Get());
	}

	return true;
}

// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2DeclarationsDirectiveSpansTest,
	"DreamShader.Lang2.Declarations.DirectiveSpans",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2DeclarationsDirectiveSpansTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests::LangDeclarations;
	using namespace UE::DreamShader::Lang;

	// A `#` line reaches the parser as ONE token whose text has had its leading whitespace trimmed
	// and its trailing `//` comment removed, while the token's span still covers the whole physical
	// line. Everything derived from that span must therefore be anchored at the FRONT of the line:
	// measuring back from the end shifts every offset right by the length of a comment that is no
	// longer part of the text. Both lines below carry a trailing comment for exactly that reason,
	// and the assertions slice the source back out rather than hand-counting offsets.
	const FLangSourceText Source(
		TEXT("Spans.dss"),
		TEXT("#include \"/Game/Shared/Common.dsh\" // pull in the shared helpers\n")
		TEXT("#pragma layout(Node, Var = UV, X = -1100) // where the node goes\n"));

	const FLangParseResult Result = ParseDreamShaderLang(Source, FLangParseOptions());
	for (const FLangDiagnostic& Diagnostic : Result.Diagnostics.GetDiagnostics())
	{
		AddInfo(FLangDiagnosticSink::ToWireString(Diagnostic));
	}
	if (!TestTrue(TEXT("both directives parse"), Result.Succeeded()))
	{
		return false;
	}
	if (!TestEqual(TEXT("two declarations"), Result.Module->Declarations.Num(), 2))
	{
		return false;
	}

	const FIncludeDecl* Include = Result.Module->Declarations[0]->As<FIncludeDecl>();
	if (TestNotNull(TEXT("the #include"), Include))
	{
		TestTrue(
			TEXT("PathSpan slices back to the quoted path, not into the comment"),
			Source.Slice(Include->PathSpan).Equals(TEXT("\"/Game/Shared/Common.dsh\""), ESearchCase::CaseSensitive));
	}

	const FPragmaDecl* Layout = Result.Module->Declarations[1]->As<FPragmaDecl>();
	if (TestNotNull(TEXT("the #pragma layout"), Layout)
		&& TestEqual(TEXT("three pragma arguments"), Layout->Arguments.Num(), 3))
	{
		TestTrue(
			TEXT("the positional argument's span slices back to 'Node'"),
			Source.Slice(Layout->Arguments[0].Span).Equals(TEXT("Node"), ESearchCase::CaseSensitive));

		const FPragmaArgument* X = Layout->Find(TEXT("X"));
		if (TestNotNull(TEXT("the X argument"), X))
		{
			TestTrue(
				TEXT("a keyed argument's span covers the whole `X = -1100`"),
				Source.Slice(X->Span).Equals(TEXT("X = -1100"), ESearchCase::CaseSensitive));
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2DeclarationsDiagnosticsTest,
	"DreamShader.Lang2.Declarations.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2DeclarationsDiagnosticsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests::LangDeclarations;
	using namespace UE::DreamShader::Lang;

	struct FCase
	{
		const TCHAR* What;
		const TCHAR* Source;
		const TCHAR* Code;
		const TCHAR* Path;
	};

	// Every source ends with a declaration that must still parse after the error (recovery).
	const FCase Cases[] =
	{
		{ TEXT("stray token at file scope"),        TEXT(") \nuniform float Ok = 1.0;\n"),                           TEXT("DSH3200"), TEXT("Test.dss") },
		{ TEXT("stray preprocessor directive"),     TEXT("#if 1\nuniform float Ok = 1.0;\n#endif\n"),               TEXT("DSH3201"), TEXT("Test.dss") },
		{ TEXT("malformed pragma"),                 TEXT("#pragma material(ShadingModel =\nuniform float Ok = 1.0;\n"), TEXT("DSH3202"), TEXT("Test.dss") },
		{ TEXT("malformed include"),                TEXT("#include nopath\nuniform float Ok = 1.0;\n"),               TEXT("DSH3203"), TEXT("Test.dss") },
		{ TEXT("missing type"),                     TEXT("uniform = 1;\nuniform float Ok = 1.0;\n"),                  TEXT("DSH3204"), TEXT("Test.dss") },
		{ TEXT("missing name"),                     TEXT("float ;\nuniform float Ok = 1.0;\n"),                       TEXT("DSH3205"), TEXT("Test.dss") },
		{ TEXT("no body and no semicolon"),         TEXT("float f() 5\nuniform float Ok = 1.0;\n"),                   TEXT("DSH3206"), TEXT("Test.dss") },
		{ TEXT("extern with a body"),               TEXT("extern float f() { return 1.0; }\nuniform float Ok = 1.0;\n"), TEXT("DSH3207"), TEXT("Test.dss") },
		{ TEXT("function without a body"),          TEXT("float f();\nuniform float Ok = 1.0;\n"),                    TEXT("DSH3208"), TEXT("Test.dss") },
		{ TEXT("export in a header"),               TEXT("export float f() { return 1.0; }\nuniform float Ok = 1.0;\n"), TEXT("DSH3210"), TEXT("Test.dsh") },
		{ TEXT("bad storage combination"),          TEXT("uniform static float x = 1.0;\nuniform float Ok = 1.0;\n"), TEXT("DSH3213"), TEXT("Test.dss") },
		{ TEXT("storage on a function"),            TEXT("uniform float f() { return 1.0; }\nuniform float Ok = 1.0;\n"), TEXT("DSH3213"), TEXT("Test.dss") },
		{ TEXT("default on an out parameter"),      TEXT("float f(out float x = 1.0) { return 1.0; }\nuniform float Ok = 1.0;\n"), TEXT("DSH3214"), TEXT("Test.dss") },
		{ TEXT("missing semicolon"),                TEXT("uniform float x = 1.0\nuniform float Ok = 1.0;\n"),          TEXT("DSH3216"), TEXT("Test.dss") },
		{ TEXT("legacy declaration word"),          TEXT("Function float Luma(in vec3 c) { return c.x; }\nuniform float Ok = 1.0;\n"), TEXT("DSH3222"), TEXT("Test.dss") },
	};

	for (const FCase& Case : Cases)
	{
		const FLangParseResult Result = Parse(Case.Source, Case.Path);
		TestFalse(FString::Printf(TEXT("%s: parse does not succeed"), Case.What), Result.Succeeded());
		ExpectFirstErrorCode(*this, Result, Case.Code, Case.What);

		if (TestNotNull(FString::Printf(TEXT("%s: a module is still returned"), Case.What), Result.Module.Get()))
		{
			TestNotNull(
				FString::Printf(TEXT("%s: the declaration after the error still parsed"), Case.What),
				FindDecl<FVariableDecl>(*Result.Module, TEXT("Ok")));
		}
	}

	// Warnings do not fail the parse.
	{
		const FLangParseResult Result = Parse(TEXT("/// text with a lonely @ sign\nuniform float Ok = 1.0;\n/// orphan block at the end\n"));
		TestTrue(TEXT("warnings only: parse succeeds"), Result.Succeeded());
		int32 Warnings = 0;
		bool bSawMalformed = false;
		bool bSawOrphan = false;
		for (const FLangDiagnostic& Diagnostic : Result.Diagnostics.GetDiagnostics())
		{
			if (Diagnostic.Severity == ELangSeverity::Warning)
			{
				++Warnings;
				bSawMalformed |= Diagnostic.Code.Equals(TEXT("DSH3220"), ESearchCase::CaseSensitive);
				bSawOrphan |= Diagnostic.Code.Equals(TEXT("DSH3221"), ESearchCase::CaseSensitive);
			}
		}
		TestEqual(TEXT("two warnings"), Warnings, 2);
		TestTrue(TEXT("DSH3220 malformed @"), bSawMalformed);
		TestTrue(TEXT("DSH3221 orphan doc block"), bSawOrphan);
		const FVariableDecl* Ok = FindDecl<FVariableDecl>(*Result.Module, TEXT("Ok"));
		if (TestNotNull(TEXT("Ok parsed"), Ok))
		{
			TestEqual(TEXT("the lonely @ line stays as free text"), Ok->Doc.FreeText.Num(), 1);
			TestEqual(TEXT("no directive was made from it"), Ok->Doc.Directives.Num(), 0);
		}
	}

	// The legacy front end is not available yet.
	{
		const FLangParseResult Result = Parse(TEXT("Shader(Name=\"X\") { }"), TEXT("Legacy.dsm"));
		TestFalse(TEXT(".dsm does not succeed"), Result.Succeeded());
		ExpectFirstErrorCode(*this, Result, TEXT("DSH2199"), TEXT(".dsm routed to the legacy front end"));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
