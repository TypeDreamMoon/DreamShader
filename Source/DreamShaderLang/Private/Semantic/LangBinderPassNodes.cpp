// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The Custom Pass nodes in a `.dss` (DreamShader_Plan/06 s3-s6): `UE.DreamPassOutput`, the custom output a mesh pass
// draws its targets from, and `UE.DreamPassBuffer`, an exported pipeline buffer sampled by an ordinary material.
//
// Both are reflected classes of the DreamShaderPass module like any other `UE.` node: the catalog brings them and the
// expression binder binds them. What is said here is what the catalog cannot say.
//
//   * The classes reach the catalog on UE 5.8 only (the module is built everywhere; below 5.8 the reflected catalog
//     leaves them out). A call on an engine without them names a node the catalog lacks, and "not a node" would not say
//     why. The table of `UE` nodes that exist from some engine (or module) on lives here, the way Substrate's lives
//     beside its sugar; the raise is where the unknown node is found (LangBinderExpressions.cpp, DSH5300). The gate
//     keys on the class being absent from the catalog -- this module cannot know the engine version.
//   * The engine compiles custom outputs from a material's own graph only, and UE.DreamPassOutput gives the material
//     its DreamPass shader tag only there; in a material function, a layer or a blend it does nothing at all (DSH5302,
//     a warning).
//   * A material compiles one custom output of a class, so a second UE.DreamPassOutput node -- written twice, in a
//     helper inlined twice, or in a loop that unrolls -- fails in the engine (DSH5303).
//   * Neither node implements the new material translator (MaterialIR), so a material that asks for it with
//     `#pragma material(bEnableNewHLSLGenerator = true)` and uses either cannot compile (DSH5301).
//
// The counts run over what the binder records anyway: every call of the two classes (NotePassNodeUse, from
// BindReflectedCall) and every call of a user function (NotePassNodeCallSite, from BindUserFunctionCall), each with
// whether a loop encloses it. A helper is inlined where it is called, so its nodes are its callers'; an exported function
// is a function call node, so its nodes stay in its own asset.
//
// Diagnostics owned by this file: DSH5301, DSH5302, DSH5303; and the requirement text of DSH5300, which
// LangBinderExpressions.cpp raises.

#include "LangBinderInternal.h"

#include "IR/IRCatalog.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Semantic/LangBound.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "DreamShader.Binder.PassNodes"

namespace UE::DreamShader::Lang::Private
{
	// A named namespace, not an anonymous one: the module builds as one unity blob.
	namespace LangBinderPassNodesPrivate
	{
		/** A `UE` node that exists only from some engine version, or with some module, on. */
		struct FUENodeGate
		{
			const TCHAR* Name;
			const TCHAR* Engine;
			/** A class of DreamShader's own Custom Pass module (DreamShaderPass), not of the engine. */
			bool bCustomPassModule;
		};

		static const FUENodeGate UENodeGates[] =
		{
			{ TEXT("DreamPassOutput"), TEXT("5.8"), true },
			{ TEXT("DreamPassBuffer"), TEXT("5.8"), true },
			// The engine's own node for a named post-process input. Not checked against an older engine's source (only
			// 5.8 was at hand when this was written); the version is the one the 5.6 release brought User Scene Textures in.
			{ TEXT("UserSceneTexture"), TEXT("5.6"), false },
		};

		static bool IsDreamPassOutputClass(const IR::FCatalogExpression& Class)
		{
			return Class.ClassName.Equals(TEXT("MaterialExpressionDreamPassOutput"), ESearchCase::CaseSensitive)
				|| (Class.Namespace.Equals(TEXT("UE"), ESearchCase::CaseSensitive) && Class.ShortName.Equals(TEXT("DreamPassOutput"), ESearchCase::CaseSensitive));
		}

		static bool IsDreamPassBufferClass(const IR::FCatalogExpression& Class)
		{
			return Class.ClassName.Equals(TEXT("MaterialExpressionDreamPassBuffer"), ESearchCase::CaseSensitive)
				|| (Class.Namespace.Equals(TEXT("UE"), ESearchCase::CaseSensitive) && Class.ShortName.Equals(TEXT("DreamPassBuffer"), ESearchCase::CaseSensitive));
		}

		/** How many nodes of one class a function's graph gets, up to two -- all that the rules ask -- and where the first two come from. */
		struct FPassNodeCount
		{
			int32 Count = 0;
			FString FirstFile;
			FLangSpan FirstSpan;
			FString SecondFile;
			FLangSpan SecondSpan;

			void Add(const int32 Occurrences, const FString& File, const FLangSpan& Span)
			{
				for (int32 Index = 0; Index < Occurrences && Count < 2; ++Index)
				{
					if (Count == 0)
					{
						FirstFile = File;
						FirstSpan = Span;
					}
					else
					{
						SecondFile = File;
						SecondSpan = Span;
					}
					++Count;
				}
			}
		};

		/**
		 * The nodes of one class (bOutput: UE.DreamPassOutput, else UE.DreamPassBuffer) in the graph a function's body
		 * becomes: its own calls, and those of every helper it inlines, as often as it inlines it. A call under a loop
		 * counts twice, because it unrolls into more than one. A cycle counts nothing: inlining it is refused elsewhere
		 * (DSH6220). An inlined node is placed at the call that brought it in.
		 */
		static FPassNodeCount CountPassNodes(
			const int32 FunctionIndex,
			const bool bOutput,
			const TArray<FBoundFunction>& Functions,
			const TArray<FBinderPassNodeUse>& Uses,
			const TArray<FBinderPassNodeCallSite>& CallSites,
			TArray<int32>& Stack)
		{
			FPassNodeCount Result;
			if (Stack.Contains(FunctionIndex))
			{
				return Result;
			}
			Stack.Add(FunctionIndex);

			for (const FBinderPassNodeUse& Use : Uses)
			{
				if (Use.FunctionIndex == FunctionIndex && Use.bOutput == bOutput)
				{
					Result.Add(Use.bInLoop ? 2 : 1, Use.File, Use.Span);
				}
			}
			for (const FBinderPassNodeCallSite& Site : CallSites)
			{
				if (Site.Caller != FunctionIndex || !Functions.IsValidIndex(Site.Callee) || Functions[Site.Callee].Kind != EBoundFunctionKind::Helper)
				{
					continue;
				}
				const FPassNodeCount Inner = CountPassNodes(Site.Callee, bOutput, Functions, Uses, CallSites, Stack);
				if (Inner.Count > 0)
				{
					Result.Add(Inner.Count * (Site.bInLoop ? 2 : 1), Site.File, Site.Span);
				}
			}

			Stack.Pop();
			return Result;
		}

		/** `true`, `True`, `1`, quoted or not: what a `#pragma material` bool says when it says yes. */
		static bool IsTrueSetting(const FString& Value)
		{
			const FString Trimmed = Value.TrimStartAndEnd().TrimQuotes().TrimStartAndEnd();
			return Trimmed.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("1"), ESearchCase::CaseSensitive);
		}
	}

	bool FLangBinder::TryDescribeUENodeVersionGate(const FString& NodeName, FText& OutRequirement) const
	{
		for (const LangBinderPassNodesPrivate::FUENodeGate& Gate : LangBinderPassNodesPrivate::UENodeGates)
		{
			if (NodeName.Equals(Gate.Name, ESearchCase::CaseSensitive))
			{
				OutRequirement = Gate.bCustomPassModule
					? FText::Format(LOCTEXT("GateCustomPass", "Unreal Engine {0} or later and DreamShader's Custom Pass module (DreamShaderPass)"), FText::FromString(Gate.Engine))
					: FText::Format(LOCTEXT("GateEngine", "Unreal Engine {0} or later"), FText::FromString(Gate.Engine));
				return true;
			}
		}
		return false;
	}

	void FLangBinder::NotePassNodeUse(const IR::FCatalogExpression& Class, const FLangSpan& Span)
	{
		const bool bOutput = LangBinderPassNodesPrivate::IsDreamPassOutputClass(Class);
		if (!bOutput && !LangBinderPassNodesPrivate::IsDreamPassBufferClass(Class))
		{
			return;
		}

		FBinderPassNodeUse& Use = PassNodeUses.AddDefaulted_GetRef();
		Use.bOutput = bOutput;
		Use.FunctionIndex = CurrentFunctionIndex;
		Use.File = CurrentFile;
		Use.Span = Span;
		Use.bInLoop = LoopDepth > 0;
	}

	void FLangBinder::NotePassNodeCallSite(const int32 CalleeIndex, const FLangSpan& Span)
	{
		if (CurrentFunctionIndex == INDEX_NONE)
		{
			return;
		}
		FBinderPassNodeCallSite& Site = PassNodeCallSites.AddDefaulted_GetRef();
		Site.Caller = CurrentFunctionIndex;
		Site.Callee = CalleeIndex;
		Site.File = CurrentFile;
		Site.Span = Span;
		Site.bInLoop = LoopDepth > 0;
	}

	void FLangBinder::CheckPassNodeUses()
	{
		using namespace LangBinderPassNodesPrivate;

		if (PassNodeUses.Num() == 0)
		{
			return;
		}

		const FString OuterFile = CurrentFile;
		TArray<int32> Stack;
		for (int32 FunctionIndex = 0; FunctionIndex < Bound.Functions.Num(); ++FunctionIndex)
		{
			const FBoundFunction& Function = Bound.Functions[FunctionIndex];
			// A product comes from the root file (DSH3210 keeps a header from exporting).
			if (!Function.File.Equals(RootModule.FilePath, ESearchCase::IgnoreCase))
			{
				continue;
			}

			switch (Function.Kind)
			{
			case EBoundFunctionKind::Entry:
			{
				const FPassNodeCount Outputs = CountPassNodes(FunctionIndex, /* bOutput */ true, Bound.Functions, PassNodeUses, PassNodeCallSites, Stack);
				const FPassNodeCount Buffers = CountPassNodes(FunctionIndex, /* bOutput */ false, Bound.Functions, PassNodeUses, PassNodeCallSites, Stack);

				if (Outputs.Count >= 2)
				{
					Diagnostics.Error(
						TEXT("DSH5303"),
						Outputs.SecondFile,
						Outputs.SecondSpan,
						FText::Format(
							LOCTEXT("TwoPassOutputs", "Material '{0}' gets a second UE.DreamPassOutput node here (the first is on line {1}), and a material compiles one custom output of a class; write all four outputs in one call. A call inside a loop, or in a helper called twice, makes more than one node."),
							FText::FromString(Function.Name),
							FText::AsNumber(Outputs.FirstSpan.Line)));
				}

				const FString* NewTranslator = Bound.MaterialSettings.Find(FString(TEXT("bEnableNewHLSLGenerator")));
				if (NewTranslator && IsTrueSetting(*NewTranslator) && (Outputs.Count > 0 || Buffers.Count > 0))
				{
					const FLangSpan* SettingSpan = MaterialSettingSpans.Find(FString(TEXT("bEnableNewHLSLGenerator")));
					const FString* SettingFile = MaterialSettingFiles.Find(FString(TEXT("bEnableNewHLSLGenerator")));
					const FPassNodeCount& Used = Outputs.Count > 0 ? Outputs : Buffers;
					Diagnostics.Error(
						TEXT("DSH5301"),
						SettingFile ? *SettingFile : Used.FirstFile,
						SettingSpan ? *SettingSpan : Used.FirstSpan,
						FText::Format(
							LOCTEXT("PassNodeNewTranslator", "Material '{0}' asks for the new material translator ('bEnableNewHLSLGenerator = true'), and uses {1} (line {2}), which only the classic translator compiles; remove the setting."),
							FText::FromString(Function.Name),
							Outputs.Count > 0 ? FText::FromString(TEXT("UE.DreamPassOutput")) : FText::FromString(TEXT("UE.DreamPassBuffer")),
							FText::AsNumber(Used.FirstSpan.Line)));
				}
				break;
			}

			case EBoundFunctionKind::ExportFunction:
			case EBoundFunctionKind::Layer:
			case EBoundFunctionKind::LayerBlend:
			{
				const FPassNodeCount Outputs = CountPassNodes(FunctionIndex, /* bOutput */ true, Bound.Functions, PassNodeUses, PassNodeCallSites, Stack);
				if (Outputs.Count > 0)
				{
					Diagnostics.Warning(
						TEXT("DSH5302"),
						Outputs.FirstFile,
						Outputs.FirstSpan,
						FText::Format(
							LOCTEXT("PassOutputInFunction", "UE.DreamPassOutput does nothing in '{0}', a {1}: the engine compiles custom outputs from a material's own graph only, and only there does the node give the material its DreamPass shader tag. A material using '{0}' writes no pass outputs, and a mesh pass in 'Mode = Own' does not draw it; write UE.DreamPassOutput in the material itself."),
							FText::FromString(Function.Name),
							Function.Kind == EBoundFunctionKind::ExportFunction
								? LOCTEXT("KindFunction", "material function")
								: (Function.Kind == EBoundFunctionKind::Layer ? LOCTEXT("KindLayer", "material layer") : LOCTEXT("KindLayerBlend", "material layer blend"))));
				}
				break;
			}

			default:
				break;
			}
		}
		CurrentFile = OuterFile;
	}
}

#undef LOCTEXT_NAMESPACE
