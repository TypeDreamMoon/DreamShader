// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The half of the language that lives in comments and `#` lines: `///` directives, `#pragma
// material`, `#pragma layout`, `#pragma region`.
//
// This is the deliberate trade the 2.0 syntax made (proposal §5): `@asset`, `@library`, `@custom`,
// `@layer` decide behaviour, so deleting a comment changes the meaning of the file. What follows
// is the other half of that bargain -- a directive is validated as strictly as a keyword would be,
// a value that does not parse is a message and not a shrug, and a directive written on a
// declaration it cannot mean anything on is said out loud instead of being dropped.
//
// Unknown keys are the one thing that stays permissive: they land in Passthrough and the emitter
// applies them to the parameter node by reflection, which is how 1.x metadata behaved and how a
// file keeps working across an engine that grew a new property.

#include "LangBinderInternal.h"

#include "IR/IR.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Semantic/LangBound.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/CString.h"

#define LOCTEXT_NAMESPACE "DreamShader.Binder.Directives"

namespace UE::DreamShader::Lang::Private
{
	namespace
	{
		bool TryParseDouble(const FString& Text, double& OutValue)
		{
			const FString Trimmed = Text.TrimStartAndEnd();
			if (Trimmed.IsEmpty() || !Trimmed.IsNumeric())
			{
				return false;
			}
			OutValue = FCString::Atod(*Trimmed);
			return true;
		}

		bool TryParseInt(const FString& Text, int32& OutValue)
		{
			const FString Trimmed = Text.TrimStartAndEnd();
			if (Trimmed.IsEmpty() || !Trimmed.IsNumeric() || Trimmed.Contains(TEXT("."), ESearchCase::CaseSensitive))
			{
				return false;
			}
			OutValue = FCString::Atoi(*Trimmed);
			return true;
		}

		/** Splits a directive value on runs of whitespace. */
		void SplitWords(const FString& Value, TArray<FString>& OutWords)
		{
			FString Current;
			for (int32 Index = 0; Index < Value.Len(); ++Index)
			{
				const TCHAR Char = Value[Index];
				if (Char == TEXT(' ') || Char == TEXT('\t'))
				{
					if (!Current.IsEmpty())
					{
						OutWords.Add(Current);
						Current.Reset();
					}
				}
				else
				{
					Current.AppendChar(Char);
				}
			}
			if (!Current.IsEmpty())
			{
				OutWords.Add(Current);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------
	// `///` blocks
	// ---------------------------------------------------------------------------------------------

	FBoundDirectives FLangBinder::BindDirectives(const FDocBlock& Doc, EDirectiveTarget Target, const FTypeRef* DeclaredType)
	{
		FBoundDirectives Out;
		Out.FreeText = FString::Join(Doc.FreeText, TEXT("\n")).TrimStartAndEnd();

		TArray<FString> Seen;
		Seen.Reserve(Doc.Directives.Num());

		const bool bOnUniform = Target == EDirectiveTarget::Uniform;
		const bool bOnFunction = Target == EDirectiveTarget::Function;
		const bool bOnTexture = DeclaredType != nullptr && DeclaredType->IsTexture();

		for (const FDocDirective& Entry : Doc.Directives)
		{
			const FString& Key = Entry.Key;
			const FString Value = Entry.Value.TrimStartAndEnd();

			// `@param` is the one key that may legitimately repeat.
			if (!Key.Equals(Directive::Param, ESearchCase::CaseSensitive))
			{
				bool bRepeat = false;
				for (const FString& Previous : Seen)
				{
					if (Previous.Equals(Key, ESearchCase::CaseSensitive))
					{
						bRepeat = true;
						break;
					}
				}
				if (bRepeat)
				{
					Diagnostics.Warning(
						TEXT("DSH7229"),
						CurrentFile,
						Entry.Span,
						FText::Format(
							LOCTEXT("DirectiveRepeated", "'@{0}' is written twice in this block; the last one wins."),
							FText::FromString(Key)));
				}
				else
				{
					Seen.Add(Key);
				}
			}

			auto WrongTarget = [this, &Entry, &Key](const FText& Where)
			{
				Diagnostics.Warning(
					TEXT("DSH7224"),
					CurrentFile,
					Entry.Span,
					FText::Format(
						LOCTEXT("DirectiveWrongTarget", "'@{0}' means nothing here; it belongs on {1}."),
						FText::FromString(Key),
						Where));
			};

			auto RequireValue = [this, &Entry, &Key, &Value]() -> bool
			{
				if (Value.IsEmpty())
				{
					Diagnostics.Error(
						TEXT("DSH7227"),
						CurrentFile,
						Entry.Span,
						FText::Format(
							LOCTEXT("DirectiveNeedsValue", "'@{0}' needs a value after it."),
							FText::FromString(Key)));
					return false;
				}
				return true;
			};

			if (Key.Equals(Directive::Group, ESearchCase::CaseSensitive))
			{
				if (!bOnUniform)
				{
					WrongTarget(LOCTEXT("TargetUniform", "a 'uniform'"));
				}
				Out.Group = Value;
			}
			else if (Key.Equals(Directive::Desc, ESearchCase::CaseSensitive))
			{
				Out.Desc = Value;
			}
			else if (Key.Equals(Directive::Slider, ESearchCase::CaseSensitive))
			{
				if (!bOnUniform)
				{
					WrongTarget(LOCTEXT("TargetUniform2", "a 'uniform'"));
				}

				TArray<FString> Words;
				SplitWords(Value, Words);
				double Min = 0.0;
				double Max = 0.0;
				if (Words.Num() != 2 || !TryParseDouble(Words[0], Min) || !TryParseDouble(Words[1], Max))
				{
					Diagnostics.Error(
						TEXT("DSH7220"),
						CurrentFile,
						Entry.Span,
						FText::Format(
							LOCTEXT("SliderMalformed", "'@slider' takes two numbers, a minimum and a maximum; '{0}' is not that."),
							FText::FromString(Value)));
				}
				else if (Min >= Max)
				{
					Diagnostics.Error(
						TEXT("DSH7220"),
						CurrentFile,
						Entry.Span,
						LOCTEXT("SliderRange", "'@slider' needs its minimum below its maximum."));
				}
				else
				{
					Out.bHasSlider = true;
					Out.SliderMin = Min;
					Out.SliderMax = Max;
				}
			}
			else if (Key.Equals(Directive::Sort, ESearchCase::CaseSensitive))
			{
				if (!bOnUniform)
				{
					WrongTarget(LOCTEXT("TargetUniform3", "a 'uniform'"));
				}

				int32 Sort = 0;
				if (!TryParseInt(Value, Sort))
				{
					Diagnostics.Error(
						TEXT("DSH7221"),
						CurrentFile,
						Entry.Span,
						FText::Format(
							LOCTEXT("SortMalformed", "'@sort' takes one whole number; '{0}' is not that."),
							FText::FromString(Value)));
				}
				else
				{
					Out.bHasSort = true;
					Out.Sort = Sort;
				}
			}
			else if (Key.Equals(Directive::Name, ESearchCase::CaseSensitive))
			{
				if (!bOnUniform && !bOnFunction)
				{
					WrongTarget(LOCTEXT("TargetUniformOrFunction", "a 'uniform' or an exported function"));
				}
				if (RequireValue())
				{
					Out.Name = Value;
				}
			}
			else if (Key.Equals(Directive::Sampler, ESearchCase::CaseSensitive))
			{
				if (!bOnUniform || !bOnTexture)
				{
					WrongTarget(LOCTEXT("TargetTextureUniform", "a texture 'uniform'"));
				}
				if (Value.IsEmpty())
				{
					// The accepted spellings are the engine's EMaterialSamplerType enumerators, which
					// this module cannot see (CONTRACT §0.9); the emitter resolves and rejects them.
					Diagnostics.Error(
						TEXT("DSH7222"),
						CurrentFile,
						Entry.Span,
						LOCTEXT("SamplerEmpty", "'@sampler' needs a sampler type after it, such as 'Color', 'Normal' or 'LinearColor'."));
				}
				else
				{
					Out.Sampler = Value;
				}
			}
			else if (Key.Equals(Directive::Default, ESearchCase::CaseSensitive))
			{
				if (!bOnUniform || !bOnTexture)
				{
					WrongTarget(LOCTEXT("TargetTextureUniform2", "a texture 'uniform'"));
				}
				if (RequireValue())
				{
					Out.DefaultAsset = Value;
				}
			}
			else if (Key.Equals(Directive::Static, ESearchCase::CaseSensitive))
			{
				// Whether it sits on a `uniform bool` is checked where the type is known.
				Out.bStatic = true;
			}
			else if (Key.Equals(Directive::Library, ESearchCase::CaseSensitive))
			{
				if (!bOnFunction)
				{
					WrongTarget(LOCTEXT("TargetFunction", "an exported function"));
				}
				if (RequireValue())
				{
					Out.Library = Value;
				}
			}
			else if (Key.Equals(Directive::Param, ESearchCase::CaseSensitive))
			{
				if (!bOnFunction)
				{
					WrongTarget(LOCTEXT("TargetFunction2", "a function"));
				}

				TArray<FString> Words;
				SplitWords(Value, Words);
				if (Words.Num() == 0)
				{
					Diagnostics.Error(
						TEXT("DSH7227"),
						CurrentFile,
						Entry.Span,
						LOCTEXT("ParamNeedsName", "'@param' is written '@param <ParameterName> <description>'."));
				}
				else
				{
					const FString ParamName = Words[0];
					FString Text = Value.RightChop(ParamName.Len()).TrimStartAndEnd();
					Out.ParamDocs.Emplace(ParamName, MoveTemp(Text));
				}
			}
			else if (Key.Equals(Directive::Asset, ESearchCase::CaseSensitive))
			{
				if (!bOnFunction)
				{
					WrongTarget(LOCTEXT("TargetExtern", "an 'extern' prototype"));
				}
				if (RequireValue())
				{
					Out.Asset = Value;
				}
			}
			else if (Key.Equals(Directive::Custom, ESearchCase::CaseSensitive))
			{
				if (!bOnFunction)
				{
					WrongTarget(LOCTEXT("TargetFunction3", "a function"));
				}
				Out.bCustom = true;

				if (!Value.IsEmpty())
				{
					if (Value.Equals(TEXT("selfcontained"), ESearchCase::IgnoreCase))
					{
						Out.bSelfContained = true;
					}
					else
					{
						Diagnostics.Warning(
							TEXT("DSH7226"),
							CurrentFile,
							Entry.Span,
							FText::Format(
								LOCTEXT("CustomModifier", "'@custom {0}' is not a modifier this language knows; the only one is 'selfcontained'."),
								FText::FromString(Value)));
					}
				}
			}
			else if (Key.Equals(Directive::Layer, ESearchCase::CaseSensitive))
			{
				if (!bOnFunction)
				{
					WrongTarget(LOCTEXT("TargetFunction4", "an exported function"));
				}
				Out.bLayer = true;
			}
			else if (Key.Equals(Directive::LayerBlend, ESearchCase::CaseSensitive))
			{
				if (!bOnFunction)
				{
					WrongTarget(LOCTEXT("TargetFunction5", "an exported function"));
				}
				Out.bLayerBlend = true;
			}
			else
			{
				// Everything the language does not define: the emitter reflects it onto the node by
				// its engine name, exactly as 1.x did with unknown metadata keys (CONTRACT §6.1).
				Out.Passthrough.Add(Key, Value);
			}
		}

		if (Out.bLayer && Out.bLayerBlend)
		{
			Diagnostics.Error(
				TEXT("DSH7215"),
				CurrentFile,
				Doc.Span,
				LOCTEXT("LayerAndLayerBlend", "'@layer' and '@layerblend' make two different assets; a function is one or the other."));
		}

		if (Out.Desc.IsEmpty())
		{
			Out.Desc = Out.FreeText;
		}

		return Out;
	}

	// ---------------------------------------------------------------------------------------------
	// `#pragma material`
	// ---------------------------------------------------------------------------------------------

	void FLangBinder::BindMaterialPragma(const FPragmaDecl& Decl)
	{
		if (!bHasMaterialPragma)
		{
			bHasMaterialPragma = true;
			FirstMaterialPragmaSpan = Decl.Span;
			FirstMaterialPragmaFile = CurrentFile;
		}

		for (const FPragmaArgument& Argument : Decl.Arguments)
		{
			if (Argument.Key.IsEmpty())
			{
				Diagnostics.Error(
					TEXT("DSH7205"),
					CurrentFile,
					Argument.Span,
					FText::Format(
						LOCTEXT("MaterialPragmaPositional", "'#pragma material' takes 'Key = Value' pairs; '{0}' has no key."),
						FText::FromString(Argument.Value)));
				continue;
			}

			// Keys are the 1.x Settings key names, which the emitter resolves against UMaterial by
			// reflection, so this side neither validates nor normalises them -- it only refuses to
			// let one key be written twice with two answers. FString map keys compare
			// case-insensitively in UE, which is the same fold 1.x applied.
			if (const FLangSpan* First = MaterialSettingSpans.Find(Argument.Key))
			{
				Diagnostics.Error(
					TEXT("DSH7200"),
					CurrentFile,
					Argument.Span,
					FText::Format(
						LOCTEXT("MaterialPragmaDuplicate", "'{0}' is set twice by '#pragma material'; it was already set on line {1}."),
						FText::FromString(Argument.Key),
						FText::AsNumber(First->Line)));
				continue;
			}

			MaterialSettingSpans.Add(Argument.Key, Argument.Span);
			MaterialSettingFiles.Add(Argument.Key, CurrentFile);
			Bound.MaterialSettings.Add(Argument.Key, Argument.Value);
		}
	}

	void FLangBinder::ResolveBackend()
	{
		ResolvedBackend = Options.DefaultBackend;

		FString Value;
		if (!Bound.MaterialSettings.RemoveAndCopyValue(FString(TEXT("Backend")), Value))
		{
			return;
		}

		// Every raise below is about the `Backend` key, which a header may have written, so the file
		// moves to that header for the length of this call and is put back before it returns: the
		// caller is between two passes over the root module and everything after expects the root
		// file again.
		FLangSpan Span;
		if (const FLangSpan* Found = MaterialSettingSpans.Find(FString(TEXT("Backend"))))
		{
			Span = *Found;
		}
		const FString OuterFile = CurrentFile;
		if (const FString* FoundFile = MaterialSettingFiles.Find(FString(TEXT("Backend"))))
		{
			CurrentFile = *FoundFile;
		}

		const FString Trimmed = Value.TrimStartAndEnd().TrimQuotes().TrimStartAndEnd();

		// Proposal §5, the one deliberate break with 1.x: 1.x read `Backend = ""` as Graph, which
		// silently overrode the project default with a value nobody wrote. 2.0 refuses it.
		if (Trimmed.IsEmpty())
		{
			Diagnostics.Error(
				TEXT("DSH7201"),
				CurrentFile,
				Span,
				LOCTEXT("BackendEmpty", "'Backend' has no value; write 'Backend = Graph' or 'Backend = ThinCustom'. An empty value meant Graph in 1.x and means nothing now."));
			CurrentFile = OuterFile;
			return;
		}

		if (Trimmed.Equals(TEXT("Graph"), ESearchCase::IgnoreCase))
		{
			ResolvedBackend = IR::EIRBackend::Graph;
		}
		else if (Trimmed.Equals(TEXT("ThinCustom"), ESearchCase::IgnoreCase))
		{
			ResolvedBackend = IR::EIRBackend::ThinCustom;
		}
		else if (Trimmed.Equals(TEXT("Instance"), ESearchCase::IgnoreCase))
		{
			// The 1.x deprecation-window alias; kept because files in the wild still write it.
			ResolvedBackend = IR::EIRBackend::ThinCustom;
			Diagnostics.Warning(
				TEXT("DSH7204"),
				CurrentFile,
				Span,
				LOCTEXT("BackendInstance", "'Backend = Instance' is the old spelling of 'Backend = ThinCustom'; write the new one."));
		}
		else
		{
			Diagnostics.Error(
				TEXT("DSH7202"),
				CurrentFile,
				Span,
				FText::Format(
					LOCTEXT("BackendUnknown", "'Backend = {0}' is not a backend; the backends are 'Graph' and 'ThinCustom'."),
					FText::FromString(Trimmed)));
		}

		CurrentFile = OuterFile;
	}

	// ---------------------------------------------------------------------------------------------
	// `#pragma layout`
	// ---------------------------------------------------------------------------------------------

	void FLangBinder::BindLayoutPragma(const FPragmaDecl& Decl)
	{
		// Layout is a machine domain: the decompiler writes it and the layout pass reads it. A line
		// that does not parse costs a node its saved position and nothing else, so every complaint
		// here is a warning -- a stale coordinate must never stop a material from building.
		IR::FIRLayoutHint Hint;

		auto BadValue = [this, &Decl](const FPragmaArgument& Argument)
		{
			Diagnostics.Warning(
				TEXT("DSH7230"),
				CurrentFile,
				Argument.Span,
				FText::Format(
					LOCTEXT("LayoutBadValue", "'#pragma layout' expects a whole number for '{0}'; '{1}' was ignored."),
					FText::FromString(Argument.Key),
					FText::FromString(Argument.Value)));
		};

		for (const FPragmaArgument& Argument : Decl.Arguments)
		{
			if (Argument.Key.IsEmpty())
			{
				if (Hint.Kind.IsEmpty())
				{
					Hint.Kind = Argument.Value.TrimStartAndEnd();
				}
				else
				{
					Diagnostics.Warning(
						TEXT("DSH7230"),
						CurrentFile,
						Argument.Span,
						FText::Format(
							LOCTEXT("LayoutExtraPositional", "'#pragma layout' takes one positional selector; '{0}' was ignored."),
							FText::FromString(Argument.Value)));
				}
				continue;
			}

			int32 Number = 0;
			if (Argument.Key.Equals(TEXT("Var"), ESearchCase::IgnoreCase))
			{
				Hint.Var = Argument.Value;
			}
			else if (Argument.Key.Equals(TEXT("Text"), ESearchCase::IgnoreCase)
				|| Argument.Key.Equals(TEXT("Name"), ESearchCase::IgnoreCase))
			{
				Hint.Name = Argument.Value;
			}
			else if (Argument.Key.Equals(TEXT("X"), ESearchCase::IgnoreCase))
			{
				if (TryParseInt(Argument.Value, Number)) { Hint.X = Number; } else { BadValue(Argument); }
			}
			else if (Argument.Key.Equals(TEXT("Y"), ESearchCase::IgnoreCase))
			{
				if (TryParseInt(Argument.Value, Number)) { Hint.Y = Number; } else { BadValue(Argument); }
			}
			else if (Argument.Key.Equals(TEXT("Width"), ESearchCase::IgnoreCase)
				|| Argument.Key.Equals(TEXT("W"), ESearchCase::IgnoreCase))
			{
				if (TryParseInt(Argument.Value, Number)) { Hint.W = Number; Hint.bHasSize = true; } else { BadValue(Argument); }
			}
			else if (Argument.Key.Equals(TEXT("Height"), ESearchCase::IgnoreCase)
				|| Argument.Key.Equals(TEXT("H"), ESearchCase::IgnoreCase))
			{
				if (TryParseInt(Argument.Value, Number)) { Hint.H = Number; Hint.bHasSize = true; } else { BadValue(Argument); }
			}
			else
			{
				Diagnostics.Warning(
					TEXT("DSH7230"),
					CurrentFile,
					Argument.Span,
					FText::Format(
						LOCTEXT("LayoutUnknownKey", "'#pragma layout' has no '{0}' key; it was ignored."),
						FText::FromString(Argument.Key)));
			}
		}

		if (Hint.Kind.IsEmpty())
		{
			Diagnostics.Warning(
				TEXT("DSH7230"),
				CurrentFile,
				Decl.Span,
				LOCTEXT("LayoutNoKind", "'#pragma layout' starts with 'Node' or 'Comment'; the line was ignored."));
			return;
		}

		if (!Hint.Kind.Equals(TEXT("Node"), ESearchCase::IgnoreCase)
			&& !Hint.Kind.Equals(TEXT("Comment"), ESearchCase::IgnoreCase))
		{
			Diagnostics.Warning(
				TEXT("DSH7230"),
				CurrentFile,
				Decl.Span,
				FText::Format(
					LOCTEXT("LayoutUnknownKind", "'#pragma layout({0}, ...)' is neither 'Node' nor 'Comment'; the line was ignored."),
					FText::FromString(Hint.Kind)));
			return;
		}

		// Canonical spelling (CONTRACT §6.13 #47): the selector was accepted case-insensitively above,
		// the validator only warns on an odd one, and the emitter compares exactly -- so it is made
		// exact here, once.
		Hint.Kind = Hint.Kind.Equals(TEXT("Node"), ESearchCase::IgnoreCase) ? TEXT("Node") : TEXT("Comment");

		Bound.LayoutHints.Add(MoveTemp(Hint));
	}

	// ---------------------------------------------------------------------------------------------
	// `#pragma region`
	// ---------------------------------------------------------------------------------------------

	int32 FLangBinder::OpenRegion(const FString& Title, const FLangSpan& Span, TArray<int32>& Stack)
	{
		IR::FIRRegion Region;
		Region.Name = Title.TrimStartAndEnd();
		// At file scope OuterRegion is INDEX_NONE; inside a body it is the file-scope box the
		// function itself was declared in, so an in-body region nests under it.
		Region.Parent = Stack.Num() > 0 ? Stack.Last() : OuterRegion;
		Region.Span = Span;

		const int32 Index = Bound.Regions.Add(MoveTemp(Region));
		Stack.Add(Index);
		CurrentRegion = Index;
		return Index;
	}

	void FLangBinder::CloseRegion(const FLangSpan& Span, TArray<int32>& Stack)
	{
		if (Stack.Num() == 0)
		{
			Diagnostics.Error(
				TEXT("DSH4240"),
				CurrentFile,
				Span,
				LOCTEXT("EndRegionWithoutRegion", "'#pragma endregion' closes a box that was never opened."));
			return;
		}

		Stack.Pop();
		CurrentRegion = Stack.Num() > 0 ? Stack.Last() : OuterRegion;
	}

	void FLangBinder::CloseDanglingRegions(TArray<int32>& Stack)
	{
		for (int32 Index : Stack)
		{
			const IR::FIRRegion& Region = Bound.Regions[Index];
			if (Region.Name.IsEmpty())
			{
				Diagnostics.Error(
					TEXT("DSH4240"),
					CurrentFile,
					Region.Span,
					LOCTEXT("RegionNotClosedUnnamed", "This '#pragma region' is never closed; add a '#pragma endregion'."));
			}
			else
			{
				Diagnostics.Error(
					TEXT("DSH4240"),
					CurrentFile,
					Region.Span,
					FText::Format(
						LOCTEXT("RegionNotClosed", "'#pragma region {0}' is never closed; add a '#pragma endregion'."),
						FText::FromString(Region.Name)));
			}
		}

		Stack.Reset();
		CurrentRegion = OuterRegion;
	}
}

#undef LOCTEXT_NAMESPACE
