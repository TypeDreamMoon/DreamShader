---
title: "Shader"
description: "Shader"
---

<a id="shader"></a>

# Shader

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Shader**

一个顶层块，用于声明一个 Unreal 材质资产：其参数、渲染状态设置、输出绑定以及为它们提供输入的节点图。

| | |
| :-- | :-- |
| 声明于 | `.dsm` — 包含它的 `.dsh` 是一个 [`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249)（参见[源文件](source-files.md#how-the-restriction-is-enforced)） |
| 类型 | 顶层块 |
| 生成 | `UMaterial`（Graph 后端）或 `UDreamShaderMaterialInstance` + 一个隐藏的 `UMaterial` 基础（ThinCustom 后端） |
| 多重性 | 每个文件最多一个（参见[备注](#notes)） |

<a id="synopsis"></a>

## 概要

```c
Shader(Name = "<asset-path>" [, Root = "<root>"])
{
    [Properties [=] { <property-declaration> ; … }]
    [Settings   [=] { <key> = <value> ; … }]
    [Outputs    [=] { { <output-declaration> ; | <output-binding> ; | Expression( … ) { Pin[i] = <source> ; … } } … }]
    [Graph      [=] { <graph-statement> … }]
    [Layout     [=] { { Node( … ) | Comment( … ) } ; … }]
}
```

节名与其 `{ … }` 块之间的 `=` 是可选的语法糖 *(自 1.5.0 起)*；节结束 `}` 之后的 `;` 是可选的。各节可以以任意顺序出现，并且可以重复。

关键字 `Shader` 采用**区分大小写**匹配；节名采用不区分大小写匹配。参见[词法元素](lexical.md#case-sensitivity)。

一个 `.dss` 写入的材质与 `#pragma material(...)`、`uniform` 和一个 `export void <Name>(inout material m)` 相同 — 参见 [DreamShaderLang 2.0](../language-v2/index.md)；[`dsc migrate`](../tools/migrate.md) 会按该方式重写一个 `.dsm`。

<a id="header-attributes"></a>

## 头部属性

| 属性 | 必需 | 值 | 效果 |
| :-- | :-- | :-- | :-- |
| **`Name`** | 是 | 字符串 | 资产的逻辑路径。最后一个由 `/` 分隔的段是资产名称；前面的段成为文件夹。 |
| `Root` | 否 | 字符串 | 文件夹所依附的包根目录。缺省或为空时默认为 `/Game`。 |
| `NoGeneration` | 否 | `true` \| `false` | *(2.1.2 之后)* 只有指定该文件名的编译才会构建此块的资产；参见 [NoGeneration](../generation/no-generation.md)。另一个值是 [`DSH2260`](../diagnostics/DSH2xxx.md#dsh2260)。 |

属性键采用不区分大小写匹配（`name=` 也可以）。值可以带引号或不带引号；不带引号的值在第一个 `,` 或 `)` 处结束。重复的键保留后一个值，并给出警告 [`DSH2244`](../diagnostics/DSH2xxx.md#dsh2244) *(自 2.0.0 起；之前为静默)*。`)` 之前的尾随逗号可被接受，与 1.x 相同 *(2.0.0 – 2.1.0 曾以 [`DSH2243`](../diagnostics/DSH2xxx.md#dsh2243) 拒绝它)*。

完整的 `Name` / `Root` 语法、可接受的根目录拼写以及最终生成的磁盘路径在[资产路径](../generation/asset-paths.md)中说明。

<a id="sections"></a>

## 节

| 节 | 可接受 | 重复行为 | 参考 |
| :-- | :-- | :-- | :-- |
| `Properties` | 是 | 追加 | [属性](properties.md) |
| `Settings` | 是 | 合并；后一个键胜出，重复写入同一个键会给出警告 [`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262) | [着色器设置](../settings/material.md) |
| `Outputs` | 是 | 追加 | [输出绑定](output-bindings.md) |
| `Graph` | 是 | 后一个主体胜出，并给出警告 [`DSH2258`](../diagnostics/DSH2xxx.md#dsh2258) | [图](../graph/index.md) |
| `Layout` | 是 | **重置** — 第二个 `Layout` 会丢弃第一个，并给出警告 `DSH2258` | [布局](layout.md) |
| `Code` | **否** — [`DSH2246`](../diagnostics/DSH2xxx.md#dsh2246) | — | — |
| `Inputs` | 否 — 未知节，[`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245) | — | — |
| `Results` | 否 — 未知节，`DSH2245` | — | — |
| `Options` | 否 — 未知节，`DSH2245` | — | — |

`Shader` 中的 `Properties` 声明参数、`const` 和 `UE.*` 内置节点。这*不是* `Properties` 在 [`VirtualFunction`](virtual-function.md) 内部所使用的同一语法，在那里它是 `Inputs` 的同义词。

<a id="the-outputs--graph-relationship"></a>

## `Outputs` / `Graph` 关系

`Outputs` 包含两种语句，按语句进行消歧：

| 语句形式 | 含义 |
| :-- | :-- |
| `<type> <name> ;` | 输出变量声明；`Graph` 必须为其赋值 |
| `<type> <name> = <expression> ;` | 带初始化的输出声明 *(自 1.3.4 起)* |
| `<target> = <variable> ;` | 输出绑定 — `Base.<Property>` 或 `Expression( … ).Pin[<i>]` |
| `Expression( … ) { Pin[<i>] = <variable> ; … }` | [块形式](output-bindings.md#block-form) — 在一个终端节点上有多个引脚 *(自 1.9.0 起)* |

决定 `Graph` 是否必需的规则在整个块读取完毕后进行求值：

- 一个没有 `Graph` 节且 `Outputs` 为空的 `Shader` 是 [`DSH2255`](../diagnostics/DSH2xxx.md#dsh2255)。没有 `Graph` 时，`Outputs` 语句必须自行计算这些值 — 带初始化器的输出声明，或者 *(自 2.0.0 起)* 右侧为表达式的绑定，例如 `Base.FrontMaterial = Substrate.Layer(Coat, Body);`。
- 空的 `Graph = { }` 是合法的。
- 绑定才是真正连接材质的部分。没有 `Outputs` 节的 `Shader` 会给出警告 [`DSH2256`](../diagnostics/DSH2xxx.md#dsh2256)：材质可以构建，但没有任何连线 *(自 2.0.0 起；到 1.9.x 为止则生成会失败)*。

```c
// Legal: no Graph body needed, the output declaration carries its own initializer.
Shader(Name="M_Flat")
{
    Properties = { vec3 Tint = vec3(1.0, 0.2, 0.2); }
    Outputs = {
        vec3 Color = Tint;
        Base.EmissiveColor = Color;
    }
    Graph = { }
}
```

<a id="generated-asset"></a>

## 生成的资产

`Name` + `Root` 解析为包路径的方式与[资产路径](../generation/asset-paths.md)中所述完全一致。最终落在那里的是哪个 UClass 取决于解析出的后端：

| 后端 | 写入的资产 | 备注 |
| :-- | :-- | :-- |
| `Graph` | `UMaterial` | 节点图直接构建在材质上 |
| `ThinCustom` *(自 1.5.0 起)* | `UDreamShaderMaterialInstance`，其父级是一个名为 `MB_DreamThinBase_<leaf>` 的隐藏 `UMaterial` 子对象 | 一个资产，一个包；你在实例上设置的参数覆盖由你保留 — 重建会捕获它们，并恢复新基础仍然声明的每个名称 *(自 1.9.0 起)*，参见[分歧](../generation/divergence.md#parameter-overrides-on-a-generated-thincustom-instance) |
| `Instance` | `ThinCustom` 的别名 *(自 1.5.0 起弃用)* | |

后端来自 `Settings = { Backend = "…"; }`（如果存在），否则来自项目的 **Default Compiler Backend** 设置。参见[后端](../settings/backend.md)和[项目设置](../settings/project.md)。

> [!NOTE]
> `Graph` 后端的 `UMaterial` 是一个普通资产，并在每次成功构建时保存 *(自 2.0.0 起)*。`ThinCustom` 产物会保持**临时（Ephemeral）**状态 — 在内存中，磁盘上没有包 — 直到某次 cook、显式 *Materialize* 或子实例赋予它一个包；而一旦它有了包，就会保留在磁盘上。参见[临时材质](../generation/ephemeral.md)。

> [!WARNING]
> 重新生成会清除目标图。未被 [`Layout`](layout.md) 固定的节点位置、添加的节点、节点属性调整，以及文本以 `DreamShader: ` 开头的注释框都会被销毁。只有**不**带 `DreamShader: ` 前缀的注释框会保留下来。在 ThinCustom 后端下，实例的参数覆盖也会被清除，但会先被捕获并在之后恢复 *(自 1.9.0 起)* — 源不再声明的参数是唯一会丢失其值的情况。参见[重新生成](../generation/regeneration.md)。

<a id="notes"></a>

## 备注

- **每个文件最多一个 `Shader`**（[`DSH2250`](../diagnostics/DSH2xxx.md#dsh2250)）。在 1.9.x 之前，导入会在解析前内联到一个文本中，因此该规则会跨越导入闭包；现在头文件会单独读取，因此不能包含 `Shader`（[`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249)），所以 [`import`](import.md) 永远不会添加一个。`.dsh` 检查读取的是 token，而不是文本：提及 `Shader(` 的注释没有问题。请参阅[源文件](source-files.md#how-the-restriction-is-enforced)。
- **没有属性的 `Shader()` 是 [`DSH2242`](../diagnostics/DSH2xxx.md#dsh2242)**，缺少的是 `Name`。
- `Shader` 可以与 `VirtualFunction`、`Function`、`GraphFunction` 和 `Namespace` 块共享一个 `.dsm`，这些块是材质的辅助块，并且也可以与 `ShaderFunction`、`ShaderLayer` 和 `ShaderLayerBlend` 块共享：对该文件的一次编译会将它们全部构建出来，每个都构建为各自的资源，并且材质可以按名称调用这样的函数 *（2.0.0 – 2.1.0 曾以 [`DSH6201`](../diagnostics/DSH6xxx.md#dsh6201) 拒绝这样做）*。`.dsf` 中的 `Shader` 是 [`DSH2259`](../diagnostics/DSH2xxx.md#dsh2259)。
- 未声明任何 `Shader` 块的 `.dsm` 仍然可以编译——它只会生成其声明的那些函数资源。
- 绑定 `Base.MaterialAttributes` 会开启材质上的 *Use Material Attributes*。有关 `Base.FrontMaterial` 和其他目标，请参阅[输出绑定](output-bindings.md)。

<a id="diagnostics"></a>

## 诊断

每个阶段都会报告其所有错误，每个错误都在各自的行和列 *（自 2.0.0 起）*。

<a id="parse-time"></a>

### 解析时

| 代码 | 触发条件 |
| :-- | :-- |
| [`DSH2241`](../diagnostics/DSH2xxx.md#dsh2241) | `Shader` 后面没有其属性列表 |
| [`DSH2242`](../diagnostics/DSH2xxx.md#dsh2242) | 属性列表没有 `Name`，或该列表为空 |
| [`DSH2243`](../diagnostics/DSH2xxx.md#dsh2243) | 格式错误的属性列表——缺少键、`=` 或值，缺少 `,` 或 `)` |
| [`DSH2244`](../diagnostics/DSH2xxx.md#dsh2244) | *（警告）* 一个属性被写了两次 |
| [`DSH2250`](../diagnostics/DSH2xxx.md#dsh2250) | 文件中出现第二个 `Shader` 块 |
| [`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249) | `.dsh` 中出现 `Shader` 块 |
| [`DSH2257`](../diagnostics/DSH2xxx.md#dsh2257) | 正文 `{`、节名称或节的 `{` 缺失 |
| [`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245) | 出现 `Properties`、`Settings`、`Outputs`、`Graph` 和 `Layout` 以外的节 |
| [`DSH2246`](../diagnostics/DSH2xxx.md#dsh2246) | 出现 `Code` 节 |
| [`DSH2258`](../diagnostics/DSH2xxx.md#dsh2258) | *（警告）* `Graph` 或 `Layout` 节被写了两次 |
| [`DSH2255`](../diagnostics/DSH2xxx.md#dsh2255) | 没有 `Graph` 节，且 `Outputs` 为空 |
| [`DSH2256`](../diagnostics/DSH2xxx.md#dsh2256) | *（警告）* 没有 `Outputs` 节 |
| [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) | 文件在该块内结束 |

<a id="bind-and-generation-time"></a>

### 绑定与生成时

| 代码 | 触发条件 |
| :-- | :-- |
| [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210) | 一个属性、一个函数或另一个名称被声明了两次 |
| [`DSH7202`](../diagnostics/DSH7xxx.md#dsh7202) | `Settings = { Backend = … }` 未指定任何后端；`Instance` 被读取为 `ThinCustom`，并带有警告 [`DSH7204`](../diagnostics/DSH7xxx.md#dsh7204) |
| [`DSH8200`](../diagnostics/DSH8xxx.md#dsh8200) | `Name` / `Root` 未解析为有效的资源路径 |
| [`DSH8201`](../diagnostics/DSH8xxx.md#dsh8201) | 无法创建或复用材质——目标路径中存放着另一个类，或存放着 DreamShader 未生成的资源；原因会附加在后面 |
| [`DSH8203`](../diagnostics/DSH8xxx.md#dsh8203) | 对于 ThinCustom 实例，情况相同 |
| [`DSH8206`](../diagnostics/DSH8xxx.md#dsh8206) | 该资源已在资源编辑器中打开，因此未被重新构建 |
| [`DSH8207`](../diagnostics/DSH8xxx.md#dsh8207) | 该资源自生成以来被手动编辑过，因此未被重新构建；其携带的原因是 `DSH8115` 文本——请参阅[差异](../generation/divergence.md) |

`Properties`、`Settings`、`Outputs` 和 `Graph` 的消息位于各自的页面。所有代码：[诊断](../diagnostics/README.md)。

<a id="example"></a>

## 示例

```c
Shader(Name="Materials/M_Emissive", Root="Game")
{
    Properties = {
        Group("Look") {
            vec3  Tint      = vec3(1.0, 0.4, 0.1) [Description="Emissive tint"];
            float Intensity = 2.0                 [Slider(0, 10)];
        }
        TextureSampleParameter2D BaseTex = Path(Game, "Textures/T_Noise");
    }

    Settings = {
        ShadingModel = "Unlit";
        BlendMode    = "Translucent";
        TwoSided     = true;
    }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph = {
        vec2 UV  = UE.TexCoord(Index = 0);
        vec4 Tex = BaseTex(Coordinates = UV);
        Color = Tex.rgb * Tint * Intensity;
        Alpha = Tex.a;
    }

    Layout = {
        Comment(Name="Sampling", X=-1200, Y=-200, W=900, H=400);
        Node(Var="UV", X=-1100, Y=-120);
    }
}
```

生成的资源：

```text
package     /Game/Materials/M_Emissive
object path /Game/Materials/M_Emissive.M_Emissive
on disk     <Project>/Content/Materials/M_Emissive.uasset      (once Materialized, or under the Graph backend)
```

> [!NOTE]
> `BaseTex` 被声明为 `TextureSampleParameter2D`，而不是紧凑的 `Texture2D`，因为只有 sample-parameter 系列拥有 `BaseTex(Coordinates = UV)` 调用形式所连接的输入引脚。紧凑的 `Texture2D` 会生成一个纹理 *对象* 参数，而它没有这样的引脚；调用它会落到函数分发器并失败。请参阅
> [在 Graph 中使用参数](../../parameters/graph-usage.md#the-pin-call-form)。

<a id="see-also"></a>

## 另请参阅

- [源文件](source-files.md) — `.dsm` / `.dsh` / `.dsf` 各自可以包含哪些块类型
- [DreamShaderLang 2.0](../language-v2/index.md) — 材质的 `.dss` 形式
- [属性](properties.md) — `Properties` 节语法
- [输入 / 输出 / 结果](inputs-outputs.md) — 类型化参数节（仅函数）
- [输出绑定](output-bindings.md) — 完整的 `Base.*` 目标目录和 `Expression(…).Pin[i]`
- [布局](layout.md) — `Node` / `Comment` 放置指令和 `#Region`
- [Graph](../graph/index.md) — `Graph` 内部的语句/表达式语言
- [着色器设置](../settings/material.md) — `Shader` 的 `Settings` 所接受的每个键
- [后端](../settings/backend.md) — `Graph` 对比 `ThinCustom`，以及已弃用的 `Instance` 别名
- [资源路径](../generation/asset-paths.md) — `Name=` + `Root=` → 包路径
- [临时材质](../generation/ephemeral.md) — 仅内存生成与落盘实体化
- [重新生成](../generation/regeneration.md) — 重新构建后哪些内容会保留、哪些不会
- [ShaderFunction](shader-function.md) — 可复用的 `UMaterialFunction` 块
- [诊断](../diagnostics/README.md) — 每个代码
