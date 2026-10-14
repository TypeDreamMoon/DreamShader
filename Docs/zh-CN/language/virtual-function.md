---
title: "VirtualFunction"
description: "VirtualFunction"
---

<a id="virtualfunction"></a>

# VirtualFunction

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **VirtualFunction**

一个顶层块，用于声明一个**已存在的** `UMaterialFunction` 资源——包括其路径和引脚
签名——以便 `Graph` 可以调用它。它不会生成任何内容。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsh` 和 `.dsf` —— 这三种文件类型均接受它 |
| 类型 | 顶层块 |
| 生成 | 无 —— 资源必须已经存在 |
| 多重性 | 每个文件可有任意数量；一个名称声明一个对象 |
| 始于 | `1.2.0` |

旧版前端会将 `VirtualFunction` 读取为带有 `/// @asset` 的 2.0 `extern` 原型；参见
[2.0 语言](../language-v2/index.md) 和 [`dsc migrate`](../tools/migrate.md)。

<a id="synopsis"></a>

## 概要

```c
VirtualFunction(Name = "<call-name>" [, Asset = <asset-reference>])
{
    [{ Options | Settings }   [=] { Asset = <asset-reference> ; … }]
    [{ Inputs | Properties }  [=] { <parameter-declaration> ; … }]
    { Outputs | Results }     [=] { <parameter-declaration> ; … }
}
```

需要提供 `Name`、一个资源引用以及至少一个输出。资源引用可以来自
头部属性或来自 `Options`；参见[资源引用](#the-asset-reference)。

节名称与其 `{ … }` 块之间的 `=` 是可选语法糖 *(自 1.5.0 起)*；节结束的
`}` 之后的 `;` 是可选的。节可以以任意顺序出现，并且可以重复。

关键字 `VirtualFunction` 在匹配时**区分大小写**；节名称在匹配时
不区分大小写。

<a id="header-attributes"></a>

## 头部属性

| 属性 | 必需 | 值 | 效果 |
| :-- | :-- | :-- | :-- |
| **`Name`** | 是 | string | 在 `Graph` 中调用该块时使用的名称。会修剪空白；必须是标识符（[`DSH6311`](../diagnostics/DSH6xxx.md#dsh6311)）。不是资源路径——不会创建任何内容。 |
| `Asset` | 否 | 资源引用 | 优先于 `Options.Asset`；仅当此属性缺失或修剪后为空时，才查询 `Options` 值。 |

属性键在匹配时不区分大小写。**没有 `Root` 属性**——包根路径是
资源引用本身的一部分。无法识别的属性键会被解析并静默忽略；重复写入
同一个键会产生警告（[`DSH2244`](../diagnostics/DSH2xxx.md#dsh2244)），后一个值生效。
`)` 之前的尾随逗号会被接受，与 1.x 中一样 *(2.0.0 – 2.1.0 曾以 [`DSH2243`](../diagnostics/DSH2xxx.md#dsh2243) 拒绝它)*。

*(自 2.0.0 起)* 未加引号的属性值会一直延伸到**括号外**的下一个 `,` 或 `)`，因此
`Asset = Path(Game, "F/X")` 在头部以及 `Options` 中均可使用。（1.x 会在第一个
`,` 处截断该值并导致解析失败。）

*(自 2.0.0 起)* `Name` 是一个标识符。其中包含 `/` —— 1.x 接受它，但只有最后一个
段可被调用 —— 这属于 `DSH6311`。

<a id="sections"></a>

## 节

| 节 | 是否接受 | 重复行为 | 参考 |
| :-- | :-- | :-- | :-- |
| `Inputs` | 是 | 追加 | [输入 / 输出 / 结果](inputs-outputs.md) |
| `Properties` | 是——**`Inputs` 的别名**，无警告 | 追加 | [输入 / 输出 / 结果](inputs-outputs.md) |
| `Outputs` | 是 | 追加 | [输入 / 输出 / 结果](inputs-outputs.md) |
| `Results` | 是——`Outputs` 的别名，无警告 | 追加 | [输入 / 输出 / 结果](inputs-outputs.md) |
| `Options` | 是 | 合并；重复写入同一个键会产生警告（[`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262)），后一个值生效 | [选项](options.md) |
| `Settings` | 是——`Options` 的别名，无警告 | 合并，与 `Options` 相同 | [选项](options.md) |
| `Graph` | **否**——[`DSH2247`](../diagnostics/DSH2xxx.md#dsh2247) | — | — |
| `Code` | **否**——`DSH2247` | — | — |
| 其他任何内容，包括 `Layout` | **否**——[`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245) | — | — |

> [!NOTE]
> **在 `VirtualFunction` 内部，且仅在此处，`Properties` 表示 `Inputs`。** 其中的语句
> 使用带类型参数的语法（`[opt] <type> <name> [= <default>] [[ … ]] ;`）读取，并追加
> 到该块的输入列表。在 [`Shader`](shader.md)、[`ShaderFunction`](shader-function.md) 或
> [`ShaderLayer`](shader-layer.md) 中，同一关键字会以不同的语法声明参数和 `const` **节点**。
> 诸如 `const float X = 1;` 这样在这些块中合法的声明，并不是有效的
> `VirtualFunction` `Properties` 语句：`const` 是关键字而非类型，因此该语句是
> [`DSH3271`](../diagnostics/DSH3xxx.md#dsh3271)。

<a id="the-asset-reference"></a>

## 资源引用

该值在解析时按写入原样存储；在发射器构建对该块的调用之前，不会进行任何检查。前导和
尾随空白会被修剪，一对包裹的 `"` 会被剥离并
反转义，且路径中的 `\` 会被改写为 `/`。

| 形式 | 解析为 |
| :-- | :-- |
| `Path(Game, "Folder/Asset")` | `/Game/Folder/Asset.Asset` |
| `Path(Engine, "Folder/Asset")` | `/Engine/Folder/Asset.Asset` |
| `Path(Plugin.<PluginName>, "Folder/Asset")` | 插件挂载的资源路径 + `/Folder/Asset` |
| `Path(Plugins.<PluginName>, "Folder/Asset")` | 同上 |
| `Path(Plugin/<PluginName>, "Folder/Asset")` | 同上 |
| `Path(Plugins/<PluginName>, "Folder/Asset")` | 同上 |
| `Path(<Root>/<Folder>…, "Asset")` | 根路径，然后是作为文件夹的额外段——`Path(Game/Materials, "F_X")` → `/Game/Materials/F_X.F_X` |
| `Path("/Game/Folder/Asset")` | 一个参数：一个绝对包路径，按原样使用 |
| `"/Game/Folder/Asset"` | 一个不带 `Path(...)` 包装的、用引号括起的裸绝对路径 |
| `/Game/Folder/Asset` | 一个不带引号的裸绝对路径 |
| `/Game/Folder/Asset.Asset` | 一个完整的 Unreal 对象路径 |
| `"MaterialFunction'/Game/Folder/Asset.Asset'"` | 引擎的*复制引用*形式，带引号；不是材质函数的类会触发 [`DSH1045`](../diagnostics/DSH1xxx.md#dsh1045) |

适用于所有形式的规则：

- 根名称和两个 `Path(...)` 参数在匹配时不区分大小写，且每个参数
  可以带引号或为裸值。
- 已经以 `/` 开头的路径会按原样使用；根参数（如果有）会被忽略。
- **不**以 `/` 开头的路径需要根
  （[`DSH8123`](../diagnostics/DSH8xxx.md#dsh8123)）。
- 当解析后的路径不带 `.ObjectName` 后缀时，会追加叶子名称，因此 `/Game/F/X`
  会变为 `/Game/F/X.X`。
- 插件根路径必须指向一个已启用、可以包含内容、具有现有
  `Content` 目录且——在 UE 5.6 及更高版本上——已挂载的项目插件。每种失败都有各自的代码
  （`DSH8118`–`DSH8122`、`DSH8124`）；参见 [Path](../parameters/path.md)。

无法解析的引用会报告为
[`DSH8270`](../diagnostics/DSH8xxx.md#dsh8270)，其消息携带解析器自身的代码
（`DSH8118`–`DSH8132`、`DSH1045`）。

<a id="options-keys"></a>

## `Options` 键

`Options`（及其别名 `Settings`）使用普通的设置语法：`<Key> = <Value> ;`，键
比较时忽略大小写，带引号的值会被去引号。

| 键 | 是否读取 | 效果 |
| :-- | :-- | :-- |
| `Asset` | 是 | 资源引用，在头部 `Asset=` 属性缺失或为空时使用 |
| `Description` | 是 | 保留为声明的描述 *(自 2.0.0 起)*，即 `dsc migrate` 写入 `extern` 上方的文本；它不会改变构建中的任何内容 |
| 任何其他键 | **否** | 解析并忽略，不产生诊断 |

`Description` 是编辑器的 VirtualFunction 工具写入生成声明中的键。参见
[选项](options.md)。

<a id="inputs-and-outputs"></a>

## 输入与输出

声明的参数描述的是**现有资源**的接口；它们不会创建任何东西。
它们有三个用途：让调用点能够进行类型检查、为引脚命名，以及固定语句调用参数
的顺序。

可接受的类型标记和参数语法是[`ShaderFunction`](shader-function.md#parameter-types)，
并且同样适用`opt`规则：`opt` —— 或者 *(自 2.0.0 起)* 默认值 —— 标记调用者
可以省略的输入。默认值和`[ … ]`元数据也会在此处解析，但不会写入任何地方：
`VirtualFunction` 永远不会触碰它所指向的资源，而调用未传递的输入在调用节点上
保持未连接，因此资源自身的默认值会生效。一个名称恰好为 `Result`
且排在最前面的输出就是该调用的返回值。

在发射时，调用传递的每个输入会按名称与加载的`UMaterialFunction`上的引脚进行匹配 ——
先精确匹配，然后忽略大小写 —— 两者都失败时则按该输入在声明输入中的位置匹配。
输出也以同样的方式匹配。未匹配到任何内容的输入或输出会变成
[`DSH8220`](../diagnostics/DSH8xxx.md#dsh8220) / [`DSH8221`](../diagnostics/DSH8xxx.md#dsh8221)。

<a id="calling-a-virtualfunction"></a>

## 调用 VirtualFunction

被调用者名称是 `Name` 属性。*(自 2.0.0 起)* 它进行精确匹配；仅在大小写上匹配的调用
在匹配唯一时解析成功，并伴有
[`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275)。

```c
// VirtualFunction(Name="BufferWriter") declared in an imported .dsh:
vec3 Written = BufferWriter(Color, Alpha);        // single-output call as a value  (since 1.5.0)
BufferWriter(Color, Alpha, OutResult);            // statement call: inputs, then one target per output  (since 1.3.5)
```

表达式调用接受位置参数或命名参数（`BufferWriter(Color = Tint)`）。对多输出块的表达式
调用通过命名 `Output=` /
`OutputName=` / `OutputIndex=` 参数来选择要求值到哪个输出；`Output`/`OutputName` 和 `OutputIndex` 一起
是[`DSH5252`](../diagnostics/DSH5xxx.md#dsh5252)。

语句调用会传递输入，然后为每个声明的输出传递一个接收者，每个接收者都必须是
变量；每个接收者会将其输出写入该名称，替换绑定到它的任何先前值。完整的
参数规则，包括用于 `opt` 输入的 `default` 哨兵值，见
[Calls](../graph/calls.md)。

> [!NOTE]
> 有三个名称会在进行任何查找之前被拦截：`BreakOutFloat2Components`、
> `BreakOutFloat3Components` 和 `BreakOutFloat4Components`（任意大小写）。对其中之一的 **表达式** 调用，
> 如果其第一个参数是位置参数并且选择了一个通道 —— `Output="G"`（为
> `R`/`G`/`B`/`A`/`X`/`Y`/`Z`/`W` 之一）、`Output="1"` 或 `OutputIndex=1` —— 在该向量的宽度范围内，
> 会编译为该参数上的一个 swizzle，而不是一个 `MaterialFunctionCall` 节点，并且不会
> 加载任何资源。拦截未识别的任何内容会回落到正常调用路径。
> 语句调用不会被拦截。

<a id="notes"></a>

## 说明

- **不会生成任何内容，也不会在调用之前对资源进行任何检查。** 一个
  其资源不存在的 `VirtualFunction` 会一直正常构建，直到某个 `Graph` 调用它；然后
  发射器会报告 [`DSH8219`](../diagnostics/DSH8xxx.md#dsh8219)。仅声明
  `VirtualFunction` 块的文件会被接受并且不产生任何内容。
- 一个 `VirtualFunction` 通常保存在一个 `.dsh` 中，并在需要的地方导入，这正是
  编辑器的 *Create VirtualFunction* 操作所做的 —— 它会在
  `DShader/VirtualFunctions` 下写入一个声明。见 [VirtualFunction 工具](../../tools/virtual-function-tools.md)。
- 头部 `Asset=` 属性优先于 `Options.Asset`；只有在
  该属性缺失或修剪后为空时，才会查询 `Options` 条目。
- 头部会被独立解析，其声明会声明到导入文件中，因此声明在被导入的
  `.dsh` 中的 `VirtualFunction` 对导入文件的 `Graph` 块可见。
  见 [import](import.md)。
- 两个同名 `VirtualFunction` 块属于[`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210)
  *(自 2.0.0 起；1.x 保留两者并调用第一个)*。

<a id="diagnostics"></a>

## 诊断

每个代码都带有该构造的行号和列号；该代码的页面包含消息。

<a id="parse-time"></a>

### 解析时

| 代码 | 引发条件 |
| :-- | :-- |
| [`DSH2241`](../diagnostics/DSH2xxx.md#dsh2241) | `VirtualFunction` 之后没有 `(` |
| `DSH2243` | 格式错误的属性列表 |
| `DSH2244` | 属性被写入两次（警告） |
| `DSH6311` | 没有 `Name`，或它不是标识符 |
| [`DSH2257`](../diagnostics/DSH2xxx.md#dsh2257) | 头部之后没有 `{`，或某个节缺少其名称或其 `{` |
| `DSH2247` | 一个 `Graph` 或 `Code` 节 |
| `DSH2245` | 任何其他未知节 |
| `DSH3271` | 不是 `[opt] <type> <name> [= <default>] [[ … ]] ;` 的参数语句 |
| [`DSH3272`](../diagnostics/DSH3xxx.md#dsh3272) / [`DSH3273`](../diagnostics/DSH3xxx.md#dsh3273) | `opt` / 输出上的默认值（警告；被忽略） |
| [`DSH3255`](../diagnostics/DSH3xxx.md#dsh3255)–[`DSH3258`](../diagnostics/DSH3xxx.md#dsh3258) | 格式错误的 `[ … ]` 元数据块 |
| [`DSH3261`](../diagnostics/DSH3xxx.md#dsh3261) | 缺少键、`=` 或值的 `Options` 语句 |
| `DSH3262` | 被写入两次的 `Options` 键（警告） |
| [`DSH6312`](../diagnostics/DSH6xxx.md#dsh6312) | 头部中没有资源引用，且 `Options.Asset` 下也没有 |
| [`DSH6313`](../diagnostics/DSH6xxx.md#dsh6313) | 没有 `Outputs` / `Results` 条目 |
| [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) | 主体从未被关闭 |

<a id="binding-and-call-time"></a>

### 绑定和调用时

| 代码 | 引发条件 |
| :-- | :-- |
| [`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201) | 类型标记未命名任何类型 |
| [`DSH4214`](../diagnostics/DSH4xxx.md#dsh4214) | 两个参数共享同一名称 |
| `DSH4210` | 名称被声明两次 |
| `DSH5252` | 一次调用上同时有 `Output`/`OutputName` 和 `OutputIndex` |
| [`DSH5250`](../diagnostics/DSH5xxx.md#dsh5250) / [`DSH5251`](../diagnostics/DSH5xxx.md#dsh5251) | 不是整数的 `OutputIndex` / 不是名称的 `Output` |

关于参数计数、命名参数、缺失输入和接收者的诊断与
其他所有调用共享，并列在 [Calls](../graph/calls.md) 和
[`Function` § 调用时](function.md#call-time) 中。

<a id="emit-time--asset-resolution-and-interface"></a>

### 发射时 —— 资源解析和接口

这些会在发射器构建调用时触发，而不是在块被声明时触发。

| 代码 | 引发条件 |
| :-- | :-- |
| `DSH8270` | 资源引用无法解析；消息携带解析器的代码（`DSH8118`–`DSH8132`、`DSH1045`） |
| `DSH8219` | 没有 `UMaterialFunction` 从解析后的路径加载，或调用节点拒绝它 |
| `DSH8220` | 传入的输入未匹配任何引脚 |
| `DSH8221` | 输出未匹配任何引脚 |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 无法创建 `MaterialFunctionCall` 节点 |

完整的跨阶段列表见[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

`DShader/VirtualFunctions/VF_BufferWriter.dsh`：

```c
VirtualFunction(Name="BufferWriter")
{
    Options = {
        Asset       = Path(Game, "MaterialFunctions/F_BufferWriter");
        Description = "Existing material function declared for Graph calls.";
    }

    Inputs = {
        vec3  Color;
        float Alpha;
        opt float Exposure = 1.0 [Description="Optional gain applied inside the function"];
    }

    Outputs = {
        vec3  Result;
        float Coverage;
    }
}
```

`DShader/M_Buffered.dsm`：

```c
import "VirtualFunctions/VF_BufferWriter.dsh";

Shader(Name="Materials/M_Buffered")
{
    Properties = {
        vec3  Tint  = vec3(1.0, 0.9, 0.8);
        float Fade  = 0.5 [Slider(0, 1)];
    }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph = {
        BufferWriter(Tint, Fade, Color, Alpha);
    }
}
```

结果：

```text
VF_BufferWriter.dsh  ->  no asset generated
M_Buffered.dsm       ->  /Game/Materials/M_Buffered
                         contains one MaterialFunctionCall node bound to
                         /Game/MaterialFunctions/F_BufferWriter.F_BufferWriter
                         Exposure is left unconnected (optional, not passed)
```

<a id="see-also"></a>

## 另请参阅

- [Options](options.md) — `Options` 节及其识别的每一个键
- [Inputs / Outputs / Results](inputs-outputs.md) — 类型化参数语法、`opt`、默认值、元数据
- [ShaderFunction](shader-function.md) — 生成 `UMaterialFunction` 而非声明一个
- [ShaderLayer / ShaderLayerBlend](shader-layer.md) — 材质层函数块
- [Shader](shader.md) — 生成 `UMaterial` 的顶层块
- [Source files](source-files.md) — `.dsm` / `.dsh` / `.dsf` 各自可包含哪些块类型
- [import](import.md) — `.dsh` 声明如何到达调用它的文件
- [Calls](../graph/calls.md) — 参数形式、`default`、语句调用与表达式调用
- [Path](../parameters/path.md) — 所有可接受的 `Path(...)` 拼写形式
- [VirtualFunction tools](../../tools/virtual-function-tools.md) — 从编辑器创建、打开和刷新声明
- [Diagnostics index](../diagnostics/index.md) — 按阶段列出的每一个代码
