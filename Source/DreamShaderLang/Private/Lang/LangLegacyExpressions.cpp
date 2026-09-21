// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The legacy (1.x) front end, expressions: how a 1.x Graph spelling becomes an ordinary 2.0 tree.
//
// Three kinds of work live here, all at parse time, all recorded in FLegacyMigrationInfo:
//
//   - classification: the 1.x type tokens (case-insensitive; `vec3`, `MaterialAttributes`, `StaticBool`)
//     are written into FTypeRef::Name in their 2.0 spelling, and `N::F` becomes the identifier `N_F`;
//   - expansion: a 1.x parameter-node property (StaticSwitchParameter, ChannelMaskParameter,
//     TextureSampleParameter2D, ...) has no 2.0 uniform, so every call or read of it becomes its own
//     reflected `UE.Expression(Class = ...)` call -- one node per use, exactly the node set 1.x made;
//   - rewriting: the output-selecting pseudo-arguments and the 1.x shorthands (research-legacy.md 3.6).
//
// Nothing here looks a name up in a symbol table. What the parser cannot know -- whether `F` is an
// extern, whether `UE.X` has an output called `Min` -- stays for the binder, which applies the legacy
// rules to everything under a declaration marked FDecl::bLegacy.

#include "LangParserInternal.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangInstanceSource.h"
#include "Lang/LangLegacy.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/Char.h"
#include "Misc/CString.h"
#include "Templates/UniquePtr.h"

#define LOCTEXT_NAMESPACE "DreamShader.Lang.LegacyExpressions"

namespace UE::DreamShader::Lang::Private
{
	// ---------------------------------------------------------------------------------------------
	// Small AST builders shared by the legacy translation units
	// ---------------------------------------------------------------------------------------------

	namespace LegacyAst
	{
		FExprPtr MakeIdentifier(const FString& Name, const FLangSpan& Span)
		{
			TUniquePtr<FIdentifierExpr> Node = MakeUnique<FIdentifierExpr>();
			Node->Name = Name;
			Node->Span = Span;
			return Node;
		}

		FExprPtr MakeStringLiteral(const FString& Value, const FLangSpan& Span)
		{
			TUniquePtr<FLiteralExpr> Node = MakeUnique<FLiteralExpr>();
			Node->LiteralKind = ELiteralKind::String;
			Node->Text = Value;
			Node->Span = Span;
			return Node;
		}

		FExprPtr MakeIntLiteral(const int64 Value, const FLangSpan& Span)
		{
			if (Value < 0)
			{
				TUniquePtr<FUnaryExpr> Negate = MakeUnique<FUnaryExpr>();
				Negate->Op = EUnaryOp::Negate;
				Negate->Operand = MakeIntLiteral(-Value, Span);
				Negate->Span = Span;
				return Negate;
			}

			TUniquePtr<FLiteralExpr> Node = MakeUnique<FLiteralExpr>();
			Node->LiteralKind = ELiteralKind::Int;
			Node->Text = FString::Printf(TEXT("%lld"), Value);
			Node->Integer = static_cast<uint64>(Value);
			Node->Real = static_cast<double>(Value);
			Node->Span = Span;
			return Node;
		}

		FExprPtr MakeFloatLiteral(const FString& Lexeme, const double Value, const FLangSpan& Span)
		{
			FString Text = Lexeme.TrimStartAndEnd();
			if (Text.StartsWith(TEXT("-")))
			{
				TUniquePtr<FUnaryExpr> Negate = MakeUnique<FUnaryExpr>();
				Negate->Op = EUnaryOp::Negate;
				Negate->Operand = MakeFloatLiteral(Text.RightChop(1), -Value, Span);
				Negate->Span = Span;
				return Negate;
			}

			TUniquePtr<FLiteralExpr> Node = MakeUnique<FLiteralExpr>();
			Node->LiteralKind = ELiteralKind::Float;
			Node->Text = Text;
			Node->Real = Value;
			Node->Span = Span;
			return Node;
		}

		FExprPtr MakeBoolLiteral(const bool bValue, const FLangSpan& Span)
		{
			TUniquePtr<FLiteralExpr> Node = MakeUnique<FLiteralExpr>();
			Node->LiteralKind = ELiteralKind::Bool;
			Node->bBool = bValue;
			Node->Text = bValue ? TEXT("true") : TEXT("false");
			Node->Span = Span;
			return Node;
		}

		FExprPtr MakeReflectedCallee(const FString& Namespace, const FString& Name, const FLangSpan& Span)
		{
			TUniquePtr<FMemberExpr> Node = MakeUnique<FMemberExpr>();
			Node->Object = MakeIdentifier(Namespace, Span);
			Node->Member = Name;
			Node->MemberSpan = Span;
			Node->Span = Span;
			return Node;
		}

		FArgument MakeNamedArgument(const FString& Name, FExprPtr Value, const FLangSpan& Span)
		{
			FArgument Argument;
			Argument.Name = Name;
			Argument.NameSpan = Span;
			Argument.Value = MoveTemp(Value);
			Argument.Span = Span;
			return Argument;
		}

		TUniquePtr<FCallExpr> MakeExpressionCall(const FString& ClassName, const FLangSpan& Span)
		{
			TUniquePtr<FCallExpr> Call = MakeUnique<FCallExpr>();
			Call->Callee = MakeReflectedCallee(TEXT("UE"), TEXT("Expression"), Span);
			Call->Arguments.Add(MakeNamedArgument(TEXT("Class"), MakeStringLiteral(ClassName, Span), Span));
			Call->Span = Span;
			return Call;
		}

		FExprPtr MakeMember(FExprPtr Object, const FString& Member, const FLangSpan& Span)
		{
			TUniquePtr<FMemberExpr> Node = MakeUnique<FMemberExpr>();
			Node->Object = MoveTemp(Object);
			Node->Member = Member;
			Node->MemberSpan = Span;
			Node->Span = Span;
			return Node;
		}

		FString SanitizeIdentifier(const FString& Text)
		{
			// The 1.x rule (DreamShaderModule.cpp SanitizeIdentifier), restated here because this module
			// may not depend on the runtime module: the flattened `N_F` must be the same string the 1.x
			// HLSL symbol `DreamShaderFn_N_F` was built from.
			FString Result;
			Result.Reserve(Text.Len() + 1);
			for (const TCHAR Character : Text)
			{
				const bool bKeep = (Character >= TEXT('A') && Character <= TEXT('Z'))
					|| (Character >= TEXT('a') && Character <= TEXT('z'))
					|| (Character >= TEXT('0') && Character <= TEXT('9'))
					|| Character == TEXT('_');
				Result.AppendChar(bKeep ? Character : TEXT('_'));
			}

			bool bOnlyUnderscores = true;
			for (const TCHAR Character : Result)
			{
				if (Character != TEXT('_'))
				{
					bOnlyUnderscores = false;
					break;
				}
			}
			if (Result.IsEmpty() || bOnlyUnderscores)
			{
				Result = TEXT("DreamShaderSymbol");
			}

			const TCHAR First = Result[0];
			if (!((First >= TEXT('A') && First <= TEXT('Z')) || (First >= TEXT('a') && First <= TEXT('z')) || First == TEXT('_')))
			{
				Result.InsertAt(0, TEXT('_'));
			}

			for (int32 Index = Result.Len() - 1; Index > 0; --Index)
			{
				if (Result[Index] == TEXT('_') && Result[Index - 1] == TEXT('_'))
				{
					Result.RemoveAt(Index, 1);
				}
			}
			return Result;
		}

		FString Unquote(const FString& Text)
		{
			const FString Trimmed = Text.TrimStartAndEnd();
			if (Trimmed.Len() < 2 || !Trimmed.StartsWith(TEXT("\"")) || !Trimmed.EndsWith(TEXT("\"")))
			{
				return Trimmed;
			}

			const FString Inner = Trimmed.Mid(1, Trimmed.Len() - 2);
			FString Result;
			Result.Reserve(Inner.Len());
			for (int32 Index = 0; Index < Inner.Len(); ++Index)
			{
				const TCHAR Character = Inner[Index];
				if (Character != TEXT('\\') || Index + 1 >= Inner.Len())
				{
					Result.AppendChar(Character);
					continue;
				}
				const TCHAR Escaped = Inner[++Index];
				switch (Escaped)
				{
				case TEXT('n'): Result.AppendChar(TEXT('\n')); break;
				case TEXT('r'): Result.AppendChar(TEXT('\r')); break;
				case TEXT('t'): Result.AppendChar(TEXT('\t')); break;
				default:        Result.AppendChar(Escaped); break;
				}
			}
			return Result;
		}

		/** A decimal number as 1.x `LexTryParseString<double>` reads it: optional sign, digits, one dot, an exponent. */
		static bool LegacyAst_TryParseNumber(const FString& Text, double& OutValue, bool& bOutIsInteger)
		{
			const FString Trimmed = Text.TrimStartAndEnd();
			if (Trimmed.IsEmpty())
			{
				return false;
			}

			int32 Index = 0;
			if (Trimmed[Index] == TEXT('-') || Trimmed[Index] == TEXT('+'))
			{
				++Index;
			}
			bool bDigits = false;
			bool bDot = false;
			bool bExponent = false;
			for (; Index < Trimmed.Len(); ++Index)
			{
				const TCHAR Character = Trimmed[Index];
				if (FChar::IsDigit(Character))
				{
					bDigits = true;
				}
				else if (Character == TEXT('.') && !bDot && !bExponent)
				{
					bDot = true;
				}
				else if ((Character == TEXT('e') || Character == TEXT('E')) && bDigits && !bExponent)
				{
					bExponent = true;
					if (Index + 1 < Trimmed.Len() && (Trimmed[Index + 1] == TEXT('-') || Trimmed[Index + 1] == TEXT('+')))
					{
						++Index;
					}
				}
				else if ((Character == TEXT('f') || Character == TEXT('F')) && Index == Trimmed.Len() - 1 && bDigits)
				{
					// 1.x strips a float suffix.
				}
				else
				{
					return false;
				}
			}
			if (!bDigits)
			{
				return false;
			}

			OutValue = FCString::Atod(*Trimmed);
			bOutIsInteger = !bDot && !bExponent && !Trimmed.EndsWith(TEXT("f"), ESearchCase::IgnoreCase);
			return true;
		}

		FExprPtr MakeValueExpressionFromText(const FString& WrittenText, const FLangSpan& Span)
		{
			const FString Trimmed = WrittenText.TrimStartAndEnd();
			if (Trimmed.Len() >= 2 && Trimmed.StartsWith(TEXT("\"")) && Trimmed.EndsWith(TEXT("\"")))
			{
				return MakeStringLiteral(Unquote(Trimmed), Span);
			}

			double Number = 0.0;
			bool bInteger = false;
			if (LegacyAst_TryParseNumber(Trimmed, Number, bInteger))
			{
				if (bInteger)
				{
					return MakeIntLiteral(static_cast<int64>(Number), Span);
				}
				return MakeFloatLiteral(Trimmed, Number, Span);
			}

			if (Trimmed.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("false"), ESearchCase::IgnoreCase))
			{
				return MakeBoolLiteral(Trimmed.Equals(TEXT("true"), ESearchCase::IgnoreCase), Span);
			}

			bool bIdentifier = Trimmed.Len() > 0 && (FChar::IsAlpha(Trimmed[0]) || Trimmed[0] == TEXT('_'));
			for (int32 Index = 1; bIdentifier && Index < Trimmed.Len(); ++Index)
			{
				bIdentifier = FChar::IsAlnum(Trimmed[Index]) || Trimmed[Index] == TEXT('_');
			}
			if (bIdentifier)
			{
				return MakeIdentifier(Trimmed, Span);
			}

			return MakeStringLiteral(Trimmed, Span);
		}

		bool TryNormalizeVectorLiteral(const FString& WrittenText, double OutValues[4])
		{
			// 1.x ParseVectorLiteral: the text between the first `(` and the last `)`, split on `,`.
			const FString Trimmed = WrittenText.TrimStartAndEnd();
			const int32 Open = Trimmed.Find(TEXT("("));
			const int32 Close = Trimmed.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (Open == INDEX_NONE || Close == INDEX_NONE || Close <= Open)
			{
				return false;
			}

			TArray<FString> Parts;
			Trimmed.Mid(Open + 1, Close - Open - 1).ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() == 0)
			{
				return false;
			}

			double Parsed[4] = { 0.0, 0.0, 0.0, 1.0 };
			for (int32 Index = 0; Index < Parts.Num() && Index < 4; ++Index)
			{
				const FString Part = Parts[Index].TrimStartAndEnd();
				bool bInteger = false;
				if (!LegacyAst_TryParseNumber(Part, Parsed[Index], bInteger))
				{
					if (Part.Equals(TEXT("true"), ESearchCase::IgnoreCase))
					{
						Parsed[Index] = 1.0;
					}
					else if (Part.Equals(TEXT("false"), ESearchCase::IgnoreCase))
					{
						Parsed[Index] = 0.0;
					}
					else
					{
						return false;
					}
				}
			}

			if (Parts.Num() == 1)
			{
				Parsed[1] = Parsed[0];
				Parsed[2] = Parsed[0];
			}
			else if (Parts.Num() == 2)
			{
				Parsed[2] = 0.0;
				Parsed[3] = 0.0;
			}
			else if (Parts.Num() == 3)
			{
				Parsed[3] = 1.0;
			}

			for (int32 Index = 0; Index < 4; ++Index)
			{
				// 1.x stored these in an FLinearColor.
				OutValues[Index] = static_cast<double>(static_cast<float>(Parsed[Index]));
			}
			return true;
		}

		FExprPtr MakeFloat4Constructor(const double Values[4], const FLangSpan& Span)
		{
			TUniquePtr<FTypeExpr> Callee = MakeUnique<FTypeExpr>();
			FLangParser::ClassifyTypeName(TEXT("float4"), Callee->Type);
			Callee->Type.Span = Span;
			Callee->Span = Span;

			TUniquePtr<FCallExpr> Call = MakeUnique<FCallExpr>();
			Call->Callee = MoveTemp(Callee);
			for (int32 Index = 0; Index < 4; ++Index)
			{
				FArgument Argument;
				Argument.Value = MakeFloatLiteral(FormatDreamShaderFloatLiteral(Values[Index]), Values[Index], Span);
				Argument.Span = Span;
				Call->Arguments.Add(MoveTemp(Argument));
			}
			Call->Span = Span;
			return Call;
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Types
	// ---------------------------------------------------------------------------------------------

	namespace LegacyExpressions
	{
		struct FLegacyTypeAlias
		{
			/** Lower case. */
			const TCHAR* From;
			const TCHAR* To;
		};

		/** The 1.x spellings with a different 2.0 name (NormalizeShaderTypeToken plus the section and Graph tokens). */
		static const FLegacyTypeAlias GLegacyTypeAliases[] =
		{
			{ TEXT("vec2"), TEXT("float2") },
			{ TEXT("vec3"), TEXT("float3") },
			{ TEXT("vec4"), TEXT("float4") },
			{ TEXT("ivec2"), TEXT("int2") },
			{ TEXT("ivec3"), TEXT("int3") },
			{ TEXT("ivec4"), TEXT("int4") },
			{ TEXT("uvec2"), TEXT("uint2") },
			{ TEXT("uvec3"), TEXT("uint3") },
			{ TEXT("uvec4"), TEXT("uint4") },
			{ TEXT("bvec2"), TEXT("bool2") },
			{ TEXT("bvec3"), TEXT("bool3") },
			{ TEXT("bvec4"), TEXT("bool4") },
			{ TEXT("mat2"), TEXT("float2x2") },
			{ TEXT("mat3"), TEXT("float3x3") },
			{ TEXT("mat4"), TEXT("float4x4") },
			{ TEXT("materialattributes"), TEXT("material") },
			{ TEXT("staticbool"), TEXT("bool") },
			{ TEXT("staticboolparameter"), TEXT("bool") },
			{ TEXT("texture2d"), TEXT("Texture2D") },
			{ TEXT("texturecube"), TEXT("TextureCube") },
			{ TEXT("texture2darray"), TEXT("Texture2DArray") },
			{ TEXT("texture3d"), TEXT("Texture3D") },
			{ TEXT("volumetexture"), TEXT("VolumeTexture") },
			{ TEXT("samplerstate"), TEXT("SamplerState") },
			{ TEXT("substrate"), TEXT("Substrate") },
		};
	}

	void FLangParser::ClassifyLegacyTypeName(const FString& Spelling, FTypeRef& InOutType)
	{
		const FString Lower = Spelling.ToLower();

		for (const LegacyExpressions::FLegacyTypeAlias& Alias : LegacyExpressions::GLegacyTypeAliases)
		{
			if (Lower.Equals(Alias.From, ESearchCase::CaseSensitive))
			{
				ClassifyTypeName(Alias.To, InOutType);
				return;
			}
		}

		// `Float3`, `HALF`, `uint2`: the scalar, vector and matrix families are lower case in 2.0.
		FTypeRef LowerType;
		ClassifyTypeName(Lower, LowerType);
		if (LowerType.Category == ETypeCategory::Scalar
			|| LowerType.Category == ETypeCategory::Vector
			|| LowerType.Category == ETypeCategory::Matrix
			|| LowerType.Category == ETypeCategory::Void)
		{
			const FLangSpan Span = InOutType.Span;
			InOutType = LowerType;
			InOutType.Span = Span;
			return;
		}

		// A user struct or an unknown word keeps its spelling; the binder judges it.
		ClassifyTypeName(Spelling, InOutType);
	}

	bool FLangParser::IsLegacyBuiltinTypeName(const FString& Name)
	{
		FTypeRef Type;
		ClassifyLegacyTypeName(Name, Type);
		return Type.Category != ETypeCategory::Named;
	}

	void FLangParser::RecordLegacyRename(const FLegacyRename::EKind Kind, const FString& From, const FString& To, const FLangSpan& Span)
	{
		if (!LegacyInfo || From.Equals(To, ESearchCase::CaseSensitive))
		{
			return;
		}

		FLegacyRename& Rename = LegacyInfo->BodyRenames.AddDefaulted_GetRef();
		Rename.Kind = Kind;
		Rename.From = From;
		Rename.To = To;
		Rename.Span = Span;
		Rename.Decl = LegacyRenameDecl;
	}

	FExprPtr FLangParser::MakeLegacyZeroInitializer(const FTypeRef& Type, const FLangSpan& Span)
	{
		const bool bBool = Type.Scalar == EScalarKind::Bool;

		switch (Type.Category)
		{
		case ETypeCategory::Scalar:
			return bBool ? LegacyAst::MakeBoolLiteral(false, Span) : LegacyAst::MakeIntLiteral(0, Span);

		case ETypeCategory::Vector:
		{
			// `float3(0, 0, 0)`: width-exact, one argument per component, so the constructor reads the
			// same whatever the broadcast rule of a one-argument form is.
			TUniquePtr<FTypeExpr> Callee = MakeUnique<FTypeExpr>();
			Callee->Type = Type;
			Callee->Type.Span = Span;
			Callee->Span = Span;

			TUniquePtr<FCallExpr> Call = MakeUnique<FCallExpr>();
			Call->Callee = MoveTemp(Callee);
			for (int32 Component = 0; Component < Type.Rows; ++Component)
			{
				FArgument Argument;
				Argument.Value = bBool ? LegacyAst::MakeBoolLiteral(false, Span) : LegacyAst::MakeIntLiteral(0, Span);
				Argument.Span = Span;
				Call->Arguments.Add(MoveTemp(Argument));
			}
			Call->Span = Span;
			return Call;
		}

		case ETypeCategory::Material:
			// 1.x built one MakeMaterialAttributes node per declaration without an initializer.
			return LegacyAst::MakeExpressionCall(TEXT("MakeMaterialAttributes"), Span);

		case ETypeCategory::Void:
		case ETypeCategory::Matrix:
		case ETypeCategory::Texture:
		case ETypeCategory::Sampler:
		case ETypeCategory::Substrate:
		case ETypeCategory::Named:
			return nullptr;
		}
		return nullptr;
	}

	// ---------------------------------------------------------------------------------------------
	// `N::F`
	// ---------------------------------------------------------------------------------------------

	FExprPtr FLangParser::ParseLegacyQualifiedName()
	{
		const int32 StartIndex = GetTokenIndex();

		FString Qualified = Advance().Text;
		while (Check(ELangTokenKind::Colon) && Peek(1).Kind == ELangTokenKind::Colon)
		{
			Advance();
			Advance();
			if (!Check(ELangTokenKind::Identifier))
			{
				Diagnostics.Error(
					TEXT("DSH5260"),
					Current().Span,
					FText::Format(LOCTEXT("QualifierNeedsName", "Expected a name after '::', found {0}."), DescribeToken(Current())));
				return nullptr;
			}
			Qualified += TEXT("::");
			Qualified += Advance().Text;
		}

		const FLangSpan Span = SpanFrom(StartIndex);
		const FString Flattened = LegacyAst::SanitizeIdentifier(Qualified);

		if (LegacyInfo)
		{
			bool bKnown = false;
			for (const TPair<FString, FString>& Entry : LegacyInfo->NamespaceFlatten)
			{
				if (Entry.Key.Equals(Qualified, ESearchCase::CaseSensitive))
				{
					bKnown = true;
					break;
				}
			}
			if (!bKnown)
			{
				LegacyInfo->NamespaceFlatten.Emplace(Qualified, Flattened);
			}
		}
		RecordLegacyRename(FLegacyRename::EKind::NamespaceQualifier, Qualified, Flattened, Span);

		return LegacyAst::MakeIdentifier(Flattened, Span);
	}

	// ---------------------------------------------------------------------------------------------
	// Expanded properties and the 1.x call shorthands
	// ---------------------------------------------------------------------------------------------

	namespace LegacyExpressions
	{
		/** A 1.x `UE.*` shorthand and the arguments it read; 1.x dropped every other argument without a word. */
		struct FLegacySugar
		{
			const TCHAR* Name;
			/** Comma-separated, compared case-insensitively. */
			const TCHAR* Arguments;
			/** The first positional argument is read as `Input` (TransformVector / TransformPosition). */
			bool bPositionalInput;
		};

		/** MaterialGeneratorCodeUE.cpp's registered builtins, in its order. */
		static const FLegacySugar GLegacySugars[] =
		{
			{ TEXT("TexCoord"), TEXT("Index,UTiling,VTiling,UnMirrorU,UnMirrorV"), false },
			{ TEXT("Time"), TEXT("Period,IgnorePause"), false },
			{ TEXT("Panner"), TEXT("Coordinate,Time,Speed,SpeedX,SpeedY,FractionalPart"), false },
			{ TEXT("WorldPosition"), TEXT(""), false },
			{ TEXT("ObjectPositionWS"), TEXT(""), false },
			{ TEXT("CameraVectorWS"), TEXT(""), false },
			{ TEXT("VertexNormalWS"), TEXT(""), false },
			{ TEXT("VertexTangentWS"), TEXT(""), false },
			{ TEXT("ScreenPosition"), TEXT(""), false },
			{ TEXT("VertexColor"), TEXT(""), false },
			{ TEXT("PixelDepth"), TEXT(""), false },
			{ TEXT("SceneDepth"), TEXT(""), false },
			{ TEXT("SceneColor"), TEXT(""), false },
			{ TEXT("TranslatedWorldPosition"), TEXT(""), false },
			{ TEXT("ObjectPosition"), TEXT(""), false },
			{ TEXT("ObjectRadius"), TEXT(""), false },
			{ TEXT("ObjectBounds"), TEXT(""), false },
			{ TEXT("CameraVector"), TEXT(""), false },
			{ TEXT("CameraPosition"), TEXT(""), false },
			{ TEXT("ReflectionVector"), TEXT(""), false },
			{ TEXT("PixelNormalWS"), TEXT(""), false },
			{ TEXT("TwoSidedSign"), TEXT(""), false },
			{ TEXT("PerInstanceRandom"), TEXT(""), false },
			{ TEXT("PerInstanceFadeAmount"), TEXT(""), false },
			{ TEXT("ViewportUV"), TEXT(""), false },
			{ TEXT("TransformVector"), TEXT("Input,Source,Destination"), true },
			{ TEXT("TransformPosition"), TEXT("Input,Source,Destination,PeriodicWorldTileSize,FirstPersonInterpolationAlpha"), true },
		};

		static const FLegacySugar* FindLegacySugar(const FString& Name)
		{
			for (const FLegacySugar& Sugar : GLegacySugars)
			{
				if (Name.Equals(Sugar.Name, ESearchCase::IgnoreCase))
				{
					return &Sugar;
				}
			}
			return nullptr;
		}

		static bool LegacySugarReads(const FLegacySugar& Sugar, const FString& ArgumentName)
		{
			TArray<FString> Names;
			FString(Sugar.Arguments).ParseIntoArray(Names, TEXT(","), true);
			for (const FString& Candidate : Names)
			{
				if (Candidate.Equals(ArgumentName, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
			return false;
		}

		static bool IsLegacyTextureSampleParameterType(const FString& NodeType)
		{
			return NodeType.Equals(TEXT("TextureSampleParameter2D"), ESearchCase::IgnoreCase)
				|| NodeType.Equals(TEXT("TextureSampleParameter2DArray"), ESearchCase::IgnoreCase)
				|| NodeType.Equals(TEXT("TextureSampleParameterCube"), ESearchCase::IgnoreCase)
				|| NodeType.Equals(TEXT("TextureSampleParameterCubeArray"), ESearchCase::IgnoreCase)
				|| NodeType.Equals(TEXT("TextureSampleParameterVolume"), ESearchCase::IgnoreCase)
				|| NodeType.Equals(TEXT("TextureSampleParameterSubUV"), ESearchCase::IgnoreCase);
		}

		/** `UE.Time` from a callee: ("UE", "Time"); `F`: ("", "F"). False for any other callee shape. */
		static bool SplitLegacyCallee(const FExpr* Callee, FString& OutNamespace, FString& OutName)
		{
			if (!Callee)
			{
				return false;
			}
			if (const FIdentifierExpr* Identifier = Callee->As<FIdentifierExpr>())
			{
				OutNamespace.Reset();
				OutName = Identifier->Name;
				return true;
			}
			if (const FMemberExpr* Member = Callee->As<FMemberExpr>())
			{
				if (const FIdentifierExpr* Object = Member->Object ? Member->Object->As<FIdentifierExpr>() : nullptr)
				{
					OutNamespace = Object->Name;
					OutName = Member->Member;
					return true;
				}
				// `Substrate` is a type spelling, so `Substrate.X` arrives with a type expression as its object.
				if (const FTypeExpr* TypeObject = Member->Object ? Member->Object->As<FTypeExpr>() : nullptr)
				{
					OutNamespace = TypeObject->Type.Name;
					OutName = Member->Member;
					return true;
				}
			}
			return false;
		}

		static int32 FindLegacyArgument(const FCallExpr& Call, const TCHAR* Name)
		{
			for (int32 Index = 0; Index < Call.Arguments.Num(); ++Index)
			{
				if (Call.Arguments[Index].Name.Equals(Name, ESearchCase::IgnoreCase))
				{
					return Index;
				}
			}
			return INDEX_NONE;
		}

		/** The spelling a literal-ish argument carries: a string's value, a number's lexeme, a name. */
		static bool TryGetLegacyLiteralText(const FExpr* Expr, FString& OutText)
		{
			if (!Expr)
			{
				return false;
			}
			if (const FLiteralExpr* Literal = Expr->As<FLiteralExpr>())
			{
				OutText = Literal->Text;
				return true;
			}
			if (const FIdentifierExpr* Identifier = Expr->As<FIdentifierExpr>())
			{
				OutText = Identifier->Name;
				return true;
			}
			if (const FTypeExpr* Type = Expr->As<FTypeExpr>())
			{
				OutText = Type->Type.Name;
				return true;
			}
			if (const FUnaryExpr* Unary = Expr->As<FUnaryExpr>())
			{
				FString Inner;
				if (Unary->Op == EUnaryOp::Negate && TryGetLegacyLiteralText(Unary->Operand.Get(), Inner))
				{
					OutText = TEXT("-") + Inner;
					return true;
				}
			}
			return false;
		}

		static bool TryGetLegacyInteger(const FExpr* Expr, int64& OutValue)
		{
			FString Text;
			if (!TryGetLegacyLiteralText(Expr, Text))
			{
				return false;
			}
			Text.TrimStartAndEndInline();
			if (Text.IsEmpty() || !FCString::IsNumeric(*Text) || Text.Contains(TEXT(".")))
			{
				return false;
			}
			OutValue = FCString::Atoi64(*Text);
			return true;
		}

		static FLegacyProperty* FindLegacyExpandedProperty(FLegacyBlockContext* Block, const FString& Name)
		{
			if (!Block)
			{
				return nullptr;
			}
			// 1.x found properties by name ignoring case.
			for (FLegacyProperty& Property : Block->Properties)
			{
				if (Property.bExpandAtUse && Property.Name.Equals(Name, ESearchCase::IgnoreCase))
				{
					return &Property;
				}
			}
			return nullptr;
		}

		/** The canonical argument name a 1.x metadata key becomes on a reflected parameter node. */
		static FString MapLegacyMetadataKeyToArgument(const FString& Key)
		{
			const FString Lower = Key.TrimStartAndEnd().ToLower();
			if (Lower == TEXT("group") || Lower == TEXT("category"))
			{
				return TEXT("Group");
			}
			if (Lower == TEXT("description") || Lower == TEXT("desc") || Lower == TEXT("tooltip"))
			{
				return TEXT("Desc");
			}
			if (Lower == TEXT("sortpriority") || Lower == TEXT("sort"))
			{
				return TEXT("SortPriority");
			}
			if (Lower == TEXT("slidermin"))
			{
				return TEXT("SliderMin");
			}
			if (Lower == TEXT("slidermax"))
			{
				return TEXT("SliderMax");
			}
			if (Lower == TEXT("samplertype"))
			{
				return TEXT("SamplerType");
			}
			return Key.TrimStartAndEnd();
		}

		static void SetLegacyNamedArgument(FCallExpr& Call, const FString& Name, FExprPtr Value, const FLangSpan& Span)
		{
			for (FArgument& Existing : Call.Arguments)
			{
				if (Existing.Name.Equals(Name, ESearchCase::IgnoreCase))
				{
					// A later metadata entry for the same property wins, as it did in 1.x.
					Existing.Value = MoveTemp(Value);
					return;
				}
			}
			Call.Arguments.Add(LegacyAst::MakeNamedArgument(Name, MoveTemp(Value), Span));
		}

		/**
		 * The reflected call one use of an expanded property becomes, before its inputs: the class, the
		 * parameter name (the `ParameterName` metadata or the property's own name), the default, then every
		 * other metadata entry under its reflected name. OutAsset receives the texture-default node.
		 */
		static TUniquePtr<FCallExpr> BuildLegacyPropertyExpansion(const FLegacyProperty& Property, const FLangSpan& UseSpan, const FExpr*& OutAsset)
		{
			OutAsset = nullptr;
			TUniquePtr<FCallExpr> Call = LegacyAst::MakeExpressionCall(Property.NodeType, UseSpan);

			FString ParameterName = Property.Name;
			for (const TPair<FString, FString>& Entry : Property.RawMetadata)
			{
				if (Entry.Key.TrimStartAndEnd().Equals(TEXT("ParameterName"), ESearchCase::IgnoreCase))
				{
					ParameterName = LegacyAst::Unquote(Entry.Value);
				}
			}
			Call->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("ParameterName"), LegacyAst::MakeStringLiteral(ParameterName, UseSpan), UseSpan));

			if (!Property.DefaultText.IsEmpty())
			{
				if (Property.NodeType.Equals(TEXT("StaticSwitchParameter"), ESearchCase::IgnoreCase))
				{
					const bool bDefault = Property.DefaultText.TrimStartAndEnd().Equals(TEXT("true"), ESearchCase::IgnoreCase);
					Call->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("DefaultValue"), LegacyAst::MakeBoolLiteral(bDefault, UseSpan), UseSpan));
				}
				else if (IsLegacyTextureSampleParameterType(Property.NodeType))
				{
					// Unresolved on purpose: the emitter resolves `Path(...)` (research-legacy.md 3.8).
					FExprPtr Asset = LegacyAst::MakeStringLiteral(Property.DefaultText.TrimStartAndEnd(), UseSpan);
					OutAsset = Asset.Get();
					Call->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("Texture"), MoveTemp(Asset), UseSpan));
				}
				else
				{
					double Values[4] = { 0.0, 0.0, 0.0, 1.0 };
					if (LegacyAst::TryNormalizeVectorLiteral(Property.DefaultText, Values))
					{
						Call->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("DefaultValue"), LegacyAst::MakeFloat4Constructor(Values, UseSpan), UseSpan));
					}
				}
			}

			for (const TPair<FString, FString>& Entry : Property.RawMetadata)
			{
				const FString Key = Entry.Key.TrimStartAndEnd();
				if (Key.Equals(TEXT("ParameterName"), ESearchCase::IgnoreCase))
				{
					continue;
				}
				if (Key.Equals(TEXT("Slider"), ESearchCase::IgnoreCase))
				{
					// Stored as "min, max" by the Properties section.
					TArray<FString> Bounds;
					Entry.Value.ParseIntoArray(Bounds, TEXT(","), true);
					if (Bounds.Num() == 2)
					{
						SetLegacyNamedArgument(*Call, TEXT("SliderMin"), LegacyAst::MakeValueExpressionFromText(Bounds[0], UseSpan), UseSpan);
						SetLegacyNamedArgument(*Call, TEXT("SliderMax"), LegacyAst::MakeValueExpressionFromText(Bounds[1], UseSpan), UseSpan);
					}
					continue;
				}
				SetLegacyNamedArgument(*Call, MapLegacyMetadataKeyToArgument(Key), LegacyAst::MakeValueExpressionFromText(Entry.Value, UseSpan), UseSpan);
			}

			return Call;
		}

		static void RecordLegacyExpansion(FLegacyMigrationInfo* Info, const FLegacyProperty& Property, const FLangSpan& UseSpan, const FExpr* Expansion, const FExpr* Asset)
		{
			if (!Info)
			{
				return;
			}
			for (FLegacyParameterDeclaration& Declaration : Info->ParameterDeclarations)
			{
				if (Declaration.DeclarationSpan.Offset == Property.Span.Offset && Declaration.Name.Equals(Property.Name, ESearchCase::CaseSensitive))
				{
					Declaration.UseSpans.Add(UseSpan);
					Declaration.Expansions.Add(Expansion);
					break;
				}
			}
			if (Asset)
			{
				FLegacyAssetReference& Reference = Info->AssetReferences.AddDefaulted_GetRef();
				Reference.Use = FLegacyAssetReference::EUse::PropertyArgument;
				Reference.Text = Property.DefaultText.TrimStartAndEnd();
				Reference.Span = UseSpan;
				Reference.Node = Asset;
			}
		}
	}

	FExprPtr FLangParser::TryExpandLegacyPropertyRead(const FString& Name, const FLangSpan& Span)
	{
		FLegacyProperty* Property = LegacyExpressions::FindLegacyExpandedProperty(LegacyBlock, Name);
		if (!Property || Property->NodeType.Equals(TEXT("StaticSwitchParameter"), ESearchCase::IgnoreCase))
		{
			// A bare StaticSwitchParameter read was no value in 1.x either; the binder reports the name.
			return nullptr;
		}

		const FExpr* Asset = nullptr;
		FExprPtr Result = LegacyExpressions::BuildLegacyPropertyExpansion(*Property, Span, Asset);
		if (LegacyExpressions::IsLegacyTextureSampleParameterType(Property->NodeType))
		{
			// 1.x read a four-component sample parameter through its RGBA output.
			Result = LegacyAst::MakeMember(MoveTemp(Result), TEXT("RGBA"), Span);
		}

		LegacyExpressions::RecordLegacyExpansion(LegacyInfo, *Property, Span, Result.Get(), Asset);
		return Result;
	}

	FExprPtr FLangParser::RewriteLegacyCall(FLegacyBlockContext* Block, TUniquePtr<FCallExpr> Call)
	{
		if (!Call.IsValid())
		{
			return nullptr;
		}

		const FLangSpan CallSpan = Call->Span;
		FString Namespace;
		FString Name;
		const bool bNamedCallee = LegacyExpressions::SplitLegacyCallee(Call->Callee.Get(), Namespace, Name);
		const FString CalleeText = Namespace.IsEmpty() ? Name : Namespace + TEXT(".") + Name;

		// -- `Texture = Path(Plugins.MoonToon, "Textures/T")`: an asset reference written in place, which is how the 1.x
		// decompiler wrote every texture and collection of a node. It is no call: it is carried as the text it is,
		// unresolved, the way a property's default is (research-legacy 3.8) -- the front end cannot see plugin mounts, and
		// the emitter resolves the spelling.
		if (bNamedCallee && Namespace.IsEmpty() && Name.Equals(TEXT("Path"), ESearchCase::IgnoreCase))
		{
			const FString Text = Slice(CallSpan).TrimStartAndEnd();
			FExprPtr Reference = LegacyAst::MakeStringLiteral(Text, CallSpan);
			if (LegacyInfo)
			{
				FLegacyAssetReference& Record = LegacyInfo->AssetReferences.AddDefaulted_GetRef();
				Record.Use = FLegacyAssetReference::EUse::PropertyArgument;
				Record.Text = Text;
				Record.Span = CallSpan;
				Record.Node = Reference.Get();
			}
			return Reference;
		}

		// -- the output-selecting pseudo-arguments
		int32 OutputArgument = LegacyExpressions::FindLegacyArgument(*Call, TEXT("Output"));
		if (OutputArgument == INDEX_NONE)
		{
			OutputArgument = LegacyExpressions::FindLegacyArgument(*Call, TEXT("OutputName"));
		}
		const int32 OutputIndexArgument = LegacyExpressions::FindLegacyArgument(*Call, TEXT("OutputIndex"));

		bool bSelect = false;
		FString SelectName;
		int64 SelectIndex = -1;
		FLangSpan SelectorSpan = CallSpan;

		if (OutputArgument != INDEX_NONE && OutputIndexArgument != INDEX_NONE)
		{
			Diagnostics.Error(
				TEXT("DSH5252"),
				Call->Arguments[OutputIndexArgument].Span,
				LOCTEXT("SelectorBoth", "Expected either 'Output' or 'OutputIndex' on this call, found both."));
			return nullptr;
		}
		if (OutputIndexArgument != INDEX_NONE)
		{
			const FArgument& Argument = Call->Arguments[OutputIndexArgument];
			if (!LegacyExpressions::TryGetLegacyInteger(Argument.Value.Get(), SelectIndex) || SelectIndex < 0)
			{
				Diagnostics.Error(
					TEXT("DSH5250"),
					Argument.Span,
					FText::Format(
						LOCTEXT("OutputIndexNotInteger", "Expected 'OutputIndex' to be a whole number of zero or more, found '{0}'."),
						FText::FromString(Argument.Value ? Slice(Argument.Value->Span) : FString())));
				return nullptr;
			}
			bSelect = true;
			SelectorSpan = Argument.Span;
			Call->Arguments.RemoveAt(OutputIndexArgument);
		}
		else if (OutputArgument != INDEX_NONE)
		{
			const FArgument& Argument = Call->Arguments[OutputArgument];
			// A word or a quoted name. A quoted name need not be a word (`Output = "Modifier Payload"`); a number is no name.
			const bool bText = Argument.Value.IsValid()
				&& (Argument.Value->Is<FIdentifierExpr>()
					|| (Argument.Value->Is<FLiteralExpr>() && static_cast<const FLiteralExpr&>(*Argument.Value).LiteralKind == ELiteralKind::String));
			if (!bText || !LegacyExpressions::TryGetLegacyLiteralText(Argument.Value.Get(), SelectName) || SelectName.TrimStartAndEnd().IsEmpty())
			{
				Diagnostics.Error(
					TEXT("DSH5251"),
					Argument.Span,
					FText::Format(
						LOCTEXT("OutputNotName", "Expected 'Output' to name an output with a quoted name or an identifier, found '{0}'."),
						FText::FromString(Argument.Value ? Slice(Argument.Value->Span) : FString())));
				return nullptr;
			}
			SelectName.TrimStartAndEndInline();
			bSelect = true;
			SelectorSpan = Argument.Span;
			Call->Arguments.RemoveAt(OutputArgument);
		}

		if (bSelect && Call->Callee.IsValid() && Call->Callee->Is<FTypeExpr>())
		{
			Diagnostics.Error(
				TEXT("DSH5253"),
				SelectorSpan,
				FText::Format(
					LOCTEXT("SelectorOnConstructor", "Expected an output selector only on a call that has outputs, found one on the constructor '{0}'."),
					FText::FromString(static_cast<const FTypeExpr&>(*Call->Callee).Type.Name)));
			return nullptr;
		}

		// Warns about and removes an argument 1.x read nothing from.
		auto DropIgnoredArgument = [this, &CalleeText](FCallExpr& Target, const int32 ArgumentIndex) -> void
		{
			const FArgument& Argument = Target.Arguments[ArgumentIndex];
			if (Argument.Name.IsEmpty())
			{
				Diagnostics.Warning(
					TEXT("DSH5254"),
					Argument.Span,
					FText::Format(
						LOCTEXT("IgnoredPositionalArgument", "'{0}' takes no positional argument here; 1.x ignored it, so it is dropped."),
						FText::FromString(CalleeText)));
			}
			else
			{
				Diagnostics.Warning(
					TEXT("DSH5254"),
					Argument.Span,
					FText::Format(
						LOCTEXT("IgnoredNamedArgument", "'{0}' is not an argument '{1}' reads; 1.x ignored it, so it is dropped."),
						FText::FromString(Argument.Name),
						FText::FromString(CalleeText)));
			}
			Target.Arguments.RemoveAt(ArgumentIndex);
		};

		FExprPtr Result;

		if (bNamedCallee && Namespace.IsEmpty())
		{
			if (FLegacyProperty* Property = LegacyExpressions::FindLegacyExpandedProperty(Block, Name))
			{
				const bool bStaticSwitch = Property->NodeType.Equals(TEXT("StaticSwitchParameter"), ESearchCase::IgnoreCase);
				if (bSelect)
				{
					Diagnostics.Error(
						TEXT("DSH5265"),
						SelectorSpan,
						FText::Format(
							LOCTEXT("SelectorOnParameterCall", "Expected the parameter call '{0}' to take only its inputs, found an output selector."),
							FText::FromString(Property->Name)));
					return nullptr;
				}

				const FExpr* Asset = nullptr;
				TUniquePtr<FCallExpr> Expansion = LegacyExpressions::BuildLegacyPropertyExpansion(*Property, CallSpan, Asset);

				if (bStaticSwitch)
				{
					auto FindInput = [&Call](const TCHAR* NamedA, const TCHAR* NamedB, const int32 Position) -> int32
					{
						int32 Found = LegacyExpressions::FindLegacyArgument(*Call, NamedA);
						if (Found == INDEX_NONE)
						{
							Found = LegacyExpressions::FindLegacyArgument(*Call, NamedB);
						}
						if (Found == INDEX_NONE && Call->Arguments.IsValidIndex(Position) && Call->Arguments[Position].Name.IsEmpty())
						{
							Found = Position;
						}
						return Found;
					};

					const int32 TrueIndex = FindInput(TEXT("True"), TEXT("A"), 0);
					const int32 FalseIndex = FindInput(TEXT("False"), TEXT("B"), 1);
					if (TrueIndex == INDEX_NONE || FalseIndex == INDEX_NONE || TrueIndex == FalseIndex)
					{
						Diagnostics.Error(
							TEXT("DSH5258"),
							CallSpan,
							FText::Format(
								LOCTEXT("StaticSwitchInputs", "Expected the static switch '{0}' to be called with a 'True = ...' and a 'False = ...' input, found at most one of them."),
								FText::FromString(Property->Name)));
						return nullptr;
					}

					FExprPtr TrueValue = MoveTemp(Call->Arguments[TrueIndex].Value);
					FExprPtr FalseValue = MoveTemp(Call->Arguments[FalseIndex].Value);
					for (int32 ItemIndex = Call->Arguments.Num() - 1; ItemIndex >= 0; --ItemIndex)
					{
						if (ItemIndex != TrueIndex && ItemIndex != FalseIndex)
						{
							DropIgnoredArgument(*Call, ItemIndex);
						}
					}
					Expansion->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("A"), MoveTemp(TrueValue), CallSpan));
					Expansion->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("B"), MoveTemp(FalseValue), CallSpan));
					Result = MoveTemp(Expansion);
				}
				else
				{
					for (FArgument& Argument : Call->Arguments)
					{
						if (Argument.Name.IsEmpty())
						{
							Diagnostics.Error(
								TEXT("DSH5259"),
								Argument.Span,
								FText::Format(
									LOCTEXT("PinCallPositional", "Expected every argument of the parameter call '{0}' to name an input pin, found a positional argument."),
									FText::FromString(Property->Name)));
							return nullptr;
						}
						Expansion->Arguments.Add(MoveTemp(Argument));
					}
					Result = MoveTemp(Expansion);
					if (LegacyExpressions::IsLegacyTextureSampleParameterType(Property->NodeType))
					{
						Result = LegacyAst::MakeMember(MoveTemp(Result), TEXT("RGBA"), CallSpan);
					}
				}

				LegacyExpressions::RecordLegacyExpansion(LegacyInfo, *Property, CallSpan, Result.Get(), Asset);
				return Result;
			}

			if (Name.Equals(TEXT("SampleTexture2D"), ESearchCase::CaseSensitive))
			{
				const bool bShape = !bSelect
					&& Call->Arguments.Num() == 2
					&& Call->Arguments[0].Name.IsEmpty()
					&& Call->Arguments[1].Name.IsEmpty();
				if (!bShape)
				{
					Diagnostics.Error(
						TEXT("DSH5256"),
						CallSpan,
						FText::Format(
							LOCTEXT("SampleTexture2DShape", "Expected 'SampleTexture2D' to take exactly two positional arguments, a texture and coordinates, found {0} argument(s)."),
							FText::AsNumber(Call->Arguments.Num() + (bSelect ? 1 : 0))));
					return nullptr;
				}

				// 1.x desugared this to `UE.Expression(Class="TextureSample", OutputType="float4", ...)`: the sample as a
				// WHOLE, four channels wide, whose masks its retarget then moved onto the RGB / A / RGBA pins. That is a
				// node of channel views in 2.0 terms, so the call stands for itself and is not cut down to its first
				// output: `float4 t = SampleTexture2D(T, uv)` is RGBA, `float3 c = ...` is RGB, `.a` is the A pin.
				TUniquePtr<FCallExpr> Sample = LegacyAst::MakeExpressionCall(TEXT("TextureSample"), CallSpan);
				Sample->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("TextureObject"), MoveTemp(Call->Arguments[0].Value), Call->Arguments[0].Span));
				Sample->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("Coordinates"), MoveTemp(Call->Arguments[1].Value), Call->Arguments[1].Span));
				return Sample;
			}

			const bool bBreakOut = Name.Equals(TEXT("BreakOutFloat2Components"), ESearchCase::IgnoreCase)
				|| Name.Equals(TEXT("BreakOutFloat3Components"), ESearchCase::IgnoreCase)
				|| Name.Equals(TEXT("BreakOutFloat4Components"), ESearchCase::IgnoreCase);
			if (bBreakOut && bSelect && Call->Arguments.Num() >= 1 && Call->Arguments[0].Name.IsEmpty())
			{
				const int32 Width = static_cast<int32>(Name[13] - TEXT('0'));
				int32 Channel = INDEX_NONE;
				if (SelectIndex >= 0)
				{
					Channel = static_cast<int32>(SelectIndex);
				}
				else if (FCString::IsNumeric(*SelectName) && !SelectName.Contains(TEXT(".")))
				{
					Channel = FCString::Atoi(*SelectName);
				}
				else if (SelectName.Len() == 1)
				{
					switch (FChar::ToLower(SelectName[0]))
					{
					case TEXT('x'): case TEXT('r'): Channel = 0; break;
					case TEXT('y'): case TEXT('g'): Channel = 1; break;
					case TEXT('z'): case TEXT('b'): Channel = 2; break;
					case TEXT('w'): case TEXT('a'): Channel = 3; break;
					default: break;
					}
				}

				if (Channel >= 0 && Channel < Width)
				{
					// 1.x never built the asset call: it read the channel straight off the input.
					static const TCHAR* const Channels[] = { TEXT("r"), TEXT("g"), TEXT("b"), TEXT("a") };
					FExprPtr Swizzle = LegacyAst::MakeMember(MoveTemp(Call->Arguments[0].Value), Channels[Channel], CallSpan);
					if (LegacyInfo)
					{
						FLegacyOutputSelection& Selection = LegacyInfo->OutputSelections.AddDefaulted_GetRef();
						Selection.Selection = Swizzle.Get();
						Selection.Call = nullptr;
						Selection.OutputName = SelectName;
						Selection.OutputIndex = Channel;
						Selection.Span = CallSpan;
						Selection.Group = INDEX_NONE;
					}
					return Swizzle;
				}
				// Any other selector: 1.x fell back to the asset call, and so does this.
			}
		}
		else if (bNamedCallee && Namespace.Equals(TEXT("UE"), ESearchCase::IgnoreCase))
		{
			if (Name.Equals(TEXT("SceneTexture"), ESearchCase::IgnoreCase))
			{
				const int32 IdIndex = LegacyExpressions::FindLegacyArgument(*Call, TEXT("Id"));
				if (bSelect || Call->Arguments.Num() != 1 || IdIndex != 0)
				{
					Diagnostics.Error(
						TEXT("DSH5255"),
						CallSpan,
						FText::Format(
							LOCTEXT("SceneTextureShape", "Expected 'UE.SceneTexture' to take exactly one argument, 'Id = ...', found {0} argument(s)."),
							FText::AsNumber(Call->Arguments.Num() + (bSelect ? 1 : 0))));
					return nullptr;
				}

				TUniquePtr<FCallExpr> Scene = LegacyAst::MakeExpressionCall(TEXT("SceneTexture"), CallSpan);
				Scene->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("SceneTextureId"), MoveTemp(Call->Arguments[0].Value), Call->Arguments[0].Span));

				TUniquePtr<FIndexExpr> First = MakeUnique<FIndexExpr>();
				First->Object = MoveTemp(Scene);
				First->Index = LegacyAst::MakeIntLiteral(0, CallSpan);
				First->Span = CallSpan;
				LegacySynthesizedIndexExprs.Add(First.Get());
				return First;
			}

			if (Name.Equals(TEXT("StaticSwitchParameter"), ESearchCase::IgnoreCase))
			{
				int32 NameIndex = LegacyExpressions::FindLegacyArgument(*Call, TEXT("Name"));
				if (NameIndex == INDEX_NONE)
				{
					NameIndex = LegacyExpressions::FindLegacyArgument(*Call, TEXT("ParameterName"));
				}
				FString ParameterName;
				if (NameIndex == INDEX_NONE
					|| !LegacyExpressions::TryGetLegacyLiteralText(Call->Arguments[NameIndex].Value.Get(), ParameterName)
					|| ParameterName.TrimStartAndEnd().IsEmpty())
				{
					Diagnostics.Error(
						TEXT("DSH5257"),
						NameIndex == INDEX_NONE ? CallSpan : Call->Arguments[NameIndex].Span,
						LOCTEXT("StaticSwitchSugarName", "Expected 'UE.StaticSwitchParameter' to name its parameter with 'Name = \"...\"', found no name."));
					return nullptr;
				}
				ParameterName.TrimStartAndEndInline();

				auto FindInput = [&Call](const TCHAR* NamedA, const TCHAR* NamedB, const int32 Position) -> int32
				{
					int32 Found = LegacyExpressions::FindLegacyArgument(*Call, NamedA);
					if (Found == INDEX_NONE)
					{
						Found = LegacyExpressions::FindLegacyArgument(*Call, NamedB);
					}
					if (Found == INDEX_NONE && Call->Arguments.IsValidIndex(Position) && Call->Arguments[Position].Name.IsEmpty())
					{
						Found = Position;
					}
					return Found;
				};
				const int32 TrueIndex = FindInput(TEXT("True"), TEXT("A"), 0);
				const int32 FalseIndex = FindInput(TEXT("False"), TEXT("B"), 1);
				if (bSelect || TrueIndex == INDEX_NONE || FalseIndex == INDEX_NONE || TrueIndex == FalseIndex)
				{
					Diagnostics.Error(
						TEXT("DSH5258"),
						CallSpan,
						FText::Format(
							LOCTEXT("StaticSwitchSugarInputs", "Expected the static switch '{0}' to be called with a 'True = ...' and a 'False = ...' input, found at most one of them."),
							FText::FromString(ParameterName)));
					return nullptr;
				}

				int32 DefaultIndex = LegacyExpressions::FindLegacyArgument(*Call, TEXT("Default"));
				if (DefaultIndex == INDEX_NONE)
				{
					DefaultIndex = LegacyExpressions::FindLegacyArgument(*Call, TEXT("DefaultValue"));
				}
				FString DefaultText;
				if (DefaultIndex != INDEX_NONE)
				{
					if (!LegacyExpressions::TryGetLegacyLiteralText(Call->Arguments[DefaultIndex].Value.Get(), DefaultText)
						|| !(DefaultText.Equals(TEXT("true"), ESearchCase::IgnoreCase) || DefaultText.Equals(TEXT("false"), ESearchCase::IgnoreCase)))
					{
						Diagnostics.Error(
							TEXT("DSH5263"),
							Call->Arguments[DefaultIndex].Span,
							FText::Format(
								LOCTEXT("StaticSwitchSugarDefault", "Expected 'Default' of 'UE.StaticSwitchParameter' to be true or false, found '{0}'."),
								FText::FromString(Call->Arguments[DefaultIndex].Value ? Slice(Call->Arguments[DefaultIndex].Value->Span) : FString())));
						return nullptr;
					}
				}

				const int32 SortIndex = LegacyExpressions::FindLegacyArgument(*Call, TEXT("SortPriority"));
				int64 SortPriority = 0;
				if (SortIndex != INDEX_NONE && !LegacyExpressions::TryGetLegacyInteger(Call->Arguments[SortIndex].Value.Get(), SortPriority))
				{
					Diagnostics.Error(
						TEXT("DSH5264"),
						Call->Arguments[SortIndex].Span,
						FText::Format(
							LOCTEXT("StaticSwitchSugarSort", "Expected 'SortPriority' of 'UE.StaticSwitchParameter' to be a whole number, found '{0}'."),
							FText::FromString(Call->Arguments[SortIndex].Value ? Slice(Call->Arguments[SortIndex].Value->Span) : FString())));
					return nullptr;
				}

				const int32 GroupIndex = LegacyExpressions::FindLegacyArgument(*Call, TEXT("Group"));
				const int32 DescriptionIndex = LegacyExpressions::FindLegacyArgument(*Call, TEXT("Description"));

				TUniquePtr<FCallExpr> Switch = LegacyAst::MakeExpressionCall(TEXT("StaticSwitchParameter"), CallSpan);
				Switch->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("ParameterName"), LegacyAst::MakeStringLiteral(ParameterName, CallSpan), CallSpan));
				if (DefaultIndex != INDEX_NONE)
				{
					Switch->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("DefaultValue"), LegacyAst::MakeBoolLiteral(DefaultText.Equals(TEXT("true"), ESearchCase::IgnoreCase), CallSpan), CallSpan));
				}

				FLegacyParameterDeclaration Declaration;
				Declaration.Name = ParameterName;
				Declaration.NodeType = TEXT("UE.StaticSwitchParameter");
				Declaration.DefaultText = DefaultText;
				Declaration.DeclarationSpan = CallSpan;

				const int32 CopiedIndices[] = { GroupIndex, DescriptionIndex, SortIndex };
				const TCHAR* const CopiedNames[] = { TEXT("Group"), TEXT("Desc"), TEXT("SortPriority") };
				for (int32 Copy = 0; Copy < static_cast<int32>(UE_ARRAY_COUNT(CopiedIndices)); ++Copy)
				{
					const int32 ArgumentIndex = CopiedIndices[Copy];
					if (ArgumentIndex == INDEX_NONE || !Call->Arguments[ArgumentIndex].Value.IsValid())
					{
						continue;
					}
					Declaration.Metadata.Emplace(Call->Arguments[ArgumentIndex].Name, Slice(Call->Arguments[ArgumentIndex].Value->Span));
					Switch->Arguments.Add(LegacyAst::MakeNamedArgument(CopiedNames[Copy], MoveTemp(Call->Arguments[ArgumentIndex].Value), Call->Arguments[ArgumentIndex].Span));
				}
				Switch->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("A"), MoveTemp(Call->Arguments[TrueIndex].Value), Call->Arguments[TrueIndex].Span));
				Switch->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("B"), MoveTemp(Call->Arguments[FalseIndex].Value), Call->Arguments[FalseIndex].Span));

				for (int32 ItemIndex = Call->Arguments.Num() - 1; ItemIndex >= 0; --ItemIndex)
				{
					const bool bConsumed = ItemIndex == NameIndex || ItemIndex == TrueIndex || ItemIndex == FalseIndex || ItemIndex == DefaultIndex
						|| ItemIndex == GroupIndex || ItemIndex == DescriptionIndex || ItemIndex == SortIndex;
					if (!bConsumed)
					{
						DropIgnoredArgument(*Call, ItemIndex);
					}
				}

				Declaration.UseSpans.Add(CallSpan);
				Declaration.Expansions.Add(Switch.Get());
				if (LegacyInfo)
				{
					LegacyInfo->ParameterDeclarations.Add(MoveTemp(Declaration));
				}
				return Switch;
			}

			if (Name.Equals(TEXT("TranslatedWorldPosition"), ESearchCase::IgnoreCase))
			{
				for (int32 ItemIndex = Call->Arguments.Num() - 1; ItemIndex >= 0; --ItemIndex)
				{
					DropIgnoredArgument(*Call, ItemIndex);
				}
				if (FMemberExpr* Callee = Call->Callee.IsValid() ? Call->Callee->As<FMemberExpr>() : nullptr)
				{
					Callee->Member = TEXT("WorldPosition");
				}
				Call->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("WorldPositionShaderOffset"), LegacyAst::MakeIdentifier(TEXT("WPT_CameraRelative"), CallSpan), CallSpan));
				Result = MoveTemp(Call);
			}
			else
			{
				if (const LegacyExpressions::FLegacySugar* Sugar = LegacyExpressions::FindLegacySugar(Name))
				{
					for (int32 ItemIndex = Call->Arguments.Num() - 1; ItemIndex >= 0; --ItemIndex)
					{
						const FArgument& Argument = Call->Arguments[ItemIndex];
						const bool bRead = Argument.Name.IsEmpty()
							? (Sugar->bPositionalInput && ItemIndex == 0)
							: LegacyExpressions::LegacySugarReads(*Sugar, Argument.Name);
						if (!bRead)
						{
							DropIgnoredArgument(*Call, ItemIndex);
						}
					}

					if (Name.Equals(TEXT("Time"), ESearchCase::IgnoreCase) && LegacyExpressions::FindLegacyArgument(*Call, TEXT("Period")) != INDEX_NONE)
					{
						// 1.x set bOverride_Period whenever it saw a Period.
						Call->Arguments.Add(LegacyAst::MakeNamedArgument(TEXT("bOverride_Period"), LegacyAst::MakeBoolLiteral(true, CallSpan), CallSpan));
					}
				}

				// `OutputType` / `ResultType`: a width hint 1.x needed and 2.0 reads from the catalog, except on a
				// Custom expression, where it is the node's own output type.
				FString ClassText;
				const int32 ClassIndex = LegacyExpressions::FindLegacyArgument(*Call, TEXT("Class"));
				const bool bCustom = Name.Equals(TEXT("Expression"), ESearchCase::IgnoreCase)
					&& ClassIndex != INDEX_NONE
					&& LegacyExpressions::TryGetLegacyLiteralText(Call->Arguments[ClassIndex].Value.Get(), ClassText)
					&& (ClassText.Equals(TEXT("Custom"), ESearchCase::IgnoreCase) || ClassText.Equals(TEXT("MaterialExpressionCustom"), ESearchCase::IgnoreCase));

				bool bKeptOutputType = false;
				for (int32 ItemIndex = 0; ItemIndex < Call->Arguments.Num();)
				{
					FArgument& Argument = Call->Arguments[ItemIndex];
					const bool bOutputType = Argument.Name.Equals(TEXT("OutputType"), ESearchCase::IgnoreCase)
						|| Argument.Name.Equals(TEXT("ResultType"), ESearchCase::IgnoreCase);
					if (!bOutputType)
					{
						++ItemIndex;
						continue;
					}

					if (bCustom && !bKeptOutputType)
					{
						FString TypeText;
						LegacyExpressions::TryGetLegacyLiteralText(Argument.Value.Get(), TypeText);
						const FString Key = TypeText.Replace(TEXT(" "), TEXT("")).ToLower();
						const TCHAR* Mapped = nullptr;
						if (Key == TEXT("float") || Key == TEXT("float1") || Key == TEXT("half") || Key == TEXT("half1"))
						{
							Mapped = TEXT("Float1");
						}
						else if (Key == TEXT("float2") || Key == TEXT("vec2") || Key == TEXT("half2"))
						{
							Mapped = TEXT("Float2");
						}
						else if (Key == TEXT("float3") || Key == TEXT("vec3") || Key == TEXT("half3"))
						{
							Mapped = TEXT("Float3");
						}
						else if (Key == TEXT("float4") || Key == TEXT("vec4") || Key == TEXT("half4"))
						{
							Mapped = TEXT("Float4");
						}
						else if (Key == TEXT("materialattributes"))
						{
							Mapped = TEXT("MaterialAttributes");
						}

						if (!Mapped)
						{
							Diagnostics.Error(
								TEXT("DSH5261"),
								Argument.Span,
								FText::Format(
									LOCTEXT("CustomOutputType", "Expected 'OutputType' of a Custom expression to be float1 to float4 or MaterialAttributes, found '{0}'."),
									FText::FromString(TypeText)));
							return nullptr;
						}

						Argument.Name = TEXT("OutputType");
						Argument.Value = LegacyAst::MakeIdentifier(Mapped, Argument.Span);
						bKeptOutputType = true;
						++ItemIndex;
						continue;
					}

					// Dropped as an argument and kept as what it was: the author's word for the value's width (FCallExpr).
					FString TypeText;
					if (LegacyExpressions::TryGetLegacyLiteralText(Argument.Value.Get(), TypeText))
					{
						FTypeRef Declared;
						ClassifyLegacyTypeName(TypeText.TrimStartAndEnd(), Declared);
						if (Declared.Category == ETypeCategory::Scalar || Declared.Category == ETypeCategory::Vector)
						{
							Declared.Span = Argument.Span;
							Call->LegacyResultType = Declared;
						}
					}
					Call->Arguments.RemoveAt(ItemIndex);
				}

				Result = MoveTemp(Call);
			}
		}
		else if (bNamedCallee && Namespace.Equals(TEXT("Substrate"), ESearchCase::IgnoreCase))
		{
			for (int32 ItemIndex = Call->Arguments.Num() - 1; ItemIndex >= 0; --ItemIndex)
			{
				if (Call->Arguments[ItemIndex].Name.Equals(TEXT("OutputType"), ESearchCase::IgnoreCase)
					|| Call->Arguments[ItemIndex].Name.Equals(TEXT("ResultType"), ESearchCase::IgnoreCase))
				{
					Call->Arguments.RemoveAt(ItemIndex);
				}
			}
			Result = MoveTemp(Call);
		}

		if (!Result.IsValid())
		{
			Result = MoveTemp(Call);
		}

		if (!bSelect)
		{
			return Result;
		}

		// -- the selection itself: `.Name` or `[k]` on the call
		const FCallExpr* SelectedCall = Result->As<FCallExpr>();
		FExprPtr Selection;
		if (SelectIndex >= 0)
		{
			TUniquePtr<FIndexExpr> IndexNode = MakeUnique<FIndexExpr>();
			IndexNode->Object = MoveTemp(Result);
			IndexNode->Index = LegacyAst::MakeIntLiteral(SelectIndex, SelectorSpan);
			IndexNode->Span = CallSpan;
			LegacySynthesizedIndexExprs.Add(IndexNode.Get());
			Selection = MoveTemp(IndexNode);
		}
		else
		{
			// `Output = "Modifier Payload"`: an engine output goes by its display name, which need not be a word. The member
			// carries the name as it was written -- the binder finds the output by it, as 1.x did -- and the migrator, which
			// knows which output that is, writes `[k]` where a `.dss` could not spell the name.
			TUniquePtr<FMemberExpr> Member = MakeUnique<FMemberExpr>();
			Member->Object = MoveTemp(Result);
			Member->Member = SelectName;
			Member->MemberSpan = SelectorSpan;
			Member->Span = CallSpan;
			Selection = MoveTemp(Member);
		}

		if (LegacyInfo)
		{
			// Calls with the same callee and the same arguments were one node in 1.x; migrate prints one
			// statement per group.
			const FString Key = SelectedCall ? PrintDreamShaderLangExpr(*SelectedCall) : FString();
			int32 Group = INDEX_NONE;
			if (!Key.IsEmpty())
			{
				for (int32 ItemIndex = 0; ItemIndex < LegacySelectionGroupKeys.Num(); ++ItemIndex)
				{
					if (LegacySelectionGroupKeys[ItemIndex].Equals(Key, ESearchCase::CaseSensitive))
					{
						Group = ItemIndex;
						break;
					}
				}
				if (Group == INDEX_NONE)
				{
					Group = LegacySelectionGroupKeys.Add(Key);
				}
			}

			FLegacyOutputSelection& Record = LegacyInfo->OutputSelections.AddDefaulted_GetRef();
			Record.Selection = Selection.Get();
			Record.Call = SelectedCall;
			Record.OutputName = SelectName;
			Record.OutputIndex = SelectIndex >= 0 ? static_cast<int32>(SelectIndex) : INDEX_NONE;
			Record.Span = CallSpan;
			Record.Group = Group;
		}

		return Selection;
	}
}

#undef LOCTEXT_NAMESPACE
