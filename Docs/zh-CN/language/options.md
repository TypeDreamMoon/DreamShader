---
title: "Options"
description: "Options"
---

<a id="options"></a>

# Options

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Options**

`VirtualFunction` 块中用于命名声明所代表的现有 `UMaterialFunction` 资产的部分。

| | |
| :-- | :-- |
| 声明于 | `.dsh`、`.dsf`、`.dsm` — 仅在 `VirtualFunction` 块内 |
| 种类 | 节 |
| 生成 | 无 — `VirtualFunction` 声明资产，而不是创建资产 |
| 始于 | `1.2.0` |

在 2.0 中，同一声明是一个带有 `/// @asset` 的 `extern` 原型 — 请参阅
[DreamShaderLang 2.0](../language-v2/index.md)；[`dsc migrate`](../tools/migrate.md) 会以该方式重写
`VirtualFunction`。

<a id="synopsis"></a>

## 概要

```c
{ Options | Settings } [=]
{
    Asset = <asset-reference> ;
    [ <key> = <value> ; ]…
}
```

```c
asset-reference := Path( <root> , "<relative-path>" )
                 | Path( "<absolute-object-path>" )
                 | "<absolute-object-path>"
                 | <absolute-object-path>
```

`Options` 和 `Settings` 可作为节关键字互换使用；两者都会读入同一个
列表。节名匹配不区分大小写，`{ … }` 前的 `=` 是可选的
*（自 1.5.0 起）*，重复的节会合并到同一个列表中。

<a id="keys"></a>

## 键

| 键 | 必需 | 值 | 效果 |
| :-- | :-- | :-- | :-- |
| **`Asset`** | 是，除非给出了 `VirtualFunction(Asset="…")` | 资产引用 | 对此名称的 `Graph` 调用所解析到的 `UMaterialFunction`。 |
| `Description` | 否 | 字符串 | 保留为声明的描述。 |
| *任何其他键* | — | 任意 | **读取并忽略**，不产生诊断。 |

> [!NOTE]
> [VirtualFunction 同步服务](../../tools/virtual-function-tools.md)在生成或刷新声明时，会在 `Asset` 旁写入一个 `Description` 键。

<a id="precedence"></a>

### 优先级

块属性优先于节：

```c
VirtualFunction(Name="BufferWriter", Asset=Path(Game, "MaterialFunctions/F_BufferWriter"))
{
    Options = { Asset = Path(Game, "Ignored"); }   // not consulted — the attribute was present
    Outputs = { float3 Result; }
}
```

`Options.Asset` **仅**在 `Asset` 属性缺失或修剪后为空时才会被读取。

<a id="statement-grammar"></a>

## 语句文法

与 [`Settings`](../../settings/index.md) 文法相同。

| 规则 | 详情 |
| :-- | :-- |
| 语句形式 | `<Key> = <Value> ;` — 不是单词的键、缺少 `=` 或缺少值将被视为 [`DSH3261`](../diagnostics/DSH3xxx.md#dsh3261) |
| 值 | 括号外直到 `;` 的每个标记，因此 `Asset = Path(Game, "A/B")` 是一个值 |
| 键匹配 | 不区分大小写；键按原样保留 |
| 值处理 | 单个带引号的字符串取其文本，转义会被解析；其他任何内容取其标记的文本 |
| 重复键 | 后一条语句优先，并发出警告 [`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262) *（自 2.0.0 起；1.x 会静默覆盖）* |
| 注释 | 允许出现在任意两个标记之间 |

资产引用在使用前会被修剪，因此 `Asset = "  /Game/MF/F_X  ";` 和
`Asset = /Game/MF/F_X;` 命名的是同一资产。

带引号值中可识别的字符串转义为 `\n`、`\r`、`\t`、`\"`、`\\` 和 `\0`；其他任何 `\X`
为 [`DSH2104`](../diagnostics/DSH2xxx.md#dsh2104) *（自 2.0.0 起；1.x 产生字面量 `X`）*。

<a id="asset-reference-resolution"></a>

## 资产引用解析

前端按原样保留引用，编译器在构建对该 `VirtualFunction` 的 `Graph` 调用时
解析它，使用的是通用资产引用解析器 — 而不是
纹理默认解析器。可接受的根拼写：

| 根 | 解析为 |
| :-- | :-- |
| `Game` | `/Game` |
| `Engine` | `/Engine` |
| `Plugin.<Name>` | 插件的已挂载资产路径 |
| `Plugins.<Name>` | 插件的已挂载资产路径 |

`Path( … )` 调用接受 1 个参数（绝对 `/…` 对象路径）或 2 个参数（根加上
相对路径）。不是 `Path( … )` 调用的裸值或带引号值会被直接使用，且必须是
绝对对象路径。完整文法、两种解析器以及所有根诊断见
[`Path(...)`](../parameters/path.md)。

> [!WARNING]
> 无法解析的 `Asset` 在读取或绑定文件时**不会**被诊断 — 只有在构建对该函数的 `Graph`
> 调用时才会：对于无法解析的引用，报 [`DSH8270`](../diagnostics/DSH8xxx.md#dsh8270)；对于无法加载的资产，报 [`DSH8219`](../diagnostics/DSH8xxx.md#dsh8219)。一个无任何调用的
> `VirtualFunction` 会以损坏的 `Asset` 编译，且不产生任何消息。

<a id="post-parse-rules"></a>

## 解析后规则

| 规则 | 代码 |
| :-- | :-- |
| 存在 `Name` 属性，修剪后非空，且为标识符 | [`DSH6311`](../diagnostics/DSH6xxx.md#dsh6311) |
| 资产可从属性或从 `Options.Asset` 获得 | [`DSH6312`](../diagnostics/DSH6xxx.md#dsh6312) |
| 至少声明一个输出 | [`DSH6313`](../diagnostics/DSH6xxx.md#dsh6313) |

<a id="sections-accepted-alongside-options"></a>

## 可与 `Options` 一同接受的节

| 节 | 在 `VirtualFunction` 内 |
| :-- | :-- |
| `Options` | 本页 |
| `Settings` | `Options` 的别名 |
| `Inputs` | 类型化参数 — [Inputs / Outputs / Results](inputs-outputs.md) |
| `Properties` | **`Inputs` 的别名** |
| `Outputs` | 类型化参数 |
| `Results` | `Outputs` 的别名 |
| `Graph` | **错误** — [`DSH2247`](../diagnostics/DSH2xxx.md#dsh2247) |
| `Code` | **错误** — `DSH2247` |
| `Layout`、任何其他名称 | **错误** — [`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245) |

<a id="notes"></a>

## 注意

- `VirtualFunction` 的 `Inputs` 和 `Outputs` 描述的是*现有*资产的接口。它们
  是检查 `Graph` 调用的依据；它们不会创建引脚。让它们与
  资产不一致会在构建调用处产生错误 — 对于资产没有的输入，报 [`DSH8220`](../diagnostics/DSH8xxx.md#dsh8220)；对于输出，报 [`DSH8221`](../diagnostics/DSH8xxx.md#dsh8221) —
  而不是声明错误。
- `.dsh` 文件可以包含 `VirtualFunction` 块，且不应包含其他会生成资产的内容。一个仅含 `VirtualFunction` 块的 `.dsm`
  或 `.dsf` 不会构建任何资产。

<a id="diagnostics"></a>

## 诊断

每个诊断都会携带其所属语句的行号和列号。

| 代码 | 阶段 | 引发条件 |
| :-- | :-- | :-- |
| `DSH3261` | 解析 | 不是 `Key = Value` 的语句 |
| `DSH3262` | 解析 | 警告：某个键被写了两次；后一个值优先 |
| `DSH6311` | 解析 | 没有 `Name`、为空，或不是标识符 |
| `DSH6312` | 解析 | 无法从属性或从 `Options` 获得资产 |
| `DSH6313` | 解析 | 未声明输出 |
| `DSH2247` | 解析 | 块中存在 `Graph` 或 `Code` 节 |
| `DSH2245` | 解析 | 任何其他未知节名 |
| `DSH8270`、`DSH8219` | 构建调用 | `Asset` 值无法解析或无法加载 |
| `DSH8220`、`DSH8221` | 构建调用 | 所声明的接口命名了资产没有的输入或输出 |

完整列表见[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
VirtualFunction(Name="BufferWriter")
{
    Options = {
        Asset = Path(Game, "MaterialFunctions/F_BufferWriter");
        Description = "Existing material function declared for Graph calls.";
    }

    Inputs = {
        float3 Color;
        float  Alpha;
    }

    Outputs = {
        float3 Result;
    }
}
```

从同一文件中的 `Shader` 调用它，或在导入它的文件中调用：

```c
Graph = {
    float3 Written = BufferWriter(Color = Tint, Alpha = A);
}
```

解析后的引用：

```text
Asset  Path(Game, "MaterialFunctions/F_BufferWriter")
    -> /Game/MaterialFunctions/F_BufferWriter.F_BufferWriter
```

<a id="see-also"></a>

## 另见

- [VirtualFunction](virtual-function.md) — `Options` 所属的块
- [输入 / 输出 / 结果](inputs-outputs.md) — `VirtualFunction` 的接口部分
- [设置](../../settings/index.md) — 相同的语句语法
- [`Path(...)`](../parameters/path.md) — 每种根写法以及两个解析器
- [调用](../graph/calls.md) — 从 `Graph` 调用 `VirtualFunction`
- [VirtualFunction 工具](../../tools/virtual-function-tools.md) — 编辑器操作与同步服务
- [源文件](source-files.md) — `.dsh` / `.dsf` / `.dsm` 可包含哪些块类型
- [DreamShaderLang 2.0](../language-v2/index.md) — `/// @asset` 与 `extern`，`.dss` 写法
- [诊断索引](../diagnostics/index.md) — 每个代码
