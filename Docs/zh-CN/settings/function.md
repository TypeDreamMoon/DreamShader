---
title: "函数设置"
description: "函数设置"
---

<a id="function-settings"></a>

# 函数设置

> [DreamShader](../index.md) » [设置](index.md) » **函数设置**

[`ShaderFunction`](../language/shader-function.md)、
[`ShaderLayer` 或 `ShaderLayerBlend`](../language/shader-layer.md) 块的 `Settings` 部分：用于向材质函数库描述
所生成 `UMaterialFunction` 的四个键。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` — 位于 `ShaderFunction`、`ShaderLayer` 或 `ShaderLayerBlend` 内 |
| 类型 | 部分 |
| 生成 | 对所生成的 `UMaterialFunction`、`UMaterialFunctionMaterialLayer` 或 `UMaterialFunctionMaterialLayerBlend` 的属性写入 |

<a id="synopsis"></a>

## 概要

```c
ShaderFunction(Name = "<asset-path>")
{
    Settings [=] {
        [Description       = <text>;]
        [UserExposedCaption = <text>;]
        [ExposeToLibrary   = { true | false };]
        [LibraryCategories = "<category>[, <category>]…";]
    }
}
```

在 `ShaderLayer`、`ShaderLayerBlend` 及其已废弃的 `MaterialLayer` / `MaterialLayerBlend` 拼写中，接受同样的部分，并使用同样的四个键。在 2.0 中，这些键是导出函数的 `/// @desc` 和
`/// @library` 指令 — 请参阅[2.0 语言](../language-v2/index.md)。

<a id="keys"></a>

## 键

键在匹配时忽略大小写，因此 `description`、`Description` 和 `DESCRIPTION` 是同一个键。
此处下划线**不**折叠 — `Expose_To_Library` 不等于 `ExposeToLibrary`。

| 键 | 设置的内容 | 值语法 | 键缺失时的值 |
| :-- | :-- | :-- | :-- |
| **`Description`** | `UMaterialFunction::Description` | 自由文本 | 清空为空字符串 |
| **`UserExposedCaption`** | 无内容 *(自 2.0.0 起)*：[`DSH3264`](../diagnostics/DSH3xxx.md#dsh3264) 会警告其未被应用，并且资产的标题保持原样 | 自由文本 | — |
| **`ExposeToLibrary`** | `UMaterialFunction::bExposeToLibrary`，连同 `LibraryCategories` | `true` / `false`，不区分大小写；任何其他值都会产生 [`DSH3265`](../diagnostics/DSH3xxx.md#dsh3265) 警告，并且该设置被忽略 | `false` |
| **`LibraryCategories`** | `UMaterialFunction::LibraryCategoriesText`，连同 `ExposeToLibrary` | 逗号分隔的列表；每个条目都会被修剪，空条目会被丢弃 | 类别列表被清空 |

*(自 2.0.0 起)* `ExposeToLibrary` 和 `LibraryCategories` 作为一个设置起作用，即 2.0 的
`/// @library <categories>`：当 `ExposeToLibrary`
为 `true` **且** `LibraryCategories` 不为空时，该函数会以这些类别被公开。仅 `ExposeToLibrary = true;` 会使该函数
不出现在库中，而没有 `ExposeToLibrary = true;` 的 `LibraryCategories` 会被丢弃。

`LibraryCategories` 在解析前会清空列表，因此该设置始终会替换资产的
类别，而不是追加到它们之上。`LibraryCategories = "A,,  B ,";` 恰好产生 `A` 和
`B`。

每个值上的引号都是可选的，就像 `Settings` 块中各处一样：
`Description = Tint helper;` 和 `Description = "Tint helper";` 完全相同。

> [!NOTE]
> **当键缺失时，此处的每个键都会被重置。** 省略 `ExposeToLibrary` 会将其设为 `false`；
> 省略 `Description` 会将其清空。在生成资产上手动编辑的属性会在
> 下次重新生成时被丢弃 — 但标题除外，2.0 完全不会写入它。请参阅
> [重新生成](../generation/regeneration.md)。

> [!WARNING]
> **任何其他键都会被忽略**，并产生 [`DSH3263`](../diagnostics/DSH3xxx.md#dsh3263) 警告
> *(自 2.0.0 起；1.x 不会给出任何提示)*。只会查找这四个名称：`Backend`、`Domain`、
> `ShadingModel`、`BlendMode`、`TwoSided` 以及所有其他 [`Shader` 设置](material.md) 在 `ShaderFunction` 块中
> 都**不起作用**，而这四个名称之一的拼写错误 — `Descriptions`、
> `Expose_To_Library`、`LibraryCategory` — 也会产生同样的警告。

> [!WARNING]
> 在将
> `UMaterialFunction` 导出到 `.dsf` 时，[反编译器](../tools/decompiler.md) 不会生成 `Settings` 块。`Description`、`UserExposedCaption`、`ExposeToLibrary` 和
> `LibraryCategories` 会在该往返过程中丢失，必须手动重新添加。

<a id="related-validation"></a>

## 相关验证

这些检查是最常与 `Settings` 错误混淆的诊断。

| 规则 | 代码 |
| :-- | :-- |
| 材质函数至少产生一个输出 | [`DSH4315`](../diagnostics/DSH4xxx.md#dsh4315) |
| `ShaderLayer` 或 `ShaderLayerBlend` 有一个 `MaterialAttributes` 输出 | [`DSH3278`](../diagnostics/DSH3xxx.md#dsh3278) |
| `ShaderLayer` 以该材质作为其唯一输入和输出 | [`DSH6204`](../diagnostics/DSH6xxx.md#dsh6204) |
| `ShaderLayerBlend` 除材质输出外，仅有输入 — 且其中至少一个是 `MaterialAttributes` | [`DSH6205`](../diagnostics/DSH6xxx.md#dsh6205) |
| 主体部分是 `Graph`，而不是 `Code` | [`DSH2246`](../diagnostics/DSH2xxx.md#dsh2246) |
| 文件的两个属性不共享同一名称 | [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210) |

生成资产的用途遵循块类型：`ShaderFunction` 产生普通材质
函数，`ShaderLayer` 产生材质层函数，`ShaderLayerBlend` 产生层混合函数。

<a id="notes"></a>

## 备注

- [`VirtualFunction`](../language/virtual-function.md) 块也接受名为
  `Settings` 的部分，但它是 `Options` 的同义词，并且采用完全不同的键集 — `Asset` 和
  `Description`。请参阅[选项](../language/options.md)。
- [`Function`](../language/function.md) 和 [`GraphFunction`](../language/graph-function.md) 块
  完全没有 `Settings` 部分；它们仅有的部分是 `Inputs` / `Properties`、
  `Outputs` / `Results` 和 `Code` / `Graph`。
- 一个块中的多个 `Settings` 部分会合并，后出现的键获胜，并产生
  [`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262) 警告，与 `Shader` 块中完全相同。请参阅
  [设置](index.md#how-a-statement-is-parsed)。
- 带 `LibraryCategories` 的 `ExposeToLibrary = true;` 会使该函数出现在
  材质面板的函数库中；类别决定它位于该面板中的位置。

<a id="diagnostics"></a>

## 诊断

| 代码 | 引发条件 |
| :-- | :-- |
| `DSH3265` | `ExposeToLibrary` 值不是布尔字面量（警告；该设置被忽略） |
| `DSH3264` | `UserExposedCaption` 被设置（警告；未应用） |
| `DSH3263` | 键不是这四个之一（警告；被忽略） |
| `DSH3262` | 某个键被写入了两次（警告；后出现的值获胜） |
| `DSH4315` | 该块不产生输出 |
| `DSH3278` | 层或混合没有 `MaterialAttributes` 输出 |
| `DSH6204` / `DSH6205` | 层的 / 混合的参数与其类型不符 |
| [`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245) | 部分名称不是已识别的名称之一 |

`Description` 和 `LibraryCategories` 没有失败模式：任何文本都会被接受。

<a id="example"></a>

## 示例

```c
ShaderFunction(Name="Functions/F_Tint", Root="Game")
{
    Inputs = {
        vec3 InColor;
        vec3 InTint;
    }

    Outputs = {
        vec3 OutColor;
    }

    Settings = {
        Description        = "Multiplies a colour by a tint.";
        UserExposedCaption = "Tint";
        ExposeToLibrary    = true;
        LibraryCategories  = "DreamShader, Color";
    }

    Graph = {
        OutColor = InColor * InTint;
    }
}
```

生成的资产：

```text
package  /Game/Functions/F_Tint                     UMaterialFunction
  Description           = "Multiplies a colour by a tint."
  bExposeToLibrary      = true
  LibraryCategoriesText = ["DreamShader", "Color"]
```

`UserExposedCaption` 会连同 `DSH3264` 一起被报告，且不会被应用。

<a id="see-also"></a>

## 另请参阅

- [设置](index.md) — 所有块类型共享的块语法与键归一化
- [着色器设置](material.md) — `Shader` 块截然不同的键集合
- [ShaderFunction](../language/shader-function.md) — 外层块
- [ShaderLayer / ShaderLayerBlend](../language/shader-layer.md) — 层块类型及其元数规则
- [VirtualFunction](../language/virtual-function.md) — 声明一个现有的 `UMaterialFunction`
- [选项](../language/options.md) — `VirtualFunction` 键集合
- [输入 / 输出 / 结果](../language/inputs-outputs.md) — 函数声明的参数区段
- [反编译器](../tools/decompiler.md) — 材质函数往返转换的缺口
- [重新生成](../generation/regeneration.md) — 重建会重置的内容
- [诊断索引](../diagnostics/index.md) — 所有代码
