---
title: "DreamShaderParser.h *（2.0 中已弃用）*"
description: "DreamShaderParser.h *（2.0 中已弃用）*"
---

<a id="dreamshaderparserh-retired-in-20"></a>

# DreamShaderParser.h *（2.0 中已弃用）*

> [DreamShader](../index.md) » [C++ API](../../api/index.md) » **DreamShaderParser.h**

> [!IMPORTANT]
> **`DreamShaderParser.h` 和 `FTextShaderParser` 已不再存在。** 一直到 1.9.x，
> `DreamShader` 模块中的 `FTextShaderParser::Parse` 会将 1.x 源文本转换为一个
> [`FTextShaderDefinition`](../../api/types.md#ftextshaderdefinition)，而 1.x 生成器会基于该结构体构建资产。2.0 将二者一并弃用：现在由单一编译器的**旧版前端**读取 `.dsm` / `.dsf` / `.dsh`，
> 并生成与 `.dss` 解析所得相同的 AST。

<a id="what-to-call-instead"></a>

## 改用哪些接口

| 1.x | 2.0 |
| :-- | :-- |
| `#include "DreamShaderParser.h"`（模块 `DreamShader`） | `#include "Lang/LangParser.h"`（模块 `DreamShaderLang`，仅 `Core`） |
| `FTextShaderParser::Parse(SourceText, OutDefinition, OutError)` | `Lang::ParseDreamShaderLang(Lang::FLangSourceText(Path, Text), Options)` |
| 一个 `FText` 错误，仅返回首个错误 | `FLangParseResult::Diagnostics` —— 所有诊断，每个都带有 `DSHnnnn` 代码、严重级别以及包含行、列和长度的范围 |
| `FTextShaderDefinition` | `FLangParseResult::Module`（`Lang::FModule`，两个前端都会生成的 AST）以及 `FLangParseResult::Legacy`（`Lang::FLegacyMigrationInfo`：该 AST 背后的 1.x 块、区段、参数声明、资产引用和重命名） |
| `Definition.Warnings` | 同一接收器中严重级别为 `Warning` 的诊断 |
| 文本必须先展开其 `import` | `import` 是一个 `#include`；它会在绑定时通过 `FBindOptions::IncludeResolver` 解析 |

```cpp
#include "Lang/LangParser.h"

using namespace UE::DreamShader::Lang;

const FLangSourceText Source(TEXT("DShader/Materials/M_Panel.dsm"), Text);   // the extension picks the front end
const FLangParseResult Parsed = ParseDreamShaderLang(Source);

if (Parsed.Succeeded() && Parsed.Legacy.IsValid())
{
    // Parsed.Module: functions, uniforms, pragmas -- what the 1.x blocks mean in 2.0 terms.
    // Parsed.Legacy: what the 1.x text said, for tools that need the 1.x view (migrate, the language service).
}
```

`ELangFrontend::Auto` 根据扩展名判定：`.dsm` 和 `.dsf` 属于 1.x，`.dss` 和 `.dsi` 属于 2.0，
而 `.dsh` 头文件是**按声明**分派的，因此在一个项目迁移期间，同一个头文件中可以同时包含两种方言。

<a id="what-changed-in-behaviour"></a>

## 行为上的变化

旧版前端保留了 1.x **有文档记载的**宽松处理，但去掉了其未记录的静默处理：
凡是 1.x 未作任何提示即接受、随后构建出的东西却与文本所述不符的情况，现在都会成为
诊断。这些规则收录为 `L1`–`L27`；每一条会重写或丢弃内容的规则都会用代码说明（`DSH5254`、`DSH5275`–`DSH5292`……），而 [`dsc migrate`](../tools/migrate.md) 会将
相同的规则改写为显式的 2.0 文本。

随之弃用的 1.x 解析器代码：`DSH2007`、`DSH3133`、`DSH3137` 及其相邻代码不再
抛出；旧版前端自身的代码为 `DSH2200`–`DSH2258`、`DSH3250`–`DSH3278` 以及
`DSH6300`–`DSH6330`。

<a id="see-also"></a>

## 另请参阅

- [`DreamShaderLang`](../../api/lang-module.md) —— 语言模块的每一个入口点
- [`DreamShaderTypes.h`](../../api/types.md) —— 1.x 数据模型，资产层仍在使用
- [迁移](../tools/migrate.md) —— 将 1.x 源转换为 `.dss`
- [诊断索引](../diagnostics/index.md)
