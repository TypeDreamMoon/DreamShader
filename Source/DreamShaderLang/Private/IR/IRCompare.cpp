// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See IR/IRCompare.h.

#include "IR/IRCompare.h"

#include "Hash/CityHash.h"
#include "IR/IRCatalog.h"
#include "IR/IRCoreOps.h"
#include "Lang/LangPipelineSource.h"

namespace UE::DreamShader::IR
{
	namespace ComparePrivate
	{
		static const TCHAR* const GMarkerBegin = TEXT("// Begin DreamShader source:");
		static const TCHAR* const GMarkerEnd = TEXT("// End DreamShader source:");
		static const TCHAR* const GMarkerCustom = TEXT("// DreamShader custom:");

		/**
		 * A number as the asset holds it. Every number of a material expression is a float -- a Constant's R, a
		 * parameter's DefaultValue, a `Const*` twin -- and the 1.x front end reads a vector default the way 1.x did, into
		 * floats, where a `.dss` literal is a double: `0.1` and `0.100000001` are one DefaultValue.
		 */
		static FIRPropertyValue RoundToAssetPrecision(const FIRPropertyValue& Value)
		{
			FIRPropertyValue Rounded = Value;
			if (Value.Kind == EIRPropertyKind::Float)
			{
				Rounded.F = static_cast<double>(static_cast<float>(Value.F));
			}
			else if (Value.Kind == EIRPropertyKind::Float4)
			{
				for (int32 Index = 0; Index < 4; ++Index)
				{
					Rounded.V[Index] = static_cast<double>(static_cast<float>(Value.V[Index]));
				}
			}
			return Rounded;
		}

		/** `Code` with every line trimmed, every run of blanks inside one made a single blank, and no blank line. */
		static FString NormalizeCodeSpacing(const FString& Code)
		{
			TArray<FString> Lines;
			Code.ParseIntoArrayLines(Lines, /* bCullEmpty */ false);
			for (FString& Line : Lines)
			{
				FString Out;
				Out.Reserve(Line.Len());
				bool bPendingBlank = false;
				for (const TCHAR Character : Line)
				{
					if (Character == TEXT(' ') || Character == TEXT('\t'))
					{
						bPendingBlank = !Out.IsEmpty();
						continue;
					}
					if (bPendingBlank)
					{
						Out.AppendChar(TEXT(' '));
						bPendingBlank = false;
					}
					Out.AppendChar(Character);
				}
				Line = MoveTemp(Out);
			}
			// A line of nothing but blanks is one more blank: the last line of a body that stood indented is such a line.
			Lines.RemoveAll([](const FString& Line) { return Line.IsEmpty(); });
			return FString::Join(Lines, TEXT("\n"));
		}

		/** `Code` without what only says where it came from: the file of the Begin/End lines, the line of the custom line. */
		static FString StripCustomMarkers(const FString& Code)
		{
			TArray<FString> Lines;
			Code.ParseIntoArrayLines(Lines, /* bCullEmpty */ false);
			for (FString& Line : Lines)
			{
				const FString Trimmed = Line.TrimStart();
				if (Trimmed.StartsWith(GMarkerBegin, ESearchCase::CaseSensitive))
				{
					Line = GMarkerBegin;
				}
				else if (Trimmed.StartsWith(GMarkerEnd, ESearchCase::CaseSensitive))
				{
					Line = GMarkerEnd;
				}
				else if (Trimmed.StartsWith(GMarkerCustom, ESearchCase::CaseSensitive))
				{
					// `// DreamShader custom: <Name> line <N>`: the name stays, it is part of what was built.
					const int32 LineWord = Trimmed.Find(TEXT(" line "), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
					Line = LineWord == INDEX_NONE ? Trimmed : Trimmed.Left(LineWord);
				}
			}
			return FString::Join(Lines, TEXT("\n"));
		}

		/** How many components of PreviewValue a FunctionInput of this InputType reads; 0 when it reads none. */
		static int32 PreviewWidthOfInputType(const FString& InputType)
		{
			if (InputType.Equals(TEXT("Scalar"), ESearchCase::IgnoreCase)) { return 1; }
			if (InputType.Equals(TEXT("Vector2"), ESearchCase::IgnoreCase)) { return 2; }
			if (InputType.Equals(TEXT("Vector3"), ESearchCase::IgnoreCase)) { return 3; }
			if (InputType.Equals(TEXT("Vector4"), ESearchCase::IgnoreCase)) { return 4; }
			return 0;
		}

		/**
		 * FIRCompareOptions::ConstTwinCatalog: the input as the `Const*` property a `.dss` writes for a constant on that
		 * pin ("ConstAlpha=0.5"), in the twin's type as the builder writes it (MakePropertyValue). False for an input
		 * that is no such thing: another node, a pin without a twin, a twin the node already says, a vector a scalar
		 * twin cannot hold.
		 */
		static bool TryFoldConstTwin(const FIRGraph& Graph, const FIRCompareOptions& Options, const FIRNode& Node, const FIRInput& Input, FString* OutProperty)
		{
			const FBuiltinCatalog* Catalog = Options.ConstTwinCatalog;
			if (!Catalog || Node.Op != EIROp::Reflected || !Catalog->Expressions.IsValidIndex(Node.CatalogIndex)
				|| !Input.Value.IsValid() || Input.Value.Output != 0 || !Graph.Nodes.IsValidIndex(Input.Value.Node))
			{
				return false;
			}
			const FIRNode& Source = Graph.Nodes[Input.Value.Node];
			const FIRProperty* Constant = Source.Op == EIROp::Constant ? Source.FindProperty(Prop::Value) : nullptr;
			if (!Constant || Constant->Value.Kind != EIRPropertyKind::Float4)
			{
				return false;
			}

			const FCatalogExpression& Entry = Catalog->Expressions[Node.CatalogIndex];
			const int32 PinIndex = Entry.FindInput(Input.Pin);
			if (!Entry.Inputs.IsValidIndex(PinIndex) || Entry.Inputs[PinIndex].ConstPropertyName.IsEmpty())
			{
				return false;
			}
			const FString& Twin = Entry.Inputs[PinIndex].ConstPropertyName;
			if (Node.FindProperty(*Twin) != nullptr)
			{
				return false;
			}

			const int32 TwinIndex = Entry.FindProperty(Twin);
			const ECatalogValueType TwinType = Entry.Properties.IsValidIndex(TwinIndex) && Entry.Properties[TwinIndex].Type != ECatalogValueType::Unknown
				? Entry.Properties[TwinIndex].Type
				: Entry.Inputs[PinIndex].Type;
			const int32 Width = FMath::Clamp(Constant->Value.N, 1, 4);
			const double* Components = Constant->Value.V;

			FIRPropertyValue Value;
			switch (TwinType)
			{
			case ECatalogValueType::Float1:
			case ECatalogValueType::Int:
			case ECatalogValueType::Bool:
			case ECatalogValueType::StaticBool:
				for (int32 Component = 1; Component < Width; ++Component)
				{
					if (Components[Component] != Components[0])
					{
						return false;
					}
				}
				Value = TwinType == ECatalogValueType::Float1 ? FIRPropertyValue::MakeFloat(Components[0])
					: TwinType == ECatalogValueType::Int ? FIRPropertyValue::MakeInt(static_cast<int64>(Components[0]))
					: FIRPropertyValue::MakeBool(Components[0] != 0.0);
				break;
			case ECatalogValueType::Float2:
				Value = FIRPropertyValue::MakeFloat4(Components, 2);
				break;
			case ECatalogValueType::Float3:
				Value = FIRPropertyValue::MakeFloat4(Components, 3);
				break;
			case ECatalogValueType::Float4:
				Value = FIRPropertyValue::MakeFloat4(Components, 4);
				break;
			default:
				Value = FIRPropertyValue::MakeFloat4(Components, Width);
				break;
			}

			if (OutProperty)
			{
				*OutProperty = Twin + TEXT("=") + RoundToAssetPrecision(Value).ToString();
			}
			return true;
		}

		/** FIRCompareOptions::bCompareIdentitySwizzles off: the value behind every `.xyz`-of-a-float3 in front of it. */
		static FIRValue SkipIdentitySwizzles(const FIRGraph& Graph, FIRValue Value)
		{
			static const FString Identity(TEXT("xyzw"));
			for (int32 Guard = 0; Guard < 64; ++Guard)
			{
				if (!Value.IsValid() || Value.Output != 0 || !Graph.Nodes.IsValidIndex(Value.Node))
				{
					break;
				}
				const FIRNode& Node = Graph.Nodes[Value.Node];
				if (Node.Op != EIROp::Swizzle || Node.Operands.Num() != 1)
				{
					break;
				}
				const FIRProperty* Mask = Node.FindProperty(Prop::Mask);
				const FIRValue Operand = Node.Operands[0];
				if (!Mask || !Operand.IsValid() || !Graph.Nodes.IsValidIndex(Operand.Node)
					|| !Graph.Nodes[Operand.Node].Outputs.IsValidIndex(Operand.Output))
				{
					break;
				}
				const int32 Width = Graph.Nodes[Operand.Node].Outputs[Operand.Output].GraphComponentCount();
				if (Width < 1 || Width > 4 || !Mask->Value.S.Equals(Identity.Left(Width), ESearchCase::CaseSensitive))
				{
					break;
				}
				Value = Operand;
			}
			return Value;
		}

		/**
		 * What a node reads, in an order two builds of the same thing share: operands as stored, because an operand's
		 * position is its meaning, and named inputs by pin name, because a pin's name is its meaning and two sources
		 * (or two front ends) may write `m.Roughness` and `m.Opacity` either way round. Holes are kept, as in
		 * FIRGraph::CollectInputValues. An input that is a `Const*` twin (TryFoldConstTwin) is a property, not a read.
		 */
		static void CollectReads(const FIRGraph& Graph, const FIRCompareOptions& Options, const FIRNode& Node, TArray<FIRValue>& OutReads, TArray<FString>* OutPins = nullptr)
		{
			OutReads.Reset();
			OutReads.Reserve(Node.Operands.Num() + Node.Inputs.Num());
			OutReads.Append(Node.Operands);

			TArray<const FIRInput*> Inputs;
			Inputs.Reserve(Node.Inputs.Num());
			for (const FIRInput& Input : Node.Inputs)
			{
				if (!TryFoldConstTwin(Graph, Options, Node, Input, nullptr))
				{
					Inputs.Add(&Input);
				}
			}
			// Stable: two inputs of one pin name keep the order they were written in.
			Inputs.StableSort([](const FIRInput& Left, const FIRInput& Right)
			{
				return Left.Pin.Compare(Right.Pin, ESearchCase::CaseSensitive) < 0;
			});

			if (OutPins)
			{
				OutPins->Reset();
			}
			for (const FIRInput* Input : Inputs)
			{
				OutReads.Add(Input->Value);
				if (OutPins)
				{
					OutPins->Add(Input->Pin);
				}
			}

			if (!Options.bCompareIdentitySwizzles)
			{
				for (FIRValue& Read : OutReads)
				{
					Read = SkipIdentitySwizzles(Graph, Read);
				}
			}
		}

		class FGraphView
		{
		public:
			FGraphView(const FIRGraph& InGraph, const FIRCompareOptions& InOptions)
				: Graph(InGraph)
				, Options(InOptions)
			{
				Signatures.Init(0, Graph.Nodes.Num());
				States.Init(0, Graph.Nodes.Num());
				OwnTexts.SetNum(Graph.Nodes.Num());
			}

			const FIRGraph& Graph;
			const FIRCompareOptions& Options;

			uint64 SignatureOf(const int32 NodeIndex)
			{
				if (!Graph.Nodes.IsValidIndex(NodeIndex))
				{
					return 0;
				}
				Compute(NodeIndex);
				return Signatures[NodeIndex];
			}

			uint64 SignatureOf(const FIRValue& Value)
			{
				return Value.IsValid() ? HashCombineFast64(SignatureOf(Value.Node), static_cast<uint64>(Value.Output) + 1) : 0;
			}

			/** Everything the node says about itself: what it is, not what it reads. */
			const FString& OwnTextOf(const int32 NodeIndex)
			{
				FString& Text = OwnTexts[NodeIndex];
				if (Text.IsEmpty())
				{
					Text = MakeOwnText(Graph.Nodes[NodeIndex]);
				}
				return Text;
			}

			FString Describe(const int32 NodeIndex) const
			{
				if (!Graph.Nodes.IsValidIndex(NodeIndex))
				{
					return TEXT("nothing");
				}
				const FIRNode& Node = Graph.Nodes[NodeIndex];
				FString Text = LexToString(Node.Op);
				if (!Node.ClassName.IsEmpty())
				{
					Text += FString::Printf(TEXT("[%s]"), *Node.ClassName);
				}
				if (!Node.DebugName.IsEmpty())
				{
					Text += FString::Printf(TEXT(" '%s'"), *Node.DebugName);
				}
				if (Node.Source.Span.Length > 0)
				{
					Text += FString::Printf(TEXT(" (line %d)"), Node.Source.Span.Line);
				}
				return Text;
			}

		private:
			static uint64 HashCombineFast64(const uint64 A, const uint64 B)
			{
				return A ^ (B + 0x9e3779b97f4a7c15ull + (A << 6) + (A >> 2));
			}

			static uint64 HashText(const FString& Text)
			{
				return CityHash64(reinterpret_cast<const char*>(*Text), static_cast<uint32>(Text.Len() * sizeof(TCHAR)));
			}

			FString MakeOwnText(const FIRNode& Node) const
			{
				FString Text = LexToString(Node.Op);
				Text += TEXT("|");
				Text += Node.ClassName;

				Text += TEXT("|out:");
				for (int32 Index = 0; Index < Node.Outputs.Num(); ++Index)
				{
					Text += Node.Outputs[Index].ToString();
					if (Node.OutputNames.IsValidIndex(Index))
					{
						Text += TEXT("=");
						Text += Node.OutputNames[Index];
					}
					Text += TEXT(",");
				}

				// By name: two front ends may write the same properties in another order, and that order builds nothing.
				TArray<FString> Properties;
				bool bHasPreview = false;
				for (const FIRProperty& Property : Node.Properties)
				{
					if (Node.Op == EIROp::FunctionInput && Property.Name.Equals(Prop::PreviewValue, ESearchCase::CaseSensitive))
					{
						bHasPreview = true;
						continue;
					}
					if (Property.Name.Equals(Prop::ClassSpecifier, ESearchCase::CaseSensitive)
						|| Property.Name.Equals(Prop::WrittenPins, ESearchCase::CaseSensitive)
						|| Property.Name.Equals(Prop::DeclaredInputs, ESearchCase::CaseSensitive))
					{
						// The IR's own records of how the author spelled things -- the class (`UE.Expression(Class = "Custom")`
						// or `UE.Custom`), a pin named by an alias, the order a 1.x VirtualFunction declares its inputs in.
						// None reaches reflection, and what they decide shows in the wiring, which is compared.
						continue;
					}
					if (Node.Op == EIROp::Custom && Property.Name.Equals(Prop::Code, ESearchCase::CaseSensitive)
						&& (!Options.bCompareCustomMarkers || !Options.bCompareCodeSpacing))
					{
						FString Code = Options.bCompareCustomMarkers ? Property.Value.S : StripCustomMarkers(Property.Value.S);
						if (!Options.bCompareCodeSpacing)
						{
							Code = NormalizeCodeSpacing(Code);
						}
						Properties.Add(Property.Name + TEXT("=") + Code);
						continue;
					}
					if (Node.Op == EIROp::Parameter
						&& Property.Name.Equals(Prop::DefaultValue, ESearchCase::CaseSensitive)
						&& Property.Value.Kind == EIRPropertyKind::Float4
						&& Node.Outputs.IsValidIndex(0) && Node.Outputs[0].GraphComponentCount() >= 2)
					{
						// A vector parameter is four channels whatever width was declared, and the emitter pads a narrower
						// default with (0, 0, 0, 1). `float3 c = (1, 1, 1)` and `float4 c = (1, 1, 1, 1)` are one parameter.
						const FIRPropertyValue& Written = Property.Value;
						const double Padded[4] = {
							Written.V[0],
							Written.N > 1 ? Written.V[1] : 0.0,
							Written.N > 2 ? Written.V[2] : 0.0,
							Written.N > 3 ? Written.V[3] : 1.0 };
						Properties.Add(Property.Name + TEXT("=") + RoundToAssetPrecision(FIRPropertyValue::MakeFloat4(Padded, 4)).ToString());
						continue;
					}
					if (Property.Value.Kind == EIRPropertyKind::String && Property.Value.S.Contains(TEXT("\r")))
					{
						// A description read out of a 1.x `"a\r\nb"` and the same one read off `///` lines.
						Properties.Add(Property.Name + TEXT("=") + FIRPropertyValue::MakeString(
							Property.Value.S.Replace(TEXT("\r\n"), TEXT("\n")).Replace(TEXT("\r"), TEXT("\n"))).ToString());
						continue;
					}
					Properties.Add(Property.Name + TEXT("=") + RoundToAssetPrecision(Property.Value).ToString());
				}

				for (const FIRInput& Input : Node.Inputs)
				{
					FString Folded;
					if (TryFoldConstTwin(Graph, Options, Node, Input, &Folded))
					{
						Properties.Add(MoveTemp(Folded));
					}
				}

				if (Node.Op == EIROp::FunctionInput)
				{
					const FIRProperty* InputType = Node.FindProperty(Prop::InputType);
					const int32 Width = InputType ? PreviewWidthOfInputType(InputType->Value.S) : 0;
					if (Width > 0)
					{
						double Preview[4] = { 0.0, 0.0, 0.0, 1.0 };
						if (bHasPreview)
						{
							const FIRProperty* Written = Node.FindProperty(Prop::PreviewValue);
							for (int32 Index = 0; Written && Index < 4; ++Index)
							{
								Preview[Index] = Written->Value.V[Index];
							}
						}
						Properties.Add(FString(Prop::PreviewValue) + TEXT("=") + FIRPropertyValue::MakeFloat4(Preview, Width).ToString());
					}
				}

				Properties.Sort();
				Text += TEXT("|props:");
				Text += FString::Join(Properties, TEXT(";"));

				if (Options.bCompareDebugNames)
				{
					Text += TEXT("|name:");
					Text += Node.DebugName;
				}
				if (Options.bCompareRegions)
				{
					Text += TEXT("|region:");
					Text += Graph.Regions.IsValidIndex(Node.Region) ? Graph.Regions[Node.Region].Name : FString();
				}

				TArray<FIRValue> Reads;
				TArray<FString> Pins;
				CollectReads(Graph, Options, Node, Reads, &Pins);
				Text += FString::Printf(TEXT("|reads:%d+%d"), Node.Operands.Num(), Pins.Num());
				for (const FString& Pin : Pins)
				{
					Text += TEXT(",");
					Text += Pin;
				}
				return Text;
			}

			void Compute(const int32 Root)
			{
				if (States[Root] == 2)
				{
					return;
				}

				// Not recursive: a long chain of statements is a deep graph.
				TArray<int32> Stack;
				Stack.Push(Root);
				TArray<FIRValue> Reads;
				while (Stack.Num() > 0)
				{
					const int32 NodeIndex = Stack.Last();
					if (States[NodeIndex] == 2)
					{
						Stack.Pop();
						continue;
					}

					const FIRNode& Node = Graph.Nodes[NodeIndex];
					CollectReads(Graph, Options, Node, Reads);

					if (States[NodeIndex] == 0)
					{
						States[NodeIndex] = 1;
						bool bWaits = false;
						for (const FIRValue& Read : Reads)
						{
							if (Read.IsValid() && Graph.Nodes.IsValidIndex(Read.Node) && States[Read.Node] == 0)
							{
								Stack.Push(Read.Node);
								bWaits = true;
							}
						}
						if (bWaits)
						{
							continue;
						}
					}

					// Every node it reads is done -- or is on the stack below it, which is a cycle the validator reports and
					// which reads as "nothing" here.
					uint64 Signature = HashText(OwnTextOf(NodeIndex));
					for (const FIRValue& Read : Reads)
					{
						const bool bDone = Read.IsValid() && Graph.Nodes.IsValidIndex(Read.Node) && States[Read.Node] == 2;
						const uint64 ReadSignature = bDone ? HashCombineFast64(Signatures[Read.Node], static_cast<uint64>(Read.Output) + 1) : 0;
						Signature = HashCombineFast64(Signature, ReadSignature);
					}
					Signatures[NodeIndex] = Signature;
					States[NodeIndex] = 2;
					Stack.Pop();
				}
			}

			TArray<uint64> Signatures;
			/** 0 not seen, 1 waiting for what it reads, 2 done. */
			TArray<uint8> States;
			TArray<FString> OwnTexts;
		};

		/** Follows the first difference down from a pair of nodes whose signatures differ. */
		static FString DescribeDifference(FGraphView& A, const int32 NodeA, FGraphView& B, const int32 NodeB)
		{
			int32 CurrentA = NodeA;
			int32 CurrentB = NodeB;
			for (int32 Depth = 0; Depth < 4096; ++Depth)
			{
				const bool bValidA = A.Graph.Nodes.IsValidIndex(CurrentA);
				const bool bValidB = B.Graph.Nodes.IsValidIndex(CurrentB);
				if (!bValidA || !bValidB)
				{
					return FString::Printf(TEXT("%s on one side, %s on the other"), *A.Describe(CurrentA), *B.Describe(CurrentB));
				}

				const FString& TextA = A.OwnTextOf(CurrentA);
				const FString& TextB = B.OwnTextOf(CurrentB);
				if (!TextA.Equals(TextB, ESearchCase::CaseSensitive))
				{
					return FString::Printf(TEXT("%s is not %s:\n    %s\n    %s"), *A.Describe(CurrentA), *B.Describe(CurrentB), *TextA, *TextB);
				}

				TArray<FIRValue> ReadsA;
				TArray<FIRValue> ReadsB;
				CollectReads(A.Graph, A.Options, A.Graph.Nodes[CurrentA], ReadsA);
				CollectReads(B.Graph, B.Options, B.Graph.Nodes[CurrentB], ReadsB);

				int32 Differing = INDEX_NONE;
				for (int32 Index = 0; Index < FMath::Min(ReadsA.Num(), ReadsB.Num()); ++Index)
				{
					if (A.SignatureOf(ReadsA[Index]) != B.SignatureOf(ReadsB[Index]))
					{
						Differing = Index;
						break;
					}
				}
				if (Differing == INDEX_NONE)
				{
					return FString::Printf(TEXT("%s and %s read a different number of values"), *A.Describe(CurrentA), *B.Describe(CurrentB));
				}

				if (ReadsA[Differing].IsValid() != ReadsB[Differing].IsValid() || ReadsA[Differing].Output != ReadsB[Differing].Output)
				{
					return FString::Printf(
						TEXT("input %d of %s reads %s output %d, and of %s reads %s output %d"),
						Differing,
						*A.Describe(CurrentA), *A.Describe(ReadsA[Differing].Node), ReadsA[Differing].Output,
						*B.Describe(CurrentB), *B.Describe(ReadsB[Differing].Node), ReadsB[Differing].Output);
				}
				CurrentA = ReadsA[Differing].Node;
				CurrentB = ReadsB[Differing].Node;
			}
			return TEXT("the graphs differ deeper than can be followed");
		}

		static FString RenderSettings(const TMap<FString, FString>& Settings)
		{
			TArray<FString> Lines;
			for (const TPair<FString, FString>& Setting : Settings)
			{
				// Keys as the pragma reads them: ignoring case.
				Lines.Add(Setting.Key.ToLower() + TEXT("=") + Setting.Value);
			}
			Lines.Sort();
			return FString::Join(Lines, TEXT(";"));
		}

		/** The statement roots that are neither the sink nor a function output: reflected custom-output nodes. */
		static void CollectOtherRoots(const FIRGraph& Graph, TArray<int32>& OutRoots)
		{
			for (int32 NodeIndex = 0; NodeIndex < Graph.Nodes.Num(); ++NodeIndex)
			{
				if (Graph.Nodes[NodeIndex].IsStatement() && NodeIndex != Graph.Sink && !Graph.FunctionOutputs.Contains(NodeIndex))
				{
					OutRoots.Add(NodeIndex);
				}
			}
		}

		static bool CompareRootLists(
			FGraphView& A, const TArray<int32>& RootsA,
			FGraphView& B, const TArray<int32>& RootsB,
			const TCHAR* What,
			FString& OutDifference)
		{
			if (RootsA.Num() != RootsB.Num())
			{
				OutDifference = FString::Printf(TEXT("%d %s on one side and %d on the other"), RootsA.Num(), What, RootsB.Num());
				return false;
			}
			for (int32 Index = 0; Index < RootsA.Num(); ++Index)
			{
				if (A.SignatureOf(RootsA[Index]) != B.SignatureOf(RootsB[Index]))
				{
					OutDifference = FString::Printf(TEXT("%s %d: %s"), What, Index, *DescribeDifference(A, RootsA[Index], B, RootsB[Index]));
					return false;
				}
			}
			return true;
		}

		static bool CompareInstances(const FIRInstance& A, const FIRInstance& B, FString& OutDifference)
		{
			if (!A.ParentReference.Equals(B.ParentReference, ESearchCase::CaseSensitive))
			{
				OutDifference = FString::Printf(TEXT("the parent is '%s' on one side and '%s' on the other"), *A.ParentReference, *B.ParentReference);
				return false;
			}

			const auto Render = [](const FIRInstance& Instance)
			{
				TArray<FString> Lines;
				for (const FIRInstanceOverride& Override : Instance.Overrides)
				{
					Lines.Add(FString::Printf(TEXT("%s|%s|%s|%d"), *Override.ParameterName, LexToString(Override.Kind), *Override.Value.ToString(), Override.FontPage));
				}
				Lines.Sort();
				for (const TPair<FString, FString>& Setting : Instance.Settings)
				{
					Lines.Add(Setting.Key.ToLower() + TEXT("=") + Setting.Value);
				}
				return Lines;
			};

			const TArray<FString> LinesA = Render(A);
			const TArray<FString> LinesB = Render(B);
			for (int32 Index = 0; Index < FMath::Max(LinesA.Num(), LinesB.Num()); ++Index)
			{
				const FString LineA = LinesA.IsValidIndex(Index) ? LinesA[Index] : FString(TEXT("nothing"));
				const FString LineB = LinesB.IsValidIndex(Index) ? LinesB[Index] : FString(TEXT("nothing"));
				if (!LineA.Equals(LineB, ESearchCase::CaseSensitive))
				{
					OutDifference = FString::Printf(TEXT("'%s' on one side and '%s' on the other"), *LineA, *LineB);
					return false;
				}
			}
			return true;
		}

		static bool CompareProducts(const FIRProduct& A, const FIRProduct& B, const FIRCompareOptions& Options, FString& OutDifference)
		{
			if (A.Kind != B.Kind)
			{
				OutDifference = FString::Printf(TEXT("a %s on one side and a %s on the other"), LexToString(A.Kind), LexToString(B.Kind));
				return false;
			}

			if (Options.bCompareDestinations)
			{
				if (!A.Name.Equals(B.Name, ESearchCase::CaseSensitive)
					|| !A.AssetPathOverride.Equals(B.AssetPathOverride, ESearchCase::CaseSensitive)
					|| A.bLegacyAssetPath != B.bLegacyAssetPath
					|| !A.AssetRoot.Equals(B.AssetRoot, ESearchCase::CaseSensitive))
				{
					OutDifference = FString::Printf(
						TEXT("the asset is '%s' (path '%s', root '%s') on one side and '%s' (path '%s', root '%s') on the other"),
						*A.Name, *A.AssetPathOverride, *A.AssetRoot,
						*B.Name, *B.AssetPathOverride, *B.AssetRoot);
					return false;
				}
			}

			if (Options.bCompareSettings)
			{
				if (A.Backend != B.Backend)
				{
					OutDifference = FString::Printf(TEXT("the backend is %s on one side and %s on the other"), LexToString(A.Backend), LexToString(B.Backend));
					return false;
				}
				if (A.SubstrateMode != B.SubstrateMode)
				{
					OutDifference = FString::Printf(TEXT("the Substrate mode is %s on one side and %s on the other"), LexToString(A.SubstrateMode), LexToString(B.SubstrateMode));
					return false;
				}
				const FString SettingsA = RenderSettings(A.Settings);
				const FString SettingsB = RenderSettings(B.Settings);
				if (!SettingsA.Equals(SettingsB, ESearchCase::CaseSensitive))
				{
					OutDifference = FString::Printf(TEXT("the material settings are {%s} on one side and {%s} on the other"), *SettingsA, *SettingsB);
					return false;
				}
				if (!A.LibraryPath.Equals(B.LibraryPath, ESearchCase::CaseSensitive))
				{
					OutDifference = FString::Printf(TEXT("the library is '%s' on one side and '%s' on the other"), *A.LibraryPath, *B.LibraryPath);
					return false;
				}
				if (!A.Description.Equals(B.Description, ESearchCase::CaseSensitive))
				{
					OutDifference = FString::Printf(TEXT("the description is '%s' on one side and '%s' on the other"), *A.Description, *B.Description);
					return false;
				}
			}

			if (A.Kind == EIRProductKind::MaterialInstance)
			{
				return CompareInstances(A.Instance, B.Instance, OutDifference);
			}
			if (A.Kind == EIRProductKind::PassPipeline)
			{
				// No graph: the payloads, compared the way the `.dsp` round trip compares them.
				TArray<FString> Differences;
				if (!Lang::CompareDreamShaderPipelines(A.PassPipeline, B.PassPipeline, &Differences))
				{
					OutDifference = Differences.Num() > 0 ? Differences[0] : FString(TEXT("the pipelines differ"));
					return false;
				}
				return true;
			}

			FGraphView ViewA(A.Graph, Options);
			FGraphView ViewB(B.Graph, Options);

			if ((A.Graph.Sink == INDEX_NONE) != (B.Graph.Sink == INDEX_NONE))
			{
				OutDifference = TEXT("one side writes a material and the other does not");
				return false;
			}
			if (A.Graph.Sink != INDEX_NONE && ViewA.SignatureOf(A.Graph.Sink) != ViewB.SignatureOf(B.Graph.Sink))
			{
				OutDifference = DescribeDifference(ViewA, A.Graph.Sink, ViewB, B.Graph.Sink);
				return false;
			}

			if (!CompareRootLists(ViewA, A.Graph.FunctionInputs, ViewB, B.Graph.FunctionInputs, TEXT("function input"), OutDifference)
				|| !CompareRootLists(ViewA, A.Graph.FunctionOutputs, ViewB, B.Graph.FunctionOutputs, TEXT("function output"), OutDifference))
			{
				return false;
			}

			// A uniform nothing reads builds no node, and is still a parameter an instance may have been written against.
			TArray<FString> PrunedA = A.Graph.PrunedParameters;
			TArray<FString> PrunedB = B.Graph.PrunedParameters;
			PrunedA.Sort();
			PrunedB.Sort();
			if (PrunedA != PrunedB)
			{
				OutDifference = FString::Printf(
					TEXT("the unused parameters are {%s} on one side and {%s} on the other"),
					*FString::Join(PrunedA, TEXT(", ")),
					*FString::Join(PrunedB, TEXT(", ")));
				return false;
			}

			// Custom-output nodes have no order of their own: by signature.
			TArray<int32> OthersA;
			TArray<int32> OthersB;
			CollectOtherRoots(A.Graph, OthersA);
			CollectOtherRoots(B.Graph, OthersB);
			OthersA.Sort([&ViewA](const int32 Left, const int32 Right) { return ViewA.SignatureOf(Left) < ViewA.SignatureOf(Right); });
			OthersB.Sort([&ViewB](const int32 Left, const int32 Right) { return ViewB.SignatureOf(Left) < ViewB.SignatureOf(Right); });
			return CompareRootLists(ViewA, OthersA, ViewB, OthersB, TEXT("output node"), OutDifference);
		}
	}

	bool AreDreamShaderIRModulesEquivalent(const FIRModule& A, const FIRModule& B, const FIRCompareOptions& Options, FString& OutDifference)
	{
		OutDifference.Reset();

		if (A.Products.Num() != B.Products.Num())
		{
			OutDifference = FString::Printf(TEXT("%d product(s) on one side and %d on the other"), A.Products.Num(), B.Products.Num());
			return false;
		}

		for (int32 ProductIndex = 0; ProductIndex < A.Products.Num(); ++ProductIndex)
		{
			FString Difference;
			if (!ComparePrivate::CompareProducts(A.Products[ProductIndex], B.Products[ProductIndex], Options, Difference))
			{
				OutDifference = FString::Printf(TEXT("product %d '%s': %s"), ProductIndex, *A.Products[ProductIndex].Name, *Difference);
				return false;
			}
		}
		return true;
	}
}
