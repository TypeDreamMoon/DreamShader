// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DumpDreamShaderIRText / DumpDreamShaderIRJson. See IRDump.h for the three rules the format obeys.
//
// The one thing worth restating here: every ordering decision in this file is deliberate.
// Operands and named inputs keep their STORED order, because for SetMaterialAttributes the input
// order is the meaning (Prop::AttributeSetTypes is "in Inputs order") and a dump that sorted them
// would lie. Properties are SORTED by name, because their order carries nothing and sorting keeps
// a golden from churning when the builder happens to add a property in a different place.

#include "IR/IRDump.h"

#include "IRJson.h"
#include "IR/IRCoreOps.h"
#include "IR/IRTypes.h"

#include "Misc/CString.h"
#include "Misc/Paths.h"

namespace UE::DreamShader::IR
{
	namespace Private
	{
		static const TCHAR* const GIRDumpSchemaName = TEXT("dreamshader-ir");
		static constexpr int32 GIRDumpSchemaVersion = 1;

		/** The leaf name of a path: what makes a golden portable between machines. */
		static FString CleanFileName(const FString& Path)
		{
			return Path.IsEmpty() ? FString() : FPaths::GetCleanFilename(Path);
		}

		/** True when a node came from a different file than the module's own (an included header). */
		static bool IsForeignFile(const FString& ModuleFile, const FString& NodeFile)
		{
			if (NodeFile.IsEmpty())
			{
				return false;
			}
			// Paths, so IgnoreCase: the same header reached through two spellings is one file.
			return !CleanFileName(NodeFile).Equals(CleanFileName(ModuleFile), ESearchCase::IgnoreCase);
		}

		static FString EscapeQuoted(const FString& Value)
		{
			// The value has already been through FIRPropertyValue::ToString(), which escaped the
			// backslashes and the line breaks. Only the quote is left.
			return Value.Replace(TEXT("\""), TEXT("\\\""), ESearchCase::CaseSensitive);
		}

		/** A property as it appears after `Name=`: quoted for the text kinds, bare for the numeric ones. */
		static FString RenderPropertyValue(const FIRPropertyValue& Value)
		{
			switch (Value.Kind)
			{
			case EIRPropertyKind::String:
			case EIRPropertyKind::Name:
			case EIRPropertyKind::Enum:
			case EIRPropertyKind::Object:
			case EIRPropertyKind::StringList:
				return FString::Printf(TEXT("\"%s\""), *EscapeQuoted(Value.ToString()));
			default:
				return Value.ToString();
			}
		}

		static FString RenderValueRef(const FIRValue Value)
		{
			if (!Value.IsValid())
			{
				// An absent slot. `TextureSample` always carries four operands and leaves the
				// sampler or the level as FIRValue::None() when the source did not give one
				// (CONTRACT §6.13 #14); `_` keeps the slot visible so the golden shows WHICH
				// operand is missing rather than silently shortening the list.
				return TEXT("_");
			}
			if (Value.Output == 0)
			{
				return FString::Printf(TEXT("%%%d"), Value.Node);
			}
			return FString::Printf(TEXT("%%%d#%d"), Value.Node, Value.Output);
		}

		/** Properties sorted by name, case-sensitively, as `Name=Value` fragments. */
		static void CollectSortedProperties(const FIRNode& Node, TArray<FString>& OutFragments)
		{
			OutFragments.Reset();

			TArray<int32> Order;
			Order.Reserve(Node.Properties.Num());
			for (int32 Index = 0; Index < Node.Properties.Num(); ++Index)
			{
				Order.Add(Index);
			}

			Order.Sort([&Node](const int32 Left, const int32 Right)
			{
				const int32 Comparison = FCString::Strcmp(*Node.Properties[Left].Name, *Node.Properties[Right].Name);
				return Comparison != 0 ? (Comparison < 0) : (Left < Right);
			});

			for (const int32 Index : Order)
			{
				const FIRProperty& Property = Node.Properties[Index];
				OutFragments.Add(FString::Printf(
					TEXT("%s=%s"),
					*Property.Name,
					*RenderPropertyValue(Property.Value)));
			}
		}

		/** `Key=Value` for every setting, sorted by key; TMap iteration order is not stable. */
		static FString RenderSettings(const TMap<FString, FString>& Settings)
		{
			TArray<FString> Keys;
			Settings.GetKeys(Keys);
			Keys.Sort([](const FString& Left, const FString& Right)
			{
				return FCString::Strcmp(*Left, *Right) < 0;
			});

			FString Result;
			for (int32 Index = 0; Index < Keys.Num(); ++Index)
			{
				if (Index > 0)
				{
					Result += TEXT(", ");
				}
				const FString* Value = Settings.Find(Keys[Index]);
				Result += FString::Printf(TEXT("%s=%s"), *Keys[Index], Value != nullptr ? **Value : TEXT(""));
			}
			return Result;
		}

		static FString RenderOutputTypes(const FIRNode& Node)
		{
			if (Node.Outputs.Num() == 0)
			{
				return FString();
			}

			const bool bSingleUnnamed = Node.Outputs.Num() == 1
				&& (Node.OutputNames.Num() == 0 || Node.OutputNames[0].IsEmpty());
			if (bSingleUnnamed)
			{
				return Node.Outputs[0].ToString();
			}

			FString Result = TEXT("[");
			for (int32 Index = 0; Index < Node.Outputs.Num(); ++Index)
			{
				if (Index > 0)
				{
					Result += TEXT(", ");
				}
				if (Node.OutputNames.IsValidIndex(Index) && !Node.OutputNames[Index].IsEmpty())
				{
					Result += Node.OutputNames[Index];
					Result += TEXT(":");
				}
				Result += Node.Outputs[Index].ToString();
			}
			Result += TEXT("]");
			return Result;
		}

		/** `Op`, or `Op[ClassName]` when the node names one. */
		static FString RenderOpName(const FIRNode& Node)
		{
			if (Node.ClassName.IsEmpty())
			{
				return LexToString(Node.Op);
			}
			return FString::Printf(TEXT("%s[%s]"), LexToString(Node.Op), *Node.ClassName);
		}

		static FString RenderNodeLine(
			const FIRModule& Module,
			const FIRGraph& Graph,
			const int32 NodeIndex,
			const FIRNode& Node)
		{
			FString Line = FString::Printf(TEXT("  %%%d = %s("), NodeIndex, *RenderOpName(Node));

			bool bFirstArgument = true;
			const auto AppendArgument = [&Line, &bFirstArgument](const FString& Argument)
			{
				if (!bFirstArgument)
				{
					Line += TEXT(", ");
				}
				bFirstArgument = false;
				Line += Argument;
			};

			for (const FIRValue& Operand : Node.Operands)
			{
				AppendArgument(RenderValueRef(Operand));
			}
			for (const FIRInput& Input : Node.Inputs)
			{
				AppendArgument(FString::Printf(TEXT("%s=%s"), *Input.Pin, *RenderValueRef(Input.Value)));
			}

			TArray<FString> PropertyFragments;
			CollectSortedProperties(Node, PropertyFragments);
			for (const FString& Fragment : PropertyFragments)
			{
				AppendArgument(Fragment);
			}

			Line += TEXT(")");

			const FString OutputTypes = RenderOutputTypes(Node);
			if (!OutputTypes.IsEmpty())
			{
				Line += FString::Printf(TEXT(" : %s"), *OutputTypes);
			}

			Line += FString::Printf(TEXT(" @L%d:C%d"), Node.Source.Span.Line, Node.Source.Span.Column);

			// `in` names the file the span lies in (an included helper's header); `from` names the
			// file the CALL was written in, which differs exactly when the helper was included.
			if (IsForeignFile(Module.SourceFilePath, Node.Source.File))
			{
				Line += FString::Printf(TEXT(" in \"%s\""), *CleanFileName(Node.Source.File));
			}
			if (Node.Source.HasCallSite())
			{
				Line += FString::Printf(TEXT(" via L%d:C%d"), Node.Source.CallSite.Line, Node.Source.CallSite.Column);
				if (IsForeignFile(Module.SourceFilePath, Node.Source.CallSiteFile))
				{
					Line += FString::Printf(TEXT(" from \"%s\""), *CleanFileName(Node.Source.CallSiteFile));
				}
			}

			if (Node.Region != INDEX_NONE)
			{
				if (Graph.Regions.IsValidIndex(Node.Region) && !Graph.Regions[Node.Region].Name.IsEmpty())
				{
					Line += FString::Printf(TEXT(" {%s}"), *Graph.Regions[Node.Region].Name);
				}
				else
				{
					// An unnamed or dangling region still shows, by index: the validator reports
					// the dangling case and a dump that hid it would make that report confusing.
					Line += FString::Printf(TEXT(" {#%d}"), Node.Region);
				}
			}

			if (!Node.DebugName.IsEmpty())
			{
				Line += FString::Printf(TEXT(" \"%s\""), *EscapeQuoted(Node.DebugName));
			}

			return Line;
		}

		static FString RenderIndexList(const TArray<int32>& Indices)
		{
			FString Result;
			for (int32 Index = 0; Index < Indices.Num(); ++Index)
			{
				if (Index > 0)
				{
					Result += TEXT(", ");
				}
				Result += FString::Printf(TEXT("%%%d"), Indices[Index]);
			}
			return Result;
		}

		static void WriteSourceRefJson(FIRJsonWriter& Writer, const FIRModule& Module, const FIRSourceRef& Source)
		{
			Writer.Key(TEXT("source"));
			Writer.BeginObject();
			Writer.KeyInt(TEXT("line"), Source.Span.Line);
			Writer.KeyInt(TEXT("column"), Source.Span.Column);
			Writer.KeyInt(TEXT("length"), Source.Span.Length);
			if (IsForeignFile(Module.SourceFilePath, Source.File))
			{
				Writer.KeyString(TEXT("file"), CleanFileName(Source.File));
			}
			if (Source.HasCallSite())
			{
				Writer.KeyInt(TEXT("callLine"), Source.CallSite.Line);
				Writer.KeyInt(TEXT("callColumn"), Source.CallSite.Column);
				Writer.KeyInt(TEXT("callLength"), Source.CallSite.Length);
				if (IsForeignFile(Module.SourceFilePath, Source.CallSiteFile))
				{
					Writer.KeyString(TEXT("callFile"), CleanFileName(Source.CallSiteFile));
				}
			}
			Writer.EndObject();
		}

		static const TCHAR* LexPropertyKindForDump(const EIRPropertyKind Kind)
		{
			switch (Kind)
			{
			case EIRPropertyKind::Bool:       return TEXT("Bool");
			case EIRPropertyKind::Int:        return TEXT("Int");
			case EIRPropertyKind::Float:      return TEXT("Float");
			case EIRPropertyKind::Float4:     return TEXT("Float4");
			case EIRPropertyKind::String:     return TEXT("String");
			case EIRPropertyKind::Name:       return TEXT("Name");
			case EIRPropertyKind::Enum:       return TEXT("Enum");
			case EIRPropertyKind::Object:     return TEXT("Object");
			case EIRPropertyKind::StringList: return TEXT("StringList");
			}
			return TEXT("Float");
		}

		static void WriteNodeJson(
			FIRJsonWriter& Writer,
			const FIRModule& Module,
			const int32 NodeIndex,
			const FIRNode& Node)
		{
			Writer.BeginObject();
			Writer.KeyInt(TEXT("index"), NodeIndex);
			Writer.KeyString(TEXT("op"), LexToString(Node.Op));

			if (!Node.ClassName.IsEmpty())
			{
				Writer.KeyString(TEXT("class"), Node.ClassName);
			}
			if (Node.CatalogIndex != INDEX_NONE)
			{
				Writer.KeyInt(TEXT("catalogIndex"), Node.CatalogIndex);
			}
			if (!Node.DebugName.IsEmpty())
			{
				Writer.KeyString(TEXT("debugName"), Node.DebugName);
			}
			if (Node.Region != INDEX_NONE)
			{
				Writer.KeyInt(TEXT("region"), Node.Region);
			}
			if (!Node.DedupeKey.IsEmpty())
			{
				Writer.KeyString(TEXT("dedupeKey"), Node.DedupeKey);
			}

			if (Node.Operands.Num() > 0)
			{
				Writer.Key(TEXT("operands"));
				Writer.BeginArray();
				for (const FIRValue& Operand : Node.Operands)
				{
					Writer.BeginObject();
					Writer.KeyInt(TEXT("node"), Operand.Node);
					Writer.KeyInt(TEXT("output"), Operand.Output);
					Writer.EndObject();
				}
				Writer.EndArray();
			}

			if (Node.Inputs.Num() > 0)
			{
				Writer.Key(TEXT("inputs"));
				Writer.BeginArray();
				for (const FIRInput& Input : Node.Inputs)
				{
					Writer.BeginObject();
					Writer.KeyString(TEXT("pin"), Input.Pin);
					Writer.KeyInt(TEXT("node"), Input.Value.Node);
					Writer.KeyInt(TEXT("output"), Input.Value.Output);
					Writer.EndObject();
				}
				Writer.EndArray();
			}

			if (Node.Properties.Num() > 0)
			{
				TArray<int32> Order;
				Order.Reserve(Node.Properties.Num());
				for (int32 Index = 0; Index < Node.Properties.Num(); ++Index)
				{
					Order.Add(Index);
				}
				Order.Sort([&Node](const int32 Left, const int32 Right)
				{
					const int32 Comparison = FCString::Strcmp(*Node.Properties[Left].Name, *Node.Properties[Right].Name);
					return Comparison != 0 ? (Comparison < 0) : (Left < Right);
				});

				Writer.Key(TEXT("properties"));
				Writer.BeginArray();
				for (const int32 Index : Order)
				{
					const FIRProperty& Property = Node.Properties[Index];
					Writer.BeginObject();
					Writer.KeyString(TEXT("name"), Property.Name);
					Writer.KeyString(TEXT("kind"), LexPropertyKindForDump(Property.Value.Kind));
					Writer.KeyString(TEXT("value"), Property.Value.ToString());
					Writer.EndObject();
				}
				Writer.EndArray();
			}

			if (Node.Outputs.Num() > 0)
			{
				Writer.Key(TEXT("outputs"));
				Writer.BeginArray();
				for (int32 Index = 0; Index < Node.Outputs.Num(); ++Index)
				{
					Writer.BeginObject();
					if (Node.OutputNames.IsValidIndex(Index) && !Node.OutputNames[Index].IsEmpty())
					{
						Writer.KeyString(TEXT("name"), Node.OutputNames[Index]);
					}
					Writer.KeyString(TEXT("type"), Node.Outputs[Index].ToString());
					Writer.EndObject();
				}
				Writer.EndArray();
			}

			WriteSourceRefJson(Writer, Module, Node.Source);
			Writer.EndObject();
		}
	}

	FString DumpDreamShaderIRText(const FIRModule& Module)
	{
		TArray<FString> Lines;

		Lines.Add(FString::Printf(TEXT("module \"%s\""), *Private::CleanFileName(Module.SourceFilePath)));
		for (const FString& Include : Module.Includes)
		{
			Lines.Add(FString::Printf(TEXT("include \"%s\""), *Private::CleanFileName(Include)));
		}

		for (int32 ProductIndex = 0; ProductIndex < Module.Products.Num(); ++ProductIndex)
		{
			const FIRProduct& Product = Module.Products[ProductIndex];
			const FIRGraph& Graph = Product.Graph;

			Lines.Add(FString());

			FString Header = FString::Printf(
				TEXT("product %d %s \"%s\" backend=%s"),
				ProductIndex,
				LexToString(Product.Kind),
				*Product.Name,
				LexToString(Product.Backend));

			if (Product.Settings.Num() > 0)
			{
				Header += FString::Printf(TEXT(" settings{%s}"), *Private::RenderSettings(Product.Settings));
			}
			Lines.Add(Header);

			if (!Product.AssetPathOverride.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("  path \"%s\""), *Product.AssetPathOverride));
			}
			if (!Product.LibraryPath.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("  library \"%s\""), *Product.LibraryPath));
			}
			if (!Product.Description.IsEmpty())
			{
				Lines.Add(FString::Printf(
					TEXT("  desc \"%s\""),
					*Private::EscapeQuoted(FIRPropertyValue::MakeString(Product.Description).ToString())));
			}

			for (int32 RegionIndex = 0; RegionIndex < Graph.Regions.Num(); ++RegionIndex)
			{
				const FIRRegion& Region = Graph.Regions[RegionIndex];
				Lines.Add(FString::Printf(
					TEXT("  region %d \"%s\" parent=%d"),
					RegionIndex,
					*Private::EscapeQuoted(Region.Name),
					Region.Parent));
			}

			for (const FIRLayoutHint& Hint : Graph.LayoutHints)
			{
				FString HintLine = FString::Printf(TEXT("  layout %s"), *Hint.Kind);
				if (!Hint.Var.IsEmpty())
				{
					HintLine += FString::Printf(TEXT(" var=%s"), *Hint.Var);
				}
				if (!Hint.Name.IsEmpty())
				{
					HintLine += FString::Printf(TEXT(" name=\"%s\""), *Private::EscapeQuoted(Hint.Name));
				}
				HintLine += FString::Printf(TEXT(" x=%d y=%d"), Hint.X, Hint.Y);
				if (Hint.bHasSize)
				{
					HintLine += FString::Printf(TEXT(" w=%d h=%d"), Hint.W, Hint.H);
				}
				Lines.Add(HintLine);
			}

			for (int32 NodeIndex = 0; NodeIndex < Graph.Nodes.Num(); ++NodeIndex)
			{
				Lines.Add(Private::RenderNodeLine(Module, Graph, NodeIndex, Graph.Nodes[NodeIndex]));
			}

			if (Graph.Sink != INDEX_NONE)
			{
				Lines.Add(FString::Printf(TEXT("  sink %%%d"), Graph.Sink));
			}
			if (Graph.FunctionInputs.Num() > 0)
			{
				Lines.Add(FString::Printf(TEXT("  inputs %s"), *Private::RenderIndexList(Graph.FunctionInputs)));
			}
			if (Graph.FunctionOutputs.Num() > 0)
			{
				Lines.Add(FString::Printf(TEXT("  outputs %s"), *Private::RenderIndexList(Graph.FunctionOutputs)));
			}
		}

		// Joined with `\n` and finished with one: the goldens are compared as text and a platform
		// line ending would make the same module fail on the other platform.
		FString Result;
		for (const FString& Line : Lines)
		{
			Result += Line;
			Result += TEXT("\n");
		}
		return Result;
	}

	FString DumpDreamShaderIRJson(const FIRModule& Module)
	{
		Private::FIRJsonWriter Writer;

		Writer.BeginObject();
		Writer.KeyString(TEXT("schema"), Private::GIRDumpSchemaName);
		Writer.KeyInt(TEXT("version"), Private::GIRDumpSchemaVersion);
		Writer.KeyString(TEXT("source"), Private::CleanFileName(Module.SourceFilePath));

		Writer.Key(TEXT("includes"));
		Writer.BeginArray();
		for (const FString& Include : Module.Includes)
		{
			Writer.ValueString(Private::CleanFileName(Include));
		}
		Writer.EndArray();

		Writer.Key(TEXT("products"));
		Writer.BeginArray();
		for (int32 ProductIndex = 0; ProductIndex < Module.Products.Num(); ++ProductIndex)
		{
			const FIRProduct& Product = Module.Products[ProductIndex];
			const FIRGraph& Graph = Product.Graph;

			Writer.BeginObject();
			Writer.KeyInt(TEXT("index"), ProductIndex);
			Writer.KeyString(TEXT("kind"), LexToString(Product.Kind));
			Writer.KeyString(TEXT("name"), Product.Name);
			Writer.KeyString(TEXT("backend"), LexToString(Product.Backend));

			if (!Product.AssetPathOverride.IsEmpty())
			{
				Writer.KeyString(TEXT("assetPath"), Product.AssetPathOverride);
			}
			if (!Product.LibraryPath.IsEmpty())
			{
				Writer.KeyString(TEXT("library"), Product.LibraryPath);
			}
			if (!Product.Description.IsEmpty())
			{
				Writer.KeyString(TEXT("description"), Product.Description);
			}
			if (Product.BoundFunctionIndex != INDEX_NONE)
			{
				Writer.KeyInt(TEXT("boundFunction"), Product.BoundFunctionIndex);
			}

			if (Product.Settings.Num() > 0)
			{
				TArray<FString> Keys;
				Product.Settings.GetKeys(Keys);
				Keys.Sort([](const FString& Left, const FString& Right)
				{
					return FCString::Strcmp(*Left, *Right) < 0;
				});

				Writer.Key(TEXT("settings"));
				Writer.BeginObject();
				for (const FString& Key : Keys)
				{
					const FString* Value = Product.Settings.Find(Key);
					Writer.KeyString(*Key, Value != nullptr ? *Value : FString());
				}
				Writer.EndObject();
			}

			if (Graph.Regions.Num() > 0)
			{
				Writer.Key(TEXT("regions"));
				Writer.BeginArray();
				for (const FIRRegion& Region : Graph.Regions)
				{
					Writer.BeginObject();
					Writer.KeyString(TEXT("name"), Region.Name);
					Writer.KeyInt(TEXT("parent"), Region.Parent);
					Writer.KeyInt(TEXT("line"), Region.Span.Line);
					Writer.KeyInt(TEXT("column"), Region.Span.Column);
					Writer.EndObject();
				}
				Writer.EndArray();
			}

			if (Graph.LayoutHints.Num() > 0)
			{
				Writer.Key(TEXT("layoutHints"));
				Writer.BeginArray();
				for (const FIRLayoutHint& Hint : Graph.LayoutHints)
				{
					Writer.BeginObject();
					Writer.KeyString(TEXT("kind"), Hint.Kind);
					if (!Hint.Var.IsEmpty())
					{
						Writer.KeyString(TEXT("var"), Hint.Var);
					}
					if (!Hint.Name.IsEmpty())
					{
						Writer.KeyString(TEXT("name"), Hint.Name);
					}
					Writer.KeyInt(TEXT("x"), Hint.X);
					Writer.KeyInt(TEXT("y"), Hint.Y);
					if (Hint.bHasSize)
					{
						Writer.KeyInt(TEXT("w"), Hint.W);
						Writer.KeyInt(TEXT("h"), Hint.H);
					}
					Writer.EndObject();
				}
				Writer.EndArray();
			}

			if (Graph.Sink != INDEX_NONE)
			{
				Writer.KeyInt(TEXT("sink"), Graph.Sink);
			}
			if (Graph.FunctionInputs.Num() > 0)
			{
				Writer.Key(TEXT("functionInputs"));
				Writer.BeginArray();
				for (const int32 NodeIndex : Graph.FunctionInputs)
				{
					Writer.ValueInt(NodeIndex);
				}
				Writer.EndArray();
			}
			if (Graph.FunctionOutputs.Num() > 0)
			{
				Writer.Key(TEXT("functionOutputs"));
				Writer.BeginArray();
				for (const int32 NodeIndex : Graph.FunctionOutputs)
				{
					Writer.ValueInt(NodeIndex);
				}
				Writer.EndArray();
			}

			Writer.Key(TEXT("nodes"));
			Writer.BeginArray();
			for (int32 NodeIndex = 0; NodeIndex < Graph.Nodes.Num(); ++NodeIndex)
			{
				Private::WriteNodeJson(Writer, Module, NodeIndex, Graph.Nodes[NodeIndex]);
			}
			Writer.EndArray();

			Private::WriteSourceRefJson(Writer, Module, Product.Source);
			Writer.EndObject();
		}
		Writer.EndArray();

		Writer.EndObject();
		return Writer.Release();
	}
}
