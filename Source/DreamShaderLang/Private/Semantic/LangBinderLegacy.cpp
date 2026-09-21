// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The binder's legacy rules.
//
// A declaration the legacy front end produced carries FDecl::bLegacy, and inside it -- its body, its initializer,
// the calls lifted out of its opaque body -- the binder reads the source the way 1.x did wherever the two languages
// disagree and a 1.x source relies on it:
//
//   L2   `mix` / `fract` / `mod` as call names, with a warning                (LangBinderExpressions.cpp, BindCall)
//   L3b  `F(args).Out`, `F(args)[k]` and a bare value call pick an output     (BindFunctionCallOutput, BindUserFunctionCall)
//   L5   an undeclared receiver becomes a local; trailing receivers           (DeclareLegacyImplicitOutLocal, BindUserFunctionCall)
//   L12  enumerators match leniently                                           (TryMatchLegacyEnumerator)
//   L13  a required pin left unconnected is a warning                          (BindReflectedCall)
//   L19  a unique case-insensitive match when the exact lookup misses          (the *IgnoringCase* helpers)
//
// The general rules that came with them apply to every declaration: L3a (`UE.X(...)[k]`, BindIndex), L4 (a Custom
// class's dynamic inputs, BindReflectedCall), L7 (FParam::bOptional, DeclareFunction) and L8 (the `UE.*` calls a
// GraphFunction lifts into node inputs, bound here once in the function's own scope and judged at each call site).
//
// Diagnostics owned by this file: DSH5275, DSH5276, DSH5280, DSH5282, DSH5283, DSH6325, DSH6326, DSH6329. The other
// legacy codes are raised where their rule applies: DSH5277, DSH5278, DSH5279, DSH5281, DSH5282, DSH5284 and DSH5285
// in LangBinderExpressions.cpp, DSH6330 in LangBinder.cpp.

#include "LangBinderInternal.h"

#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "IR/IRCoreOps.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Semantic/LangBound.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/Char.h"

#define LOCTEXT_NAMESPACE "DreamShader.Binder.Legacy"

namespace UE::DreamShader::Lang::Private
{
	// A named namespace, not an anonymous one: the module builds as one unity blob.
	namespace LangBinderLegacyPrivate
	{
		/** 1.x's enum lookup key (NormalizeEnumLookupKey): lower case, without spaces, tabs, `_`, `-`, `:`, `.` and `/`. */
		FString NormaliseLegacyEnumKey(const FString& Text)
		{
			FString Key;
			Key.Reserve(Text.Len());
			for (const TCHAR Char : Text)
			{
				if (Char == TCHAR(' ') || Char == TCHAR('\t') || Char == TCHAR('_') || Char == TCHAR('-')
					|| Char == TCHAR(':') || Char == TCHAR('.') || Char == TCHAR('/'))
				{
					continue;
				}
				Key.AppendChar(FChar::ToLower(Char));
			}
			return Key;
		}

		/** `'Result', 'Mask'`: output names for a message. */
		FString QuoteLegacyOutputNames(const TArray<FString>& Names)
		{
			FString Out;
			for (const FString& Name : Names)
			{
				Out += Out.IsEmpty() ? TEXT("") : TEXT(", ");
				Out += TEXT("'") + Name + TEXT("'");
			}
			return Out;
		}
	}

	// ---------------------------------------------------------------------------------------------
	// L19: names that match only ignoring case
	// ---------------------------------------------------------------------------------------------

	void FLangBinder::ReportLegacyCaseFallback(const FString& Written, const FString& Declared, const FLangSpan& Span)
	{
		Diagnostics.Warning(
			TEXT("DSH5275"),
			CurrentFile,
			Span,
			FText::Format(
				LOCTEXT("LegacyCaseFallback", "'{0}' matches '{1}' only in case; a 1.x source is read ignoring case, so this is '{1}', and a '.dss' needs the exact spelling."),
				FText::FromString(Written),
				FText::FromString(Declared)));
	}

	void FLangBinder::ReportLegacyCatalogCaseFallback(const FString& Written, const FString& Catalogued, const FLangSpan& Span)
	{
		Diagnostics.Warning(
			TEXT("DSH5276"),
			CurrentFile,
			Span,
			FText::Format(
				LOCTEXT("LegacyCatalogCaseFallback", "'{0}' matches the engine name '{1}' only in case; 1.x matched engine names ignoring case, so this is '{1}', and a '.dss' needs the exact spelling."),
				FText::FromString(Written),
				FText::FromString(Catalogued)));
	}

	bool FLangBinder::TryBindLegacyIdentifierIgnoringCase(const FIdentifierExpr& Expr, FBoundExpr& OutBinding)
	{
		enum class ELegacyCandidateKind : uint8
		{
			Local,
			Param,
			Global,
		};
		struct FLegacyCandidate
		{
			ELegacyCandidateKind Kind;
			int32 Index;
			FString Name;
		};

		TArray<FLegacyCandidate> Candidates;
		for (int32 ScopeIndex = Scopes.Num() - 1; ScopeIndex >= 0; --ScopeIndex)
		{
			for (const TPair<FString, int32>& Name : Scopes[ScopeIndex].Names)
			{
				const bool bSeen = Candidates.ContainsByPredicate([&Name](const FLegacyCandidate& Candidate)
				{
					return Candidate.Kind == ELegacyCandidateKind::Local && Candidate.Index == Name.Value;
				});
				if (!bSeen && Name.Key.Equals(Expr.Name, ESearchCase::IgnoreCase))
				{
					Candidates.Add({ ELegacyCandidateKind::Local, Name.Value, Name.Key });
				}
			}
		}
		if (CurrentFunction)
		{
			for (int32 Index = 0; Index < CurrentFunction->Params.Num(); ++Index)
			{
				if (CurrentFunction->Params[Index].Name.Equals(Expr.Name, ESearchCase::IgnoreCase))
				{
					Candidates.Add({ ELegacyCandidateKind::Param, Index, CurrentFunction->Params[Index].Name });
				}
			}
		}
		for (int32 Index = 0; Index < Bound.Globals.Num(); ++Index)
		{
			if (Bound.Globals[Index].Name.Equals(Expr.Name, ESearchCase::IgnoreCase))
			{
				Candidates.Add({ ELegacyCandidateKind::Global, Index, Bound.Globals[Index].Name });
			}
		}

		if (Candidates.Num() != 1)
		{
			return false;
		}

		const FLegacyCandidate& Found = Candidates[0];
		switch (Found.Kind)
		{
		case ELegacyCandidateKind::Local:
			if (!CurrentFunction || !CurrentFunction->Locals.IsValidIndex(Found.Index))
			{
				return false;
			}
			OutBinding.Kind = EBoundExprKind::Local;
			OutBinding.LocalSlot = Found.Index;
			OutBinding.Type = CurrentFunction->Locals[Found.Index].Type;
			OutBinding.bLValue = true;
			break;

		case ELegacyCandidateKind::Param:
		{
			const FBoundParam& Param = CurrentFunction->Params[Found.Index];
			OutBinding.Kind = EBoundExprKind::Param;
			OutBinding.Index = Found.Index;
			OutBinding.Type = Param.Type;
			OutBinding.bLValue = Param.Direction != EParamDirection::In;
			break;
		}

		case ELegacyCandidateKind::Global:
		{
			const FBoundGlobal& Global = Bound.Globals[Found.Index];
			OutBinding.Kind = EBoundExprKind::Global;
			OutBinding.Index = Found.Index;
			OutBinding.Type = Global.Type;
			OutBinding.bLValue = false;
			// The same fold BindIdentifier gives a `static const` read.
			if (Global.bIsConstant && Global.Decl && Global.Decl->Declarator.Initializer)
			{
				if (const FBoundExpr* Init = Lookup(*Global.Decl->Declarator.Initializer))
				{
					if (Init->bIsConstant && GetGlobalArrayCount(Found.Index) == 0)
					{
						OutBinding.bIsConstant = true;
						for (int32 Component = 0; Component < 4; ++Component)
						{
							OutBinding.ConstantValue[Component] = Init->ConstantValue[Component];
						}
					}
				}
			}
			break;
		}
		}

		ReportLegacyCaseFallback(Expr.Name, Found.Name, Expr.Span);
		return true;
	}

	bool FLangBinder::HasDeclarationIgnoringCase(const FString& Name) const
	{
		for (const FBinderScope& Scope : Scopes)
		{
			for (const TPair<FString, int32>& Entry : Scope.Names)
			{
				if (Entry.Key.Equals(Name, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
		}
		if (CurrentFunction)
		{
			for (const FBoundParam& Param : CurrentFunction->Params)
			{
				if (Param.Name.Equals(Name, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
		}
		for (const FBoundGlobal& Global : Bound.Globals)
		{
			if (Global.Name.Equals(Name, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	int32 FLangBinder::FindFunctionIgnoringCaseUniquely(const FString& Name) const
	{
		int32 Found = INDEX_NONE;
		for (int32 Index = 0; Index < Bound.Functions.Num(); ++Index)
		{
			if (Bound.Functions[Index].Name.Equals(Name, ESearchCase::IgnoreCase))
			{
				if (Found != INDEX_NONE)
				{
					return INDEX_NONE;
				}
				Found = Index;
			}
		}
		return Found;
	}

	int32 FLangBinder::FindExpressionByClassIgnoringCaseUniquely(const FString& ClassSpecifier) const
	{
		const FString Specifier = ClassSpecifier.TrimStartAndEnd();
		if (Specifier.IsEmpty())
		{
			return INDEX_NONE;
		}

		// The spellings FindExpressionByClass accepts, compared ignoring case.
		int32 Found = INDEX_NONE;
		for (int32 Index = 0; Index < Catalog.Expressions.Num(); ++Index)
		{
			const IR::FCatalogExpression& Expression = Catalog.Expressions[Index];
			const bool bMatches = Expression.ShortName.Equals(Specifier, ESearchCase::IgnoreCase)
				|| Expression.ClassName.Equals(Specifier, ESearchCase::IgnoreCase)
				|| Expression.ClassPathName.Equals(Specifier, ESearchCase::IgnoreCase);
			if (bMatches)
			{
				if (Found != INDEX_NONE && Found != Index)
				{
					return INDEX_NONE;
				}
				Found = Index;
			}
		}
		return Found;
	}

	int32 FLangBinder::FindExpressionIgnoringCaseUniquely(const FString& Namespace, const FString& Name) const
	{
		// FBuiltinCatalog::FindExpressionIgnoreCase answers the first match; the fallback wants to know there is one.
		int32 Found = INDEX_NONE;
		for (int32 Index = 0; Index < Catalog.Expressions.Num(); ++Index)
		{
			const IR::FCatalogExpression& Expression = Catalog.Expressions[Index];
			if (!Expression.Namespace.Equals(Namespace, ESearchCase::CaseSensitive))
			{
				continue;
			}
			bool bMatches = Expression.ShortName.Equals(Name, ESearchCase::IgnoreCase);
			for (const FString& Alias : Expression.Aliases)
			{
				bMatches = bMatches || Alias.Equals(Name, ESearchCase::IgnoreCase);
			}
			if (bMatches)
			{
				if (Found != INDEX_NONE)
				{
					return INDEX_NONE;
				}
				Found = Index;
			}
		}
		return Found;
	}

	int32 FLangBinder::FindCatalogInputIgnoringCaseUniquely(const IR::FCatalogExpression& Class, const FString& Name)
	{
		int32 Found = INDEX_NONE;
		for (int32 Index = 0; Index < Class.Inputs.Num(); ++Index)
		{
			bool bMatches = Class.Inputs[Index].Name.Equals(Name, ESearchCase::IgnoreCase);
			for (const FString& Alias : Class.Inputs[Index].Aliases)
			{
				bMatches = bMatches || Alias.Equals(Name, ESearchCase::IgnoreCase);
			}
			if (bMatches)
			{
				if (Found != INDEX_NONE)
				{
					return INDEX_NONE;
				}
				Found = Index;
			}
		}
		return Found;
	}

	int32 FLangBinder::FindCatalogPropertyIgnoringCaseUniquely(const IR::FCatalogExpression& Class, const FString& Name)
	{
		int32 Found = INDEX_NONE;
		for (int32 Index = 0; Index < Class.Properties.Num(); ++Index)
		{
			bool bMatches = Class.Properties[Index].Name.Equals(Name, ESearchCase::IgnoreCase);
			for (const FString& Alias : Class.Properties[Index].Aliases)
			{
				bMatches = bMatches || Alias.Equals(Name, ESearchCase::IgnoreCase);
			}
			if (bMatches)
			{
				if (Found != INDEX_NONE)
				{
					return INDEX_NONE;
				}
				Found = Index;
			}
		}
		return Found;
	}

	int32 FLangBinder::FindCatalogOutputIgnoringCaseUniquely(const IR::FCatalogExpression& Class, const FString& Name)
	{
		int32 Found = INDEX_NONE;
		for (int32 Index = 0; Index < Class.Outputs.Num(); ++Index)
		{
			if (!Class.Outputs[Index].Name.IsEmpty() && Class.Outputs[Index].Name.Equals(Name, ESearchCase::IgnoreCase))
			{
				if (Found != INDEX_NONE)
				{
					return INDEX_NONE;
				}
				Found = Index;
			}
		}
		return Found;
	}

	const IR::FIRCoreOpInfo* FLangBinder::FindCoreOpIgnoringCase(const FString& Name, bool& bOutIsGlslAlias)
	{
		const IR::FIRCoreOpInfo* Found = nullptr;
		for (int32 Op = 0; Op < static_cast<int32>(IR::EIROp::Count); ++Op)
		{
			const IR::FIRCoreOpInfo& Info = IR::GetCoreOpInfo(static_cast<IR::EIROp>(Op));
			const bool bHlsl = Info.HlslName != nullptr && Name.Equals(Info.HlslName, ESearchCase::IgnoreCase);
			const bool bGlsl = Info.GlslAlias != nullptr && Name.Equals(Info.GlslAlias, ESearchCase::IgnoreCase);
			if (!bHlsl && !bGlsl)
			{
				continue;
			}
			if (Found != nullptr && Found != &Info)
			{
				return nullptr;
			}
			Found = &Info;
			bOutIsGlslAlias = !bHlsl;
		}
		return Found;
	}

	// ---------------------------------------------------------------------------------------------
	// L12: enumerators
	// ---------------------------------------------------------------------------------------------

	bool FLangBinder::TryMatchLegacyEnumerator(const FString& Spelling, const IR::FCatalogProperty& Property, FString& OutEnumerator)
	{
		using namespace LangBinderLegacyPrivate;

		// What 1.x compared (TryResolveEnumLiteral, now Compiler/Reflection/DreamShaderMaterialValueParsing.cpp): one
		// normalisation over the value and over each enumerator's short, full and prefixless names. The catalog lists
		// prefixless names, so the value is tried whole, after an `EnumName::` scope, and after its own prefix.
		TArray<FString> Keys;
		FString Rest = Spelling.TrimStartAndEnd();
		Keys.Add(NormaliseLegacyEnumKey(Rest));
		const int32 Scope = Rest.Find(TEXT("::"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (Scope != INDEX_NONE)
		{
			Rest = Rest.Mid(Scope + 2);
			Keys.Add(NormaliseLegacyEnumKey(Rest));
		}
		const int32 Prefix = Rest.Find(TEXT("_"), ESearchCase::CaseSensitive);
		if (Prefix != INDEX_NONE)
		{
			Keys.Add(NormaliseLegacyEnumKey(Rest.Mid(Prefix + 1)));
		}

		int32 Found = INDEX_NONE;
		for (int32 Index = 0; Index < Property.EnumValues.Num(); ++Index)
		{
			const FString Key = NormaliseLegacyEnumKey(Property.EnumValues[Index]);
			const bool bMatches = !Key.IsEmpty() && Keys.ContainsByPredicate([&Key](const FString& Candidate)
			{
				return Candidate.Equals(Key, ESearchCase::CaseSensitive);
			});
			if (bMatches)
			{
				if (Found != INDEX_NONE)
				{
					return false;
				}
				Found = Index;
			}
		}
		if (Found == INDEX_NONE)
		{
			return false;
		}
		OutEnumerator = Property.EnumValues[Found];
		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// L3b: output selection on calls to functions
	// ---------------------------------------------------------------------------------------------

	int32 FLangBinder::FindSelectableLegacyCallee(const FCallExpr& Call) const
	{
		const FIdentifierExpr* Identifier = Call.Callee ? Call.Callee->As<FIdentifierExpr>() : nullptr;
		if (!Identifier)
		{
			return INDEX_NONE;
		}
		int32 FunctionIndex = FindFunction(Identifier->Name);
		if (FunctionIndex == INDEX_NONE)
		{
			FunctionIndex = FindFunctionIgnoringCaseUniquely(Identifier->Name);
		}
		return Bound.Functions.IsValidIndex(FunctionIndex) && IsSelectableLegacyKind(Bound.Functions[FunctionIndex].Kind)
			? FunctionIndex
			: INDEX_NONE;
	}

	bool FLangBinder::IsSelectableLegacyKind(const EBoundFunctionKind Kind)
	{
		// The kinds a 1.x source called as nodes with outputs: a Function or
		// GraphFunction block (Custom), a ShaderFunction of the same file (ExportFunction), a VirtualFunction (Extern).
		switch (Kind)
		{
		case EBoundFunctionKind::Extern:
		case EBoundFunctionKind::ExportFunction:
		case EBoundFunctionKind::Custom:
			return true;
		case EBoundFunctionKind::Helper:
		case EBoundFunctionKind::Entry:
		case EBoundFunctionKind::Layer:
		case EBoundFunctionKind::LayerBlend:
			return false;
		}
		return false;
	}

	bool FLangBinder::NamesLegacyOutput(const FBoundFunction& Function, const FString& Name)
	{
		TArray<FString> Names;
		TArray<FString> PinNames;
		TArray<IR::FIRType> Types;
		CollectLegacyOutputs(Function, Names, PinNames, Types);
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			if (Names[Index].Equals(Name, ESearchCase::IgnoreCase) || PinNames[Index].Equals(Name, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	void FLangBinder::CollectLegacyOutputs(const FBoundFunction& Function, TArray<FString>& OutNames, TArray<FString>& OutPinNames, TArray<IR::FIRType>& OutTypes)
	{
		// The order of the IR builder's CollectCallOutputs, so the ordinal is the FunctionCall node's output slot.
		if (!Function.ReturnType.IsVoid())
		{
			const FString* ResultPin = Function.Directives.FindPinName(TEXT("Result"));
			OutNames.Add(TEXT("Result"));
			OutPinNames.Add(ResultPin ? *ResultPin : FString(TEXT("Result")));
			OutTypes.Add(Function.ReturnType);
		}
		for (const FBoundParam& Param : Function.Params)
		{
			if (Param.Direction == EParamDirection::In)
			{
				continue;
			}
			OutNames.Add(Param.Name);
			OutPinNames.Add(Param.PinName.IsEmpty() ? Param.Name : Param.PinName);
			OutTypes.Add(Param.Type);
		}
	}

	IR::FIRType FLangBinder::BindFunctionCallOutput(
		const FExpr& Selector,
		const FCallExpr& Call,
		const int32 FunctionIndex,
		const FString& OutputName,
		const int32 OutputOrdinal,
		const FLangSpan& SelectorSpan)
	{
		using namespace LangBinderLegacyPrivate;

		const FBoundFunction& Function = Bound.Functions[FunctionIndex];
		if (const FIdentifierExpr* Identifier = Call.Callee ? Call.Callee->As<FIdentifierExpr>() : nullptr)
		{
			if (!Function.Name.Equals(Identifier->Name, ESearchCase::CaseSensitive))
			{
				ReportLegacyCaseFallback(Identifier->Name, Function.Name, Identifier->Span);
			}
		}

		BindUserFunctionCall(Call, FunctionIndex, /* bStatement */ false, /* bSelection */ true);
		const FBoundExpr* CallBinding = Lookup(Call);
		if (!CallBinding || CallBinding->Kind != EBoundExprKind::FunctionCall)
		{
			return Fail(Selector);
		}

		TArray<FString> Names;
		TArray<FString> PinNames;
		TArray<IR::FIRType> Types;
		CollectLegacyOutputs(Function, Names, PinNames, Types);

		int32 Ordinal = OutputOrdinal;
		if (!OutputName.IsEmpty())
		{
			Ordinal = INDEX_NONE;
			for (int32 Index = 0; Index < Names.Num(); ++Index)
			{
				if (Names[Index].Equals(OutputName, ESearchCase::CaseSensitive) || PinNames[Index].Equals(OutputName, ESearchCase::CaseSensitive))
				{
					Ordinal = Index;
					break;
				}
			}
			if (Ordinal == INDEX_NONE)
			{
				int32 Loose = INDEX_NONE;
				bool bAmbiguous = false;
				for (int32 Index = 0; Index < Names.Num(); ++Index)
				{
					if (Names[Index].Equals(OutputName, ESearchCase::IgnoreCase) || PinNames[Index].Equals(OutputName, ESearchCase::IgnoreCase))
					{
						bAmbiguous = bAmbiguous || Loose != INDEX_NONE;
						Loose = Index;
					}
				}
				if (Loose != INDEX_NONE && !bAmbiguous)
				{
					ReportLegacyCaseFallback(OutputName, Names[Loose], SelectorSpan);
					Ordinal = Loose;
				}
			}
			if (Ordinal == INDEX_NONE)
			{
				Diagnostics.Error(
					TEXT("DSH5280"),
					CurrentFile,
					SelectorSpan,
					Names.IsEmpty()
						? FText::Format(
							LOCTEXT("LegacySelectNoOutputs", "'{0}' has no output to select, and this call selects '{1}'."),
							FText::FromString(Function.Name),
							FText::FromString(OutputName))
						: FText::Format(
							LOCTEXT("LegacySelectUnknownOutput", "'{0}' has no output called '{1}'; its outputs are {2}."),
							FText::FromString(Function.Name),
							FText::FromString(OutputName),
							FText::FromString(QuoteLegacyOutputNames(Names))));
				return Fail(Selector);
			}
		}
		else if (!Names.IsValidIndex(Ordinal))
		{
			Diagnostics.Error(
				TEXT("DSH5282"),
				CurrentFile,
				SelectorSpan,
				FText::Format(
					LOCTEXT("LegacySelectOrdinalRange", "'{0}' has {1} output(s), counted from 0 with the return value first, and this call selects output {2}."),
					FText::FromString(Function.Name),
					FText::AsNumber(Names.Num()),
					FText::AsNumber(Ordinal)));
			return Fail(Selector);
		}

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::FunctionCallOutput;
		Binding.Index = FunctionIndex;
		Binding.FieldIndex = Ordinal;
		Binding.Type = Types[Ordinal];
		return Emit(Selector, MoveTemp(Binding));
	}

	// ---------------------------------------------------------------------------------------------
	// L5: receivers nobody declared
	// ---------------------------------------------------------------------------------------------

	void FLangBinder::DeclareLegacyImplicitOutLocal(const FExpr& Argument, const IR::FIRType& Type, const FString& Receives, const FString& CalleeName)
	{
		const FIdentifierExpr* Identifier = Argument.As<FIdentifierExpr>();
		if (!Identifier || !CurrentFunction || Type.IsError())
		{
			return;
		}
		if (FindLocal(Identifier->Name) != INDEX_NONE
			|| FindParam(Identifier->Name) != INDEX_NONE
			|| FindGlobal(Identifier->Name) != INDEX_NONE
			|| HasDeclarationIgnoringCase(Identifier->Name))
		{
			// Declared -- exactly, or in another case, which L19 then binds with its own warning.
			return;
		}

		DeclareLocal(Identifier->Name, Type, nullptr, 0, Identifier->Span);
		Diagnostics.Info(
			TEXT("DSH5283"),
			CurrentFile,
			Identifier->Span,
			FText::Format(
				LOCTEXT("LegacyImplicitOutLocal", "'{0}' is not declared, and as in 1.x it is declared here as a local of type {1} receiving '{2}' of '{3}'."),
				FText::FromString(Identifier->Name),
				DescribeType(Type),
				FText::FromString(Receives),
				FText::FromString(CalleeName)));
	}

	// ---------------------------------------------------------------------------------------------
	// L8: calls lifted out of an opaque body
	// ---------------------------------------------------------------------------------------------

	void FLangBinder::BindHoistedCalls()
	{
		for (int32 FunctionIndex = 0; FunctionIndex < Bound.Functions.Num(); ++FunctionIndex)
		{
			const FFunctionDecl* Decl = Bound.Functions[FunctionIndex].Decl;
			if (!Decl || Decl->HoistedCalls.Num() == 0)
			{
				continue;
			}

			// The function's own scope: its parameters, the file's globals and literals, nothing of any caller. A call
			// site replaces the parameters with its arguments when the IR builder lowers the lifted calls there.
			CurrentFunctionIndex = FunctionIndex;
			CurrentFunction = &Bound.Functions[FunctionIndex];
			CurrentDecl = Decl;
			CurrentFile = CurrentFunction->File;
			Scopes.Reset();
			LocalArrayValues.Reset();
			LocalWrites.Reset();
			ParamWrites.Init(false, CurrentFunction->Params.Num());
			CurrentCallees.Reset();
			LoopDepth = 0;
			CurrentRegion = INDEX_NONE;
			OuterRegion = INDEX_NONE;

			if (CurrentFunction->Kind != EBoundFunctionKind::Custom || !Decl->bOpaqueBody)
			{
				Diagnostics.Error(
					TEXT("DSH6329"),
					CurrentFile,
					Decl->NameSpan,
					FText::Format(
						LOCTEXT("HoistedCallsNotCustom", "'{0}' carries 'UE.' calls lifted out of its body, and only a '@custom' function with a verbatim body lifts calls into its node's inputs."),
						FText::FromString(CurrentFunction->Name)));
			}
			else
			{
				PushScope();
				bBindingHoistedCall = true;
				HoistingFunctionIndex = FunctionIndex;
				for (const FHoistedCall& Hoisted : Decl->HoistedCalls)
				{
					if (!Hoisted.Call)
					{
						continue;
					}
					const IR::FIRType Type = BindExpr(*Hoisted.Call);
					if (Type.IsError())
					{
						continue;
					}
					if (Type.IsNode())
					{
						// A multi-output node feeds the input its default output, as a node used as a value does.
						SetConversion(*Hoisted.Call, IR::EIRConversion::DefaultOutput);
						continue;
					}
					if (!Type.IsNumeric() && !Type.IsBool() && !Type.IsTexture())
					{
						Diagnostics.Error(
							TEXT("DSH6329"),
							CurrentFile,
							Hoisted.Span,
							FText::Format(
								LOCTEXT("HoistedCallNoInputValue", "'{0}' lifts the call behind its input '{1}', and that call makes {2}, which no custom node input carries."),
								FText::FromString(CurrentFunction->Name),
								FText::FromString(Hoisted.InputName),
								DescribeType(Type)));
					}
				}
				bBindingHoistedCall = false;
				HoistingFunctionIndex = INDEX_NONE;
				PopScope();
			}

			if (CallGraph.IsValidIndex(FunctionIndex))
			{
				CallGraph[FunctionIndex].Append(CurrentCallees);
			}
			CurrentFunction = nullptr;
			CurrentFunctionIndex = INDEX_NONE;
			CurrentDecl = nullptr;
		}

		CurrentFile = RootModule.FilePath;
	}

	void FLangBinder::CheckHoistedCallNamesAtCallSite(const FCallExpr& Call, const int32 CalleeIndex)
	{
		const TArray<TPair<FString, FLangSpan>>* Names = HoistedUnresolvedNames.Find(CalleeIndex);
		if (!Names || !Bound.Functions.IsValidIndex(CalleeIndex))
		{
			return;
		}
		const FBoundFunction& Callee = Bound.Functions[CalleeIndex];

		for (const TPair<FString, FLangSpan>& Name : *Names)
		{
			// 1.x evaluated a lifted call in the caller's scope; 2.0 gives it the function's own. A caller that has the
			// name is the case that changed meaning; a caller that does not is a plain unknown name, said once.
			const bool bCallerHasIt = FindLocal(Name.Key) != INDEX_NONE || FindParam(Name.Key) != INDEX_NONE;
			const FString Key = bCallerHasIt
				? FString::Printf(TEXT("L:%d:%d:%s"), CurrentFunctionIndex, CalleeIndex, *Name.Key)
				: FString::Printf(TEXT("N:%d:%s"), CalleeIndex, *Name.Key);
			if (HoistedNamesReported.ContainsByPredicate([&Key](const FString& Reported) { return Reported.Equals(Key, ESearchCase::CaseSensitive); }))
			{
				continue;
			}
			HoistedNamesReported.Add(Key);

			if (bCallerHasIt)
			{
				Diagnostics.Error(
					TEXT("DSH6325"),
					CurrentFile,
					Call.Span,
					FText::Format(
						LOCTEXT("HoistedCallReadsCallerLocal", "'{0}' lifts a 'UE.' call out of its body that reads '{1}', which 1.x took from the caller's scope and 2.0 does not; pass '{1}' to '{0}' as a parameter."),
						FText::FromString(Callee.Name),
						FText::FromString(Name.Key)));
			}
			else
			{
				Diagnostics.Error(
					TEXT("DSH6326"),
					Callee.File,
					Name.Value,
					FText::Format(
						LOCTEXT("HoistedCallUnknownName", "'{0}' lifts a 'UE.' call out of its body that reads '{1}', which is neither a parameter of '{0}' nor declared at file scope."),
						FText::FromString(Callee.Name),
						FText::FromString(Name.Key)));
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
