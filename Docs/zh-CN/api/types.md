---
title: "DreamShaderTypes.h"
description: "DreamShaderTypes.h"
---

<a id="dreamshadertypesh"></a>

# DreamShaderTypes.h

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderTypes.h**

1.x 源代码数据模型。一直到 1.9.x，`FTextShaderParser::Parse` 会填充这些结构体，生成器则根据它们构建资产。**自 2.0 起，已没有任何解析会填充它们**：1.x 源代码由旧版前端读取到语言模块的 AST 中（参见[已废弃的解析器页面](parser.md)）。该头文件之所以保留，是因为编译器的资产层仍然在使用它——`BuildDefinitionForIRProduct` 会从某个 IR 产物填充一个 `FTextShaderDefinition`（名称、根、设置），以解析资产应放置于何处以及其材质如何配置——同时也因为编辑器模块中的 VirtualFunction 工具和 `dump-graph` 仍在使用其部分结构体和枚举。

定义于头文件 `DreamShaderTypes.h`。

| | |
| :-- | :-- |
| 模块 | `DreamShader`（Runtime） |
| 包含 | `#include "DreamShaderTypes.h"` |
| 命名空间 | `UE::DreamShader` |
| 内容 | 13 个结构体、5 个枚举、1 个内联函数、1 个自由函数、2 个成员函数 |
| 反射 | 无——没有 `USTRUCT`、没有 `UENUM`、没有 `UPROPERTY` |
| 引入 | 仅 `CoreMinimal.h` |

> [!NOTE]
> **这些结构体均不携带 `DREAMSHADER_API`。** 它们是仅头文件的聚合体，具有隐式的
> 特殊成员函数，因此无需导出符号即可跨模块边界构造、复制或销毁它们。
> 此头文件中只有三样东西是导出的：
> `FTextShaderDefinition::TryGetSetting`、`FTextShaderDefinition::GetSetting` 和
> `NormalizeSettingKey`。

<a id="declaration-index"></a>

## 声明索引

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

<a id="enumerations"></a>

## 枚举

<a id="etextshaderpropertytype"></a>

### `ETextShaderPropertyType`

```cpp
enum class ETextShaderPropertyType : uint8
{
    Scalar,
    Vector,
    Texture2D,
};
```

| 枚举值 | 值 | 含义 |
| :-- | :-- | :-- |
| `Scalar` | `0` | 标量值属性——`float` / `int` / `bool` 系列 |
| `Vector` | `1` | 矢量值属性——`float2` / `float3` / `float4` 系列 |
| `Texture2D` | `2` | **任意**维度的纹理值属性；维度单独携带于 `ETextShaderTextureType` 中 |

`Texture2D` 枚举值名称是一种历史拼写。`TextureCube` 或 `VolumeTexture` 属性
也拥有 `Type == Texture2D`。

<a id="etextshadertexturetype"></a>

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

| 枚举值 | 值 |
| :-- | :-- |
| `Texture2D` | `0` |
| `TextureCube` | `1` |
| `Texture2DArray` | `2` |
| `VolumeTexture` | `3` |

<a id="etextshaderpropertysource"></a>

### `ETextShaderPropertySource`

```cpp
enum class ETextShaderPropertySource : uint8
{
    Parameter,
    UEBuiltin,
};
```

| 枚举值 | 值 | 含义 |
| :-- | :-- | :-- |
| `Parameter` | `0` | 该属性变为材质参数节点 |
| `UEBuiltin` | `1` | 该属性是绑定到某个名称的内置 `UE.*` 调用 |

<a id="ftextshaderoutputbindingetargetkind"></a>

### `FTextShaderOutputBinding::ETargetKind`

```cpp
enum class ETargetKind : uint8
{
    MaterialProperty,
    ExpressionInput,
};
```

| 枚举值 | 值 | 含义 |
| :-- | :-- | :-- |
| `MaterialProperty` | `0` | 绑定目标是材质属性，例如 `Base.BaseColor` |
| `ExpressionInput` | `1` | 绑定目标是某个具名表达式上的输入引脚，例如 `Expression(Class="…").Pin[0]` |

<a id="etextshadermaterialfunctionkind"></a>

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

| 枚举值 | 值 | `LexToString` 结果 | 生成 |
| :-- | :-- | :-- | :-- |
| `ShaderFunction` | `0` | `"ShaderFunction"` | `UMaterialFunction` |
| `MaterialLayer` | `1` | **`"ShaderLayer"`** | `UMaterialFunctionMaterialLayer` |
| `MaterialLayerBlend` | `2` | **`"ShaderLayerBlend"`** | `UMaterialFunctionMaterialLayerBlend` |

> [!NOTE]
> 枚举值名称仍使用 1.3.0 之前的 `MaterialLayer*` 拼写，但 `LexToString` 打印
> 的是当前的 `ShaderLayer*` 拼写。因此生成的消息会说 `ShaderLayer`，而绝不会是
> `MaterialLayer`，即使源文件使用了已弃用的关键字。`default:` 标签
> 与 `ShaderFunction` 情形共用，因此超出范围的值也会打印 `"ShaderFunction"`。

<a id="structures"></a>

## 结构体

> [!NOTE]
> 下面的**含义**列说明了 1.x 解析器在每个成员中存储了什么，以及 1.x
> 生成器如何读取它。自 2.0.0 起，没有任何解析会填充这些结构体；剩下的少数使用者已在
> 引言和[备注](#notes)下命名。此处提到的解析时检查、警告和源映射
> 属于已废弃的解析器：1.x 源代码现在由旧版前端诊断，
> 其 `DSHnnnn` 代码会携带该构造的行号和列号。

<a id="ftextshadermetadata"></a>

### `FTextShaderMetadata`

附加到属性或函数参数的声明元数据。

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

| 成员 | 类型 | 默认值 | 含义 |
| :-- | :-- | :-- | :-- |
| `Group` | `FString` | `""` | 参数组。当声明未显式设置时，从外层 `Properties Group("…")` 作用域继承。 |
| `bHasSortPriority` | `bool` | `false` | `SortPriority` 是否有意义。由显式的 `SortPriority` / `Sort` 键设为 `true`，也会由应用于 `Group("…")` 作用域内声明的自动编号设置。省略该键的顶层（未分组）声明保持 `false`。 |
| `SortPriority` | `int32` | **`32`** | 组内的排序顺序。 |
| `Description` | `FString` | `""` | 工具提示 / 描述文本。 |
| `ReflectedProperties` | `TMap<FString, FString>` | 空 | 来自 `[ … ]` 块的任意反射 `UPROPERTY` 名称 → 字面文本对。**键会使用 `NormalizeSettingKey` 进行规范化**，即去除首尾空白并转为小写。 |

1.x 解析器自行消费（而非放入 `ReflectedProperties`）的键：`Group` /
`Category`、`Description`、`SortPriority` / `Sort`，以及由 `Slider(min, max)` 生成的
`SliderMin` / `SliderMax` 对。参见[元数据块](../parameters/metadata.md)。

<a id="ftextshaderpropertydefinition"></a>

### `FTextShaderPropertyDefinition`

`Properties` 节的一个条目。

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

| 成员 | 类型 | 默认值 | 含义 |
| :-- | :-- | :-- | :-- |
| `Name` | `FString` | `""` | 按书写形式给出的属性 / 参数名称。 |
| `Source` | `ETextShaderPropertySource` | `Parameter` | 表示参数节点还是 `UE.*` 内置项。 |
| `ParameterNodeType` | `FString` | `""` | 当声明指定了 Unreal 表达式类标记时，该标记的显式内容，例如 `ScalarParameter`、`TextureSampleParameter2D`。对于紧凑类型标记则为空。 |
| `UEBuiltinFunctionName` | `FString` | `""` | 当 `Source == UEBuiltin` 时的内置项名称。 |
| `UEBuiltinArguments` | `TMap<FString, FString>` | 空 | 该内置项的命名参数，以原始文本形式给出。 |
| `Type` | `ETextShaderPropertyType` | `Scalar` | 标量 / 向量 / 纹理系列。 |
| `TextureType` | `ETextShaderTextureType` | `Texture2D` | 纹理维度。仅在配合 `bHasExplicitTextureType` 时才有意义。 |
| `bHasExplicitTextureType` | `bool` | `false` | 仅当声明的标记指明了维度时为 `true` —— `Texture2D`、`TextureCube`、`Texture2DArray`、`VolumeTexture`，或某个 `TextureSampleParameter*` 变体。 |
| `ComponentCount` | `int32` | `1` | 对于标量和向量属性为 1–4。 |
| `bConst` | `bool` | `false` | 声明为 `const` —— 一个常量节点，而非参数。 |
| `bHasDefaultValue` | `bool` | `false` | 该声明是否带有 `= <value>`。 |
| `ScalarDefaultValue` | `double` | `0.0` | 标量属性的默认值。 |
| `VectorDefaultValue` | `FLinearColor` | `FLinearColor::White` | 向量属性的默认值。 |
| `TextureDefaultObjectPath` | `FString` | `""` | 由 `Path(...)` 解析出的对象路径。 |
| `Metadata` | `FTextShaderMetadata` | 默认构造 | `[ … ]` 块。 |

诸如 `TextureObjectParameter` 之类的标记映射到单个 Unreal 节点类，该类可容纳任意纹理维度。对于这些标记，`bHasExplicitTextureType` 保持为 `false`，`TextureType` 保持其 `Texture2D` 默认值，而 1.x 生成器会从所指定的默认资产推断出真实维度，而不是拒绝它。自 2.0.0 起，1.x 对纹理默认值中所写类与其属性之间的检查在绑定之前针对源进行：
[`DSH1043`](../diagnostics/DSH1xxx.md#dsh1043) 针对非纹理类，
[`DSH1044`](../diagnostics/DSH1xxx.md#dsh1044) 针对另一维度的纹理类。

<a id="ftextshaderoutputbinding"></a>

### `FTextShaderOutputBinding`

`Outputs` 段内的一条 `<target> = <variable>;` 语句。

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

| 成员 | 类型 | 默认值 | 含义 |
| :-- | :-- | :-- | :-- |
| `TargetKind` | `ETargetKind` | `MaterialProperty` | 书写的是两种目标形式中的哪一种。 |
| `MaterialProperty` | `FString` | `""` | 材质属性名称，例如 `Base.BaseColor`。当 `TargetKind == MaterialProperty` 时使用。 |
| `ExpressionClass` | `FString` | `""` | 当 `TargetKind == ExpressionInput` 时的目标表达式类。 |
| `ExpressionArguments` | `TMap<FString, FString>` | 空 | 该表达式的构造函数参数，以原始文本形式给出。 |
| `ExpressionPinIndex` | `int32` | `INDEX_NONE` | 目标输入引脚索引。1.x 会在解析时拒绝负数或无法解析的索引。 |
| `TargetText` | `FString` | `""` | 原始左侧源文本，1.x 会在诊断中原样重现。 |
| `SourceText` | `FString` | `""` | 原始右侧源文本 —— 即 1.x 生成器所求值的表达式。 |

<a id="ftextshadervariabledeclaration"></a>

### `FTextShaderVariableDeclaration`

`Outputs` 段中的输出变量声明。

```cpp
struct FTextShaderVariableDeclaration
{
    FString Type;
    FString Name;
    bool bHasDefaultValue = false;
    FString DefaultValueText;
};
```

| 成员 | 类型 | 默认值 | 含义 |
| :-- | :-- | :-- | :-- |
| `Type` | `FString` | `""` | 类型标记，已通过 GLSL 别名规范化处理 —— `vec3` 会以 `float3` 的形式传入。 |
| `Name` | `FString` | `""` | 变量名称。 |
| `bHasDefaultValue` | `bool` | `false` | 该声明是否带有初始化器。带有至少一个已初始化输出声明的 `Shader` 可以省略其 `Graph` 块。 |
| `DefaultValueText` | `FString` | `""` | 初始化器表达式，以原始文本形式给出。 |

<a id="ftextshadergraphregion"></a>

### `FTextShaderGraphRegion`

`Graph` 块内的一个 `#Region` / `#EndRegion` 范围。

```cpp
struct FTextShaderGraphRegion
{
    FString Name;
    int32 StartLine = 1;
    int32 EndLine = MAX_int32;
};
```

| 成员 | 类型 | 默认值 | 含义 |
| :-- | :-- | :-- | :-- |
| `Name` | `FString` | `""` | 区域标签；会成为注释框标题。 |
| `StartLine` | `int32` | **`1`** | 从 1 开始，含端点。 |
| `EndLine` | `int32` | **`MAX_int32`** | 从 1 开始。因此未终止的区域会一直延伸到图的末尾。 |

<a id="ftextshaderlayoutnode"></a>

### `FTextShaderLayoutNode`

一条 `Node(Var=…, X=…, Y=…)` 指令。

```cpp
struct FTextShaderLayoutNode
{
    FString Var;
    int32 X = 0;
    int32 Y = 0;
};
```

| 成员 | 类型 | 默认值 | 含义 |
| :-- | :-- | :-- | :-- |
| `Var` | `FString` | `""` | 其节点被固定的图变量。 |
| `X` | `int32` | `0` | 编辑器 X 坐标。 |
| `Y` | `int32` | `0` | 编辑器 Y 坐标。 |

<a id="ftextshaderlayoutcomment"></a>

### `FTextShaderLayoutComment`

一条 `Comment(Name=…, X=…, Y=…, W=…, H=…)` 指令。

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

| 成员 | 类型 | 默认值 | 含义 |
| :-- | :-- | :-- | :-- |
| `Name` | `FString` | `""` | 注释框文本。 |
| `X` | `int32` | `0` | 编辑器 X 坐标。 |
| `Y` | `int32` | `0` | 编辑器 Y 坐标。 |
| `W` | `int32` | **`420`** | 框宽度。 |
| `H` | `int32` | **`240`** | 框高度。 |
| `Color` | `FLinearColor` | **`(0.10, 0.16, 0.22, 0.35)`** | 框色调。 |

<a id="ftextshaderlayout"></a>

### `FTextShaderLayout`

整个 `Layout` 段。

```cpp
struct FTextShaderLayout
{
    TArray<FTextShaderLayoutNode> Nodes;
    TArray<FTextShaderLayoutComment> Comments;
};
```

| 成员 | 类型 | 含义 |
| :-- | :-- | :-- |
| `Nodes` | `TArray<FTextShaderLayoutNode>` | 固定的节点位置，按声明顺序排列。 |
| `Comments` | `TArray<FTextShaderLayoutComment>` | 注释框，按声明顺序排列。 |

既没有节点也没有注释的布局会被视为“无布局”，转而运行自动放置。请参阅 [图布局](../generation/graph-layout.md)。

<a id="ftextshaderfunctionparameter"></a>

### `FTextShaderFunctionParameter`

`Inputs`、`Outputs` 或 `Results` 段中的一条条目。

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

| 成员 | 类型 | 默认值 | 含义 |
| :-- | :-- | :-- | :-- |
| `Type` | `FString` | `""` | 类型标记，GLSL 别名已重写。 |
| `Name` | `FString` | `""` | 参数名称。对于经过返回类型降级的 `Function`，合成的结果参数名为 `__return`。 |
| `bOptional` | `bool` | `false` | 声明为 `opt`。 |
| `bHasDefaultValue` | `bool` | `false` | 是否给出了预览值 / 默认值。 |
| `DefaultValueText` | `FString` | `""` | 该值，以原始文本形式给出。 |
| `Metadata` | `FTextShaderMetadata` | 默认构造 | `[ … ]` 块。 |

<a id="ftextshaderfunctiondefinition"></a>

### `FTextShaderFunctionDefinition`

一个 `Function` 或 `GraphFunction` 块 —— 是一个 HLSL 辅助函数，而非资产。

```cpp
struct FTextShaderFunctionDefinition
{
    FString Name;
    bool bSelfContained = false;
    TArray<FTextShaderFunctionParameter> Inputs;
    TArray<FTextShaderFunctionParameter> Results;
    FString HLSL;
    TArray<FString> IncludePaths;
};
```

| 成员 | 类型 | 默认值 | 含义 |
| :-- | :-- | :-- | :-- |
| `Name` | `FString` | `""` | 函数名。在 `Namespace` 块内声明时限定为 `Namespace::Function`。 |
| `bSelfContained` | `bool` | `false` | 由 `SelfContained` 或 `Inline` 修饰符关键字设置。 |
| `Inputs` | `TArray<FTextShaderFunctionParameter>` | 空 | `in` 参数 —— 包括限定符为隐式的那些参数。 |
| `Results` | `TArray<FTextShaderFunctionParameter>` | 空 | `out` 参数，**外加**声明带有返回类型时的合成 `__return` 参数。 |
| `HLSL` | `FString` | `""` | 规范化后的正文文本。 |
| `IncludePaths` | `TArray<FString>` | 空 | 位于正文最顶部的 `#include` 行，已从 `HLSL` 中剥离：对于普通 `Function`，提升到生成的 `.ush` 顶部；对于嵌入式正文，提升到 Custom 节点的 include 列表。虚拟着色器路径，已去除引号。 |

<a id="ftextshadermaterialfunctiondefinition"></a>

### `FTextShaderMaterialFunctionDefinition`

一个 `ShaderFunction`、`ShaderLayer` 或 `ShaderLayerBlend` 块 —— 每一种生成一个资产。

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

| 成员 | 类型 | 默认值 | 含义 |
| :-- | :-- | :-- | :-- |
| `Name` | `FString` | `""` | 来自 `Name=` 特性的资产路径。 |
| `Root` | `FString` | `""` | 来自 `Root=` 特性的包根路径。 |
| `Kind` | `ETextShaderMaterialFunctionKind` | `ShaderFunction` | 使用了三个关键字中的哪一个。 |
| `Properties` | `TArray<FTextShaderPropertyDefinition>` | 空 | 函数局部属性节点。 |
| `Inputs` | `TArray<FTextShaderFunctionParameter>` | 空 | `Inputs` 区段。 |
| `Outputs` | `TArray<FTextShaderFunctionParameter>` | 空 | `Outputs` 区段。 |
| `Settings` | `TMap<FString, FString>` | 空 | `Settings` 区段。**键会被 `NormalizeSettingKey` 转为小写**。 |
| `Code` | `FString` | `""` | `Graph` 块正文。 |
| `CodeStartIndex` | `int32` | `INDEX_NONE` | 该正文在预处理源中的字符偏移量，1.x 曾据此推算图诊断所在行。自 2.0.0 起无任何读取。 |
| `GraphRegions` | `TArray<FTextShaderGraphRegion>` | 空 | `#Region` 跨度。 |
| `Layout` | `FTextShaderLayout` | default-constructed | `Layout` 区段。 |
| `HLSL` | `FString` | `""` | 从不被 1.x 解析器填充。 |

<a id="ftextshadervirtualfunctiondefinition"></a>

### `FTextShaderVirtualFunctionDefinition`

一个 `VirtualFunction` 块 —— 对现有 `UMaterialFunction` 的声明；不生成任何内容。

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

| 成员 | 类型 | 默认值 | 含义 |
| :-- | :-- | :-- | :-- |
| `Name` | `FString` | `""` | 可从 `Graph` 调用的名称。**必需**；没有它，旧版前端会报告 [`DSH6311`](../diagnostics/DSH6xxx.md#dsh6311)。 |
| `Asset` | `FString` | `""` | 目标 `UMaterialFunction` 的已解析对象路径。**必需**，来自 `Asset=` 特性或 `Options.Asset`；没有它，[`DSH6312`](../diagnostics/DSH6xxx.md#dsh6312)。 |
| `Inputs` | `TArray<FTextShaderFunctionParameter>` | 空 | 已声明的输入。`Properties` 可作为该区段的同义词接受。 |
| `Outputs` | `TArray<FTextShaderFunctionParameter>` | 空 | 已声明的输出。**必须非空**；否则 [`DSH6313`](../diagnostics/DSH6xxx.md#dsh6313)。 |
| `Options` | `TMap<FString, FString>` | 空 | `Options` 区段。**键会被 `NormalizeSettingKey` 转为小写**。 |

<a id="ftextshaderdefinition"></a>

### `FTextShaderDefinition`

解析结果的根：一整个翻译单元。

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

| 成员 | 类型 | 默认值 | 含义 |
| :-- | :-- | :-- | :-- |
| `Name` | `FString` | `""` | 顶层 `Shader(Name="…")`。**为空表示该单元未声明材质** —— 这是判断“这里是否有 `Shader`”的常用测试。 |
| `Root` | `FString` | `""` | `Shader(Root="…")`。 |
| `Properties` | `TArray<FTextShaderPropertyDefinition>` | 空 | `Shader` 的 `Properties` 区段。 |
| `Settings` | `TMap<FString, FString>` | 空 | `Shader` 的 `Settings` 区段。**键会被转为小写。** |
| `OutputDeclarations` | `TArray<FTextShaderVariableDeclaration>` | 空 | 输出变量声明。 |
| `Outputs` | `TArray<FTextShaderOutputBinding>` | 空 | 输出绑定。在 1.x 中，空数组会产生解析警告和生成错误；自 2.0.0 起，没有 `Outputs` 区段的 `Shader` 会警告 [`DSH2256`](../diagnostics/DSH2xxx.md#dsh2256)。 |
| `Code` | `FString` | `""` | `Graph` 块正文。 |
| `CodeStartIndex` | `int32` | `INDEX_NONE` | 该正文在预处理源中的字符偏移量，1.x 曾据此推算图诊断所在行。自 2.0.0 起无任何读取。 |
| `GraphRegions` | `TArray<FTextShaderGraphRegion>` | 空 | `#Region` 跨度。 |
| `Layout` | `FTextShaderLayout` | default-constructed | `Layout` 区段。 |
| `HLSL` | `FString` | `""` | 从不被 1.x 解析器填充。 |
| `Functions` | `TArray<FTextShaderFunctionDefinition>` | 空 | `Function` 块。 |
| `GraphFunctions` | `TArray<FTextShaderFunctionDefinition>` | 空 | `GraphFunction` 块。 |
| `MaterialFunctions` | `TArray<FTextShaderMaterialFunctionDefinition>` | 空 | `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` 块，按声明顺序。 |
| `VirtualFunctions` | `TArray<FTextShaderVirtualFunctionDefinition>` | 空 | `VirtualFunction` 块。 |
| `Warnings` | `TArray<FString>` | 空 | 1.x 解析收集的非致命诊断；它们从未使 `Parse` 返回 `false`。自 2.0.0 起从不填充。 |

<a id="trygetsetting"></a>

#### `TryGetSetting`

```cpp
bool TryGetSetting(const TCHAR* Key, FString& OutValue) const;
```

| | |
| :-- | :-- |
| 参数 `Key` | 任意大小写的设置键，可带或不带外围空白 |
| 参数 `OutValue` | 命中时接收存储的值；未命中时**保持不变** |
| 返回值 | 若 `Settings` 包含 `NormalizeSettingKey(Key)` 则为 `true` |
| 常量性 | `const`，纯函数，任意线程 |

<a id="getsetting"></a>

#### `GetSetting`

```cpp
FString GetSetting(const TCHAR* Key, const TCHAR* DefaultValue = TEXT("")) const;
```

| | |
| :-- | :-- |
| 参数 `Key` | 同上 |
| 参数 `DefaultValue` | 未命中时返回。默认为空字符串 |
| 返回值 | 存储的值，或 `DefaultValue` |
| 常量性 | `const`，纯函数，任意线程 |

由于存储的键和查询都会经过 `NormalizeSettingKey`，查找是
**不区分大小写且会裁剪空白的**。它对内部空格、下划线或连字符
*不*敏感：`Two Sided` 和 `TwoSided` 仍是不同的键。

<a id="normalizesettingkey"></a>

## `NormalizeSettingKey`

```cpp
DREAMSHADER_API FString NormalizeSettingKey(const FString& InKey);
```

裁剪前导和尾随空白，然后转为小写。这就是全部操作。

| 输入 | 结果 |
| :-- | :-- |
| `"  ShadingModel "` | `"shadingmodel"` |
| `"TWOSIDED"` | `"twosided"` |
| `"Two Sided"` | `"two sided"` |
| `"Two_Sided"` | `"two_sided"` |

> [!WARNING]
> `NormalizeSettingKey` 和 [`UDreamShaderSettings::NormalizeMappingKey`](settings.md#normalizemappingkey)
> 不可互换。后者还会去除所有空格、`_` 和 `-`，并且适用于
> 设置**值**（枚举别名），而非键。使用错误的一个会让 `Two Sided Foliage` 变成
> 查找未命中。这四个规范化辅助函数在
> [`DreamShaderModule.h`](dreamshader-module.md#the-four-normalizers-compared) 中并排比较。

此头文件中每一个持有设置或选项的 `TMap<FString, FString>` ——
`FTextShaderDefinition::Settings`、`FTextShaderMaterialFunctionDefinition::Settings`、
`FTextShaderVirtualFunctionDefinition::Options` 和 `FTextShaderMetadata::ReflectedProperties` —— 在存储之前
都会以这种方式对其键进行规范化。只通过规范化键直接读取这些映射。

<a id="notes"></a>

## 说明

- **在 2.0 中大多数成员是残留物。** 编译器填充 `FTextShaderDefinition` 的 `Name`、`Root` 和 `Settings`
  （以及 `FTextShaderMaterialFunctionDefinition` 的 kind、name、root 和 settings）
  而不填充其他任何内容：属性、输出、图代码和
  布局都以 IR 形式传递。不要指望从编译器获取的定义能描述源代码。
  在定义之外，`Classic` 图布局会从 IR 的布局
  提示填充 `FTextShaderLayout`，而 VirtualFunction 工具会从它们用旧版前端解析的声明中
  填充 `FTextShaderVirtualFunctionDefinition` /
  `FTextShaderFunctionParameter`。
- 这些结构体没有反射、没有 `Serialize`，也没有 `operator==`。它们不会被
  网络复制、不可保存，并且除非你自己编写比较，否则不可比较。
- 1.x 解析器在任何内容落入 `Type` 字段之前会规范化类型标记：`vec3` 变为
  `float3`，`mat4` 变为 `float4x4`，依此类推。自 2.0.0 起，1.x 文本中的 GLSL 拼写会通过 [`DSH5277`](../diagnostics/DSH5xxx.md#dsh5277) 被读取为
  其 HLSL 类型。参见
  [类型](../language/types.md)。
- 容器顺序过去是源顺序，1.x 会从前到后生成 `MaterialFunctions`。自
  2.0.0 起，一个文件的资产按调用顺序构建 —— 被另一个函数调用的函数会先
  构建 —— 而相互循环调用的函数会被 [`DSH8299`](../diagnostics/DSH8xxx.md#dsh8299)。
- 每个结构体都可使用编译器生成的操作进行复制和移动。复制是深层的 ——
  成员是 `FString`、`TArray` 和 `TMap` 值，而非指针。

<a id="example"></a>

## 示例

读取一个定义：

```cpp
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

对于 [生成](../generation/index.md#example) 中的源代码，当 1.x 解析器填充
该定义时，会打印：

```text
LogDreamShader: Shader 'Materials/M_Emissive' (Root='', Backend=<project default>): 1 propert(ies), 1 binding(s)
LogDreamShader:   ShaderFunction 'Functions/F_Tint'
```

2.0 编译器构建的定义只携带 `Name`、`Root` 和 `Settings` —— 而 `Settings`
从不持有 `Backend`，绑定器会将其取出放到 IR 产物上 —— 因此它会打印
`Backend=<project default>`，两个计数都是 `0`，并且不会列出其他任何内容。

<a id="see-also"></a>

## 另请参见

- [`DreamShaderParser.h`](parser.md) —— 在 2.0 中已弃用；现在读取 1.x 源代码的内容
- [`DreamShaderModule.h`](dreamshader-module.md) —— `SanitizeIdentifier` 以及规范化器比较
- [`DreamShaderSettings.h`](settings.md) —— `NormalizeMappingKey` 以及枚举别名目录
- [属性](../language/properties.md) —— `FTextShaderPropertyDefinition` 背后的语法
- [输入 / 输出 / 结果](../language/inputs-outputs.md) —— `FTextShaderFunctionParameter` 背后的语法
- [输出绑定](../language/output-bindings.md) —— `FTextShaderOutputBinding` 背后的语法
- [布局](../language/layout.md) —— `Node`、`Comment` 和 `#Region`
- [元数据块](../parameters/metadata.md) —— 落入 `FTextShaderMetadata` 的内容
- [类型](../language/types.md) —— 填充 `Type` 字段之前应用的标记规范化
- [设置](../../settings/index.md) —— 从语言侧进行的键规范化
