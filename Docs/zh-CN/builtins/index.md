---
title: "内置项"
description: "内置项"
---

<a id="builtins"></a>

# 内置项

> [DreamShader](../index.md) » **内置项**

DreamShaderLang 自行解析而不在作者声明中查找的名称：
`UE.*` 命名空间、`Substrate.*` 命名空间、保留的 `SampleTexture2D` 形式，以及数学
内置函数集。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf`、`.dsh` |
| 类型 | 章节中心 |
| 生成 | 每次调用生成一个或多个 `UMaterialExpression` 节点 |

<a id="the-call-surfaces"></a>

## 调用表面

自 2.0.0 起，这些表面共享同一实现。`UE.` 或 `Substrate.` 调用会绑定到
**内置目录**——通过反射读取运行中引擎的材质表达式类
（`dsc export-catalog` 将其写出）——而数学内置函数是编译器的核心操作之一。
各行的差异仍在于 **旧版前端** 在绑定器看到 1.x 写法之前对其所做的处理。

| # | 表面 | 写法 | 位置 | 旧版前端首先做什么 | 参考 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| 1 | 1.x `UE.*` 名称 | `UE.TexCoord(Index = 0)` | `Graph` | 保留 27 个名称各自的 1.x 参数列表，用 [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254) 丢弃所有其他参数；重写 `UE.SceneTexture`、`UE.StaticSwitchParameter`、`UE.TranslatedWorldPosition` | [`UE.*` 目录](ue.md) |
| 2 | 任意节点，按类 | `UE.Expression(Class = "Sine", …)` 或 `UE.Sine(…)` | `Graph` | 从调用中取出 `OutputType` / `ResultType`（宽度提示；在 Custom 节点上保留） | [`UE.Expression`](ue-expression.md) |
| 3 | 属性声明 | `UE.TexCoord(Index = 0) UV;` | `Properties` | 在每个读取它的 `Graph` 体开头声明一个用该调用初始化的局部变量 | [`UE.*` 目录](ue.md#properties-declaration-form) |
| 4 | 数学内置函数 | `pow(a, b)` | `Graph` | 无 | [数学内置函数](math.md) |
| 5 | `Substrate.*` | `Substrate.Slab(BaseColor = c)` | `Graph`、`Outputs` | 丢弃 `OutputType` / `ResultType` | [Substrate](substrate.md) |

表面 3 是一种*声明*，而非表达式；从它声明的局部变量来看，该调用是普通的
`UE.` 调用。表面 1、2、4 和 5 是在 `Graph` 体内求值的表达式。

> [!IMPORTANT]
> 表面 1 和 3 共享内置名称，但不共享参数列表。`UE.TexCoord` 在 `Graph` 中保留
> `Index` 并用 `DSH5254` 丢弃 `CoordinateIndex`；作为属性声明时两者都接受，因为此处不适用
> 1.x 过滤器。所有差异均列于
> [`UE.*` 目录 § 属性声明形式](ue.md#properties-declaration-form)。

<a id="the-reserved-sampletexture2d-form"></a>

### 保留的 `SampleTexture2D` 形式

`SampleTexture2D(<texture-object>, <uv>)` 不在任何命名空间中，但属于同一集群。它
是一个保留名称，由旧版前端在查找用户属性和函数之前重写为

```c
UE.Expression(Class = "TextureSample", TextureObject = <arg0>, Coordinates = <arg1>)
```

——即整个样本，在需要 `float4` 的地方使用 `RGBA`。它是唯一**区分大小写**匹配的内置调用形式：`sampletexture2d(t, uv)` 不是它。它恰好接受两个位置参数
（否则会报 [`DSH5256`](../diagnostics/DSH5xxx.md#dsh5256)）。参见
[`UE.Expression`](ue-expression.md#sampletexture2d)。

<a id="dispatch-order-inside-a-graph"></a>

## `Graph` 内的分派顺序

调用按以下顺序解析；首个匹配者胜出。完整查找见
[名称解析](../graph/name-resolution.md)。

| 顺序 | 对被调用者的测试 | 结果 |
| :-- | :-- | :-- |
| 1 | 在 1.x 源中，读取调用时：`Path(…)`、已声明参数节点属性的调用、`SampleTexture2D`（区分**大小写**）、`UE.SceneTexture`、`UE.StaticSwitchParameter`、`UE.TranslatedWorldPosition`，以及其他 `UE.` 名称的 1.x 参数列表 | 重写为其 2.0 形态 |
| 2 | 是类型名称 | [构造函数](../graph/constructors.md) |
| 3 | `UE.<Name>` 或 `Substrate.<Name>` | 目录中的节点——[`UE.Expression`](ue-expression.md) |
| 4 | 纹理的 `.Sample(…)` / `.SampleLevel(…)`、`Texture2DSample`、`Texture2DSampleLevel` | 纹理采样 |
| 5 | 数学内置函数名称，然后是 GLSL 写法（在 1.x 源中为 `DSH5277`） | [数学内置函数](math.md) |
| 6 | 此文件声明或导入的函数 | [调用](../graph/calls.md) |
| 7 | 仅在 1.x 源中：忽略大小写匹配的数学内置函数或函数 | 同 5 或 6，附 [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275) |

在步骤 3 中，前缀之后的名称会在该前缀的命名空间中的目录内查找：

| 顺序 | 匹配 | 结果 |
| :-- | :-- | :-- |
| 1 | `Class = "…"`，如果调用带有该参数 | 它命名的类——[类解析](ue-expression.md#class-resolution) |
| 2 | 某个类的目录短名称——`Sine`、`TextureCoordinate`、`SubstrateSlabBSDF` | 该类 |
| 3 | 目录携带的别名——`TexCoord`、`ViewportUV`、`Slab`、`HorizontalMix`、…… | 该类 |
| 4 | 仅在 1.x 源中：忽略大小写匹配的唯一名称或别名 | 该类，附 [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276) |
| 5 | 无 | [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210)，或者对于后续引擎才有的节点为 `DSH5294` / `DSH5300` |

`UE.Expression` 不命名任何类，因此没有 `Class` 时它是
[`DSH5218`](../diagnostics/DSH5xxx.md#dsh5218)。

<a id="name-and-argument-matching"></a>

## 名称与参数匹配

| 元素 | 规则 |
| :-- | :-- |
| `UE.` 前缀 | 精确*(自 2.0.0 起)*——`ue.X` 是未知变量 `ue`（[`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200)）；名为 `UE` 的变量会隐藏该命名空间 |
| `Substrate.` 前缀 | 在 1.x 源中任意大小写——它是 1.x 类型写法 |
| 内置名称 | 精确；在 1.x 源中唯一的不区分大小写匹配，附 `DSH5276` |
| `SampleTexture2D` | 区分**大小写** |
| 数学内置函数名称 | 精确；在 1.x 源中不区分大小写，附 `DSH5275`——参见 [数学内置函数](math.md#name-resolution) |
| 参数名称 | 精确，或目录携带的别名；在 1.x 源中唯一的不区分大小写匹配，附 `DSH5276`。无其他规范化——`_`、`-` 和空格**不会**被移除 |
| `Class` | 恰为此写法 |
| `Output`、`OutputName`、`OutputIndex`、`OutputType`、`ResultType` | 任意大小写——旧版前端将它们从调用中取出 |
| 传递给属性的枚举值 | 值的名称去掉其前缀，精确匹配；在 1.x 源中忽略大小写、空格、`_`、`-`、`:`、`.`、`/`、前缀以及 `Enum::` 作用域，附 [`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278) |
| 未加引号的文本参数 | 在任何接受带引号字符串的地方都接受裸词或点分名称——`Class = Sine` 等于 `Class = "Sine"` |

目录给出了参数顺序的类接受位置参数——
`TextureCoordinate`、`Constant` … `Constant4Vector`、`Transform`、`TransformPosition`、`Panner`、
`ComponentMask`、`Time` 以及五个 Substrate 合成节点——在其他位置则会被拒绝并产生
[`DSH5220`](../diagnostics/DSH5xxx.md#dsh5220) *(自 2.0.0 起)*。在 27 个 1.x 名称上，旧版前端会优先丢弃它们，
但 `UE.TransformVector` 和 `UE.TransformPosition` 的位置 0（`Input`）除外；
`UE.StaticSwitchParameter` 在位置 0 读取 `True`，在位置 1 读取 `False`，而 `SampleTexture2D`
以位置方式接受其两个参数。[数学内置项](math.md)按位置接受参数，
或按节点引脚命名接受参数 *(自 2.0.0 起)*。

> [!WARNING]
> **在 27 个 1.x 名称上，该名称未读取的参数会被丢弃——现在会给出警告。** 旧版
> 前端保留 1.x 参数列表，并以 `DSH5254` 丢弃其他任何内容
> *(自 2.0.0 起；1.x 会无声丢弃)*。`UE.Time(Bogus = 1)` 可以编译并会说明这一点；
> `UE.TexCoord(0)` 会产生 UV 通道 0，因为位置参数被丢弃，而
> `CoordinateIndex` 保留其节点默认值。其他所有调用都会根据目录
> 逐参数绑定，不属于任何引脚或属性的名称会引发 [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213)——
> 或者在 1.x 源中，为已构建节点保留并产生 [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291)。

<a id="pages"></a>

## 页面

| 页面 | 内容 |
| :-- | :-- |
| [`UE.*` 目录](ue.md) | 27 个 1.x 名称、它们的节点类、参数列表和输出；特殊处理的内置项；属性声明形式 |
| [`UE.Expression`](ue-expression.md) | 类解析、参数绑定、属性值、输出选择、Custom 节点 |
| [`OutputType`](output-type.md) | `OutputType` / `ResultType` 在 1.x 源中仍然做什么，以及输出名称 |
| [数学内置项](math.md) | `pow`、`min`、`max`、`fmod`、`frac` 以及其他 |
| [Transform 基础值](transform.md) | `UE.TransformVector` 和 `UE.TransformPosition` 的基础值 |
| [Substrate](substrate.md) | `Substrate.*` 节点目录 |
| [HLSL 库](hlsl-library.md) | `Shaders/DreamShaderBuiltins.ush` |
| [Custom Pass 节点](dream-pass.md) | `UE.DreamPassOutput` 和 `UE.DreamPassBuffer`，DreamShaderPass 模块（UE 5.8）的反射表达式 |

<a id="notes"></a>

## 说明

- **在任何界面上，相同的调用都只对应一个节点。** IR 会合并具有相同类、
  属性和输入的节点 *(自 2.0.0 起；1.x 仅为泛型调用和 `Substrate.*` 调用
  以及数学内置项复用节点，从不为 Custom 节点复用)*。请参阅[节点复用](../graph/node-reuse.md)。
- 节点落在画布上的位置由布局处理决定。请参阅
  [图表布局](../generation/graph-layout.md)。
- 在 [`GraphFunction`](../language/graph-function.md) 主体内编写的 `UE.*` 调用会被提升出
  HLSL，并成为生成的 Custom 节点上的输入引脚。本页上的参数规则
  仍然适用于该调用本身。
- `Substrate.*` 命名空间包含正在运行的引擎的 Substrate 类，在
  UE 5.4 之前没有任何此类；在那里调用会引发 `DSH5210`。请参阅 [Substrate](substrate.md#availability)。
- 内置项目录是引擎的反射类，包括已加载插件的类。不属于内置项、已声明属性、
  `Graph` 变量或已声明函数的名称属于
  [未知标识符](../graph/name-resolution.md)。
- `.dss` 写入与 `UE.<ClassShortName>(Pin = …, Property = …)` 相同的节点；`dsc migrate` 以这种方式写入
  每个 1.x 调用。请参阅 [`dsc migrate`](../tools/migrate.md) 和
  [DreamShaderLang 2.0](../language-v2/index.md)。

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_Surfaces")
{
    Properties {
        Texture2D BaseTex = Path(Game, "Textures/T_Noise");
        float     Speed   = 0.2;
    }

    Settings { ShadingModel = "Unlit"; }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        // 1 — 1.x UE.* names
        float2 uv = UE.Panner(Coordinate = UE.TexCoord(Index = 0), Speed = Speed);

        // 2 — any node by its class name; OutputType is optional since 2.0.0
        float pulse = UE.Sine(OutputType = "float1", Input = UE.Time());

        // the reserved two-argument sample form
        float4 tex = SampleTexture2D(BaseTex, uv);

        // 4 — math builtin
        float k = pow(pulse, 2.0);

        Color = tex.rgb * k;
    }
}
```

<a id="see-also"></a>

## 另请参阅

- [`UE.*` 目录](ue.md) — 每个 1.x 名称各占一项
- [`UE.Expression`](ue-expression.md) — 按类查找任意节点
- [`OutputType`](output-type.md) — 1.x 需要的、2.0 作为提示读取的参数
- [Substrate](substrate.md) — `Substrate.*` 的同类命名空间
- [Custom Pass 节点](dream-pass.md) — `.dsp` 管线材质使用的两个表达式
- [数学内置项](math.md) — 标量/向量数学集合
- [调用](../graph/calls.md) — 调用语法、命名参数和输出参数
- [名称解析](../graph/name-resolution.md) — 被调用方经历的名称查找顺序
- [节点复用](../graph/node-reuse.md) — 相同的调用与节点
- [属性](../language/properties.md) — 声明形式所在的章节
- [`Path(…)`](../parameters/path.md) — 资产引用语法
- [诊断索引](../diagnostics/index.md) — 每个代码
