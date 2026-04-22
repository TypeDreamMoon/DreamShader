#include "DreamShaderParser.h"

#include "DreamShaderParserInternal.h"

namespace UE::DreamShader
{
	namespace Private
	{
		static bool ExtractBalancedDelimited(
			FScanner& Scanner,
			const TCHAR OpenChar,
			const TCHAR CloseChar,
			FString& OutContent,
			FString& OutError)
		{
			Scanner.SkipIgnored();
			if (Scanner.Peek() != OpenChar)
			{
				OutError = FString::Printf(TEXT("Expected '%c' near index %d."), OpenChar, Scanner.Index);
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

			OutError = FString::Printf(TEXT("Unterminated '%c' block."), OpenChar);
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

		static bool ParseModernFunctionSignature(
			const FString& FunctionName,
			const FString& ParameterBlock,
			FTextShaderFunctionDefinition& OutFunction,
			FString& OutError)
		{
			OutFunction.Inputs.Reset();
			OutFunction.Results.Reset();

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
					OutError = FString::Printf(TEXT("Function '%s' has an invalid parameter declaration '%s'."), *FunctionName, *Parameter);
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
					OutError = FString::Printf(
						TEXT("Function '%s' parameter '%s' uses unsupported qualifier '%s'. Supported qualifiers are in and out."),
						*FunctionName,
						*Parameter,
						*Qualifier);
					return false;
				}

				if (TypeToken.IsEmpty() || NameToken.IsEmpty())
				{
					OutError = FString::Printf(TEXT("Function '%s' has an invalid parameter declaration '%s'."), *FunctionName, *Parameter);
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

			if (OutFunction.Results.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Function '%s' must declare at least one out parameter."), *FunctionName);
				return false;
			}

			return true;
		}

		static bool ParseModernFunctionDeclaration(
			FScanner& Scanner,
			const FString& NamespaceName,
			FTextShaderDefinition& OutDefinition,
			FString& OutError)
		{
			FTextShaderFunctionDefinition Function;

			FString FunctionName;
			if (!Scanner.ParseIdentifier(FunctionName, OutError))
			{
				OutError = TEXT("Function declaration is missing a valid function name.");
				return false;
			}

			if (FunctionName.Equals(TEXT("SelfContained"), ESearchCase::IgnoreCase)
				|| FunctionName.Equals(TEXT("Inline"), ESearchCase::IgnoreCase))
			{
				Function.bSelfContained = true;
				if (!Scanner.ParseIdentifier(FunctionName, OutError))
				{
					OutError = TEXT("Function declaration is missing a valid function name after SelfContained.");
					return false;
				}
			}

			const FString QualifiedFunctionName = NamespaceName.IsEmpty()
				? FunctionName
				: NamespaceName + TEXT("::") + FunctionName;

			FString ParameterBlock;
			if (!ExtractBalancedDelimited(Scanner, TCHAR('('), TCHAR(')'), ParameterBlock, OutError))
			{
				OutError = FString::Printf(TEXT("Function '%s' is missing a valid parameter list. %s"), *QualifiedFunctionName, *OutError);
				return false;
			}

			FString FunctionBody;
			if (!ExtractBalancedDelimited(Scanner, TCHAR('{'), TCHAR('}'), FunctionBody, OutError))
			{
				OutError = FString::Printf(TEXT("Function '%s' is missing a valid body block. %s"), *QualifiedFunctionName, *OutError);
				return false;
			}

			Function.Name = QualifiedFunctionName;
			if (!ParseModernFunctionSignature(QualifiedFunctionName, ParameterBlock, Function, OutError))
			{
				return false;
			}

			Function.HLSL = NormalizeShaderLanguageText(FunctionBody.TrimStartAndEnd());
			OutDefinition.Functions.Add(Function);
			return true;
		}

		static bool ParseNamespaceBlock(
			FScanner& Scanner,
			FTextShaderDefinition& OutDefinition,
			FString& OutError)
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
				OutError = TEXT("Namespace(Name=\"...\") is required.");
				return false;
			}

			NamespaceName.TrimStartAndEndInline();
			if (NamespaceName.IsEmpty())
			{
				OutError = TEXT("Namespace name cannot be empty.");
				return false;
			}

			for (int32 Index = 0; Index < NamespaceName.Len(); ++Index)
			{
				const TCHAR Char = NamespaceName[Index];
				if ((Index == 0 && !IsIdentifierStart(Char)) || (Index > 0 && !IsIdentifierPart(Char)))
				{
					OutError = FString::Printf(TEXT("Namespace name '%s' is not a valid identifier."), *NamespaceName);
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
					if (!ParseModernFunctionDeclaration(BodyScanner, NamespaceName, OutDefinition, OutError))
					{
						return false;
					}
					continue;
				}

				OutError = FString::Printf(TEXT("Namespace '%s' may only contain Function blocks."), *NamespaceName);
				return false;
			}
		}
	}

	bool FTextShaderParser::Parse(const FString& SourceText, FTextShaderDefinition& OutDefinition, FString& OutError)
	{
		OutDefinition = FTextShaderDefinition();
		OutError.Reset();

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
					OutError = TEXT("Only one top-level Shader block is currently supported.");
					return false;
				}

				TMap<FString, FString> Attributes;
				if (!Scanner.ParseAttributes(Attributes, OutError))
				{
					return false;
				}

				if (const FString* Name = Attributes.Find(TEXT("Name")))
				{
					OutDefinition.Name = *Name;
				}
				else
				{
					OutError = TEXT("Shader(Name=\"...\") is required.");
					return false;
				}

				FString BodyContent;
				if (!Scanner.ExtractBalancedBlock(BodyContent, OutError))
				{
					return false;
				}

				if (!Private::ParseShaderBody(BodyContent, OutDefinition, OutError))
				{
					return false;
				}

				bFoundShader = true;
			}
			else if (Scanner.TryConsumeKeyword(TEXT("Function")))
			{
				if (!Private::ParseModernFunctionDeclaration(Scanner, FString(), OutDefinition, OutError))
				{
					return false;
				}
			}
			else if (Scanner.TryConsumeKeyword(TEXT("Namespace")))
			{
				if (!Private::ParseNamespaceBlock(Scanner, OutDefinition, OutError))
				{
					return false;
				}
			}
			else if (Scanner.TryConsumeKeyword(TEXT("ShaderFunction")))
			{
				TMap<FString, FString> Attributes;
				if (!Scanner.ParseAttributes(Attributes, OutError))
				{
					return false;
				}

				FTextShaderMaterialFunctionDefinition Function;
				if (const FString* Name = Attributes.Find(TEXT("Name")))
				{
					Function.Name = *Name;
				}
				else
				{
					OutError = TEXT("ShaderFunction(Name=\"...\") is required.");
					return false;
				}

				FString BodyContent;
				if (!Scanner.ExtractBalancedBlock(BodyContent, OutError))
				{
					return false;
				}

				if (!Private::ParseMaterialFunctionBody(BodyContent, Function, OutError))
				{
					return false;
				}

				OutDefinition.MaterialFunctions.Add(Function);
			}
			else
			{
				OutError = FString::Printf(TEXT("Unexpected token near index %d."), Scanner.Index);
				return false;
			}
		}

		if (!bFoundShader && OutDefinition.Functions.IsEmpty() && OutDefinition.MaterialFunctions.IsEmpty())
		{
			OutError = TEXT("A top-level Shader, Function, Namespace, or ShaderFunction block was not found.");
			return false;
		}

		if (bFoundShader && OutDefinition.Code.IsEmpty())
		{
			OutError = TEXT("Shader must provide a Graph block.");
			return false;
		}

		if (bFoundShader && OutDefinition.Outputs.IsEmpty())
		{
			OutDefinition.Warnings.Add(TEXT("No Outputs block was provided. Generation requires explicit material property bindings."));
		}

		return true;
	}
}
