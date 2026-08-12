#include "DreamShaderParser.h"

#include "DreamShaderModule.h"
#include "DreamShaderParserInternal.h"
#include "Internationalization/Text.h"

#define LOCTEXT_NAMESPACE "DreamShader.Parser"

namespace UE::DreamShader
{
	namespace Private
	{
		static bool ExtractBalancedDelimited(
			FScanner& Scanner,
			const TCHAR OpenChar,
			const TCHAR CloseChar,
			FString& OutContent,
			FText& OutError)
		{
			Scanner.SkipIgnored();
			if (Scanner.Peek() != OpenChar)
			{
				OutError = FText::Format(LOCTEXT("ExpectedCNearIndexD", "Expected '{0}' near index {1}."),
					FText::FromString(FString::ChrN(1, OpenChar)),
					FText::AsNumber(Scanner.Index));
				return false;
			}

			++Scanner.Index;
			const int32 ContentStart = Scanner.Index;
			int32 Depth = 1;
			bool bInString = false;
			bool bInLineComment = false;
			bool bInBlockComment = false;

			while (!Scanner.IsAtEnd())
			{
				const TCHAR Char = Scanner.Source[Scanner.Index++];
				const TCHAR Next = Scanner.Peek();

				if (bInLineComment)
				{
					if (Char == TCHAR('\n'))
					{
						bInLineComment = false;
					}
					continue;
				}

				if (bInBlockComment)
				{
					if (Char == TCHAR('*') && Next == TCHAR('/'))
					{
						++Scanner.Index;
						bInBlockComment = false;
					}
					continue;
				}

				if (bInString)
				{
					if (Char == TCHAR('\\') && !Scanner.IsAtEnd())
					{
						++Scanner.Index;
					}
					else if (Char == TCHAR('"'))
					{
						bInString = false;
					}
					continue;
				}

				if (Char == TCHAR('"'))
				{
					bInString = true;
					continue;
				}

				if (Char == TCHAR('/') && Next == TCHAR('/'))
				{
					++Scanner.Index;
					bInLineComment = true;
					continue;
				}

				if (Char == TCHAR('/') && Next == TCHAR('*'))
				{
					++Scanner.Index;
					bInBlockComment = true;
					continue;
				}

				if (Char == OpenChar)
				{
					++Depth;
				}
				else if (Char == CloseChar)
				{
					--Depth;
					if (Depth == 0)
					{
						OutContent = Scanner.Source.Mid(ContentStart, (Scanner.Index - ContentStart) - 1);
						return true;
					}
				}
			}

			OutError = FText::Format(LOCTEXT("UnterminatedCBlock", "Unterminated '{0}' block."),
					FText::FromString(FString::ChrN(1, OpenChar)));
			return false;
		}

		static FString NormalizeShaderTypeToken(const FString& InTypeToken)
		{
			FString TypeToken = InTypeToken.TrimStartAndEnd();
			FString Lower = TypeToken;
			Lower.ToLowerInline();

			if (Lower == TEXT("vec2")) return TEXT("float2");
			if (Lower == TEXT("vec3")) return TEXT("float3");
			if (Lower == TEXT("vec4")) return TEXT("float4");
			if (Lower == TEXT("ivec2")) return TEXT("int2");
			if (Lower == TEXT("ivec3")) return TEXT("int3");
			if (Lower == TEXT("ivec4")) return TEXT("int4");
			if (Lower == TEXT("uvec2")) return TEXT("uint2");
			if (Lower == TEXT("uvec3")) return TEXT("uint3");
			if (Lower == TEXT("uvec4")) return TEXT("uint4");
			if (Lower == TEXT("bvec2")) return TEXT("bool2");
			if (Lower == TEXT("bvec3")) return TEXT("bool3");
			if (Lower == TEXT("bvec4")) return TEXT("bool4");
			if (Lower == TEXT("mat2")) return TEXT("float2x2");
			if (Lower == TEXT("mat3")) return TEXT("float3x3");
			if (Lower == TEXT("mat4")) return TEXT("float4x4");

			return TypeToken;
		}

		static bool IsIdentifierStart(const TCHAR Char)
		{
			return FChar::IsAlpha(Char) || Char == TCHAR('_');
		}

		static bool IsIdentifierPart(const TCHAR Char)
		{
			return FChar::IsAlnum(Char) || Char == TCHAR('_');
		}

		static FString NormalizeShaderLanguageText(const FString& InCode)
		{
			static const TMap<FString, FString> IdentifierMap = {
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
				{ TEXT("mix"), TEXT("lerp") },
				{ TEXT("fract"), TEXT("frac") },
				{ TEXT("mod"), TEXT("fmod") }
			};

			FString OutCode;
			OutCode.Reserve(InCode.Len() + 32);

			bool bInString = false;
			bool bInLineComment = false;
			bool bInBlockComment = false;

			for (int32 Index = 0; Index < InCode.Len();)
			{
				const TCHAR Char = InCode[Index];
				const TCHAR Next = InCode.IsValidIndex(Index + 1) ? InCode[Index + 1] : TCHAR('\0');

				if (bInLineComment)
				{
					OutCode.AppendChar(Char);
					if (Char == TCHAR('\n'))
					{
						bInLineComment = false;
					}
					++Index;
					continue;
				}

				if (bInBlockComment)
				{
					OutCode.AppendChar(Char);
					if (Char == TCHAR('*') && Next == TCHAR('/'))
					{
						OutCode.AppendChar(Next);
						bInBlockComment = false;
						Index += 2;
					}
					else
					{
						++Index;
					}
					continue;
				}

				if (bInString)
				{
					OutCode.AppendChar(Char);
					if (Char == TCHAR('\\') && InCode.IsValidIndex(Index + 1))
					{
						OutCode.AppendChar(InCode[Index + 1]);
						Index += 2;
					}
					else
					{
						if (Char == TCHAR('"'))
						{
							bInString = false;
						}
						++Index;
					}
					continue;
				}

				if (Char == TCHAR('"'))
				{
					bInString = true;
					OutCode.AppendChar(Char);
					++Index;
					continue;
				}

				if (Char == TCHAR('/') && Next == TCHAR('/'))
				{
					bInLineComment = true;
					OutCode.AppendChar(Char);
					OutCode.AppendChar(Next);
					Index += 2;
					continue;
				}

				if (Char == TCHAR('/') && Next == TCHAR('*'))
				{
					bInBlockComment = true;
					OutCode.AppendChar(Char);
					OutCode.AppendChar(Next);
					Index += 2;
					continue;
				}

				if (IsIdentifierStart(Char))
				{
					const int32 IdentifierStart = Index++;
					while (InCode.IsValidIndex(Index) && IsIdentifierPart(InCode[Index]))
					{
						++Index;
					}

					FString Identifier = InCode.Mid(IdentifierStart, Index - IdentifierStart);
					FString QualifiedIdentifier = Identifier;
					int32 QualifiedEnd = Index;
					bool bHasNamespaceQualifier = false;
					while (InCode.IsValidIndex(QualifiedEnd + 1)
						&& InCode[QualifiedEnd] == TCHAR(':')
						&& InCode[QualifiedEnd + 1] == TCHAR(':')
						&& InCode.IsValidIndex(QualifiedEnd + 2)
						&& IsIdentifierStart(InCode[QualifiedEnd + 2]))
					{
						int32 NextIdentifierStart = QualifiedEnd + 2;
						int32 NextIdentifierEnd = NextIdentifierStart + 1;
						while (InCode.IsValidIndex(NextIdentifierEnd) && IsIdentifierPart(InCode[NextIdentifierEnd]))
						{
							++NextIdentifierEnd;
						}

						QualifiedIdentifier += TEXT("::") + InCode.Mid(NextIdentifierStart, NextIdentifierEnd - NextIdentifierStart);
						QualifiedEnd = NextIdentifierEnd;
						bHasNamespaceQualifier = true;
					}

					if (bHasNamespaceQualifier)
					{
						OutCode += UE::DreamShader::SanitizeIdentifier(QualifiedIdentifier);
						Index = QualifiedEnd;
						continue;
					}

					if (const FString* Replacement = IdentifierMap.Find(Identifier.ToLower()))
					{
						OutCode += *Replacement;
					}
					else
					{
						OutCode += Identifier;
					}
					continue;
				}

				OutCode.AppendChar(Char);
				++Index;
			}

			return OutCode;
		}

		// Rewrite each top-level `return <expr>;` in a function body into `__return = <expr>;` so a
		// function declared with a return type lowers onto the synthetic __return out-parameter. Mirrors
		// EnsureTopLevelReturn's comment/string/char/brace-aware scan; errors on a bare `return;`.
		static bool LowerReturnToOutAssign(const FString& InHLSL, FString& OutLoweredHLSL, FText& OutError)
		{
			const FString Sanitized = InHLSL.Replace(TEXT("\r\n"), TEXT("\n"));
			auto IsIdentifierPart = [](const TCHAR Character)
			{
				return FChar::IsAlnum(Character) || Character == TCHAR('_');
			};

			FString Result;
			Result.Reserve(Sanitized.Len() + 16);
			int32 BraceDepth = 0;
			bool bInString = false;
			bool bInChar = false;
			bool bInLineComment = false;
			bool bInBlockComment = false;

			int32 Index = 0;
			while (Index < Sanitized.Len())
			{
				const TCHAR Char = Sanitized[Index];
				const TCHAR Next = Sanitized.IsValidIndex(Index + 1) ? Sanitized[Index + 1] : TCHAR('\0');

				if (bInLineComment)
				{
					Result.AppendChar(Char);
					if (Char == TCHAR('\n')) { bInLineComment = false; }
					++Index;
					continue;
				}
				if (bInBlockComment)
				{
					Result.AppendChar(Char);
					if (Char == TCHAR('*') && Next == TCHAR('/')) { Result.AppendChar(Next); bInBlockComment = false; Index += 2; }
					else { ++Index; }
					continue;
				}
				if (bInString || bInChar)
				{
					Result.AppendChar(Char);
					if (Char == TCHAR('\\') && Sanitized.IsValidIndex(Index + 1)) { Result.AppendChar(Sanitized[Index + 1]); Index += 2; continue; }
					if (bInString && Char == TCHAR('"')) { bInString = false; }
					else if (bInChar && Char == TCHAR('\'')) { bInChar = false; }
					++Index;
					continue;
				}
				if (Char == TCHAR('/') && Next == TCHAR('/')) { bInLineComment = true; Result.AppendChar(Char); Result.AppendChar(Next); Index += 2; continue; }
				if (Char == TCHAR('/') && Next == TCHAR('*')) { bInBlockComment = true; Result.AppendChar(Char); Result.AppendChar(Next); Index += 2; continue; }
				if (Char == TCHAR('"')) { bInString = true; Result.AppendChar(Char); ++Index; continue; }
				if (Char == TCHAR('\'')) { bInChar = true; Result.AppendChar(Char); ++Index; continue; }
				if (Char == TCHAR('{')) { ++BraceDepth; Result.AppendChar(Char); ++Index; continue; }
				if (Char == TCHAR('}')) { if (BraceDepth > 0) { --BraceDepth; } Result.AppendChar(Char); ++Index; continue; }

				if (BraceDepth == 0 && Char == TCHAR('r') && Sanitized.Mid(Index, 6) == TEXT("return"))
				{
					const bool bLeftBoundary = (Index == 0) || !IsIdentifierPart(Sanitized[Index - 1]);
					const TCHAR After = Sanitized.IsValidIndex(Index + 6) ? Sanitized[Index + 6] : TCHAR('\0');
					if (bLeftBoundary && !IsIdentifierPart(After))
					{
						int32 Probe = Index + 6;
						while (Sanitized.IsValidIndex(Probe) && FChar::IsWhitespace(Sanitized[Probe])) { ++Probe; }
						if (Sanitized.IsValidIndex(Probe) && Sanitized[Probe] == TCHAR(';'))
						{
							OutError = LOCTEXT("AFunctionWithAReturnType", "A function with a return type cannot use a bare 'return;'. Return a value, e.g. 'return expr;'.");
							return false;
						}
						Result.Append(TEXT("__return ="));
						Index += 6;
						continue;
					}
				}

				Result.AppendChar(Char);
				++Index;
			}

			OutLoweredHLSL = Result;
			return true;
		}

		static bool ParseModernFunctionSignature(
			const FString& FunctionName,
			const FString& ParameterBlock,
			const bool bHasReturnType,
			const FString& ReturnTypeToken,
			FTextShaderFunctionDefinition& OutFunction,
			FText& OutError)
		{
			OutFunction.Inputs.Reset();
			OutFunction.Results.Reset();

			if (bHasReturnType)
			{
				FTextShaderFunctionParameter ReturnParameter;
				ReturnParameter.Type = NormalizeShaderTypeToken(ReturnTypeToken);
				ReturnParameter.Name = TEXT("__return");
				if (ReturnParameter.Type.IsEmpty())
				{
					OutError = FText::Format(LOCTEXT("FunctionSHasAnInvalidReturn", "Function '{0}' has an invalid return type '{1}'."),
					FText::FromString(FunctionName),
					FText::FromString(ReturnTypeToken));
					return false;
				}
				OutFunction.Results.Add(ReturnParameter);
			}

			for (const FString& RawParameter : SplitTopLevelDelimited(ParameterBlock, TCHAR(',')))
			{
				const FString Parameter = RawParameter.TrimStartAndEnd();
				if (Parameter.IsEmpty())
				{
					continue;
				}

				TArray<FString> Parts;
				Parameter.ParseIntoArrayWS(Parts);
				if (Parts.Num() < 2 || Parts.Num() > 3)
				{
					OutError = FText::Format(LOCTEXT("FunctionSHasAnInvalidParameter", "Function '{0}' has an invalid parameter declaration '{1}'."),
					FText::FromString(FunctionName),
					FText::FromString(Parameter));
					return false;
				}

				FString Qualifier = TEXT("in");
				FString TypeToken;
				FString NameToken;
				if (Parts.Num() == 2)
				{
					TypeToken = Parts[0];
					NameToken = Parts[1];
				}
				else
				{
					Qualifier = Parts[0];
					TypeToken = Parts[1];
					NameToken = Parts[2];
				}

				Qualifier = Qualifier.TrimStartAndEnd();
				Qualifier.ToLowerInline();
				TypeToken = NormalizeShaderTypeToken(TypeToken);
				NameToken = NameToken.TrimStartAndEnd();

				if (!Qualifier.Equals(TEXT("in")) && !Qualifier.Equals(TEXT("out")))
				{
					OutError = FText::Format(LOCTEXT("FunctionSParameterSUsesUnsupported", "Function '{0}' parameter '{1}' uses unsupported qualifier '{2}'. Supported qualifiers are in and out."),
					FText::FromString(FunctionName),
					FText::FromString(Parameter),
					FText::FromString(Qualifier));
					return false;
				}

				if (TypeToken.IsEmpty() || NameToken.IsEmpty())
				{
					OutError = FText::Format(LOCTEXT("FunctionSHasAnInvalidParameter2", "Function '{0}' has an invalid parameter declaration '{1}'."),
					FText::FromString(FunctionName),
					FText::FromString(Parameter));
					return false;
				}

				if (NameToken.Equals(TEXT("__return"), ESearchCase::IgnoreCase))
				{
					OutError = FText::Format(LOCTEXT("FunctionSParameterNameReturnIs", "Function '{0}' parameter name '__return' is reserved for return-type lowering."),
					FText::FromString(FunctionName));
					return false;
				}

				FTextShaderFunctionParameter ParsedParameter;
				ParsedParameter.Type = TypeToken;
				ParsedParameter.Name = NameToken;
				if (Qualifier.Equals(TEXT("out")))
				{
					OutFunction.Results.Add(ParsedParameter);
				}
				else
				{
					OutFunction.Inputs.Add(ParsedParameter);
				}
			}

			if (bHasReturnType && OutFunction.Results.Num() > 1)
			{
				OutError = FText::Format(LOCTEXT("FunctionSHasAReturnType", "Function '{0}' has a return type and cannot also declare out parameters. Use out parameters without a return type for multiple outputs."),
					FText::FromString(FunctionName));
				return false;
			}

			if (OutFunction.Results.IsEmpty())
			{
				OutError = FText::Format(LOCTEXT("FunctionSMustDeclareAtLeast", "Function '{0}' must declare at least one out parameter."),
					FText::FromString(FunctionName));
				return false;
			}

			return true;
		}

		static bool ParseModernFunctionDeclaration(
			FScanner& Scanner,
			const FString& NamespaceName,
			FTextShaderDefinition& OutDefinition,
			const bool bGraphFunction,
			FText& OutError)
		{
			FTextShaderFunctionDefinition Function;

			FString FunctionName;
			if (!Scanner.ParseIdentifier(FunctionName, OutError))
			{
				OutError = bGraphFunction
					? LOCTEXT("GraphFunctionMissingName", "GraphFunction declaration is missing a valid function name.")
					: LOCTEXT("FunctionMissingName", "Function declaration is missing a valid function name.");
				return false;
			}

			if (!bGraphFunction
				&& (FunctionName.Equals(TEXT("SelfContained"), ESearchCase::IgnoreCase)
				|| FunctionName.Equals(TEXT("Inline"), ESearchCase::IgnoreCase))
				)
			{
				Function.bSelfContained = true;
				if (!Scanner.ParseIdentifier(FunctionName, OutError))
				{
					OutError = LOCTEXT("FunctionMissingNameAfterSelfContained", "Function declaration is missing a valid function name after SelfContained.");
					return false;
				}
			}

			// Optional leading return type: `Function float Foo(...)` declares Foo returning float.
			// Disambiguate by lookahead -- if the identifier just read is followed by '(', it is the
			// function name (legacy form); otherwise it is the return type and the next identifier names
			// the function.
			bool bHasReturnType = false;
			FString ReturnTypeToken;
			Scanner.SkipIgnored();
			if (Scanner.Peek() != TCHAR('('))
			{
				ReturnTypeToken = FunctionName;
				bHasReturnType = true;
				if (!Scanner.ParseIdentifier(FunctionName, OutError))
				{
					OutError = FText::Format(LOCTEXT("SDeclarationIsMissingAFunction", "{0} declaration is missing a function name after the return type '{1}'."),
					FText::FromString(bGraphFunction ? TEXT("GraphFunction") : TEXT("Function")),
					FText::FromString(ReturnTypeToken));
					return false;
				}
			}

			const FString QualifiedFunctionName = NamespaceName.IsEmpty()
				? FunctionName
				: NamespaceName + TEXT("::") + FunctionName;

			FString ParameterBlock;
			if (!ExtractBalancedDelimited(Scanner, TCHAR('('), TCHAR(')'), ParameterBlock, OutError))
			{
				OutError = FText::Format(LOCTEXT("SSIsMissingAValid", "{0} '{1}' is missing a valid parameter list. {2}"),
					FText::FromString(bGraphFunction ? TEXT("GraphFunction") : TEXT("Function")),
					FText::FromString(QualifiedFunctionName),
					OutError);
				return false;
			}

			FString FunctionBody;
			if (!ExtractBalancedDelimited(Scanner, TCHAR('{'), TCHAR('}'), FunctionBody, OutError))
			{
				OutError = FText::Format(LOCTEXT("SSIsMissingAValid2", "{0} '{1}' is missing a valid body block. {2}"),
					FText::FromString(bGraphFunction ? TEXT("GraphFunction") : TEXT("Function")),
					FText::FromString(QualifiedFunctionName),
					OutError);
				return false;
			}

			Function.Name = QualifiedFunctionName;
			if (!ParseModernFunctionSignature(QualifiedFunctionName, ParameterBlock, bHasReturnType, ReturnTypeToken, Function, OutError))
			{
				return false;
			}

			Function.HLSL = NormalizeShaderLanguageText(FunctionBody.TrimStartAndEnd());
			if (bHasReturnType)
			{
				FString LoweredBody;
				if (!LowerReturnToOutAssign(Function.HLSL, LoweredBody, OutError))
				{
					return false;
				}
				Function.HLSL = LoweredBody;
			}
			if (bGraphFunction)
			{
				OutDefinition.GraphFunctions.Add(Function);
			}
			else
			{
				OutDefinition.Functions.Add(Function);
			}
			return true;
		}

		static bool ParseNamespaceBlock(
			FScanner& Scanner,
			FTextShaderDefinition& OutDefinition,
			FText& OutError)
		{
			TMap<FString, FString> Attributes;
			if (!Scanner.ParseAttributes(Attributes, OutError))
			{
				return false;
			}

			FString NamespaceName;
			if (const FString* Name = Attributes.Find(TEXT("Name")))
			{
				NamespaceName = *Name;
			}
			else
			{
				OutError = LOCTEXT("NamespaceNameRequired", "Namespace(Name=\"...\") is required.");
				return false;
			}

			NamespaceName.TrimStartAndEndInline();
			if (NamespaceName.IsEmpty())
			{
				OutError = LOCTEXT("NamespaceNameEmpty", "Namespace name cannot be empty.");
				return false;
			}

			for (int32 Index = 0; Index < NamespaceName.Len(); ++Index)
			{
				const TCHAR Char = NamespaceName[Index];
				if ((Index == 0 && !IsIdentifierStart(Char)) || (Index > 0 && !IsIdentifierPart(Char)))
				{
					OutError = FText::Format(LOCTEXT("NamespaceNameSIsNotA", "Namespace name '{0}' is not a valid identifier."),
					FText::FromString(NamespaceName));
					return false;
				}
			}

			FString BodyContent;
			if (!Scanner.ExtractBalancedBlock(BodyContent, OutError))
			{
				return false;
			}

			FScanner BodyScanner(BodyContent);
			while (true)
			{
				BodyScanner.SkipIgnored();
				if (BodyScanner.IsAtEnd())
				{
					return true;
				}

				if (BodyScanner.TryConsumeKeyword(TEXT("Function")))
				{
					if (!ParseModernFunctionDeclaration(BodyScanner, NamespaceName, OutDefinition, false, OutError))
					{
						return false;
					}
					continue;
				}

				if (BodyScanner.TryConsumeKeyword(TEXT("GraphFunction")))
				{
					if (!ParseModernFunctionDeclaration(BodyScanner, NamespaceName, OutDefinition, true, OutError))
					{
						return false;
					}
					continue;
				}

				OutError = FText::Format(LOCTEXT("NamespaceSMayOnlyContainFunction", "Namespace '{0}' may only contain Function or GraphFunction blocks."),
					FText::FromString(NamespaceName));
				return false;
			}
		}
	}

	bool FTextShaderParser::Parse(const FString& SourceText, FTextShaderDefinition& OutDefinition, FText& OutError)
	{
		OutDefinition = FTextShaderDefinition();
		FText ErrorText;
		auto Fail = [&OutError](const FText& Error) -> bool
		{
			OutError = Error;
			return false;
		};
		auto PropagateFail = [&OutError, &ErrorText]() -> bool
		{
			OutError = ErrorText;
			return false;
		};

		Private::FScanner Scanner(SourceText);
		bool bFoundShader = false;

		while (true)
		{
			Scanner.SkipIgnored();
			if (Scanner.IsAtEnd())
			{
				break;
			}

			if (Scanner.TryConsumeKeyword(TEXT("Shader")))
			{
				if (bFoundShader)
				{
					return Fail(LOCTEXT("OnlyOneTopLevelShaderBlock", "Only one top-level Shader block is currently supported."));
				}

				TMap<FString, FString> Attributes;
				if (!Scanner.ParseAttributes(Attributes, ErrorText))
				{
					return PropagateFail();
				}

				if (const FString* Name = Attributes.Find(TEXT("Name")))
				{
					OutDefinition.Name = *Name;
				}
				else
				{
				return Fail(LOCTEXT("ShaderNameRequired", "Shader(Name=\"...\") is required."));
				}
				if (const FString* Root = Attributes.Find(TEXT("Root")))
				{
					OutDefinition.Root = *Root;
				}

				FString BodyContent;
				int32 BodyContentStartIndex = INDEX_NONE;
				if (!Scanner.ExtractBalancedBlock(BodyContent, BodyContentStartIndex, ErrorText))
				{
					return PropagateFail();
				}

				if (!Private::ParseShaderBody(BodyContent, BodyContentStartIndex, OutDefinition, ErrorText))
				{
					return PropagateFail();
				}

				bFoundShader = true;
			}
			else if (Scanner.TryConsumeKeyword(TEXT("Function")))
			{
				if (!Private::ParseModernFunctionDeclaration(Scanner, FString(), OutDefinition, false, ErrorText))
				{
					return PropagateFail();
				}
			}
			else if (Scanner.TryConsumeKeyword(TEXT("GraphFunction")))
			{
				if (!Private::ParseModernFunctionDeclaration(Scanner, FString(), OutDefinition, true, ErrorText))
				{
					return PropagateFail();
				}
			}
			else if (Scanner.TryConsumeKeyword(TEXT("Namespace")))
			{
				if (!Private::ParseNamespaceBlock(Scanner, OutDefinition, ErrorText))
				{
					return PropagateFail();
				}
			}
			else if (Scanner.TryConsumeKeyword(TEXT("VirtualFunction")))
			{
				TMap<FString, FString> Attributes;
				if (!Scanner.ParseAttributes(Attributes, ErrorText))
				{
					return PropagateFail();
				}

				FTextShaderVirtualFunctionDefinition Function;
				if (const FString* Name = Attributes.Find(TEXT("Name")))
				{
					Function.Name = *Name;
				}
				else
				{
					return Fail(LOCTEXT("VirtualFunctionNameRequired", "VirtualFunction(Name=\"...\") is required."));
				}
				Function.Name.TrimStartAndEndInline();
				if (Function.Name.IsEmpty())
				{
					return Fail(LOCTEXT("VirtualFunctionNameCannotBeEmpty", "VirtualFunction name cannot be empty."));
				}
				if (const FString* Asset = Attributes.Find(TEXT("Asset")))
				{
					Function.Asset = *Asset;
				}

				FString BodyContent;
				if (!Scanner.ExtractBalancedBlock(BodyContent, ErrorText))
				{
					return PropagateFail();
				}

				if (!Private::ParseVirtualFunctionBody(BodyContent, Function, ErrorText))
				{
					return PropagateFail();
				}

				if (Function.Asset.TrimStartAndEnd().IsEmpty())
				{
					if (const FString* Asset = Function.Options.Find(NormalizeSettingKey(TEXT("Asset"))))
					{
						Function.Asset = *Asset;
					}
				}
				Function.Asset.TrimStartAndEndInline();
				if (Function.Asset.IsEmpty())
				{
					return Fail(FText::Format(
						LOCTEXT("VirtualFunctionMustProvideOptionsAsset", "VirtualFunction '{0}' must provide Options = {{ Asset = Path(...); }}."),
						FText::FromString(Function.Name)));
				}
				if (Function.Outputs.IsEmpty())
				{
					return Fail(FText::Format(
						LOCTEXT("VirtualFunctionMustDeclareAtLeastOneOutput", "VirtualFunction '{0}' must declare at least one output."),
						FText::FromString(Function.Name)));
				}

				OutDefinition.VirtualFunctions.Add(Function);
			}
			else
			{
				FString MaterialFunctionBlockName;
				ETextShaderMaterialFunctionKind MaterialFunctionKind = ETextShaderMaterialFunctionKind::ShaderFunction;
				if (Scanner.TryConsumeKeyword(TEXT("ShaderFunction")))
				{
					MaterialFunctionBlockName = TEXT("ShaderFunction");
				}
				else if (Scanner.TryConsumeKeyword(TEXT("ShaderLayerBlend")))
				{
					MaterialFunctionBlockName = TEXT("ShaderLayerBlend");
					MaterialFunctionKind = ETextShaderMaterialFunctionKind::MaterialLayerBlend;
				}
				else if (Scanner.TryConsumeKeyword(TEXT("ShaderLayer")))
				{
					MaterialFunctionBlockName = TEXT("ShaderLayer");
					MaterialFunctionKind = ETextShaderMaterialFunctionKind::MaterialLayer;
				}
				else if (Scanner.TryConsumeKeyword(TEXT("MaterialLayerBlend")))
				{
					MaterialFunctionBlockName = TEXT("MaterialLayerBlend");
					MaterialFunctionKind = ETextShaderMaterialFunctionKind::MaterialLayerBlend;
					OutDefinition.Warnings.Add(TEXT("MaterialLayerBlend is deprecated; use ShaderLayerBlend instead."));
				}
				else if (Scanner.TryConsumeKeyword(TEXT("MaterialLayer")))
				{
					MaterialFunctionBlockName = TEXT("MaterialLayer");
					MaterialFunctionKind = ETextShaderMaterialFunctionKind::MaterialLayer;
					OutDefinition.Warnings.Add(TEXT("MaterialLayer is deprecated; use ShaderLayer instead."));
				}

				if (MaterialFunctionBlockName.IsEmpty())
				{
				return Fail(FText::Format(
					LOCTEXT("UnexpectedTokenNearIndex", "Unexpected token near index {0}."),
					FText::AsNumber(Scanner.Index)));
				}

				TMap<FString, FString> Attributes;
				if (!Scanner.ParseAttributes(Attributes, ErrorText))
				{
					return PropagateFail();
				}

				FTextShaderMaterialFunctionDefinition Function;
				Function.Kind = MaterialFunctionKind;
				if (const FString* Name = Attributes.Find(TEXT("Name")))
				{
					Function.Name = *Name;
				}
				else
				{
					return Fail(FText::Format(
						LOCTEXT("MaterialFunctionNameRequired", "{0}(Name=\"...\") is required."),
						FText::FromString(MaterialFunctionBlockName)));
				}
				if (const FString* Root = Attributes.Find(TEXT("Root")))
				{
					Function.Root = *Root;
				}

				FString BodyContent;
				int32 BodyContentStartIndex = INDEX_NONE;
				if (!Scanner.ExtractBalancedBlock(BodyContent, BodyContentStartIndex, ErrorText))
				{
					return PropagateFail();
				}

				if (!Private::ParseMaterialFunctionBody(BodyContent, BodyContentStartIndex, Function, ErrorText))
				{
					return PropagateFail();
				}

				OutDefinition.MaterialFunctions.Add(Function);
			}
		}

		if (!bFoundShader && OutDefinition.Functions.IsEmpty() && OutDefinition.GraphFunctions.IsEmpty() && OutDefinition.MaterialFunctions.IsEmpty() && OutDefinition.VirtualFunctions.IsEmpty())
		{
			return Fail(LOCTEXT("ATopLevelShaderFunctionGraphFunction", "A top-level Shader, Function, GraphFunction, Namespace, ShaderFunction, ShaderLayer, ShaderLayerBlend, or VirtualFunction block was not found."));
		}

		bool bHasInitializedOutput = false;
		for (const FTextShaderVariableDeclaration& OutputDeclaration : OutDefinition.OutputDeclarations)
		{
			if (OutputDeclaration.bHasDefaultValue)
			{
				bHasInitializedOutput = true;
				break;
			}
		}

		if (bFoundShader && OutDefinition.Code.IsEmpty() && !bHasInitializedOutput)
		{
			return Fail(LOCTEXT("ShaderMustProvideAGraphBlock", "Shader must provide a Graph block."));
		}

		if (bFoundShader && OutDefinition.Outputs.IsEmpty())
		{
			OutDefinition.Warnings.Add(TEXT("No Outputs block was provided. Generation requires explicit material property bindings."));
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
