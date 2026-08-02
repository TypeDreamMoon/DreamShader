# DreamShaderTypes.h

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderTypes.h**

The parsed-source data model: every struct and enum that [`FTextShaderParser::Parse`](parser.md)
fills and the generator consumes.

Defined in header `DreamShaderTypes.h`.

| | |
| :-- | :-- |
| Module | `DreamShader` (Runtime) |
| Include | `#include "DreamShaderTypes.h"` |
| Namespace | `UE::DreamShader` |
| Contents | 13 structs, 5 enums, 1 inline function, 1 free function, 2 member functions |
| Reflection | none — no `USTRUCT`, no `UENUM`, no `UPROPERTY` |
| Pulls in | `CoreMinimal.h` only |

> [!NOTE]
> **None of the structs carries `DREAMSHADER_API`.** They are header-only aggregates with implicit
> special member functions, so no exported symbol is needed to construct, copy or destroy them
> across a module boundary. Only three things in this header are exported:
> `FTextShaderDefinition::TryGetSetting`, `FTextShaderDefinition::GetSetting` and
> `NormalizeSettingKey`.

## Declaration index

```cpp
namespace UE::DreamShader
{
    enum class ETextShaderPropertyType : uint8;
    enum class ETextShaderTextureType : uint8;
    enum class ETextShaderPropertySource : uint8;
    enum class ETextShaderMaterialFunctionKind : uint8;

    struct FTextShaderMetadata;
    struct FTextShaderPropertyDefinition;
    struct FTextShaderOutputBinding;             // nests enum class ETargetKind : uint8
    struct FTextShaderVariableDeclaration;
    struct FTextShaderGraphRegion;
    struct FTextShaderLayoutNode;
    struct FTextShaderLayoutComment;
    struct FTextShaderLayout;
    struct FTextShaderFunctionParameter;
    struct FTextShaderFunctionDefinition;
    struct FTextShaderMaterialFunctionDefinition;
    struct FTextShaderVirtualFunctionDefinition;
    struct FTextShaderDefinition;

    inline const TCHAR* LexToString(ETextShaderMaterialFunctionKind Kind);

    DREAMSHADER_API FString NormalizeSettingKey(const FString& InKey);
}
```

## Enumerations

### `ETextShaderPropertyType`

```cpp
enum class ETextShaderPropertyType : uint8
{
    Scalar,
    Vector,
    Texture2D,
};
```

| Enumerator | Value | Meaning |
| :-- | :-- | :-- |
| `Scalar` | `0` | Scalar-valued property — the `float` / `int` / `bool` families |
| `Vector` | `1` | Vector-valued property — the `float2` / `float3` / `float4` family |
| `Texture2D` | `2` | Texture-valued property of **any** dimension; the dimension is carried separately in `ETextShaderTextureType` |

The `Texture2D` enumerator name is a historical spelling. A `TextureCube` or `VolumeTexture` property
also has `Type == Texture2D`.

### `ETextShaderTextureType`

```cpp
enum class ETextShaderTextureType : uint8
{
    Texture2D,
    TextureCube,
    Texture2DArray,
    VolumeTexture,
};
```

| Enumerator | Value |
| :-- | :-- |
| `Texture2D` | `0` |
| `TextureCube` | `1` |
| `Texture2DArray` | `2` |
| `VolumeTexture` | `3` |

### `ETextShaderPropertySource`

```cpp
enum class ETextShaderPropertySource : uint8
{
    Parameter,
    UEBuiltin,
};
```

| Enumerator | Value | Meaning |
| :-- | :-- | :-- |
| `Parameter` | `0` | The property becomes a material parameter node |
| `UEBuiltin` | `1` | The property is a `UE.*` builtin call bound to a name |

### `FTextShaderOutputBinding::ETargetKind`

```cpp
enum class ETargetKind : uint8
{
    MaterialProperty,
    ExpressionInput,
};
```

| Enumerator | Value | Meaning |
| :-- | :-- | :-- |
| `MaterialProperty` | `0` | The binding target is a material property, e.g. `Base.BaseColor` |
| `ExpressionInput` | `1` | The binding target is an input pin on a named expression, e.g. `Expression(Class="…").Pin[0]` |

### `ETextShaderMaterialFunctionKind`

```cpp
enum class ETextShaderMaterialFunctionKind : uint8
{
    ShaderFunction,
    MaterialLayer,
    MaterialLayerBlend,
};

inline const TCHAR* LexToString(const ETextShaderMaterialFunctionKind Kind);
```

| Enumerator | Value | `LexToString` result | Generates |
| :-- | :-- | :-- | :-- |
| `ShaderFunction` | `0` | `"ShaderFunction"` | `UMaterialFunction` |
| `MaterialLayer` | `1` | **`"ShaderLayer"`** | `UMaterialFunctionMaterialLayer` |
| `MaterialLayerBlend` | `2` | **`"ShaderLayerBlend"`** | `UMaterialFunctionMaterialLayerBlend` |

> [!NOTE]
> The enumerator names still use the pre-1.3.0 `MaterialLayer*` spelling, but `LexToString` prints
> the current `ShaderLayer*` spelling. Generated messages therefore say `ShaderLayer`, never
> `MaterialLayer`, even for a source file that used the deprecated keyword. The `default:` label
> shares the `ShaderFunction` case, so an out-of-range value also prints `"ShaderFunction"`.

## Structures

### `FTextShaderMetadata`

Declaration metadata attached to a property or a function parameter.

```cpp
struct FTextShaderMetadata
{
    FString Group;
    bool bHasSortPriority = false;
    int32 SortPriority = 32;
    FString Description;
    TMap<FString, FString> ReflectedProperties;
};
```

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `Group` | `FString` | `""` | Parameter group. Inherited from an enclosing `Properties Group("…")` scope when the declaration does not set it explicitly. |
| `bHasSortPriority` | `bool` | `false` | Whether `SortPriority` is meaningful. Set to `true` by an explicit `SortPriority` / `Sort` key, and also by the auto-numbering applied to declarations inside a `Group("…")` scope. A top-level (ungrouped) declaration that omits the key keeps `false`. |
| `SortPriority` | `int32` | **`32`** | Sort order within the group. |
| `Description` | `FString` | `""` | Tooltip / description text. |
| `ReflectedProperties` | `TMap<FString, FString>` | empty | Arbitrary reflected `UPROPERTY` name → literal-text pairs from the `[ … ]` block. **Keys are normalized with `NormalizeSettingKey`**, i.e. trimmed and lower-cased. |

Keys the parser consumes itself, rather than putting them in `ReflectedProperties`: `Group` /
`Category`, `Description`, `SortPriority` / `Sort`, and the `SliderMin` / `SliderMax` pair produced
by `Slider(min, max)`. See [Metadata block](../parameters/metadata.md).

### `FTextShaderPropertyDefinition`

One entry of a `Properties` section.

```cpp
struct FTextShaderPropertyDefinition
{
    FString Name;
    ETextShaderPropertySource Source = ETextShaderPropertySource::Parameter;
    FString ParameterNodeType;
    FString UEBuiltinFunctionName;
    TMap<FString, FString> UEBuiltinArguments;
    ETextShaderPropertyType Type = ETextShaderPropertyType::Scalar;
    ETextShaderTextureType TextureType = ETextShaderTextureType::Texture2D;
    bool bHasExplicitTextureType = false;
    int32 ComponentCount = 1;
    bool bConst = false;
    bool bHasDefaultValue = false;
    double ScalarDefaultValue = 0.0;
    FLinearColor VectorDefaultValue = FLinearColor::White;
    FString TextureDefaultObjectPath;
    FTextShaderMetadata Metadata;
};
```

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `Name` | `FString` | `""` | Property / parameter name as written. |
| `Source` | `ETextShaderPropertySource` | `Parameter` | Parameter node vs. `UE.*` builtin. |
| `ParameterNodeType` | `FString` | `""` | The explicit Unreal expression-class token when the declaration names one, e.g. `ScalarParameter`, `TextureSampleParameter2D`. Empty for a compact type token. |
| `UEBuiltinFunctionName` | `FString` | `""` | Builtin name when `Source == UEBuiltin`. |
| `UEBuiltinArguments` | `TMap<FString, FString>` | empty | Named arguments for that builtin, as raw text. |
| `Type` | `ETextShaderPropertyType` | `Scalar` | Scalar / vector / texture family. |
| `TextureType` | `ETextShaderTextureType` | `Texture2D` | Texture dimension. Meaningful only alongside `bHasExplicitTextureType`. |
| `bHasExplicitTextureType` | `bool` | `false` | `true` only when the declared token names the dimension — `Texture2D`, `TextureCube`, `Texture2DArray`, `VolumeTexture`, or a `TextureSampleParameter*` variant. |
| `ComponentCount` | `int32` | `1` | 1–4 for scalar and vector properties. |
| `bConst` | `bool` | `false` | Declared `const` — a constant node, not a parameter. |
| `bHasDefaultValue` | `bool` | `false` | Whether the declaration carried `= <value>`. |
| `ScalarDefaultValue` | `double` | `0.0` | Default for a scalar property. |
| `VectorDefaultValue` | `FLinearColor` | `FLinearColor::White` | Default for a vector property. |
| `TextureDefaultObjectPath` | `FString` | `""` | Resolved object path from `Path(...)`. |
| `Metadata` | `FTextShaderMetadata` | default-constructed | The `[ … ]` block. |

Tokens such as `TextureObjectParameter` map to a single Unreal node class that can hold any texture
dimension. For those, `bHasExplicitTextureType` stays `false`, `TextureType` stays at its `Texture2D`
default, and the generator infers the real dimension from the assigned default asset rather than
rejecting it. Explicit tokens still validate strictly.

### `FTextShaderOutputBinding`

One `<target> = <variable>;` statement inside an `Outputs` section.

```cpp
struct FTextShaderOutputBinding
{
    enum class ETargetKind : uint8 { MaterialProperty, ExpressionInput };

    ETargetKind TargetKind = ETargetKind::MaterialProperty;
    FString MaterialProperty;
    FString ExpressionClass;
    TMap<FString, FString> ExpressionArguments;
    int32 ExpressionPinIndex = INDEX_NONE;
    FString TargetText;
    FString SourceText;
};
```

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `TargetKind` | `ETargetKind` | `MaterialProperty` | Which of the two target forms was written. |
| `MaterialProperty` | `FString` | `""` | The material property name, e.g. `Base.BaseColor`. Used when `TargetKind == MaterialProperty`. |
| `ExpressionClass` | `FString` | `""` | Target expression class when `TargetKind == ExpressionInput`. |
| `ExpressionArguments` | `TMap<FString, FString>` | empty | Constructor arguments for that expression, as raw text. |
| `ExpressionPinIndex` | `int32` | `INDEX_NONE` | Target input-pin index. Rejected at parse time if negative or unparseable. |
| `TargetText` | `FString` | `""` | Raw left-hand-side source text, reproduced verbatim in diagnostics. |
| `SourceText` | `FString` | `""` | Raw right-hand-side source text — the expression the generator evaluates. |

### `FTextShaderVariableDeclaration`

An output-variable declaration in an `Outputs` section.

```cpp
struct FTextShaderVariableDeclaration
{
    FString Type;
    FString Name;
    bool bHasDefaultValue = false;
    FString DefaultValueText;
};
```

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `Type` | `FString` | `""` | Type token, already run through the GLSL-alias normalization — `vec3` arrives as `float3`. |
| `Name` | `FString` | `""` | Variable name. |
| `bHasDefaultValue` | `bool` | `false` | Whether the declaration carried an initializer. A `Shader` with at least one initialized output declaration may omit its `Graph` block. |
| `DefaultValueText` | `FString` | `""` | The initializer expression, as raw text. |

### `FTextShaderGraphRegion`

A `#Region` / `#EndRegion` span inside a `Graph` block.

```cpp
struct FTextShaderGraphRegion
{
    FString Name;
    int32 StartLine = 1;
    int32 EndLine = MAX_int32;
};
```

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `Name` | `FString` | `""` | Region label; becomes the comment-box title. |
| `StartLine` | `int32` | **`1`** | 1-based, inclusive. |
| `EndLine` | `int32` | **`MAX_int32`** | 1-based. An unterminated region therefore extends to the end of the graph. |

### `FTextShaderLayoutNode`

One `Node(Var=…, X=…, Y=…)` directive.

```cpp
struct FTextShaderLayoutNode
{
    FString Var;
    int32 X = 0;
    int32 Y = 0;
};
```

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `Var` | `FString` | `""` | The graph variable whose node is pinned. |
| `X` | `int32` | `0` | Editor X coordinate. |
| `Y` | `int32` | `0` | Editor Y coordinate. |

### `FTextShaderLayoutComment`

One `Comment(Name=…, X=…, Y=…, W=…, H=…)` directive.

```cpp
struct FTextShaderLayoutComment
{
    FString Name;
    int32 X = 0;
    int32 Y = 0;
    int32 W = 420;
    int32 H = 240;
    FLinearColor Color = FLinearColor(0.10f, 0.16f, 0.22f, 0.35f);
};
```

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `Name` | `FString` | `""` | Comment-box text. |
| `X` | `int32` | `0` | Editor X coordinate. |
| `Y` | `int32` | `0` | Editor Y coordinate. |
| `W` | `int32` | **`420`** | Box width. |
| `H` | `int32` | **`240`** | Box height. |
| `Color` | `FLinearColor` | **`(0.10, 0.16, 0.22, 0.35)`** | Box tint. |

### `FTextShaderLayout`

The whole `Layout` section.

```cpp
struct FTextShaderLayout
{
    TArray<FTextShaderLayoutNode> Nodes;
    TArray<FTextShaderLayoutComment> Comments;
};
```

| Member | Type | Meaning |
| :-- | :-- | :-- |
| `Nodes` | `TArray<FTextShaderLayoutNode>` | Pinned node positions, in declaration order. |
| `Comments` | `TArray<FTextShaderLayoutComment>` | Comment boxes, in declaration order. |

A layout with neither nodes nor comments is treated as "no layout" and automatic placement runs
instead. See [Graph layout](../generation/graph-layout.md).

### `FTextShaderFunctionParameter`

One entry of an `Inputs`, `Outputs` or `Results` section.

```cpp
struct FTextShaderFunctionParameter
{
    FString Type;
    FString Name;
    bool bOptional = false;
    bool bHasDefaultValue = false;
    FString DefaultValueText;
    FTextShaderMetadata Metadata;
};
```

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `Type` | `FString` | `""` | Type token, GLSL aliases already rewritten. |
| `Name` | `FString` | `""` | Parameter name. For a return-type-lowered `Function`, the synthetic result parameter is named `__return`. |
| `bOptional` | `bool` | `false` | Declared `opt`. |
| `bHasDefaultValue` | `bool` | `false` | Whether a preview/default value was given. |
| `DefaultValueText` | `FString` | `""` | That value, as raw text. |
| `Metadata` | `FTextShaderMetadata` | default-constructed | The `[ … ]` block. |

### `FTextShaderFunctionDefinition`

A `Function` or `GraphFunction` block — an HLSL helper, not an asset.

```cpp
struct FTextShaderFunctionDefinition
{
    FString Name;
    bool bSelfContained = false;
    TArray<FTextShaderFunctionParameter> Inputs;
    TArray<FTextShaderFunctionParameter> Results;
    FString HLSL;
};
```

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `Name` | `FString` | `""` | Function name. Qualified as `Namespace::Function` when declared inside a `Namespace` block. |
| `bSelfContained` | `bool` | `false` | Set by the `SelfContained` or `Inline` modifier keyword. |
| `Inputs` | `TArray<FTextShaderFunctionParameter>` | empty | `in` parameters — including the ones whose qualifier was implicit. |
| `Results` | `TArray<FTextShaderFunctionParameter>` | empty | `out` parameters, **plus** the synthetic `__return` parameter when the declaration carried a return type. |
| `HLSL` | `FString` | `""` | The normalized body text. |

### `FTextShaderMaterialFunctionDefinition`

A `ShaderFunction`, `ShaderLayer` or `ShaderLayerBlend` block — each generates one asset.

```cpp
struct FTextShaderMaterialFunctionDefinition
{
    FString Name;
    FString Root;
    ETextShaderMaterialFunctionKind Kind = ETextShaderMaterialFunctionKind::ShaderFunction;
    TArray<FTextShaderPropertyDefinition> Properties;
    TArray<FTextShaderFunctionParameter> Inputs;
    TArray<FTextShaderFunctionParameter> Outputs;
    TMap<FString, FString> Settings;
    FString Code;
    int32 CodeStartIndex = INDEX_NONE;
    TArray<FTextShaderGraphRegion> GraphRegions;
    FTextShaderLayout Layout;
    FString HLSL;
};
```

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `Name` | `FString` | `""` | Asset path from the `Name=` attribute. |
| `Root` | `FString` | `""` | Package root from the `Root=` attribute. |
| `Kind` | `ETextShaderMaterialFunctionKind` | `ShaderFunction` | Which of the three keywords was used. |
| `Properties` | `TArray<FTextShaderPropertyDefinition>` | empty | Function-local property nodes. |
| `Inputs` | `TArray<FTextShaderFunctionParameter>` | empty | The `Inputs` section. |
| `Outputs` | `TArray<FTextShaderFunctionParameter>` | empty | The `Outputs` section. |
| `Settings` | `TMap<FString, FString>` | empty | The `Settings` section. **Keys are lower-cased** by `NormalizeSettingKey`. |
| `Code` | `FString` | `""` | The `Graph` block body. |
| `CodeStartIndex` | `int32` | `INDEX_NONE` | Character offset of that body in the prepared source, used to map graph diagnostics back to a file, line and column. |
| `GraphRegions` | `TArray<FTextShaderGraphRegion>` | empty | `#Region` spans. |
| `Layout` | `FTextShaderLayout` | default-constructed | The `Layout` section. |
| `HLSL` | `FString` | `""` | See the note below — never populated by the parser. |

### `FTextShaderVirtualFunctionDefinition`

A `VirtualFunction` block — a declaration of an existing `UMaterialFunction`; generates nothing.

```cpp
struct FTextShaderVirtualFunctionDefinition
{
    FString Name;
    FString Asset;
    TArray<FTextShaderFunctionParameter> Inputs;
    TArray<FTextShaderFunctionParameter> Outputs;
    TMap<FString, FString> Options;
};
```

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `Name` | `FString` | `""` | The name callable from `Graph`. **Required** — an empty or whitespace-only name fails the parse. |
| `Asset` | `FString` | `""` | Resolved object path of the target `UMaterialFunction`. **Required**, from the `Asset=` attribute or `Options.Asset`. |
| `Inputs` | `TArray<FTextShaderFunctionParameter>` | empty | Declared inputs. `Properties` is accepted as a synonym for this section. |
| `Outputs` | `TArray<FTextShaderFunctionParameter>` | empty | Declared outputs. **Must be non-empty.** |
| `Options` | `TMap<FString, FString>` | empty | The `Options` section. **Keys are lower-cased** by `NormalizeSettingKey`. |

### `FTextShaderDefinition`

The root of the parse result: one whole translation unit.

```cpp
struct FTextShaderDefinition
{
    FString Name;
    FString Root;
    TArray<FTextShaderPropertyDefinition> Properties;
    TMap<FString, FString> Settings;
    TArray<FTextShaderVariableDeclaration> OutputDeclarations;
    TArray<FTextShaderOutputBinding> Outputs;
    FString Code;
    int32 CodeStartIndex = INDEX_NONE;
    TArray<FTextShaderGraphRegion> GraphRegions;
    FTextShaderLayout Layout;
    FString HLSL;
    TArray<FTextShaderFunctionDefinition> Functions;
    TArray<FTextShaderFunctionDefinition> GraphFunctions;
    TArray<FTextShaderMaterialFunctionDefinition> MaterialFunctions;
    TArray<FTextShaderVirtualFunctionDefinition> VirtualFunctions;
    TArray<FString> Warnings;

    DREAMSHADER_API bool TryGetSetting(const TCHAR* Key, FString& OutValue) const;
    DREAMSHADER_API FString GetSetting(const TCHAR* Key, const TCHAR* DefaultValue = TEXT("")) const;
};
```

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `Name` | `FString` | `""` | The top-level `Shader(Name="…")`. **Empty means the unit declares no material** — the usual test for "is there a `Shader` here". |
| `Root` | `FString` | `""` | `Shader(Root="…")`. |
| `Properties` | `TArray<FTextShaderPropertyDefinition>` | empty | The `Shader`'s `Properties` section. |
| `Settings` | `TMap<FString, FString>` | empty | The `Shader`'s `Settings` section. **Keys are lower-cased.** |
| `OutputDeclarations` | `TArray<FTextShaderVariableDeclaration>` | empty | Output-variable declarations. |
| `Outputs` | `TArray<FTextShaderOutputBinding>` | empty | Output bindings. An empty array produces a parse **warning** and a generation **error**. |
| `Code` | `FString` | `""` | The `Graph` block body. |
| `CodeStartIndex` | `int32` | `INDEX_NONE` | Character offset of that body in the prepared source, for source-mapped diagnostics. |
| `GraphRegions` | `TArray<FTextShaderGraphRegion>` | empty | `#Region` spans. |
| `Layout` | `FTextShaderLayout` | default-constructed | The `Layout` section. |
| `HLSL` | `FString` | `""` | See the note below — never populated by the parser. |
| `Functions` | `TArray<FTextShaderFunctionDefinition>` | empty | `Function` blocks. |
| `GraphFunctions` | `TArray<FTextShaderFunctionDefinition>` | empty | `GraphFunction` blocks. |
| `MaterialFunctions` | `TArray<FTextShaderMaterialFunctionDefinition>` | empty | `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` blocks, in declaration order. |
| `VirtualFunctions` | `TArray<FTextShaderVirtualFunctionDefinition>` | empty | `VirtualFunction` blocks. |
| `Warnings` | `TArray<FString>` | empty | Non-fatal diagnostics collected during the parse. Never causes `Parse` to return `false`. |

#### `TryGetSetting`

```cpp
bool TryGetSetting(const TCHAR* Key, FString& OutValue) const;
```

| | |
| :-- | :-- |
| Parameter `Key` | Setting key in any casing, with or without surrounding whitespace |
| Parameter `OutValue` | Receives the stored value on a hit; **left untouched** on a miss |
| Returns | `true` if `Settings` contains `NormalizeSettingKey(Key)` |
| Constness | `const`, pure, any thread |

#### `GetSetting`

```cpp
FString GetSetting(const TCHAR* Key, const TCHAR* DefaultValue = TEXT("")) const;
```

| | |
| :-- | :-- |
| Parameter `Key` | As above |
| Parameter `DefaultValue` | Returned on a miss. Defaults to the empty string |
| Returns | The stored value, or `DefaultValue` |
| Constness | `const`, pure, any thread |

Because both the stored keys and the query pass through `NormalizeSettingKey`, lookup is
**case-insensitive and whitespace-trimmed**. It is *not* insensitive to interior spaces,
underscores or hyphens: `Two Sided` and `TwoSided` remain distinct keys.

## `NormalizeSettingKey`

```cpp
DREAMSHADER_API FString NormalizeSettingKey(const FString& InKey);
```

Trims leading and trailing whitespace, then lower-cases. That is the whole operation.

| Input | Result |
| :-- | :-- |
| `"  ShadingModel "` | `"shadingmodel"` |
| `"TWOSIDED"` | `"twosided"` |
| `"Two Sided"` | `"two sided"` |
| `"Two_Sided"` | `"two_sided"` |

> [!WARNING]
> `NormalizeSettingKey` and [`UDreamShaderSettings::NormalizeMappingKey`](settings.md#normalizemappingkey)
> are not interchangeable. The latter additionally strips every space, `_` and `-`, and applies to
> setting **values** (enum aliases), not keys. Using the wrong one turns `Two Sided Foliage` into a
> lookup miss. The four normalizing helpers are compared side by side in
> [`DreamShaderModule.h`](dreamshader-module.md#the-four-normalizers-compared).

Every `TMap<FString, FString>` in this header that holds settings or options —
`FTextShaderDefinition::Settings`, `FTextShaderMaterialFunctionDefinition::Settings`,
`FTextShaderVirtualFunctionDefinition::Options` and `FTextShaderMetadata::ReflectedProperties` — has
its keys normalized this way before storage. Read those maps directly only through a normalized key.

## Notes

- **`FTextShaderDefinition::HLSL` and `FTextShaderMaterialFunctionDefinition::HLSL` are never
  populated by the parser.** They are always empty in a definition returned by
  [`Parse`](parser.md). Only `FTextShaderFunctionDefinition::HLSL` — the body of a `Function` or
  `GraphFunction` — is filled. Code that reads either of the other two gets an empty string, and
  code that writes to them is writing to a field the shipped generator reads on a fallback path
  only.
- The structs have no reflection, no `Serialize`, and no `operator==`. They are not
  network-replicated, not saveable, and not comparable without writing a comparison yourself.
- Type tokens arrive already normalized. `vec3` becomes `float3`, `mat4` becomes `float4x4`, and so
  on for all fifteen GLSL aliases, before anything lands in a `Type` field. See
  [Types](../language/types.md).
- Container order is source order. `MaterialFunctions` is generated front to back, which is why a
  `Shader` can call a `ShaderFunction` declared beside it in the same file.
- Every struct is copyable and movable with the compiler-generated operations. Copies are deep —
  the members are `FString`, `TArray` and `TMap` values, not pointers.

## Example

Reading a parsed definition without touching the generator:

```cpp
#include "DreamShaderParser.h"
#include "DreamShaderTypes.h"

using namespace UE::DreamShader;

void Describe(const FTextShaderDefinition& Definition)
{
    if (!Definition.Name.IsEmpty())
    {
        // Case-insensitive; "backend", "Backend" and " BACKEND " all hit.
        const FString Backend = Definition.GetSetting(TEXT("Backend"), TEXT("<project default>"));

        UE_LOG(LogDreamShader, Display, TEXT("Shader '%s' (Root='%s', Backend=%s): %d propert(ies), %d binding(s)"),
            *Definition.Name,
            *Definition.Root,
            *Backend,
            Definition.Properties.Num(),
            Definition.Outputs.Num());
    }

    for (const FTextShaderMaterialFunctionDefinition& Function : Definition.MaterialFunctions)
    {
        UE_LOG(LogDreamShader, Display, TEXT("  %s '%s'"), LexToString(Function.Kind), *Function.Name);
    }

    for (const FString& Warning : Definition.Warnings)
    {
        UE_LOG(LogDreamShader, Warning, TEXT("  %s"), *Warning);
    }
}
```

For the source in [Generation](../generation/index.md#example) this prints:

```text
LogDreamShader: Shader 'Materials/M_Emissive' (Root='', Backend=<project default>): 1 propert(ies), 1 binding(s)
LogDreamShader:   ShaderFunction 'Functions/F_Tint'
```

## See also

- [`DreamShaderParser.h`](parser.md) — the function that fills every struct on this page
- [`DreamShaderModule.h`](dreamshader-module.md) — `SanitizeIdentifier` and the normalizer comparison
- [`DreamShaderSettings.h`](settings.md) — `NormalizeMappingKey` and the enum-alias catalogues
- [Properties](../language/properties.md) — the grammar behind `FTextShaderPropertyDefinition`
- [Inputs / Outputs / Results](../language/inputs-outputs.md) — the grammar behind `FTextShaderFunctionParameter`
- [Output bindings](../language/output-bindings.md) — the grammar behind `FTextShaderOutputBinding`
- [Layout](../language/layout.md) — `Node`, `Comment` and `#Region`
- [Metadata block](../parameters/metadata.md) — what lands in `FTextShaderMetadata`
- [Types](../language/types.md) — the token normalization applied before a `Type` field is filled
- [Settings](../settings/index.md) — key normalization from the language side
