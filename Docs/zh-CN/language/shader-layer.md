---
title: "ShaderLayer / ShaderLayerBlend"
description: "ShaderLayer / ShaderLayerBlend"
---

<a id="shaderlayer--shaderlayerblend"></a>

# ShaderLayer / ShaderLayerBlend

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **ShaderLayer / ShaderLayerBlend**

两个顶层块，用于声明 Unreal 的材质层函数资产：层（layer）产生一个
`MaterialAttributes` 值，而层混合（layer blend）将其中两个组合起来。

| | |
| :-- | :-- |
| Declared in | `.dsf` 或 `.dsm`，旁边还有一个 `Shader`；在 `.dsh` 中，任一个都是 [`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249)（参见 [Source files](source-files.md#how-the-restriction-is-enforced)） |
| Kind | 顶层块 |
| Generates | `UMaterialFunctionMaterialLayer`（`ShaderLayer`）/ `UMaterialFunctionMaterialLayerBlend`（`ShaderLayerBlend`） |
| Multiplicity | 每个文件可任意数量 |
| Since | `1.3.0` |

旧版前端将 `ShaderLayer` 读取为 2.0 `/// @layer export void L(inout material m)`，将
`ShaderLayerBlend` 读取为 `/// @layerblend export void B(material …, inout material Result)`；参见
[the 2.0 language](../language-v2/index.md) 和 [`dsc migrate`](../tools/migrate.md)。

<a id="synopsis"></a>

## Synopsis

```c
ShaderLayer(Name = "<asset-path>" [, Root = "<root>"])
{
    [Properties            [=] { <property-declaration> ; … }]
    [Inputs                [=] { MaterialAttributes <name> ; }]
    { Outputs | Results }  [=] { MaterialAttributes <name> ; }
    Graph                  [=] { <graph-statement> … }
    [Settings              [=] { <key> = <value> ; … }]
    [Layout                [=] { { Node( … ) | Comment( … ) } ; … }]
}
```

```c
ShaderLayerBlend(Name = "<asset-path>" [, Root = "<root>"])
{
    [Properties            [=] { <property-declaration> ; … }]
    Inputs                 [=] { MaterialAttributes <name> ; MaterialAttributes <name> ; }
    { Outputs | Results }  [=] { MaterialAttributes <name> ; }
    Graph                  [=] { <graph-statement> … }
    [Settings              [=] { <key> = <value> ; … }]
    [Layout                [=] { { Node( … ) | Comment( … ) } ; … }]
}
```

节名称与其 `{ … }` 块之间的 `=` 是可选的语法糖 *(自 1.5.0 起)*；节闭合 `}` 之后的 `;` 是可选的。节可以以任意顺序出现，也可以重复出现。

两个关键字均以**区分大小写**的方式按完整单词匹配；节名称则以不区分大小写的方式匹配。

<a id="header-attributes"></a>

## 头部属性

| Attribute | Required | Value | Effect |
| :-- | :-- | :-- | :-- |
| **`Name`** | 是 | string | 资产的逻辑路径。最后一个由 `/` 分隔的段是资产名称；前面的段成为文件夹。 |
| `Root` | 否 | string | 文件夹所依附的包根。缺省或为空时默认为 `/Game`。 |
| `NoGeneration` | 否 | `true` \| `false` | *(2.1.2 之后)* 只有指明该文件的编译才会构建此块的资产；参见 [NoGeneration](../generation/no-generation.md)。另一个值会触发 [`DSH2260`](../diagnostics/DSH2xxx.md#dsh2260)。 |

属性键以不区分大小写的方式匹配。值可以带引号或不带引号；不带引号的值一直延续到括号外下一个
`,` 或 `)`。重复写入同一个键会产生警告
（[`DSH2244`](../diagnostics/DSH2xxx.md#dsh2244)），且后写的值生效。
`)` 之前的尾随逗号会被接受，与 1.x 一样 *(2.0.0 – 2.1.0 会以
[`DSH2243`](../diagnostics/DSH2xxx.md#dsh2243) 拒绝它)*。参见
[Asset paths](../generation/asset-paths.md)。

<a id="sections"></a>

## 节

两个块共用 [`ShaderFunction`](shader-function.md#sections) 主体解析器，因此节表完全相同。

| 节 | 是否接受 | 重复行为 | 参考 |
| :-- | :-- | :-- | :-- |
| `Properties` | 是 — 函数局部参数，`const` 和 `UE.*` 节点；层控制项就放在这里 | 追加 | [Properties](properties.md) |
| `Inputs` | 是 — 受约束，见下文 | 追加 | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Outputs` | 是 — 一个 `MaterialAttributes` 输出 | 追加 | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Results` | 是 — `Outputs` 的别名，无警告 | 追加 | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Settings` | 是 — `ShaderFunction` 认可的键 | 合并；重复写入同一个键会产生警告（[`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262)） | [Function settings](../../settings/function.md) |
| `Graph` | 是 | 后写的生效，并伴有警告（[`DSH2258`](../diagnostics/DSH2xxx.md#dsh2258)） | [Graph](../graph/index.md) |
| `Layout` | 是 | 后写的替换先写的，并伴有 `DSH2258` | [Layout](layout.md) |
| `Code` | **否** — [`DSH2246`](../diagnostics/DSH2xxx.md#dsh2246) | — | — |
| 其他任何内容，包括 `Options` | **否** — [`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245) | — | — |

<a id="interface-rules"></a>

## 接口规则

这些规则将层或混合与普通
[`ShaderFunction`](shader-function.md) 区分开来。*(自 2.0.0 起)* 它们是 `@layer`
和 `@layerblend` 的 2.0 签名规则，在文件绑定时检查。

| # | 适用于 | 规则 | Code |
| :-- | :-- | :-- | :-- |
| 1 | 两者 | 声明了一个 `MaterialAttributes` 输出 | [`DSH3278`](../diagnostics/DSH3xxx.md#dsh3278) |
| 2 | `ShaderLayer` | 材质是唯一存在的内容：任何不是 `MaterialAttributes` 的输入，以及除材质外的任何输出，都会破坏签名 | [`DSH6204`](../diagnostics/DSH6xxx.md#dsh6204) |
| 3 | `ShaderLayerBlend` | 至少一个 `MaterialAttributes` 输入，且除材质外没有输出 | [`DSH6205`](../diagnostics/DSH6xxx.md#dsh6205) |

层的材质贯穿其中：*(自 2.0.0 起)* 层始终恰好有一个
`MaterialAttributes` 输入，以其输出命名 — 即使 1.x 块完全没有声明任何输入也是如此。一个名称不同的 1.x `MaterialAttributes` 输入会成为该输入，引脚会改名并伴随警告（[`DSH3277`](../diagnostics/DSH3xxx.md#dsh3277)）；主体仍然以其 1.x 名称读取它。仅当 1.x 输入被声明为 `opt` 时，该输入才是可选的。

混合的输出一开始为空，其每个输入都是各自的引脚。绑定器接受从 1 开始的任意数量的
`MaterialAttributes` 输入；引擎的层堆栈恰好混合两个。

`MaterialAttributes` 是一个单词，匹配时忽略大小写；带空格的 `Material Attributes` 是
[`DSH3271`](../diagnostics/DSH3xxx.md#dsh3271) *(自 2.0.0 起)*。缺少 `Graph` 不会被报告。

> [!NOTE]
> 标量、向量和纹理不能作为层输入 — 规则 2 会拒绝它们。请改为通过
> `Properties` 暴露它们：它们会成为生成函数内部的参数节点，并出现在
> 层堆栈的参数面板上。对混合控制项也照此处理。

<a id="blend-input-relevance-since-ue-57"></a>

## 混合输入相关性 *(自 UE 5.7 起)*

在 UE 5.7 及更新版本上，`ShaderLayerBlend` 的每个 `MaterialAttributes` 输入都会根据其声明的名称获得一个
`BlendInputRelevance`。该名称通过移除空格、`_`
和 `-` 进行规范化，然后以不区分大小写的方式比较。

| 规范化后的输入名称 | `BlendInputRelevance` |
| :-- | :-- |
| `Top`、`TopLayer` | `Top` |
| `Bottom`、`BottomLayer`、`Base`、`BaseLayer` | `Bottom` |
| 其他任何内容 — 第一个 `MaterialAttributes` 输入 | `Bottom` |
| 其他任何内容 — 靠后的 `MaterialAttributes` 输入 | `Top` |

`ShaderLayer` 的输入，以及任何非 `MaterialAttributes` 输入，都不会获得相关性，并保留
引擎的 `General`。在 UE 5.3 – 5.6 上该属性不存在，不会写入任何内容。

<a id="generated-asset"></a>

## 生成的资产

`Name` + `Root` 按照
[Asset paths](../generation/asset-paths.md) 中所述的方式精确解析为包路径。

| 块 | 创建的 UClass | `EMaterialFunctionUsage` |
| :-- | :-- | :-- |
| `ShaderLayer` | `UMaterialFunctionMaterialLayer` | `MaterialLayer` |
| `ShaderLayerBlend` | `UMaterialFunctionMaterialLayerBlend` | `MaterialLayerBlend` |

当目标路径上已存在资产时，如果它**是**预期类的子类，则会被复用；适用于
[`ShaderFunction`](shader-function.md) 的精确类规则在这里被放宽。
因此，在未移动或删除现有资产的情况下将块在 `ShaderFunction` 和 `ShaderLayer` 之间切换会失败，并报
[`DSH8110`](../diagnostics/DSH8xxx.md#dsh8110)。

其余关于生成的内容——受支持的 `Settings` 键、属性节点的惰性创建、重新生成过程中输入/输出 `Id` 的保留、源哈希跳过，以及哪些因素会阻止重新构建——均与 [`ShaderFunction`](shader-function.md#generated-asset) 完全相同。

混合的 `MaterialAttributes` 输出最初是一个空材质。图层的输出最初是通过其输入传入的材质——或者当 1.x 输入使用了其他名称时为空（参见[接口规则](#interface-rules)）。无论哪种情况，在 `Graph` 中诸如 `Attrs.BaseColor = …;` 这样的成员写入都是合法的。参见 [MaterialAttributes](../graph/material-attributes.md)。

<a id="deprecated-spellings"></a>

## 已弃用的拼写

> [!WARNING]
> `MaterialLayer(...)` 和 `MaterialLayerBlend(...)` **自 1.3.0 起已弃用**。两者仍可解析，
> 并生成与其现代拼写完全相同的资源，且各自都会产生一条警告
> ([`DSH2251`](../diagnostics/DSH2xxx.md#dsh2251))。在新代码中请使用 `ShaderLayer` / `ShaderLayerBlend`。

| 已弃用的拼写 | 替代 | 行为 |
| :-- | :-- | :-- |
| `MaterialLayer(Name = …)` | `ShaderLayer(Name = …)` | 完全相同：相同的节、相同的规则、相同的资源 |
| `MaterialLayerBlend(Name = …)` | `ShaderLayerBlend(Name = …)` | 完全相同 |

缺失的 `Name` ([`DSH2242`](../diagnostics/DSH2xxx.md#dsh2242)) 会指明作者
实际输入的拼写。

<a id="notes"></a>

## 说明

- **编译一个文件会构建它声明的每个资源**：`.dsm` 或 `.dsf` 的每个图层、混合和 `ShaderFunction`，
  以及 `.dsm` 的 `Shader` *(2.0.0 – 2.1.0 拒绝在同一图层
  或混合旁放置带有 `DSH6201` 的 `Shader`)*。参见[源文件](source-files.md)。
- **文件类型限制是一种解析，按块判定** *(自 2.0.0 起)*。包含这四种
  拼写中任何一种的 `.dsh` 都是 `DSH2249`；提及 `ShaderLayer(` 的注释或字符串则没有问题。
- 图层或混合的使用者是材质或材质实例上的 Unreal 材质图层堆栈。*(自 2.0.0 起)* 两者都无法从 `Graph` 中调用：
  [`DSH6208`](../diagnostics/DSH6xxx.md#dsh6208)。
- `Code` 节会被直接拒绝（`DSH2246`）；请使用 `Graph`。

<a id="diagnostics"></a>

## 诊断

每个代码都带有该构造的行和列；代码页面中包含相应消息。共享正文解析器的
解析时代码——属性、节、参数语句、元数据、
`Settings`——列于 [`ShaderFunction` § 诊断](shader-function.md#diagnostics)。

| 代码 | 触发时机 |
| :-- | :-- |
| `DSH2251` | `MaterialLayer` 或 `MaterialLayerBlend`（警告） |
| `DSH2242` | 没有 `Name`，或其为空 |
| `DSH3278` | 没有 `MaterialAttributes` 输出 |
| `DSH3277` | 图层的 `MaterialAttributes` 输入在输出之后被重命名（警告） |
| `DSH6204` | 带有任何其他输入或输出的 `ShaderLayer` |
| `DSH6205` | 不带 `MaterialAttributes` 输入、或带有其他输出的 `ShaderLayerBlend` |
| `DSH6208` | `Graph` 调用了图层或混合 |
| `DSH8110` | 目标路径处存在不兼容类的资源 |

资源本身的发出时代码——[`DSH8111`](../diagnostics/DSH8xxx.md#dsh8111)、
[`DSH8112`](../diagnostics/DSH8xxx.md#dsh8112)、[`DSH8114`](../diagnostics/DSH8xxx.md#dsh8114)、
[`DSH8206`](../diagnostics/DSH8xxx.md#dsh8206)、[`DSH8207`](../diagnostics/DSH8xxx.md#dsh8207)——
是 `ShaderFunction` 的。

成功时，编译输出会有一行 `Generated <Kind> <AssetPath> from <SourceFile>.`，其中 `<Kind>`
是 `MaterialLayer` 或 `MaterialLayerBlend`。

完整的跨阶段列表见[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
ShaderLayer(Name="Layers/L_Rust", Root="Game")
{
    Properties = {
        Group("Rust") {
            vec3  RustColor = vec3(0.35, 0.13, 0.05) [Description="Base colour of the rust"];
            float RustRough = 0.85                   [Slider(0, 1)];
        }
    }

    Outputs = {
        MaterialAttributes Attrs;
    }

    Graph = {
        Attrs.BaseColor = RustColor;
        Attrs.Roughness = RustRough;
        Attrs.Metallic  = 0.0;
    }
}

ShaderLayerBlend(Name="Layers/LB_Wear", Root="Game")
{
    Properties = {
        float WearAmount = 0.5 [Slider(0, 1); Description="0 keeps Bottom, 1 keeps Top"];
    }

    Inputs = {
        MaterialAttributes Bottom;
        MaterialAttributes Top;
    }

    Outputs = {
        MaterialAttributes Attrs;
    }

    Graph = {
        Attrs.BaseColor = lerp(Bottom.BaseColor, Top.BaseColor, WearAmount);
        Attrs.Roughness = lerp(Bottom.Roughness, Top.Roughness, WearAmount);
        Attrs.Normal    = lerp(Bottom.Normal,    Top.Normal,    WearAmount);
    }
}
```

生成的资源：

```text
package     /Game/Layers/L_Rust
object path /Game/Layers/L_Rust.L_Rust
class       UMaterialFunctionMaterialLayer        (usage: MaterialLayer)
            in  Attrs   MaterialAttributes   (the material the layer is applied to; required, no `opt` input in the source)
            out Attrs   MaterialAttributes

package     /Game/Layers/LB_Wear
object path /Game/Layers/LB_Wear.LB_Wear
class       UMaterialFunctionMaterialLayerBlend   (usage: MaterialLayerBlend)
            in  Bottom  MaterialAttributes   (BlendInputRelevance: Bottom, UE 5.7+)
            in  Top     MaterialAttributes   (BlendInputRelevance: Top,    UE 5.7+)
            out Attrs   MaterialAttributes
parameters  WearAmount  ScalarParameter, slider 0..1
```

<a id="see-also"></a>

## 另请参见

- [ShaderFunction](shader-function.md) — 这两个块所特化的普通 `UMaterialFunction` 块
- [Shader](shader.md) — 生成 `UMaterial` 的顶层块
- [VirtualFunction](virtual-function.md) — 声明一个现有的 `UMaterialFunction` 而不是生成它
- [源文件](source-files.md) — `.dsm` / `.dsh` / `.dsf` 各自可以包含哪些块类型
- [关键词](keywords.md) — 完整的关键词索引，包括已弃用的拼写
- [属性](properties.md) — `Properties` 节语法、`Group(…)` 作用域和 `const`
- [输入 / 输出 / 结果](inputs-outputs.md) — 完整的类型化参数语法
- [MaterialAttributes](../graph/material-attributes.md) — 这些块所围绕构建的值类型
- [函数设置](../../settings/function.md) — 材质函数 `Settings` 所支持的键
- [资源路径](../generation/asset-paths.md) — `Name=` + `Root=` → 包路径
- [重新生成](../generation/regeneration.md) — 重新构建时哪些内容会保留、哪些不会
- [诊断索引](../diagnostics/index.md) — 按阶段列出的每个代码
