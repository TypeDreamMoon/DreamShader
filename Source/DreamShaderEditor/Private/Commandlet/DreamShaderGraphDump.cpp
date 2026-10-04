// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderGraphDump.h for what this is for. This file is the policy: what a canonical dump
// contains, and in what order.

#include "DreamShaderGraphDump.h"

#include "DreamShaderMaterialInstance.h"
#include "DreamShaderModule.h"
#include "DreamShaderTypes.h"
#include "DreamShaderVersionCompat.h"

// GetMaterialInputForDecompile / MakeInputMaskSuffix / the Domain-Blend-ShadingModel formatters /
// the function parameter type names. Reused rather than rewritten: a second spelling of "what type
// is this function input" would be a second thing to keep in step with the language.
#include "Decompiler/DreamShaderGraphDecompilerHelpers.h"

// Product resolution and the pipeline, which DumpDreamShaderGraphsForSource runs for every source kind.
#include "DreamShaderCompilerDiagnostics.h"
#include "DreamShaderCompilePipeline.h"
#include "Tools/DreamShaderCompilerTools.h"

// IsDigestProperty -- the project's existing answer to "which reflected property is CONTENT rather
// than presentation". It already excludes node coordinates, node colour, Desc, the comment-bubble
// and collapsed flags, every FGuid (MaterialExpressionGuid and the named-reroute variable id), every
// FExpressionInput (connections are recorded structurally below), and everything transient or
// non-editable (GraphNode, and the Material / Function back-pointers). Sharing it is deliberate: a
// dump that disagreed with the divergence digest about what counts as content would be two answers
// to one question.
#include "DreamShaderGeneratedAssetDigest.h"
#include "DreamShaderInstanceSettings.h"
#include "DreamShaderCompilerService.h"
#include "DreamShaderMaterialExpressionCompat.h"
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderCompilePipeline.h"

// A `.dsp` product: the pipeline asset, and the `.dsp` spelling of its enumerations.
#include "DreamPassPipeline.h"
#include "DreamPassTypes.h"
#include "Pass/DreamPassSpellings.h"

#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionGetMaterialAttributes.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
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

			// The attribute lists of Get/SetMaterialAttributes are arrays of FGuid, which the loop above leaves out with
			// every other guid -- and they are the one place a guid is behaviour: which attribute output 1 of a Get is,
			// which one input 2 of a Set drives. Written by the attribute's own name, which no culture translates.
			const auto MakeAttributeNamesJson = [](const TArray<FGuid>& AttributeIds)
			{
				const FDumpJsonRef Names = FDumpJson::Array();
				for (const FGuid& AttributeId : AttributeIds)
				{
					Names->Add(FDumpJson::String(FMaterialAttributeDefinitionMap::GetAttributeName(AttributeId)));
				}
				return Names;
			};
			if (const UMaterialExpressionSetMaterialAttributes* Set = Cast<UMaterialExpressionSetMaterialAttributes>(Expression))
			{
				Props->Set(TEXT("AttributeSetTypes"), MakeAttributeNamesJson(Set->AttributeSetTypes));
			}
			else if (const UMaterialExpressionGetMaterialAttributes* Get = Cast<UMaterialExpressionGetMaterialAttributes>(Expression))
			{
				Props->Set(TEXT("AttributeGetTypes"), MakeAttributeNamesJson(Get->AttributeGetTypes));
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
					// The stable name: three engine nodes name their inputs with translated text (GetDreamShaderStableInputName).
					const FName InputName = GetDreamShaderStableInputName(Expression, InputIndex);
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
		// Pass pipelines
		//
		// A `.dsp` product has no graph. Its dump is the pipeline as the runtime reads it: every
		// enumeration spelled the way the `.dsp` spells it (Pass/DreamPassSpellings.h), arrays in
		// declaration order -- for passes that is execution order inside an injection point -- and
		// only the settings of each pass's own kind, so a value the runtime never reads cannot make
		// two captures differ. The slots are listed again on their own: they are what a rebuild of
		// another pipeline can move.
		// -----------------------------------------------------------------------------------------

		FDumpJsonRef MakeNameJson(const FName Name)
		{
			return Name.IsNone() ? FDumpJson::Null() : FDumpJson::String(Name.ToString());
		}

		FDumpJsonRef MakeFloatsJson(std::initializer_list<double> Values)
		{
			const FDumpJsonRef Array = FDumpJson::Array();
			for (const double Value : Values)
			{
				Array->Add(FDumpJson::Double(Value));
			}
			return Array;
		}

		FDumpJsonRef MakeStringsJson(const TArray<FString>& Strings)
		{
			const FDumpJsonRef Array = FDumpJson::Array();
			for (const FString& String : Strings)
			{
				Array->Add(FDumpJson::String(String));
			}
			return Array;
		}

		FDumpJsonRef MakePassValueJson(const FDreamPassParameterValue& Value)
		{
			namespace PassSpelling = UE::DreamShader::Editor::Private::PassSpelling;

			const FDumpJsonRef Object = FDumpJson::Object();
			Object->Set(TEXT("type"), FDumpJson::String(PassSpelling::ParameterType(Value.Type)));
			switch (Value.Type)
			{
			case EDreamPassParameterType::Int:
				Object->Set(TEXT("value"), FDumpJson::Int(Value.Int));
				break;
			case EDreamPassParameterType::Bool:
				Object->Set(TEXT("value"), FDumpJson::Bool(Value.Bool));
				break;
			case EDreamPassParameterType::Texture:
				Object->Set(TEXT("value"), Value.Texture ? FDumpJson::String(Value.Texture->GetPathName()) : FDumpJson::Null());
				break;
			default:
			{
				const FDumpJsonRef Channels = FDumpJson::Array();
				for (int32 Channel = 0; Channel < PassSpelling::ParameterWidth(Value.Type); ++Channel)
				{
					Channels->Add(FDumpJson::Double(Value.Vector[Channel]));
				}
				Object->Set(TEXT("value"), Channels);
				break;
			}
			}
			return Object;
		}

		FDumpJsonRef MakePassBindingsJson(const TArray<FDreamPassBufferBinding>& Bindings)
		{
			const FDumpJsonRef Array = FDumpJson::Array();
			for (const FDreamPassBufferBinding& Binding : Bindings)
			{
				const FDumpJsonRef Object = FDumpJson::Object();
				Object->Set(TEXT("slot"), MakeNameJson(Binding.Slot));
				Object->Set(TEXT("buffer"), MakeNameJson(Binding.Buffer));
				Object->Set(TEXT("previous"), FDumpJson::Bool(Binding.bPrevious));
				Array->Add(Object);
			}
			return Array;
		}

		FDumpJsonRef MakePassParamsJson(const TArray<FDreamPassParamBinding>& Params)
		{
			namespace PassSpelling = UE::DreamShader::Editor::Private::PassSpelling;

			const FDumpJsonRef Array = FDumpJson::Array();
			for (const FDreamPassParamBinding& Param : Params)
			{
				const FDumpJsonRef Object = FDumpJson::Object();
				Object->Set(TEXT("target"), MakeNameJson(Param.Target));
				Object->Set(TEXT("source"), FDumpJson::String(PassSpelling::ParamSource(Param.Source)));
				if (Param.Source == EDreamPassParamSource::Constant)
				{
					Object->Set(TEXT("constant"), MakePassValueJson(Param.Constant));
				}
				else
				{
					if (Param.Source == EDreamPassParamSource::Parameter)
					{
						Object->Set(TEXT("parameter"), MakeNameJson(Param.Parameter));
					}
					Object->Set(TEXT("multiplier"), FDumpJson::Double(Param.Multiplier));
					Object->Set(TEXT("offset"), FDumpJson::Double(Param.Offset));
				}
				Array->Add(Object);
			}
			return Array;
		}

		FDumpJsonRef MakePassFilterJson(const FDreamPassMeshFilter& Filter)
		{
			namespace PassSpelling = UE::DreamShader::Editor::Private::PassSpelling;

			// Any clause, each all of its terms: the disjunctive normal form the runtime evaluates.
			const FDumpJsonRef Clauses = FDumpJson::Array();
			for (const FDreamPassFilterClause& Clause : Filter.AnyOf)
			{
				const FDumpJsonRef Terms = FDumpJson::Array();
				for (const FDreamPassFilterTerm& Term : Clause.AllOf)
				{
					const FDumpJsonRef Object = FDumpJson::Object();
					Object->Set(TEXT("kind"), FDumpJson::String(PassSpelling::FilterKind(Term.Kind)));
					switch (Term.Kind)
					{
					case EDreamPassFilterKind::Stencil:
						Object->Set(TEXT("value"), FDumpJson::Int(Term.StencilValue));
						Object->Set(TEXT("mask"), FDumpJson::Int(Term.StencilMask));
						break;
					case EDreamPassFilterKind::Layer:
					{
						TArray<FString> Names;
						for (const FName Layer : Term.LayerNames)
						{
							Names.Add(Layer.ToString());
						}
						Object->Set(TEXT("layers"), MakeStringsJson(Names));
						Object->Set(TEXT("layerMask"), FDumpJson::UInt(static_cast<uint32>(Term.LayerMask)));
						break;
					}
					case EDreamPassFilterKind::List:
						Object->Set(TEXT("list"), MakeNameJson(Term.ListName));
						break;
					}
					Terms->Add(Object);
				}
				Clauses->Add(Terms);
			}
			return Clauses;
		}

		/** Where an HLSL pass's code is (EDreamPassHlslSource): File, Block, Body, Shared. */
		const TCHAR* HlslSourceText(const EDreamPassHlslSource Source)
		{
			switch (Source)
			{
			case EDreamPassHlslSource::Block:  return TEXT("Block");
			case EDreamPassHlslSource::Body:   return TEXT("Body");
			case EDreamPassHlslSource::Shared: return TEXT("Shared");
			case EDreamPassHlslSource::File:
			default:                           return TEXT("File");
			}
		}

		FDumpJsonRef MakePassJson(const FDreamPassDesc& Pass)
		{
			namespace PassSpelling = UE::DreamShader::Editor::Private::PassSpelling;

			const FDumpJsonRef Object = FDumpJson::Object();
			Object->Set(TEXT("name"), MakeNameJson(Pass.Name));
			Object->Set(TEXT("kind"), FDumpJson::String(PassSpelling::Kind(Pass.Kind)));
			Object->Set(TEXT("injection"), FDumpJson::String(UE::DreamPass::LexToString(Pass.Injection)));
			Object->Set(TEXT("enabled"), MakeNameJson(Pass.EnabledParameter));
			Object->Set(TEXT("reads"), MakePassBindingsJson(Pass.Reads));
			Object->Set(TEXT("writes"), MakePassBindingsJson(Pass.Writes));
			Object->Set(TEXT("params"), MakePassParamsJson(Pass.Params));
			Object->Set(TEXT("description"), FDumpJson::String(Pass.Description));

			const FDumpJsonRef Settings = FDumpJson::Object();
			switch (Pass.Kind)
			{
			case EDreamPassKind::Fullscreen:
			{
				const FDreamPassFullscreenSettings& Fullscreen = Pass.Fullscreen;
				Settings->Set(TEXT("material"), Fullscreen.Material ? FDumpJson::String(Fullscreen.Material->GetPathName()) : FDumpJson::Null());
				Settings->Set(TEXT("shader"), FDumpJson::String(Fullscreen.ShaderPath));
				Settings->Set(TEXT("entry"), FDumpJson::String(Fullscreen.Entry));
				Settings->Set(TEXT("hlslSource"), FDumpJson::String(HlslSourceText(Fullscreen.HlslSource)));
				Settings->Set(TEXT("inlineLine"), FDumpJson::Int(Fullscreen.InlineHlslLine));
				Settings->Set(TEXT("pixelSlot"), FDumpJson::Int(Fullscreen.PixelSlot));
				break;
			}
			case EDreamPassKind::Compute:
			{
				const FDreamPassComputeSettings& Compute = Pass.Compute;
				Settings->Set(TEXT("shader"), FDumpJson::String(Compute.ShaderPath));
				Settings->Set(TEXT("entry"), FDumpJson::String(Compute.Entry));
				Settings->Set(TEXT("hlslSource"), FDumpJson::String(HlslSourceText(Compute.HlslSource)));
				Settings->Set(TEXT("inlineLine"), FDumpJson::Int(Compute.InlineHlslLine));
				Settings->Set(TEXT("slot"), FDumpJson::Int(Compute.Slot));
				const FDumpJsonRef Threads = FDumpJson::Array();
				Threads->Add(FDumpJson::Int(Compute.ThreadGroupSize.X));
				Threads->Add(FDumpJson::Int(Compute.ThreadGroupSize.Y));
				Threads->Add(FDumpJson::Int(Compute.ThreadGroupSize.Z));
				Settings->Set(TEXT("threads"), Threads);
				Settings->Set(TEXT("dispatchMode"), FDumpJson::String(PassSpelling::DispatchMode(Compute.DispatchMode)));
				if (Compute.DispatchMode == EDreamPassDispatchMode::Buffer)
				{
					Settings->Set(TEXT("dispatchBuffer"), MakeNameJson(Compute.DispatchBuffer));
					Settings->Set(TEXT("dispatchScale"), FDumpJson::Double(Compute.DispatchScale));
				}
				else
				{
					const FDumpJsonRef Size = FDumpJson::Array();
					Size->Add(FDumpJson::Int(Compute.DispatchSize.X));
					Size->Add(FDumpJson::Int(Compute.DispatchSize.Y));
					Size->Add(FDumpJson::Int(Compute.DispatchSize.Z));
					Settings->Set(TEXT("dispatchSize"), Size);
				}
				break;
			}
			case EDreamPassKind::Mesh:
			{
				const FDreamPassMeshSettings& Mesh = Pass.Mesh;
				Settings->Set(TEXT("filter"), MakePassFilterJson(Mesh.Filter));
				Settings->Set(TEXT("material"), Mesh.OverrideMaterial ? FDumpJson::String(Mesh.OverrideMaterial->GetPathName()) : FDumpJson::Null());
				Settings->Set(TEXT("mode"), FDumpJson::String(PassSpelling::MeshMode(Mesh.Mode)));
				Settings->Set(TEXT("depth"), FDumpJson::String(PassSpelling::Depth(Mesh.Depth)));
				if (Mesh.Depth == EDreamPassDepthMode::Own)
				{
					Settings->Set(TEXT("depthBuffer"), MakeNameJson(Mesh.DepthBuffer));
				}
				Settings->Set(TEXT("cull"), FDumpJson::String(PassSpelling::Cull(Mesh.Cull)));
				Settings->Set(TEXT("blend"), FDumpJson::String(PassSpelling::Blend(Mesh.Blend)));
				Settings->Set(TEXT("usage"), MakeStringsJson(PassSpelling::FlagNames(PassSpelling::MeshUsageFlags(), Mesh.Usage)));
				Settings->Set(TEXT("nanite"), FDumpJson::String(PassSpelling::Nanite(Mesh.Nanite)));
				if (Mesh.Nanite != EDreamPassNanitePolicy::Skip)
				{
					Settings->Set(TEXT("naniteValue"), MakeFloatsJson({ Mesh.NaniteValue.R, Mesh.NaniteValue.G, Mesh.NaniteValue.B, Mesh.NaniteValue.A }));
				}
				if (Mesh.Nanite == EDreamPassNanitePolicy::AssignStencil)
				{
					Settings->Set(TEXT("assignedStencilValue"), FDumpJson::Int(Mesh.AssignedStencilValue));
				}
				break;
			}
			case EDreamPassKind::Clear:
				Settings->Set(TEXT("value"), MakeFloatsJson({ Pass.Clear.Value.R, Pass.Clear.Value.G, Pass.Clear.Value.B, Pass.Clear.Value.A }));
				break;
			case EDreamPassKind::Copy:
				break;
			}
			Object->Set(TEXT("settings"), Settings);
			return Object;
		}

		void AppendPassPipelineSections(const UDreamPassPipeline& Pipeline, const FDumpJsonRef& Root, int32& OutPassCount)
		{
			namespace PassSpelling = UE::DreamShader::Editor::Private::PassSpelling;

			const FDumpJsonRef Header = FDumpJson::Object();
			Header->Set(TEXT("order"), FDumpJson::Int(Pipeline.Order));
			Header->Set(TEXT("defaultInjection"), FDumpJson::String(UE::DreamPass::LexToString(Pipeline.DefaultInjection)));
			Header->Set(TEXT("views"), MakeStringsJson(PassSpelling::FlagNames(PassSpelling::ViewFlags(), Pipeline.Views)));
			Header->Set(TEXT("requires"), MakeStringsJson(PassSpelling::FlagNames(PassSpelling::RequirementFlags(), Pipeline.Requires)));
			Header->Set(TEXT("enabled"), MakeNameJson(Pipeline.EnabledParameter));
			Root->Set(TEXT("pipeline"), Header);

			const FDumpJsonRef Parameters = FDumpJson::Array();
			for (const FDreamPassParameterDesc& Parameter : Pipeline.Parameters)
			{
				const FDumpJsonRef Object = FDumpJson::Object();
				Object->Set(TEXT("name"), MakeNameJson(Parameter.Name));
				Object->Set(TEXT("default"), MakePassValueJson(Parameter.Default));
				Object->Set(TEXT("group"), FDumpJson::String(Parameter.Group));
				Object->Set(TEXT("description"), FDumpJson::String(Parameter.Description));
				Object->Set(TEXT("slider"), Parameter.bHasSlider ? MakeFloatsJson({ Parameter.SliderMin, Parameter.SliderMax }) : FDumpJson::Null());
				Object->Set(TEXT("sortPriority"), FDumpJson::Int(Parameter.SortPriority));
				Parameters->Add(Object);
			}
			Root->Set(TEXT("parameters"), Parameters);

			const FDumpJsonRef Buffers = FDumpJson::Array();
			for (const FDreamPassBufferDesc& Buffer : Pipeline.Buffers)
			{
				const FDumpJsonRef Object = FDumpJson::Object();
				Object->Set(TEXT("name"), MakeNameJson(Buffer.Name));
				Object->Set(TEXT("format"), FDumpJson::String(UE::DreamPass::LexToString(Buffer.Format)));
				Object->Set(TEXT("resolution"), FDumpJson::String(PassSpelling::Resolution(Buffer.Resolution)));
				if (Buffer.Resolution == EDreamPassBufferResolution::Fixed)
				{
					const FDumpJsonRef Size = FDumpJson::Array();
					Size->Add(FDumpJson::Int(Buffer.FixedSize.X));
					Size->Add(FDumpJson::Int(Buffer.FixedSize.Y));
					Object->Set(TEXT("size"), Size);
				}
				else
				{
					Object->Set(TEXT("scale"), FDumpJson::Double(Buffer.Scale));
				}
				Object->Set(TEXT("clear"), Buffer.bClear
					? MakeFloatsJson({ Buffer.ClearValue.R, Buffer.ClearValue.G, Buffer.ClearValue.B, Buffer.ClearValue.A })
					: FDumpJson::Null());
				Object->Set(TEXT("mips"), FDumpJson::Int(Buffer.Mips));
				Object->Set(TEXT("history"), FDumpJson::Bool(Buffer.bHistory));
				Object->Set(TEXT("export"), FDumpJson::Bool(Buffer.bExport));
				const UTextureRenderTarget2D* const Target = Pipeline.GetExportTarget(Buffer.Name);
				Object->Set(TEXT("exportTarget"), Target ? FDumpJson::String(Target->GetPathName()) : FDumpJson::Null());
				Object->Set(TEXT("description"), FDumpJson::String(Buffer.Description));
				Buffers->Add(Object);
			}
			Root->Set(TEXT("buffers"), Buffers);
			// The file's `hlsl` block: whether there is one, and the line of its `{` in the `.dsp`.
			Root->Set(TEXT("sharedHlsl"), FDumpJson::Bool(Pipeline.bHasSharedHlsl));
			Root->Set(TEXT("sharedHlslLine"), FDumpJson::Int(Pipeline.SharedHlslLine));

			const FDumpJsonRef Passes = FDumpJson::Array();
			const FDumpJsonRef Slots = FDumpJson::Array();
			for (const FDreamPassDesc& Pass : Pipeline.Passes)
			{
				Passes->Add(MakePassJson(Pass));

				const bool bComputeSlot = Pass.Kind == EDreamPassKind::Compute;
				const bool bPixelSlot = Pass.Kind == EDreamPassKind::Fullscreen && Pass.Fullscreen.RunsHlsl();
				if (bComputeSlot || bPixelSlot)
				{
					const FDumpJsonRef Slot = FDumpJson::Object();
					Slot->Set(TEXT("pass"), MakeNameJson(Pass.Name));
					Slot->Set(TEXT("kind"), FDumpJson::String(bComputeSlot ? TEXT("compute") : TEXT("pixel")));
					Slot->Set(TEXT("slot"), FDumpJson::Int(bComputeSlot ? Pass.Compute.Slot : Pass.Fullscreen.PixelSlot));
					Slots->Add(Slot);
				}
			}
			Root->Set(TEXT("passes"), Passes);
			Root->Set(TEXT("slots"), Slots);

			OutPassCount = Passes->Num();
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

		// Instances first: UDreamShaderMaterialInstance and a `.dsi` product are UMaterialInstances and never
		// UMaterials, but asking in the other order would be a live trap for whoever adds the next asset class.
		if (UDreamShaderMaterialInstance* const ThinCustomInstance = Cast<UDreamShaderMaterialInstance>(Asset))
		{
			UMaterialInstance* const Instance = ThinCustomInstance;
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
		else if (UMaterialInstance* const MaterialInstance = Cast<UMaterialInstance>(Asset))
		{
			// A `.dsi` product: an instance of a parent that is an asset in its own right. The dump is the instance --
			// its parent and the values it overrides -- and never the parent's graph, which the parent's own dump
			// describes; appending it would change every instance baseline whenever its parent changes.
			Root->Set(TEXT("kind"), FDumpJson::String(TEXT("MaterialInstance")));

			const FDumpJsonRef InstanceJson = FDumpJson::Object();
			const FDumpJsonRef Parent = FDumpJson::Object();
			Parent->Set(
				TEXT("kind"),
				FDumpJson::String(MaterialInstance->Parent ? MaterialInstance->Parent->GetClass()->GetName() : TEXT("None")));
			// Unlike a ThinCustom base, this parent is addressable, so its path is the same in every capture.
			Parent->Set(
				TEXT("path"),
				FDumpJson::String(MaterialInstance->Parent ? MaterialInstance->Parent->GetPathName() : FString()));
			InstanceJson->Set(TEXT("parent"), Parent);
			InstanceJson->Set(TEXT("parameters"), MakeInstanceParametersJson(MaterialInstance));

			// The `#pragma instance` keys the asset holds, read back the way the decompiler reads them: the base property
			// overrides whose flag is set and the table rows, so a BlendMode that was written without its flag is absent.
			const FDumpJsonRef Settings = FDumpJson::Object();
			if (const UMaterialInstanceConstant* const Constant = Cast<UMaterialInstanceConstant>(MaterialInstance))
			{
				TArray<TPair<FString, FString>> Keys;
				TArray<FString> Unsupported;
				UE::DreamShader::Editor::Compiler::ReadInstanceSettings(Constant, Keys, Unsupported);
				for (const TPair<FString, FString>& Key : Keys)
				{
					Settings->Set(Key.Key, FDumpJson::String(Key.Value));
				}
			}
			InstanceJson->Set(TEXT("settings"), Settings);
			Root->Set(TEXT("instance"), InstanceJson);
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
		else if (const UDreamPassPipeline* const Pipeline = Cast<UDreamPassPipeline>(Asset))
		{
			// A `.dsp` product: no graph and so no backend. The pipeline is the dump, and its passes stand for the nodes.
			Root->Set(TEXT("kind"), FDumpJson::String(TEXT("PassPipeline")));
			AppendPassPipelineSections(*Pipeline, Root, NodeCount);
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

	namespace
	{
		/** The `Kind` string of one dumped asset: the `kind` BuildDreamShaderGraphDumpJson writes for it. */
		FString ClassifyDumpedAsset(UObject* Asset)
		{
			if (Asset->IsA<UDreamPassPipeline>())
			{
				return TEXT("PassPipeline");
			}
			if (Asset->IsA<UDreamShaderMaterialInstance>())
			{
				return TEXT("ThinCustomInstance");
			}
			if (Asset->IsA<UMaterialInstance>())
			{
				return TEXT("MaterialInstance");
			}
			if (Asset->IsA<UMaterialFunctionMaterialLayerBlend>())
			{
				return TEXT("MaterialLayerBlend");
			}
			if (Asset->IsA<UMaterialFunctionMaterialLayer>())
			{
				return TEXT("MaterialLayer");
			}
			if (Asset->IsA<UMaterialFunction>())
			{
				return TEXT("MaterialFunction");
			}
			return TEXT("Material");
		}
	}

	bool DumpDreamShaderGraphsForSource(
		const FString& SourceFilePath,
		const FString& OutputDirectory,
		TArray<FDreamShaderGraphDumpEntry>& OutEntries,
		UE::DreamShader::FDreamShaderError& OutError)
	{
		const FString NormalizedSource = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);

		// The assets a source compiles to, resolved by the compile's own front half -- the front end its extension
		// picks, the binder, the IR builder and the destination rules -- for a `.dss`, `.dsi`, `.dsp`, `.dsm` and `.dsf` alike.
		// Reading them off the compile's success MESSAGE instead would mean parsing prose, and the message is empty for
		// the assets the write guard refuses.
		UE::DreamShader::Editor::Compiler::FDreamShaderProductResolution Resolution;
		if (!UE::DreamShader::Editor::Compiler::ResolveDreamShaderSourceProducts(NormalizedSource, Resolution))
		{
			UE::DreamShader::Editor::Compiler::BuildLang2CompileError(Resolution.Diagnostics, NormalizedSource, OutError);
			if (OutError.IsEmpty())
			{
				FailWith(OutError, TEXT("DSH9032"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("DreamShader could not work out which assets '%s' builds, so there is no graph to dump."),
					*NormalizedSource));
			}
			return false;
		}

		if (Resolution.Products.IsEmpty())
		{
			return FailWith(OutError, TEXT("DSH9032"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("DreamShader source '%s' declares no material, material instance or exported function, so there is no graph to dump."),
				*NormalizedSource));
		}

		struct FDumpTarget
		{
			int32 ProductIndex = INDEX_NONE;
			FString ObjectPath;
			bool bExistedOnDisk = false;
		};

		TArray<FDumpTarget> Targets;
		Targets.Reserve(Resolution.Products.Num());
		for (const UE::DreamShader::Editor::Compiler::FDreamShaderResolvedProduct& Product : Resolution.Products)
		{
			FDumpTarget& Target = Targets.AddDefaulted_GetRef();
			Target.ProductIndex = Product.ProductIndex;
			Target.ObjectPath = Product.ObjectPath;
			// Asked BEFORE the compile, because that is when the answer is still about the project on disk rather
			// than about anything this run created. It is also exactly the condition the write guard asks, so it
			// predicts which assets the compile leaves as they stand.
			Target.bExistedOnDisk = FPackageName::DoesPackageExist(Product.PackageName);
		}

		UE::DreamShader::Editor::Compiler::FDreamShaderLang2PipelineOptions Options;
		// Always forced: a dump of a graph that was skipped because its hash matched would be a dump of whatever
		// happened to be in memory.
		Options.bForce = true;
		Options.bEmitAssets = true;
		// A ThinCustom product stays Ephemeral, and the caller's write guard makes that binding rather than advisory.
		Options.ThinCustomPersistence = ::UE::DreamShader::EThinCustomPersistence::Ephemeral;

		UE::DreamShader::Editor::Compiler::FDreamShaderLang2PipelineResult Result;
		if (!UE::DreamShader::Editor::Compiler::RunDreamShaderLang2Pipeline(NormalizedSource, Options, Result))
		{
			UE::DreamShader::Editor::Compiler::BuildLang2CompileError(Result.Diagnostics, NormalizedSource, OutError);
			if (OutError.IsEmpty())
			{
				FailWith(OutError, TEXT("DSH9032"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("DreamShader could not compile '%s', so there is no graph to dump."),
					*NormalizedSource));
			}
			return false;
		}

		bool bSucceeded = true;
		for (const FDumpTarget& Target : Targets)
		{
			// The asset the run handed back for this product or, for one the write guard left alone, whatever the
			// object path holds on disk.
			UObject* Asset = nullptr;
			const int32 Slot = Result.ProductOrder.IndexOfByKey(Target.ProductIndex);
			if (Result.ProductAssets.IsValidIndex(Slot))
			{
				Asset = Result.ProductAssets[Slot].Get();
			}
			if (!Asset)
			{
				Asset = LoadObject<UObject>(nullptr, *Target.ObjectPath);
			}
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
					TEXT("DreamShader cannot dump '%s': %s is not a Material, MaterialFunction, material instance or pass pipeline."),
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
			Entry.Kind = ClassifyDumpedAsset(Asset);
			OutEntries.Add(MoveTemp(Entry));
		}

		return bSucceeded;
	}
}
