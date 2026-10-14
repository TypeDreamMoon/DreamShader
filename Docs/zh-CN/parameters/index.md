---
title: "参数"
description: "参数"
---

<a id="parameters"></a>

# 参数

> [DreamShader](../index.md) » **参数**

参数是一种 `Properties` 声明，在生成的资源中会成为命名的 `UMaterialExpression` 节点——可以是实例能够覆盖的材质参数、常量，或内置输入节点。

| | |
| :-- | :-- |
| 声明位置 | `.dsm`、`.dsf` —— 即 `Shader`、`ShaderFunction`、`ShaderLayer` 或 `ShaderLayerBlend` 的 `Properties` 部分 |
| 类别 | 参数类型目录 |
| 生成 | 主体每读取一个属性就生成一个节点；没有任何读取的属性则不生成任何节点（[`DSH4390`](../diagnostics/DSH4xxx.md#dsh4390)，信息）。在使用处展开的参数节点类型（参见[参数节点标记](parameter-nodes.md#the-22-tokens)）会为每个不同的使用各生成一个节点。节点位置来自[图布局](../generation/graph-layout.md)。 |
| 起始版本 | `1.2.3`（显式 `*Parameter` 标记）、`1.2.6`（`const`）、`1.5.0`（`Group(…) { … }`、`Slider(…)`） |

<a id="synopsis"></a>

## 概要

```c
[const] <type-token> <name> [ = <default-value> ] [ [ <metadata-entry> { ; | , } … ] ] ;
```

元数据块外层的 `[ … ]` 是**字面 DreamShaderLang 标点**；`= <default-value>` 周围的 `[ … ]` 以及整个元数据块周围的是“可选”元方括号。换句话说，带元数据的声明写作 `float A = 1.0 [Group="X"];`。

外层节语法——`Properties` 可以出现的位置、`{` 之前可选的 `=`、重复、`Group("Name") { … }` 作用域以及排序规则——在[属性（节）](../language/properties.md)中规定。本节仅记录可以出现在 `<type-token>`、`<default-value>` 和 `<metadata-entry>` 槽位中的内容。

自 2.0.0 起，1.x 源由旧版前端读取，它会将每个属性转换为 2.0 语言为其书写的形式：一个 `uniform`、一个 `static const`，或在其被读取处构建的参数节点。`.dss` 的写法见[2.0 语言](../language-v2/index.md)；`dsc migrate` 会为你书写（[Migrate](../tools/migrate.md)）。

<a id="the-four-kinds-of-type-token"></a>

## 四种类型标记

| 类别 | 数量 | 示例 | 生成的节点 | 参考 |
| :-- | :-- | :-- | :-- | :-- |
| 紧凑标量 | 7 | `float Strength = 1.0;` | `UMaterialExpressionScalarParameter` | [紧凑类型标记](compact-types.md#scalar-tokens) |
| 紧凑向量 | 27 | `vec3 Tint = vec3(1, 1, 1);` | `UMaterialExpressionVectorParameter` | [紧凑类型标记](compact-types.md#vector-tokens) |
| 紧凑纹理 | 5 | `Texture2D Base = Path(Game, "T_X");` | `UMaterialExpressionTextureObjectParameter` | [紧凑类型标记](compact-types.md#texture-tokens) |
| 显式 `*Parameter` 节点 | 22，其中 15 个可构建 | `TextureSampleParameter2D Tex;` | 命名的 `UMaterialExpression` 子类 | [参数节点标记](parameter-nodes.md) |
| `UE.<Name>` 内置 | — | `UE.TexCoord(Index = 0) UV;` | 该内置的节点，或反射类 | [UE 内置](../builtins/ue.md) |

每个标记都**不区分大小写**匹配。不匹配以上任何一项的标记是[`DSH3252`](../diagnostics/DSH3xxx.md#dsh3252)；七种没有 2.0 写法的参数节点类型之一则是[`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253) *(自 2.0.0 起)*。

<a id="which-form-to-use"></a>

## 应使用哪种形式

| 目标 | 写法 | 原因 |
| :-- | :-- | :-- |
| 美术师可在实例上调整的浮点数 | `float`、`half`、`int`、`uint`、`bool`（或 `ScalarParameter`） | 全部七种紧凑标量标记与 `ScalarParameter` 都会生成完全相同的 `ScalarParameter` 节点 |
| 颜色或 2/3/4 分量参数 | `float2` … `bvec4` | 声明的分量数量控制 `Graph` 读取哪个输出（`RG` / `RGB` / `RGBA`） |
| 必须始终按 RGBA 读取的四分量参数 | `VectorParameter` | 固定为 4 个分量，无论其如何使用 |
| 着色器自行采样的纹理 | `Texture2D` / `TextureCube` / `Texture2DArray` / `Texture3D` / `VolumeTexture` | 生成该维度的纹理*对象*参数 |
| 带采样器节点且采样可配置的纹理 | `TextureSampleParameter2D` 及类似项 | 拥有 `Coordinates` 和 mip 引脚；接受 `SamplerType` / `MipValueMode` 元数据 |
| 无固定维度的纹理对象 | `TextureObjectParameter` | 从所赋默认资源获取其维度 *(自 1.6.0 起)* |
| 编译期分支 | `StaticSwitchParameter` | 必须以调用形式 `N(True = …, False = …)` 使用 |
| 无分支的编译期布尔值 | `StaticBoolParameter` | 仅值 |
| 美术师不得更改的烘焙值 | `const <compact-token>` | 生成 `Constant` / `Constant2Vector` / `Constant3Vector` / `Constant4Vector` / `TextureObject` —— 无参数、无实例覆盖 |
| 引擎馈送的节点（UV、时间、相机、顶点色、MPC） | `UE.<Name>( … )` | 参见 [UE 内置](../builtins/ue.md) |
| 任何其他 `UMaterialExpression` 类 | `UE.<Class>(OutputType = "float4", … )` | 通用反射式构建 —— 参见 [UE.Expression](../builtins/ue-expression.md) |

> [!NOTE]
> `const` 与 39 种紧凑标记一起使用是合法的，并且 *(自 2.0.0 起)* 与 `ScalarParameter`、
> `VectorParameter` 和 `TextureObjectParameter` 一起使用也是合法的，此时它们会构建与 `const float`、
> `const float4` 和 `const Texture2D` 相同的常量。在任何其他 `*Parameter` 标记上则是 `DSH3253`。在 `UE.*`
> 声明上，`const` 会被忽略 *(自 2.0.0 起)*。

<a id="pages"></a>

## 页面

| 页面 | 涵盖内容 |
| :-- | :-- |
| [紧凑类型标记](compact-types.md) | 全部 39 种紧凑标记、各自生成的节点、默认值语法，以及在 `Properties` 中*无效*的标记 |
| [参数节点标记](parameter-nodes.md) | 全部 22 种显式 `*Parameter` 标记、其生成的类、按类型划分的默认值与元数据槽位 |
| [元数据块](metadata.md) | `[ Key = Value ; … ]`、`Slider(min, max)`、`ParameterName` 以及反射式 UPROPERTY 透传 |
| [SamplerType](sampler-type.md) | 每个 `SamplerType` 值及其拼写、`SamplerSource`，以及纹理维度如何被检查或推断 |
| [在图（Graph）中使用参数](graph-usage.md) | 值读取、分量选择以及引脚调用形式 —— 附带引脚表 |
| [Path(…) 资源引用](path.md) | 每种根拼写、解析器及其诊断 |

<a id="notes"></a>

## 附注

- **每种类型的默认值都是可选的。** 没有 `= <default>` 时，标量为 `0`，向量在每个通道中为
  `0`（对于 2 分量或 3 分量 token，alpha 为 `1`），纹理为该维度下引擎的默认
  纹理 *(自 2.0.0 起；1.x 会将向量保留为节点自身的默认值)*。
  `Texture2DArray` 没有引擎默认值：其节点保留类的 2D 默认纹理，且没有
  诊断 *(自 2.0.0 起)*。请指定一个数组资产。
- **属性名是标识符** *(自 2.0.0 起)*。`float 1Bad = 0;` 是
  [`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105)（格式错误的数字）以及
  [`DSH3250`](../diagnostics/DSH3xxx.md#dsh3250)。
- **成为声明的属性在文件作用域内声明** *(自 2.0.0 起)*。同一名称的两个属性——无论是在同一个块中，
  还是在同一文件的两个块中——都会产生
  [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210)。名称按精确方式比较。1.x 中关于
  函数的属性不得与其某个输入同名这一规则，在 2.0 中没有对应检查。
- **声明顺序不约束读取。** `Graph` 可以读取声明在块中更靠后位置的
  属性。例外是 `UE.*` 属性：它是在每个读取它的函数体头部声明的局部变量，
  按 `Properties` 顺序排列，因此若某个参数命名了另一个 `UE.*` 属性，该属性需要在
  列表中更早出现并由同一个函数体读取；否则该名称就是未声明的
  ([`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200))。
- **`Properties` 在 `VirtualFunction` 内部含义不同**：在那里它是 `Inputs` 的同义词，
  采用类型化参数语法，而非本语法。请参见
  [VirtualFunction](../language/virtual-function.md)。
- 写入材质的参数名是所声明的名称，除非 `[ParameterName="…"]`
  覆盖它。请参见 [Metadata](metadata.md#parametername)。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| [`DSH3250`](../diagnostics/DSH3xxx.md#dsh3250) | 语句不以类型和名称开头、名称缺失，或属性后没有 `;`；仅包含一个 `[ … ]` 块的语句也会触发此项 |
| [`DSH3251`](../diagnostics/DSH3xxx.md#dsh3251) | `const` 后没有类型 |
| `DSH3252` | 类型 token 不是紧凑 token，不是 `*Parameter` token，且不以 `UE.` 开头 |
| `DSH3253` | `*Parameter` token 没有 2.0 写法，或在不能作为常量的 `*Parameter` token 上使用了 `const` |
| [`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254) | `=` 后没有默认值，或该 token 无法采用的默认值 |
| [`DSH3259`](../diagnostics/DSH3xxx.md#dsh3259) | `UE.*` 声明带有未闭合的 `(`，或其名称后带有默认值 |
| [`DSH3260`](../diagnostics/DSH3xxx.md#dsh3260) | `Group("") { … }` |
| `DSH4210` | 同一文件中有两个同名属性 |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 发射器无法创建参数节点（内部错误） |

元数据、默认值和资产引用诊断位于 [Metadata](metadata.md#diagnostics)、
[紧凑类型 token](compact-types.md#diagnostics) 和 [Path(…)](path.md#diagnostics)。完整
列表见[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_Parameters")
{
    Properties = {
        const float DebugScale = 1.0;                     // Constant, not overridable

        Group("Surface") {
            vec3  Tint      = vec3(1.0, 0.5, 0.2);        // VectorParameter, reads RGB
            float Roughness = 0.4 [Slider(0, 1)];         // ScalarParameter with a UI range
        }

        TextureSampleParameter2D BaseTex = Path(Game, "Textures/T_White") [
            SamplerType  = "LinearColor";
            MipValueMode = "None";
        ];

        StaticSwitchParameter UseDetail = true;

        UE.TexCoord(Index = 0) UV;                        // MaterialExpressionTextureCoordinate
    }

    Settings = { Domain = "Surface"; ShadingModel = "DefaultLit"; BlendMode = "Opaque"; }

    Outputs = {
        vec3  Color;
        float Rough;

        Base.BaseColor  = Color;
        Base.Roughness  = Rough;
    }

    Graph = {
        vec4 Sample = BaseTex(Coordinates = UV);
        vec3 Lit    = Sample.rgb * Tint * DebugScale;
        Color = UseDetail(True = Lit, False = Tint);
        Rough = Roughness;
    }
}
```

生成的节点：

```text
Constant              DebugScale        (const -> not a parameter)
VectorParameter       Tint              Group="Surface" SortPriority=0
ScalarParameter       Roughness         Group="Surface" SortPriority=10 SliderMin=0 SliderMax=1
TextureSampleParameter2D BaseTex        SamplerType=LinearColor
StaticSwitchParameter UseDetail
TextureCoordinate     UV                CoordinateIndex=0
```

<a id="see-also"></a>

## 另请参见

- [Properties（节）](../language/properties.md) — 节的语法、`const` 和 `Group(…)` 作用域
- [类型 token](../language/types.md) — 每个类型 token 在各上下文中的有效性矩阵
- [Inputs / Outputs / Results](../language/inputs-outputs.md) — 函数使用的*另一种*参数语法
- [Shader](../language/shader.md) — `Properties` 节所在的块
- [ShaderFunction](../language/shader-function.md) — 函数局部属性
- [UE 内置项](../builtins/ue.md) — 完整的 `UE.*` 目录
- [UE.Expression](../builtins/ue-expression.md) — 通用反射节点构建
- [Graph](../graph/index.md) — 消费这些参数的语言
- [2.0 语言](../language-v2/index.md) — `uniform`、`static const` 和 `///` 指令
- [诊断索引](../diagnostics/index.md) — 每个代码
