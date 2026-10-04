// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Lang2.LegacyRules.* -- the middle end's rules that are claims about ONE value rather than about a
// whole graph, and so read better as a unit test than as a corpus golden:
//
//   SourceStamping      FIRBuildOptions::StampSourcePath reaches every place a file is written down.
//   StatementBindings   the probe table: which statement bound which name, in statement order, surviving the passes.
//   PinNames            `/// @pin`: an engine pin name that is not an identifier.
//   LegacyScope         the 1.x rules are rules of a 1.x BODY: the same text in a `.dss` is refused.
//   InstanceSchema      BuildParameterSchemaFromIR and the schema's lookups.
//
// The whole-graph halves of the same agreements are fixtures: Tests/Corpus/IR/Branches/B6_*, Tests/Corpus/IR/Instances,
// Tests/Corpus/Legacy/IR.
//
// Core only: the hand-built test catalog.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "IR/IR.h"
#include "IR/IRInstanceSchema.h"

#include "Algo/Reverse.h"

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::LegacyRulesTests
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;

	using FIRRun = UE::DreamShader::Editor::Private::Tests::FDreamShaderIRRun;
	using FIRRunOptions = UE::DreamShader::Editor::Private::Tests::FDreamShaderIRRunOptions;

	inline void Lower(FIRRun& Run, const TCHAR* FileName, const TCHAR* Text, const FIRRunOptions& Options = FIRRunOptions())
	{
		UE::DreamShader::Editor::Private::Tests::RunDreamShaderIRPipeline(FileName, Text, Options, Run);
	}

	inline bool HasLine(const TArray<FString>& Lines, const TCHAR* Code)
	{
		const FString Needle = FString::Printf(TEXT("%s:"), Code);
		return Lines.ContainsByPredicate([&Needle](const FString& Line) { return Line.StartsWith(Needle, ESearchCase::CaseSensitive); });
	}

	inline const FIRNode* FindNode(const FIRGraph& Graph, const EIROp Op, const int32 Skip = 0)
	{
		int32 Seen = 0;
		for (const FIRNode& Node : Graph.Nodes)
		{
			if (Node.Op == Op && Seen++ == Skip)
			{
				return &Node;
			}
		}
		return nullptr;
	}

	inline FString PropertyText(const FIRNode& Node, const TCHAR* Name)
	{
		const FIRProperty* Property = Node.FindProperty(Name);
		return Property ? Property->Value.S : FString();
	}
}

// ---------------------------------------------------------------------------------------------
// Debt B5
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLegacyRulesSourceStampingTest,
	"DreamShader.Lang2.LegacyRules.SourceStamping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLegacyRulesSourceStampingTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LegacyRulesTests;

	const TCHAR* Source = TEXT(
		"/// @custom\n"
		"float3 Posterise(float3 Colour, float Steps)\n"
		"{\n"
		"    return floor(Colour * Steps) / Steps;\n"
		"}\n"
		"float Twice(float X)\n"
		"{\n"
		"    return X * 2;\n"
		"}\n"
		"export void M_Stamped(inout material m)\n"
		"{\n"
		"    float Steps = Twice(2);\n"
		"    m.EmissiveColor = Posterise(float3(1, 0.5, 0.25), Steps);\n"
		"}\n");

	// With a stamper: every file the builder writes down went through it.
	{
		FIRRunOptions Options;
		Options.StampSourcePath = [](const FString& File) { return FString(TEXT("Stamped/")) + FPaths::GetCleanFilename(File); };
		FIRRun Run;
		Lower(Run, TEXT("C:/Somewhere/Deep/M_Stamped.dss"), Source, Options);
		if (!TestTrue(FString::Printf(TEXT("the source builds (%s)"), *Run.ErrorText()), Run.Succeeded() && Run.Module.IsValid() && Run.Module->Products.Num() == 1))
		{
			return false;
		}
		const FIRProduct& Product = Run.Module->Products[0];
		TestTrue(FString::Printf(TEXT("the product's file is stamped ('%s')"), *Product.Source.File), Product.Source.File.StartsWith(TEXT("Stamped/")));

		for (const FIRNode& Node : Product.Graph.Nodes)
		{
			if (!Node.Source.File.IsEmpty() && !Node.Source.File.StartsWith(TEXT("Stamped/")))
			{
				AddError(FString::Printf(TEXT("a %s node names '%s'"), LexToString(Node.Op), *Node.Source.File));
			}
			if (!Node.Source.CallSiteFile.IsEmpty() && !Node.Source.CallSiteFile.StartsWith(TEXT("Stamped/")))
			{
				AddError(FString::Printf(TEXT("a %s node's call site names '%s'"), LexToString(Node.Op), *Node.Source.CallSiteFile));
			}
		}
		for (const FIRStatementBinding& Binding : Product.Graph.StatementBindings)
		{
			if (!Binding.Source.File.IsEmpty() && !Binding.Source.File.StartsWith(TEXT("Stamped/")))
			{
				AddError(FString::Printf(TEXT("the binding of '%s' names '%s'"), *Binding.Name, *Binding.Source.File));
			}
		}

		const FIRNode* Custom = FindNode(Product.Graph, EIROp::Custom);
		if (TestNotNull(TEXT("the Custom node"), Custom))
		{
			const FString Code = PropertyText(*Custom, Prop::Code);
			TestTrue(FString::Printf(TEXT("the Begin marker is stamped\n%s"), *Code), Code.Contains(TEXT("// Begin DreamShader source: Stamped/M_Stamped.dss")));
			TestTrue(TEXT("the End marker is stamped"), Code.Contains(TEXT("// End DreamShader source: Stamped/M_Stamped.dss")));
			TestFalse(TEXT("no drive letter in the code"), Code.Contains(TEXT("C:/")));
		}
	}

	// Without one the paths are what the front end was given.
	{
		FIRRun Run;
		Lower(Run, TEXT("C:/Somewhere/Deep/M_Stamped.dss"), Source);
		const FIRNode* Custom = Run.Module.IsValid() && Run.Module->Products.Num() == 1 ? FindNode(Run.Module->Products[0].Graph, EIROp::Custom) : nullptr;
		if (TestNotNull(TEXT("the Custom node, unstamped"), Custom))
		{
			TestTrue(TEXT("the marker names the path as given"), PropertyText(*Custom, Prop::Code).Contains(TEXT("C:/Somewhere/Deep/M_Stamped.dss")));
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// The probe table
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLegacyRulesStatementBindingsTest,
	"DreamShader.Lang2.LegacyRules.StatementBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLegacyRulesStatementBindingsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LegacyRulesTests;

	const TCHAR* Source = TEXT(
		"/// @custom\n"
		"void Split(float V, out float Half)\n"
		"{\n"
		"    Half = V * 0.5;\n"
		"}\n"
		"export void M_Bindings(inout material m)\n"
		"{\n"
		"    float a = 1;\n"
		"    float3 c = UE.VertexColor().rgb;\n"
		"    c.x = a;\n"
		"    c += 1;\n"
		"    float OutLocal;\n"
		"    Split(a, OutLocal);\n"
		"    m.BaseColor = c * OutLocal;\n"
		"}\n");

	// Before the passes: every statement that bound a name, in statement order.
	{
		FIRRunOptions Options;
		Options.bRunPasses = false;
		FIRRun Run;
		Lower(Run, TEXT("M_Bindings.dss"), Source, Options);
		if (!TestTrue(FString::Printf(TEXT("the source builds (%s)"), *Run.ErrorText()), Run.Module.IsValid() && Run.Module->Products.Num() == 1 && Run.Errors.Num() == 0))
		{
			return false;
		}
		TArray<FString> Names;
		for (const FIRStatementBinding& Binding : Run.Module->Products[0].Graph.StatementBindings)
		{
			Names.Add(Binding.Name);
		}
		const FString Joined = FString::Join(Names, TEXT(", "));
		TestTrue(FString::Printf(TEXT("the names in statement order (actual: %s)"), *Joined), Joined.Equals(TEXT("a, c, c, c, OutLocal, m.BaseColor"), ESearchCase::CaseSensitive));
		for (const FIRStatementBinding& Binding : Run.Module->Products[0].Graph.StatementBindings)
		{
			TestTrue(FString::Printf(TEXT("'%s' names a node before the passes"), *Binding.Name), Run.Module->Products[0].Graph.IsValidValue(Binding.Value));
		}
	}

	// After them: a binding either still names a node of the graph, or says it names none. Never a stale index.
	{
		FIRRun Run;
		Lower(Run, TEXT("M_Bindings.dss"), Source);
		if (Run.Module.IsValid() && Run.Module->Products.Num() == 1)
		{
			const FIRGraph& Graph = Run.Module->Products[0].Graph;
			for (const FIRStatementBinding& Binding : Graph.StatementBindings)
			{
				TestTrue(FString::Printf(TEXT("'%s' is valid or None after the passes"), *Binding.Name), !Binding.Value.IsValid() || Graph.IsValidValue(Binding.Value));
			}
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// `/// @pin`
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLegacyRulesPinNamesTest,
	"DreamShader.Lang2.LegacyRules.PinNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLegacyRulesPinNamesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LegacyRulesTests;

	{
		FIRRun Run;
		Lower(Run, TEXT("MF_Pins.dss"), TEXT(
			"/// @pin BaseColor Base Color\n"
			"/// @pin Tint Tint Colour\n"
			"/// @pin Result Out\n"
			"export float3 MF_Pins(float3 BaseColor, out float3 Tint)\n"
			"{\n"
			"    Tint = BaseColor * 0.5;\n"
			"    return BaseColor;\n"
			"}\n"));
		if (!TestTrue(FString::Printf(TEXT("the library builds (%s)"), *Run.ErrorText()), Run.Succeeded() && Run.Module.IsValid() && Run.Module->Products.Num() == 1))
		{
			return false;
		}

		const FBoundFunction& Function = Run.Bind.Bound->Functions[0];
		TestTrue(TEXT("the bound parameter carries the engine name"), Function.Params.Num() == 2 && Function.Params[0].PinName == TEXT("Base Color") && Function.Params[1].PinName == TEXT("Tint Colour"));

		const FIRGraph& Graph = Run.Module->Products[0].Graph;
		if (TestEqual(TEXT("one input"), Graph.FunctionInputs.Num(), 1) && TestEqual(TEXT("two outputs"), Graph.FunctionOutputs.Num(), 2))
		{
			TestEqual(TEXT("the input's engine name"), PropertyText(Graph.Nodes[Graph.FunctionInputs[0]], Prop::InputName), FString(TEXT("Base Color")));
			TestEqual(TEXT("the return value's engine name"), PropertyText(Graph.Nodes[Graph.FunctionOutputs[0]], Prop::OutputName), FString(TEXT("Out")));
			TestEqual(TEXT("the out parameter's engine name"), PropertyText(Graph.Nodes[Graph.FunctionOutputs[1]], Prop::OutputName), FString(TEXT("Tint Colour")));
			// The variable keeps the identifier: that is what a probe and a layout hint are keyed by.
			TestEqual(TEXT("the input's variable"), Graph.Nodes[Graph.FunctionInputs[0]].DebugName, FString(TEXT("BaseColor")));
		}
	}

	struct FCase { const TCHAR* What; const TCHAR* Text; const TCHAR* Code; bool bError; };
	const FCase Cases[] = {
		{ TEXT("a parameter that does not exist"), TEXT("/// @pin Missing X\nexport float MF_P(float A)\n{\n    return A;\n}\n"), TEXT("DSH7225"), true },
		{ TEXT("one word only"), TEXT("/// @pin OnlyOneWord\nexport float MF_P(float OnlyOneWord)\n{\n    return OnlyOneWord;\n}\n"), TEXT("DSH7227"), true },
		{ TEXT("on a uniform"), TEXT("/// @pin A Alpha\nuniform float A = 1;\nexport float MF_P(float B)\n{\n    return A * B;\n}\n"), TEXT("DSH7224"), false },
	};
	for (const FCase& Case : Cases)
	{
		FIRRun Run;
		Lower(Run, TEXT("MF_P.dss"), Case.Text);
		TestTrue(
			FString::Printf(TEXT("%s: %s (errors: %s; warnings: %s)"), Case.What, Case.Code, *Run.ErrorText(), *Run.WarningText()),
			HasLine(Run.Errors, Case.Code) || HasLine(Run.Warnings, Case.Code));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// The 1.x rules belong to 1.x bodies
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLegacyRulesLegacyScopeTest,
	"DreamShader.Lang2.LegacyRules.LegacyScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLegacyRulesLegacyScopeTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LegacyRulesTests;

	// One line of meaning, written in both languages.
	{
		FIRRun Legacy;
		Lower(Legacy, TEXT("M_Scope.dsm"), TEXT(
			"Shader(Name=\"M_Scope\")\n"
			"{\n"
			"    Outputs = { vec3 Color; Base.emissivecolor = Color; }\n"
			"    Graph = {\n"
			"        float2 MyUV = UE.TexCoord(Index = 0);\n"
			"        Color = mix(vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0), myuv.x);\n"
			"    }\n"
			"}\n"));
		TestTrue(FString::Printf(TEXT("the 1.x body builds (%s)"), *Legacy.ErrorText()), Legacy.Succeeded());
		TestTrue(FString::Printf(TEXT("`mix` is said out loud: DSH5277 (warnings: %s)"), *Legacy.WarningText()), HasLine(Legacy.Warnings, TEXT("DSH5277")));
		TestTrue(TEXT("a name in another case is said out loud: DSH5275"), HasLine(Legacy.Warnings, TEXT("DSH5275")) || HasLine(Legacy.Infos, TEXT("DSH5275")));
	}
	{
		FIRRun Strict;
		Lower(Strict, TEXT("M_Scope.dss"), TEXT(
			"export void M_Scope(inout material m)\n"
			"{\n"
			"    float2 MyUV = UE.TexCoord(CoordinateIndex = 0);\n"
			"    m.EmissiveColor = mix(float3(1, 0, 0), float3(0, 0, 1), myuv.x);\n"
			"}\n"));
		TestFalse(TEXT("the same text in a `.dss` is refused"), Strict.Succeeded());
		TestTrue(FString::Printf(TEXT("`mix`: DSH4250 (errors: %s)"), *Strict.ErrorText()), HasLine(Strict.Errors, TEXT("DSH4250")));
		TestTrue(TEXT("`myuv`: DSH4200"), HasLine(Strict.Errors, TEXT("DSH4200")));
	}

	// A `.dss` that includes a mixed header keeps its own body strict: the header's 1.x function is callable, the GLSL
	// spelling in the `.dss` body is still an error.
	{
		const FString Directory = FPaths::Combine(UE::DreamShader::Editor::Private::Tests::GetDreamShaderCorpusRoot(), TEXT("Migrate"), TEXT("Basics"));
		FIRRunOptions Options;
		Options.IncludeDirectory = Directory;
		FIRRun Run;
		Lower(Run, *FPaths::Combine(Directory, TEXT("Scope_Includer.dss")), TEXT(
			"#include \"MigrateShared.dsh\"\n"
			"export void M_ScopeIncluder(inout material m)\n"
			"{\n"
			"    float L = Luma(float3(1, 0.5, 0.25));\n"
			"    m.EmissiveColor = mix(float3(L, L, L), float3(0, 0, 1), 0.5);\n"
			"}\n"), Options);
		TestTrue(FString::Printf(TEXT("the includer's own body is strict: DSH4250 (errors: %s)"), *Run.ErrorText()), HasLine(Run.Errors, TEXT("DSH4250")));
		TestFalse(TEXT("and the header's 1.x function was found: no DSH4208"), HasLine(Run.Errors, TEXT("DSH4208")));
	}

	// The shapes migrate reads off the bound module.
	{
		FIRRun Run;
		Lower(Run, TEXT("M_Shapes.dsm"), TEXT(
			"VirtualFunction(Name=\"MF_Two\")\n"
			"{\n"
			"    Options = { Asset = Path(Game, \"Functions/MF_Two\"); }\n"
			"    Inputs = { float3 Colour; }\n"
			"    Outputs = { float3 Result; float Extra; }\n"
			"}\n"
			"Shader(Name=\"M_Shapes\")\n"
			"{\n"
			"    Outputs = { vec3 Color; float Alpha; Base.EmissiveColor = Color; Base.Opacity = Alpha; }\n"
			"    Graph = {\n"
			"        Alpha = MF_Two(vec3(1.0, 1.0, 1.0), Output = \"Extra\");\n"
			"        MF_Two(vec3(0.5, 0.5, 0.5), Color, Unused);\n"
			"    }\n"
			"}\n"));
		if (TestTrue(FString::Printf(TEXT("the source binds (%s)"), *Run.ErrorText()), Run.Bind.Bound.IsValid() && Run.Errors.Num() == 0))
		{
			int32 Selections = 0;
			int32 Receivers = 0;
			int32 ImplicitLocals = 0;
			for (const TPair<const FNode*, FBoundExpr>& Pair : Run.Bind.Bound->Expressions)
			{
				const FBoundExpr& Expr = Pair.Value;
				if (Expr.Kind == EBoundExprKind::FunctionCallOutput)
				{
					++Selections;
					TestEqual(TEXT("the selection's ordinal counts the return value first"), Expr.FieldIndex, 1);
				}
				if (Expr.Kind == EBoundExprKind::FunctionCall && Run.Bind.Bound->Functions.IsValidIndex(Expr.Index))
				{
					const int32 ParamCount = Run.Bind.Bound->Functions[Expr.Index].Params.Num();
					Receivers += Expr.Args.ContainsByPredicate([ParamCount](const FBoundArgument& Argument) { return Argument.TargetIndex == ParamCount; }) ? 1 : 0;
				}
			}
			for (const FBoundFunction& Function : Run.Bind.Bound->Functions)
			{
				for (const FBoundLocal& Local : Function.Locals)
				{
					ImplicitLocals += (Local.Decl == nullptr) ? 1 : 0;
				}
			}
			TestEqual(TEXT("one output selection"), Selections, 1);
			TestEqual(TEXT("one statement call with a return-value receiver (TargetIndex == Params.Num())"), Receivers, 1);
			TestEqual(TEXT("one local the call declared (`Unused`)"), ImplicitLocals, 1);
			TestTrue(TEXT("and the binder said so: DSH5283"), HasLine(Run.Infos, TEXT("DSH5283")));
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// The instance schema
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLegacyRulesInstanceSchemaTest,
	"DreamShader.Lang2.LegacyRules.InstanceSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLegacyRulesInstanceSchemaTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LegacyRulesTests;

	FIRRun Parent;
	Lower(Parent, TEXT("M_SchemaParent.dss"), TEXT(
		"uniform float A = 0.5;\n"
		"uniform float3 B = float3(1, 2, 3);\n"
		"/// @static\n"
		"uniform bool C = true;\n"
		"uniform Texture2D D;\n"
		"uniform float E = 9;\n"
		"export void M_SchemaParent(inout material m)\n"
		"{\n"
		"    float3 Colour = B * A;\n"
		"    if (C)\n"
		"    {\n"
		"        Colour = D.Sample(UE.TexCoord(CoordinateIndex = 0)).rgb;\n"
		"    }\n"
		"    m.EmissiveColor = Colour;\n"
		"}\n"));
	if (!TestTrue(FString::Printf(TEXT("the parent builds (%s)"), *Parent.ErrorText()), Parent.Succeeded() && Parent.Module.IsValid()))
	{
		return false;
	}

	FIRParameterSchema Schema;
	TestTrue(TEXT("a material product has a schema"), BuildParameterSchemaFromIR(*Parent.Module, 0, Parent.Bind.Bound.Get(), Schema));
	TestTrue(TEXT("the schema is valid"), Schema.bValid);

	const auto Entry = [&Schema](const TCHAR* Name) -> const FIRParameterSchemaEntry*
	{
		const int32 Index = Schema.Find(Name);
		return Schema.Parameters.IsValidIndex(Index) ? &Schema.Parameters[Index] : nullptr;
	};

	if (const FIRParameterSchemaEntry* A = Entry(TEXT("A")); TestNotNull(TEXT("A"), A))
	{
		TestTrue(TEXT("A is a scalar"), A->Kind == EIRParameterKind::Scalar);
		TestTrue(TEXT("A carries its declared type"), A->DeclaredType == FIRType::Float(1));
		TestFalse(TEXT("A is read"), A->bPruned);
	}
	if (const FIRParameterSchemaEntry* B = Entry(TEXT("B")); TestNotNull(TEXT("B"), B))
	{
		TestTrue(TEXT("B is a vector"), B->Kind == EIRParameterKind::Vector);
		TestTrue(TEXT("B's parent value has an alpha of one"), B->ParentValue.N == 4 && B->ParentValue.V[0] == 1.0 && B->ParentValue.V[2] == 3.0 && B->ParentValue.V[3] == 1.0);
	}
	if (const FIRParameterSchemaEntry* C = Entry(TEXT("C")); TestNotNull(TEXT("C"), C))
	{
		TestTrue(TEXT("C is a static switch"), C->Kind == EIRParameterKind::StaticSwitch);
	}
	if (const FIRParameterSchemaEntry* D = Entry(TEXT("D")); TestNotNull(TEXT("D"), D))
	{
		TestTrue(TEXT("D is a texture"), D->Kind == EIRParameterKind::Texture);
	}
	if (const FIRParameterSchemaEntry* E = Entry(TEXT("E")); TestNotNull(TEXT("E"), E))
	{
		TestTrue(TEXT("E is declared and read by nothing: pruned"), E->bPruned);
	}

	// Lookups: case-sensitive, with a case-only match found on request.
	TestEqual(TEXT("Find is case-sensitive"), Schema.Find(TEXT("a")), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("FindIgnoreCase finds it"), Schema.FindIgnoreCase(TEXT("a")) != INDEX_NONE);

	// Without the bound module: kinds only, and what the source declared but the graph dropped is not known.
	{
		FIRParameterSchema KindsOnly;
		TestTrue(TEXT("kinds only"), BuildParameterSchemaFromIR(*Parent.Module, 0, nullptr, KindsOnly));
		const int32 Index = KindsOnly.Find(TEXT("A"));
		TestTrue(TEXT("no declared type without the bound module"), KindsOnly.Parameters.IsValidIndex(Index) && KindsOnly.Parameters[Index].DeclaredType.IsError());
	}

	// The fingerprint does not depend on the order of the entries.
	{
		FIRParameterSchema Shuffled = Schema;
		Algo::Reverse(Shuffled.Parameters);
		TestTrue(TEXT("MakeFingerprint is order-independent"), Shuffled.MakeFingerprint().Equals(Schema.MakeFingerprint(), ESearchCase::CaseSensitive));
	}

	// A function product has no parameters an instance could override.
	{
		FIRRun Library;
		Lower(Library, TEXT("MF_NoSchema.dss"), TEXT("export float MF_NoSchema(float A)\n{\n    return A;\n}\n"));
		if (Library.Module.IsValid())
		{
			FIRParameterSchema None;
			TestFalse(TEXT("a function product has no schema"), BuildParameterSchemaFromIR(*Library.Module, 0, Library.Bind.Bound.Get(), None));
		}
	}

	// The eleven kinds read back.
	for (int32 KindIndex = 0; KindIndex <= static_cast<int32>(EIRParameterKind::StaticComponentMask); ++KindIndex)
	{
		const EIRParameterKind Kind = static_cast<EIRParameterKind>(KindIndex);
		EIRParameterKind ReadBack = EIRParameterKind::Scalar;
		TestTrue(FString::Printf(TEXT("'%s' reads back"), LexToString(Kind)), TryParseParameterKind(LexToString(Kind), ReadBack) && ReadBack == Kind);
	}
	EIRParameterKind Untouched = EIRParameterKind::Font;
	TestFalse(TEXT("kinds are case-sensitive"), TryParseParameterKind(TEXT("scalar"), Untouched));
	return true;
}

// ---------------------------------------------------------------------------------------------
// Where a 1.x source's problems are reported (dreamshader-language-support issues #2 and #3)
// ---------------------------------------------------------------------------------------------

namespace UE::DreamShader::Editor::Private::LegacyRulesTests
{
	/** The 1-based line and column of the first character of Marker in Text; (0, 0) when it is not there. */
	inline FIntPoint LineAndColumnOf(const FString& Text, const TCHAR* Marker)
	{
		const int32 Offset = Text.Find(Marker, ESearchCase::CaseSensitive);
		if (Offset == INDEX_NONE)
		{
			return FIntPoint(0, 0);
		}
		int32 Line = 1;
		int32 LineStart = 0;
		for (int32 Index = 0; Index < Offset; ++Index)
		{
			if (Text[Index] == TCHAR('\n'))
			{
				++Line;
				LineStart = Index + 1;
			}
		}
		return FIntPoint(Offset - LineStart + 1, Line);
	}

	inline const FLangDiagnostic* FindDiagnostic(const FLangDiagnosticSink& Sink, const TCHAR* Code)
	{
		return Sink.GetDiagnostics().FindByPredicate([Code](const FLangDiagnostic& Diagnostic) { return Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive); });
	}

	inline int32 CountDiagnostics(const FLangDiagnosticSink& Sink, const TCHAR* Code)
	{
		int32 Count = 0;
		for (const FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
		{
			Count += Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive) ? 1 : 0;
		}
		return Count;
	}

	inline const FIRNode* FindNamedNode(const FIRGraph& Graph, const EIROp Op, const TCHAR* DebugName)
	{
		return Graph.Nodes.FindByPredicate([Op, DebugName](const FIRNode& Node) { return Node.Op == Op && Node.DebugName.Equals(DebugName, ESearchCase::CaseSensitive); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLegacyRulesWhereProblemsLandTest,
	"DreamShader.Lang2.LegacyRules.WhereProblemsLand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLegacyRulesWhereProblemsLandTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LegacyRulesTests;

	// #2: an unquoted asset path as a Graph argument. The 1.x generator said "In Graph statement '...'" at 1:1; the error
	// belongs at the '/' that starts the path.
	{
		const FString Text = TEXT(
			"Shader(Name=\"M_LpIssue2\")\n"
			"{\n"
			"    Outputs = { vec3 Color; Base.EmissiveColor = Color; }\n"
			"\n"
			"    Graph = {\n"
			"        float2 WP_UV = UE.WorldPosition().xy;\n"
			"        vec3 CloudTex = UE.TextureSampleParameter2D(OutputType=\"float3\", ParameterName=\"CloudTexture\", Coordinates=WP_UV, TextureObject=/Engine/Textures/Mask);\n"
			"        Color = CloudTex;\n"
			"    }\n"
			"}\n");
		FIRRun Run;
		Lower(Run, TEXT("M_LpIssue2.dsm"), *Text);
		const FLangDiagnostic* Error = FindDiagnostic(Run.Parse.Diagnostics, TEXT("DSH2151"));
		const FIntPoint Expected = LineAndColumnOf(Text, TEXT("/Engine/Textures"));
		if (TestNotNull(FString::Printf(TEXT("the unquoted path is DSH2151 (errors: %s)"), *Run.ErrorText()), Error))
		{
			TestEqual(TEXT("at the path's line"), Error->Span.Line, Expected.Y);
			TestEqual(TEXT("at the path's '/'"), Error->Span.Column, Expected.X);
		}
	}

	// #3: Slider(...) on a vec3 property. The 1.x generator failed the whole compile at the Graph line that used the
	// parameter, naming a 'slidermin' nobody wrote; a vector parameter has no slider. One warning at the Slider entry, the
	// range dropped -- and a scalar property's Slider still applies.
	{
		const FString Text = TEXT(
			"Shader(Name=\"M_LpIssue3\")\n"
			"{\n"
			"    Properties = {\n"
			"        vec3 Tint = vec3(1.0, 1.0, 1.0) [\n"
			"            Group = \"Look\"; SortPriority = 10; Slider(0.01, 8);\n"
			"            ParameterName = \"Base Tint\";\n"
			"        ];\n"
			"        float Gain = 0.5 [Slider(0, 4)];\n"
			"    }\n"
			"    Outputs = { vec3 Color; Base.EmissiveColor = Color; }\n"
			"    Graph = {\n"
			"        Color = Tint * Gain;\n"
			"    }\n"
			"}\n");
		FIRRun Run;
		Lower(Run, TEXT("M_LpIssue3.dsm"), *Text);
		TestTrue(FString::Printf(TEXT("the material builds (%s)"), *Run.ErrorText()), Run.Succeeded());
		TestEqual(FString::Printf(TEXT("one DSH7233, for the vector only (warnings: %s)"), *Run.WarningText()), CountDiagnostics(Run.Bind.Diagnostics, TEXT("DSH7233")), 1);
		const FLangDiagnostic* Warning = FindDiagnostic(Run.Bind.Diagnostics, TEXT("DSH7233"));
		const FIntPoint Expected = LineAndColumnOf(Text, TEXT("Slider(0.01"));
		if (Warning)
		{
			TestTrue(TEXT("DSH7233 is a warning"), Warning->Severity == ELangSeverity::Warning);
			TestEqual(TEXT("at the Slider entry's line"), Warning->Span.Line, Expected.Y);
			TestEqual(TEXT("at the Slider entry"), Warning->Span.Column, Expected.X);
		}
		if (Run.Module.IsValid() && Run.Module->Products.Num() == 1)
		{
			const FIRGraph& Graph = Run.Module->Products[0].Graph;
			const FIRNode* Tint = FindNamedNode(Graph, EIROp::Parameter, TEXT("Tint"));
			const FIRNode* Gain = FindNamedNode(Graph, EIROp::Parameter, TEXT("Gain"));
			if (TestNotNull(TEXT("the vector parameter"), Tint))
			{
				TestNull(TEXT("carries no slider range"), Tint->FindProperty(Prop::SliderMin));
			}
			if (TestNotNull(TEXT("the scalar parameter"), Gain))
			{
				TestNotNull(TEXT("keeps its slider range"), Gain->FindProperty(Prop::SliderMax));
			}
		}
	}

	// The same rule in a .dss: '@slider' on a float3 or a bool uniform warns at the directive; on a float it does not.
	{
		FIRRun Run;
		Lower(Run, TEXT("M_Sliders.dss"), TEXT(
			"/// @slider 0 1\n"
			"uniform float3 Tint = float3(1, 1, 1);\n"
			"/// @static @slider 0 1\n"
			"uniform bool Flip = false;\n"
			"/// @slider 0 4\n"
			"uniform float Gain = 0.5;\n"
			"export void M_Sliders(inout material m)\n"
			"{\n"
			"    m.EmissiveColor = Tint * Gain;\n"
			"}\n"));
		TestTrue(FString::Printf(TEXT("the .dss builds (%s)"), *Run.ErrorText()), Run.Succeeded());
		TestEqual(FString::Printf(TEXT("two DSH7233, the float3's and the bool's (warnings: %s)"), *Run.WarningText()), CountDiagnostics(Run.Bind.Diagnostics, TEXT("DSH7233")), 2);
		const FLangDiagnostic* First = FindDiagnostic(Run.Bind.Diagnostics, TEXT("DSH7233"));
		if (First)
		{
			TestEqual(TEXT("the float3's on its directive line"), First->Span.Line, 1);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
