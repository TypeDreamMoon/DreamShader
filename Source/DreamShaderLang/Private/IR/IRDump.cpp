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

#include "Misc/Char.h"
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
				// sampler or the level as FIRValue::None() when the source did not give one;
				// `_` keeps the slot visible so the golden shows WHICH
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

		/**
		 * A layout comment colour channel as a dump writes it: rounded to six decimals first, so the float32 a
		 * hint stores reads `0.1` and not `0.100000001`, then the one culture-invariant number formatter.
		 */
		static double RoundIRDumpLayoutColorChannel(const float Channel)
		{
			return FMath::RoundToDouble(static_cast<double>(Channel) * 1000000.0) / 1000000.0;
		}

		// ------------------------------------------------------------------ instances

		/** A text the dump writes between quotes, escaped like a property: backslash, line breaks, tab, quote. */
		static FString QuoteIRDumpText(const FString& Text)
		{
			return FString::Printf(TEXT("\"%s\""), *EscapeQuoted(FIRPropertyValue::MakeString(Text).ToString()));
		}

		static const TCHAR* LexIRDumpParameterAssociation(const EIRParameterAssociation Association)
		{
			switch (Association)
			{
			case EIRParameterAssociation::Global: return TEXT("Global");
			case EIRParameterAssociation::Layer:  return TEXT("Layer");
			case EIRParameterAssociation::Blend:  return TEXT("Blend");
			}
			return TEXT("Global");
		}

		/** An instance key's value: bare when it is one word (`Translucent`, `true`, `0.5`), quoted otherwise (a path). */
		static FString RenderIRDumpInstanceSettingValue(const FString& Value)
		{
			bool bBare = !Value.IsEmpty();
			for (const TCHAR Char : Value)
			{
				if (!(FChar::IsAlnum(Char) || Char == TCHAR('_') || Char == TCHAR('.') || Char == TCHAR('+') || Char == TCHAR('-')))
				{
					bBare = false;
					break;
				}
			}
			return bBare ? Value : QuoteIRDumpText(Value);
		}

		/** `4`, `(1, 0.2, 0.1)` (a vector as wide as it was declared), `true`, `"/Game/T"`, `None`. */
		static FString RenderIRDumpInstanceOverrideValue(const FIRInstanceOverride& Override)
		{
			const FIRPropertyValue& Value = Override.Value;
			switch (Value.Kind)
			{
			case EIRPropertyKind::Bool:
				return Value.B ? TEXT("true") : TEXT("false");
			case EIRPropertyKind::Float4:
			{
				int32 Count = FMath::Clamp(Value.N, 1, 4);
				const bool bVector = Override.Kind == EIRParameterKind::Vector || Override.Kind == EIRParameterKind::DoubleVector;
				if (bVector && Override.DeclaredType.GraphComponentCount() > 0)
				{
					Count = FMath::Min(Count, Override.DeclaredType.GraphComponentCount());
				}
				if (Count == 1 && Override.Kind == EIRParameterKind::Scalar)
				{
					return FormatIRNumber(Value.V[0]);
				}
				FString Result = TEXT("(");
				for (int32 Index = 0; Index < Count; ++Index)
				{
					Result += Index > 0 ? TEXT(", ") : TEXT("");
					Result += FormatIRNumber(Value.V[Index]);
				}
				return Result + TEXT(")");
			}
			case EIRPropertyKind::Object:
				return Value.S.IsEmpty() ? FString(TEXT("None")) : QuoteIRDumpText(Value.S);
			case EIRPropertyKind::Int:
			case EIRPropertyKind::Float:
			case EIRPropertyKind::String:
			case EIRPropertyKind::Name:
			case EIRPropertyKind::Enum:
			case EIRPropertyKind::StringList:
				return RenderPropertyValue(Value);
			}
			return RenderPropertyValue(Value);
		}

		/**
		 * A MaterialInstance product as text: no backend and no graph, the parent
		 * with the schema it was checked against, the keys in source order, one line per override.
		 */
		static void AppendIRDumpInstanceLines(const FIRProduct& Product, const int32 ProductIndex, TArray<FString>& Lines)
		{
			Lines.Add(FString::Printf(TEXT("product %d %s \"%s\""), ProductIndex, LexToString(Product.Kind), *Product.Name));
			if (!Product.AssetPathOverride.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("  path \"%s\""), *Product.AssetPathOverride));
			}

			const FIRInstance& Instance = Product.Instance;
			FString ParentLine = FString::Printf(TEXT("  parent %s"), *QuoteIRDumpText(Instance.ParentReference));
			if (!Instance.ParentObjectPath.IsEmpty())
			{
				ParentLine += FString::Printf(TEXT(" object=%s"), *QuoteIRDumpText(Instance.ParentObjectPath));
			}
			if (Instance.ParentSchema.bValid)
			{
				ParentLine += FString::Printf(
					TEXT(" schema=%s params=%d"),
					Instance.ParentSchema.Origin.IsEmpty() ? TEXT("?") : *Instance.ParentSchema.Origin,
					Instance.ParentSchema.Parameters.Num());
			}
			else
			{
				ParentLine += TEXT(" schema=none");
			}
			Lines.Add(ParentLine);

			for (const TPair<FString, FString>& Setting : Instance.Settings)
			{
				Lines.Add(FString::Printf(TEXT("  setting %s %s"), *Setting.Key, *RenderIRDumpInstanceSettingValue(Setting.Value)));
			}

			for (const FIRInstanceOverride& Override : Instance.Overrides)
			{
				// An asset kind (a Font, a runtime virtual texture) has no value type: `-`, not the Error type's name.
				FString Line = FString::Printf(
					TEXT("  override %s %s %s %s"),
					*QuoteIRDumpText(Override.ParameterName),
					LexToString(Override.Kind),
					Override.DeclaredType.IsError() ? TEXT("-") : *Override.DeclaredType.ToString(),
					*RenderIRDumpInstanceOverrideValue(Override));
				if (Override.Kind == EIRParameterKind::Font || Override.FontPage != 0)
				{
					Line += FString::Printf(TEXT(" page=%d"), Override.FontPage);
				}
				if (!Override.VariableName.IsEmpty() && !Override.VariableName.Equals(Override.ParameterName, ESearchCase::CaseSensitive))
				{
					Line += FString::Printf(TEXT(" var=%s"), *Override.VariableName);
				}
				Lines.Add(Line);
			}
		}

		static void WriteIRDumpInstanceJson(FIRJsonWriter& Writer, const FIRModule& Module, const FIRInstance& Instance)
		{
			Writer.Key(TEXT("instance"));
			Writer.BeginObject();
			Writer.KeyString(TEXT("parent"), Instance.ParentReference);
			Writer.KeyString(TEXT("parentObject"), Instance.ParentObjectPath);
			Writer.KeyString(TEXT("schemaOrigin"), Instance.ParentSchema.bValid ? Instance.ParentSchema.Origin : FString());
			Writer.KeyInt(TEXT("schemaParameters"), Instance.ParentSchema.bValid ? Instance.ParentSchema.Parameters.Num() : 0);

			Writer.Key(TEXT("settings"));
			Writer.BeginArray();
			for (const TPair<FString, FString>& Setting : Instance.Settings)
			{
				Writer.BeginArray();
				Writer.ValueString(Setting.Key);
				Writer.ValueString(Setting.Value);
				Writer.EndArray();
			}
			Writer.EndArray();

			Writer.Key(TEXT("overrides"));
			Writer.BeginArray();
			for (const FIRInstanceOverride& Override : Instance.Overrides)
			{
				Writer.BeginObject();
				Writer.KeyString(TEXT("name"), Override.ParameterName);
				Writer.KeyString(TEXT("kind"), LexToString(Override.Kind));
				Writer.KeyString(TEXT("association"), LexIRDumpParameterAssociation(Override.Association));
				Writer.KeyInt(TEXT("index"), Override.AssociationIndex);
				Writer.KeyString(TEXT("type"), Override.DeclaredType.IsError() ? FString() : Override.DeclaredType.ToString());
				Writer.KeyString(TEXT("value"), Override.Value.ToString());
				Writer.KeyString(TEXT("var"), Override.VariableName);
				if (Override.Kind == EIRParameterKind::Font || Override.FontPage != 0)
				{
					Writer.KeyInt(TEXT("page"), Override.FontPage);
				}
				Writer.Key(TEXT("span"));
				Writer.BeginObject();
				Writer.KeyInt(TEXT("line"), Override.Source.Span.Line);
				Writer.KeyInt(TEXT("column"), Override.Source.Span.Column);
				Writer.KeyInt(TEXT("length"), Override.Source.Span.Length);
				if (IsForeignFile(Module.SourceFilePath, Override.Source.File))
				{
					Writer.KeyString(TEXT("file"), CleanFileName(Override.Source.File));
				}
				Writer.EndObject();
				Writer.EndObject();
			}
			Writer.EndArray();

			Writer.EndObject();
		}

		// ---------------------------------------------------------------- pass pipelines (`.dsp`)

		/** `(1, 0.5, 0, 1)`: the first Count of four numbers. */
		static FString RenderIRDumpNumbers(const double* Values, const int32 Count)
		{
			FString Result = TEXT("(");
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Result += Index > 0 ? TEXT(", ") : TEXT("");
				Result += FormatIRNumber(Values[Index]);
			}
			return Result + TEXT(")");
		}

		/** A pipeline parameter's default or a param constant: `0.5`, `(1, 0, 0, 1)`, `3`, `true`, `"/Game/T"`, `None`. */
		static FString RenderIRDumpPipelineValue(const FIRPropertyValue& Value)
		{
			switch (Value.Kind)
			{
			case EIRPropertyKind::Bool:
				return Value.B ? TEXT("true") : TEXT("false");
			case EIRPropertyKind::Int:
				return FString::Printf(TEXT("%lld"), static_cast<long long>(Value.I));
			case EIRPropertyKind::Float4:
			{
				const int32 Count = FMath::Clamp(Value.N, 1, 4);
				return Count == 1 ? FormatIRNumber(Value.V[0]) : RenderIRDumpNumbers(Value.V, Count);
			}
			case EIRPropertyKind::Object:
				return Value.S.IsEmpty() ? FString(TEXT("None")) : QuoteIRDumpText(Value.S);
			default:
				return RenderPropertyValue(Value);
			}
		}

		static FString RenderIRDumpPipelineBinding(const FIRPassBinding& Binding)
		{
			return FString::Printf(TEXT("%s = %s%s"), *Binding.Slot, *Binding.Buffer, Binding.bPrevious ? TEXT(".Previous") : TEXT(""));
		}

		static FString RenderIRDumpPipelineParam(const FIRPassParam& Param)
		{
			if (Param.SourceKind.Equals(TEXT("Constant"), ESearchCase::CaseSensitive))
			{
				return FString::Printf(TEXT("%s = %s %s"), *Param.Target, *Param.ConstantType, *RenderIRDumpPipelineValue(Param.Constant));
			}
			const FString Source = Param.SourceKind.Equals(TEXT("Weight"), ESearchCase::CaseSensitive) ? FString(TEXT("Weight")) : Param.Parameter;
			return FString::Printf(TEXT("%s = %s * %s + %s"), *Param.Target, *Source, *FormatIRNumber(Param.Multiplier), *FormatIRNumber(Param.Offset));
		}

		/** A filter in its normal form: clauses joined by `|`, the terms of one by `&`. */
		static FString RenderIRDumpPipelineFilter(const TArray<FIRPassFilterClause>& Filter)
		{
			TArray<FString> Clauses;
			for (const FIRPassFilterClause& Clause : Filter)
			{
				TArray<FString> Terms;
				for (const FIRPassFilterTerm& Term : Clause.AllOf)
				{
					if (Term.Kind.Equals(TEXT("Stencil"), ESearchCase::CaseSensitive))
					{
						Terms.Add(FString::Printf(TEXT("Stencil(%d, %d)"), Term.StencilValue, Term.StencilMask));
					}
					else if (Term.Kind.Equals(TEXT("Layer"), ESearchCase::CaseSensitive))
					{
						Terms.Add(FString::Printf(TEXT("Layer(%s)"), *FString::Join(Term.Layers, TEXT(" | "))));
					}
					else
					{
						Terms.Add(FString::Printf(TEXT("%s(%s)"), *Term.Kind, *Term.List));
					}
				}
				Clauses.Add(FString::Join(Terms, TEXT(" & ")));
			}
			return FString::Join(Clauses, TEXT(" | "));
		}

		/**
		 * A PassPipeline product as text: no backend and no graph; the pipeline's keys, then one line per parameter and
		 * buffer, and per pass its keys, its kind's settings and its bindings, every value as the binder settled it.
		 */
		static void AppendIRDumpPipelineLines(const FIRProduct& Product, const int32 ProductIndex, TArray<FString>& Lines)
		{
			Lines.Add(FString::Printf(TEXT("product %d %s \"%s\""), ProductIndex, LexToString(Product.Kind), *Product.Name));
			if (!Product.AssetPathOverride.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("  path \"%s\""), *Product.AssetPathOverride));
			}

			const FIRPassPipeline& Pipeline = Product.PassPipeline;
			Lines.Add(FString::Printf(
				TEXT("  pipeline order=%d injection=%s views=%s requires=%s enabled=%s"),
				Pipeline.Order,
				*Pipeline.DefaultInjection,
				Pipeline.Views.Num() > 0 ? *FString::Join(Pipeline.Views, TEXT("|")) : TEXT("Game|Editor"),
				Pipeline.Requires.Num() > 0 ? *FString::Join(Pipeline.Requires, TEXT("|")) : TEXT("-"),
				Pipeline.EnabledParameter.IsEmpty() ? TEXT("-") : *Pipeline.EnabledParameter));

			for (const FIRPassParameter& Parameter : Pipeline.Parameters)
			{
				FString Line = FString::Printf(TEXT("  parameter %s %s default=%s"), *QuoteIRDumpText(Parameter.Name), *Parameter.Type, *RenderIRDumpPipelineValue(Parameter.Default));
				if (!Parameter.Group.IsEmpty())
				{
					Line += FString::Printf(TEXT(" group=%s"), *QuoteIRDumpText(Parameter.Group));
				}
				if (Parameter.bHasSlider)
				{
					Line += FString::Printf(TEXT(" slider=(%s, %s)"), *FormatIRNumber(Parameter.SliderMin), *FormatIRNumber(Parameter.SliderMax));
				}
				if (Parameter.SortPriority != 0)
				{
					Line += FString::Printf(TEXT(" sort=%d"), Parameter.SortPriority);
				}
				if (!Parameter.Description.IsEmpty())
				{
					Line += FString::Printf(TEXT(" desc=%s"), *QuoteIRDumpText(Parameter.Description));
				}
				Lines.Add(Line);
			}

			for (const FIRPassBuffer& Buffer : Pipeline.Buffers)
			{
				FString Line = FString::Printf(TEXT("  buffer %s %s resolution=%s"), *QuoteIRDumpText(Buffer.Name), *Buffer.Format, *Buffer.Resolution);
				Line += Buffer.Resolution.Equals(TEXT("Fixed"), ESearchCase::CaseSensitive)
					? FString::Printf(TEXT(" size=%dx%d"), Buffer.FixedWidth, Buffer.FixedHeight)
					: FString::Printf(TEXT(" scale=%s"), *FormatIRNumber(Buffer.Scale));
				if (!Buffer.bResolutionWritten)
				{
					Line += TEXT(" (inferred)");
				}
				Line += Buffer.bClear ? FString::Printf(TEXT(" clear=%s"), *RenderIRDumpNumbers(Buffer.ClearValue, 4)) : FString(TEXT(" clear=None"));
				Line += FString::Printf(TEXT(" mips=%d"), Buffer.Mips);
				if (Buffer.bHistory)
				{
					Line += TEXT(" history");
				}
				if (Buffer.bExport)
				{
					Line += TEXT(" export");
				}
				if (!Buffer.Description.IsEmpty())
				{
					Line += FString::Printf(TEXT(" desc=%s"), *QuoteIRDumpText(Buffer.Description));
				}
				Lines.Add(Line);
			}

			for (const FIRPass& Pass : Pipeline.Passes)
			{
				FString Head = FString::Printf(TEXT("  pass %s %s injection=%s"), *QuoteIRDumpText(Pass.Name), *Pass.Kind, *Pass.Injection);
				if (!Pass.EnabledParameter.IsEmpty())
				{
					Head += FString::Printf(TEXT(" enabled=%s"), *Pass.EnabledParameter);
				}
				if (!Pass.Description.IsEmpty())
				{
					Head += FString::Printf(TEXT(" desc=%s"), *QuoteIRDumpText(Pass.Description));
				}
				Lines.Add(Head);

				if (!Pass.MaterialReference.IsEmpty() || !Pass.MaterialObjectPath.IsEmpty())
				{
					Lines.Add(FString::Printf(TEXT("    material %s object=%s"), *QuoteIRDumpText(Pass.MaterialReference), *QuoteIRDumpText(Pass.MaterialObjectPath)));
				}
				if (!Pass.ShaderReference.IsEmpty() || !Pass.ShaderVirtualPath.IsEmpty())
				{
					// The file on disk is this machine's; the virtual path is what the asset keeps.
					Lines.Add(FString::Printf(TEXT("    shader %s virtual=%s entry=%s"), *QuoteIRDumpText(Pass.ShaderReference), *QuoteIRDumpText(Pass.ShaderVirtualPath), *Pass.Entry));
				}
				if (Pass.Kind.Equals(TEXT("compute"), ESearchCase::CaseSensitive))
				{
					FString Line = FString::Printf(TEXT("    threads=(%d, %d, %d)%s"), Pass.ThreadsX, Pass.ThreadsY, Pass.ThreadsZ, Pass.bThreadsWritten ? TEXT("") : TEXT(" (from the shader)"));
					Line += Pass.DispatchMode.Equals(TEXT("Fixed"), ESearchCase::CaseSensitive)
						? FString::Printf(TEXT(" dispatch=(%d, %d, %d)"), Pass.DispatchX, Pass.DispatchY, Pass.DispatchZ)
						: FString::Printf(TEXT(" dispatch=%s * %s"), *Pass.DispatchBuffer, *FormatIRNumber(Pass.DispatchScale));
					Lines.Add(Line);
				}
				if (Pass.Kind.Equals(TEXT("mesh"), ESearchCase::CaseSensitive))
				{
					Lines.Add(FString::Printf(TEXT("    filter %s"), *RenderIRDumpPipelineFilter(Pass.Filter)));
					FString Line = FString::Printf(
						TEXT("    mesh mode=%s depth=%s%s cull=%s blend=%s usage=%s nanite=%s"),
						*Pass.MeshMode,
						*Pass.Depth,
						Pass.DepthBuffer.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("(%s)"), *Pass.DepthBuffer),
						*Pass.Cull,
						*Pass.Blend,
						Pass.Usage.Num() > 0 ? *FString::Join(Pass.Usage, TEXT("|")) : TEXT("default"),
						*Pass.Nanite);
					if (Pass.Nanite.Equals(TEXT("AssignStencil"), ESearchCase::CaseSensitive))
					{
						Line += FString::Printf(TEXT(" assign=%d"), Pass.AssignedStencilValue);
					}
					if (!Pass.Nanite.Equals(TEXT("Skip"), ESearchCase::CaseSensitive))
					{
						Line += FString::Printf(TEXT(" naniteValue=%s"), *RenderIRDumpNumbers(Pass.NaniteValue, 4));
					}
					Lines.Add(Line);
				}
				if (Pass.Kind.Equals(TEXT("clear"), ESearchCase::CaseSensitive))
				{
					Lines.Add(FString::Printf(TEXT("    value=%s"), *RenderIRDumpNumbers(Pass.ClearValue, 4)));
				}
				for (const FIRPassBinding& Read : Pass.Reads)
				{
					Lines.Add(FString::Printf(TEXT("    read %s"), *RenderIRDumpPipelineBinding(Read)));
				}
				for (const FIRPassBinding& Write : Pass.Writes)
				{
					Lines.Add(FString::Printf(TEXT("    write %s"), *RenderIRDumpPipelineBinding(Write)));
				}
				for (const FIRPassParam& Param : Pass.Params)
				{
					Lines.Add(FString::Printf(TEXT("    param %s"), *RenderIRDumpPipelineParam(Param)));
				}
			}
		}

		static void WriteIRDumpPipelineSpanJson(FIRJsonWriter& Writer, const FIRModule& Module, const FIRSourceRef& Source)
		{
			Writer.Key(TEXT("span"));
			Writer.BeginObject();
			Writer.KeyInt(TEXT("line"), Source.Span.Line);
			Writer.KeyInt(TEXT("column"), Source.Span.Column);
			Writer.KeyInt(TEXT("length"), Source.Span.Length);
			if (IsForeignFile(Module.SourceFilePath, Source.File))
			{
				Writer.KeyString(TEXT("file"), CleanFileName(Source.File));
			}
			Writer.EndObject();
		}

		static void WriteIRDumpPipelineBindingsJson(FIRJsonWriter& Writer, const FIRModule& Module, const TCHAR* Key, const TArray<FIRPassBinding>& Bindings)
		{
			Writer.Key(Key);
			Writer.BeginArray();
			for (const FIRPassBinding& Binding : Bindings)
			{
				Writer.BeginObject();
				Writer.KeyString(TEXT("slot"), Binding.Slot);
				Writer.KeyString(TEXT("buffer"), Binding.Buffer);
				Writer.KeyBool(TEXT("previous"), Binding.bPrevious);
				WriteIRDumpPipelineSpanJson(Writer, Module, Binding.Source);
				Writer.EndObject();
			}
			Writer.EndArray();
		}

		static void WriteIRDumpNumbersJson(FIRJsonWriter& Writer, const TCHAR* Key, const double* Values, const int32 Count)
		{
			Writer.Key(Key);
			Writer.BeginArray();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Writer.ValueNumber(Values[Index]);
			}
			Writer.EndArray();
		}

		static void WriteIRDumpPipelineJson(FIRJsonWriter& Writer, const FIRModule& Module, const FIRPassPipeline& Pipeline)
		{
			Writer.Key(TEXT("pipeline"));
			Writer.BeginObject();
			Writer.KeyInt(TEXT("order"), Pipeline.Order);
			Writer.KeyString(TEXT("injection"), Pipeline.DefaultInjection);
			Writer.KeyStringArray(TEXT("views"), Pipeline.Views);
			Writer.KeyStringArray(TEXT("requires"), Pipeline.Requires);
			Writer.KeyString(TEXT("enabled"), Pipeline.EnabledParameter);

			Writer.Key(TEXT("parameters"));
			Writer.BeginArray();
			for (const FIRPassParameter& Parameter : Pipeline.Parameters)
			{
				Writer.BeginObject();
				Writer.KeyString(TEXT("name"), Parameter.Name);
				Writer.KeyString(TEXT("type"), Parameter.Type);
				Writer.KeyString(TEXT("default"), Parameter.Default.ToString());
				Writer.KeyString(TEXT("group"), Parameter.Group);
				Writer.KeyString(TEXT("description"), Parameter.Description);
				if (Parameter.bHasSlider)
				{
					Writer.KeyNumber(TEXT("sliderMin"), Parameter.SliderMin);
					Writer.KeyNumber(TEXT("sliderMax"), Parameter.SliderMax);
				}
				Writer.KeyInt(TEXT("sort"), Parameter.SortPriority);
				WriteIRDumpPipelineSpanJson(Writer, Module, Parameter.Source);
				Writer.EndObject();
			}
			Writer.EndArray();

			Writer.Key(TEXT("buffers"));
			Writer.BeginArray();
			for (const FIRPassBuffer& Buffer : Pipeline.Buffers)
			{
				Writer.BeginObject();
				Writer.KeyString(TEXT("name"), Buffer.Name);
				Writer.KeyString(TEXT("format"), Buffer.Format);
				Writer.KeyString(TEXT("resolution"), Buffer.Resolution);
				Writer.KeyBool(TEXT("resolutionWritten"), Buffer.bResolutionWritten);
				Writer.KeyNumber(TEXT("scale"), Buffer.Scale);
				Writer.KeyInt(TEXT("width"), Buffer.FixedWidth);
				Writer.KeyInt(TEXT("height"), Buffer.FixedHeight);
				Writer.KeyBool(TEXT("clear"), Buffer.bClear);
				WriteIRDumpNumbersJson(Writer, TEXT("clearValue"), Buffer.ClearValue, 4);
				Writer.KeyInt(TEXT("mips"), Buffer.Mips);
				Writer.KeyBool(TEXT("history"), Buffer.bHistory);
				Writer.KeyBool(TEXT("export"), Buffer.bExport);
				Writer.KeyString(TEXT("description"), Buffer.Description);
				WriteIRDumpPipelineSpanJson(Writer, Module, Buffer.Source);
				Writer.EndObject();
			}
			Writer.EndArray();

			Writer.Key(TEXT("passes"));
			Writer.BeginArray();
			for (const FIRPass& Pass : Pipeline.Passes)
			{
				Writer.BeginObject();
				Writer.KeyString(TEXT("name"), Pass.Name);
				Writer.KeyString(TEXT("kind"), Pass.Kind);
				Writer.KeyString(TEXT("injection"), Pass.Injection);
				Writer.KeyBool(TEXT("injectionWritten"), Pass.bInjectionWritten);
				Writer.KeyString(TEXT("enabled"), Pass.EnabledParameter);
				Writer.KeyString(TEXT("description"), Pass.Description);
				Writer.KeyString(TEXT("material"), Pass.MaterialReference);
				Writer.KeyString(TEXT("materialObject"), Pass.MaterialObjectPath);
				Writer.KeyString(TEXT("shader"), Pass.ShaderReference);
				Writer.KeyString(TEXT("shaderVirtualPath"), Pass.ShaderVirtualPath);
				Writer.KeyString(TEXT("entry"), Pass.Entry);
				if (Pass.Kind.Equals(TEXT("compute"), ESearchCase::CaseSensitive))
				{
					Writer.Key(TEXT("threads"));
					Writer.BeginArray();
					Writer.ValueInt(Pass.ThreadsX);
					Writer.ValueInt(Pass.ThreadsY);
					Writer.ValueInt(Pass.ThreadsZ);
					Writer.EndArray();
					Writer.KeyBool(TEXT("threadsWritten"), Pass.bThreadsWritten);
					Writer.KeyString(TEXT("dispatchMode"), Pass.DispatchMode);
					Writer.KeyString(TEXT("dispatchBuffer"), Pass.DispatchBuffer);
					Writer.KeyNumber(TEXT("dispatchScale"), Pass.DispatchScale);
					Writer.Key(TEXT("dispatchSize"));
					Writer.BeginArray();
					Writer.ValueInt(Pass.DispatchX);
					Writer.ValueInt(Pass.DispatchY);
					Writer.ValueInt(Pass.DispatchZ);
					Writer.EndArray();
				}
				if (Pass.Kind.Equals(TEXT("mesh"), ESearchCase::CaseSensitive))
				{
					Writer.Key(TEXT("filter"));
					Writer.BeginArray();
					for (const FIRPassFilterClause& Clause : Pass.Filter)
					{
						Writer.BeginArray();
						for (const FIRPassFilterTerm& Term : Clause.AllOf)
						{
							Writer.BeginObject();
							Writer.KeyString(TEXT("kind"), Term.Kind);
							if (Term.Kind.Equals(TEXT("Stencil"), ESearchCase::CaseSensitive))
							{
								Writer.KeyInt(TEXT("value"), Term.StencilValue);
								Writer.KeyInt(TEXT("mask"), Term.StencilMask);
							}
							else if (Term.Kind.Equals(TEXT("Layer"), ESearchCase::CaseSensitive))
							{
								Writer.KeyStringArray(TEXT("layers"), Term.Layers);
							}
							else
							{
								Writer.KeyString(TEXT("list"), Term.List);
							}
							Writer.EndObject();
						}
						Writer.EndArray();
					}
					Writer.EndArray();
					Writer.KeyString(TEXT("mode"), Pass.MeshMode);
					Writer.KeyString(TEXT("depth"), Pass.Depth);
					Writer.KeyString(TEXT("depthBuffer"), Pass.DepthBuffer);
					Writer.KeyString(TEXT("cull"), Pass.Cull);
					Writer.KeyString(TEXT("blend"), Pass.Blend);
					Writer.KeyStringArray(TEXT("usage"), Pass.Usage);
					Writer.KeyString(TEXT("nanite"), Pass.Nanite);
					Writer.KeyInt(TEXT("assignedStencil"), Pass.AssignedStencilValue);
					WriteIRDumpNumbersJson(Writer, TEXT("naniteValue"), Pass.NaniteValue, 4);
				}
				if (Pass.Kind.Equals(TEXT("clear"), ESearchCase::CaseSensitive))
				{
					WriteIRDumpNumbersJson(Writer, TEXT("value"), Pass.ClearValue, 4);
				}
				WriteIRDumpPipelineBindingsJson(Writer, Module, TEXT("reads"), Pass.Reads);
				WriteIRDumpPipelineBindingsJson(Writer, Module, TEXT("writes"), Pass.Writes);

				Writer.Key(TEXT("params"));
				Writer.BeginArray();
				for (const FIRPassParam& Param : Pass.Params)
				{
					Writer.BeginObject();
					Writer.KeyString(TEXT("target"), Param.Target);
					Writer.KeyString(TEXT("source"), Param.SourceKind);
					Writer.KeyString(TEXT("parameter"), Param.Parameter);
					if (Param.SourceKind.Equals(TEXT("Constant"), ESearchCase::CaseSensitive))
					{
						Writer.KeyString(TEXT("constant"), Param.Constant.ToString());
						Writer.KeyString(TEXT("constantType"), Param.ConstantType);
					}
					else
					{
						Writer.KeyNumber(TEXT("multiplier"), Param.Multiplier);
						Writer.KeyNumber(TEXT("offset"), Param.Offset);
					}
					WriteIRDumpPipelineSpanJson(Writer, Module, Param.Source);
					Writer.EndObject();
				}
				Writer.EndArray();

				WriteIRDumpPipelineSpanJson(Writer, Module, Pass.Source);
				Writer.EndObject();
			}
			Writer.EndArray();

			Writer.EndObject();
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

			if (Product.Kind == EIRProductKind::MaterialInstance)
			{
				Private::AppendIRDumpInstanceLines(Product, ProductIndex, Lines);
				continue;
			}
			if (Product.Kind == EIRProductKind::PassPipeline)
			{
				Private::AppendIRDumpPipelineLines(Product, ProductIndex, Lines);
				continue;
			}

			FString Header = FString::Printf(
				TEXT("product %d %s \"%s\" backend=%s"),
				ProductIndex,
				LexToString(Product.Kind),
				*Product.Name,
				LexToString(Product.Backend));

			if (Product.SubstrateMode != EIRSubstrateMode::Legacy)
			{
				Header += FString::Printf(TEXT(" substrate=%s"), LexToString(Product.SubstrateMode));
			}

			if (Product.Settings.Num() > 0)
			{
				Header += FString::Printf(TEXT(" settings{%s}"), *Private::RenderSettings(Product.Settings));
			}
			Lines.Add(Header);

			if (!Product.AssetPathOverride.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("  path \"%s\""), *Product.AssetPathOverride));
			}
			if (Product.bLegacyAssetPath)
			{
				// 1.x destination (legacy rule L10): Name may carry folders, AssetRoot is the Root= spelling.
				Lines.Add(FString::Printf(TEXT("  legacy root \"%s\""), *Private::EscapeQuoted(Product.AssetRoot)));
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
				if (Hint.bHasColor)
				{
					HintLine += FString::Printf(
						TEXT(" color=(%s, %s, %s, %s)"),
						*Private::FormatIRNumber(Private::RoundIRDumpLayoutColorChannel(Hint.Color[0])),
						*Private::FormatIRNumber(Private::RoundIRDumpLayoutColorChannel(Hint.Color[1])),
						*Private::FormatIRNumber(Private::RoundIRDumpLayoutColorChannel(Hint.Color[2])),
						*Private::FormatIRNumber(Private::RoundIRDumpLayoutColorChannel(Hint.Color[3])));
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
			if (Product.SubstrateMode != EIRSubstrateMode::Legacy)
			{
				Writer.KeyString(TEXT("substrate"), LexToString(Product.SubstrateMode));
			}

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
			if (Product.bLegacyAssetPath)
			{
				Writer.KeyBool(TEXT("legacyAssetPath"), true);
				Writer.KeyString(TEXT("assetRoot"), Product.AssetRoot);
			}
			if (Product.Kind == EIRProductKind::MaterialInstance)
			{
				Private::WriteIRDumpInstanceJson(Writer, Module, Product.Instance);
			}
			if (Product.Kind == EIRProductKind::PassPipeline)
			{
				Private::WriteIRDumpPipelineJson(Writer, Module, Product.PassPipeline);
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
					if (Hint.bHasColor)
					{
						Writer.Key(TEXT("color"));
						Writer.BeginArray();
						for (int32 Channel = 0; Channel < 4; ++Channel)
						{
							Writer.ValueNumber(Private::RoundIRDumpLayoutColorChannel(Hint.Color[Channel]));
						}
						Writer.EndArray();
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
