#include "DreamShaderParserInternal.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/PackageName.h"

namespace UE::DreamShader::Private
{
	FScanner::FScanner(const FString& InSource)
		: Source(InSource)
	{
	}

	bool FScanner::IsAtEnd() const
	{
		return Index >= Source.Len();
	}

	TCHAR FScanner::Peek(const int32 Offset) const
	{
		const int32 PeekIndex = Index + Offset;
		return Source.IsValidIndex(PeekIndex) ? Source[PeekIndex] : TCHAR('\0');
	}

	void FScanner::SkipIgnored()
	{
		bool bAdvanced = true;
		while (bAdvanced && !IsAtEnd())
		{
			bAdvanced = false;

			while (!IsAtEnd() && FChar::IsWhitespace(Peek()))
			{
				++Index;
				bAdvanced = true;
			}

			if (Peek() == TCHAR('/') && Peek(1) == TCHAR('/'))
			{
				Index += 2;
				while (!IsAtEnd() && Peek() != TCHAR('\n'))
				{
					++Index;
				}
				bAdvanced = true;
			}
			else if (Peek() == TCHAR('/') && Peek(1) == TCHAR('*'))
			{
				Index += 2;
				while (!IsAtEnd() && !(Peek() == TCHAR('*') && Peek(1) == TCHAR('/')))
				{
					++Index;
				}
				if (!IsAtEnd())
				{
					Index += 2;
				}
				bAdvanced = true;
			}
		}
	}

	bool FScanner::TryConsume(const TCHAR Expected)
	{
		SkipIgnored();
		if (Peek() == Expected)
		{
			++Index;
			return true;
		}
		return false;
	}

	bool FScanner::Expect(const TCHAR Expected, FString& OutError)
	{
		SkipIgnored();
		if (!TryConsume(Expected))
		{
			OutError = FString::Printf(TEXT("Expected '%c' near index %d."), Expected, Index);
			return false;
		}
		return true;
	}

	bool FScanner::TryConsumeKeyword(const TCHAR* Keyword)
	{
		SkipIgnored();

		const int32 KeywordLength = FCString::Strlen(Keyword);
		if (Source.Mid(Index, KeywordLength).Equals(Keyword, ESearchCase::CaseSensitive))
		{
			const TCHAR Boundary = Peek(KeywordLength);
			if (!(FChar::IsAlnum(Boundary) || Boundary == TCHAR('_')))
			{
				Index += KeywordLength;
				return true;
			}
		}

		return false;
	}

	bool FScanner::ParseIdentifier(FString& OutIdentifier, FString& OutError)
	{
		SkipIgnored();
		if (!(FChar::IsAlpha(Peek()) || Peek() == TCHAR('_')))
		{
			OutError = FString::Printf(TEXT("Expected identifier near index %d."), Index);
			return false;
		}

		const int32 Start = Index;
		++Index;
		while (FChar::IsAlnum(Peek()) || Peek() == TCHAR('_'))
		{
			++Index;
		}

		OutIdentifier = Source.Mid(Start, Index - Start);
		return true;
	}

	bool FScanner::ParseSimpleValue(FString& OutValue, FString& OutError)
	{
		SkipIgnored();

		if (Peek() == TCHAR('"'))
		{
			const int32 Start = ++Index;
			while (!IsAtEnd() && Peek() != TCHAR('"'))
			{
				if (Peek() == TCHAR('\\') && !IsAtEnd())
				{
					++Index;
				}
				++Index;
			}

			if (IsAtEnd())
			{
				OutError = TEXT("Unterminated string literal.");
				return false;
			}

			OutValue = Source.Mid(Start, Index - Start);
			++Index;
			return true;
		}

		const int32 Start = Index;
		while (!IsAtEnd())
		{
			const TCHAR Char = Peek();
			if (Char == TCHAR(',') || Char == TCHAR(')'))
			{
				break;
			}
			++Index;
		}

		OutValue = Source.Mid(Start, Index - Start).TrimStartAndEnd();
		if (OutValue.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Expected value near index %d."), Index);
			return false;
		}

		return true;
	}

	bool FScanner::ParseAttributes(TMap<FString, FString>& OutAttributes, FString& OutError)
	{
		if (!Expect(TCHAR('('), OutError))
		{
			return false;
		}

		while (true)
		{
			SkipIgnored();
			if (TryConsume(TCHAR(')')))
			{
				return true;
			}

			FString Key;
			if (!ParseIdentifier(Key, OutError))
			{
				return false;
			}

			if (!Expect(TCHAR('='), OutError))
			{
				return false;
			}

			FString Value;
			if (!ParseSimpleValue(Value, OutError))
			{
				return false;
			}

			OutAttributes.Add(Key, Value);

			SkipIgnored();
			if (TryConsume(TCHAR(',')))
			{
				continue;
			}

			if (TryConsume(TCHAR(')')))
			{
				return true;
			}

			OutError = FString::Printf(TEXT("Expected ',' or ')' near index %d."), Index);
			return false;
		}
	}

	bool FScanner::ExtractBalancedBlock(FString& OutBlock, FString& OutError)
	{
		SkipIgnored();
		if (Peek() != TCHAR('{'))
		{
			OutError = FString::Printf(TEXT("Expected '{' near index %d."), Index);
			return false;
		}

		++Index;
		const int32 BlockStart = Index;
		int32 Depth = 1;

		bool bInString = false;
		bool bInLineComment = false;
		bool bInBlockComment = false;

		while (!IsAtEnd())
		{
			const TCHAR Char = Source[Index++];

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
				if (Char == TCHAR('*') && Peek() == TCHAR('/'))
				{
					++Index;
					bInBlockComment = false;
				}
				continue;
			}

			if (bInString)
			{
				if (Char == TCHAR('\\') && !IsAtEnd())
				{
					++Index;
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

			if (Char == TCHAR('/') && Peek() == TCHAR('/'))
			{
				++Index;
				bInLineComment = true;
				continue;
			}

			if (Char == TCHAR('/') && Peek() == TCHAR('*'))
			{
				++Index;
				bInBlockComment = true;
				continue;
			}

			if (Char == TCHAR('{'))
			{
				++Depth;
			}
			else if (Char == TCHAR('}'))
			{
				--Depth;
				if (Depth == 0)
				{
					OutBlock = Source.Mid(BlockStart, (Index - BlockStart) - 1);
					return true;
				}
			}
		}

		OutError = TEXT("Unterminated block.");
		return false;
	}

	FString RemoveComments(const FString& Input)
	{
		FString Output;
		Output.Reserve(Input.Len());

		bool bInString = false;
		bool bInLineComment = false;
		bool bInBlockComment = false;

		for (int32 Index = 0; Index < Input.Len(); ++Index)
		{
			const TCHAR Char = Input[Index];
			const TCHAR Next = Input.IsValidIndex(Index + 1) ? Input[Index + 1] : TCHAR('\0');

			if (bInLineComment)
			{
				if (Char == TCHAR('\n'))
				{
					bInLineComment = false;
					Output.AppendChar(Char);
				}
				continue;
			}

			if (bInBlockComment)
			{
				if (Char == TCHAR('*') && Next == TCHAR('/'))
				{
					bInBlockComment = false;
					++Index;
				}
				continue;
			}

			if (bInString)
			{
				Output.AppendChar(Char);
				if (Char == TCHAR('\\') && Input.IsValidIndex(Index + 1))
				{
					Output.AppendChar(Input[++Index]);
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
				Output.AppendChar(Char);
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('/'))
			{
				bInLineComment = true;
				++Index;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('*'))
			{
				bInBlockComment = true;
				++Index;
				continue;
			}

			Output.AppendChar(Char);
		}

		return Output;
	}

	TArray<FString> SplitStatements(const FString& BlockContent)
	{
		TArray<FString> Statements;
		FString Current;
		int32 ParenthesisDepth = 0;
		int32 BracketDepth = 0;
		bool bInString = false;

		for (int32 Index = 0; Index < BlockContent.Len(); ++Index)
		{
			const TCHAR Char = BlockContent[Index];

			if (bInString)
			{
				Current.AppendChar(Char);
				if (Char == TCHAR('\\') && BlockContent.IsValidIndex(Index + 1))
				{
					Current.AppendChar(BlockContent[++Index]);
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
				Current.AppendChar(Char);
				continue;
			}

			if (Char == TCHAR('('))
			{
				++ParenthesisDepth;
				Current.AppendChar(Char);
				continue;
			}

			if (Char == TCHAR(')'))
			{
				ParenthesisDepth = FMath::Max(0, ParenthesisDepth - 1);
				Current.AppendChar(Char);
				continue;
			}

			if (Char == TCHAR('['))
			{
				++BracketDepth;
				Current.AppendChar(Char);
				continue;
			}

			if (Char == TCHAR(']'))
			{
				BracketDepth = FMath::Max(0, BracketDepth - 1);
				Current.AppendChar(Char);
				continue;
			}

			if (Char == TCHAR(';') && ParenthesisDepth == 0 && BracketDepth == 0)
			{
				Current.TrimStartAndEndInline();
				if (!Current.IsEmpty())
				{
					Statements.Add(Current);
				}
				Current.Reset();
				continue;
			}

			Current.AppendChar(Char);
		}

		Current.TrimStartAndEndInline();
		if (!Current.IsEmpty())
		{
			Statements.Add(Current);
		}

		return Statements;
	}

	TArray<FString> SplitTopLevelDelimited(const FString& Input, const TCHAR Delimiter)
	{
		TArray<FString> Parts;
		FString Current;
		int32 ParenthesisDepth = 0;
		int32 BracketDepth = 0;
		bool bInString = false;

		for (int32 Index = 0; Index < Input.Len(); ++Index)
		{
			const TCHAR Char = Input[Index];

			if (bInString)
			{
				Current.AppendChar(Char);
				if (Char == TCHAR('\\') && Input.IsValidIndex(Index + 1))
				{
					Current.AppendChar(Input[++Index]);
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
				Current.AppendChar(Char);
				continue;
			}

			if (Char == TCHAR('('))
			{
				++ParenthesisDepth;
				Current.AppendChar(Char);
				continue;
			}

			if (Char == TCHAR(')'))
			{
				ParenthesisDepth = FMath::Max(0, ParenthesisDepth - 1);
				Current.AppendChar(Char);
				continue;
			}

			if (Char == TCHAR('['))
			{
				++BracketDepth;
				Current.AppendChar(Char);
				continue;
			}

			if (Char == TCHAR(']'))
			{
				BracketDepth = FMath::Max(0, BracketDepth - 1);
				Current.AppendChar(Char);
				continue;
			}

			if (Char == Delimiter && ParenthesisDepth == 0 && BracketDepth == 0)
			{
				Current.TrimStartAndEndInline();
				if (!Current.IsEmpty())
				{
					Parts.Add(Current);
				}
				Current.Reset();
				continue;
			}

			Current.AppendChar(Char);
		}

		Current.TrimStartAndEndInline();
		if (!Current.IsEmpty())
		{
			Parts.Add(Current);
		}

		return Parts;
	}

	bool SplitTopLevelAssignment(const FString& InText, FString& OutLeft, FString& OutRight)
	{
		int32 ParenthesisDepth = 0;
		int32 BracketDepth = 0;
		bool bInString = false;

		for (int32 Index = 0; Index < InText.Len(); ++Index)
		{
			const TCHAR Char = InText[Index];

			if (bInString)
			{
				if (Char == TCHAR('\\') && InText.IsValidIndex(Index + 1))
				{
					++Index;
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

			if (Char == TCHAR('('))
			{
				++ParenthesisDepth;
				continue;
			}

			if (Char == TCHAR(')'))
			{
				ParenthesisDepth = FMath::Max(0, ParenthesisDepth - 1);
				continue;
			}

			if (Char == TCHAR('['))
			{
				++BracketDepth;
				continue;
			}

			if (Char == TCHAR(']'))
			{
				BracketDepth = FMath::Max(0, BracketDepth - 1);
				continue;
			}

			if (Char == TCHAR('=') && ParenthesisDepth == 0 && BracketDepth == 0)
			{
				OutLeft = InText.Left(Index).TrimStartAndEnd();
				OutRight = InText.Mid(Index + 1).TrimStartAndEnd();
				return true;
			}
		}

		return false;
	}

	bool SplitDeclarationTypeAndName(const FString& InText, FString& OutTypeToken, FString& OutNameToken)
	{
		const FString Trimmed = InText.TrimStartAndEnd();
		int32 ParenthesisDepth = 0;
		int32 BracketDepth = 0;
		bool bInString = false;
		int32 LastWhitespaceIndex = INDEX_NONE;

		for (int32 Index = 0; Index < Trimmed.Len(); ++Index)
		{
			const TCHAR Char = Trimmed[Index];

			if (bInString)
			{
				if (Char == TCHAR('\\') && Trimmed.IsValidIndex(Index + 1))
				{
					++Index;
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

			if (Char == TCHAR('('))
			{
				++ParenthesisDepth;
				continue;
			}

			if (Char == TCHAR(')'))
			{
				ParenthesisDepth = FMath::Max(0, ParenthesisDepth - 1);
				continue;
			}

			if (Char == TCHAR('['))
			{
				++BracketDepth;
				continue;
			}

			if (Char == TCHAR(']'))
			{
				BracketDepth = FMath::Max(0, BracketDepth - 1);
				continue;
			}

			if (ParenthesisDepth == 0 && BracketDepth == 0 && FChar::IsWhitespace(Char))
			{
				LastWhitespaceIndex = Index;
			}
		}

		if (LastWhitespaceIndex == INDEX_NONE)
		{
			return false;
		}

		OutTypeToken = Trimmed.Left(LastWhitespaceIndex).TrimStartAndEnd();
		OutNameToken = Trimmed.Mid(LastWhitespaceIndex + 1).TrimStartAndEnd();
		return !OutTypeToken.IsEmpty() && !OutNameToken.IsEmpty();
	}

	FString Unquote(const FString& InValue)
	{
		FString Result = InValue.TrimStartAndEnd();
		if (Result.Len() >= 2 && Result.StartsWith(TEXT("\"")) && Result.EndsWith(TEXT("\"")))
		{
			Result = Result.Mid(1, Result.Len() - 2);
		}
		return Result;
	}

	bool ParseScalarLiteral(const FString& InText, double& OutValue)
	{
		FString Candidate = InText.TrimStartAndEnd();
		if (LexTryParseString(OutValue, *Candidate))
		{
			return true;
		}

		if (Candidate.Equals(TEXT("true"), ESearchCase::IgnoreCase))
		{
			OutValue = 1.0;
			return true;
		}

		if (Candidate.Equals(TEXT("false"), ESearchCase::IgnoreCase))
		{
			OutValue = 0.0;
			return true;
		}

		return false;
	}

	bool ParseIntegerLiteral(const FString& InText, int32& OutValue)
	{
		FString Candidate = InText.TrimStartAndEnd();
		return LexTryParseString(OutValue, *Candidate);
	}

	bool ParseVectorLiteral(const FString& InText, FLinearColor& OutColor)
	{
		FString Candidate = InText.TrimStartAndEnd();
		const int32 OpenParenIndex = Candidate.Find(TEXT("("));
		const int32 CloseParenIndex = Candidate.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);

		if (OpenParenIndex == INDEX_NONE || CloseParenIndex == INDEX_NONE || CloseParenIndex <= OpenParenIndex)
		{
			return false;
		}

		const FString ValueBlock = Candidate.Mid(OpenParenIndex + 1, CloseParenIndex - OpenParenIndex - 1);
		TArray<FString> Parts;
		ValueBlock.ParseIntoArray(Parts, TEXT(","), true);
		if (Parts.IsEmpty())
		{
			return false;
		}

		double Parsed[4] = { 0.0, 0.0, 0.0, 1.0 };
		for (int32 Index = 0; Index < Parts.Num() && Index < 4; ++Index)
		{
			const FString Part = Parts[Index].TrimStartAndEnd();
			if (!LexTryParseString(Parsed[Index], *Part))
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

		OutColor = FLinearColor(
			static_cast<float>(Parsed[0]),
			static_cast<float>(Parsed[1]),
			static_cast<float>(Parsed[2]),
			static_cast<float>(Parsed[3]));

		return true;
	}

	bool ParseBooleanLiteral(const FString& InText, bool& OutValue)
	{
		const FString Candidate = InText.TrimStartAndEnd();
		if (Candidate.Equals(TEXT("true"), ESearchCase::IgnoreCase))
		{
			OutValue = true;
			return true;
		}
		if (Candidate.Equals(TEXT("false"), ESearchCase::IgnoreCase))
		{
			OutValue = false;
			return true;
		}
		return false;
	}

	bool IsValidPluginPathSegment(const FString& Segment)
	{
		if (Segment.IsEmpty())
		{
			return false;
		}

		for (const TCHAR Character : Segment)
		{
			if (!FChar::IsAlnum(Character) && Character != TCHAR('_'))
			{
				return false;
			}
		}

		return true;
	}

	bool ResolveTexturePluginRootPackagePath(const FString& Root, const FString& PluginName, FString& OutPackagePath, FString& OutError)
	{
		if (!IsValidPluginPathSegment(PluginName))
		{
			OutError = FString::Printf(TEXT("Texture Path root '%s' has an invalid plugin name."), *Root);
			return false;
		}

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
		if (!Plugin.IsValid())
		{
			OutError = FString::Printf(TEXT("Texture Path root '%s' references plugin '%s', but no enabled plugin with that name was found."), *Root, *PluginName);
			return false;
		}
		if (!Plugin->IsEnabled())
		{
			OutError = FString::Printf(TEXT("Texture Path root '%s' references plugin '%s', but the plugin is not enabled."), *Root, *PluginName);
			return false;
		}
		if (!Plugin->CanContainContent())
		{
			OutError = FString::Printf(TEXT("Texture Path root '%s' references plugin '%s', but the plugin cannot contain content."), *Root, *PluginName);
			return false;
		}

		FString MountedPath = Plugin->GetMountedAssetPath();
		MountedPath.TrimStartAndEndInline();
		MountedPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (MountedPath.EndsWith(TEXT("/")))
		{
			MountedPath.LeftChopInline(1, EAllowShrinking::No);
		}
		if (!MountedPath.StartsWith(TEXT("/")))
		{
			MountedPath = TEXT("/") + MountedPath;
		}
		if (MountedPath.IsEmpty() || MountedPath == TEXT("/"))
		{
			MountedPath = TEXT("/") + PluginName;
		}

		OutPackagePath = MountedPath;
		return true;
	}

	bool ResolveTexturePathRootPackagePath(const FString& Root, FString& OutPackagePath, FString& OutError)
	{
		FString Normalized = Unquote(Root.TrimStartAndEnd());
		Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Normalized.StartsWith(TEXT("/")))
		{
			Normalized.RightChopInline(1, EAllowShrinking::No);
		}
		while (Normalized.EndsWith(TEXT("/")))
		{
			Normalized.LeftChopInline(1, EAllowShrinking::No);
		}

		TArray<FString> Segments;
		Normalized.ParseIntoArray(Segments, TEXT("/"), true);
		if (Segments.IsEmpty())
		{
			OutError = TEXT("Relative texture Path(...) references require a root such as Game, Engine, or Plugin.PluginName.");
			return false;
		}

		const FString RootSegment = Segments[0].TrimStartAndEnd();
		int32 FirstFolderSegmentIndex = 1;
		if (RootSegment.Equals(TEXT("Game"), ESearchCase::IgnoreCase))
		{
			OutPackagePath = TEXT("/Game");
		}
		else if (RootSegment.Equals(TEXT("Engine"), ESearchCase::IgnoreCase))
		{
			OutPackagePath = TEXT("/Engine");
		}
		else if (RootSegment.StartsWith(TEXT("Plugin."), ESearchCase::IgnoreCase)
			|| RootSegment.StartsWith(TEXT("Plugins."), ESearchCase::IgnoreCase))
		{
			const int32 PluginPrefixLength = RootSegment.StartsWith(TEXT("Plugins."), ESearchCase::IgnoreCase) ? 8 : 7;
			const FString PluginName = RootSegment.Mid(PluginPrefixLength).TrimStartAndEnd();
			if (!ResolveTexturePluginRootPackagePath(Root, PluginName, OutPackagePath, OutError))
			{
				return false;
			}
		}
		else if ((RootSegment.Equals(TEXT("Plugin"), ESearchCase::IgnoreCase)
			|| RootSegment.Equals(TEXT("Plugins"), ESearchCase::IgnoreCase)) && Segments.IsValidIndex(1))
		{
			const FString PluginName = Segments[1].TrimStartAndEnd();
			if (!ResolveTexturePluginRootPackagePath(Root, PluginName, OutPackagePath, OutError))
			{
				return false;
			}
			FirstFolderSegmentIndex = 2;
		}
		else
		{
			OutError = FString::Printf(TEXT("Unsupported texture Path root '%s'. Use Game, Engine, or Plugin.PluginName."), *Root);
			return false;
		}

		for (int32 Index = FirstFolderSegmentIndex; Index < Segments.Num(); ++Index)
		{
			OutPackagePath += TEXT("/");
			OutPackagePath += Segments[Index].TrimStartAndEnd();
		}

		return true;
	}

	bool ParseTextureAssetReference(const FString& InText, FString& OutObjectPath, FString& OutError)
	{
		const FString Trimmed = InText.TrimStartAndEnd();
		FScanner Scanner(Trimmed);

		FString FunctionName;
		if (!Scanner.ParseIdentifier(FunctionName, OutError) || !FunctionName.Equals(TEXT("Path"), ESearchCase::IgnoreCase))
		{
			OutError = TEXT("Texture defaults must use Path(Game|Engine|Plugin.PluginName, \"/Folder/Asset\") or Path(\"/Game/Folder/Asset\").");
			return false;
		}

		if (!Scanner.Expect(TCHAR('('), OutError))
		{
			return false;
		}

		FString FirstArgument;
		if (!Scanner.ParseSimpleValue(FirstArgument, OutError))
		{
			return false;
		}

		FString RootName;
		FString AssetPath;
		Scanner.SkipIgnored();
		if (Scanner.TryConsume(TCHAR(',')))
		{
			RootName = FirstArgument.TrimStartAndEnd();
			if (!Scanner.ParseSimpleValue(AssetPath, OutError))
			{
				return false;
			}
		}
		else
		{
			AssetPath = FirstArgument;
		}

		if (!Scanner.Expect(TCHAR(')'), OutError))
		{
			return false;
		}

		Scanner.SkipIgnored();
		if (!Scanner.IsAtEnd())
		{
			OutError = TEXT("Unexpected trailing tokens after texture Path(...) reference.");
			return false;
		}

		AssetPath = Unquote(AssetPath.TrimStartAndEnd());
		AssetPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (AssetPath.IsEmpty())
		{
			OutError = TEXT("Texture Path(...) requires a non-empty asset path.");
			return false;
		}

		const bool bAssetPathIsAbsolute = AssetPath.StartsWith(TEXT("/"));
		if (!bAssetPathIsAbsolute)
		{
			AssetPath = TEXT("/") + AssetPath;
		}

		FString LongObjectPath;
		if (RootName.TrimStartAndEnd().IsEmpty() && bAssetPathIsAbsolute)
		{
			LongObjectPath = AssetPath;
		}
		else
		{
			FString PackageRoot;
			if (!ResolveTexturePathRootPackagePath(RootName, PackageRoot, OutError))
			{
				return false;
			}
			LongObjectPath = PackageRoot + AssetPath;
		}

		const int32 LastSlashIndex = LongObjectPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		const int32 LastDotIndex = LongObjectPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (LastSlashIndex == INDEX_NONE || LastSlashIndex >= LongObjectPath.Len() - 1)
		{
			OutError = FString::Printf(TEXT("Invalid texture asset path '%s'."), *LongObjectPath);
			return false;
		}

		if (LastDotIndex == INDEX_NONE || LastDotIndex < LastSlashIndex)
		{
			const FString AssetName = LongObjectPath.Mid(LastSlashIndex + 1);
			LongObjectPath += TEXT(".") + AssetName;
		}

		FText PathError;
		if (!FPackageName::IsValidObjectPath(LongObjectPath, &PathError))
		{
			OutError = PathError.ToString();
			return false;
		}

		OutObjectPath = LongObjectPath;
		return true;
	}
}
