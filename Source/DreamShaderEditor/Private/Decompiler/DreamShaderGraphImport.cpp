// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// ImportDreamShaderGraphToIR: the emitter, inverted.
//
// The walk is root-driven and iterative. Roots are what a build's roots become: the connected material attributes, the
// function outputs, the custom-output expressions. An expression is built into a node only after every expression it
// reads has been, so a node's inputs are IR values with types by the time it needs them; the stack is explicit because
// a material is allowed a chain longer than the thread's.
//
// Three kinds of expression never become a node. A reroute, plain or named, is the wire it stands on. A StaticSwitch
// with nothing wired to Value is whichever branch its default picks. And an engine GetMaterialAttributes hands its
// material through on output 0.
//
// Two classes keep their pins in arrays, which no call can name, and are read back as what they compute instead: a
// Convert (Make / Break FloatN) as the channels each output is put together from, a Switch as the chain of branches the
// engine makes of it.
//
// What an expression becomes: the core op the emitter would write the very same
// expression for -- every operand pin wired, every other property at its default -- and otherwise a Reflected node over
// the catalog, with the connected pins as inputs and the non-default properties, `Const*` twins of unwired pins always
// among them, as properties. The twins are always written because that is the shape RaiseDreamShaderIR matches.
//
// Output slots. A Reflected node's slot is the engine's output index: the catalog lists a class's outputs off the same
// array. A value node (a constant, a parameter, a core op) has one slot, and an engine output that is a channel view of
// it is a Swizzle of that slot -- which the emitter turns back into the very output (TryResolveSwizzleAsNamedOutput).

#include "Decompiler/DreamShaderGraphImport.h"

#include "Decompiler/DreamShaderGraphDecompilerHelpers.h"
#include "Decompiler/DreamShaderInlineMask.h"
#include "DreamShaderBuiltinCatalog.h"
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderVersionCompat.h"
#include "IR/IRCatalog.h"
#include "IR/IRCoreOps.h"
#include "IR/IRTypes.h"

#include "Dom/JsonObject.h"
#include "Engine/Texture.h"
#include "MaterialValueType.h"
#include "Materials/Material.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionGetMaterialAttributes.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

// Convert and Switch keep their pins in arrays; an engine without their headers has no such node to read.
#if __has_include("Materials/MaterialExpressionConvert.h")
#include "Materials/MaterialExpressionConvert.h"
#define DREAMSHADER_GRAPH_IMPORT_WITH_CONVERT 1
#else
#define DREAMSHADER_GRAPH_IMPORT_WITH_CONVERT 0
#endif
#if __has_include("Materials/MaterialExpressionSwitch.h")
#include "Materials/MaterialExpressionSwitch.h"
#define DREAMSHADER_GRAPH_IMPORT_WITH_SWITCH 1
#else
#define DREAMSHADER_GRAPH_IMPORT_WITH_SWITCH 0
#endif

#define LOCTEXT_NAMESPACE "DreamShader.Decompiler.GraphImport"

namespace UE::DreamShader::Editor::Private
{
	namespace GraphImport
	{
		using UE::DreamShader::IR::EIROp;
		using UE::DreamShader::IR::FIRGraph;
		using UE::DreamShader::IR::FIRInput;
		using UE::DreamShader::IR::FIRNode;
		using UE::DreamShader::IR::FIRProduct;
		using UE::DreamShader::IR::FIRPropertyValue;
		using UE::DreamShader::IR::FIRType;
		using UE::DreamShader::IR::FIRValue;
		using UE::DreamShader::Lang::ETextureKind;

		static const TCHAR* const SwizzleComponents = TEXT("xyzw");
		static const TCHAR* const MaterialAttributesPin = TEXT("MaterialAttributes");
		static const TCHAR* const GeneratedBoxPrefix = TEXT("DreamShader: ");
		static const TCHAR* const MaterialOutputBoxTitle = TEXT("Material Output");

		// -------------------------------------------------------------------------------- small facts

		/** The enumerator without its prefix, exactly as the catalog lists an enum property's values. */
		static FString MakeImportEnumSpelling(const UEnum* Enum, const int64 Value)
		{
			if (!Enum)
			{
				return FString();
			}
			FString Name = Enum->GetNameStringByValue(Value);
			int32 Separator = INDEX_NONE;
			if (Name.FindChar(TCHAR('_'), Separator))
			{
				Name.RightChopInline(Separator + 1, DREAMSHADER_ALLOW_SHRINKING_NO);
			}
			return Name;
		}

		static FString NormalizeImportAssetPath(const FString& ObjectPath)
		{
			// `/Game/F/MF_X.MF_X` and `/Game/F/MF_X` name one asset; the package spelling is what `@asset` carries.
			FString Result = ObjectPath;
			int32 Dot = INDEX_NONE;
			int32 Slash = INDEX_NONE;
			if (Result.FindLastChar(TCHAR('.'), Dot) && Result.FindLastChar(TCHAR('/'), Slash) && Dot > Slash)
			{
				if (Result.Mid(Slash + 1, Dot - Slash - 1).Equals(Result.Mid(Dot + 1), ESearchCase::IgnoreCase))
				{
					Result.LeftInline(Dot);
				}
			}
			return Result;
		}

		static ETextureKind TextureKindFromValueType(const EMaterialValueType ValueType)
		{
			switch (ValueType)
			{
			case MCT_TextureCube: return ETextureKind::TextureCube;
			case MCT_Texture2DArray: return ETextureKind::Texture2DArray;
			case MCT_VolumeTexture: return ETextureKind::VolumeTexture;
			default: break;
			}
			return ETextureKind::Texture2D;
		}

		/** An engine value type as an IR type; Error when the engine only says "some float". */
		static FIRType TypeFromMaterialValueType(const EMaterialValueType ValueType)
		{
			switch (ValueType)
			{
			case MCT_Float1: return FIRType::Float(1);
			case MCT_Float2: return FIRType::Float(2);
			case MCT_Float3: return FIRType::Float(3);
			case MCT_Float4: return FIRType::Float(4);
			case MCT_Texture2D:
			case MCT_TextureCube:
			case MCT_Texture2DArray:
			case MCT_VolumeTexture:
			case MCT_TextureExternal:
				return FIRType::TextureOf(TextureKindFromValueType(ValueType));
			case MCT_StaticBool:
			case MCT_Bool:
				return FIRType::Bool(1);
			case MCT_MaterialAttributes: return FIRType::Material();
			case MCT_Substrate: return FIRType::Substrate();
			default: break;
			}
			return FIRType::Error();
		}

		static FIRType TypeFromFunctionInputType(const EFunctionInputType InputType)
		{
			switch (InputType)
			{
			case FunctionInput_Scalar: return FIRType::Float(1);
			case FunctionInput_Vector2: return FIRType::Float(2);
			case FunctionInput_Vector3: return FIRType::Float(3);
			case FunctionInput_Vector4: return FIRType::Float(4);
			case FunctionInput_Texture2D: return FIRType::TextureOf(ETextureKind::Texture2D);
			case FunctionInput_TextureCube: return FIRType::TextureOf(ETextureKind::TextureCube);
			case FunctionInput_Texture2DArray: return FIRType::TextureOf(ETextureKind::Texture2DArray);
			case FunctionInput_VolumeTexture: return FIRType::TextureOf(ETextureKind::VolumeTexture);
			case FunctionInput_TextureExternal: return FIRType::TextureOf(ETextureKind::Texture2D);
			case FunctionInput_StaticBool:
			case FunctionInput_Bool:
				return FIRType::Bool(1);
			case FunctionInput_MaterialAttributes: return FIRType::Material();
			case FunctionInput_Substrate: return FIRType::Substrate();
			default: break;
			}
			return FIRType::Float(1);
		}

		static FIRType TypeFromCustomOutputType(const ECustomMaterialOutputType OutputType)
		{
			switch (OutputType)
			{
			case CMOT_Float2: return FIRType::Float(2);
			case CMOT_Float3: return FIRType::Float(3);
			case CMOT_Float4: return FIRType::Float(4);
			case CMOT_MaterialAttributes: return FIRType::Material();
			default: break;
			}
			return FIRType::Float(1);
		}

		static bool IsImportableProperty(const FProperty* Property)
		{
			// The catalog's own filter (IsCatalogProperty): editable, not deprecated, not transient.
			return Property
				&& !Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient | CPF_DuplicateTransient)
				&& Property->HasAnyPropertyFlags(CPF_Edit)
				&& !IsMaterialExpressionInputProperty(Property);
		}

		static FString MakeImportPinName(const FProperty* Property, const int32 ArrayIndex)
		{
			return Property->ArrayDim <= 1
				? Property->GetName()
				: FString::Printf(TEXT("%s[%d]"), *Property->GetName(), ArrayIndex); /* I18N-EXEMPT: pin identifier */
		}

		// ---------------------------------------------------------------------------------- the importer

		class FGraphImporter
		{
		public:
			FGraphImporter(
				const FGraphImportOptions& InOptions,
				const UE::DreamShader::IR::FBuiltinCatalog& InCatalog,
				FIRProduct& InProduct,
				FGraphImportContext& InContext,
				UE::DreamShader::Lang::FLangDiagnosticSink& InDiagnostics)
				: Options(InOptions)
				, Catalog(InCatalog)
				, Product(InProduct)
				, Graph(InProduct.Graph)
				, Context(InContext)
				, Diagnostics(InDiagnostics)
			{
				// Class short name -> the core op the emitter writes that class for.
				for (int32 OpIndex = 0; OpIndex < static_cast<int32>(EIROp::Count); ++OpIndex)
				{
					const EIROp Op = static_cast<EIROp>(OpIndex);
					const UE::DreamShader::IR::FIRCoreOpInfo& Info = UE::DreamShader::IR::GetCoreOpInfo(Op);
					if (UE::DreamShader::IR::IsCoreMathOp(Op) && Info.ExpressionClass && Info.InputPins[0])
					{
						CoreOpByClass.Add(Info.ExpressionClass, Op);
					}
				}
			}

			void ImportMaterial(UMaterial* Material);
			void ImportFunction(UMaterialFunction* Function);

		private:
			enum class EState : uint8
			{
				Visiting,
				Done,
			};

			struct FHints
			{
				bool bValid = false;
				TMap<FGuid, FString> Names;
				TMap<FGuid, int32> NodeRegions;
			};

			struct FBox
			{
				FString Title;
				int32 X = 0;
				int32 Y = 0;
				int32 W = 0;
				int32 H = 0;
				FLinearColor Color = FLinearColor::White;
				bool bGenerated = false;
				int32 Region = INDEX_NONE;
				int32 NodesInside = 0;
			};

			// ----- diagnostics
			void Info(const TCHAR* Code, const FText& Message) { Diagnostics.Info(Code, UE::DreamShader::Lang::FLangSpan(), Message); }
			void Warning(const TCHAR* Code, const FText& Message) { Diagnostics.Warning(Code, UE::DreamShader::Lang::FLangSpan(), Message); }
			FText DescribeExpression(const UMaterialExpression* Expression) const;

			// ----- the walk
			void EnsureImported(UMaterialExpression* Root);
			void CollectDependencies(UMaterialExpression* Expression, TArray<UMaterialExpression*>& OutDependencies);
			/** The expression and output a pin ends up reading, reroutes followed; the pins crossed on the way, outermost first. */
			bool TraceInput(const FExpressionInput& Input, UMaterialExpression*& OutSource, int32& OutOutputIndex, TArray<const FExpressionInput*>& OutHops);
			FIRValue ResolveInput(const FExpressionInput& Input);
			FIRValue ValueOfOutput(UMaterialExpression* Expression, int32 OutputIndex);
			FIRValue MakeSwizzle(FIRValue Value, const FString& Mask);
			FIRValue MakeZero(int32 Width);

			// ----- nodes
			void BuildNode(UMaterialExpression* Expression);
			int32 AddNode(FIRNode&& Node, const UMaterialExpression* Expression);
			/** One of the several nodes an expression is read back as: in the expression's region, with no name or position of its own. */
			FIRValue AddPartNode(FIRNode&& Node, const UMaterialExpression* Expression);
			FIRType TypeOf(const FIRValue& Value) const { return Graph.IsValidValue(Value) ? Graph.TypeOf(Value) : FIRType::Error(); }
			int32 WidthOf(const FIRValue& Value) const { return TypeOf(Value).GraphComponentCount(); }

			bool ImportConstant(UMaterialExpression* Expression);
			bool ImportParameter(UMaterialExpression* Expression);
			bool ImportStaticSwitchParameter(UMaterialExpression* Expression);
			bool ImportTextureSampleParameter(UMaterialExpression* Expression);
			bool ImportFunctionInput(UMaterialExpression* Expression);
			bool ImportFunctionCall(UMaterialExpression* Expression);
			bool ImportCustom(UMaterialExpression* Expression);
			bool ImportMaterialAttributes(UMaterialExpression* Expression);
			bool ImportStructural(UMaterialExpression* Expression);
			bool ImportConvert(UMaterialExpression* Expression);
			bool ImportSwitch(UMaterialExpression* Expression);
			bool ImportTextureSample(UMaterialExpressionTextureSample* Sample, FIRValue Texture, const UMaterialExpression* Owner);
			bool ImportCoreMath(UMaterialExpression* Expression);
			void ImportReflected(UMaterialExpression* Expression);

			void AddParameterMetadata(FIRNode& Node, const UMaterialExpression* Expression, const TSet<FName>& Handled);
			/** The catalog's outputs onto a reflected node that has none yet; how many it has now. */
			int32 AddCatalogOutputs(int32 NodeIndex, UMaterialExpression* Expression);
			bool ReadProperty(const UObject* Object, const FProperty* Property, FIRPropertyValue& OutValue) const;
			bool AreOtherPropertiesDefault(const UMaterialExpression* Expression, const TSet<FName>& Ignored) const;
			bool CanBeCoreSample(const UMaterialExpressionTextureSample* Sample, bool& bOutHasLevel) const;
			FIRType ResolveOutputType(UMaterialExpression* Expression, int32 OutputIndex, UE::DreamShader::IR::ECatalogValueType CatalogType, const FIRNode& Node) const;

			// ----- what the graph cannot say
			void ReadHints(UObject* Asset);
			void ReadBoxes(const TConstArrayView<TObjectPtr<UMaterialExpressionComment>> Comments);
			void AssignRegions();
			void AddCommentHints();
			void ReportUnreachable(const TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions);
			void AddStatementRoots(const TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions);

			const FGraphImportOptions& Options;
			const UE::DreamShader::IR::FBuiltinCatalog& Catalog;
			FIRProduct& Product;
			FIRGraph& Graph;
			FGraphImportContext& Context;
			UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics;

			TMap<FString, EIROp> CoreOpByClass;
			TMap<const UMaterialExpression*, EState> States;
			TMap<const UMaterialExpression*, int32> NodeOfExpression;
			/** Expressions that are another value outright: a folded StaticSwitch, a GetMaterialAttributes' output 0. */
			TMap<TPair<const UMaterialExpression*, int32>, FIRValue> ForwardedOutputs;
			TMap<TPair<const UMaterialExpression*, int32>, FIRValue> OutputCache;
			/** Break / GetMaterialAttributes: engine output index -> slot. */
			TMap<int32, TArray<int32>> OutputSlotsOfNode;
			TMap<FString, FIRValue> SwizzleCache;
			/** Reroutes already reported as broken: every pin that reads one walks over it again. */
			TSet<const UMaterialExpression*> ReportedReroutes;
			/** Node index -> the expression it was read from, for positions and regions. */
			TMap<int32, const UMaterialExpression*> ExpressionOfNode;

			FHints Hints;
			TArray<FBox> Boxes;
			int32 NextLayoutBinding = 0;
		};

		FText FGraphImporter::DescribeExpression(const UMaterialExpression* Expression) const
		{
			if (!Expression)
			{
				return LOCTEXT("DescribeNoExpression", "an expression");
			}
			return FText::Format(
				LOCTEXT("DescribeExpression", "'{0}' ({1}) of '{2}'"),
				FText::FromString(Expression->GetName()),
				FText::FromString(GetMaterialExpressionShortName(Expression->GetClass())),
				FText::FromString(Product.Name));
		}

		// ------------------------------------------------------------------------------------- the walk

		bool FGraphImporter::TraceInput(const FExpressionInput& Input, UMaterialExpression*& OutSource, int32& OutOutputIndex, TArray<const FExpressionInput*>& OutHops)
		{
			OutSource = nullptr;
			OutOutputIndex = 0;
			OutHops.Reset();

			const FExpressionInput* Current = &Input;
			TSet<const UMaterialExpression*> Seen;
			while (Current && Current->Expression)
			{
				UMaterialExpression* Expression = Current->Expression;
				OutHops.Add(Current);

				const FExpressionInput* Next = nullptr;
				if (const UMaterialExpressionReroute* Reroute = Cast<UMaterialExpressionReroute>(Expression))
				{
					Next = &Reroute->Input;
				}
				else if (const UMaterialExpressionNamedRerouteUsage* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Expression))
				{
					if (!IsValid(Usage->Declaration))
					{
						if (!ReportedReroutes.Contains(Expression))
						{
							ReportedReroutes.Add(Expression);
							Warning(TEXT("DSH9063"), FText::Format(
								LOCTEXT("RerouteUsageWithoutDeclaration", "The named reroute {0} has no declaration; the pin that reads it is treated as unconnected."),
								DescribeExpression(Expression)));
						}
						return false;
					}
					Next = &Usage->Declaration->Input;
				}
				else if (const UMaterialExpressionNamedRerouteDeclaration* Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Expression))
				{
					Next = &Declaration->Input;
				}
				else
				{
					OutSource = Expression;
					OutOutputIndex = Current->OutputIndex;
					return true;
				}

				if (Seen.Contains(Expression))
				{
					if (!ReportedReroutes.Contains(Expression))
					{
						ReportedReroutes.Add(Expression);
						Warning(TEXT("DSH9062"), FText::Format(
							LOCTEXT("RerouteCycle", "The reroute {0} feeds itself; the pin that reads it is treated as unconnected."),
							DescribeExpression(Expression)));
					}
					return false;
				}
				Seen.Add(Expression);
				Current = Next;
			}

			// A reroute nothing is wired into.
			if (OutHops.Num() > 0 && !ReportedReroutes.Contains(OutHops.Last()->Expression))
			{
				ReportedReroutes.Add(OutHops.Last()->Expression);
				Warning(TEXT("DSH9063"), FText::Format(
					LOCTEXT("RerouteDangling", "The reroute {0} has nothing wired into it; the pin that reads it is treated as unconnected."),
					DescribeExpression(OutHops.Last()->Expression)));
			}
			return false;
		}

		void FGraphImporter::CollectDependencies(UMaterialExpression* Expression, TArray<UMaterialExpression*>& OutDependencies)
		{
			OutDependencies.Reset();
			// A function input's Preview pin is the editor's, not the function's.
			if (Expression->IsA<UMaterialExpressionFunctionInput>())
			{
				return;
			}

			TArray<const FExpressionInput*> Hops;
			for (int32 InputIndex = 0; FExpressionInput* Input = Expression->GetInput(InputIndex); ++InputIndex)
			{
				if (!Input->Expression)
				{
					continue;
				}
				UMaterialExpression* Source = nullptr;
				int32 OutputIndex = 0;
				if (TraceInput(*Input, Source, OutputIndex, Hops) && Source)
				{
					OutDependencies.AddUnique(Source);
				}
			}
		}

		void FGraphImporter::EnsureImported(UMaterialExpression* Root)
		{
			if (!Root || States.Contains(Root))
			{
				return;
			}

			struct FFrame
			{
				UMaterialExpression* Expression = nullptr;
				bool bExpanded = false;
			};

			TArray<FFrame> Stack;
			Stack.Add(FFrame{ Root, false });
			TArray<UMaterialExpression*> Dependencies;

			while (Stack.Num() > 0)
			{
				UMaterialExpression* Expression = Stack.Last().Expression;
				const EState* State = States.Find(Expression);
				if (State && *State == EState::Done)
				{
					Stack.Pop();
					continue;
				}

				if (!Stack.Last().bExpanded)
				{
					Stack.Last().bExpanded = true;
					if (State && *State == EState::Visiting)
					{
						// A second frame of something already being expanded further down: that one builds it.
						Stack.Pop();
						continue;
					}
					States.Add(Expression, EState::Visiting);

					CollectDependencies(Expression, Dependencies);
					for (UMaterialExpression* Dependency : Dependencies)
					{
						const EState* DependencyState = States.Find(Dependency);
						if (!DependencyState)
						{
							Stack.Add(FFrame{ Dependency, false });
						}
						else if (*DependencyState == EState::Visiting)
						{
							Warning(TEXT("DSH9062"), FText::Format(
								LOCTEXT("GraphCycle", "{0} reads {1}, which reads it back; the graph has a cycle, and a zero stands in for that read."),
								DescribeExpression(Expression),
								DescribeExpression(Dependency)));
						}
					}
					continue;
				}

				BuildNode(Expression);
				States.Add(Expression, EState::Done);
				Stack.Pop();
			}
		}

		FIRValue FGraphImporter::MakeZero(const int32 Width)
		{
			FIRNode Node;
			Node.Op = EIROp::Constant;
			const int32 Components = FMath::Clamp(Width, 1, 4);
			const double Zero[4] = { 0.0, 0.0, 0.0, 0.0 };
			Node.Properties.Add({ FString(UE::DreamShader::IR::Prop::Value), FIRPropertyValue::MakeFloat4(Zero, Components) });
			Node.Outputs.Add(FIRType::Float(Components));
			return FIRValue{ Graph.AddNode(MoveTemp(Node)), 0 };
		}

		FIRValue FGraphImporter::MakeSwizzle(const FIRValue Value, const FString& Mask)
		{
			if (!Value.IsValid() || Mask.IsEmpty())
			{
				return Value;
			}

			const FString Key = FString::Printf(TEXT("%d#%d#%s"), Value.Node, Value.Output, *Mask);
			if (const FIRValue* Existing = SwizzleCache.Find(Key))
			{
				return *Existing;
			}

			FIRNode Node;
			Node.Op = EIROp::Swizzle;
			Node.Operands.Add(Value);
			Node.Properties.Add({ FString(UE::DreamShader::IR::Prop::Mask), FIRPropertyValue::MakeString(Mask) });
			Node.Outputs.Add(FIRType::Float(Mask.Len()));
			// In the box its operand is in: an output selection has no node of its own to be anywhere else.
			if (Graph.Nodes.IsValidIndex(Value.Node))
			{
				Node.Region = Graph.Nodes[Value.Node].Region;
			}

			const FIRValue Result{ Graph.AddNode(MoveTemp(Node)), 0 };
			SwizzleCache.Add(Key, Result);
			return Result;
		}

		FIRValue FGraphImporter::ValueOfOutput(UMaterialExpression* Expression, const int32 OutputIndex)
		{
			const TPair<const UMaterialExpression*, int32> Key(Expression, OutputIndex);
			if (const FIRValue* Forwarded = ForwardedOutputs.Find(Key))
			{
				return *Forwarded;
			}
			if (const FIRValue* Cached = OutputCache.Find(Key))
			{
				return *Cached;
			}

			const int32* NodeIndexPtr = NodeOfExpression.Find(Expression);
			if (!NodeIndexPtr)
			{
				// Read before it was built: the far side of a cycle, already reported.
				return MakeZero(1);
			}
			const int32 NodeIndex = *NodeIndexPtr;
			// By value: MakeSwizzle and MakeZero below add nodes, and the array may move.
			const EIROp NodeOp = Graph.Nodes[NodeIndex].Op;
			const int32 NodeOutputCount = Graph.Nodes[NodeIndex].Outputs.Num();

			FIRValue Result{ NodeIndex, 0 };
			const FExpressionOutput* Output = Expression->Outputs.IsValidIndex(OutputIndex) ? &Expression->Outputs[OutputIndex] : nullptr;

			if (const TArray<int32>* Slots = OutputSlotsOfNode.Find(NodeIndex))
			{
				const int32 Slot = Slots->IsValidIndex(OutputIndex) ? (*Slots)[OutputIndex] : INDEX_NONE;
				if (Slot >= 0 && Slot < NodeOutputCount)
				{
					Result.Output = Slot;
				}
				else
				{
					Warning(TEXT("DSH9072"), FText::Format(
						LOCTEXT("AttributeOutputUnknown", "{0} is read through its output {1}, which is no material attribute the catalog knows; a zero stands in for that read."),
						DescribeExpression(Expression),
						FText::AsNumber(OutputIndex)));
					Result = MakeZero(1);
				}
			}
			else if (NodeOp == EIROp::TextureSample)
			{
				// IR slots RGBA, R, G, B, A; the engine's RGB is the leading three of RGBA.
				const FString Name = Output ? Output->OutputName.ToString() : FString();
				if (Name.Equals(TEXT("R"), ESearchCase::IgnoreCase)) { Result.Output = 1; }
				else if (Name.Equals(TEXT("G"), ESearchCase::IgnoreCase)) { Result.Output = 2; }
				else if (Name.Equals(TEXT("B"), ESearchCase::IgnoreCase)) { Result.Output = 3; }
				else if (Name.Equals(TEXT("A"), ESearchCase::IgnoreCase)) { Result.Output = 4; }
				else if (Name.Equals(TEXT("RGB"), ESearchCase::IgnoreCase)) { Result = MakeSwizzle(FIRValue{ NodeIndex, 0 }, TEXT("xyz")); }
			}
			else if (NodeOp == EIROp::Reflected || NodeOp == EIROp::FunctionCall || NodeOp == EIROp::Custom)
			{
				// A custom-output class is read back as a statement, with no output; the one that also hands a value on
				// (VertexInterpolator's PS) is that value as soon as something reads it, as the builder makes it.
				int32 OutputCount = NodeOutputCount;
				if (OutputCount == 0 && NodeOp == EIROp::Reflected)
				{
					OutputCount = AddCatalogOutputs(NodeIndex, Expression);
				}

				if (OutputIndex >= 0 && OutputIndex < OutputCount)
				{
					Result.Output = OutputIndex;
				}
				else if (OutputCount > 0)
				{
					Warning(TEXT("DSH9072"), FText::Format(
						LOCTEXT("OutputPastCatalog", "{0} is read through its output {1}, which the node has no slot for; its first output is read instead."),
						DescribeExpression(Expression),
						FText::AsNumber(OutputIndex)));
				}
				else
				{
					Warning(TEXT("DSH9072"), FText::Format(
						LOCTEXT("OutputOfStatement", "{0} is read through its output {1}, and the catalog lists no output for its class; a zero stands in for that read."),
						DescribeExpression(Expression),
						FText::AsNumber(OutputIndex)));
					Result = MakeZero(1);
				}
			}
			else if (Output && Output->Mask != 0)
			{
				// A value node: an output that is a channel view of the value is a Swizzle of it, unless it is all of it.
				const int32 Width = FMath::Clamp(WidthOf(FIRValue{ NodeIndex, 0 }), 1, 4);
				const bool bChannels[4] = { Output->MaskR != 0, Output->MaskG != 0, Output->MaskB != 0, Output->MaskA != 0 };
				FString Mask;
				for (int32 Channel = 0; Channel < Width; ++Channel)
				{
					if (bChannels[Channel])
					{
						Mask.AppendChar(SwizzleComponents[Channel]);
					}
				}
				if (!Mask.IsEmpty() && Mask.Len() < Width)
				{
					Result = MakeSwizzle(FIRValue{ NodeIndex, 0 }, Mask);
				}
			}

			OutputCache.Add(Key, Result);
			return Result;
		}

		FIRValue FGraphImporter::ResolveInput(const FExpressionInput& Input)
		{
			if (!Input.Expression)
			{
				return FIRValue::None();
			}

			UMaterialExpression* Source = nullptr;
			int32 OutputIndex = 0;
			TArray<const FExpressionInput*> Hops;
			if (!TraceInput(Input, Source, OutputIndex, Hops) || !Source)
			{
				return FIRValue::None();
			}

			FIRValue Value = ValueOfOutput(Source, OutputIndex);

			// Every pin crossed may carry an inline mask; the innermost one is the one that reads the source's output.
			for (int32 HopIndex = Hops.Num() - 1; HopIndex >= 0; --HopIndex)
			{
				const FExpressionInput& Hop = *Hops[HopIndex];
				const FExpressionOutput* SourceOutput = (HopIndex == Hops.Num() - 1 && Source->Outputs.IsValidIndex(OutputIndex))
					? &Source->Outputs[OutputIndex]
					: nullptr;
				// The value already IS the masked output's channels, so the output only decides what the bits refer to.
				const FDreamShaderInlineMask Mask = ResolveDreamShaderInlineMask(Hop, SourceOutput, WidthOf(Value));
				if (Mask.bMasked && !Mask.bIdentity && WidthOf(Value) > 0)
				{
					Value = MakeSwizzle(Value, Mask.Relative);
				}
			}

			// A named reroute is a name somebody gave the value.
			if (!Hints.bValid && Graph.Nodes.IsValidIndex(Value.Node) && Graph.Nodes[Value.Node].DebugName.IsEmpty())
			{
				for (const FExpressionInput* Hop : Hops)
				{
					const UMaterialExpressionNamedRerouteDeclaration* Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Hop->Expression);
					if (const UMaterialExpressionNamedRerouteUsage* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Hop->Expression))
					{
						Declaration = Usage->Declaration;
					}
					if (Declaration && !Declaration->Name.IsNone() && !Declaration->Name.ToString().StartsWith(TEXT("DS_"), ESearchCase::CaseSensitive))
					{
						Graph.Nodes[Value.Node].DebugName = Declaration->Name.ToString();
						break;
					}
				}
			}
			return Value;
		}

		// -------------------------------------------------------------------------------------- nodes

		int32 FGraphImporter::AddNode(FIRNode&& Node, const UMaterialExpression* Expression)
		{
			if (Expression)
			{
				if (const FString* Name = Hints.Names.Find(Expression->MaterialExpressionGuid))
				{
					Node.DebugName = *Name;
				}
				if (const int32* Region = Hints.NodeRegions.Find(Expression->MaterialExpressionGuid))
				{
					Node.Region = *Region;
				}
			}

			const int32 NodeIndex = Graph.AddNode(MoveTemp(Node));
			if (Expression)
			{
				ExpressionOfNode.Add(NodeIndex, Expression);

				if (Options.bImportLayout)
				{
					// `$n`: BuildDreamShaderAstFromIR writes the position under whatever the node's value ends up called,
					// and the binding follows the node through the passes (IRToAst.h).
					const FString BindingName = FString::Printf(TEXT("$%d"), NextLayoutBinding++);

					UE::DreamShader::IR::FIRStatementBinding Binding;
					Binding.Name = BindingName;
					Binding.Value = FIRValue{ NodeIndex, 0 };
					Graph.StatementBindings.Add(MoveTemp(Binding));

					UE::DreamShader::IR::FIRLayoutHint Hint;
					Hint.Kind = TEXT("Node");
					Hint.Var = BindingName;
					Hint.X = Expression->MaterialExpressionEditorX;
					Hint.Y = Expression->MaterialExpressionEditorY;
					Graph.LayoutHints.Add(MoveTemp(Hint));
				}
			}
			return NodeIndex;
		}

		FIRValue FGraphImporter::AddPartNode(FIRNode&& Node, const UMaterialExpression* Expression)
		{
			// The region AddNode would give it; AssignRegions reads the rest off ExpressionOfNode. No `$n` binding: the
			// expression's position belongs to none of its parts more than to another.
			if (const int32* Region = Hints.NodeRegions.Find(Expression->MaterialExpressionGuid))
			{
				Node.Region = *Region;
			}
			const int32 NodeIndex = Graph.AddNode(MoveTemp(Node));
			ExpressionOfNode.Add(NodeIndex, Expression);
			return FIRValue{ NodeIndex, 0 };
		}

		bool FGraphImporter::ReadProperty(const UObject* Object, const FProperty* Property, FIRPropertyValue& OutValue) const
		{
			const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
			if (!ValuePtr || Property->ArrayDim != 1)
			{
				return false;
			}

			if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
			{
				OutValue = FIRPropertyValue::MakeBool(BoolProperty->GetPropertyValue(ValuePtr));
				return true;
			}
			if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
			{
				OutValue = FIRPropertyValue::MakeEnum(MakeImportEnumSpelling(
					EnumProperty->GetEnum(),
					EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr)));
				return !OutValue.S.IsEmpty();
			}
			if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
			{
				if (ByteProperty->Enum)
				{
					OutValue = FIRPropertyValue::MakeEnum(MakeImportEnumSpelling(ByteProperty->Enum, ByteProperty->GetPropertyValue(ValuePtr)));
					return !OutValue.S.IsEmpty();
				}
				OutValue = FIRPropertyValue::MakeInt(ByteProperty->GetPropertyValue(ValuePtr));
				return true;
			}
			if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
			{
				if (NumericProperty->IsFloatingPoint())
				{
					OutValue = FIRPropertyValue::MakeFloat(NumericProperty->GetFloatingPointPropertyValue(ValuePtr));
				}
				else
				{
					OutValue = FIRPropertyValue::MakeInt(NumericProperty->GetSignedIntPropertyValue(ValuePtr));
				}
				return true;
			}
			if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
			{
				const FName Name = NameProperty->GetPropertyValue(ValuePtr);
				OutValue = FIRPropertyValue::MakeName(Name.IsNone() ? FString() : Name.ToString());
				return true;
			}
			if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
			{
				OutValue = FIRPropertyValue::MakeString(StringProperty->GetPropertyValue(ValuePtr));
				return true;
			}
			if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
			{
				OutValue = FIRPropertyValue::MakeString(TextProperty->GetPropertyValue(ValuePtr).ToString());
				return true;
			}
			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				const UObject* Referenced = ObjectProperty->GetObjectPropertyValue(ValuePtr);
				OutValue = FIRPropertyValue::MakeObject(Referenced ? NormalizeImportAssetPath(Referenced->GetPathName()) : FString());
				return true;
			}
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
				{
					const FLinearColor& Color = *static_cast<const FLinearColor*>(ValuePtr);
					const double Components[4] = { Color.R, Color.G, Color.B, Color.A };
					OutValue = FIRPropertyValue::MakeFloat4(Components, 4);
					return true;
				}
				// Any other struct in the spelling ImportText reads back; the builder hands a string through untouched.
				FString Text;
				Property->ExportTextItem_Direct(Text, ValuePtr, nullptr, nullptr, PPF_None);
				OutValue = FIRPropertyValue::MakeString(Text);
				return !Text.IsEmpty();
			}
			return false;
		}

		bool FGraphImporter::AreOtherPropertiesDefault(const UMaterialExpression* Expression, const TSet<FName>& Ignored) const
		{
			const UObject* Defaults = Expression->GetClass()->GetDefaultObject(false);
			if (!Defaults)
			{
				return false;
			}
			for (TFieldIterator<FProperty> It(Expression->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				const FProperty* Property = *It;
				// Desc is a note on the node, not part of what it computes.
				if (!IsImportableProperty(Property) || Ignored.Contains(Property->GetFName()) || Property->GetFName() == FName(TEXT("Desc")))
				{
					continue;
				}
				if (!Property->Identical_InContainer(Expression, Defaults))
				{
					return false;
				}
			}
			return true;
		}

		bool FGraphImporter::ImportConstant(UMaterialExpression* Expression)
		{
			double Components[4] = { 0.0, 0.0, 0.0, 0.0 };
			int32 Width = 0;
			if (const UMaterialExpressionConstant* Scalar = Cast<UMaterialExpressionConstant>(Expression))
			{
				Components[0] = Scalar->R;
				Width = 1;
			}
			else if (const UMaterialExpressionConstant2Vector* Vector2 = Cast<UMaterialExpressionConstant2Vector>(Expression))
			{
				Components[0] = Vector2->R;
				Components[1] = Vector2->G;
				Width = 2;
			}
			else if (const UMaterialExpressionConstant3Vector* Vector3 = Cast<UMaterialExpressionConstant3Vector>(Expression))
			{
				// The alpha is not part of a float3: the emitter writes 1 there whatever the source said.
				Components[0] = Vector3->Constant.R;
				Components[1] = Vector3->Constant.G;
				Components[2] = Vector3->Constant.B;
				Width = 3;
			}
			else if (const UMaterialExpressionConstant4Vector* Vector4 = Cast<UMaterialExpressionConstant4Vector>(Expression))
			{
				Components[0] = Vector4->Constant.R;
				Components[1] = Vector4->Constant.G;
				Components[2] = Vector4->Constant.B;
				Components[3] = Vector4->Constant.A;
				Width = 4;
			}
			if (Width == 0)
			{
				return false;
			}

			FIRNode Node;
			Node.Op = EIROp::Constant;
			Node.Properties.Add({ FString(UE::DreamShader::IR::Prop::Value), FIRPropertyValue::MakeFloat4(Components, Width) });
			Node.Outputs.Add(FIRType::Float(Width));
			NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));
			return true;
		}

		void FGraphImporter::AddParameterMetadata(FIRNode& Node, const UMaterialExpression* Expression, const TSet<FName>& Handled)
		{
			namespace Prop = UE::DreamShader::IR::Prop;

			const UClass* Class = Expression->GetClass();
			const auto ReadName = [Expression, Class](const TCHAR* PropertyName) -> FString
			{
				const FNameProperty* Property = FindFProperty<FNameProperty>(Class, PropertyName);
				const FName Value = Property ? Property->GetPropertyValue_InContainer(Expression) : NAME_None;
				return Value.IsNone() ? FString() : Value.ToString();
			};

			Node.Properties.Add({ FString(Prop::ParameterName), FIRPropertyValue::MakeName(ReadName(TEXT("ParameterName"))) });
			const FString Group = ReadName(TEXT("Group"));
			if (!Group.IsEmpty())
			{
				Node.Properties.Add({ FString(Prop::Group), FIRPropertyValue::MakeName(Group) });
			}
			if (!Expression->Desc.IsEmpty())
			{
				Node.Properties.Add({ FString(Prop::Description), FIRPropertyValue::MakeString(Expression->Desc) });
			}
			if (const FIntProperty* SortPriority = FindFProperty<FIntProperty>(Class, TEXT("SortPriority")))
			{
				Node.Properties.Add({ FString(Prop::SortPriority), FIRPropertyValue::MakeInt(SortPriority->GetPropertyValue_InContainer(Expression)) });
			}

			// Whatever else was changed on the parameter rides along under its engine name: `/// @<Name> <value>`.
			const UObject* Defaults = Class->GetDefaultObject(false);
			for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				const FProperty* Property = *It;
				const FName Name = Property->GetFName();
				if (!IsImportableProperty(Property)
					|| Handled.Contains(Name)
					|| Name == FName(TEXT("ParameterName"))
					|| Name == FName(TEXT("Group"))
					|| Name == FName(TEXT("SortPriority"))
					|| Name == FName(TEXT("Desc"))
					|| (Defaults && Property->Identical_InContainer(Expression, Defaults)))
				{
					continue;
				}

				FIRPropertyValue Value;
				if (!ReadProperty(Expression, Property, Value) || Value.Kind == UE::DreamShader::IR::EIRPropertyKind::Float4)
				{
					Warning(TEXT("DSH9068"), FText::Format(
						LOCTEXT("ParameterPropertySkipped", "{0} changes '{1}', which a '///' directive cannot carry; the rebuilt parameter has the default."),
						DescribeExpression(Expression),
						FText::FromString(Property->GetName())));
					continue;
				}
				// A passthrough is text: the emitter hands it to the literal writer under the property's name.
				Node.Properties.Add({ Property->GetName(), FIRPropertyValue::MakeString(
					Value.Kind == UE::DreamShader::IR::EIRPropertyKind::Bool ? FString(Value.B ? TEXT("true") : TEXT("false"))
					: Value.Kind == UE::DreamShader::IR::EIRPropertyKind::Int ? FString::Printf(TEXT("%lld"), Value.I)
					: Value.Kind == UE::DreamShader::IR::EIRPropertyKind::Float ? FString::SanitizeFloat(Value.F)
					: Value.S) });
			}
		}

		bool FGraphImporter::ImportParameter(UMaterialExpression* Expression)
		{
			namespace Prop = UE::DreamShader::IR::Prop;
			const UClass* Class = Expression->GetClass();

			// Exact classes only: CurveAtlasRowParameter is a ScalarParameter and ChannelMaskParameter a VectorParameter
			// as far as Cast is concerned, and neither is what the emitter makes for a uniform.
			if (Class == UMaterialExpressionScalarParameter::StaticClass())
			{
				const UMaterialExpressionScalarParameter* Scalar = CastChecked<UMaterialExpressionScalarParameter>(Expression);
				FIRNode Node;
				Node.Op = EIROp::Parameter;
				Node.Outputs.Add(FIRType::Float(1));
				AddParameterMetadata(Node, Expression, { FName(TEXT("DefaultValue")), FName(TEXT("SliderMin")), FName(TEXT("SliderMax")) });
				// The binder wants min < max (DSH7221's neighbour); the engine's "no slider" is max <= min.
				if (Scalar->SliderMax > Scalar->SliderMin)
				{
					Node.Properties.Add({ FString(Prop::SliderMin), FIRPropertyValue::MakeFloat(Scalar->SliderMin) });
					Node.Properties.Add({ FString(Prop::SliderMax), FIRPropertyValue::MakeFloat(Scalar->SliderMax) });
				}
				const double Default[4] = { Scalar->DefaultValue, 0.0, 0.0, 0.0 };
				Node.Properties.Add({ FString(Prop::DefaultValue), FIRPropertyValue::MakeFloat4(Default, 1) });
				NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));
				return true;
			}

			if (Class == UMaterialExpressionVectorParameter::StaticClass())
			{
				const UMaterialExpressionVectorParameter* Vector = CastChecked<UMaterialExpressionVectorParameter>(Expression);
				FIRNode Node;
				Node.Op = EIROp::Parameter;
				Node.Outputs.Add(FIRType::Float(4));
				AddParameterMetadata(Node, Expression, { FName(TEXT("DefaultValue")) });
				const double Default[4] = { Vector->DefaultValue.R, Vector->DefaultValue.G, Vector->DefaultValue.B, Vector->DefaultValue.A };
				Node.Properties.Add({ FString(Prop::DefaultValue), FIRPropertyValue::MakeFloat4(Default, 4) });
				NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));
				return true;
			}

			if (Class == UMaterialExpressionStaticBoolParameter::StaticClass())
			{
				const UMaterialExpressionStaticBoolParameter* StaticBool = CastChecked<UMaterialExpressionStaticBoolParameter>(Expression);
				FIRNode Node;
				Node.Op = EIROp::Parameter;
				Node.Outputs.Add(FIRType::Bool(1));
				Node.Properties.Add({ FString(Prop::IsStatic), FIRPropertyValue::MakeBool(true) });
				AddParameterMetadata(Node, Expression, { FName(TEXT("DefaultValue")) });
				const double Default[4] = { StaticBool->DefaultValue ? 1.0 : 0.0, 0.0, 0.0, 0.0 };
				Node.Properties.Add({ FString(Prop::DefaultValue), FIRPropertyValue::MakeFloat4(Default, 1) });
				NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));
				return true;
			}

			if (Class == UMaterialExpressionTextureObjectParameter::StaticClass())
			{
				const UMaterialExpressionTextureObjectParameter* Texture = CastChecked<UMaterialExpressionTextureObjectParameter>(Expression);
				FIRNode Node;
				Node.Op = EIROp::TextureParameter;
				Node.Outputs.Add(FIRType::TextureOf(Texture->Texture ? TextureKindFromValueType(Texture->Texture->GetMaterialType()) : ETextureKind::Texture2D));
				AddParameterMetadata(Node, Expression, { FName(TEXT("Texture")), FName(TEXT("SamplerType")) });
				if (Texture->Texture)
				{
					Node.Properties.Add({ FString(Prop::DefaultAsset), FIRPropertyValue::MakeObject(NormalizeImportAssetPath(Texture->Texture->GetPathName())) });
				}
				// Always: a TextureSample copies it off the parameter, and "absent" reads as Color there.
				Node.Properties.Add({ FString(Prop::SamplerType), FIRPropertyValue::MakeEnum(
					MakeImportEnumSpelling(StaticEnum<EMaterialSamplerType>(), static_cast<int64>(Texture->SamplerType.GetValue()))) });
				NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));
				return true;
			}

			return false;
		}

		bool FGraphImporter::ImportStaticSwitchParameter(UMaterialExpression* Expression)
		{
			// RD-3: the 2.0 emitter never makes this class; a `/// @static uniform bool` and a StaticSwitch say the same.
			namespace Prop = UE::DreamShader::IR::Prop;
			const UMaterialExpressionStaticSwitchParameter* Switch = Cast<UMaterialExpressionStaticSwitchParameter>(Expression);
			if (!Switch || Expression->GetClass() != UMaterialExpressionStaticSwitchParameter::StaticClass())
			{
				return false;
			}

			const FIRValue IfTrue = ResolveInput(Switch->A);
			const FIRValue IfFalse = ResolveInput(Switch->B);
			if (!IfTrue.IsValid() || !IfFalse.IsValid())
			{
				return false;
			}

			FIRNode Parameter;
			Parameter.Op = EIROp::Parameter;
			Parameter.Outputs.Add(FIRType::Bool(1));
			Parameter.Properties.Add({ FString(Prop::IsStatic), FIRPropertyValue::MakeBool(true) });
			AddParameterMetadata(Parameter, Expression, { FName(TEXT("DefaultValue")) });
			const double Default[4] = { Switch->DefaultValue ? 1.0 : 0.0, 0.0, 0.0, 0.0 };
			Parameter.Properties.Add({ FString(Prop::DefaultValue), FIRPropertyValue::MakeFloat4(Default, 1) });
			const int32 ParameterNode = AddNode(MoveTemp(Parameter), nullptr);

			FIRNode Node;
			Node.Op = EIROp::StaticSwitch;
			Node.Operands = { FIRValue{ ParameterNode, 0 }, IfTrue, IfFalse };
			const FIRType TrueType = TypeOf(IfTrue);
			Node.Outputs.Add(TrueType.GraphComponentCount() > 0 ? FIRType::Float(FMath::Max(WidthOf(IfTrue), WidthOf(IfFalse))) : TrueType);
			NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));

			Info(TEXT("DSH9073"), FText::Format(
				LOCTEXT("StaticSwitchParameterSplit", "{0} is a StaticSwitchParameter, which the language writes as a '/// @static' uniform and a static branch; the rebuilt graph has those two nodes in its place."),
				DescribeExpression(Expression)));
			return true;
		}

		namespace
		{
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
			// Known to be there from 5.6: a misspelling below would otherwise read as "nothing is set".
			static_assert(
				requires(const UMaterialExpressionTextureSample* Sample) { Sample->GatherMode; },
				"UMaterialExpressionTextureSample::GatherMode is asked for under the wrong name.");
#endif

			/** A TextureSample has a gather mode from UE 5.6 on; an engine without the property has nothing set on it. */
			template <typename SampleType>
			bool HasDefaultGatherMode(const SampleType* Sample, const SampleType* Defaults)
			{
				if constexpr (requires { Sample->GatherMode; })
				{
					return Sample->GatherMode == Defaults->GatherMode;
				}
				else
				{
					return true;
				}
			}
		}

		bool FGraphImporter::CanBeCoreSample(const UMaterialExpressionTextureSample* Sample, bool& bOutHasLevel) const
		{
			bOutHasLevel = false;
			const UMaterialExpressionTextureSample* Defaults = GetDefault<UMaterialExpressionTextureSample>();
			if (!Sample->Coordinates.Expression
				|| Sample->CoordinatesDX.Expression
				|| Sample->CoordinatesDY.Expression
				|| Sample->AutomaticViewMipBiasValue.Expression
				|| Sample->SamplerSource != Defaults->SamplerSource
				|| !HasDefaultGatherMode(Sample, Defaults)
				|| Sample->AutomaticViewMipBias != Defaults->AutomaticViewMipBias)
			{
				return false;
			}

			if (Sample->MipValueMode == TMVM_None)
			{
				return !Sample->MipValue.Expression;
			}
			if (Sample->MipValueMode == TMVM_MipLevel && Sample->MipValue.Expression)
			{
				bOutHasLevel = true;
				return true;
			}
			return false;
		}

		bool FGraphImporter::ImportTextureSample(UMaterialExpressionTextureSample* Sample, const FIRValue Texture, const UMaterialExpression* Owner)
		{
			namespace Prop = UE::DreamShader::IR::Prop;

			bool bHasLevel = false;
			if (!Texture.IsValid() || !TypeOf(Texture).IsTexture() || !CanBeCoreSample(Sample, bHasLevel))
			{
				return false;
			}

			// `Tex.Sample(UV)` takes the sampler type off the texture it reads and Color off anything else, so a sample
			// that says otherwise has no such spelling.
			// Copied out: resolving the other pins below may add nodes and move the array the property lives in.
			const FString SamplerType = MakeImportEnumSpelling(StaticEnum<EMaterialSamplerType>(), static_cast<int64>(Sample->SamplerType.GetValue()));
			const UE::DreamShader::IR::FIRProperty* TextureSamplerProperty = Graph.Nodes[Texture.Node].FindProperty(Prop::SamplerType);
			const bool bTextureHasSampler = Graph.Nodes[Texture.Node].Op == EIROp::TextureParameter && TextureSamplerProperty != nullptr;
			const FIRPropertyValue TextureSampler = bTextureHasSampler ? TextureSamplerProperty->Value : FIRPropertyValue::MakeEnum(TEXT("Color"));
			if (!SamplerType.Equals(TextureSampler.S, ESearchCase::CaseSensitive))
			{
				return false;
			}

			const FIRValue Coordinates = ResolveInput(Sample->Coordinates);
			const FIRValue Level = bHasLevel ? ResolveInput(Sample->MipValue) : FIRValue::None();
			if (!Coordinates.IsValid() || (bHasLevel && !Level.IsValid()))
			{
				return false;
			}

			FIRNode Node;
			Node.Op = EIROp::TextureSample;
			Node.Operands = { Texture, Coordinates, FIRValue::None(), Level };
			if (bTextureHasSampler)
			{
				Node.Properties.Add({ FString(Prop::SamplerType), TextureSampler });
			}
			if (bHasLevel)
			{
				Node.Properties.Add({ FString(Prop::MipValueMode), FIRPropertyValue::MakeEnum(TEXT("MipLevel")) });
			}
			Node.Outputs = { FIRType::Float(4), FIRType::Float(1), FIRType::Float(1), FIRType::Float(1), FIRType::Float(1) };
			Node.OutputNames = { TEXT("RGBA"), TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A") };
			NodeOfExpression.Add(Owner, AddNode(MoveTemp(Node), Owner));
			return true;
		}

		bool FGraphImporter::ImportTextureSampleParameter(UMaterialExpression* Expression)
		{
			// RD-4: a 2.0 texture uniform is a texture OBJECT a separate sample reads. Only where the sample has the core
			// spelling; anything else stays the class it is, as a reflected call.
			namespace Prop = UE::DreamShader::IR::Prop;
			UMaterialExpressionTextureSampleParameter* Parameter = Cast<UMaterialExpressionTextureSampleParameter>(Expression);
			if (!Parameter || Expression->IsA<UMaterialExpressionTextureObjectParameter>())
			{
				return false;
			}

			bool bHasLevel = false;
			if (!CanBeCoreSample(Parameter, bHasLevel) || Parameter->TextureObject.Expression)
			{
				return false;
			}

			FIRNode Texture;
			Texture.Op = EIROp::TextureParameter;
			Texture.Outputs.Add(FIRType::TextureOf(Parameter->Texture ? TextureKindFromValueType(Parameter->Texture->GetMaterialType()) : ETextureKind::Texture2D));
			AddParameterMetadata(Texture, Expression, {
				FName(TEXT("Texture")), FName(TEXT("SamplerType")), FName(TEXT("MipValueMode")), FName(TEXT("SamplerSource")),
				FName(TEXT("AutomaticViewMipBias")), FName(TEXT("ConstCoordinate")), FName(TEXT("ConstMipValue")), FName(TEXT("GatherMode")) });
			if (Parameter->Texture)
			{
				Texture.Properties.Add({ FString(Prop::DefaultAsset), FIRPropertyValue::MakeObject(NormalizeImportAssetPath(Parameter->Texture->GetPathName())) });
			}
			Texture.Properties.Add({ FString(Prop::SamplerType), FIRPropertyValue::MakeEnum(
				MakeImportEnumSpelling(StaticEnum<EMaterialSamplerType>(), static_cast<int64>(Parameter->SamplerType.GetValue()))) });
			const int32 TextureNode = AddNode(MoveTemp(Texture), nullptr);

			if (!ImportTextureSample(Parameter, FIRValue{ TextureNode, 0 }, Expression))
			{
				return false;
			}
			Info(TEXT("DSH9073"), FText::Format(
				LOCTEXT("TextureSampleParameterSplit", "{0} is a texture sample parameter, which the language writes as a texture uniform and a sample of it; the rebuilt graph has a TextureObjectParameter and a TextureSample in its place."),
				DescribeExpression(Expression)));
			return true;
		}

		bool FGraphImporter::ImportFunctionInput(UMaterialExpression* Expression)
		{
			namespace Prop = UE::DreamShader::IR::Prop;
			const UMaterialExpressionFunctionInput* Input = Cast<UMaterialExpressionFunctionInput>(Expression);
			if (!Input)
			{
				return false;
			}
			if (NodeOfExpression.Contains(Expression))
			{
				return true;
			}

			FIRNode Node;
			Node.Op = EIROp::FunctionInput;
			Node.Outputs.Add(TypeFromFunctionInputType(Input->InputType.GetValue()));
			Node.Properties.Add({ FString(Prop::InputName), FIRPropertyValue::MakeName(Input->InputName.IsNone() ? FString() : Input->InputName.ToString()) });
			Node.Properties.Add({ FString(Prop::InputType), FIRPropertyValue::MakeEnum(MakeImportEnumSpelling(StaticEnum<EFunctionInputType>(), static_cast<int64>(Input->InputType.GetValue()))) });
			Node.Properties.Add({ FString(Prop::SortPriority), FIRPropertyValue::MakeInt(Input->SortPriority) });
			if (!Input->Description.IsEmpty())
			{
				Node.Properties.Add({ FString(Prop::Description), FIRPropertyValue::MakeString(Input->Description) });
			}
			Node.Properties.Add({ FString(Prop::IsOptional), FIRPropertyValue::MakeBool(Input->bUsePreviewValueAsDefault != 0) });
			if (Input->bUsePreviewValueAsDefault)
			{
				const double Preview[4] = { Input->PreviewValue.X, Input->PreviewValue.Y, Input->PreviewValue.Z, Input->PreviewValue.W };
				Node.Properties.Add({ FString(Prop::PreviewValue), FIRPropertyValue::MakeFloat4(Preview, 4) });
			}
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 7)
			// Which of a blend's two materials the input is. The language derives it (IRBuilderMaterial.cpp); it is read so
			// that a rebuild which derives another one shows as a difference.
			if (Input->BlendInputRelevance != EBlendInputRelevance::General)
			{
				Node.Properties.Add({
					FString(Prop::BlendInputRelevance),
					FIRPropertyValue::MakeEnum(Input->BlendInputRelevance == EBlendInputRelevance::Top ? TEXT("Top") : TEXT("Bottom")) });
			}
#endif

			// A StaticBool pin is `bool` plus the function's `/// @static`; its default sits on a StaticBool node wired to
			// the Preview pin (the engine reads no PreviewValue for one), which is read back here.
			bool bPreviewIsStaticDefault = false;
			if (Input->InputType == FunctionInput_StaticBool)
			{
				if (const UMaterialExpressionStaticBool* DefaultNode = Cast<UMaterialExpressionStaticBool>(Input->Preview.Expression))
				{
					bPreviewIsStaticDefault = true;
					const double Preview[4] = { DefaultNode->Value ? 1.0 : 0.0, 0.0, 0.0, 0.0 };
					Node.Properties.RemoveAll([](const UE::DreamShader::IR::FIRProperty& Entry) { return Entry.Name.Equals(UE::DreamShader::IR::Prop::PreviewValue, ESearchCase::CaseSensitive); });
					Node.Properties.Add({ FString(Prop::PreviewValue), FIRPropertyValue::MakeFloat4(Preview, 4) });
				}
			}
			else if (Input->InputType == FunctionInput_Bool)
			{
				Warning(TEXT("DSH9066"), FText::Format(
					LOCTEXT("BoolFunctionInput", "The input '{0}' of '{1}' is a dynamic bool pin, which the language has no parameter for; it is written as 'bool' and rebuilds as a scalar pin."),
					FText::FromName(Input->InputName),
					FText::FromString(Product.Name)));
			}
			if (Input->Preview.Expression && !bPreviewIsStaticDefault)
			{
				Warning(TEXT("DSH9071"), FText::Format(
					LOCTEXT("PreviewPinConnected", "The input '{0}' of '{1}' has its Preview pin wired. Source says that as a default that is an expression ('float {0} = UE.TexCoord(Index = 1).r'), which the decompiler does not write yet; the rebuilt input previews its number."),
					FText::FromName(Input->InputName),
					FText::FromString(Product.Name)));
			}

			const int32 NodeIndex = AddNode(MoveTemp(Node), Expression);
			NodeOfExpression.Add(Expression, NodeIndex);
			return true;
		}

		bool FGraphImporter::ImportFunctionCall(UMaterialExpression* Expression)
		{
			namespace Prop = UE::DreamShader::IR::Prop;
			UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);
			if (!Call)
			{
				return false;
			}

			if (!Call->MaterialFunction)
			{
				Warning(TEXT("DSH9064"), FText::Format(
					LOCTEXT("CallWithoutFunction", "{0} calls a material function that is missing; a zero stands in for every value read from it."),
					DescribeExpression(Expression)));
				for (int32 OutputIndex = 0; OutputIndex < FMath::Max(Call->FunctionOutputs.Num(), 1); ++OutputIndex)
				{
					ForwardedOutputs.Add(TPair<const UMaterialExpression*, int32>(Expression, OutputIndex), MakeZero(1));
				}
				return true;
			}

			const FString FunctionPath = NormalizeImportAssetPath(Call->MaterialFunction->GetPathName());
			const UE::DreamShader::Lang::FIRToAstExternInterface* Interface =
				ImportDreamShaderFunctionInterface(Call->MaterialFunction, Options, Context, Diagnostics);

			FIRNode Node;
			Node.Op = EIROp::FunctionCall;
			Node.ClassName = FunctionPath;
			Node.Properties.Add({ FString(Prop::FunctionPath), FIRPropertyValue::MakeObject(FunctionPath) });

			for (const FFunctionExpressionInput& Input : Call->FunctionInputs)
			{
				const FName PinName = Input.ExpressionInput ? Input.ExpressionInput->InputName : Input.Input.InputName;
				const FIRValue Value = ResolveInput(Input.Input);
				if (Value.IsValid() && !PinName.IsNone())
				{
					Node.Inputs.Add({ PinName.ToString(), Value });
				}
			}

			for (int32 OutputIndex = 0; OutputIndex < Call->FunctionOutputs.Num(); ++OutputIndex)
			{
				const FFunctionExpressionOutput& Output = Call->FunctionOutputs[OutputIndex];
				const FName PinName = Output.ExpressionOutput ? Output.ExpressionOutput->OutputName : Output.Output.OutputName;
				const FString Name = PinName.IsNone() ? FString() : PinName.ToString();

				// Widths come from the callee's own graph; the engine only knows them once it compiles the call.
				FIRType Type = FIRType::Float(1);
				if (Interface)
				{
					for (const UE::DreamShader::Lang::FIRToAstExternPin& Pin : Interface->Outputs)
					{
						if (Pin.Name.Equals(Name, ESearchCase::CaseSensitive))
						{
							Type = Pin.Type;
							break;
						}
					}
				}
				Node.Outputs.Add(Type);
				Node.OutputNames.Add(Name);
			}
			if (Node.Outputs.IsEmpty())
			{
				Node.Outputs.Add(FIRType::Float(1));
				Node.OutputNames.Add(TEXT("Result"));
			}

			NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));
			return true;
		}

		bool FGraphImporter::ImportCustom(UMaterialExpression* Expression)
		{
			namespace Prop = UE::DreamShader::IR::Prop;
			const UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression);
			if (!Custom)
			{
				return false;
			}

			FIRNode Node;
			Node.Op = EIROp::Custom;
			Node.ClassName = Custom->Description;
			Node.Properties.Add({ FString(Prop::Code), FIRPropertyValue::MakeString(Custom->Code) });
			Node.Properties.Add({ FString(Prop::Description), FIRPropertyValue::MakeString(Custom->Description) });
			if (Custom->IncludeFilePaths.Num() > 0)
			{
				Node.Properties.Add({ FString(Prop::IncludeFilePaths), FIRPropertyValue::MakeStringList(Custom->IncludeFilePaths) });
			}
			Node.Properties.Add({ FString(Prop::OutputType), FIRPropertyValue::MakeEnum(
				MakeImportEnumSpelling(StaticEnum<ECustomMaterialOutputType>(), static_cast<int64>(Custom->OutputType.GetValue()))) });

			for (const FCustomInput& Input : Custom->Inputs)
			{
				const FIRValue Value = ResolveInput(Input.Input);
				if (Input.InputName.IsNone())
				{
					continue;
				}
				// An unwired pin is still one of the function's parameters: a hole, which the validator allows here.
				Node.Inputs.Add({ Input.InputName.ToString(), Value });
			}

			Node.Outputs.Add(TypeFromCustomOutputType(Custom->OutputType.GetValue()));
			Node.OutputNames.Add(TEXT("Result"));
			TArray<FString> Additional;
			for (const FCustomOutput& Output : Custom->AdditionalOutputs)
			{
				if (Output.OutputName.IsNone())
				{
					continue;
				}
				Node.Outputs.Add(TypeFromCustomOutputType(Output.OutputType.GetValue()));
				Node.OutputNames.Add(Output.OutputName.ToString());
				Additional.Add(FString::Printf(TEXT("%s:%s"), *Output.OutputName.ToString(),
					*MakeImportEnumSpelling(StaticEnum<ECustomMaterialOutputType>(), static_cast<int64>(Output.OutputType.GetValue()))));
			}
			if (Additional.Num() > 0)
			{
				Node.Properties.Add({ FString(Prop::AdditionalOutputs), FIRPropertyValue::MakeStringList(Additional) });
			}

			if (Custom->AdditionalDefines.Num() > 0)
			{
				Warning(TEXT("DSH9067"), FText::Format(
					LOCTEXT("CustomDefinesDropped", "{0} carries {1} additional define(s), which a '/// @custom' function cannot declare; the rebuilt node has none. Move them into the body as '#define' lines."),
					DescribeExpression(Expression),
					FText::AsNumber(Custom->AdditionalDefines.Num())));
			}

			NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));
			return true;
		}

		bool FGraphImporter::ImportMaterialAttributes(UMaterialExpression* Expression)
		{
			namespace Prop = UE::DreamShader::IR::Prop;

			if (Expression->IsA<UMaterialExpressionMakeMaterialAttributes>())
			{
				FIRNode Node;
				Node.Op = EIROp::MakeMaterialAttributes;
				Node.Outputs.Add(FIRType::Material());
				for (TFieldIterator<FProperty> It(Expression->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
				{
					const FProperty* Property = *It;
					if (!IsMaterialExpressionInputProperty(Property))
					{
						continue;
					}
					for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ++ArrayIndex)
					{
						const FExpressionInput* Input = Property->ContainerPtrToValuePtr<FExpressionInput>(Expression, ArrayIndex);
						const FIRValue Value = Input ? ResolveInput(*Input) : FIRValue::None();
						if (!Value.IsValid())
						{
							continue;
						}
						// The pin is named after the attribute, except the eight UVs, which are one array (the inverse of
						// CollectMakeAttributePinCandidates).
						const FString Attribute = Property->ArrayDim > 1
							? FString::Printf(TEXT("CustomizedUV%d"), ArrayIndex) /* I18N-EXEMPT: attribute identifier */
							: Property->GetName();
						if (Catalog.FindMaterialAttribute(Attribute) == INDEX_NONE)
						{
							Warning(TEXT("DSH9072"), FText::Format(
								LOCTEXT("MakeAttributeUnknown", "{0} sets '{1}', which is no material attribute the catalog knows; the connection is dropped."),
								DescribeExpression(Expression),
								FText::FromString(Attribute)));
							continue;
						}
						Node.Inputs.Add({ Attribute, Value });
					}
				}
				NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));
				return true;
			}

			if (const UMaterialExpressionSetMaterialAttributes* Set = Cast<UMaterialExpressionSetMaterialAttributes>(Expression))
			{
				const FIRValue Base = Set->Inputs.IsValidIndex(0) ? ResolveInput(Set->Inputs[0]) : FIRValue::None();

				FIRNode Node;
				Node.Outputs.Add(FIRType::Material());
				TArray<FString> AttributeNames;
				for (int32 Index = 0; Index < Set->AttributeSetTypes.Num(); ++Index)
				{
					const FIRValue Value = Set->Inputs.IsValidIndex(Index + 1) ? ResolveInput(Set->Inputs[Index + 1]) : FIRValue::None();
					const FString Attribute = FMaterialAttributeDefinitionMap::GetAttributeName(Set->AttributeSetTypes[Index]);
					if (!Value.IsValid())
					{
						continue;
					}
					if (Catalog.FindMaterialAttribute(Attribute) == INDEX_NONE)
					{
						// As on a make: a custom attribute has no EMaterialProperty, so no `m.<Attribute>` says it.
						Warning(TEXT("DSH9072"), FText::Format(
							LOCTEXT("SetAttributeUnknown", "{0} sets '{1}', which is no material attribute the catalog knows; the connection is dropped."),
							DescribeExpression(Expression),
							FText::FromString(Attribute)));
						continue;
					}
					Node.Inputs.Add({ Attribute, Value });
					AttributeNames.Add(Attribute);
				}

				if (Base.IsValid())
				{
					Node.Op = EIROp::SetMaterialAttributes;
					FIRInput BaseInput;
					BaseInput.Pin = MaterialAttributesPin;
					BaseInput.Value = Base;
					Node.Inputs.Insert(MoveTemp(BaseInput), 0);
					Node.Properties.Add({ FString(Prop::AttributeSetTypes), FIRPropertyValue::MakeStringList(AttributeNames) });
				}
				else
				{
					// A set on top of nothing is what a make is.
					Node.Op = EIROp::MakeMaterialAttributes;
				}
				NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));
				return true;
			}

			const UMaterialExpressionBreakMaterialAttributes* Break = Cast<UMaterialExpressionBreakMaterialAttributes>(Expression);
			const UMaterialExpressionGetMaterialAttributes* Get = Cast<UMaterialExpressionGetMaterialAttributes>(Expression);
			if (!Break && !Get)
			{
				return false;
			}

			const FIRValue Base = ResolveInput(Break ? static_cast<const FExpressionInput&>(Break->MaterialAttributes) : static_cast<const FExpressionInput&>(Get->MaterialAttributes));
			if (!Base.IsValid())
			{
				Warning(TEXT("DSH9063"), FText::Format(
					LOCTEXT("BreakWithoutMaterial", "{0} reads the attributes of no material; a zero stands in for every value read from it."),
					DescribeExpression(Expression)));
				for (int32 OutputIndex = 0; OutputIndex < Expression->Outputs.Num(); ++OutputIndex)
				{
					ForwardedOutputs.Add(TPair<const UMaterialExpression*, int32>(Expression, OutputIndex), MakeZero(1));
				}
				return true;
			}

			// One slot per catalog attribute, as the builder makes it (ReadMaterialField); the engine's outputs map onto them.
			FIRNode Node;
			Node.Op = EIROp::GetMaterialAttributes;
			Node.Inputs.Add({ FString(MaterialAttributesPin), Base });
			for (const UE::DreamShader::IR::FCatalogMaterialAttribute& Attribute : Catalog.MaterialAttributes)
			{
				Node.Outputs.Add(Attribute.ValueType);
				Node.OutputNames.Add(Attribute.Name);
			}

			TArray<int32> Slots;
			for (int32 OutputIndex = 0; OutputIndex < Expression->Outputs.Num(); ++OutputIndex)
			{
				FString Attribute;
				if (Get)
				{
					// Output 0 hands the material through; the others follow AttributeGetTypes.
					if (OutputIndex == 0)
					{
						ForwardedOutputs.Add(TPair<const UMaterialExpression*, int32>(Expression, 0), Base);
						Slots.Add(INDEX_NONE);
						continue;
					}
					if (Get->AttributeGetTypes.IsValidIndex(OutputIndex - 1))
					{
						Attribute = FMaterialAttributeDefinitionMap::GetAttributeName(Get->AttributeGetTypes[OutputIndex - 1]);
					}
				}
				else
				{
					Attribute = Expression->Outputs[OutputIndex].OutputName.ToString();
				}
				Slots.Add(Catalog.FindMaterialAttribute(Attribute));
			}

			const int32 NodeIndex = AddNode(MoveTemp(Node), Expression);
			NodeOfExpression.Add(Expression, NodeIndex);
			OutputSlotsOfNode.Add(NodeIndex, MoveTemp(Slots));
			if (Get)
			{
				Info(TEXT("DSH9073"), FText::Format(
					LOCTEXT("GetAttributesAsBreak", "{0} is a GetMaterialAttributes node; reading a material's attributes always rebuilds as a BreakMaterialAttributes."),
					DescribeExpression(Expression)));
			}
			return true;
		}

		bool FGraphImporter::ImportStructural(UMaterialExpression* Expression)
		{
			namespace Prop = UE::DreamShader::IR::Prop;

			if (const UMaterialExpressionComponentMask* Mask = Cast<UMaterialExpressionComponentMask>(Expression))
			{
				const FIRValue Operand = ResolveInput(Mask->Input);
				const int32 Width = WidthOf(Operand);
				if (!Operand.IsValid() || Width <= 0)
				{
					return false;
				}
				const bool bChannels[4] = { Mask->R != 0, Mask->G != 0, Mask->B != 0, Mask->A != 0 };
				FString Letters;
				bool bPastOperand = false;
				for (int32 Channel = 0; Channel < 4; ++Channel)
				{
					if (bChannels[Channel])
					{
						bPastOperand = bPastOperand || Channel >= Width;
						Letters.AppendChar(SwizzleComponents[Channel]);
					}
				}
				// A mask wider than what it masks does not compile in the engine either; it stays the node it is.
				if (Letters.IsEmpty() || bPastOperand)
				{
					return false;
				}

				FIRNode Node;
				Node.Op = EIROp::Swizzle;
				Node.Operands.Add(Operand);
				Node.Properties.Add({ FString(Prop::Mask), FIRPropertyValue::MakeString(Letters) });
				Node.Outputs.Add(FIRType::Float(Letters.Len()));
				NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));
				return true;
			}

			if (const UMaterialExpressionAppendVector* Append = Cast<UMaterialExpressionAppendVector>(Expression))
			{
				const FIRValue A = ResolveInput(Append->A);
				const FIRValue B = ResolveInput(Append->B);
				const int32 Width = WidthOf(A) + WidthOf(B);
				if (!A.IsValid() || !B.IsValid() || WidthOf(A) <= 0 || WidthOf(B) <= 0 || Width > 4)
				{
					return false;
				}
				FIRNode Node;
				Node.Op = EIROp::Append;
				Node.Operands = { A, B };
				Node.Outputs.Add(FIRType::Float(Width));
				NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));
				return true;
			}

			if (Expression->GetClass() == UMaterialExpressionStaticSwitch::StaticClass())
			{
				const UMaterialExpressionStaticSwitch* Switch = CastChecked<UMaterialExpressionStaticSwitch>(Expression);
				const FIRValue IfTrue = ResolveInput(Switch->A);
				const FIRValue IfFalse = ResolveInput(Switch->B);
				const FIRValue Condition = ResolveInput(Switch->Value);
				if (!Condition.IsValid())
				{
					// Nothing decides it but the default: the branch it picks IS the value.
					const FIRValue Picked = Switch->DefaultValue ? IfTrue : IfFalse;
					if (!Picked.IsValid())
					{
						return false;
					}
					ForwardedOutputs.Add(TPair<const UMaterialExpression*, int32>(Expression, 0), Picked);
					Info(TEXT("DSH9069"), FText::Format(
						LOCTEXT("StaticSwitchFolded", "{0} has nothing wired to Value, so it is its {1} branch and nothing else; the rebuilt graph has no switch there."),
						DescribeExpression(Expression),
						Switch->DefaultValue ? LOCTEXT("StaticSwitchTrueBranch", "True") : LOCTEXT("StaticSwitchFalseBranch", "False")));
					return true;
				}
				if (!IfTrue.IsValid() || !IfFalse.IsValid())
				{
					return false;
				}

				FIRNode Node;
				Node.Op = EIROp::StaticSwitch;
				Node.Operands = { Condition, IfTrue, IfFalse };
				const FIRType TrueType = TypeOf(IfTrue);
				Node.Outputs.Add(TrueType.GraphComponentCount() > 0 ? FIRType::Float(FMath::Max(WidthOf(IfTrue), WidthOf(IfFalse))) : TrueType);
				NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));
				return true;
			}

			if (Expression->GetClass() == UMaterialExpressionIf::StaticClass())
			{
				const UMaterialExpressionIf* If = CastChecked<UMaterialExpressionIf>(Expression);
				const FIRValue A = ResolveInput(If->A);
				const FIRValue B = ResolveInput(If->B);
				const FIRValue Greater = ResolveInput(If->AGreaterThanB);
				const FIRValue Equal = ResolveInput(If->AEqualsB);
				const FIRValue Less = ResolveInput(If->ALessThanB);
				// The engine reads a missing AEqualsB its own way, and ConstB and the threshold are the node's; all of
				// those are what `UE.If(...)` is for.
				if (!A.IsValid() || !B.IsValid() || !Greater.IsValid() || !Equal.IsValid() || !Less.IsValid()
					|| !AreOtherPropertiesDefault(Expression, { FName(TEXT("ConstB")) }))
				{
					return false;
				}

				FIRNode Node;
				Node.Op = EIROp::Compare;
				Node.Operands = { A, B, Greater, Equal, Less };
				Node.Outputs.Add(FIRType::Float(FMath::Clamp(FMath::Max3(WidthOf(Greater), WidthOf(Equal), WidthOf(Less)), 1, 4)));
				NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));
				return true;
			}

			if (Expression->GetClass() == UMaterialExpressionTextureSample::StaticClass())
			{
				UMaterialExpressionTextureSample* Sample = CastChecked<UMaterialExpressionTextureSample>(Expression);
				return Sample->TextureObject.Expression && ImportTextureSample(Sample, ResolveInput(Sample->TextureObject), Expression);
			}

			return false;
		}

		bool FGraphImporter::ImportConvert(UMaterialExpression* Expression)
		{
#if DREAMSHADER_GRAPH_IMPORT_WITH_CONVERT
			// Its inputs, its outputs and the mappings between their channels are arrays of structs, so no call can say the
			// node and the reflected one reads back without a single wire. What it computes can be said: per output, what
			// UMaterialExpressionConvert::Compile builds (and Build, for the new translator) -- each channel masked out of an
			// input, or a default, appended together.
			namespace Prop = UE::DreamShader::IR::Prop;
			UMaterialExpressionConvert* Convert = Cast<UMaterialExpressionConvert>(Expression);
			if (!Convert)
			{
				return false;
			}

			// MaterialExpressionConvertType::GetComponentCount, without its checkNoEntry: a type this engine does not know
			// leaves the node to the reflected call and what that one warns.
			const auto WidthOfType = [](const EMaterialExpressionConvertType Type) -> int32
			{
				switch (Type)
				{
				case EMaterialExpressionConvertType::Scalar: return 1;
				case EMaterialExpressionConvertType::Vector2: return 2;
				case EMaterialExpressionConvertType::Vector3: return 3;
				case EMaterialExpressionConvertType::Vector4: return 4;
				default: break;
				}
				return 0;
			};
			for (const FMaterialExpressionConvertInput& Input : Convert->ConvertInputs)
			{
				if (WidthOfType(Input.Type) == 0)
				{
					return false;
				}
			}
			for (const FMaterialExpressionConvertOutput& Output : Convert->ConvertOutputs)
			{
				if (WidthOfType(Output.Type) == 0)
				{
					return false;
				}
			}

			// A channel of an output: a channel of the value wired to an input, or a number -- an unwired input's default,
			// the output's own default, or the zero that stands in for what the engine refuses.
			struct FChannel
			{
				FIRValue Source = FIRValue::None();
				int32 Index = 0;
				double Literal = 0.0;
			};

			// An input is read once, and only when a mapping reads it: the engine compiles it the same way.
			TArray<TOptional<FIRValue>> InputValues;
			InputValues.SetNum(Convert->ConvertInputs.Num());
			const auto InputValue = [this, Convert, &InputValues](const int32 InputIndex) -> FIRValue
			{
				if (!InputValues[InputIndex].IsSet())
				{
					InputValues[InputIndex] = ResolveInput(Convert->ConvertInputs[InputIndex].ExpressionInput);
				}
				return InputValues[InputIndex].GetValue();
			};

			const auto MakeLiteral = [this, Expression](const double* Components, const int32 Width) -> FIRValue
			{
				FIRNode Node;
				Node.Op = EIROp::Constant;
				Node.Properties.Add({ FString(Prop::Value), FIRPropertyValue::MakeFloat4(Components, Width) });
				Node.Outputs.Add(FIRType::Float(Width));
				return AddPartNode(MoveTemp(Node), Expression);
			};

			static const TCHAR* const ChannelLetters = TEXT("RGBA");
			for (int32 OutputIndex = 0; OutputIndex < Convert->ConvertOutputs.Num(); ++OutputIndex)
			{
				const FMaterialExpressionConvertOutput& Output = Convert->ConvertOutputs[OutputIndex];
				const int32 Width = WidthOfType(Output.Type);

				FChannel Channels[4];
				bool bMapped[4] = { false, false, false, false };
				// In the node's order: of two mappings onto one channel, the engine keeps the later.
				for (const FMaterialExpressionConvertMapping& Mapping : Convert->ConvertMappings)
				{
					if (Mapping.OutputIndex != OutputIndex)
					{
						continue;
					}
					if (Mapping.OutputComponentIndex < 0 || Mapping.OutputComponentIndex >= Width
						|| !Convert->ConvertInputs.IsValidIndex(Mapping.InputIndex)
						|| Mapping.InputComponentIndex < 0
						|| Mapping.InputComponentIndex >= WidthOfType(Convert->ConvertInputs[Mapping.InputIndex].Type))
					{
						Warning(TEXT("DSH9063"), FText::Format(
							LOCTEXT("ConvertMappingUnknown", "{0} maps channel {1} of its input {2} onto channel {3} of its output {4}, and the node has no such pin or channel; the engine refuses the mapping, and it is left out."),
							DescribeExpression(Expression),
							FText::AsNumber(Mapping.InputComponentIndex),
							FText::AsNumber(Mapping.InputIndex),
							FText::AsNumber(Mapping.OutputComponentIndex),
							FText::AsNumber(OutputIndex)));
						continue;
					}

					FChannel& Channel = Channels[Mapping.OutputComponentIndex];
					Channel = FChannel();
					bMapped[Mapping.OutputComponentIndex] = true;

					const FIRValue Value = InputValue(Mapping.InputIndex);
					if (!Value.IsValid())
					{
						// Nothing wired: the input is its default, a float4 constant.
						Channel.Literal = Convert->ConvertInputs[Mapping.InputIndex].DefaultValue.Component(Mapping.InputComponentIndex);
						continue;
					}

					const int32 ValueWidth = WidthOf(Value);
					if (ValueWidth == 1)
					{
						// A scalar is each of its channels: the new translator splats it to the input's type, the classic one
						// reads `.r` of what it only knows as "a float".
						Channel.Source = Value;
					}
					else if (Mapping.InputComponentIndex < ValueWidth)
					{
						Channel.Source = Value;
						Channel.Index = Mapping.InputComponentIndex;
					}
					else
					{
						Warning(TEXT("DSH9063"), FText::Format(
							LOCTEXT("ConvertChannelPastValue", "{0} reads channel {1} of its input {2}, which carries a {3}; the new translator reads zero there and the classic one refuses the node, so a zero stands in."),
							DescribeExpression(Expression),
							FText::FromString(FString::Chr(ChannelLetters[Mapping.InputComponentIndex])),
							FText::AsNumber(Mapping.InputIndex),
							FText::FromString(TypeOf(Value).ToString())));
					}
				}
				for (int32 Index = 0; Index < Width; ++Index)
				{
					if (!bMapped[Index])
					{
						Channels[Index].Literal = Output.DefaultValue.Component(Index);
					}
				}

				const TPair<const UMaterialExpression*, int32> Key(Expression, OutputIndex);

				// Numbers throughout: one constant, which is what `float2(0.5, 1.0)` folds to (FIRBuilder::MakeAppend).
				bool bAllLiteral = true;
				double Literals[4] = { 0.0, 0.0, 0.0, 0.0 };
				for (int32 Index = 0; Index < Width; ++Index)
				{
					bAllLiteral = bAllLiteral && !Channels[Index].Source.IsValid();
					Literals[Index] = Channels[Index].Literal;
				}
				if (bAllLiteral)
				{
					ForwardedOutputs.Add(Key, MakeLiteral(Literals, Width));
					continue;
				}

				// Otherwise the parts of a constructor, chained as the builder chains `float3(v.xy, 0.0)`: a run of ascending
				// channels of one value is one channel selection -- the value itself when the run is all of it -- and every
				// number is a part of its own.
				TArray<FIRValue> Parts;
				for (int32 Index = 0; Index < Width;)
				{
					const FChannel& Head = Channels[Index];
					if (!Head.Source.IsValid())
					{
						const double Literal[4] = { Head.Literal, 0.0, 0.0, 0.0 };
						Parts.Add(MakeLiteral(Literal, 1));
						++Index;
						continue;
					}

					FString Mask;
					Mask.AppendChar(SwizzleComponents[Head.Index]);
					int32 Next = Index + 1;
					while (Next < Width && Channels[Next].Source == Head.Source && Channels[Next].Index > Channels[Next - 1].Index)
					{
						Mask.AppendChar(SwizzleComponents[Channels[Next].Index]);
						++Next;
					}

					bool bWhole = Mask.Len() == WidthOf(Head.Source);
					for (int32 Letter = 0; bWhole && Letter < Mask.Len(); ++Letter)
					{
						bWhole = Mask[Letter] == SwizzleComponents[Letter];
					}
					if (bWhole)
					{
						Parts.Add(Head.Source);
					}
					else
					{
						FIRNode Node;
						Node.Op = EIROp::Swizzle;
						Node.Operands.Add(Head.Source);
						Node.Properties.Add({ FString(Prop::Mask), FIRPropertyValue::MakeString(Mask) });
						Node.Outputs.Add(FIRType::Float(Mask.Len()));
						Parts.Add(AddPartNode(MoveTemp(Node), Expression));
					}
					Index = Next;
				}

				FIRValue Result = Parts[0];
				for (int32 PartIndex = 1; PartIndex < Parts.Num(); ++PartIndex)
				{
					FIRNode Node;
					Node.Op = EIROp::Append;
					Node.Operands = { Result, Parts[PartIndex] };
					Node.Outputs.Add(FIRType::Float(FMath::Min(WidthOf(Result) + WidthOf(Parts[PartIndex]), 4)));
					Result = AddPartNode(MoveTemp(Node), Expression);
				}
				ForwardedOutputs.Add(Key, Result);
			}

			Info(TEXT("DSH9073"), FText::Format(
				LOCTEXT("ConvertAsChannels", "{0} is a Convert node, whose pins no call can name; each of its outputs is written as the channels it is made of ('v.xy', 'float3(a, b, 0.0)'), and the rebuilt graph has ComponentMask and AppendVector nodes in its place."),
				DescribeExpression(Expression)));
			return true;
#else
			(void)Expression;
			return false;
#endif
		}

		bool FGraphImporter::ImportSwitch(UMaterialExpression* Expression)
		{
#if DREAMSHADER_GRAPH_IMPORT_WITH_SWITCH
			// Its cases are an array of structs, so the reflected call reads it back with none, and a Switch without cases
			// compiles to nothing. What it computes can be said: the chain the new translator builds for it
			// (UMaterialExpressionSwitch::Build), `floor(s) == i ? case i : ...` from the first case on and the default last,
			// which is the case the classic translator's sum of steps picks for every finite selector.
			namespace Prop = UE::DreamShader::IR::Prop;
			UMaterialExpressionSwitch* Switch = Cast<UMaterialExpressionSwitch>(Expression);
			if (!Switch || Expression->GetClass() != UMaterialExpressionSwitch::StaticClass())
			{
				return false;
			}

			// A case with nothing wired is refused by both translators: the node stays the reflected call, which says what
			// it drops.
			TArray<FIRValue> Cases;
			for (const FSwitchCustomInput& Case : Switch->Inputs)
			{
				const FIRValue Value = ResolveInput(Case.Input);
				if (!Value.IsValid() || WidthOf(Value) <= 0)
				{
					return false;
				}
				Cases.Add(Value);
			}

			const auto MakeNumber = [this, Expression](const double Number) -> FIRValue
			{
				const double Components[4] = { Number, 0.0, 0.0, 0.0 };
				FIRNode Node;
				Node.Op = EIROp::Constant;
				Node.Properties.Add({ FString(Prop::Value), FIRPropertyValue::MakeFloat4(Components, 1) });
				Node.Outputs.Add(FIRType::Float(1));
				return AddPartNode(MoveTemp(Node), Expression);
			};

			FIRValue Default = ResolveInput(Switch->Default);
			if (!Default.IsValid())
			{
				Default = MakeNumber(Switch->ConstDefault);
			}

			// The type the engine casts every branch to: a scalar goes with any width, two other widths with nothing.
			int32 Width = WidthOf(Default);
			if (Width <= 0)
			{
				return false;
			}
			for (const FIRValue& Case : Cases)
			{
				const int32 CaseWidth = WidthOf(Case);
				if (CaseWidth != 1 && Width != 1 && CaseWidth != Width)
				{
					return false;
				}
				Width = FMath::Max(Width, CaseWidth);
			}
			const auto ToCommonType = [this, Expression, Width](const FIRValue Value) -> FIRValue
			{
				// A scalar picked where the others are vectors is splatted, as `float3(x)` is.
				if (Width <= 1 || WidthOf(Value) != 1)
				{
					return Value;
				}
				FIRNode Node;
				Node.Op = EIROp::Broadcast;
				Node.Operands.Add(Value);
				Node.Outputs.Add(FIRType::Float(Width));
				return AddPartNode(MoveTemp(Node), Expression);
			};

			const TPair<const UMaterialExpression*, int32> Key(Expression, 0);
			const FIRValue Selector = ResolveInput(Switch->SwitchValue);
			if (Cases.IsEmpty())
			{
				// The new translator hands the default on; the classic one has nothing to choose from.
				ForwardedOutputs.Add(Key, Default);
				Info(TEXT("DSH9069"), FText::Format(
					LOCTEXT("SwitchWithoutCases", "{0} has no input besides its default, which is what it hands on; the rebuilt graph has no switch there."),
					DescribeExpression(Expression)));
				return true;
			}
			if (!Selector.IsValid())
			{
				// Nothing wired to SwitchValue: its number picks one input for good, in both translators.
				const int32 Picked = FMath::FloorToInt(Switch->ConstSwitchValue);
				const bool bCase = Cases.IsValidIndex(Picked);
				ForwardedOutputs.Add(Key, ToCommonType(bCase ? Cases[Picked] : Default));
				Info(TEXT("DSH9069"), FText::Format(
					LOCTEXT("SwitchFolded", "{0} has nothing wired to SwitchValue, so its value {1} picks {2} and nothing else; the rebuilt graph has no switch there."),
					DescribeExpression(Expression),
					FText::AsNumber(Switch->ConstSwitchValue),
					bCase
						? FText::Format(LOCTEXT("SwitchFoldedCase", "the input '{0}'"), FText::FromName(Switch->Inputs[Picked].InputName))
						: LOCTEXT("SwitchFoldedDefault", "the default")));
				return true;
			}

			// The first channel of a vector selector (Build), floored, so that each case is one exact comparison.
			FIRValue Index = Selector;
			if (WidthOf(Index) > 1)
			{
				FIRNode Node;
				Node.Op = EIROp::Swizzle;
				Node.Operands.Add(Index);
				Node.Properties.Add({ FString(Prop::Mask), FIRPropertyValue::MakeString(TEXT("x")) });
				Node.Outputs.Add(FIRType::Float(1));
				Index = AddPartNode(MoveTemp(Node), Expression);
			}
			FIRNode FloorNode;
			FloorNode.Op = EIROp::Floor;
			FloorNode.Operands.Add(Index);
			FloorNode.Outputs.Add(FIRType::Float(1));
			const FIRValue Floored = AddPartNode(MoveTemp(FloorNode), Expression);

			// From the last case back, as Build chains them: each case's "else" is everything after it. The number is the
			// If's A, so that no case is the `c ? a : b` shape RaiseDreamShaderIR reads an If against a Constant 0 B as; and
			// a case that is what follows it anyway is no branch (FIRBuilder::MakeConditional).
			FIRValue Result = Default;
			for (int32 CaseIndex = Cases.Num() - 1; CaseIndex >= 0; --CaseIndex)
			{
				if (Cases[CaseIndex] == Result)
				{
					continue;
				}
				const FIRValue Number = MakeNumber(CaseIndex);
				FIRNode Node;
				Node.Op = EIROp::Compare;
				Node.Operands = { Number, Floored, Result, Cases[CaseIndex], Result };
				Node.Outputs.Add(FIRType::Float(FMath::Max(WidthOf(Result), WidthOf(Cases[CaseIndex]))));
				Result = AddPartNode(MoveTemp(Node), Expression);
			}
			ForwardedOutputs.Add(Key, ToCommonType(Result));

			Info(TEXT("DSH9073"), FText::Format(
				LOCTEXT("SwitchAsBranches", "{0} is a Switch node, whose inputs no call can name; it is written as the branches the engine makes of it ('0.0 == floor(s) ? a : ...'), and the rebuilt graph has Floor and If nodes in its place."),
				DescribeExpression(Expression)));
			return true;
#else
			(void)Expression;
			return false;
#endif
		}

		bool FGraphImporter::ImportCoreMath(UMaterialExpression* Expression)
		{
			const EIROp* Op = CoreOpByClass.Find(GetMaterialExpressionShortName(Expression->GetClass()));
			if (!Op)
			{
				return false;
			}
			const UE::DreamShader::IR::FIRCoreOpInfo& Info = UE::DreamShader::IR::GetCoreOpInfo(*Op);

			// The core op is the node with every operand wired and nothing else touched. Anything less is the class with
			// its own arguments, which only the reflected call can say.
			TArray<FIRValue> Operands;
			TSet<FName> OperandTwins;
			for (int32 PinIndex = 0; PinIndex < static_cast<int32>(UE_ARRAY_COUNT(Info.InputPins)) && Info.InputPins[PinIndex]; ++PinIndex)
			{
				FProperty* Property = FindMaterialExpressionArgumentProperty(Expression->GetClass(), Info.InputPins[PinIndex]);
				if (!Property || !IsMaterialExpressionInputProperty(Property))
				{
					return false;
				}
				const FExpressionInput* Input = Property->ContainerPtrToValuePtr<FExpressionInput>(Expression);
				const FIRValue Value = Input ? ResolveInput(*Input) : FIRValue::None();
				if (!Value.IsValid() || WidthOf(Value) <= 0)
				{
					return false;
				}
				Operands.Add(Value);
				// A twin under a wired pin is dead weight, whatever it holds.
				OperandTwins.Add(FName(*FString::Printf(TEXT("Const%s"), Info.InputPins[PinIndex])));
			}
			if (Operands.Num() < Info.MinArity || !AreOtherPropertiesDefault(Expression, OperandTwins))
			{
				return false;
			}

			int32 Widest = 1;
			for (const FIRValue& Operand : Operands)
			{
				Widest = FMath::Max(Widest, WidthOf(Operand));
			}

			FIRNode Node;
			Node.Op = *Op;
			Node.Operands = MoveTemp(Operands);
			switch (Info.Typing)
			{
			case UE::DreamShader::IR::EIRTypingRule::SameAsFirst:
				Node.Outputs.Add(FIRType::Float(FMath::Clamp(WidthOf(Node.Operands[0]), 1, 4)));
				break;
			case UE::DreamShader::IR::EIRTypingRule::Scalar:
				Node.Outputs.Add(FIRType::Float(1));
				break;
			case UE::DreamShader::IR::EIRTypingRule::Float3:
				Node.Outputs.Add(FIRType::Float(3));
				break;
			case UE::DreamShader::IR::EIRTypingRule::Bool:
				Node.Outputs.Add(FIRType::Bool(Widest));
				break;
			default:
				Node.Outputs.Add(FIRType::Float(Widest));
				break;
			}
			NodeOfExpression.Add(Expression, AddNode(MoveTemp(Node), Expression));
			return true;
		}

		FIRType FGraphImporter::ResolveOutputType(UMaterialExpression* Expression, const int32 OutputIndex, const UE::DreamShader::IR::ECatalogValueType CatalogType, const FIRNode& Node) const
		{
			using UE::DreamShader::IR::ECatalogValueType;

			// What the engine says about this very node comes first: it knows a texture's dimension and a width the
			// class alone does not give away.
			FIRType FromEngine = FIRType::Error();
			if (Expression->Outputs.IsValidIndex(OutputIndex))
			{
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
				FromEngine = TypeFromMaterialValueType(Expression->GetOutputValueType(OutputIndex));
#else
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				FromEngine = TypeFromMaterialValueType(static_cast<EMaterialValueType>(Expression->GetOutputType(OutputIndex)));
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
			}

			if (CatalogType != ECatalogValueType::Numeric && CatalogType != ECatalogValueType::Unknown)
			{
				const FIRType FromCatalog = UE::DreamShader::IR::TypeFromCatalogValueType(CatalogType);
				return (FromCatalog.IsTexture() && FromEngine.IsTexture()) ? FromEngine : FromCatalog;
			}
			if (!FromEngine.IsError())
			{
				return FromEngine;
			}

			// "Some float": the width follows the inputs, as the builder types the same call.
			int32 Widest = 1;
			for (const FIRInput& Input : Node.Inputs)
			{
				Widest = FMath::Max(Widest, WidthOf(Input.Value));
			}
			return FIRType::Float(FMath::Clamp(Widest, 1, 4));
		}

		void FGraphImporter::ImportReflected(UMaterialExpression* Expression)
		{
			const UClass* Class = Expression->GetClass();
			const int32 CatalogIndex = Catalog.FindExpressionByClass(Class->GetPathName());
			if (!Catalog.Expressions.IsValidIndex(CatalogIndex))
			{
				Warning(TEXT("DSH9065"), FText::Format(
					LOCTEXT("ClassNotInCatalog", "{0} is of a class the builtin catalog does not list (abstract, deprecated, or from a module loaded after the catalog was built); a zero stands in for every value read from it."),
					DescribeExpression(Expression)));
				for (int32 OutputIndex = 0; OutputIndex < FMath::Max(Expression->Outputs.Num(), 1); ++OutputIndex)
				{
					ForwardedOutputs.Add(TPair<const UMaterialExpression*, int32>(Expression, OutputIndex), MakeZero(1));
				}
				return;
			}
			const UE::DreamShader::IR::FCatalogExpression& Entry = Catalog.Expressions[CatalogIndex];

			FIRNode Node;
			Node.Op = EIROp::Reflected;
			Node.ClassName = Entry.ShortName;
			Node.CatalogIndex = CatalogIndex;

			// Pins in the order the catalog lists them, which is the order of the class's own input properties.
			TSet<FName> UnwiredTwins;
			int32 ReflectedPins = 0;
			for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				const FProperty* Property = *It;
				if (Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient | CPF_DuplicateTransient) || !IsMaterialExpressionInputProperty(Property))
				{
					continue;
				}
				for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ++ArrayIndex)
				{
					++ReflectedPins;
					const FString PinName = MakeImportPinName(Property, ArrayIndex);
					const int32 PinIndex = Entry.FindInput(PinName);
					const FExpressionInput* Input = Property->ContainerPtrToValuePtr<FExpressionInput>(Expression, ArrayIndex);
					const FIRValue Value = Input ? ResolveInput(*Input) : FIRValue::None();
					if (Value.IsValid() && PinIndex != INDEX_NONE)
					{
						Node.Inputs.Add({ PinName, Value });
					}
					else if (!Value.IsValid() && PinIndex != INDEX_NONE && !Entry.Inputs[PinIndex].ConstPropertyName.IsEmpty())
					{
						UnwiredTwins.Add(FName(*Entry.Inputs[PinIndex].ConstPropertyName));
					}
				}
			}

			// Pins the class keeps in an array of its own (Switch, a layer stack) have no name a call could use.
			int32 WiredDynamicPins = 0;
			for (int32 InputIndex = ReflectedPins; FExpressionInput* Input = Expression->GetInput(InputIndex); ++InputIndex)
			{
				WiredDynamicPins += Input->Expression ? 1 : 0;
			}
			if (WiredDynamicPins > 0)
			{
				Warning(TEXT("DSH9070"), FText::Format(
					LOCTEXT("DynamicPinsDropped", "{0} has {1} wired input(s) the class does not declare as named pins, so a call cannot connect them; the rebuilt node leaves them unconnected."),
					DescribeExpression(Expression),
					FText::AsNumber(WiredDynamicPins)));
			}

			const UObject* Defaults = Class->GetDefaultObject(false);
			for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				const FProperty* Property = *It;
				if (!IsImportableProperty(Property) || Entry.FindProperty(Property->GetName()) == INDEX_NONE)
				{
					continue;
				}

				// The twin of an unwired pin is what the pin reads: always said, default or not, because that is the shape
				// RaiseDreamShaderIR looks for. The twin of a wired pin says nothing.
				const bool bUnwiredTwin = UnwiredTwins.Contains(Property->GetFName());
				bool bWiredTwin = false;
				for (const UE::DreamShader::IR::FCatalogPin& Pin : Entry.Inputs)
				{
					bWiredTwin = bWiredTwin || (!bUnwiredTwin && Pin.ConstPropertyName.Equals(Property->GetName(), ESearchCase::CaseSensitive));
				}
				if (bWiredTwin || (!bUnwiredTwin && Defaults && Property->Identical_InContainer(Expression, Defaults)))
				{
					continue;
				}

				FIRPropertyValue Value;
				if (ReadProperty(Expression, Property, Value))
				{
					Node.Properties.Add({ Property->GetName(), MoveTemp(Value) });
				}
				else
				{
					Warning(TEXT("DSH9068"), FText::Format(
						LOCTEXT("PropertyKindSkipped", "{0} changes '{1}', a property of a kind a call argument cannot carry (an array, a map, a delegate); the rebuilt node has the default."),
						DescribeExpression(Expression),
						FText::FromString(Property->GetName())));
				}
			}

			const int32 NodeIndex = AddNode(MoveTemp(Node), Expression);
			NodeOfExpression.Add(Expression, NodeIndex);
			// A custom-output class is a statement until something reads it (ValueOfOutput).
			if (!Entry.bIsCustomOutput)
			{
				AddCatalogOutputs(NodeIndex, Expression);
			}
		}

		int32 FGraphImporter::AddCatalogOutputs(const int32 NodeIndex, UMaterialExpression* Expression)
		{
			FIRNode& Node = Graph.Nodes[NodeIndex];
			if (Node.Outputs.Num() > 0 || !Catalog.Expressions.IsValidIndex(Node.CatalogIndex))
			{
				return Node.Outputs.Num();
			}

			// The slot is the engine's output index: the catalog lists a class's outputs off the same array.
			const UE::DreamShader::IR::FCatalogExpression& Entry = Catalog.Expressions[Node.CatalogIndex];
			TArray<FIRType> Types;
			for (int32 OutputIndex = 0; OutputIndex < Entry.Outputs.Num(); ++OutputIndex)
			{
				Types.Add(ResolveOutputType(Expression, OutputIndex, Entry.Outputs[OutputIndex].Type, Node));
			}
			for (int32 OutputIndex = 0; OutputIndex < Entry.Outputs.Num(); ++OutputIndex)
			{
				Node.Outputs.Add(Types[OutputIndex]);
				Node.OutputNames.Add(Entry.Outputs[OutputIndex].Name);
			}
			return Node.Outputs.Num();
		}

		void FGraphImporter::BuildNode(UMaterialExpression* Expression)
		{
			if (NodeOfExpression.Contains(Expression))
			{
				return;
			}
			if (ImportConstant(Expression)
				|| ImportParameter(Expression)
				|| ImportStaticSwitchParameter(Expression)
				|| ImportTextureSampleParameter(Expression)
				|| ImportFunctionInput(Expression)
				|| ImportFunctionCall(Expression)
				|| ImportCustom(Expression)
				|| ImportMaterialAttributes(Expression)
				|| ImportStructural(Expression)
				|| ImportConvert(Expression)
				|| ImportSwitch(Expression)
				|| ImportCoreMath(Expression))
			{
				return;
			}
			ImportReflected(Expression);
		}

		// --------------------------------------------------------------------- what the graph cannot say

		void FGraphImporter::ReadHints(UObject* Asset)
		{
			UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
			if (!Package)
			{
				return;
			}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
			const FString Json = Package->GetMetaData().GetValue(Asset, TEXT("DreamShader.DecompileHints"));
#else
			// Not const: UMetaData::GetValue is not, before the metadata became a plain struct.
			UMetaData* MetaData = Package->GetMetaData();
			const FString Json = MetaData ? MetaData->GetValue(Asset, TEXT("DreamShader.DecompileHints")) : FString();
#endif
			if (Json.IsEmpty())
			{
				return;
			}

			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
			double Version = 0.0;
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || !Root->TryGetNumberField(TEXT("version"), Version) || static_cast<int32>(Version) != 1)
			{
				return;
			}
			Hints.bValid = true;

			const TSharedPtr<FJsonObject>* Names = nullptr;
			if (Root->TryGetObjectField(TEXT("names"), Names) && Names)
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Names)->Values)
				{
					FGuid Guid;
					if (FGuid::Parse(Pair.Key, Guid) && Pair.Value.IsValid())
					{
						Hints.Names.Add(Guid, Pair.Value->AsString());
					}
				}
			}

			const TSharedPtr<FJsonObject>* NodeRegions = nullptr;
			if (Root->TryGetObjectField(TEXT("nodeRegions"), NodeRegions) && NodeRegions)
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*NodeRegions)->Values)
				{
					FGuid Guid;
					if (FGuid::Parse(Pair.Key, Guid) && Pair.Value.IsValid())
					{
						Hints.NodeRegions.Add(Guid, static_cast<int32>(Pair.Value->AsNumber()));
					}
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
			if (Root->TryGetArrayField(TEXT("regions"), Regions) && Regions)
			{
				for (const TSharedPtr<FJsonValue>& Value : *Regions)
				{
					const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
					// One entry per element, valid or not: nodeRegions indexes this array.
					UE::DreamShader::IR::FIRRegion Region;
					double Parent = -1.0;
					if (Object.IsValid())
					{
						Object->TryGetStringField(TEXT("name"), Region.Name);
						Object->TryGetNumberField(TEXT("parent"), Parent);
					}
					Region.Parent = static_cast<int32>(Parent);
					Graph.Regions.Add(MoveTemp(Region));
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* Comments = nullptr;
			if (Options.bImportLayout && Root->TryGetArrayField(TEXT("comments"), Comments) && Comments)
			{
				for (const TSharedPtr<FJsonValue>& Value : *Comments)
				{
					const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
					if (!Object.IsValid())
					{
						continue;
					}
					UE::DreamShader::IR::FIRLayoutHint Hint;
					Hint.Kind = TEXT("Comment");
					double X = 0.0;
					double Y = 0.0;
					Object->TryGetStringField(TEXT("name"), Hint.Name);
					Object->TryGetNumberField(TEXT("x"), X);
					Object->TryGetNumberField(TEXT("y"), Y);
					Hint.X = static_cast<int32>(X);
					Hint.Y = static_cast<int32>(Y);
					double Width = 0.0;
					double Height = 0.0;
					if (Object->TryGetNumberField(TEXT("w"), Width) && Object->TryGetNumberField(TEXT("h"), Height))
					{
						Hint.bHasSize = true;
						Hint.W = static_cast<int32>(Width);
						Hint.H = static_cast<int32>(Height);
					}
					const TArray<TSharedPtr<FJsonValue>>* Color = nullptr;
					if (Object->TryGetArrayField(TEXT("color"), Color) && Color && Color->Num() == 4)
					{
						Hint.bHasColor = true;
						for (int32 Channel = 0; Channel < 4; ++Channel)
						{
							Hint.Color[Channel] = static_cast<float>((*Color)[Channel]->AsNumber());
						}
					}
					Graph.LayoutHints.Add(MoveTemp(Hint));
				}
			}
		}

		void FGraphImporter::ReadBoxes(const TConstArrayView<TObjectPtr<UMaterialExpressionComment>> Comments)
		{
			for (const TObjectPtr<UMaterialExpressionComment>& Comment : Comments)
			{
				if (!Comment)
				{
					continue;
				}
				FBox Box;
				Box.Title = Comment->Text;
				Box.bGenerated = Box.Title.StartsWith(GeneratedBoxPrefix, ESearchCase::CaseSensitive);
				if (Box.bGenerated)
				{
					Box.Title.RightChopInline(FCString::Strlen(GeneratedBoxPrefix), DREAMSHADER_ALLOW_SHRINKING_NO);
				}
				Box.X = Comment->MaterialExpressionEditorX;
				Box.Y = Comment->MaterialExpressionEditorY;
				Box.W = Comment->SizeX;
				Box.H = Comment->SizeY;
				Box.Color = Comment->CommentColor;
				// The frame the layout pass draws around the material's own pins is nobody's region.
				if (Box.bGenerated && Box.Title.Equals(MaterialOutputBoxTitle, ESearchCase::CaseSensitive))
				{
					continue;
				}
				Boxes.Add(MoveTemp(Box));
			}
		}

		void FGraphImporter::AssignRegions()
		{
			// With hints the regions are the source's own and every node already has its index. Without them the boxes are
			// all there is: a node belongs to the smallest box its top-left corner is in, a box to the smallest box it is in.
			if (Hints.bValid)
			{
				return;
			}

			const auto Contains = [](const FBox& Outer, const int32 X, const int32 Y)
			{
				return X >= Outer.X && Y >= Outer.Y && X <= Outer.X + Outer.W && Y <= Outer.Y + Outer.H;
			};
			const auto Area = [](const FBox& Box) { return static_cast<int64>(Box.W) * static_cast<int64>(Box.H); };

			for (const TPair<int32, const UMaterialExpression*>& Pair : ExpressionOfNode)
			{
				int32 Smallest = INDEX_NONE;
				for (int32 BoxIndex = 0; BoxIndex < Boxes.Num(); ++BoxIndex)
				{
					if (Contains(Boxes[BoxIndex], Pair.Value->MaterialExpressionEditorX, Pair.Value->MaterialExpressionEditorY)
						&& (Smallest == INDEX_NONE || Area(Boxes[BoxIndex]) < Area(Boxes[Smallest])))
					{
						Smallest = BoxIndex;
					}
				}
				if (Smallest == INDEX_NONE)
				{
					continue;
				}

				FBox& Box = Boxes[Smallest];
				++Box.NodesInside;
				if (Box.Region == INDEX_NONE)
				{
					UE::DreamShader::IR::FIRRegion Region;
					Region.Name = Box.Title;
					Box.Region = Graph.Regions.Add(MoveTemp(Region));
				}
				Graph.Nodes[Pair.Key].Region = Box.Region;
			}

			for (int32 BoxIndex = 0; BoxIndex < Boxes.Num(); ++BoxIndex)
			{
				const FBox& Box = Boxes[BoxIndex];
				if (Box.Region == INDEX_NONE)
				{
					continue;
				}
				int32 Parent = INDEX_NONE;
				for (int32 OtherIndex = 0; OtherIndex < Boxes.Num(); ++OtherIndex)
				{
					const FBox& Other = Boxes[OtherIndex];
					if (OtherIndex != BoxIndex
						&& Other.Region != INDEX_NONE
						&& Contains(Other, Box.X, Box.Y)
						&& Contains(Other, Box.X + Box.W, Box.Y + Box.H)
						&& Area(Other) > Area(Box)
						&& (Parent == INDEX_NONE || Area(Other) < Area(Boxes[Parent])))
					{
						Parent = OtherIndex;
					}
				}
				if (Parent != INDEX_NONE)
				{
					Graph.Regions[Box.Region].Parent = Boxes[Parent].Region;
				}
			}
		}

		void FGraphImporter::AddCommentHints()
		{
			if (!Options.bImportLayout)
			{
				return;
			}
			// A box somebody drew that is not a region -- the build's own are redrawn, and a box around nodes became one
			// above -- is a `#pragma layout(Comment, ...)`.
			for (const FBox& Box : Boxes)
			{
				if (Box.bGenerated || Box.Region != INDEX_NONE || Box.Title.IsEmpty())
				{
					continue;
				}
				const bool bKnown = Graph.LayoutHints.ContainsByPredicate([&Box](const UE::DreamShader::IR::FIRLayoutHint& Hint)
				{
					return Hint.Kind.Equals(TEXT("Comment"), ESearchCase::CaseSensitive) && Hint.Name.Equals(Box.Title, ESearchCase::CaseSensitive);
				});
				if (bKnown)
				{
					continue;
				}

				UE::DreamShader::IR::FIRLayoutHint Hint;
				Hint.Kind = TEXT("Comment");
				Hint.Name = Box.Title;
				Hint.X = Box.X;
				Hint.Y = Box.Y;
				Hint.W = Box.W;
				Hint.H = Box.H;
				Hint.bHasSize = Box.W > 0 && Box.H > 0;
				if (Box.Color != FLinearColor::White)
				{
					Hint.bHasColor = true;
					Hint.Color[0] = Box.Color.R;
					Hint.Color[1] = Box.Color.G;
					Hint.Color[2] = Box.Color.B;
					Hint.Color[3] = Box.Color.A;
				}
				Graph.LayoutHints.Add(MoveTemp(Hint));
			}
		}

		void FGraphImporter::AddStatementRoots(const TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions)
		{
			for (const TObjectPtr<UMaterialExpression>& Expression : Expressions)
			{
				if (Expression && Expression->IsA<UMaterialExpressionCustomOutput>())
				{
					EnsureImported(Expression.Get());
				}
			}
		}

		void FGraphImporter::ReportUnreachable(const TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions)
		{
			int32 Unreachable = 0;
			for (const TObjectPtr<UMaterialExpression>& Expression : Expressions)
			{
				if (!Expression
					|| States.Contains(Expression.Get())
					|| Expression->IsA<UMaterialExpressionReroute>()
					|| Expression->IsA<UMaterialExpressionNamedRerouteUsage>()
					|| Expression->IsA<UMaterialExpressionNamedRerouteDeclaration>()
					|| Expression->IsA<UMaterialExpressionComment>())
				{
					continue;
				}
				++Unreachable;
			}
			if (Unreachable > 0)
			{
				Info(TEXT("DSH9061"), FText::Format(
					LOCTEXT("UnreachableExpressions", "{0} expression(s) of '{1}' feed no output and are not part of the source; a build would prune them all the same."),
					FText::AsNumber(Unreachable),
					FText::FromString(Product.Name)));
			}
		}

		// ---------------------------------------------------------------------------------- the products

		void FGraphImporter::ImportMaterial(UMaterial* Material)
		{
			Product.Kind = UE::DreamShader::IR::EIRProductKind::Material;
			Product.Name = Material->GetName();

			// `#pragma material`: what a rebuild would not get anyway. The 1.x lines are `<tabs>Key = Value;`.
			TArray<FString> Lines;
			if (Material->MaterialDomain != MD_Surface)
			{
				Product.Settings.Add(TEXT("Domain"), GetMaterialDomainText(Material->MaterialDomain));
			}
			if (Material->BlendMode != BLEND_Opaque)
			{
				Product.Settings.Add(TEXT("BlendMode"), GetBlendModeText(Material->BlendMode));
			}
			const FString ShadingModel = GetShadingModelText(Material);
			if (!ShadingModel.Equals(TEXT("DefaultLit"), ESearchCase::IgnoreCase))
			{
				Product.Settings.Add(TEXT("ShadingModel"), ShadingModel);
			}
			AppendAdditionalMaterialSettings(Lines, Material);
			AppendEnumMaterialSettingIfDifferent(Lines, Material, TEXT("TranslucencyLightingMode"));
			AppendEnumMaterialSettingIfDifferent(Lines, Material, TEXT("RefractionMethod"));
			for (const FString& Line : Lines)
			{
				FString Key;
				FString Value;
				if (Line.TrimStartAndEnd().Split(TEXT("="), &Key, &Value))
				{
					Value.TrimStartAndEndInline();
					Value.RemoveFromEnd(TEXT(";"));
					Product.Settings.Add(Key.TrimStartAndEnd(), Value.TrimStartAndEnd().TrimQuotes());
				}
			}
			if (!FMath::IsNearlyEqual(Material->OpacityMaskClipValue, 0.3333f, 1.e-6f))
			{
				Product.Settings.Add(TEXT("OpacityMaskClipValue"), FString::SanitizeFloat(Material->OpacityMaskClipValue));
			}

			ReadHints(Material);
			ReadBoxes(Material->GetEditorComments());

			// The roots, in the catalog's attribute order. A material that reads its attributes as a set looks at that
			// one pin and at no other (EmitMaterialSink sets the flag from the same input).
			const UEnum* PropertyEnum = StaticEnum<EMaterialProperty>();
			FIRNode Sink;
			Sink.Op = EIROp::MaterialSink;
			int32 IgnoredPins = 0;
			for (const UE::DreamShader::IR::FCatalogMaterialAttribute& Attribute : Catalog.MaterialAttributes)
			{
				const int64 EnumValue = PropertyEnum ? PropertyEnum->GetValueByNameString(Attribute.PropertyName) : INDEX_NONE;
				if (EnumValue == INDEX_NONE)
				{
					continue;
				}
				const EMaterialProperty Property = static_cast<EMaterialProperty>(EnumValue);
				const FExpressionInput* Input = GetMaterialInputForDecompile(Material, Property);
				if (!Input || !Input->Expression)
				{
					continue;
				}
				if ((Property == MP_MaterialAttributes) != (Material->bUseMaterialAttributes != 0))
				{
					IgnoredPins += Property == MP_MaterialAttributes ? 0 : 1;
					continue;
				}

				UMaterialExpression* Source = nullptr;
				int32 OutputIndex = 0;
				TArray<const FExpressionInput*> Hops;
				if (TraceInput(*Input, Source, OutputIndex, Hops) && Source)
				{
					EnsureImported(Source);
				}
				const FIRValue Value = ResolveInput(*Input);
				if (Value.IsValid())
				{
					Sink.Inputs.Add({ Attribute.Name, Value });
				}
			}
			if (IgnoredPins > 0)
			{
				Info(TEXT("DSH9074"), FText::Format(
					LOCTEXT("PinsIgnoredUnderAttributes", "'{0}' reads its attributes as one set, so the {1} individual pin(s) that are also wired are ignored, by the engine and here."),
					FText::FromString(Product.Name),
					FText::AsNumber(IgnoredPins)));
			}

			AddStatementRoots(Material->GetExpressions());
			Graph.Sink = Graph.AddNode(MoveTemp(Sink));

			AssignRegions();
			AddCommentHints();
			ReportUnreachable(Material->GetExpressions());
		}

		void FGraphImporter::ImportFunction(UMaterialFunction* Function)
		{
			namespace Prop = UE::DreamShader::IR::Prop;

			Product.Kind = Function->IsA<UMaterialFunctionMaterialLayerBlend>() ? UE::DreamShader::IR::EIRProductKind::MaterialLayerBlend
				: Function->IsA<UMaterialFunctionMaterialLayer>() ? UE::DreamShader::IR::EIRProductKind::MaterialLayer
				: UE::DreamShader::IR::EIRProductKind::MaterialFunction;
			Product.Name = Function->GetName();
			Product.Description = Function->Description;
			if (Function->bExposeToLibrary)
			{
				// A bar nests, a comma separates (the emitter splits it the same way).
				TArray<FString> Categories;
				for (const FText& Category : Function->LibraryCategoriesText)
				{
					const FString Text = Category.ToString().TrimStartAndEnd();
					if (!Text.IsEmpty())
					{
						Categories.Add(Text);
					}
				}
				Product.LibraryPath = Categories.Num() > 0 ? FString::Join(Categories, TEXT(", ")) : FString(TEXT("Misc"));
			}
			if (!Function->UserExposedCaption.IsEmpty())
			{
				Warning(TEXT("DSH9071"), FText::Format(
					LOCTEXT("CaptionDropped", "'{0}' has the caption '{1}', which source has no directive for; the rebuilt function has none."),
					FText::FromString(Product.Name),
					FText::FromString(Function->UserExposedCaption)));
			}

			ReadHints(Function);
			ReadBoxes(Function->GetEditorComments());

			// (SortPriority, place among the expressions): the engine's own sort is unstable on ties, and a decompiler
			// that inherited it would write a different signature from one run to the next.
			TArray<UMaterialExpressionFunctionInput*> Inputs;
			TArray<UMaterialExpressionFunctionOutput*> Outputs;
			for (const TObjectPtr<UMaterialExpression>& Expression : Function->GetExpressions())
			{
				if (UMaterialExpressionFunctionInput* Input = Cast<UMaterialExpressionFunctionInput>(Expression.Get()))
				{
					Inputs.Add(Input);
				}
				else if (UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(Expression.Get()))
				{
					Outputs.Add(Output);
				}
			}
			Inputs.StableSort([](const UMaterialExpressionFunctionInput& A, const UMaterialExpressionFunctionInput& B) { return A.SortPriority < B.SortPriority; });
			Outputs.StableSort([](const UMaterialExpressionFunctionOutput& A, const UMaterialExpressionFunctionOutput& B) { return A.SortPriority < B.SortPriority; });

			for (UMaterialExpressionFunctionInput* Input : Inputs)
			{
				ImportFunctionInput(Input);
				States.Add(Input, EState::Done);
				Graph.FunctionInputs.Add(NodeOfExpression.FindChecked(Input));
			}

			for (UMaterialExpressionFunctionOutput* Output : Outputs)
			{
				UMaterialExpression* Source = nullptr;
				int32 OutputIndex = 0;
				TArray<const FExpressionInput*> Hops;
				if (TraceInput(Output->A, Source, OutputIndex, Hops) && Source)
				{
					EnsureImported(Source);
				}

				FIRNode Node;
				Node.Op = EIROp::FunctionOutput;
				Node.Operands.Add(ResolveInput(Output->A));
				Node.Properties.Add({ FString(Prop::OutputName), FIRPropertyValue::MakeName(Output->OutputName.IsNone() ? FString() : Output->OutputName.ToString()) });
				Node.Properties.Add({ FString(Prop::SortPriority), FIRPropertyValue::MakeInt(Output->SortPriority) });
				if (!Output->Description.IsEmpty())
				{
					Node.Properties.Add({ FString(Prop::Description), FIRPropertyValue::MakeString(Output->Description) });
				}
				States.Add(Output, EState::Done);
				const int32 NodeIndex = AddNode(MoveTemp(Node), Output);
				NodeOfExpression.Add(Output, NodeIndex);
				Graph.FunctionOutputs.Add(NodeIndex);
			}

			AddStatementRoots(Function->GetExpressions());
			AssignRegions();
			AddCommentHints();
			ReportUnreachable(Function->GetExpressions());
		}
	}

	bool ImportDreamShaderGraphToIR(
		UObject* Asset,
		const FGraphImportOptions& Options,
		UE::DreamShader::IR::FIRModule& InOutModule,
		FGraphImportContext& InOutContext,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics)
	{
		UMaterial* Material = Cast<UMaterial>(Asset);
		UMaterialFunction* Function = Cast<UMaterialFunction>(Asset);
		if (!Material && !Function)
		{
			Diagnostics.Error(TEXT("DSH9060"), UE::DreamShader::Lang::FLangSpan(), FText::Format(
				LOCTEXT("UnsupportedAsset", "'{0}' is neither a material nor a material function, so it has no graph to read. A material instance decompiles to a '.dsi'."),
				FText::FromString(Asset ? Asset->GetPathName() : FString(TEXT("<null>")))));
			return false;
		}

		const UE::DreamShader::IR::FBuiltinCatalog& Catalog = Options.Catalog
			? *Options.Catalog
			: UE::DreamShader::Editor::Compiler::GetDreamShaderBuiltinCatalog();

		UE::DreamShader::IR::FIRProduct& Product = InOutModule.Products.AddDefaulted_GetRef();
		GraphImport::FGraphImporter Importer(Options, Catalog, Product, InOutContext, Diagnostics);
		if (Material)
		{
			Importer.ImportMaterial(Material);
		}
		else
		{
			Importer.ImportFunction(Function);
		}
		return true;
	}

	const UE::DreamShader::Lang::FIRToAstExternInterface* ImportDreamShaderFunctionInterface(
		UMaterialFunctionInterface* Function,
		const FGraphImportOptions& Options,
		FGraphImportContext& InOutContext,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics)
	{
		if (!Function)
		{
			return nullptr;
		}

		const FString Path = GraphImport::NormalizeImportAssetPath(Function->GetPathName());
		if (const int32* Existing = InOutContext.InterfaceByPath.Find(Path))
		{
			return &InOutContext.ExternInterfaces[*Existing];
		}
		// A function that reaches itself: whoever is reading it further out will finish the interface.
		if (InOutContext.InterfacesInProgress.Contains(Path))
		{
			return nullptr;
		}

		UMaterialFunction* Base = Function->GetBaseFunction();
		if (!Base)
		{
			return nullptr;
		}

		// Its graph, read into a module of its own and thrown away: only the types of what reaches the outputs are kept.
		// Layout off, and the diagnostics of a callee are not the caller's to hear.
		InOutContext.InterfacesInProgress.Add(Path);
		FGraphImportOptions CalleeOptions = Options;
		CalleeOptions.bImportLayout = false;
		UE::DreamShader::IR::FIRModule Scratch;
		UE::DreamShader::Lang::FLangDiagnosticSink Quiet;
		const bool bImported = ImportDreamShaderGraphToIR(Base, CalleeOptions, Scratch, InOutContext, Quiet);
		InOutContext.InterfacesInProgress.Remove(Path);
		if (!bImported || Scratch.Products.Num() == 0)
		{
			return nullptr;
		}

		namespace Prop = UE::DreamShader::IR::Prop;
		const UE::DreamShader::IR::FIRGraph& Graph = Scratch.Products.Last().Graph;
		const auto TextOf = [](const UE::DreamShader::IR::FIRNode& Node, const TCHAR* Name) -> FString
		{
			const UE::DreamShader::IR::FIRProperty* Property = Node.FindProperty(Name);
			return Property ? Property->Value.S : FString();
		};

		UE::DreamShader::Lang::FIRToAstExternInterface Interface;
		Interface.AssetPath = Path;
		Interface.Description = Scratch.Products.Last().Description;

		for (const int32 NodeIndex : Graph.FunctionInputs)
		{
			const UE::DreamShader::IR::FIRNode& Node = Graph.Nodes[NodeIndex];
			UE::DreamShader::Lang::FIRToAstExternPin Pin;
			Pin.Name = TextOf(Node, Prop::InputName);
			Pin.Type = Node.Outputs.IsValidIndex(0) ? Node.Outputs[0] : UE::DreamShader::IR::FIRType::Float(1);
			Pin.Description = TextOf(Node, Prop::Description);
			if (const UE::DreamShader::IR::FIRProperty* Optional = Node.FindProperty(Prop::IsOptional))
			{
				Pin.bOptional = Optional->Value.B;
			}
			if (const UE::DreamShader::IR::FIRProperty* Preview = Node.FindProperty(Prop::PreviewValue))
			{
				Pin.bHasDefault = Pin.bOptional;
				for (int32 Index = 0; Index < 4; ++Index)
				{
					Pin.Default[Index] = Preview->Value.V[Index];
				}
			}
			Interface.Inputs.Add(MoveTemp(Pin));
		}

		for (const int32 NodeIndex : Graph.FunctionOutputs)
		{
			const UE::DreamShader::IR::FIRNode& Node = Graph.Nodes[NodeIndex];
			UE::DreamShader::Lang::FIRToAstExternPin Pin;
			Pin.Name = TextOf(Node, Prop::OutputName);
			Pin.Description = TextOf(Node, Prop::Description);
			Pin.Type = (Node.Operands.Num() > 0 && Graph.IsValidValue(Node.Operands[0]))
				? Graph.TypeOf(Node.Operands[0])
				: UE::DreamShader::IR::FIRType::Float(1);
			// A bool that reaches an output pin is the 0/1 float it is on the wire.
			if (Pin.Type.IsBool())
			{
				Pin.Type = UE::DreamShader::IR::FIRType::Float(FMath::Clamp(Pin.Type.GraphComponentCount(), 1, 4));
			}
			Interface.Outputs.Add(MoveTemp(Pin));
		}

		const int32 Index = InOutContext.ExternInterfaces.Add(MoveTemp(Interface));
		InOutContext.InterfaceByPath.Add(Path, Index);
		return &InOutContext.ExternInterfaces[Index];
	}
}

#undef LOCTEXT_NAMESPACE
