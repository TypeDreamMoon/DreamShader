---
title: "禁用诊断"
description: "禁用诊断"
---

<a id="disabling-diagnostics"></a>

# 禁用诊断

> [DreamShader](../index.md) » [DreamShaderLang 2.0](index.md) » **禁用诊断**

*（2.1.2 之后）* 源文件可以按代码关闭警告和错误，作用于整个文件或单行。两种语言都接受下面所有写法。

| 写法 | 禁用对象 |
| :-- | :-- |
| `#pragma disable_warning(DSH7224, DSH8237)` | 这些警告，在文件中的任何位置 —— 而且在正在编译的文件中，该次编译的所有诊断，以及它所包含的头文件 |
| `#pragma disable_error(DSH8219)` | 这些错误，方式相同 |
| `code; // disable_warning DSH5251` | 该行上的那个警告 |
| `// disable_error DSH4210` 单独占一行 | 下一行包含代码的那个错误（中间的空行或普通注释不算；`///` 行算） |

列表是以逗号或空格分隔的代码；大小写不敏感。不是代码的词会
[`DSH7236`](../diagnostics/DSH7xxx.md#dsh7236) 并被忽略，完全不含代码的列表也会被忽略 ——
两者都不会禁用“所有内容”。`#pragma` 行可以位于文件作用域中的任何位置；其位置无关紧要。

```hlsl
#pragma disable_warning(DSH9011)        // this material is slow to compile on purpose

// disable_warning DSH7224
/// @library Legacy
float Helper(float x) { return x; }
```

`///` 行是直到行尾的文档，因此文档注释之后的 `// disable_…` 属于文档文本的一部分；请将它放在上面一行，如此处所示。

<a id="what-disabling-does"></a>

## 禁用的作用

- **警告**不会被报告：不在编译结果中，不在桥接写入的诊断中，也不在编辑器日志中。
- **错误**不会被报告**且不会被计入**：本应因它而停止的阶段会继续。该错误所涉及的内容随后就由编译器自行处理 —— 后续阶段可能会改为报告其他内容。当某个阶段离开它就无法继续且没有其他说明时，编译最终仍会以该被禁用的错误失败，并随后给出
  `(The source disables DSHnnnn, but the compile could not go on past it.)`。
- **信息**无法被禁用 —— 流水线会回读其中一些（被跳过的资产、先编译的父级）。

<a id="where-the-lists-are-read-from"></a>

## 列表的读取来源

来自每个文件的文本，在文件被解析之前逐行读取：`//` 注释永远不会到达解析器，而 `#if` 也不会隐藏两种写法中的任何一种。正在编译的文件在编译开始时读取。头文件在其自身的某个诊断出现时读取，其列表仅覆盖该头文件 —— 它的 `#pragma disable_*` 行不会传递到包含它的文件。

为另一个源文件而首先编译的源文件（`.dsi` 的父级）使用其自身的列表编译，而不是请求它的源文件的列表。

没有位置的项目级警告 —— [`DSH9011`](../diagnostics/DSH9xxx.md#dsh9011)、
[`DSH9012`](../diagnostics/DSH9xxx.md#dsh9012)、`DSH8155` —— 只能通过 `#pragma
disable_warning` 禁用。

<a id="see-also"></a>

## 另请参阅

- [诊断索引](../diagnostics/index.md) —— 每个 `DSHnnnn`
- [NoGeneration](../generation/no-generation.md) —— 另一个文件级 `#pragma`
