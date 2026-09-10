// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See the header for what this is. The interesting half is the rewrite, and the interesting thing
// about the rewrite is everything it refuses to touch.

#include "DreamShaderAssetRenameSyncService.h"

#include "Bridge/DreamShaderEditorBridge.h"
#include "DreamShaderDiagnostic.h"
#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"
#include "SourceFiles/DreamShaderSourceFileUtils.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		/**
		 * How long a rename batch is allowed to keep growing before it is flushed.
		 *
		 * "Fix Up Redirectors" on a content folder raises a few hundred OnAssetRenamed events inside
		 * one frame, and each of them, handled on its own, would mean one full pass over every source
		 * file in the project. Coalescing turns that back into a single pass. The window restarts on
		 * every event, so a long burst produces one batch rather than one per half second.
		 */
		constexpr double AssetRenameCoalesceWindowSeconds = 0.5;

		/** The poll rate of the flush ticker. Only ever runs while a batch is pending. */
		constexpr float AssetRenameFlushTickIntervalSeconds = 0.1f;

		// -----------------------------------------------------------------------------------------
		// Object paths
		// -----------------------------------------------------------------------------------------

		/** `/Game/A/T_X.T_X` -> `/Game/A/T_X`. A path with no trailing `.Name` is returned as-is. */
		FString GetPackagePathFromObjectPath(const FString& InObjectPath)
		{
			int32 SlashIndex = INDEX_NONE;
			if (!InObjectPath.FindLastChar(TCHAR('/'), SlashIndex))
			{
				SlashIndex = INDEX_NONE;
			}

			int32 DotIndex = INDEX_NONE;
			if (InObjectPath.FindLastChar(TCHAR('.'), DotIndex) && DotIndex > SlashIndex)
			{
				return InObjectPath.Left(DotIndex);
			}

			return InObjectPath;
		}

		/** `/Game/A/T_X` -> `T_X`. */
		FString GetAssetNameFromPackagePath(const FString& InPackagePath)
		{
			int32 SlashIndex = INDEX_NONE;
			if (InPackagePath.FindLastChar(TCHAR('/'), SlashIndex))
			{
				return InPackagePath.Mid(SlashIndex + 1);
			}

			return InPackagePath;
		}

		/**
		 * The batch, indexed for lookup.
		 *
		 * Both spellings of every renamed asset are keys, because a source may name it either way:
		 * `"/Game/A/T_X"` and `"/Game/A/T_X.T_X"` are the same reference, and whichever one the file
		 * used is the one it gets back. Keys are lowercased; the values are not, so the replacement
		 * carries the asset's real casing.
		 *
		 * Lookup is exact equality on the whole key, which is what keeps `/Game/Foo` from matching a
		 * rename of `/Game/FooBar` -- a prefix test here would quietly corrupt a neighbouring asset's
		 * references every time somebody renamed something whose name another asset starts with.
		 */
		struct FRenameTable
		{
			TMap<FString, FString> ByObjectPath;
			TMap<FString, FString> ByPackagePath;

			bool IsEmpty() const { return ByObjectPath.IsEmpty(); }
		};

		FRenameTable BuildRenameTable(const TArray<FDreamShaderAssetRename>& InRenames)
		{
			TMap<FString, FString> Direct;
			for (const FDreamShaderAssetRename& Rename : InRenames)
			{
				if (Rename.OldObjectPath.IsEmpty()
					|| Rename.NewObjectPath.IsEmpty()
					|| Rename.OldObjectPath.Equals(Rename.NewObjectPath, ESearchCase::IgnoreCase))
				{
					continue;
				}

				// Last write wins: the registry reports renames in order, so the newest event for an
				// old path is the one that describes where the asset ended up.
				Direct.Add(Rename.OldObjectPath.ToLower(), Rename.NewObjectPath);
			}

			FRenameTable Table;
			for (const TPair<FString, FString>& Pair : Direct)
			{
				// A->B followed by B->C arrives as two events in one batch, and the text is rewritten
				// in a single pass -- so the chain has to be collapsed here or a reference to A would
				// be left pointing at B, an asset that no longer exists either.
				FString Final = Pair.Value;
				for (int32 Hop = 0; Hop < Direct.Num(); ++Hop)
				{
					const FString* Next = Direct.Find(Final.ToLower());
					if (Next == nullptr || Next->Equals(Final, ESearchCase::IgnoreCase))
					{
						break;
					}
					Final = *Next;
				}

				if (Final.Equals(Pair.Key, ESearchCase::IgnoreCase))
				{
					continue;
				}

				Table.ByObjectPath.Add(Pair.Key, Final);
				Table.ByPackagePath.Add(GetPackagePathFromObjectPath(Pair.Key), GetPackagePathFromObjectPath(Final));
			}

			return Table;
		}

		/** An absolute object or package path, replaced if the batch renamed exactly it. */
		bool TryReplacePathText(const FString& InPathText, const FRenameTable& InTable, FString& OutPathText)
		{
			if (InPathText.IsEmpty() || InPathText[0] != TCHAR('/'))
			{
				return false;
			}

			const FString Key = InPathText.ToLower();
			if (const FString* Found = InTable.ByObjectPath.Find(Key))
			{
				OutPathText = *Found;
				return true;
			}
			if (const FString* Found = InTable.ByPackagePath.Find(Key))
			{
				OutPathText = *Found;
				return true;
			}

			return false;
		}

		/**
		 * One reference *value* -- the body of a string literal, or a `Path(...)` argument.
		 *
		 * Three shapes reach here: a bare absolute path, and the two shelled spellings the Content
		 * Browser's *Copy Reference* produces (`Texture2D'/Game/X.X'` and
		 * `/Script/Engine.Texture2D'/Game/X.X'`). For a shelled one only the text inside the quotes is
		 * touched: the class prefix is the author's, and rewriting it would be this service inventing
		 * a type name.
		 *
		 * Surrounding whitespace is carried across rather than trimmed off, because the result is
		 * spliced back into the file exactly where the input came from.
		 */
		bool TryReplaceReferenceValue(const FString& InValue, const FRenameTable& InTable, FString& OutValue)
		{
			int32 Start = 0;
			int32 End = InValue.Len();
			while (Start < End && FChar::IsWhitespace(InValue[Start]))
			{
				++Start;
			}
			while (End > Start && FChar::IsWhitespace(InValue[End - 1]))
			{
				--End;
			}
			if (Start >= End)
			{
				return false;
			}

			const FString Core = InValue.Mid(Start, End - Start);

			int32 FirstQuote = INDEX_NONE;
			int32 LastQuote = INDEX_NONE;
			if (Core.FindChar(TCHAR('\''), FirstQuote)
				&& Core.FindLastChar(TCHAR('\''), LastQuote)
				&& LastQuote > FirstQuote
				&& LastQuote == Core.Len() - 1)
			{
				const FString Inner = Core.Mid(FirstQuote + 1, LastQuote - FirstQuote - 1);
				FString NewInner;
				if (!TryReplacePathText(Inner, InTable, NewInner))
				{
					return false;
				}

				OutValue = InValue.Left(Start) + Core.Left(FirstQuote + 1) + NewInner + TEXT("'") + InValue.Mid(End);
				return true;
			}

			FString NewCore;
			if (!TryReplacePathText(Core, InTable, NewCore))
			{
				return false;
			}

			OutValue = InValue.Left(Start) + NewCore + InValue.Mid(End);
			return true;
		}

		// -----------------------------------------------------------------------------------------
		// Function bodies are opaque
		// -----------------------------------------------------------------------------------------

		/**
		 * Tracks whether the current line is inside a `Function` / `GraphFunction` body.
		 *
		 * A deliberate re-implementation of `FOpaqueRegionTracker` in
		 * `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp`, which is
		 * file-local to that translation unit and has no header. The rules are the same, and they must
		 * stay the same: a `Function` body is raw HLSL, no asset path can legally live in one, and a
		 * false hit inside one would splice a `/Game/...` string into shader code.
		 *
		 * Comment and literal state is tracked for the same reason it is there: a `// }` or a `"{"`
		 * inside an HLSL body would otherwise close the region early and hand the rest of the function
		 * back to the rewriter.
		 */
		struct FFunctionBodyTracker
		{
			enum class EState : uint8
			{
				Outside,
				SeekingBody,
				InsideBody,
			};

			EState State = EState::Outside;
			int32 BraceDepth = 0;
			/** Read by the rewriter before ScanLine, to seed its own per-line walk. */
			bool bInBlockComment = false;

			/** Asked BEFORE the line is scanned, so a declaration line is still ordinary source. */
			bool IsOpaque() const { return State != EState::Outside; }

			void ScanLine(const FString& InLine)
			{
				const int32 Length = InLine.Len();

				bool bLineIsHashShaped = false;
				for (int32 Probe = 0; Probe < Length; ++Probe)
				{
					if (!FChar::IsWhitespace(InLine[Probe]))
					{
						bLineIsHashShaped = InLine[Probe] == TCHAR('#');
						break;
					}
				}

				bool bInString = false;
				bool bInCharacter = false;

				for (int32 Index = 0; Index < Length; ++Index)
				{
					const TCHAR Character = InLine[Index];

					if (bInBlockComment)
					{
						if (Character == TCHAR('*') && Index + 1 < Length && InLine[Index + 1] == TCHAR('/'))
						{
							bInBlockComment = false;
							++Index;
						}
						continue;
					}

					if (bInString || bInCharacter)
					{
						if (Character == TCHAR('\\') && Index + 1 < Length)
						{
							++Index;
							continue;
						}
						if (bInString && Character == TCHAR('"'))
						{
							bInString = false;
						}
						else if (bInCharacter && Character == TCHAR('\''))
						{
							bInCharacter = false;
						}
						continue;
					}

					if (Character == TCHAR('/') && Index + 1 < Length)
					{
						if (InLine[Index + 1] == TCHAR('/'))
						{
							return;
						}
						if (InLine[Index + 1] == TCHAR('*'))
						{
							bInBlockComment = true;
							++Index;
							continue;
						}
					}

					if (Character == TCHAR('"'))
					{
						bInString = true;
						continue;
					}

					if (Character == TCHAR('\'') && HasClosingQuoteOnLine(InLine, Index))
					{
						bInCharacter = true;
						continue;
					}

					if (State == EState::Outside)
					{
						if (FChar::IsAlpha(Character) || Character == TCHAR('_'))
						{
							const int32 TokenStart = Index;
							while (Index < Length && (FChar::IsAlnum(InLine[Index]) || InLine[Index] == TCHAR('_')))
							{
								++Index;
							}

							const FString Token = InLine.Mid(TokenStart, Index - TokenStart);
							--Index;

							if (!bLineIsHashShaped
								&& (Token.Equals(TEXT("Function"), ESearchCase::CaseSensitive)
									|| Token.Equals(TEXT("GraphFunction"), ESearchCase::CaseSensitive)))
							{
								State = EState::SeekingBody;
								BraceDepth = 0;
							}
						}

						continue;
					}

					if (Character == TCHAR('{'))
					{
						++BraceDepth;
						State = EState::InsideBody;
						continue;
					}

					if (Character == TCHAR('}') && State == EState::InsideBody)
					{
						--BraceDepth;
						if (BraceDepth <= 0)
						{
							State = EState::Outside;
							BraceDepth = 0;
						}
					}
				}
			}

		private:
			static bool HasClosingQuoteOnLine(const FString& InLine, const int32 InOpenIndex)
			{
				for (int32 Index = InOpenIndex + 1; Index < InLine.Len(); ++Index)
				{
					if (InLine[Index] == TCHAR('\\'))
					{
						++Index;
						continue;
					}
					if (InLine[Index] == TCHAR('\''))
					{
						return true;
					}
				}
				return false;
			}
		};

		// -----------------------------------------------------------------------------------------
		// Path(...) argument handling
		// -----------------------------------------------------------------------------------------

		/** The `)` that closes the `(` at `InOpenIndex`, or INDEX_NONE if the line does not close it. */
		int32 FindMatchingParenthesis(const FString& InLine, const int32 InOpenIndex)
		{
			int32 Depth = 0;
			bool bInString = false;
			for (int32 Index = InOpenIndex; Index < InLine.Len(); ++Index)
			{
				const TCHAR Character = InLine[Index];
				if (bInString)
				{
					if (Character == TCHAR('\\') && Index + 1 < InLine.Len())
					{
						++Index;
						continue;
					}
					if (Character == TCHAR('"'))
					{
						bInString = false;
					}
					continue;
				}

				if (Character == TCHAR('"'))
				{
					bInString = true;
					continue;
				}
				if (Character == TCHAR('('))
				{
					++Depth;
				}
				else if (Character == TCHAR(')'))
				{
					--Depth;
					if (Depth == 0)
					{
						return Index;
					}
				}
			}

			return INDEX_NONE;
		}

		/** Trimmed [start, end) spans of the top-level comma-separated arguments of `InInner`. */
		bool SplitPathArguments(const FString& InInner, TArray<TPair<int32, int32>>& OutRanges)
		{
			OutRanges.Reset();

			auto AddTrimmed = [&InInner, &OutRanges](int32 Start, int32 End)
			{
				while (Start < End && FChar::IsWhitespace(InInner[Start]))
				{
					++Start;
				}
				while (End > Start && FChar::IsWhitespace(InInner[End - 1]))
				{
					--End;
				}
				OutRanges.Emplace(Start, End);
			};

			int32 ArgumentStart = 0;
			int32 Depth = 0;
			bool bInString = false;
			for (int32 Index = 0; Index < InInner.Len(); ++Index)
			{
				const TCHAR Character = InInner[Index];
				if (bInString)
				{
					if (Character == TCHAR('\\') && Index + 1 < InInner.Len())
					{
						++Index;
						continue;
					}
					if (Character == TCHAR('"'))
					{
						bInString = false;
					}
					continue;
				}

				if (Character == TCHAR('"'))
				{
					bInString = true;
					continue;
				}
				if (Character == TCHAR('('))
				{
					++Depth;
					continue;
				}
				if (Character == TCHAR(')'))
				{
					--Depth;
					continue;
				}
				if (Character == TCHAR(',') && Depth == 0)
				{
					AddTrimmed(ArgumentStart, Index);
					ArgumentStart = Index + 1;
				}
			}

			if (bInString)
			{
				return false;
			}

			AddTrimmed(ArgumentStart, InInner.Len());
			return true;
		}

		/** The text of a quoted or bare argument, plus whether it was quoted. */
		FString UnquoteArgument(const FString& InArgument, bool& bOutWasQuoted)
		{
			bOutWasQuoted = InArgument.Len() >= 2
				&& InArgument[0] == TCHAR('"')
				&& InArgument[InArgument.Len() - 1] == TCHAR('"');
			return bOutWasQuoted ? InArgument.Mid(1, InArgument.Len() - 2) : InArgument;
		}

		FString QuoteLike(const FString& InText, const bool bQuoted)
		{
			return bQuoted ? FString::Printf(TEXT("\"%s\""), *InText) : InText;
		}

		/**
		 * `Path(<root>, "<relative>")`.
		 *
		 * The hard one, and the reason the resolver is a parameter: a relative path means nothing
		 * until the root has been resolved, and only the generator knows what `Plugin.MoonToon`
		 * currently mounts to.
		 *
		 * On a hit the *written form is preserved wherever it still can be*. The root prefix is
		 * recovered by subtraction rather than by re-resolving a probe: the relative text is, by
		 * construction, the tail of the old package path, so removing it leaves exactly the prefix the
		 * root contributed. If the asset moved somewhere still under that prefix, only the relative
		 * argument changes and the file keeps reading the way its author wrote it. If it moved out
		 * from under the root, there is no relative spelling left, and the whole call collapses to the
		 * single-argument absolute form -- quoted, because that is the one spelling both resolvers
		 * accept.
		 */
		bool TryRewriteRootedPathCall(
			const FString& InInner,
			const FRenameTable& InTable,
			FDreamShaderAssetRenameSyncService::FPathExpressionResolver& InResolver,
			FString& OutCallText,
			FDreamShaderAssetReferenceRewrite& OutRewrite)
		{
			TArray<TPair<int32, int32>> Arguments;
			if (!SplitPathArguments(InInner, Arguments) || Arguments.Num() != 2)
			{
				return false;
			}

			const FString RootArgument = InInner.Mid(Arguments[0].Key, Arguments[0].Value - Arguments[0].Key);
			const FString PathArgument = InInner.Mid(Arguments[1].Key, Arguments[1].Value - Arguments[1].Key);
			if (RootArgument.IsEmpty() || PathArgument.IsEmpty())
			{
				return false;
			}

			FString ResolvedObjectPath;
			if (!InResolver(FString::Printf(TEXT("Path(%s,%s)"), *RootArgument, *PathArgument), ResolvedObjectPath))
			{
				return false;
			}

			const FString* NewObjectPath = InTable.ByObjectPath.Find(ResolvedObjectPath.ToLower());
			if (NewObjectPath == nullptr)
			{
				return false;
			}

			bool bWasQuoted = false;
			const FString RelativeText = UnquoteArgument(PathArgument, bWasQuoted);

			FString RelativePackagePart = RelativeText;
			RelativePackagePart.ReplaceInline(TEXT("\\"), TEXT("/"));
			while (RelativePackagePart.StartsWith(TEXT("/")))
			{
				RelativePackagePart.RightChopInline(1);
			}
			while (RelativePackagePart.EndsWith(TEXT("/")))
			{
				RelativePackagePart.LeftChopInline(1);
			}
			if (RelativePackagePart.IsEmpty())
			{
				return false;
			}

			// Did the author spell the object form (`Folder/T_X.T_X`) or the package form?
			const FString LastRelativeSegment = GetAssetNameFromPackagePath(RelativePackagePart);
			const bool bObjectStyle = LastRelativeSegment.Contains(TEXT("."));
			RelativePackagePart = GetPackagePathFromObjectPath(RelativePackagePart);

			const FString OldPackagePath = GetPackagePathFromObjectPath(ResolvedObjectPath);
			const FString NewPackagePath = GetPackagePathFromObjectPath(*NewObjectPath);

			FString RootPrefix;
			const FString RelativeSuffix = TEXT("/") + RelativePackagePart;
			if (OldPackagePath.EndsWith(RelativeSuffix, ESearchCase::IgnoreCase))
			{
				RootPrefix = OldPackagePath.Left(OldPackagePath.Len() - RelativeSuffix.Len());
			}

			OutRewrite.OldText = FString::Printf(TEXT("Path(%s, %s)"), *RootArgument, *PathArgument);

			if (!RootPrefix.IsEmpty() && NewPackagePath.StartsWith(RootPrefix + TEXT("/"), ESearchCase::IgnoreCase))
			{
				FString NewRelative = NewPackagePath.Mid(RootPrefix.Len() + 1);
				if (bObjectStyle)
				{
					NewRelative += TEXT(".") + GetAssetNameFromPackagePath(NewPackagePath);
				}

				// Only the second argument is replaced, so whatever the author wrote between the
				// arguments -- one space, none, a line of alignment padding -- survives.
				OutCallText = FString::Printf(
					TEXT("Path(%s%s%s)"),
					*InInner.Left(Arguments[1].Key),
					*QuoteLike(NewRelative, bWasQuoted),
					*InInner.Mid(Arguments[1].Value));
			}
			else
			{
				OutCallText = FString::Printf(
					TEXT("Path(\"%s\")"),
					bObjectStyle ? **NewObjectPath : *NewPackagePath);
			}

			OutRewrite.NewText = OutCallText;
			return true;
		}

		// -----------------------------------------------------------------------------------------
		// The line rewriter
		// -----------------------------------------------------------------------------------------

		/**
		 * One non-opaque line.
		 *
		 * Comments are copied through untouched. A reference inside one is not a reference the
		 * compiler will ever read, and leaving them alone means the service can never be blamed for
		 * editing prose.
		 */
		FString RewriteLine(
			const FString& InLine,
			bool bInBlockComment,
			const FRenameTable& InTable,
			FDreamShaderAssetRenameSyncService::FPathExpressionResolver& InResolver,
			FDreamShaderAssetRenameRewriteResult& InOutResult)
		{
			FString Output;
			Output.Reserve(InLine.Len() + 32);

			const int32 Length = InLine.Len();
			int32 Index = 0;
			while (Index < Length)
			{
				const TCHAR Character = InLine[Index];

				if (bInBlockComment)
				{
					if (Character == TCHAR('*') && Index + 1 < Length && InLine[Index + 1] == TCHAR('/'))
					{
						Output += TEXT("*/");
						Index += 2;
						bInBlockComment = false;
						continue;
					}
					Output.AppendChar(Character);
					++Index;
					continue;
				}

				if (Character == TCHAR('/') && Index + 1 < Length && InLine[Index + 1] == TCHAR('/'))
				{
					Output += InLine.Mid(Index);
					return Output;
				}
				if (Character == TCHAR('/') && Index + 1 < Length && InLine[Index + 1] == TCHAR('*'))
				{
					Output += TEXT("/*");
					Index += 2;
					bInBlockComment = true;
					continue;
				}

				if (Character == TCHAR('"'))
				{
					int32 CloseIndex = Index + 1;
					bool bEscaped = false;
					while (CloseIndex < Length)
					{
						const TCHAR Inner = InLine[CloseIndex];
						if (bEscaped)
						{
							bEscaped = false;
						}
						else if (Inner == TCHAR('\\'))
						{
							bEscaped = true;
						}
						else if (Inner == TCHAR('"'))
						{
							break;
						}
						++CloseIndex;
					}

					if (CloseIndex >= Length)
					{
						// Unterminated on this line. Not this service's business to guess.
						Output += InLine.Mid(Index);
						return Output;
					}

					const FString Body = InLine.Mid(Index + 1, CloseIndex - Index - 1);
					FString NewBody;
					if (TryReplaceReferenceValue(Body, InTable, NewBody))
					{
						Output.AppendChar(TCHAR('"'));
						Output += NewBody;
						Output.AppendChar(TCHAR('"'));
						InOutResult.bChanged = true;
						InOutResult.Rewrites.Add({ Body, NewBody });
					}
					else
					{
						Output += InLine.Mid(Index, CloseIndex - Index + 1);
					}

					Index = CloseIndex + 1;
					continue;
				}

				if (Character == TCHAR('\''))
				{
					// An unquoted shelled reference: `[Texture = Texture2D'/Game/X.X']`.
					int32 CloseIndex = Index + 1;
					while (CloseIndex < Length && InLine[CloseIndex] != TCHAR('\''))
					{
						++CloseIndex;
					}

					if (CloseIndex < Length)
					{
						const FString Inner = InLine.Mid(Index + 1, CloseIndex - Index - 1);
						FString NewInner;
						if (TryReplacePathText(Inner, InTable, NewInner))
						{
							Output.AppendChar(TCHAR('\''));
							Output += NewInner;
							Output.AppendChar(TCHAR('\''));
							InOutResult.bChanged = true;
							InOutResult.Rewrites.Add({ Inner, NewInner });
							Index = CloseIndex + 1;
							continue;
						}
					}

					Output.AppendChar(Character);
					++Index;
					continue;
				}

				if (FChar::IsAlpha(Character) || Character == TCHAR('_'))
				{
					int32 TokenEnd = Index;
					while (TokenEnd < Length && (FChar::IsAlnum(InLine[TokenEnd]) || InLine[TokenEnd] == TCHAR('_')))
					{
						++TokenEnd;
					}

					const FString Token = InLine.Mid(Index, TokenEnd - Index);

					// `Path(` with nothing between, matching what the resolvers accept.
					if (Token.Equals(TEXT("Path"), ESearchCase::IgnoreCase)
						&& TokenEnd < Length
						&& InLine[TokenEnd] == TCHAR('('))
					{
						const int32 CloseIndex = FindMatchingParenthesis(InLine, TokenEnd);
						if (CloseIndex != INDEX_NONE)
						{
							const FString Arguments = InLine.Mid(TokenEnd + 1, CloseIndex - TokenEnd - 1);
							FString NewCall;
							FDreamShaderAssetReferenceRewrite Rewrite;
							if (TryRewriteRootedPathCall(Arguments, InTable, InResolver, NewCall, Rewrite))
							{
								Output += NewCall;
								InOutResult.bChanged = true;
								InOutResult.Rewrites.Add(MoveTemp(Rewrite));
								Index = CloseIndex + 1;
								continue;
							}
						}

						// Not a two-argument rooted reference, or one that no rename touched. Emit the
						// head and let the loop walk into the arguments: the single-argument form is a
						// plain string literal and is handled there.
						Output += Token;
						Output.AppendChar(TCHAR('('));
						Index = TokenEnd + 1;
						continue;
					}

					Output += Token;
					Index = TokenEnd;
					continue;
				}

				Output.AppendChar(Character);
				++Index;
			}

			return Output;
		}

		bool RewriteSourceTextInternal(
			const FString& InSourceText,
			const FRenameTable& InTable,
			FDreamShaderAssetRenameSyncService::FPathExpressionResolver& InResolver,
			FString& OutSourceText,
			FDreamShaderAssetRenameRewriteResult& OutResult)
		{
			OutResult = FDreamShaderAssetRenameRewriteResult();
			OutSourceText.Reset();

			if (InTable.IsEmpty())
			{
				OutSourceText = InSourceText;
				return false;
			}

			OutSourceText.Reserve(InSourceText.Len() + 64);

			FFunctionBodyTracker Tracker;
			int32 Index = 0;
			while (Index < InSourceText.Len())
			{
				int32 LineEnd = Index;
				while (LineEnd < InSourceText.Len() && InSourceText[LineEnd] != TCHAR('\n'))
				{
					++LineEnd;
				}

				int32 ContentEnd = LineEnd;
				if (ContentEnd > Index && InSourceText[ContentEnd - 1] == TCHAR('\r'))
				{
					--ContentEnd;
				}

				const FString Line = InSourceText.Mid(Index, ContentEnd - Index);

				// Asked before the line is scanned, so the `Function` declaration line itself is still
				// ordinary source and the body's closing `}` line is still the last opaque one.
				const bool bOpaque = Tracker.IsOpaque();
				const bool bBlockCommentAtLineStart = Tracker.bInBlockComment;

				OutSourceText += bOpaque
					? Line
					: RewriteLine(Line, bBlockCommentAtLineStart, InTable, InResolver, OutResult);

				// The line terminator is copied verbatim: this tree is CRLF, and a rewrite that
				// normalized line endings would show up as a whole-file diff every time.
				OutSourceText += InSourceText.Mid(ContentEnd, LineEnd - ContentEnd);
				if (LineEnd < InSourceText.Len())
				{
					OutSourceText.AppendChar(TCHAR('\n'));
				}

				Tracker.ScanLine(Line);
				Index = LineEnd + 1;
			}

			return OutResult.bChanged;
		}

		// -----------------------------------------------------------------------------------------
		// The coalescing front end
		// -----------------------------------------------------------------------------------------

		struct FAssetRenameSyncState
		{
			FDelegateHandle AssetRenamedHandle;
			FTSTicker::FDelegateHandle FlushTickerHandle;
			TArray<FDreamShaderAssetRename> Pending;
			double LastEventSeconds = 0.0;
			bool bStarted = false;
		};

		FAssetRenameSyncState GAssetRenameSyncState;

		bool IsAssetRenameSyncEnabled()
		{
			const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
			return Settings == nullptr || Settings->bSyncSourceReferencesOnAssetRename;
		}

		/**
		 * Only the editor that owns the bridge rewrites source files.
		 *
		 * Two editors open on one project both hear every rename. Both writing would mean two
		 * processes racing on the same `.dsm` and two `.bak` files describing different moments, and
		 * the second write would land on a file the first had already fixed.
		 */
		bool IsBridgeOwnerProcess()
		{
			const FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge();
			return Bridge != nullptr && Bridge->IsBridgeOwner();
		}

		void LogBatch(const FDreamShaderAssetRenameSyncResult& InResult, const TArray<FDreamShaderAssetRename>& InRenames)
		{
			if (InResult.Files.IsEmpty())
			{
				return;
			}

			// One Display line for the whole batch. A line per file would bury the rename pairs that
			// explain it, and this is the record a user goes looking for after VSCode tells them a
			// file they had open changed on disk.
			FString Report = FString::Printf(
				TEXT("DreamShader rewrote %d source file(s) after %d asset rename(s):"),
				InResult.Files.Num(),
				InRenames.Num());

			for (const FDreamShaderAssetRenameSyncFileResult& File : InResult.Files)
			{
				Report += FString::Printf(
					TEXT("\n  %s  (%d reference(s), backup '%s')"),
					*File.SourceFilePath,
					File.Rewrites.Num(),
					*File.BackupFilePath);

				for (const FDreamShaderAssetReferenceRewrite& Rewrite : File.Rewrites)
				{
					Report += FString::Printf(TEXT("\n      %s  ->  %s"), *Rewrite.OldText, *Rewrite.NewText);
				}
			}

			UE_LOG(LogDreamShader, Display, TEXT("%s"), *Report);
		}

		void FlushPendingRenames()
		{
			TArray<FDreamShaderAssetRename> Batch = MoveTemp(GAssetRenameSyncState.Pending);
			GAssetRenameSyncState.Pending.Reset();

			if (Batch.IsEmpty() || !IsAssetRenameSyncEnabled() || !IsBridgeOwnerProcess())
			{
				return;
			}

			const FDreamShaderAssetRenameSyncResult Result = FDreamShaderAssetRenameSyncService::SyncRenames(Batch);
			LogBatch(Result, Batch);

			// Text first, rebuild second: SyncRenames has written the WHOLE batch before this line,
			// so no file is compiled while a sibling still holds the old path. The rebuild is
			// requested explicitly rather than left to the directory watcher, because the watcher is
			// gated on Auto Compile On Save and a file the plugin itself rewrote must be rebuilt
			// regardless -- otherwise the asset and the text it claims to come from disagree until
			// the user happens to compile. The watcher will also see these writes; queueing is keyed
			// by path, so the second request is a no-op.
			TArray<FString> RewrittenFiles;
			for (const FDreamShaderAssetRenameSyncFileResult& File : Result.Files)
			{
				RewrittenFiles.Add(File.SourceFilePath);
			}
			if (FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge())
			{
				Bridge->RequestRebuildAfterSourceRewrite(RewrittenFiles);
			}
		}

		bool TickFlush(float /*DeltaSeconds*/)
		{
			if (IsEngineExitRequested() || GExitPurge)
			{
				GAssetRenameSyncState.FlushTickerHandle.Reset();
				GAssetRenameSyncState.Pending.Reset();
				return false;
			}

			if (FPlatformTime::Seconds() - GAssetRenameSyncState.LastEventSeconds < AssetRenameCoalesceWindowSeconds)
			{
				return true;
			}

			GAssetRenameSyncState.FlushTickerHandle.Reset();
			FlushPendingRenames();
			return false;
		}

		void HandleAssetRenamed(const FAssetData& InAssetData, const FString& InOldObjectPath)
		{
			if (!GAssetRenameSyncState.bStarted || IsEngineExitRequested() || GExitPurge)
			{
				return;
			}

			// Read live, not cached at startup: turning the setting off in Project Settings has to
			// take effect on the next rename, not on the next editor restart.
			if (!IsAssetRenameSyncEnabled() || !IsBridgeOwnerProcess())
			{
				return;
			}

			const FString NewObjectPath = InAssetData.GetObjectPathString();
			if (InOldObjectPath.IsEmpty()
				|| NewObjectPath.IsEmpty()
				|| InOldObjectPath.Equals(NewObjectPath, ESearchCase::IgnoreCase))
			{
				return;
			}

			GAssetRenameSyncState.Pending.Add({ InOldObjectPath, NewObjectPath });
			GAssetRenameSyncState.LastEventSeconds = FPlatformTime::Seconds();

			if (!GAssetRenameSyncState.FlushTickerHandle.IsValid())
			{
				GAssetRenameSyncState.FlushTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateStatic(&TickFlush),
					AssetRenameFlushTickIntervalSeconds);
			}
		}
	}

	void FDreamShaderAssetRenameSyncService::Startup()
	{
		if (GAssetRenameSyncState.bStarted)
		{
			return;
		}

		IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
		if (AssetRegistry == nullptr)
		{
			UE_LOG(
				LogDreamShader,
				Warning,
				TEXT("DreamShader could not subscribe to asset renames: the asset registry is unavailable. Source files will not follow assets that are renamed or moved."));
			return;
		}

		GAssetRenameSyncState.bStarted = true;
		GAssetRenameSyncState.AssetRenamedHandle = AssetRegistry->OnAssetRenamed().AddStatic(&HandleAssetRenamed);
	}

	void FDreamShaderAssetRenameSyncService::Shutdown()
	{
		if (GAssetRenameSyncState.FlushTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(GAssetRenameSyncState.FlushTickerHandle);
			GAssetRenameSyncState.FlushTickerHandle.Reset();
		}

		if (GAssetRenameSyncState.bStarted)
		{
			if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
			{
				AssetRegistry->OnAssetRenamed().Remove(GAssetRenameSyncState.AssetRenamedHandle);
			}
			GAssetRenameSyncState.AssetRenamedHandle.Reset();
			GAssetRenameSyncState.bStarted = false;
		}

		// Dropped rather than flushed: an editor on its way out must not start rewriting sources.
		GAssetRenameSyncState.Pending.Reset();
	}

	bool FDreamShaderAssetRenameSyncService::RewriteSourceText(
		const FString& InSourceText,
		const TArray<FDreamShaderAssetRename>& InRenames,
		FString& OutSourceText,
		FDreamShaderAssetRenameRewriteResult& OutResult)
	{
		auto GeneratorResolver = [](const FString& InPathExpression, FString& OutObjectPath)
		{
			FDreamShaderError ResolveError;
			return TryResolveDreamShaderAssetReference(InPathExpression, OutObjectPath, ResolveError);
		};

		return RewriteSourceText(InSourceText, InRenames, FPathExpressionResolver(GeneratorResolver), OutSourceText, OutResult);
	}

	bool FDreamShaderAssetRenameSyncService::RewriteSourceText(
		const FString& InSourceText,
		const TArray<FDreamShaderAssetRename>& InRenames,
		FPathExpressionResolver InResolver,
		FString& OutSourceText,
		FDreamShaderAssetRenameRewriteResult& OutResult)
	{
		const FRenameTable Table = BuildRenameTable(InRenames);
		return RewriteSourceTextInternal(InSourceText, Table, InResolver, OutSourceText, OutResult);
	}

	FDreamShaderAssetRenameSyncResult FDreamShaderAssetRenameSyncService::SyncRenames(
		const TArray<FDreamShaderAssetRename>& InRenames)
	{
		FDreamShaderAssetRenameSyncResult Result;

		const FRenameTable Table = BuildRenameTable(InRenames);
		if (Table.IsEmpty())
		{
			return Result;
		}

		auto GeneratorResolver = [](const FString& InPathExpression, FString& OutObjectPath)
		{
			FDreamShaderError ResolveError;
			return TryResolveDreamShaderAssetReference(InPathExpression, OutObjectPath, ResolveError);
		};
		FPathExpressionResolver Resolver(GeneratorResolver);

		TArray<FString> SourceFiles;
		FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(SourceFiles);

		for (const FString& SourceFile : SourceFiles)
		{
			// A plugin ships its sources as authored. Renaming a project asset that a plugin source
			// happens to reference is the plugin author's problem to solve in their own repository,
			// not something this editor may fix by editing files it does not own.
			if (!UE::DreamShader::IsWritableSourceFilePath(SourceFile))
			{
				continue;
			}

			++Result.ScannedFileCount;

			FString SourceText;
			if (!FFileHelper::LoadFileToString(SourceText, *SourceFile))
			{
				continue;
			}

			FString UpdatedText;
			FDreamShaderAssetRenameRewriteResult FileRewrite;
			if (!RewriteSourceTextInternal(SourceText, Table, Resolver, UpdatedText, FileRewrite))
			{
				continue;
			}

			// The same `.bak` name and the same mechanism as Adopt Into Source, so there is one place
			// to look after any DreamShader action has rewritten a source, and one habit to learn.
			const FString BackupFilePath = SourceFile + TEXT(".bak");
			if (IFileManager::Get().Copy(*BackupFilePath, *SourceFile, true) != COPY_OK)
			{
				FDreamShaderError BackupError;
				UE::DreamShader::FailWith(
					BackupError,
					TEXT("DSH9020"),
					FString::Printf(
						TEXT("Could not back up '%s' to '%s'; its asset references were left pointing at the old path."),
						*SourceFile,
						*BackupFilePath));

				++Result.FailureCount;
				UE_LOG(LogDreamShader, Warning, TEXT("%s: %s"), *BackupError.Code, *BackupError.Message);
				continue;
			}

			if (!FFileHelper::SaveStringToFile(UpdatedText, *SourceFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				FDreamShaderError WriteError;
				UE::DreamShader::FailWith(
					WriteError,
					TEXT("DSH9021"),
					FString::Printf(
						TEXT("Could not write '%s' after renaming an asset it references; the file as it was is in '%s'."),
						*SourceFile,
						*BackupFilePath));

				++Result.FailureCount;
				UE_LOG(LogDreamShader, Warning, TEXT("%s: %s"), *WriteError.Code, *WriteError.Message);
				continue;
			}

			FDreamShaderAssetRenameSyncFileResult& FileResult = Result.Files.AddDefaulted_GetRef();
			FileResult.SourceFilePath = SourceFile;
			FileResult.BackupFilePath = BackupFilePath;
			FileResult.Rewrites = MoveTemp(FileRewrite.Rewrites);
		}

		return Result;
	}
}
