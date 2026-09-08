// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderGraphDump.h for what this is for. This file is the policy: what a canonical dump
// contains, and in what order.

#include "DreamShaderGraphDump.h"

#include "DreamShaderMaterialInstance.h"
#include "DreamShaderModule.h"
#include "DreamShaderParser.h"
#include "DreamShaderTypes.h"
#include "DreamShaderVersionCompat.h"

// GetMaterialInputForDecompile / MakeInputMaskSuffix / the Domain-Blend-ShadingModel formatters /
// the function parameter type names. Reused rather than rewritten: a second spelling of "what type
// is this function input" would be a second thing to keep in step with the language.
#include "Decompiler/DreamShaderGraphDecompilerHelpers.h"

// IsDigestProperty -- the project's existing answer to "which reflected property is CONTENT rather
// than presentation". It already excludes node coordinates, node colour, Desc, the comment-bubble
// and collapsed flags, every FGuid (MaterialExpressionGuid and the named-reroute variable id), every
// FExpressionInput (connections are recorded structurally below), and everything transient or
// non-editable (GraphNode, and the Material / Function back-pointers). Sharing it is deliberate: a
// dump that disagreed with the divergence digest about what counts as content would be two answers
// to one question.
#include "MaterialAssetGeneration/DreamShaderGeneratedAssetDigest.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeShared.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorSourceLoading.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Materials/MaterialInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "SceneTypes.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		// Bumped when the shape of the JSON changes. A capture is only comparable against another
		// capture carrying the same number, which is the whole point of writing it down.
		constexpr int32 GDreamShaderGraphDumpSchema = 1;

		// -----------------------------------------------------------------------------------------
		// A canonical JSON value.
		//
		// Hand-rolled rather than FJsonObject + TJsonWriter, for three reasons that are each fatal to
		// a fingerprint: FJsonObject stores its members in a TMap (iteration order is not stable),
		// TPrettyJsonPrintPolicy writes LINE_TERMINATOR (\r\n on Windows, so the same capture taken on
		// two platforms would differ on every line), and the number policy is not ours to pin.
		// -----------------------------------------------------------------------------------------
		class FDumpJson;
		using FDumpJsonRef = TSharedRef<FDumpJson>;

		class FDumpJson
		{
		public:
			enum class EKind : uint8
			{
				Null,
				Bool,
				Number,
				String,
				Object,
				Array
			};

			explicit FDumpJson(const EKind InKind)
				: Kind(InKind)
			{
			}

			static FDumpJsonRef Null()
			{
				return MakeShared<FDumpJson>(EKind::Null);
			}

			static FDumpJsonRef Bool(const bool bValue)
			{
				FDumpJsonRef Value = MakeShared<FDumpJson>(EKind::Bool);
				Value->bBoolValue = bValue;
				return Value;
			}

			static FDumpJsonRef Int(const int64 InValue)
			{
				FDumpJsonRef Value = MakeShared<FDumpJson>(EKind::Number);
				Value->Text = FString::Printf(TEXT("%lld"), static_cast<long long>(InValue)); /* I18N-EXEMPT: machine-readable dump */
				return Value;
			}

			static FDumpJsonRef UInt(const uint64 InValue)
			{
				FDumpJsonRef Value = MakeShared<FDumpJson>(EKind::Number);
				Value->Text = FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(InValue)); /* I18N-EXEMPT: machine-readable dump */
				return Value;
			}

			/**
			 * %.9g: the shortest fixed precision that round-trips a float exactly, and short enough
			 * that a double which merely holds a float never grows a tail of noise digits. JSON has no
			 * spelling for a non-finite number, so those become strings rather than invalid syntax.
			 */
			static FDumpJsonRef Double(const double InValue)
			{
				if (!FMath::IsFinite(InValue))
				{
					return String(FString::Printf(TEXT("%f"), InValue)); /* I18N-EXEMPT: machine-readable dump */
				}

				FDumpJsonRef Value = MakeShared<FDumpJson>(EKind::Number);
				Value->Text = FString::Printf(TEXT("%.9g"), InValue); /* I18N-EXEMPT: machine-readable dump */
				return Value;
			}

			static FDumpJsonRef String(const FString& InText)
			{
				FDumpJsonRef Value = MakeShared<FDumpJson>(EKind::String);
				Value->Text = InText;
				return Value;
			}

			static FDumpJsonRef Object()
			{
				return MakeShared<FDumpJson>(EKind::Object);
			}

			static FDumpJsonRef Array()
			{
				return MakeShared<FDumpJson>(EKind::Array);
			}

			void Set(const FString& Key, const FDumpJsonRef& Value)
			{
				Members.Emplace(Key, Value);
			}

			void Add(const FDumpJsonRef& Value)
			{
				Items.Add(Value);
			}

			int32 Num() const
			{
				return Items.Num();
			}

			void Write(FString& OutText, const int32 IndentLevel) const
			{
				switch (Kind)
				{
				case EKind::Null:
					OutText += TEXT("null");
					return;
				case EKind::Bool:
					OutText += bBoolValue ? TEXT("true") : TEXT("false");
					return;
				case EKind::Number:
					OutText += Text;
					return;
				case EKind::String:
					AppendQuoted(Text, OutText);
					return;
				case EKind::Object:
					WriteObject(OutText, IndentLevel);
					return;
				case EKind::Array:
					WriteArray(OutText, IndentLevel);
					return;
				}
			}

			/** One line, no spaces -- used as a sort key, never written to a file. */
			FString ToSortKey() const
			{
				FString Text2;
				AppendSortKey(Text2);
				return Text2;
			}

		private:
			static void AppendIndent(FString& OutText, const int32 IndentLevel)
			{
				for (int32 Index = 0; Index < IndentLevel; ++Index)
				{
					OutText += TEXT("  ");
				}
			}

			static void AppendQuoted(const FString& InText, FString& OutText)
			{
				OutText += TEXT("\"");
				for (const TCHAR Character : InText)
				{
					switch (Character)
					{
					case TEXT('\"'): OutText += TEXT("\\\""); break;
					case TEXT('\\'): OutText += TEXT("\\\\"); break;
					case TEXT('\b'): OutText += TEXT("\\b"); break;
					case TEXT('\f'): OutText += TEXT("\\f"); break;
					case TEXT('\n'): OutText += TEXT("\\n"); break;
					case TEXT('\r'): OutText += TEXT("\\r"); break;
					case TEXT('\t'): OutText += TEXT("\\t"); break;
					default:
						if (Character < 0x20)
						{
							OutText += FString::Printf(TEXT("\\u%04x"), static_cast<int32>(Character)); /* I18N-EXEMPT: machine-readable dump */
						}
						else
						{
							OutText.AppendChar(Character);
						}
						break;
					}
				}
				OutText += TEXT("\"");
			}

			void WriteObject(FString& OutText, const int32 IndentLevel) const
			{
				if (Members.IsEmpty())
				{
					OutText += TEXT("{}");
					return;
				}

				// Sorted case-SENSITIVELY. FString::operator< compares with Stricmp, under which two
				// keys differing only in case have no defined relative order -- and UPROPERTY names
				// differing only in case are legal.
				TArray<TPair<FString, FDumpJsonRef>> Sorted = Members;
				Sorted.Sort([](const TPair<FString, FDumpJsonRef>& Left, const TPair<FString, FDumpJsonRef>& Right)
				{
					return Left.Key.Compare(Right.Key, ESearchCase::CaseSensitive) < 0;
				});

				OutText += TEXT("{\n");
				for (int32 Index = 0; Index < Sorted.Num(); ++Index)
				{
					AppendIndent(OutText, IndentLevel + 1);
					AppendQuoted(Sorted[Index].Key, OutText);
					OutText += TEXT(": ");
					Sorted[Index].Value->Write(OutText, IndentLevel + 1);
					OutText += (Index + 1 < Sorted.Num()) ? TEXT(",\n") : TEXT("\n");
				}
				AppendIndent(OutText, IndentLevel);
				OutText += TEXT("}");
			}

			void WriteArray(FString& OutText, const int32 IndentLevel) const
			{
				if (Items.IsEmpty())
				{
					OutText += TEXT("[]");
					return;
				}

				OutText += TEXT("[\n");
				for (int32 Index = 0; Index < Items.Num(); ++Index)
				{
					AppendIndent(OutText, IndentLevel + 1);
					Items[Index]->Write(OutText, IndentLevel + 1);
					OutText += (Index + 1 < Items.Num()) ? TEXT(",\n") : TEXT("\n");
				}
				AppendIndent(OutText, IndentLevel);
				OutText += TEXT("]");
			}

			void AppendSortKey(FString& OutText) const
			{
				switch (Kind)
				{
				case EKind::Null:
					OutText += TEXT("null");
					return;
				case EKind::Bool:
					OutText += bBoolValue ? TEXT("true") : TEXT("false");
					return;
				case EKind::Number:
				case EKind::String:
					OutText += Text;
					return;
				case EKind::Object:
				{
					TArray<TPair<FString, FDumpJsonRef>> Sorted = Members;
					Sorted.Sort([](const TPair<FString, FDumpJsonRef>& Left, const TPair<FString, FDumpJsonRef>& Right)
					{
						return Left.Key.Compare(Right.Key, ESearchCase::CaseSensitive) < 0;
					});
					OutText += TEXT("{");
					for (const TPair<FString, FDumpJsonRef>& Member : Sorted)
					{
						OutText += Member.Key;
						OutText += TEXT("=");
						Member.Value->AppendSortKey(OutText);
						OutText += TEXT(";");
					}
					OutText += TEXT("}");
					return;
				}
				case EKind::Array:
					OutText += TEXT("[");
					for (const FDumpJsonRef& Item : Items)
					{
						Item->AppendSortKey(OutText);
						OutText += TEXT(";");
					}
					OutText += TEXT("]");
					return;
				}
			}

			EKind Kind = EKind::Null;
			bool bBoolValue = false;
			FString Text;
			TArray<TPair<FString, FDumpJsonRef>> Members;
			TArray<FDumpJsonRef> Items;
		};

		// -----------------------------------------------------------------------------------------
		// Reflection -> JSON
		// -----------------------------------------------------------------------------------------

		FString MakeEnumValueName(const UEnum* Enum, const int64 Value)
		{
			if (Enum)
			{
				const FString Name = Enum->GetNameStringByValue(Value);
				if (!Name.IsEmpty())
				{
					return Name;
				}
			}
			return FString::Printf(TEXT("%lld"), static_cast<long long>(Value)); /* I18N-EXEMPT: machine-readable dump */
		}

		bool IsGuidStructProperty(const FProperty* Property)
		{
			const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
			return StructProperty && StructProperty->Struct == TBaseStructure<FGuid>::Get();
		}

		FDumpJsonRef MakeReflectedValueJson(const FProperty* Property, const void* ValuePtr);

		/**
		 * A struct's fields as an object, skipping every FGuid. Used for the material instance's
		 * parameter overrides, whose element structs each carry an ExpressionGUID that is generated
		 * per session and would be the one thing in the whole dump that never repeats.
		 */
		FDumpJsonRef MakeStructValueJson(const UScriptStruct* Struct, const void* Data, const int32 Depth = 0)
		{
			const FDumpJsonRef Object = FDumpJson::Object();
			if (!Struct || !Data || Depth > 4)
			{
				return Object;
			}

			for (TFieldIterator<FProperty> It(Struct, EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				const FProperty* Property = *It;
				if (!Property || IsGuidStructProperty(Property))
				{
					continue;
				}

				const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Data);
				if (!ValuePtr)
				{
					continue;
				}

				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					Object->Set(Property->GetName(), MakeStructValueJson(StructProperty->Struct, ValuePtr, Depth + 1));
					continue;
				}

				Object->Set(Property->GetName(), MakeReflectedValueJson(Property, ValuePtr));
			}

			return Object;
		}

		FDumpJsonRef MakeReflectedValueJson(const FProperty* Property, const void* ValuePtr)
		{
			if (!Property || !ValuePtr)
			{
				return FDumpJson::Null();
			}

			if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
			{
				return FDumpJson::Bool(BoolProperty->GetPropertyValue(ValuePtr));
			}

			if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
			{
				const int64 Value = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
				return FDumpJson::String(MakeEnumValueName(EnumProperty->GetEnum(), Value));
			}

			// Before FNumericProperty: FByteProperty is one, and a TEnumAsByte must read as its
			// enumerator name rather than as the integer behind it, or a reordered engine enum would
			// silently rewrite the meaning of an old capture.
			if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
			{
				const uint8 Value = ByteProperty->GetPropertyValue(ValuePtr);
				return ByteProperty->Enum
					? FDumpJson::String(MakeEnumValueName(ByteProperty->Enum, Value))
					: FDumpJson::Int(Value);
			}

			if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
			{
				if (NumericProperty->IsFloatingPoint())
				{
					return FDumpJson::Double(NumericProperty->GetFloatingPointPropertyValue(ValuePtr));
				}

				if (CastField<FUInt16Property>(Property) || CastField<FUInt32Property>(Property) || CastField<FUInt64Property>(Property))
				{
					return FDumpJson::UInt(NumericProperty->GetUnsignedIntPropertyValue(ValuePtr));
				}

				return FDumpJson::Int(NumericProperty->GetSignedIntPropertyValue(ValuePtr));
			}

			if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
			{
				return FDumpJson::String(NameProperty->GetPropertyValue(ValuePtr).ToString());
			}

			if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
			{
				return FDumpJson::String(StringProperty->GetPropertyValue(ValuePtr));
			}

			// The display string only: an FText exports with a namespace/key envelope that a rebuild
			// is free to reissue.
			if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
			{
				return FDumpJson::String(TextProperty->GetPropertyValue(ValuePtr).ToString());
			}

			// The referenced asset's path, never the pointer: a texture swap has to register, and the
			// pointer value differs every session.
			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				const UObject* Referenced = ObjectProperty->GetObjectPropertyValue(ValuePtr);
				return Referenced ? FDumpJson::String(Referenced->GetPathName()) : FDumpJson::Null();
			}

			// Structs, arrays, sets and maps: exported text. IsDigestProperty has already refused
			// every struct that can reach an FExpressionInput or an FGuid, so what is left exports
			// deterministically.
			FString Exported;
			Property->ExportTextItem_Direct(Exported, ValuePtr, nullptr, nullptr, PPF_None);
			return FDumpJson::String(Exported);
		}

		/** The behavioural properties of one node, as a sorted object. */
		FDumpJsonRef MakeExpressionPropsJson(const UMaterialExpression* Expression)
		{
			const FDumpJsonRef Props = FDumpJson::Object();
			if (!Expression)
			{
				return Props;
			}

			for (TFieldIterator<FProperty> It(Expression->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				const FProperty* Property = *It;
				if (!IsDigestProperty(Property))
				{
					continue;
				}

				const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Expression);
				if (!ValuePtr)
				{
					continue;
				}

				Props->Set(Property->GetName(), MakeReflectedValueJson(Property, ValuePtr));
			}

			return Props;
		}

		// -----------------------------------------------------------------------------------------
		// Canonical traversal
		// -----------------------------------------------------------------------------------------

		/**
		 * Node order, and therefore node identity.
		 *
		 * Post-order depth-first from the sinks: a node is numbered only after everything feeding it
		 * is, so `from` always names a node that appeared earlier in the array. The alternative --
		 * the engine's own `GetExpressions()` order -- is creation order, which is a property of the
		 * compiler that built the graph rather than of the graph, and is exactly what two compilers
		 * cannot be expected to agree on.
		 */
		class FGraphDumpOrder
		{
		public:
			void VisitSink(UMaterialExpression* Expression)
			{
				if (!Expression || VisitedExpressions.Contains(Expression))
				{
					return;
				}

				struct FFrame
				{
					UMaterialExpression* Expression = nullptr;
					TArray<UMaterialExpression*> Children;
					int32 NextChild = 0;
				};

				TArray<FFrame> Stack;
				Stack.Add({ Expression, GatherChildren(Expression), 0 });
				OnStack.Add(Expression);

				while (!Stack.IsEmpty())
				{
					FFrame& Top = Stack.Last();
					if (Top.NextChild < Top.Children.Num())
					{
						UMaterialExpression* Child = Top.Children[Top.NextChild++];
						if (Child && !VisitedExpressions.Contains(Child) && !OnStack.Contains(Child))
						{
							Stack.Add({ Child, GatherChildren(Child), 0 });
							OnStack.Add(Child);
						}
						continue;
					}

					UMaterialExpression* Finished = Top.Expression;
					Stack.Pop();
					OnStack.Remove(Finished);
					VisitedExpressions.Add(Finished);
					IdByExpression.Add(Finished, Order.Num());
					Order.Add(Finished);
				}
			}

			/**
			 * Everything the sink walk did not reach: dead nodes, custom-output sinks, unused function
			 * inputs. Sorted by class then by the node's own property object, so the order is a
			 * property of the nodes rather than of the array they happen to sit in. Two nodes that
			 * are identical on both keys are genuinely interchangeable except for what feeds them;
			 * the array index breaks that last tie, which keeps a single capture reproducible even
			 * though it is the one ordering rule two different compilers need not agree on.
			 */
			void VisitRemaining(TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions)
			{
				struct FPendingNode
				{
					UMaterialExpression* Expression = nullptr;
					FString ClassPath;
					FString PropsKey;
					int32 ArrayIndex = 0;
				};

				TArray<FPendingNode> Pending;
				for (int32 Index = 0; Index < Expressions.Num(); ++Index)
				{
					UMaterialExpression* Expression = Expressions[Index].Get();
					if (!Expression || VisitedExpressions.Contains(Expression))
					{
						continue;
					}

					Pending.Add({
						Expression,
						Expression->GetClass()->GetPathName(),
						MakeExpressionPropsJson(Expression)->ToSortKey(),
						Index });
				}

				Pending.Sort([](const FPendingNode& Left, const FPendingNode& Right)
				{
					const int32 ClassCompare = Left.ClassPath.Compare(Right.ClassPath, ESearchCase::CaseSensitive);
					if (ClassCompare != 0)
					{
						return ClassCompare < 0;
					}

					const int32 PropsCompare = Left.PropsKey.Compare(Right.PropsKey, ESearchCase::CaseSensitive);
					if (PropsCompare != 0)
					{
						return PropsCompare < 0;
					}

					return Left.ArrayIndex < Right.ArrayIndex;
				});

				for (const FPendingNode& Node : Pending)
				{
					VisitSink(Node.Expression);
				}
			}

			const TArray<UMaterialExpression*>& GetOrder() const
			{
				return Order;
			}

			bool TryGetId(const UMaterialExpression* Expression, int32& OutId) const
			{
				if (const int32* Found = IdByExpression.Find(Expression))
				{
					OutId = *Found;
					return true;
				}
				return false;
			}

		private:
			static TArray<UMaterialExpression*> GatherChildren(UMaterialExpression* Expression)
			{
				TArray<UMaterialExpression*> Children;
				if (!Expression)
				{
					return Children;
				}

				const int32 InputCount = GetDreamShaderExpressionInputCount(Expression);
				for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
				{
					const FExpressionInput* Input = Expression->GetInput(InputIndex);
					if (Input && Input->Expression)
					{
						// Raw pointer on this engine, TObjectPtr on others: assignment converts either way.
						UMaterialExpression* Child = Input->Expression;
						Children.Add(Child);
					}
				}

				// A named-reroute usage has no input pin; its edge is the Declaration pointer. Walking
				// it here is what keeps the declaration's whole subtree inside the sink traversal
				// instead of falling into the unreached bucket, where its order would be decided by a
				// sort rather than by the graph.
				if (const UMaterialExpressionNamedRerouteUsage* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Expression))
				{
					UMaterialExpression* DeclarationNode = Usage->Declaration;
					if (DeclarationNode)
					{
						Children.Add(DeclarationNode);
					}
				}

				return Children;
			}

			TArray<UMaterialExpression*> Order;
			TMap<const UMaterialExpression*, int32> IdByExpression;
			TSet<const UMaterialExpression*> VisitedExpressions;
			TSet<const UMaterialExpression*> OnStack;
		};

		FString MakeNodeId(const int32 Index)
		{
			return FString::Printf(TEXT("n%d"), Index); /* I18N-EXEMPT: machine-readable dump */
		}

		/** `{ from, output, mask }` for one connected pin, or null when the source is not in the dump. */
		FDumpJsonRef MakeConnectionJson(const FExpressionInput& Input, const FGraphDumpOrder& Order)
		{
			int32 SourceId = INDEX_NONE;
			const UMaterialExpression* SourceExpression = Input.Expression;
			if (!SourceExpression || !Order.TryGetId(SourceExpression, SourceId))
			{
				return FDumpJson::Null();
			}

			const FDumpJsonRef Connection = FDumpJson::Object();
			Connection->Set(TEXT("from"), FDumpJson::String(MakeNodeId(SourceId)));
			Connection->Set(TEXT("output"), FDumpJson::Int(Input.OutputIndex));

			const FString Mask = MakeInputMaskSuffix(Input);
			Connection->Set(TEXT("mask"), Mask.IsEmpty() ? FDumpJson::Null() : FDumpJson::String(Mask));
			return Connection;
		}

		FDumpJsonRef MakeNodesJson(const FGraphDumpOrder& Order)
		{
			const FDumpJsonRef Nodes = FDumpJson::Array();
			const TArray<UMaterialExpression*>& Expressions = Order.GetOrder();
			for (int32 Index = 0; Index < Expressions.Num(); ++Index)
			{
				UMaterialExpression* Expression = Expressions[Index];
				const FDumpJsonRef Node = FDumpJson::Object();
				Node->Set(TEXT("id"), FDumpJson::String(MakeNodeId(Index)));
				Node->Set(TEXT("class"), FDumpJson::String(Expression->GetClass()->GetPathName()));
				Node->Set(TEXT("props"), MakeExpressionPropsJson(Expression));

				const FDumpJsonRef Inputs = FDumpJson::Array();
				const int32 InputCount = GetDreamShaderExpressionInputCount(Expression);
				for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
				{
					const FExpressionInput* Input = Expression->GetInput(InputIndex);
					if (!Input || !Input->Expression)
					{
						continue;
					}

					const FDumpJsonRef Connection = MakeConnectionJson(*Input, Order);
					const FName InputName = Expression->GetInputName(InputIndex);
					Connection->Set(TEXT("index"), FDumpJson::Int(InputIndex));
					Connection->Set(TEXT("name"), InputName.IsNone() ? FDumpJson::Null() : FDumpJson::String(InputName.ToString()));
					Inputs->Add(Connection);
				}
				Node->Set(TEXT("inputs"), Inputs);

				// Named reroutes are linked by NAME, not by node id: the declaration's identity in the
				// source is its name, and the usage's own VariableGuid is excluded from the dump.
				if (const UMaterialExpressionNamedRerouteUsage* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Expression))
				{
					const FDumpJsonRef Reroute = FDumpJson::Object();
					Reroute->Set(
						TEXT("declaration"),
						Usage->Declaration
							? FDumpJson::String(Usage->Declaration->Name.ToString())
							: FDumpJson::Null());
					Node->Set(TEXT("reroute"), Reroute);
				}

				Nodes->Add(Node);
			}

			return Nodes;
		}

		// -----------------------------------------------------------------------------------------
		// Asset-level sections
		// -----------------------------------------------------------------------------------------

		/**
		 * The bool settings the decompiler writes into a Shader block's `Settings`, which is the set
		 * the generator applies -- but written unconditionally rather than only when they differ from
		 * the CDO. A fingerprint that omitted a value because it happened to equal a default would
		 * change shape when the engine changed that default, which is the one thing it must not do.
		 *
		 * Looked up by name so a setting that only exists on some engine versions (bHasPixelAnimation
		 * is 5.4+) needs no version macro here: an absent property is simply absent from the dump.
		 */
		const TCHAR* GDumpMaterialBoolSettings[] =
		{
			TEXT("TwoSided"),
			TEXT("Wireframe"),
			TEXT("DitheredLODTransition"),
			TEXT("DitherOpacityMask"),
			TEXT("bAllowNegativeEmissiveColor"),
			TEXT("bCastDynamicShadowAsMasked"),
			TEXT("bEnableResponsiveAA"),
			TEXT("bScreenSpaceReflections"),
			TEXT("bContactShadows"),
			TEXT("bDisableDepthTest"),
			TEXT("bOutputTranslucentVelocity"),
			TEXT("bTangentSpaceNormal"),
			TEXT("bFullyRough"),
			TEXT("bIsSky"),
			TEXT("bIsThinSurface"),
			TEXT("bHasPixelAnimation"),
			TEXT("bUsedWithSkeletalMesh"),
			TEXT("bUsedWithMorphTargets"),
			TEXT("bUsedWithClothing"),
			TEXT("bUsedWithNanite"),
			TEXT("bUsedWithEditorCompositing"),
			TEXT("bUsedWithParticleSprites"),
			TEXT("bUsedWithBeamTrails"),
			TEXT("bUsedWithMeshParticles"),
			TEXT("bUsedWithNiagaraSprites"),
			TEXT("bUsedWithNiagaraRibbons"),
			TEXT("bUsedWithNiagaraMeshParticles"),
			TEXT("bUsedWithGeometryCache"),
			TEXT("bUsedWithStaticLighting"),
			TEXT("bUsedWithSplineMeshes"),
			TEXT("bUsedWithInstancedStaticMeshes"),
			TEXT("bUsedWithGeometryCollections"),
			TEXT("bUsedWithHairStrands"),
			TEXT("bUsedWithWater"),
			TEXT("bUsedWithVirtualHeightfieldMesh"),
			TEXT("bUsedWithVolumetricCloud"),
			TEXT("bCastRayTracedShadows"),
			TEXT("bWriteOnlyAlpha"),
			TEXT("BlendableOutputAlpha"),
			TEXT("bAlwaysEvaluateWorldPositionOffset")
		};

		FDumpJsonRef MakeMaterialSettingsJson(UMaterial* Material)
		{
			const FDumpJsonRef Settings = FDumpJson::Object();
			if (!Material)
			{
				return Settings;
			}

			Settings->Set(TEXT("Domain"), FDumpJson::String(GetMaterialDomainText(Material->MaterialDomain.GetValue())));
			Settings->Set(TEXT("ShadingModel"), FDumpJson::String(GetShadingModelText(Material)));
			Settings->Set(TEXT("BlendMode"), FDumpJson::String(GetBlendModeText(Material->BlendMode.GetValue())));

			for (const TCHAR* SettingName : GDumpMaterialBoolSettings)
			{
				const FBoolProperty* BoolProperty = CastField<FBoolProperty>(
					UMaterial::StaticClass()->FindPropertyByName(FName(SettingName)));
				if (!BoolProperty)
				{
					continue;
				}

				Settings->Set(SettingName, FDumpJson::Bool(BoolProperty->GetPropertyValue_InContainer(Material)));
			}

			if (const FProperty* DecalResponse = UMaterial::StaticClass()->FindPropertyByName(TEXT("MaterialDecalResponse")))
			{
				if (const void* ValuePtr = DecalResponse->ContainerPtrToValuePtr<void>(Material))
				{
					Settings->Set(TEXT("MaterialDecalResponse"), MakeReflectedValueJson(DecalResponse, ValuePtr));
				}
			}

			return Settings;
		}

		FDumpJsonRef MakeMaterialPropertiesJson(UMaterial* Material, const FGraphDumpOrder& Order)
		{
			const FDumpJsonRef Properties = FDumpJson::Object();
			if (!Material)
			{
				return Properties;
			}

			const UEnum* PropertyEnum = StaticEnum<EMaterialProperty>();
			for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
			{
				const EMaterialProperty Property = static_cast<EMaterialProperty>(PropertyIndex);
				const FExpressionInput* Input = GetMaterialInputForDecompile(Material, Property);
				if (!Input || !Input->Expression)
				{
					continue;
				}

				Properties->Set(MakeEnumValueName(PropertyEnum, PropertyIndex), MakeConnectionJson(*Input, Order));
			}

			return Properties;
		}

		void CollectMaterialSinks(UMaterial* Material, FGraphDumpOrder& Order)
		{
			if (!Material)
			{
				return;
			}

			// EMaterialProperty enumerator order, which is the engine's own declaration order and the
			// only ordering of the material's outputs that exists outside a particular compiler.
			for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
			{
				const FExpressionInput* Input =
					GetMaterialInputForDecompile(Material, static_cast<EMaterialProperty>(PropertyIndex));
				if (Input && Input->Expression)
				{
					UMaterialExpression* Sink = Input->Expression;
					Order.VisitSink(Sink);
				}
			}

			Order.VisitRemaining(Material->GetExpressions());
		}

		FDumpJsonRef MakeFunctionSettingsJson(UMaterialFunction* MaterialFunction)
		{
			const FDumpJsonRef Settings = FDumpJson::Object();
			if (!MaterialFunction)
			{
				return Settings;
			}

			Settings->Set(
				TEXT("Usage"),
				FDumpJson::String(MakeEnumValueName(
					StaticEnum<EMaterialFunctionUsage>(),
					static_cast<int64>(MaterialFunction->GetMaterialFunctionUsage()))));
			Settings->Set(TEXT("Description"), FDumpJson::String(MaterialFunction->Description));
			Settings->Set(TEXT("bExposeToLibrary"), FDumpJson::Bool(MaterialFunction->bExposeToLibrary != 0));

			const FDumpJsonRef Categories = FDumpJson::Array();
			for (const FText& Category : MaterialFunction->LibraryCategoriesText)
			{
				Categories->Add(FDumpJson::String(Category.ToString()));
			}
			Settings->Set(TEXT("LibraryCategories"), Categories);
			return Settings;
		}

		FDumpJsonRef MakePreviewValueJson(const FVector4f& PreviewValue)
		{
			const FDumpJsonRef Preview = FDumpJson::Array();
			Preview->Add(FDumpJson::Double(PreviewValue.X));
			Preview->Add(FDumpJson::Double(PreviewValue.Y));
			Preview->Add(FDumpJson::Double(PreviewValue.Z));
			Preview->Add(FDumpJson::Double(PreviewValue.W));
			return Preview;
		}

		// -----------------------------------------------------------------------------------------
		// The three asset shapes
		// -----------------------------------------------------------------------------------------

		void AppendMaterialGraphSections(UMaterial* Material, const FDumpJsonRef& Root, int32& OutNodeCount)
		{
			FGraphDumpOrder Order;
			CollectMaterialSinks(Material, Order);
			Root->Set(TEXT("nodes"), MakeNodesJson(Order));
			Root->Set(TEXT("properties"), MakeMaterialPropertiesJson(Material, Order));
			Root->Set(TEXT("settings"), MakeMaterialSettingsJson(Material));
			OutNodeCount = Order.GetOrder().Num();
		}

		void AppendFunctionGraphSections(UMaterialFunction* MaterialFunction, const FDumpJsonRef& Root, int32& OutNodeCount)
		{
			TArray<FFunctionExpressionInput> FunctionInputs;
			TArray<FFunctionExpressionOutput> FunctionOutputs;
			MaterialFunction->GetInputsAndOutputs(FunctionInputs, FunctionOutputs);

			// GetInputsAndOutputs returns both lists already ordered by SortPriority with declaration
			// order breaking ties -- the same order the decompiler emits an Inputs/Outputs block in,
			// and the same order a call site's pins appear in.
			FGraphDumpOrder Order;
			for (const FFunctionExpressionOutput& Output : FunctionOutputs)
			{
				if (UMaterialExpression* OutputSink = Output.ExpressionOutput.Get())
				{
					Order.VisitSink(OutputSink);
				}
			}
			Order.VisitRemaining(MaterialFunction->GetExpressions());

			Root->Set(TEXT("nodes"), MakeNodesJson(Order));
			Root->Set(TEXT("settings"), MakeFunctionSettingsJson(MaterialFunction));
			OutNodeCount = Order.GetOrder().Num();

			const FDumpJsonRef Inputs = FDumpJson::Array();
			for (const FFunctionExpressionInput& Input : FunctionInputs)
			{
				const UMaterialExpressionFunctionInput* InputExpression = Input.ExpressionInput;
				const FDumpJsonRef Entry = FDumpJson::Object();
				Entry->Set(
					TEXT("name"),
					FDumpJson::String(InputExpression ? InputExpression->InputName.ToString() : Input.Input.InputName.ToString()));
				const EFunctionInputType InputType = InputExpression ? InputExpression->InputType.GetValue() : FunctionInput_Vector4;
				Entry->Set(TEXT("type"), FDumpJson::String(GetDreamShaderTypeForFunctionInput(InputType)));
				Entry->Set(TEXT("sortPriority"), FDumpJson::Int(InputExpression ? InputExpression->SortPriority : 0));
				Entry->Set(TEXT("description"), FDumpJson::String(InputExpression ? InputExpression->Description : FString()));
				Entry->Set(
					TEXT("optional"),
					FDumpJson::Bool(InputExpression && InputExpression->bUsePreviewValueAsDefault != 0));
				Entry->Set(
					TEXT("preview"),
					InputExpression ? MakePreviewValueJson(InputExpression->PreviewValue) : FDumpJson::Null());
				Inputs->Add(Entry);
			}
			Root->Set(TEXT("inputs"), Inputs);

			const FDumpJsonRef Outputs = FDumpJson::Array();
			for (const FFunctionExpressionOutput& Output : FunctionOutputs)
			{
				const UMaterialExpressionFunctionOutput* OutputExpression = Output.ExpressionOutput;
				const FDumpJsonRef Entry = FDumpJson::Object();
				Entry->Set(
					TEXT("name"),
					FDumpJson::String(OutputExpression ? OutputExpression->OutputName.ToString() : Output.Output.OutputName.ToString()));
				Entry->Set(TEXT("type"), FDumpJson::String(GetDreamShaderTypeForFunctionOutput(OutputExpression)));
				Entry->Set(TEXT("sortPriority"), FDumpJson::Int(OutputExpression ? OutputExpression->SortPriority : 0));
				Entry->Set(TEXT("description"), FDumpJson::String(OutputExpression ? OutputExpression->Description : FString()));
				Outputs->Add(Entry);
			}
			Root->Set(TEXT("outputs"), Outputs);
		}

		FDumpJsonRef MakeInstanceParametersJson(UMaterialInstance* Instance)
		{
			const FDumpJsonRef Parameters = FDumpJson::Object();
			if (!Instance)
			{
				return Parameters;
			}

			// Every `*ParameterValues` array in one reflection sweep rather than a hand-written list:
			// the set grows between engine versions (texture collections, sparse volume textures) and
			// a missed array would be an override that vanished from the fingerprint.
			for (TFieldIterator<FProperty> It(Instance->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(*It);
				if (!ArrayProperty || !ArrayProperty->GetName().EndsWith(TEXT("ParameterValues"), ESearchCase::CaseSensitive))
				{
					continue;
				}

				const FStructProperty* ElementProperty = CastField<FStructProperty>(ArrayProperty->Inner);
				if (!ElementProperty)
				{
					continue;
				}

				const void* ArrayPtr = ArrayProperty->ContainerPtrToValuePtr<void>(Instance);
				if (!ArrayPtr)
				{
					continue;
				}

				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayPtr);
				TArray<TPair<FString, FDumpJsonRef>> Entries;
				for (int32 ElementIndex = 0; ElementIndex < ArrayHelper.Num(); ++ElementIndex)
				{
					const FDumpJsonRef Entry = MakeStructValueJson(ElementProperty->Struct, ArrayHelper.GetRawPtr(ElementIndex));
					Entries.Emplace(Entry->ToSortKey(), Entry);
				}

				// Sorted: the engine's array order is whatever the last UpdateStaticPermutation left,
				// which is a fact about a run rather than about the asset.
				Entries.Sort([](const TPair<FString, FDumpJsonRef>& Left, const TPair<FString, FDumpJsonRef>& Right)
				{
					return Left.Key.Compare(Right.Key, ESearchCase::CaseSensitive) < 0;
				});

				const FDumpJsonRef Values = FDumpJson::Array();
				for (const TPair<FString, FDumpJsonRef>& Entry : Entries)
				{
					Values->Add(Entry.Value);
				}
				Parameters->Set(ArrayProperty->GetName(), Values);
			}

			const FStaticParameterSet& StaticParameters = Instance->GetStaticParameters();

			TArray<TPair<FString, FDumpJsonRef>> Switches;
			for (const FStaticSwitchParameter& Switch : StaticParameters.StaticSwitchParameters)
			{
				const FDumpJsonRef Entry = FDumpJson::Object();
				Entry->Set(TEXT("name"), FDumpJson::String(Switch.ParameterInfo.Name.ToString()));
				Entry->Set(TEXT("value"), FDumpJson::Bool(Switch.Value));
				Entry->Set(TEXT("override"), FDumpJson::Bool(Switch.bOverride != 0));
				Switches.Emplace(Entry->ToSortKey(), Entry);
			}
			Switches.Sort([](const TPair<FString, FDumpJsonRef>& Left, const TPair<FString, FDumpJsonRef>& Right)
			{
				return Left.Key.Compare(Right.Key, ESearchCase::CaseSensitive) < 0;
			});
			const FDumpJsonRef SwitchValues = FDumpJson::Array();
			for (const TPair<FString, FDumpJsonRef>& Entry : Switches)
			{
				SwitchValues->Add(Entry.Value);
			}
			Parameters->Set(TEXT("StaticSwitchParameters"), SwitchValues);

			TArray<TPair<FString, FDumpJsonRef>> Masks;
			for (const FStaticComponentMaskParameter& Mask : StaticParameters.EditorOnly.StaticComponentMaskParameters)
			{
				const FDumpJsonRef Entry = FDumpJson::Object();
				Entry->Set(TEXT("name"), FDumpJson::String(Mask.ParameterInfo.Name.ToString()));
				Entry->Set(TEXT("r"), FDumpJson::Bool(Mask.R));
				Entry->Set(TEXT("g"), FDumpJson::Bool(Mask.G));
				Entry->Set(TEXT("b"), FDumpJson::Bool(Mask.B));
				Entry->Set(TEXT("a"), FDumpJson::Bool(Mask.A));
				Entry->Set(TEXT("override"), FDumpJson::Bool(Mask.bOverride != 0));
				Masks.Emplace(Entry->ToSortKey(), Entry);
			}
			Masks.Sort([](const TPair<FString, FDumpJsonRef>& Left, const TPair<FString, FDumpJsonRef>& Right)
			{
				return Left.Key.Compare(Right.Key, ESearchCase::CaseSensitive) < 0;
			});
			const FDumpJsonRef MaskValues = FDumpJson::Array();
			for (const TPair<FString, FDumpJsonRef>& Entry : Masks)
			{
				MaskValues->Add(Entry.Value);
			}
			Parameters->Set(TEXT("StaticComponentMaskParameters"), MaskValues);

			return Parameters;
		}

		// -----------------------------------------------------------------------------------------
		// Source location and file naming
		// -----------------------------------------------------------------------------------------

		bool TryMakeRelativeUnder(const FString& Path, const FString& Directory, FString& OutRelative)
		{
			if (Directory.IsEmpty() || !UE::DreamShader::IsPathUnderSourceDirectory(Path, Directory))
			{
				return false;
			}

			// Both sides through the SAME normalizer the containment test above used, before any
			// length is taken off one of them. NormalizeSourceFilePath ends in MakeStandardFilename,
			// which rewrites a path under the project or engine into a relative one -- so chopping a
			// raw directory's length off a normalized path would cut in the wrong place, and the
			// containment test would still have said yes.
			const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(Path);
			FString Base = UE::DreamShader::NormalizeSourceFilePath(Directory);
			Base.RemoveFromEnd(TEXT("/"));
			Base += TEXT("/");

			if (!NormalizedPath.StartsWith(Base, ESearchCase::IgnoreCase))
			{
				return false;
			}

			OutRelative = NormalizedPath.RightChop(Base.Len());
			return !OutRelative.IsEmpty();
		}

		void ResolveDumpSourceLocation(const FString& SourceFilePath, FString& OutRootName, FString& OutRelativePath)
		{
			const FString Normalized = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);

			if (const UE::DreamShader::FDreamShaderSourceRoot* Root = UE::DreamShader::FindSourceRootForFile(Normalized))
			{
				if (TryMakeRelativeUnder(Normalized, Root->Directory, OutRelativePath))
				{
					OutRootName = Root->DisplayName;
					return;
				}
			}

			// The corpus lives inside the plugin rather than under a DShader root, and a baseline
			// covers it too; naming its root explicitly keeps those files from colliding in the
			// output tree the way bare filenames would.
			if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader")))
			{
				const FString PluginDirectory =
					UE::DreamShader::NormalizeSourceFilePath(FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()));
				if (TryMakeRelativeUnder(Normalized, PluginDirectory, OutRelativePath))
				{
					OutRootName = TEXT("DreamShaderPlugin");
					return;
				}
			}

			OutRootName = TEXT("External");
			OutRelativePath = FPaths::GetCleanFilename(Normalized);
		}

		/** Path-hostile characters become `_`; `/` survives, because it is what makes folders. */
		FString SanitizeDumpPathSegment(const FString& Segment)
		{
			FString Sanitized;
			Sanitized.Reserve(Segment.Len());
			for (const TCHAR Character : Segment)
			{
				const bool bInvalid = Character < 0x20
					|| Character == TEXT('<')
					|| Character == TEXT('>')
					|| Character == TEXT(':')
					|| Character == TEXT('\"')
					|| Character == TEXT('|')
					|| Character == TEXT('?')
					|| Character == TEXT('*')
					|| Character == TEXT('\\');
				Sanitized.AppendChar(bInvalid ? TEXT('_') : Character);
			}
			return Sanitized;
		}
	}

	FScopedDreamShaderGraphDumpWriteGuard::FScopedDreamShaderGraphDumpWriteGuard()
		: bSavedMayWrite(MayWriteGeneratedAssetsToDisk())
	{
		SetMayWriteGeneratedAssetsToDisk(false);
	}

	FScopedDreamShaderGraphDumpWriteGuard::~FScopedDreamShaderGraphDumpWriteGuard()
	{
		SetMayWriteGeneratedAssetsToDisk(bSavedMayWrite);
	}

	FString GetDefaultDreamShaderGraphDumpDirectory()
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader"), TEXT("GraphBaseline")));
	}

	FString MakeDreamShaderGraphDumpFilePath(
		const FString& OutputDirectory,
		const FString& SourceFilePath,
		const FString& ObjectPath)
	{
		FString RootName;
		FString RelativePath;
		ResolveDumpSourceLocation(SourceFilePath, RootName, RelativePath);

		FString AssetLeaf = ObjectPath;
		int32 DotIndex = INDEX_NONE;
		if (AssetLeaf.FindLastChar(TEXT('.'), DotIndex))
		{
			AssetLeaf.RightChopInline(DotIndex + 1, DREAMSHADER_ALLOW_SHRINKING_NO);
		}
		if (AssetLeaf.IsEmpty())
		{
			AssetLeaf = TEXT("Asset");
		}

		const FString FileName = FString::Printf( /* I18N-EXEMPT: machine-readable dump */
			TEXT("%s.%s.graph.json"),
			*SanitizeDumpPathSegment(RelativePath),
			*SanitizeDumpPathSegment(AssetLeaf));

		return FPaths::ConvertRelativePathToFull(
			FPaths::Combine(OutputDirectory, SanitizeDumpPathSegment(RootName), FileName));
	}

	FString BuildDreamShaderGraphDumpJson(UObject* Asset, const FString& SourceFilePath, int32* OutNodeCount)
	{
		if (OutNodeCount)
		{
			*OutNodeCount = 0;
		}

		if (!Asset)
		{
			return FString();
		}

		const FDumpJsonRef Root = FDumpJson::Object();
		Root->Set(TEXT("schema"), FDumpJson::Int(GDreamShaderGraphDumpSchema));

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader"));
		Root->Set(
			TEXT("plugin"),
			FDumpJson::String(Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : TEXT("unknown")));

		FString RootName;
		FString RelativePath;
		ResolveDumpSourceLocation(SourceFilePath, RootName, RelativePath);
		const FDumpJsonRef Source = FDumpJson::Object();
		Source->Set(TEXT("root"), FDumpJson::String(RootName));
		Source->Set(TEXT("path"), FDumpJson::String(RelativePath));
		Root->Set(TEXT("source"), Source);

		Root->Set(TEXT("asset"), FDumpJson::String(Asset->GetPathName()));

		int32 NodeCount = 0;

		// UDreamShaderMaterialInstance is a UMaterialInstance and never a UMaterial, but asking in the
		// other order would be a live trap for whoever adds the next asset class.
		if (UMaterialInstance* Instance = Cast<UMaterialInstance>(Asset))
		{
			Root->Set(TEXT("kind"), FDumpJson::String(TEXT("ThinCustomInstance")));
			Root->Set(TEXT("backend"), FDumpJson::String(TEXT("ThinCustom")));

			const FDumpJsonRef InstanceJson = FDumpJson::Object();
			// The hidden base's OWN path is deliberately absent: it is a transient object in the
			// editor and a subobject of the instance on disk, so the string differs between two
			// captures of the identical graph. Its kind is all the dump needs to say.
			const FDumpJsonRef Parent = FDumpJson::Object();
			Parent->Set(
				TEXT("kind"),
				FDumpJson::String(Instance->Parent ? Instance->Parent->GetClass()->GetName() : TEXT("None")));
			InstanceJson->Set(TEXT("parent"), Parent);
			InstanceJson->Set(TEXT("parameters"), MakeInstanceParametersJson(Instance));
			Root->Set(TEXT("instance"), InstanceJson);

			// The graph itself lives on the hidden base material the instance parents to; a
			// ThinCustom dump that stopped at the instance would describe an empty asset.
			AppendMaterialGraphSections(Cast<UMaterial>(Instance->Parent), Root, NodeCount);
		}
		else if (UMaterial* Material = Cast<UMaterial>(Asset))
		{
			Root->Set(TEXT("kind"), FDumpJson::String(TEXT("Material")));
			Root->Set(TEXT("backend"), FDumpJson::String(TEXT("Graph")));
			AppendMaterialGraphSections(Material, Root, NodeCount);
		}
		else if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Asset))
		{
			const TCHAR* Kind = TEXT("MaterialFunction");
			if (MaterialFunction->IsA<UMaterialFunctionMaterialLayerBlend>())
			{
				Kind = TEXT("MaterialLayerBlend");
			}
			else if (MaterialFunction->IsA<UMaterialFunctionMaterialLayer>())
			{
				Kind = TEXT("MaterialLayer");
			}

			Root->Set(TEXT("kind"), FDumpJson::String(Kind));
			Root->Set(TEXT("backend"), FDumpJson::String(TEXT("Graph")));
			AppendFunctionGraphSections(MaterialFunction, Root, NodeCount);
		}
		else
		{
			return FString();
		}

		if (OutNodeCount)
		{
			*OutNodeCount = NodeCount;
		}

		FString Text;
		Root->Write(Text, 0);
		Text += TEXT("\n");
		return Text;
	}

	bool DumpDreamShaderGraphsForSource(
		const FString& SourceFilePath,
		const FString& OutputDirectory,
		TArray<FDreamShaderGraphDumpEntry>& OutEntries,
		UE::DreamShader::FDreamShaderError& OutError)
	{
		const FString NormalizedSource = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);

		// The asset paths a source compiles to, resolved the same way the generator resolves them --
		// preprocess, parse, apply the plugin-root default, then ResolveDreamShaderAssetDestination.
		// Reading them off the generator's success MESSAGE instead would mean parsing prose, and the
		// message is empty for the assets the write guard refuses.
		FString PreparedSource;
		if (!LoadPreparedDreamShaderSource(NormalizedSource, PreparedSource, OutError))
		{
			return false;
		}

		FTextShaderDefinition Definition;
		FString ParseError;
		if (!FTextShaderParser::Parse(PreparedSource, Definition, ParseError))
		{
			return FailWith(OutError, TEXT("DSH9032"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("%s: %s"),
				*NormalizedSource,
				*ParseError));
		}

		ApplyDefaultRootFromSourceFile(NormalizedSource, Definition);

		struct FDumpTarget
		{
			FString PackageName;
			FString ObjectPath;
			bool bExistedOnDisk = false;
		};

		TArray<FDumpTarget> Targets;
		auto AddTarget = [&Targets, &OutError](const FString& Name, const FString& RootValue) -> bool
		{
			FDumpTarget Target;
			FString AssetLeaf;
			if (!ResolveDreamShaderAssetDestination(Name, RootValue, Target.PackageName, Target.ObjectPath, AssetLeaf, OutError))
			{
				return false;
			}

			// Asked BEFORE generation, because that is when the answer is still about the project on
			// disk rather than about anything this run created. It is also exactly the condition
			// IsGeneratedAssetPersisted asks, so it predicts which assets the write guard will refuse
			// to rebuild.
			Target.bExistedOnDisk = FPackageName::DoesPackageExist(Target.PackageName);
			Targets.Add(MoveTemp(Target));
			return true;
		};

		for (const FTextShaderMaterialFunctionDefinition& FunctionDefinition : Definition.MaterialFunctions)
		{
			if (!AddTarget(FunctionDefinition.Name, FunctionDefinition.Root))
			{
				return false;
			}
		}

		if (!Definition.Name.IsEmpty() && !AddTarget(Definition.Name, Definition.Root))
		{
			return false;
		}

		if (Targets.IsEmpty())
		{
			return FailWith(OutError, TEXT("DSH9032"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("DreamShader source '%s' declares no material, ShaderFunction, ShaderLayer or ShaderLayerBlend block, so there is no graph to dump."),
				*NormalizedSource));
		}

		// Always forced: a dump of a graph that was skipped because its hash matched would be a dump
		// of whatever happened to be in memory. Always transient, and the caller's write guard makes
		// that binding rather than advisory.
		if (!FMaterialGenerator::GenerateAssetsFromFile(NormalizedSource, OutError, /*bForce*/ true, /*bTransient*/ true))
		{
			return false;
		}

		bool bSucceeded = true;
		for (const FDumpTarget& Target : Targets)
		{
			UObject* Asset = LoadObject<UObject>(nullptr, *Target.ObjectPath);
			if (!Asset)
			{
				FailWith(OutError, TEXT("DSH9033"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("DreamShader could not resolve generated asset '%s' from '%s' after generation."),
					*Target.ObjectPath,
					*NormalizedSource));
				bSucceeded = false;
				continue;
			}

			int32 NodeCount = 0;
			const FString Json = BuildDreamShaderGraphDumpJson(Asset, NormalizedSource, &NodeCount);
			if (Json.IsEmpty())
			{
				FailWith(OutError, TEXT("DSH9034"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("DreamShader cannot dump '%s': %s is not a Material, MaterialFunction or DreamShader instance material."),
					*Target.ObjectPath,
					*Asset->GetClass()->GetName()));
				bSucceeded = false;
				continue;
			}

			const FString OutputFilePath = MakeDreamShaderGraphDumpFilePath(OutputDirectory, NormalizedSource, Target.ObjectPath);
			const FString OutputFileDirectory = FPaths::GetPath(OutputFilePath);
			if (!IFileManager::Get().MakeDirectory(*OutputFileDirectory, true))
			{
				FailWith(OutError, TEXT("DSH9030"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("DreamShader failed to create graph dump directory '%s'."),
					*OutputFileDirectory));
				bSucceeded = false;
				continue;
			}

			// UTF-8 without a BOM, and the string already ends in a single LF. SaveStringToFile writes
			// the bytes as given, so nothing here can turn the file into CRLF.
			if (!FFileHelper::SaveStringToFile(Json, *OutputFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				FailWith(OutError, TEXT("DSH9031"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("DreamShader failed to write graph dump '%s'."),
					*OutputFilePath));
				bSucceeded = false;
				continue;
			}

			FDreamShaderGraphDumpEntry Entry;
			Entry.ObjectPath = Target.ObjectPath;
			Entry.OutputFilePath = OutputFilePath;
			Entry.NodeCount = NodeCount;
			Entry.bReadFromDisk = Target.bExistedOnDisk;
			if (Asset->IsA<UDreamShaderMaterialInstance>())
			{
				Entry.Kind = TEXT("ThinCustomInstance");
			}
			else if (Asset->IsA<UMaterialFunctionMaterialLayerBlend>())
			{
				Entry.Kind = TEXT("MaterialLayerBlend");
			}
			else if (Asset->IsA<UMaterialFunctionMaterialLayer>())
			{
				Entry.Kind = TEXT("MaterialLayer");
			}
			else if (Asset->IsA<UMaterialFunction>())
			{
				Entry.Kind = TEXT("MaterialFunction");
			}
			else
			{
				Entry.Kind = TEXT("Material");
			}
			OutEntries.Add(MoveTemp(Entry));
		}

		return bSucceeded;
	}
}
