// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderPassShaderText.h.
//
// Diagnostics owned by this file: none. Every caller words its own failure from what these return, because only the
// caller knows whether a missing file is a pass's reference (bind), a snapshot that cannot be built (emit) or a watch
// that has nothing to watch (bridge).

#include "Pass/DreamShaderPassShaderText.h"

#include "DreamShaderModule.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "ShaderCore.h"

namespace UE::DreamShader::Editor::Compiler
{
	namespace DreamPassShaderTextDetail
	{
		bool IsIdentifierStart(const TCHAR Character)
		{
			return (Character >= TCHAR('A') && Character <= TCHAR('Z'))
				|| (Character >= TCHAR('a') && Character <= TCHAR('z'))
				|| Character == TCHAR('_');
		}

		bool IsIdentifierCharacter(const TCHAR Character)
		{
			return IsIdentifierStart(Character) || (Character >= TCHAR('0') && Character <= TCHAR('9'));
		}

		/** A real directory of a mapping, absolute and normalized, without a trailing slash. */
		FString NormalizeMappedDirectory(const FString& Directory)
		{
			FString Result = FPaths::ConvertRelativePathToFull(Directory);
			FPaths::NormalizeDirectoryName(Result);
			FPaths::RemoveDuplicateSlashes(Result);
			Result.RemoveFromEnd(TEXT("/"));
			return Result;
		}

		/** The text with every preprocessor line (and its continuation lines) blanked, newlines kept. */
		FString BlankPreprocessorLines(const FString& Text)
		{
			FString Result = Text;
			bool bAtLineStart = true;
			bool bInDirective = false;
			for (int32 Index = 0; Index < Result.Len(); ++Index)
			{
				const TCHAR Character = Result[Index];
				if (Character == TCHAR('\n'))
				{
					// A directive continues past its newline only when the line ended in a backslash.
					const bool bContinued = bInDirective && Index > 0 && (Result[Index - 1] == TCHAR('\\')
						|| (Result[Index - 1] == TCHAR('\r') && Index > 1 && Result[Index - 2] == TCHAR('\\')));
					bInDirective = bContinued;
					bAtLineStart = true;
					continue;
				}

				if (bAtLineStart && !FChar::IsWhitespace(Character))
				{
					bAtLineStart = false;
					if (Character == TCHAR('#'))
					{
						bInDirective = true;
					}
				}

				if (bInDirective && Character != TCHAR('\r'))
				{
					Result[Index] = TCHAR(' ');
				}
			}
			return Result;
		}

		/** The index just past the parenthesis that closes the one at OpenIndex, or INDEX_NONE. */
		int32 FindMatchingParenthesis(const FString& Text, const int32 OpenIndex)
		{
			int32 Depth = 0;
			for (int32 Index = OpenIndex; Index < Text.Len(); ++Index)
			{
				if (Text[Index] == TCHAR('('))
				{
					++Depth;
				}
				else if (Text[Index] == TCHAR(')'))
				{
					if (--Depth == 0)
					{
						return Index + 1;
					}
				}
			}
			return INDEX_NONE;
		}

		int32 SkipWhitespace(const FString& Text, int32 Index)
		{
			while (Index < Text.Len() && FChar::IsWhitespace(Text[Index]))
			{
				++Index;
			}
			return Index;
		}

		/** `numthreads(8, 8, 1)` inside an attribute: the group size when all three are integer literals. */
		bool TryParseNumThreads(const FString& AttributeText, FIntVector& OutGroupSize)
		{
			const FString Trimmed = AttributeText.TrimStartAndEnd();
			if (!Trimmed.StartsWith(TEXT("numthreads"), ESearchCase::IgnoreCase))
			{
				return false;
			}

			int32 Open = INDEX_NONE;
			int32 Close = INDEX_NONE;
			if (!Trimmed.FindChar(TCHAR('('), Open) || !Trimmed.FindLastChar(TCHAR(')'), Close) || Close <= Open)
			{
				return false;
			}

			TArray<FString> Arguments;
			Trimmed.Mid(Open + 1, Close - Open - 1).ParseIntoArray(Arguments, TEXT(","), /*InCullEmpty*/ false);
			if (Arguments.Num() != 3)
			{
				return false;
			}

			int32 Values[3] = { 0, 0, 0 };
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				FString Argument = Arguments[Axis].TrimStartAndEnd();
				// `8u` is a literal as well.
				Argument.RemoveFromEnd(TEXT("u"), ESearchCase::IgnoreCase);
				if (Argument.IsEmpty() || !Argument.IsNumeric() || Argument.Contains(TEXT(".")) || Argument.Contains(TEXT("-")))
				{
					return false;
				}
				Values[Axis] = FCString::Atoi(*Argument);
				if (Values[Axis] <= 0)
				{
					return false;
				}
			}

			OutGroupSize = FIntVector(Values[0], Values[1], Values[2]);
			return true;
		}

		/**
		 * `a/b/../c` collapsed, forward slashes, absolute. Not NormalizeSourceFilePath: its MakeStandardFilename turns a
		 * file under the engine root into a `../../../Engine/...` spelling, and a shader file is compared against the
		 * mapped directories, which ConvertRelativePathToFull makes absolute.
		 */
		FString NormalizeShaderFilePath(const FString& Path)
		{
			FString Result = FPaths::ConvertRelativePathToFull(Path);
			Result.ReplaceInline(TEXT("\\"), TEXT("/"));
			FPaths::CollapseRelativeDirectories(Result);
			FPaths::RemoveDuplicateSlashes(Result);
			return Result;
		}

		/** The deepest directory holding both. */
		FString CommonDirectoryOf(const FString& A, const FString& B)
		{
			TArray<FString> PartsA;
			TArray<FString> PartsB;
			A.ParseIntoArray(PartsA, TEXT("/"), /*InCullEmpty*/ false);
			B.ParseIntoArray(PartsB, TEXT("/"), /*InCullEmpty*/ false);

			TArray<FString> Common;
			for (int32 Index = 0; Index < PartsA.Num() && Index < PartsB.Num(); ++Index)
			{
				if (!PartsA[Index].Equals(PartsB[Index], ESearchCase::IgnoreCase))
				{
					break;
				}
				Common.Add(PartsA[Index]);
			}
			return FString::Join(Common, TEXT("/"));
		}

		/** Includes that name engine or plugin code: compiled as they are, never copied. */
		bool IsEngineOrPluginVirtualPath(const FString& Path)
		{
			return Path.StartsWith(TEXT("/Engine/"), ESearchCase::IgnoreCase)
				|| Path.StartsWith(TEXT("/Plugin/"), ESearchCase::IgnoreCase)
				|| Path.StartsWith(TEXT("/ThirdParty/"), ESearchCase::IgnoreCase);
		}
	}

	FString StripDreamPassShaderComments(const FString& Text)
	{
		FString Result = Text;
		enum class EState : uint8 { Code, LineComment, BlockComment, String };
		EState State = EState::Code;

		for (int32 Index = 0; Index < Result.Len(); ++Index)
		{
			const TCHAR Character = Result[Index];
			const TCHAR Next = Index + 1 < Result.Len() ? Result[Index + 1] : TCHAR('\0');

			switch (State)
			{
			case EState::Code:
				if (Character == TCHAR('/') && Next == TCHAR('/'))
				{
					State = EState::LineComment;
					Result[Index] = TCHAR(' ');
					Result[Index + 1] = TCHAR(' ');
					++Index;
				}
				else if (Character == TCHAR('/') && Next == TCHAR('*'))
				{
					State = EState::BlockComment;
					Result[Index] = TCHAR(' ');
					Result[Index + 1] = TCHAR(' ');
					++Index;
				}
				else if (Character == TCHAR('"'))
				{
					State = EState::String;
				}
				break;

			case EState::LineComment:
				if (Character == TCHAR('\n'))
				{
					State = EState::Code;
				}
				else if (Character != TCHAR('\r'))
				{
					Result[Index] = TCHAR(' ');
				}
				break;

			case EState::BlockComment:
				if (Character == TCHAR('*') && Next == TCHAR('/'))
				{
					State = EState::Code;
					Result[Index] = TCHAR(' ');
					Result[Index + 1] = TCHAR(' ');
					++Index;
				}
				else if (Character != TCHAR('\n') && Character != TCHAR('\r'))
				{
					Result[Index] = TCHAR(' ');
				}
				break;

			case EState::String:
				if (Character == TCHAR('\\'))
				{
					++Index;
				}
				else if (Character == TCHAR('"') || Character == TCHAR('\n'))
				{
					State = EState::Code;
				}
				break;
			}
		}
		return Result;
	}

	void ScanDreamPassShaderIncludes(const FString& StrippedText, TArray<FDreamPassShaderInclude>& OutIncludes)
	{
		OutIncludes.Reset();

		TArray<FString> Lines;
		StrippedText.ParseIntoArrayLines(Lines, /*InCullEmpty*/ false);
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			FString Line = Lines[LineIndex].TrimStartAndEnd();
			if (!Line.StartsWith(TEXT("#")))
			{
				continue;
			}
			Line.RightChopInline(1);
			Line.TrimStartInline();
			if (!Line.StartsWith(TEXT("include")))
			{
				continue;
			}
			Line.RightChopInline(7);
			Line.TrimStartInline();
			if (Line.Len() < 2)
			{
				continue;
			}

			const TCHAR Open = Line[0];
			const TCHAR Close = Open == TCHAR('<') ? TCHAR('>') : TCHAR('"');
			if (Open != TCHAR('"') && Open != TCHAR('<'))
			{
				// `#include SOME_MACRO`: nothing a text scan can follow, and the slot shader would not track it either.
				continue;
			}

			int32 CloseIndex = INDEX_NONE;
			for (int32 Index = 1; Index < Line.Len(); ++Index)
			{
				if (Line[Index] == Close)
				{
					CloseIndex = Index;
					break;
				}
			}
			if (CloseIndex <= 1)
			{
				continue;
			}

			FDreamPassShaderInclude& Include = OutIncludes.AddDefaulted_GetRef();
			Include.Path = Line.Mid(1, CloseIndex - 1).TrimStartAndEnd();
			Include.Path.ReplaceInline(TEXT("\\"), TEXT("/"));
			Include.Line = LineIndex + 1;
		}
	}

	void ScanDreamPassShaderFunctions(const FString& StrippedText, TMap<FString, FIntVector>& OutComputeEntries, TArray<FString>& OutFunctions)
	{
		using namespace DreamPassShaderTextDetail;

		OutComputeEntries.Reset();
		OutFunctions.Reset();

		const FString Text = BlankPreprocessorLines(StrippedText);

		int32 BraceDepth = 0;
		// The attributes seen at file scope since the last declaration ended: `[numthreads(8, 8, 1)]` belongs to the
		// function that follows it.
		TArray<FString> PendingAttributes;
		FString LastIdentifier;
		int32 LastIdentifierEnd = INDEX_NONE;

		for (int32 Index = 0; Index < Text.Len(); )
		{
			const TCHAR Character = Text[Index];

			if (Character == TCHAR('"'))
			{
				// A string at file scope (an `#include` is blanked already): skipped whole.
				int32 End = Index + 1;
				while (End < Text.Len() && Text[End] != TCHAR('"') && Text[End] != TCHAR('\n'))
				{
					End += Text[End] == TCHAR('\\') ? 2 : 1;
				}
				Index = End + 1;
				continue;
			}

			if (Character == TCHAR('{'))
			{
				++BraceDepth;
				++Index;
				continue;
			}
			if (Character == TCHAR('}'))
			{
				BraceDepth = FMath::Max(0, BraceDepth - 1);
				if (BraceDepth == 0)
				{
					PendingAttributes.Reset();
					LastIdentifier.Reset();
				}
				++Index;
				continue;
			}

			if (BraceDepth > 0)
			{
				++Index;
				continue;
			}

			if (Character == TCHAR(';'))
			{
				PendingAttributes.Reset();
				LastIdentifier.Reset();
				++Index;
				continue;
			}

			if (Character == TCHAR('['))
			{
				int32 Depth = 0;
				int32 End = Index;
				for (; End < Text.Len(); ++End)
				{
					if (Text[End] == TCHAR('['))
					{
						++Depth;
					}
					else if (Text[End] == TCHAR(']') && --Depth == 0)
					{
						break;
					}
				}
				if (End >= Text.Len())
				{
					break;
				}
				PendingAttributes.Add(Text.Mid(Index + 1, End - Index - 1));
				Index = End + 1;
				continue;
			}

			if (IsIdentifierStart(Character))
			{
				int32 End = Index + 1;
				while (End < Text.Len() && IsIdentifierCharacter(Text[End]))
				{
					++End;
				}
				LastIdentifier = Text.Mid(Index, End - Index);
				LastIdentifierEnd = End;
				Index = End;
				continue;
			}

			if (Character == TCHAR('(') && !LastIdentifier.IsEmpty() && SkipWhitespace(Text, LastIdentifierEnd) == Index)
			{
				const FString Name = LastIdentifier;
				const int32 AfterParameters = FindMatchingParenthesis(Text, Index);
				if (AfterParameters == INDEX_NONE)
				{
					break;
				}

				// `) : SV_Target0 {` -- a semantic between the parameter list and the body is allowed.
				int32 Cursor = SkipWhitespace(Text, AfterParameters);
				if (Cursor < Text.Len() && Text[Cursor] == TCHAR(':'))
				{
					Cursor = SkipWhitespace(Text, Cursor + 1);
					while (Cursor < Text.Len() && IsIdentifierCharacter(Text[Cursor]))
					{
						++Cursor;
					}
					Cursor = SkipWhitespace(Text, Cursor);
				}

				if (Cursor < Text.Len() && Text[Cursor] == TCHAR('{'))
				{
					FIntVector GroupSize;
					bool bCompute = false;
					for (const FString& Attribute : PendingAttributes)
					{
						if (TryParseNumThreads(Attribute, GroupSize))
						{
							bCompute = true;
							break;
						}
					}

					if (bCompute)
					{
						OutComputeEntries.Add(Name, GroupSize);
					}
					else
					{
						OutFunctions.AddUnique(Name);
					}
				}

				PendingAttributes.Reset();
				LastIdentifier.Reset();
				Index = AfterParameters;
				continue;
			}

			if (!FChar::IsWhitespace(Character))
			{
				// Anything else between a name and a `(` -- `=`, `,`, a template bracket -- means the name was not a function's.
				LastIdentifier.Reset();
			}
			++Index;
		}
	}

	FString NormalizeDreamPassShaderFilePath(const FString& Path)
	{
		return DreamPassShaderTextDetail::NormalizeShaderFilePath(Path);
	}

	bool IsDreamPassShaderFileExtension(const FString& Path)
	{
		const FString Extension = FPaths::GetExtension(Path);
		return Extension.Equals(TEXT("usf"), ESearchCase::IgnoreCase) || Extension.Equals(TEXT("ush"), ESearchCase::IgnoreCase);
	}

	bool MapDreamPassShaderFileToVirtualPath(const FString& FilePath, FString& OutVirtualPath)
	{
		using namespace DreamPassShaderTextDetail;

		OutVirtualPath.Reset();
		const FString File = NormalizeShaderFilePath(FilePath);

		int32 BestLength = INDEX_NONE;
		for (const TPair<FString, FString>& Mapping : AllShaderSourceDirectoryMappings())
		{
			const FString Directory = NormalizeMappedDirectory(Mapping.Value);
			if (Directory.IsEmpty() || !File.StartsWith(Directory + TEXT("/"), ESearchCase::IgnoreCase))
			{
				continue;
			}
			if (Directory.Len() > BestLength)
			{
				BestLength = Directory.Len();
				OutVirtualPath = Mapping.Key / File.RightChop(Directory.Len() + 1);
			}
		}
		return BestLength != INDEX_NONE;
	}

	bool MapDreamPassShaderVirtualPathToFile(const FString& VirtualPath, FString& OutFilePath)
	{
		using namespace DreamPassShaderTextDetail;

		OutFilePath.Reset();
		FString Virtual = VirtualPath.TrimStartAndEnd();
		Virtual.ReplaceInline(TEXT("\\"), TEXT("/"));

		int32 BestLength = INDEX_NONE;
		for (const TPair<FString, FString>& Mapping : AllShaderSourceDirectoryMappings())
		{
			if (!Virtual.StartsWith(Mapping.Key + TEXT("/"), ESearchCase::IgnoreCase) || Mapping.Key.Len() <= BestLength)
			{
				continue;
			}
			BestLength = Mapping.Key.Len();
			OutFilePath = NormalizeShaderFilePath(FPaths::Combine(NormalizeMappedDirectory(Mapping.Value), Virtual.RightChop(Mapping.Key.Len() + 1)));
		}
		return BestLength != INDEX_NONE;
	}

	void ResolveDreamPassShaderReference(const FString& Reference, const FString& PipelineSourceFile, FString& OutVirtualPath, FString& OutFilePath)
	{
		using namespace DreamPassShaderTextDetail;

		OutVirtualPath.Reset();
		OutFilePath.Reset();

		FString Written = Reference.TrimStartAndEnd().TrimQuotes();
		Written.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (Written.IsEmpty())
		{
			return;
		}

		if (Written.StartsWith(TEXT("/")))
		{
			// A virtual path, taken as written: it is what the slot's registry section and every message name.
			OutVirtualPath = Written;
			MapDreamPassShaderVirtualPathToFile(Written, OutFilePath);
			return;
		}

		OutFilePath = NormalizeShaderFilePath(FPaths::Combine(FPaths::GetPath(PipelineSourceFile), Written));
		MapDreamPassShaderFileToVirtualPath(OutFilePath, OutVirtualPath);
	}

	const FDreamPassShaderClosureFile* FDreamPassShaderClosure::FindByRelativePath(const FString& RelativePath) const
	{
		return Files.FindByPredicate([&RelativePath](const FDreamPassShaderClosureFile& File)
		{
			return File.RelativePath.Equals(RelativePath, ESearchCase::IgnoreCase);
		});
	}

	FString FDreamPassShaderClosure::ComputeContentHash() const
	{
		TArray<const FDreamPassShaderClosureFile*> SortedFiles;
		for (const FDreamPassShaderClosureFile& File : Files)
		{
			SortedFiles.Add(&File);
		}
		SortedFiles.Sort([](const FDreamPassShaderClosureFile& A, const FDreamPassShaderClosureFile& B)
		{
			return A.RelativePath.Compare(B.RelativePath, ESearchCase::IgnoreCase) < 0;
		});

		TArray<const FDreamPassLiveInclude*> SortedLive;
		for (const FDreamPassLiveInclude& Live : LiveIncludes)
		{
			SortedLive.Add(&Live);
		}
		SortedLive.Sort([](const FDreamPassLiveInclude& A, const FDreamPassLiveInclude& B)
		{
			return A.VirtualPath.Compare(B.VirtualPath, ESearchCase::IgnoreCase) < 0;
		});

		FString Material;
		for (const FDreamPassShaderClosureFile* File : SortedFiles)
		{
			Material += FString::Printf(TEXT("F %s\n"), *File->RelativePath.ToLower()); /* I18N-EXEMPT: hash material, never displayed */
			Material += File->Text;
			Material += TEXT("\n");
		}
		for (const FDreamPassLiveInclude* Live : SortedLive)
		{
			Material += FString::Printf(TEXT("L %s\n"), *Live->VirtualPath.ToLower()); /* I18N-EXEMPT: hash material, never displayed */
			Material += Live->Text;
			Material += TEXT("\n");
		}
		return HashDreamPassText(Material);
	}

	bool CollectDreamPassShaderClosure(const FString& RootFilePath, FDreamPassShaderClosure& OutClosure)
	{
		using namespace DreamPassShaderTextDetail;

		OutClosure = FDreamPassShaderClosure();
		OutClosure.RootFilePath = NormalizeShaderFilePath(RootFilePath);

		TArray<FString> Pending;
		Pending.Add(OutClosure.RootFilePath);
		TSet<FString> Visited;
		TSet<FString> LiveSeen;

		while (Pending.Num() > 0)
		{
			const FString File = Pending[0];
			Pending.RemoveAt(0);
			if (Visited.Contains(File))
			{
				continue;
			}
			Visited.Add(File);

			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *File))
			{
				if (OutClosure.Files.IsEmpty())
				{
					return false;
				}
				continue;
			}

			FDreamPassShaderClosureFile& Entry = OutClosure.Files.AddDefaulted_GetRef();
			Entry.FilePath = File;
			Entry.Text = Text;

			TArray<FDreamPassShaderInclude> Includes;
			ScanDreamPassShaderIncludes(StripDreamPassShaderComments(Text), Includes);
			for (const FDreamPassShaderInclude& Include : Includes)
			{
				if (Include.Path.StartsWith(TEXT("/")))
				{
					if (IsEngineOrPluginVirtualPath(Include.Path) || LiveSeen.Contains(Include.Path))
					{
						continue;
					}
					LiveSeen.Add(Include.Path);

					FDreamPassLiveInclude& Live = OutClosure.LiveIncludes.AddDefaulted_GetRef();
					Live.IncludingFile = File;
					Live.Line = Include.Line;
					Live.VirtualPath = Include.Path;
					if (MapDreamPassShaderVirtualPathToFile(Include.Path, Live.FilePath))
					{
						FFileHelper::LoadFileToString(Live.Text, *Live.FilePath);
					}
					continue;
				}

				const FString Included = NormalizeShaderFilePath(FPaths::Combine(FPaths::GetPath(File), Include.Path));
				if (!IFileManager::Get().FileExists(*Included))
				{
					FDreamPassMissingInclude& Missing = OutClosure.MissingIncludes.AddDefaulted_GetRef();
					Missing.IncludingFile = File;
					Missing.Line = Include.Line;
					Missing.Path = Include.Path;
					continue;
				}
				if (!Visited.Contains(Included))
				{
					Pending.Add(Included);
				}
			}
		}

		// The layout below the deepest directory holding every file, so a `../Shared/Noise.ush` still resolves in the
		// slot directory exactly as it did next to the source.
		FString Common = FPaths::GetPath(OutClosure.RootFilePath);
		for (const FDreamPassShaderClosureFile& File : OutClosure.Files)
		{
			Common = CommonDirectoryOf(Common, FPaths::GetPath(File.FilePath));
		}
		OutClosure.CommonDirectory = Common;

		for (FDreamPassShaderClosureFile& File : OutClosure.Files)
		{
			File.RelativePath = Common.IsEmpty() ? FPaths::GetCleanFilename(File.FilePath) : File.FilePath.RightChop(Common.Len() + 1);
		}
		return true;
	}

	FString HashDreamPassText(const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		FSHA1 Sha;
		Sha.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		Sha.Final();
		uint8 Digest[FSHA1::DigestSize];
		Sha.GetHash(Digest);
		return BytesToHex(Digest, FSHA1::DigestSize);
	}
}
