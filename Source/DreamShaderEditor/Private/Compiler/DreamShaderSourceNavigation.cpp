// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderSourceNavigation.h for what this is and why the table is keyed by expression guid.

#include "DreamShaderSourceNavigation.h"

#include "Diagnostics/DreamShaderTextWireUtils.h"
#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"
#include "Lang/LangDiagnostic.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"
#include "Workspace/DreamShaderWorkspaceService.h"

#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "IMaterialEditor.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialInstance.h"
#include "Misc/App.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "UObject/MetaData.h"
#include "UObject/ObjectKey.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "DreamShader.Navigation"

namespace UE::DreamShader::Editor::Private
{
	using UE::DreamShader::Lang::FLangDiagnosticSink;
	using UE::DreamShader::Lang::FLangSpan;

	namespace
	{
		static const FName DreamShaderNavigationMenuOwnerName(TEXT("DreamShaderSourceNavigation"));

		/** Set by Register(), consumed by Unregister(). Mirrors the Material Content Browser's handle. */
		static FDelegateHandle GNavigationMenuStartupHandle;

		/** The diagnostics `stage` for everything this file raises (contract §6.12: stage is the wire's). */
		static const TCHAR* NavigationDiagnosticStage = TEXT("navigate");

		/**
		 * The graph-node context menus DreamShader-generated nodes can appear under.
		 *
		 * Spelled as strings rather than `UClass::GetName()` for two reasons. UEdGraphNode::
		 * IncludeParentNodeContextMenu() defaults to FALSE, so a node class does NOT inherit its
		 * parent's menu -- extending only "MaterialGraphNode" would silently miss every `@custom`
		 * node (UMaterialGraphNode_Custom) and every operator node. And naming the classes in C++
		 * would make this file stop compiling on an engine that does not have one of them yet:
		 * UMaterialGraphNode_Operator, for instance, is not in every engine this plugin supports.
		 * An ExtendMenu on a name nothing ever registers is a no-op, so an extra name costs nothing.
		 *
		 * The list is UMaterialGraph::AddExpression's dispatch, minus the comment node (which has no
		 * expression at all). See Editor/UnrealEd/Private/MaterialGraph.cpp.
		 */
		static const TCHAR* const GraphNodeContextMenuNames[] =
		{
			TEXT("GraphEditor.GraphNodeContextMenu.MaterialGraphNode"),
			TEXT("GraphEditor.GraphNodeContextMenu.MaterialGraphNode_Custom"),
			TEXT("GraphEditor.GraphNodeContextMenu.MaterialGraphNode_Composite"),
			TEXT("GraphEditor.GraphNodeContextMenu.MaterialGraphNode_Operator"),
			TEXT("GraphEditor.GraphNodeContextMenu.MaterialGraphNode_PinBase"),
			TEXT("GraphEditor.GraphNodeContextMenu.MaterialGraphNode_Knot"),
		};

		/**
		 * One package metadata value.
		 *
		 * The same mechanism MaterialAssetGeneration/DreamShaderGeneratedAssetMetadata.cpp writes
		 * through, down to the engine gate: UPackage::GetMetaData() returns FMetaData& from 5.6 and
		 * UMetaData* before it. That file's accessor is file-local, which is why this is a copy and
		 * not a call -- keep the two in step.
		 */
		FString GetAssetMetadataValue(UObject* Asset, const TCHAR* Key)
		{
			if (!Asset)
			{
				return FString();
			}

			UPackage* Package = Asset->GetOutermost();
			if (!Package)
			{
				return FString();
			}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
			return Package->GetMetaData().GetValue(Asset, Key);
#else
			if (UMetaData* MetaData = Package->GetMetaData())
			{
				return MetaData->GetValue(Asset, Key);
			}
			return FString();
#endif
		}

		/** A span standing in for "this is about <Line>:<Column> of the sink's file". */
		FLangSpan MakeLineSpan(const int32 Line, const int32 Column = 1, const int32 Length = 0)
		{
			FLangSpan Span;
			Span.Offset = 0;
			Span.Length = Length;
			Span.Line = FMath::Max(1, Line);
			Span.Column = FMath::Max(1, Column);
			return Span;
		}

		/**
		 * The sink's diagnostics in the form the bridge writes out.
		 *
		 * A local converter on purpose: P owns the real one (Compiler/DreamShaderCompilerDiagnostics,
		 * contract §5), it does not exist yet, and a navigation refusal must not wait on it. Replace
		 * the body with a call to P's converter when it lands -- the field mapping is the same.
		 */
		TArray<FDreamShaderDiagnosticRecord> ToDiagnosticRecords(
			const FLangDiagnosticSink& Sink,
			const FString& FallbackFilePath,
			const FString& AssetPath)
		{
			TArray<FDreamShaderDiagnosticRecord> Records;
			Records.Reserve(Sink.Num());
			for (const Lang::FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
			{
				FDreamShaderDiagnosticRecord& Record = Records.AddDefaulted_GetRef();
				Record.FilePath = Diagnostic.FilePath.IsEmpty() ? FallbackFilePath : Diagnostic.FilePath;
				Record.Message = Diagnostic.Message;
				Record.Stage = NavigationDiagnosticStage;
				Record.AssetPath = AssetPath;
				Record.Code = Diagnostic.Code;
				Record.Line = FMath::Max(1, Diagnostic.Span.Line);
				Record.Column = FMath::Max(1, Diagnostic.Span.Column);
				Record.Severity = Lang::LexToString(Diagnostic.Severity);
			}
			return Records;
		}

		/** Four seconds, no buttons -- the shape every non-question DreamShader toast already has. */
		void ShowNavigationNotification(const FText& Message, const SNotificationItem::ECompletionState State)
		{
			if (!FSlateApplication::IsInitialized() || IsRunningCommandlet() || FApp::IsUnattended())
			{
				return;
			}

			FNotificationInfo Info(Message);
			Info.ExpireDuration = 4.0f;
			Info.bFireAndForget = true;
			if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
			{
				Item->SetCompletionState(State);
			}
		}

		/** Every loaded object that could carry a SourceSpans table: the graph-bearing asset classes. */
		void ForEachLoadedGeneratedGraphAsset(const TFunctionRef<bool(UObject*)>& Visitor)
		{
			// Transient objects are skipped before anything else is asked of them: the material editor
			// works on a DUPLICATE of the asset in the transient package, and those duplicates are
			// both numerous and guaranteed to carry no package metadata. Asking each one for a
			// metadata value would be pure cost.
			const UPackage* Transient = GetTransientPackage();

			for (TObjectIterator<UMaterial> It; It; ++It)
			{
				UMaterial* Material = *It;
				if (!IsValid(Material) || Material->GetOutermost() == Transient)
				{
					continue;
				}
				if (!Visitor(Material))
				{
					return;
				}
			}

			// UMaterialFunctionInterface, not UMaterialFunction: material layers and layer blends are
			// their own classes, and a `@layer` product is exactly one of those.
			for (TObjectIterator<UMaterialFunctionInterface> It; It; ++It)
			{
				UMaterialFunctionInterface* Function = *It;
				if (!IsValid(Function) || Function->GetOutermost() == Transient)
				{
					continue;
				}
				if (!Visitor(Function))
				{
					return;
				}
			}
		}

		/**
		 * The parsed table of one asset, remembered between right-clicks.
		 *
		 * The graph->source direction searches every generated asset's table for one guid, so without
		 * this every right-click would re-parse every table in the project. Keyed by FObjectKey (an
		 * object that has been destroyed simply never matches again) and validated against the raw
		 * metadata string's length and CRC, so a recompile that rewrote the table invalidates the
		 * entry without anyone having to remember to.
		 */
		struct FCachedSpanTable
		{
			int32 JsonLength = 0;
			uint32 JsonCrc = 0;
			FDreamShaderSourceSpanTable Table;
		};

		static TMap<FObjectKey, FCachedSpanTable> GSpanTableCache;

		const FDreamShaderSourceSpanTable& GetCachedSpanTable(UObject* TableOwner, const FString& JsonText)
		{
			const FObjectKey Key(TableOwner);
			const int32 JsonLength = JsonText.Len();
			const uint32 JsonCrc = FCrc::StrCrc32(*JsonText);

			if (const FCachedSpanTable* Existing = GSpanTableCache.Find(Key))
			{
				if (Existing->JsonLength == JsonLength && Existing->JsonCrc == JsonCrc)
				{
					return Existing->Table;
				}
			}

			FCachedSpanTable& Entry = GSpanTableCache.FindOrAdd(Key);
			Entry.JsonLength = JsonLength;
			Entry.JsonCrc = JsonCrc;
			Entry.Table = FDreamShaderSourceSpanTable::ParseJson(JsonText, TableOwner->GetPathName(), nullptr);
			return Entry.Table;
		}

		/**
		 * The stamped (project-relative) source path as an absolute normalized one, or empty.
		 *
		 * Prefixed rather than named StampedSourcePathToAbsolute, which is what it mirrors:
		 * UI/Model/DreamShaderBrowserModel.cpp has a function of that exact name and signature in
		 * an anonymous namespace inside this same enclosing namespace. Two anonymous namespaces at
		 * one scope in one translation unit ARE one namespace, so a unity blob that ever held both
		 * files would be a redefinition error -- and which files share a blob is only their sorted
		 * byte offsets (UnrealBuildTool System/Unity.cs), which every added file moves.
		 */
		FString NavigationStampedSourcePathToAbsolute(const FString& StampedPath)
		{
			if (StampedPath.IsEmpty())
			{
				return FString();
			}
			FString AbsolutePath = StampedPath;
			if (FPaths::IsRelative(AbsolutePath))
			{
				AbsolutePath = FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), StampedPath);
			}
			return UE::DreamShader::NormalizeSourceFilePath(AbsolutePath);
		}

		/** True when a resolved table path names the same source file as the request. */
		bool SpanFileMatches(const FString& ResolvedSpanFile, const FString& NormalizedRequestedFile)
		{
			// Case-insensitively, and only after both sides have been through the same normalizer:
			// the table's `file` is whatever the compiler was handed (the bridge normalizes, a
			// commandlet may not), and Windows paths differ in case for the same file.
			return !ResolvedSpanFile.IsEmpty()
				&& ResolvedSpanFile.Equals(NormalizedRequestedFile, ESearchCase::IgnoreCase);
		}

		/** Reads one int field, tolerating the JSON number type it arrives as. */
		int32 ReadIntField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const int32 Default)
		{
			double Value = static_cast<double>(Default);
			if (!Object->TryGetNumberField(Field, Value))
			{
				return Default;
			}
			return FMath::RoundToInt32(Value);
		}
	}

	// ----------------------------------------------------------------------------- FDreamShaderSourceSpan

	FString FDreamShaderSourceSpan::GetAbsoluteFilePath() const
	{
		return NavigationStampedSourcePathToAbsolute(File);
	}

	FString FDreamShaderSourceSpan::GetAbsoluteCallSiteFilePath() const
	{
		return CallFile.IsEmpty() ? GetAbsoluteFilePath() : NavigationStampedSourcePathToAbsolute(CallFile);
	}

	// ------------------------------------------------------------------------ FDreamShaderSourceSpanTable

	const TCHAR* FDreamShaderSourceSpanTable::GetMetadataKey()
	{
		return TEXT("DreamShader.SourceSpans");
	}

	UObject* FDreamShaderSourceSpanTable::ResolveTableOwner(UObject* Asset)
	{
		if (!IsValid(Asset))
		{
			return nullptr;
		}

		if (Asset->IsA<UMaterial>() || Asset->IsA<UMaterialFunctionInterface>())
		{
			return Asset;
		}

		// A ThinCustom product is addressed as the instance, but the graph -- and therefore the table
		// -- lives on the hidden base material it parents to. Walk the chain rather than reading
		// Parent once: an instance OF a generated instance (the .dsi case, plan §13.1) is legal.
		UMaterialInstance* Instance = Cast<UMaterialInstance>(Asset);
		int32 Guard = 16;
		while (Instance && Guard-- > 0)
		{
			UMaterialInterface* Parent = Instance->Parent.Get();
			if (UMaterial* BaseMaterial = Cast<UMaterial>(Parent))
			{
				return BaseMaterial;
			}
			Instance = Cast<UMaterialInstance>(Parent);
		}

		return nullptr;
	}

	FDreamShaderSourceSpanTable FDreamShaderSourceSpanTable::LoadFromAsset(UObject* Asset, FLangDiagnosticSink* Sink)
	{
		FDreamShaderSourceSpanTable Table;

		UObject* Owner = ResolveTableOwner(Asset);
		if (!Owner)
		{
			return Table;
		}

		const FString JsonText = GetAssetMetadataValue(Owner, GetMetadataKey());
		if (JsonText.IsEmpty())
		{
			// Not an error and not diagnosed: a hand-authored material, or one the 1.x generator
			// wrote, simply has no table. Whether that is a problem is the caller's question.
			return Table;
		}

		Table = ParseJson(JsonText, Owner->GetPathName(), Sink);
		Table.OwningAsset = Owner;
		return Table;
	}

	FDreamShaderSourceSpanTable FDreamShaderSourceSpanTable::ParseJson(
		const FString& JsonText,
		const FString& AssetPathForMessages,
		FLangDiagnosticSink* Sink)
	{
		FDreamShaderSourceSpanTable Table;

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			if (Sink)
			{
				Sink->Error(TEXT("DSH9053"), MakeLineSpan(1), FText::Format(
					LOCTEXT("SpanTableNotJson", "The DreamShader.SourceSpans metadata of '{Asset}' is not a JSON object, so no node on it can be mapped back to a source line; rebuild the asset from its source."),
					FFormatNamedArguments{ { TEXT("Asset"), FText::FromString(AssetPathForMessages) } }));
			}
			return Table;
		}

		Table.Spans.Reserve(Root->Values.Num());
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
		{
			FGuid ExpressionGuid;
			if (!FGuid::Parse(Pair.Key, ExpressionGuid) || !ExpressionGuid.IsValid())
			{
				if (Sink)
				{
					Sink->Warning(TEXT("DSH9059"), MakeLineSpan(1), FText::Format(
						LOCTEXT("SpanTableBadKey", "The DreamShader.SourceSpans entry '{Key}' of '{Asset}' is not an expression GUID and was skipped; that node cannot be navigated to."),
						FFormatNamedArguments{
							{ TEXT("Key"), FText::FromString(Pair.Key) },
							{ TEXT("Asset"), FText::FromString(AssetPathForMessages) } }));
				}
				continue;
			}

			const TSharedPtr<FJsonObject>* RowObject = nullptr;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(RowObject) || !RowObject || !RowObject->IsValid())
			{
				if (Sink)
				{
					Sink->Warning(TEXT("DSH9059"), MakeLineSpan(1), FText::Format(
						LOCTEXT("SpanTableBadRow", "The DreamShader.SourceSpans entry '{Key}' of '{Asset}' is not an object with file/line/col and was skipped; that node cannot be navigated to."),
						FFormatNamedArguments{
							{ TEXT("Key"), FText::FromString(Pair.Key) },
							{ TEXT("Asset"), FText::FromString(AssetPathForMessages) } }));
				}
				continue;
			}

			FDreamShaderSourceSpan Span;
			(*RowObject)->TryGetStringField(TEXT("file"), Span.File);
			Span.Line = ReadIntField(*RowObject, TEXT("line"), 1);
			Span.Column = ReadIntField(*RowObject, TEXT("col"), 1);
			Span.Length = ReadIntField(*RowObject, TEXT("len"), 0);
			// `callFile` is written only when the call site is in another file (CONTRACT 6.13 #6);
			// absent means the same file as `file`. See FDreamShaderSourceSpan.
			(*RowObject)->TryGetStringField(TEXT("callFile"), Span.CallFile);
			Span.CallLine = ReadIntField(*RowObject, TEXT("callLine"), 0);
			Span.CallColumn = ReadIntField(*RowObject, TEXT("callCol"), 0);

			if (!Span.IsValid())
			{
				if (Sink)
				{
					Sink->Warning(TEXT("DSH9059"), MakeLineSpan(1), FText::Format(
						LOCTEXT("SpanTableBadSpan", "The DreamShader.SourceSpans entry '{Key}' of '{Asset}' names no file or a line below 1 and was skipped; that node cannot be navigated to."),
						FFormatNamedArguments{
							{ TEXT("Key"), FText::FromString(Pair.Key) },
							{ TEXT("Asset"), FText::FromString(AssetPathForMessages) } }));
				}
				continue;
			}

			Table.Spans.Add(ExpressionGuid, MoveTemp(Span));
		}

		return Table;
	}

	const FDreamShaderSourceSpan* FDreamShaderSourceSpanTable::FindForExpression(const UMaterialExpression* Expression) const
	{
		if (!Expression)
		{
			return nullptr;
		}
		// MaterialExpressionGuid, NOT GetMaterialExpressionId(): a parameter expression overrides that
		// accessor to return its PARAMETER guid, which is a different value and is shared by every
		// expression driving the same parameter name.
		return Spans.Find(Expression->MaterialExpressionGuid);
	}

	void FDreamShaderSourceSpanTable::FindByLine(
		const FString& File,
		const int32 Line,
		TArray<FGuid>& OutOnLine,
		TArray<FGuid>& OutCovering,
		TArray<FGuid>& OutViaCallSite) const
	{
		const FString NormalizedFile = UE::DreamShader::NormalizeSourceFilePath(File);

		// Sorted at the end rather than kept sorted: a TMap has no order to rely on, and "the first
		// match" must be the same answer on two runs or the same request would select a different node
		// each time. The key is the column for a match ON the line, the distance back to the span start
		// for a covering one; the guid breaks a tie, so the order is total.
		TArray<TPair<int32, FGuid>> OnLine;
		TArray<TPair<int32, FGuid>> Covering;
		TArray<TPair<int32, FGuid>> ViaCallSite;

		for (const TPair<FGuid, FDreamShaderSourceSpan>& Pair : Spans)
		{
			const FDreamShaderSourceSpan& Span = Pair.Value;
			// The two positions are matched against their OWN files. They are the same file today,
			// because the table records only one -- but a helper lives in an imported `.dsh` as often
			// as not, and the day the emitter records the call site's file this already does the
			// right thing instead of matching the helper's body against the caller's line numbers.
			const bool bSpanFileMatches = SpanFileMatches(Span.GetAbsoluteFilePath(), NormalizedFile);
			const bool bCallFileMatches = Span.HasCallSite()
				&& SpanFileMatches(Span.GetAbsoluteCallSiteFilePath(), NormalizedFile);
			if (!bSpanFileMatches && !bCallFileMatches)
			{
				continue;
			}

			if (bSpanFileMatches && Span.Line == Line)
			{
				OnLine.Emplace(Span.Column, Pair.Key);
			}
			else if (bCallFileMatches && Span.CallLine == Line)
			{
				// The node itself sits inside a helper, on a different line of (possibly) a different
				// file; the line the user pointed at is where that helper was CALLED. Contract §6.4.
				ViaCallSite.Emplace(Span.CallColumn, Pair.Key);
			}
			else if (bSpanFileMatches && Span.Line < Line && Line <= Span.Line + Span.Length)
			{
				// A span that starts on an earlier line can still REACH this one -- a `@custom` body,
				// or an expression broken across lines. How far it reaches cannot be known here: the
				// table records a length in characters, not an end line, and deciding it exactly would
				// mean reading the source. `Line + Length` is the sound upper bound (a line break costs
				// at least one character), which is loose but never excludes a span that does reach.
				//
				// A weak match on purpose, ordered nearest-first and consulted only when nothing
				// started on the line and nothing was called there. It is rarely needed: the IR gives
				// every SUB-expression its own span, so a continuation line of a multi-line expression
				// normally has nodes of its own that land in OnLine.
				Covering.Emplace(Line - Span.Line, Pair.Key);
			}
		}

		auto SortAndAppend = [](TArray<TPair<int32, FGuid>>& Matches, TArray<FGuid>& Out)
		{
			Matches.Sort([](const TPair<int32, FGuid>& A, const TPair<int32, FGuid>& B)
			{
				return A.Key != B.Key ? A.Key < B.Key : A.Value < B.Value;
			});
			Out.Reserve(Out.Num() + Matches.Num());
			for (const TPair<int32, FGuid>& Match : Matches)
			{
				Out.Add(Match.Value);
			}
		};

		SortAndAppend(OnLine, OutOnLine);
		SortAndAppend(Covering, OutCovering);
		SortAndAppend(ViaCallSite, OutViaCallSite);
	}

	// --------------------------------------------------------------------------------- guid -> span

	bool FindDreamShaderSourceSpanForExpression(
		const UMaterialExpression* Expression,
		FDreamShaderSourceSpan& OutSpan,
		UObject*& OutAsset)
	{
		OutAsset = nullptr;
		if (!Expression || !Expression->MaterialExpressionGuid.IsValid())
		{
			return false;
		}

		const FGuid& ExpressionGuid = Expression->MaterialExpressionGuid;
		bool bFound = false;

		ForEachLoadedGeneratedGraphAsset([&ExpressionGuid, &OutSpan, &OutAsset, &bFound](UObject* Asset)
		{
			const FString JsonText = GetAssetMetadataValue(Asset, FDreamShaderSourceSpanTable::GetMetadataKey());
			if (JsonText.IsEmpty())
			{
				return true; // keep looking
			}

			const FDreamShaderSourceSpanTable& Table = GetCachedSpanTable(Asset, JsonText);
			if (const FDreamShaderSourceSpan* Span = Table.Find(ExpressionGuid))
			{
				OutSpan = *Span;
				OutAsset = Asset;
				bFound = true;
				return false; // stop
			}
			return true;
		});

		return bFound;
	}

	bool OpenDreamShaderSourceLocation(const FString& File, const int32 Line, const int32 Column)
	{
		const FString AbsolutePath = NavigationStampedSourcePathToAbsolute(File);

		FLangDiagnosticSink Sink(AbsolutePath);
		if (AbsolutePath.IsEmpty() || !IFileManager::Get().FileExists(*AbsolutePath))
		{
			Sink.Error(TEXT("DSH9058"), MakeLineSpan(Line, Column), FText::Format(
				LOCTEXT("SourceFileMissing", "The source file '{File}' this node was generated from is not on disk any more, so it could not be opened; regenerate the asset, or restore the file."),
				FFormatNamedArguments{ { TEXT("File"), FText::FromString(AbsolutePath.IsEmpty() ? File : AbsolutePath) } }));
		}
		else if (!FDreamShaderEditorLaunchUtils::LaunchTextFileInPreferredEditor(AbsolutePath, FMath::Max(1, Line), FMath::Max(1, Column)))
		{
			Sink.Error(TEXT("DSH9058"), MakeLineSpan(Line, Column), FText::Format(
				LOCTEXT("SourceFileLaunchFailed", "No text editor could be launched for '{File}'; VSCode, the OS default editor and Notepad all refused."),
				FFormatNamedArguments{ { TEXT("File"), FText::FromString(AbsolutePath) } }));
		}

		if (const Lang::FLangDiagnostic* FirstError = Sink.FirstError())
		{
			UE_LOG(LogDreamShader, Error, TEXT("DreamShader navigation: %s"), *FLangDiagnosticSink::ToWireString(*FirstError));
			ShowNavigationNotification(FirstError->Message, SNotificationItem::CS_Fail);
			return false;
		}

		return true;
	}

	// ------------------------------------------------------------------------------ the context menu

	namespace
	{
		void PopulateSourceNavigationSubMenu(UToolMenu* InMenu, FDreamShaderSourceSpan Span)
		{
			if (!InMenu)
			{
				return;
			}

			FToolMenuSection& Section = InMenu->FindOrAddSection(
				TEXT("DreamShader.SourceNavigation"),
				LOCTEXT("SourceNavigationSectionLabel", "Source"));

			const FString AbsoluteFile = Span.GetAbsoluteFilePath();
			const FString FileLeafName = FPaths::GetCleanFilename(AbsoluteFile);
			const FString AbsoluteCallFile = Span.GetAbsoluteCallSiteFilePath();
			const FString CallFileLeafName = FPaths::GetCleanFilename(AbsoluteCallFile);

			Section.AddMenuEntry(
				TEXT("DreamShader.OpenSourceLine"),
				LOCTEXT("OpenSourceLineLabel", "Open Source Line"),
				FText::Format(
					LOCTEXT("OpenSourceLineTooltip", "Open {File} at line {Line}, column {Column} -- the DreamShader source this node was generated from."),
					FFormatNamedArguments{
						{ TEXT("File"), FText::FromString(FileLeafName) },
						{ TEXT("Line"), FText::AsNumber(Span.Line) },
						{ TEXT("Column"), FText::AsNumber(Span.Column) } }),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.OpenInExternalEditor")),
				FUIAction(FExecuteAction::CreateLambda([AbsoluteFile, Line = Span.Line, Column = Span.Column]()
				{
					OpenDreamShaderSourceLocation(AbsoluteFile, Line, Column);
				})));

			if (!Span.HasCallSite())
			{
				return;
			}

			// A node made while inlining a helper: its own span points inside the helper body, which
			// is very often a different file. The call site is the line the material's author wrote,
			// and is usually the one they are actually looking for -- so both are offered, in the
			// order "where the node came from" then "where it was asked for".
			Section.AddMenuEntry(
				TEXT("DreamShader.OpenCallSite"),
				LOCTEXT("OpenCallSiteLabel", "Open Call Site"),
				FText::Format(
					LOCTEXT("OpenCallSiteTooltip", "Open {File} at line {Line}, column {Column} -- the call that inlined the helper this node came from."),
					FFormatNamedArguments{
						{ TEXT("File"), FText::FromString(CallFileLeafName) },
						{ TEXT("Line"), FText::AsNumber(Span.CallLine) },
						{ TEXT("Column"), FText::AsNumber(Span.CallColumn) } }),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.OpenInExternalEditor")),
				FUIAction(FExecuteAction::CreateLambda([AbsoluteCallFile, Line = Span.CallLine, Column = Span.CallColumn]()
				{
					OpenDreamShaderSourceLocation(AbsoluteCallFile, Line, Column);
				})));
		}

		void PopulateGraphNodeContextMenu(UToolMenu* InMenu)
		{
			if (!InMenu)
			{
				return;
			}

			const UGraphNodeContextMenuContext* Context = InMenu->FindContext<UGraphNodeContextMenuContext>();
			if (!Context || !Context->Node)
			{
				return;
			}

			const UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(Context->Node.Get());
			if (!MaterialNode)
			{
				return;
			}

			const UMaterialExpression* Expression = MaterialNode->MaterialExpression;
			FDreamShaderSourceSpan Span;
			UObject* OwningAsset = nullptr;
			if (!FindDreamShaderSourceSpanForExpression(Expression, Span, OwningAsset))
			{
				// Absent, not disabled: a node nobody can navigate from is the ordinary case in a
				// hand-authored material, and a greyed-out DreamShader submenu on every node of every
				// material in the project would be noise on a scale nothing justifies.
				return;
			}

			FToolMenuSection& Section = InMenu->FindOrAddSection(TEXT("DreamShader"));
			Section.AddSubMenu(
				TEXT("DreamShader.SourceNavigationActions"),
				LOCTEXT("SourceNavigationMenuLabel", "DreamShader"),
				LOCTEXT("SourceNavigationMenuTooltip", "Jump from this generated node back to the DreamShader source it came from."),
				FNewToolMenuDelegate::CreateStatic(&PopulateSourceNavigationSubMenu, Span),
				false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings")));
		}
	}

	void FDreamShaderSourceNavigationMenu::Register()
	{
		if (GNavigationMenuStartupHandle.IsValid())
		{
			return;
		}

		GNavigationMenuStartupHandle = UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
			{
				FToolMenuOwnerScoped OwnerScope(DreamShaderNavigationMenuOwnerName);
				for (const TCHAR* MenuName : GraphNodeContextMenuNames)
				{
					// ExtendMenu before the menu exists is the supported order: SGraphEditorImpl
					// registers a node's context menu lazily, the first time one is opened, and
					// UToolMenus::RegisterMenu adopts an existing unregistered extension object
					// instead of replacing it (ToolMenus.cpp, RegisterMenu).
					if (UToolMenu* NodeMenu = UToolMenus::Get()->ExtendMenu(FName(MenuName)))
					{
						// Dynamic, because what belongs on the menu depends on which node was
						// right-clicked -- and a node with no source span must add nothing at all.
						NodeMenu->AddDynamicSection(
							TEXT("DreamShader.SourceNavigation"),
							FNewSectionConstructChoice(FNewToolMenuDelegate::CreateStatic(&PopulateGraphNodeContextMenu)));
					}
				}
			}));
	}

	void FDreamShaderSourceNavigationMenu::Unregister()
	{
		if (GNavigationMenuStartupHandle.IsValid())
		{
			UToolMenus::UnRegisterStartupCallback(GNavigationMenuStartupHandle);
			GNavigationMenuStartupHandle.Reset();
		}
		if (UObjectInitialized())
		{
			UToolMenus::UnregisterOwner(DreamShaderNavigationMenuOwnerName);
		}
		GSpanTableCache.Empty();
	}

	// --------------------------------------------------------------------------- the bridge request

	namespace
	{
		/** Why OpenMaterialEditorFor could not hand back an IMaterialEditor. Two codes, two causes. */
		enum class EOpenMaterialEditorResult : uint8
		{
			Opened,
			/** Nothing came back for the asset at all: it has no editor, or the editor refused. */
			NoEditor,
			/** Something opened, but not a material editor -- so there is no graph to jump around in. */
			NotAMaterialEditor,
		};

		/** Opens (or fronts) the material editor serving Asset. OutEditor is set only on Opened. */
		EOpenMaterialEditorResult OpenMaterialEditorFor(UObject* Asset, IMaterialEditor*& OutEditor)
		{
			OutEditor = nullptr;
			if (!GEditor || !Asset)
			{
				return EOpenMaterialEditorResult::NoEditor;
			}

			UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
			if (!AssetEditorSubsystem)
			{
				return EOpenMaterialEditorResult::NoEditor;
			}

			AssetEditorSubsystem->OpenEditorForAsset(Asset);

			IAssetEditorInstance* AssetEditor = AssetEditorSubsystem->FindEditorForAsset(Asset, /*bFocusIfOpen*/ true);
			if (!AssetEditor)
			{
				return EOpenMaterialEditorResult::NoEditor;
			}

			// The same clumsy-but-correct type check FMaterialEditorHelpers::OpenMaterialEditorForAsset
			// makes: IAssetEditorInstance has no RTTI hook, and a UMaterialInstance opens a material
			// INSTANCE editor, which is not an IMaterialEditor and has no graph to jump around in.
			if (AssetEditor->GetEditorName() != FName(TEXT("MaterialEditor")))
			{
				return EOpenMaterialEditorResult::NotAMaterialEditor;
			}

			OutEditor = static_cast<IMaterialEditor*>(AssetEditor);
			return EOpenMaterialEditorResult::Opened;
		}

		/**
		 * The expression carrying ExpressionGuid inside the editor's OWN material.
		 *
		 * Not the asset's expression: FMaterialEditor edits a transient duplicate (a preview material
		 * for a UMaterial, a wrapper material holding the duplicated function expressions for a
		 * UMaterialFunction), and JumpToExpression resolves a node through Expression->GraphNode,
		 * which only the duplicate has. The guid survives duplication, so it is the join.
		 */
		UMaterialExpression* FindExpressionInEditor(IMaterialEditor& MaterialEditor, const FGuid& ExpressionGuid)
		{
			UMaterial* EditorMaterial = Cast<UMaterial>(MaterialEditor.GetMaterialInterface());
			if (!EditorMaterial)
			{
				return nullptr;
			}

			for (const TObjectPtr<UMaterialExpression>& Expression : EditorMaterial->GetExpressions())
			{
				if (Expression && Expression->MaterialExpressionGuid == ExpressionGuid)
				{
					return Expression.Get();
				}
			}

			return nullptr;
		}

		/**
		 * One candidate asset for a reveal-node request, holding the MATCHES and not the table.
		 *
		 * Deliberately not a pointer into the parsed-table cache: that cache is a TMap, and the very
		 * next asset examined can make it rehash, which would leave every pointer taken before it
		 * dangling. The three guid arrays are all this needs anyway.
		 */
		struct FRevealCandidate
		{
			UObject* Asset = nullptr;
			TArray<FGuid> OnLine;
			TArray<FGuid> Covering;
			TArray<FGuid> ViaCallSite;

			bool HasMatch() const { return !OnLine.IsEmpty() || !ViaCallSite.IsEmpty() || !Covering.IsEmpty(); }

			/** Strongest first: a node that STARTS on the line, then an inlined one whose call is there. */
			const TArray<FGuid>& BestMatches() const
			{
				if (!OnLine.IsEmpty())
				{
					return OnLine;
				}
				return ViaCallSite.IsEmpty() ? Covering : ViaCallSite;
			}
		};
	}

	FDreamShaderRevealNodeResult HandleDreamShaderRevealNodeRequest(const FJsonObject& Request)
	{
		FDreamShaderRevealNodeResult Result;

		FString RequestedFile;
		Request.TryGetStringField(TEXT("file"), RequestedFile);

		double RequestedLineValue = 0.0;
		Request.TryGetNumberField(TEXT("line"), RequestedLineValue);
		const int32 RequestedLine = FMath::RoundToInt32(RequestedLineValue);

		const FString NormalizedFile = RequestedFile.IsEmpty()
			? FString()
			: UE::DreamShader::NormalizeSourceFilePath(RequestedFile);

		FLangDiagnosticSink Sink(NormalizedFile);

		auto Fail = [&Result, &Sink, &NormalizedFile](const TCHAR* Code, const FLangSpan& Span, const FText& Message)
		{
			Sink.Error(Code, Span, Message);
			Result.bOk = false;
			Result.Message = Message;
			Result.Diagnostics = ToDiagnosticRecords(Sink, NormalizedFile, Result.AssetPath);
			return Result;
		};

		if (RequestedFile.IsEmpty() || RequestedLine < 1)
		{
			return Fail(TEXT("DSH9050"), MakeLineSpan(1), FText::Format(
				LOCTEXT("RevealNodeBadRequest", "reveal-node needs a non-empty 'file' and a 'line' of 1 or more; got file '{File}' and line {Line}."),
				FFormatNamedArguments{
					{ TEXT("File"), FText::FromString(RequestedFile) },
					{ TEXT("Line"), FText::AsNumber(RequestedLine) } }));
		}

		// Which assets came from this source. The stamp is the authority (it is what the divergence
		// check, the browser and the provenance actions all key on); it is project-relative, so both
		// sides go through the same normalizer before they are compared.
		TArray<UObject*> StampedAssets;
		ForEachLoadedGeneratedGraphAsset([&StampedAssets, &NormalizedFile](UObject* Asset)
		{
			const FString StampedSource = NavigationStampedSourcePathToAbsolute(GetGeneratedAssetSourceFile(Asset));
			if (!StampedSource.IsEmpty() && StampedSource.Equals(NormalizedFile, ESearchCase::IgnoreCase))
			{
				StampedAssets.Add(Asset);
			}
			return true;
		});

		if (StampedAssets.IsEmpty())
		{
			return Fail(TEXT("DSH9051"), MakeLineSpan(RequestedLine), FText::Format(
				LOCTEXT("RevealNodeNoAsset", "No asset loaded in this editor was generated from '{File}', so there is no graph to reveal a node in; compile the file first, or open one of its assets once so the editor knows about it."),
				FFormatNamedArguments{ { TEXT("File"), FText::FromString(NormalizedFile) } }));
		}

		// Deterministic, because two assets may both match the line and the answer must not depend on
		// object iteration order.
		StampedAssets.Sort([](const UObject& A, const UObject& B)
		{
			return A.GetPathName() < B.GetPathName();
		});

		TArray<FRevealCandidate> Candidates;
		int32 TablesFound = 0;
		for (UObject* Asset : StampedAssets)
		{
			const FString JsonText = GetAssetMetadataValue(Asset, FDreamShaderSourceSpanTable::GetMetadataKey());
			if (JsonText.IsEmpty())
			{
				continue;
			}
			++TablesFound;

			FRevealCandidate Candidate;
			Candidate.Asset = Asset;
			GetCachedSpanTable(Asset, JsonText).FindByLine(
				NormalizedFile, RequestedLine, Candidate.OnLine, Candidate.Covering, Candidate.ViaCallSite);
			if (Candidate.HasMatch())
			{
				Candidates.Add(MoveTemp(Candidate));
			}
		}

		if (TablesFound == 0)
		{
			Result.AssetPath = StampedAssets[0]->GetPathName();
			return Fail(TEXT("DSH9052"), MakeLineSpan(RequestedLine), FText::Format(
				LOCTEXT("RevealNodeNoSpanTable", "The {Count} asset(s) generated from '{File}' carry no DreamShader.SourceSpans metadata, so no node on them can be located; they predate node navigation -- rebuild them with -Force."),
				FFormatNamedArguments{
					{ TEXT("Count"), FText::AsNumber(StampedAssets.Num()) },
					{ TEXT("File"), FText::FromString(NormalizedFile) } }));
		}

		if (Candidates.IsEmpty())
		{
			return Fail(TEXT("DSH9054"), MakeLineSpan(RequestedLine), FText::Format(
				LOCTEXT("RevealNodeNoSpanOnLine", "No node generated from '{File}' has a source span on line {Line}; the line produced no graph node (a declaration, a comment, or a statement that folded away)."),
				FFormatNamedArguments{
					{ TEXT("File"), FText::FromString(NormalizedFile) },
					{ TEXT("Line"), FText::AsNumber(RequestedLine) } }));
		}

		// A node that STARTS on the line beats one that was merely inlined from a call there, whichever
		// asset each is in -- so the strength of the match, not the asset order, picks the winner.
		Candidates.Sort([](const FRevealCandidate& A, const FRevealCandidate& B)
		{
			auto Rank = [](const FRevealCandidate& Candidate)
			{
				return !Candidate.OnLine.IsEmpty() ? 0 : (!Candidate.ViaCallSite.IsEmpty() ? 1 : 2);
			};
			const int32 RankA = Rank(A);
			const int32 RankB = Rank(B);
			return RankA != RankB ? RankA < RankB : A.Asset->GetPathName() < B.Asset->GetPathName();
		});

		const FRevealCandidate& Winner = Candidates[0];
		Result.AssetPath = Winner.Asset->GetPathName();
		Result.ExpressionGuids = Winner.BestMatches();

		// The instance half of a ThinCustom product, when the graph is on a hidden base: the client
		// asked about a source file, and the asset it knows by name is the instance.
		if (UMaterial* BaseMaterial = Cast<UMaterial>(Winner.Asset))
		{
			for (TObjectIterator<UMaterialInstance> It; It; ++It)
			{
				UMaterialInstance* Instance = *It;
				if (!IsValid(Instance) || Instance->GetOutermost() == GetTransientPackage())
				{
					continue;
				}
				// Compared through a raw UMaterialInterface*, so the derived-to-base conversion is the
				// built-in pointer one and no TObjectPtr comparison operator has to be chosen.
				UMaterialInterface* ParentInterface = Instance->Parent.Get();
				if (ParentInterface != nullptr && ParentInterface == BaseMaterial)
				{
					Result.InstanceAssetPath = Instance->GetPathName();
					break;
				}
			}
		}

		IMaterialEditor* MaterialEditor = nullptr;
		const EOpenMaterialEditorResult OpenResult = OpenMaterialEditorFor(Winner.Asset, MaterialEditor);
		if (OpenResult == EOpenMaterialEditorResult::NoEditor)
		{
			return Fail(TEXT("DSH9055"), MakeLineSpan(RequestedLine), FText::Format(
				LOCTEXT("RevealNodeEditorRefused", "The material editor would not open for '{Asset}', so the node could not be revealed."),
				FFormatNamedArguments{ { TEXT("Asset"), FText::FromString(Result.AssetPath) } }));
		}
		if (OpenResult == EOpenMaterialEditorResult::NotAMaterialEditor || !MaterialEditor)
		{
			return Fail(TEXT("DSH9056"), MakeLineSpan(RequestedLine), FText::Format(
				LOCTEXT("RevealNodeNotMaterialEditor", "'{Asset}' opened in an editor that is not the material editor, which has no node graph to select in; only a node of a Material or a Material Function can be revealed."),
				FFormatNamedArguments{ { TEXT("Asset"), FText::FromString(Result.AssetPath) } }));
		}

		UMaterialExpression* TargetExpression = FindExpressionInEditor(*MaterialEditor, Result.ExpressionGuids[0]);
		if (!TargetExpression)
		{
			return Fail(TEXT("DSH9057"), MakeLineSpan(RequestedLine), FText::Format(
				LOCTEXT("RevealNodeExpressionMissing", "The expression {Guid} recorded for line {Line} is not in '{Asset}' any more, so the node could not be selected; the asset changed since it was generated -- rebuild it from its source."),
				FFormatNamedArguments{
					{ TEXT("Guid"), FText::FromString(Result.ExpressionGuids[0].ToString(EGuidFormats::DigitsWithHyphens)) },
					{ TEXT("Line"), FText::AsNumber(RequestedLine) },
					{ TEXT("Asset"), FText::FromString(Result.AssetPath) } }));
		}

		MaterialEditor->FocusWindow();
		MaterialEditor->JumpToExpression(TargetExpression);

		Result.bOk = true;
		Result.Message = FText::Format(
			LOCTEXT("RevealNodeOk", "Revealed {Count} node(s) for {File}({Line}) in '{Asset}'."),
			FFormatNamedArguments{
				{ TEXT("Count"), FText::AsNumber(Result.ExpressionGuids.Num()) },
				{ TEXT("File"), FText::FromString(NormalizedFile) },
				{ TEXT("Line"), FText::AsNumber(RequestedLine) },
				{ TEXT("Asset"), FText::FromString(Result.AssetPath) } });
		return Result;
	}

	void WriteDreamShaderRevealNodeResponse(
		const FString& RequestId,
		const FString& ResponseDirectory,
		const FDreamShaderRevealNodeResult& Result,
		const double DurationMs)
	{
		// Same rule the bridge's RespondTo follows: no id means a client that is not listening, and
		// writing a response anyway litters the directory with files nobody ever deletes.
		if (RequestId.IsEmpty())
		{
			return;
		}

		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		Writer->WriteObjectStart();
		// protocol + version: `protocol` is the bridge response envelope's (RespondTo writes it on
		// every answer); `version` is the diagnostics payload's, which is what the extra fields below
		// are versioned by. Both are 1 and they are bumped for different reasons.
		Writer->WriteValue(TEXT("protocol"), 1);
		Writer->WriteValue(TEXT("version"), 1);
		Writer->WriteValue(TEXT("requestId"), RequestId);
		Writer->WriteValue(TEXT("ok"), Result.bOk);
		Writer->WriteValue(TEXT("durationMs"), static_cast<int32>(DurationMs));
		Writer->WriteValue(TEXT("message"), ToInvariantWireString(Result.Message));
		if (!Result.AssetPath.IsEmpty())
		{
			Writer->WriteValue(TEXT("assetPath"), Result.AssetPath);
		}
		if (!Result.InstanceAssetPath.IsEmpty())
		{
			Writer->WriteValue(TEXT("instanceAssetPath"), Result.InstanceAssetPath);
		}
		Writer->WriteArrayStart(TEXT("expressions"));
		for (const FGuid& ExpressionGuid : Result.ExpressionGuids)
		{
			// EGuidFormats::DigitsWithHyphens: the spelling the emitter writes the table's KEYS in
			// (DreamShaderIREmitter.cpp, WriteDreamShaderSourceSpans), so a client can match a guid
			// from a response against a guid from the metadata without normalising either. Reading is
			// forgiving -- FGuid::Parse takes every standard spelling -- but writing is not the place
			// to be creative.
			Writer->WriteValue(ExpressionGuid.ToString(EGuidFormats::DigitsWithHyphens));
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("diagnostics"));
		for (const FDreamShaderDiagnosticRecord& Record : Result.Diagnostics)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("file"), Record.FilePath);
			Writer->WriteValue(TEXT("line"), FMath::Max(1, Record.Line));
			Writer->WriteValue(TEXT("column"), FMath::Max(1, Record.Column));
			Writer->WriteValue(TEXT("severity"), Record.Severity);
			if (!Record.Code.IsEmpty())
			{
				Writer->WriteValue(TEXT("code"), Record.Code);
			}
			if (!Record.Stage.IsEmpty())
			{
				Writer->WriteValue(TEXT("stage"), Record.Stage);
			}
			if (!Record.AssetPath.IsEmpty())
			{
				Writer->WriteValue(TEXT("assetPath"), Record.AssetPath);
			}
			// Invariant English, never the localised display text: a client on a zh-Hans editor has
			// to be able to match on this.
			Writer->WriteValue(TEXT("message"), ToInvariantWireString(Record.Message));
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
		Writer->Close();

		// Beside the target then renamed, so appearing and being complete are the same event for a
		// client that polls for the file. Same recipe as the bridge's WriteFileAtomically.
		IFileManager::Get().MakeDirectory(*ResponseDirectory, true);
		const FString ResponsePath = FPaths::Combine(ResponseDirectory, RequestId + TEXT(".json"));
		const FString TemporaryPath = ResponsePath + TEXT(".tmp");
		if (!FFileHelper::SaveStringToFile(Text, *TemporaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return;
		}
		if (!IFileManager::Get().Move(*ResponsePath, *TemporaryPath, /*bReplace*/ true))
		{
			IFileManager::Get().Delete(*TemporaryPath);
		}
	}
}

#undef LOCTEXT_NAMESPACE
