---
title: "`DreamShaderLang`"
description: "`DreamShaderLang`"
---

<a id="dreamshaderlang"></a>

# `DreamShaderLang`

> [DreamShader](../index.md) » [C++ API](index.md) » **`DreamShaderLang`**

`DreamShaderLang` 是前端**也是中间端**：文本输入，降级后的图输出 —— 反之亦然。词法分析器、预处理器、两个解析器（2.0 和 1.x 旧版前端）、AST、打印器、绑定器、带各 pass 的图 IR、验证器和比较器、把 IR 转回源码的写出器、1.x → 2.0 迁移器，以及诊断类型都位于此处 —— 别无其他。它在 2.0 中从 `DreamShader` 中拆分出来，使这门语言可以在没有编辑器的情况下被使用、被语言服务复用，并作为一门语言而非插件来推理。

该模块*不*跨越的界限是引擎：一个 `FIRModule` 不命名任何 `UObject`、任何资产和任何表达式类对象。引擎知识以纯数据形式出现在 [`FBuiltinCatalog`](#ir--the-graph-ir) 中，Editor 模块通过反射填充它，也可以将其保存到 JSON 以及从 JSON 加载。

| | |
| :-- | :-- |
| 模块 | `DreamShaderLang` |
| 类型 / 加载阶段 | `Runtime` / `PostConfigInit` |
| 依赖项 | **仅 `Core`** |
| 导出宏 | `DREAMSHADERLANG_API` |
| 公共头文件 | 30：模块根目录下 4 个 + `Lang/` 下 11 个 + `Semantic/` 下 1 个 + `IR/` 下 12 个 + `Decompile/` 下 1 个 + `Migrate/` 下 1 个 |
| 命名空间 | `UE::DreamShader::Lang`（2.0 前端和绑定器）· `UE::DreamShader::IR`（图 IR 和内置目录）· `UE::DreamShader`（预处理器、define 表、`FDreamShaderError`） |
| 反射类型 | **无** —— 没有 `UCLASS`，没有 `USTRUCT`，没有 `UENUM` |
| 状态 | `2.0` 中新增：词法分析器、2.0 解析器和打印器；绑定器、IR、其构建器、各 pass 和验证器，以及自定义 HLSL 构建器；旧版前端、`.dsi` 源码、注释琐项、IR 比较器、IR → 源码，以及迁移器。`.dsp` 管线 *(自 2.1.0 起)* |

<a id="the-core-only-rule"></a>

## 仅 Core 规则

`DreamShaderLang.Build.cs` 恰好声明一个依赖项：

```csharp
PublicDependencyModuleNames.AddRange(new[] { "Core" });
```

这是一条硬性边界，而不是当前状况：

- **没有 `UObject`，没有 `Engine`，没有 `CoreUObject`。** 该模块中没有任何内容可以命名 `UMaterial`、`UMaterialExpression` 或资产路径类型。
- **没有 `IPluginManager`，没有 `GetDefault<>()`，没有配置访问。** 该模块无法查找有关其运行所在项目的任何信息。
- **引擎事实以参数形式传入。** `DS_*` 预处理器常量、资产根和表达式类名由宿主以纯数据形式提供 —— `DreamShader` 模块中的 [`DreamShaderDefineResolution.h`](index.md#public-headers) 是*构建* define 表的那一半，它会向引擎询问；而 `DreamShaderLang` 只是*消费* define 表。
- **每个头文件都能在 commandlet、测试或未来的独立语言服务器中编译。** 前端不创建任何对象，也不触碰任何文件：`FLangSourceText` 接收的是别人已经读取的文本。

<a id="public-headers"></a>

## 公共头文件

<a id="lang--the-20-front-end"></a>

### `Lang/` —— 2.0 前端

这里的一切都位于 `UE::DreamShader::Lang`。包含路径保留 `Lang/` 前缀，例如 `#include "Lang/LangParser.h"`。

| 头文件 | 用途 |
| :-- | :-- |
| `Lang/LangSource.h` | `FLangSpan`（偏移量 + 长度 + 解析后的基于 1 的行/列）、`ELangFileKind`（`Dss`、`Dsh`、`Dsm`、`Dsf`、`Dsi`、`Dsp` *(自 2.1.0 起)*、`Unknown`）、`GetLangFileKindFromPath` 和 `FLangSourceText` —— 一份源文本及其行表，每个 span 所指向的对象。 |
| `Lang/LangDiagnostic.h` | `ELangSeverity`、`FLangDiagnostic`（一个 `DSHnnnn` 代码、一个严重性、一条 `FText` 消息、一个 span、一个文件路径）和 `FLangDiagnosticSink`，后者为一次运行收集它们。`FLangDiagnosticSink::ToWireString` 以不变（源）形式渲染 `DSHnnnn: message`，用于日志、JSON 和测试黄金文件。 |
| `Lang/LangToken.h` | `ELangTokenKind`、`ELangKeyword`、`FLangToken`、`TryGetLangKeyword`、`GetLangTokenSpelling`。类型名**不是**关键字：`float3`、`Texture2D`、`material` 和用户结构体名都会作为标识符到达解析器。 |
| `Lang/LangLexer.h` | `LexDreamShaderLang` 和 `FLangLexOptions`。一个词法分析器同时服务两种语法。 |
| `Lang/LangAst.h` | 两个前端都产生的树：`FTypeRef`、`FDocBlock`/`FDocDirective`、`ENodeKind`、表达式 / 语句 / 声明节点结构体，以及 `FModule`。所有权沿树向下为 `TUniquePtr`；子种类通过 `As<T>()` 区分 —— 没有 RTTI，没有访问者。一个 `.dsp` 添加 `FBufferDecl`、带其 `FPassStmt` 的 `FPassDecl`（`EPassStmtKind`）、`FPipelineKeyValue` 和 `EPragmaKind::Pipeline` *(自 2.1.0 起)*；其键和值是普通表达式，它们的含义由绑定器决定。 |
| `Lang/LangParser.h` | `ParseDreamShaderLang`、`ParseDreamShaderLangExpression`、`ELangFrontend`、`FLangParseOptions`（`Frontend`、`bKeepTrivia`）、`FLangParseResult`（`Module`、`Diagnostics`，以及用于 1.x 源的 `Legacy`）。 |
| `Lang/LangPrinter.h` | `PrintDreamShaderLang` 以及各节点重载（`PrintDreamShaderLangExpr`，……），加上 `FLangPrintOptions`。 |
| `Lang/LangLegacy.h` | `FLegacyMigrationInfo` —— 1.x 文本所说的内容，与其所变成的 AST 并列：`FLegacyBlock`、`FLegacySection`、`FLegacyParameterDeclaration`、`FLegacyAssetReference`、`FLegacyOutputSelection`、`FLegacyRename`、合成初始化器和指令。绑定器读取它以应用 1.x 规则；迁移器读取它以将它们写出。 |
| `Lang/LangInstanceSource.h` | `.dsi` 文本：`BuildDreamShaderInstanceModule` / `PrintDreamShaderInstance`（一个 `IR::FIRInstance` → 树 → 文本）、`RewriteDreamShaderInstanceSource` 和 `RewriteDreamShaderUniformDefaults`（以 `FLangSourceEdit` 形式对现有文件进行逐值编辑，因此注释和顺序得以保留）、`FormatDreamShaderFloatLiteral`、`MakeDreamShaderIdentifier`。 |
| `Lang/LangPipelineSource.h` *(自 2.1.0 起)* | 不带编译器的 `.dsp` 文本：`CollectDreamShaderPipelineReferences`（已解析 `.dsp` 的每个 `Material = "..."` 和 `Shader = "..."`，各一次，按源码顺序 —— 宿主在绑定前解析的内容）、`BuildDreamShaderPipelineModule` / `PrintDreamShaderPipeline`（一个 `IR::FIRPassPipeline` → 树 → 文本，省略默认值：反编译器的文本）、`RewriteDreamShaderPipelineSource`（Adopt：使现有文件表示某个负载的编辑，仅触及不同的声明和键）、`CompareDreamShaderPipelines`（两个负载的结构相等性，忽略源码引用，每个差异一行），以及 `GetDreamShaderPassInjectionNames` / `GetDreamShaderPassFormatNames` —— 规范拼写，按运行时枚举的顺序。 |
| `Lang/LangFormat.h` | 不带编辑器的 `dsc fmt`：`FormatDreamShaderLangSource`、`FLangFormatOptions` 和 `ELangFormatOutcome`（`Unchanged`、`Changed`、`Skipped`、`Failed`）—— 在保留其琐项的解析之上运行的打印器，拒绝写出它无法担保的文件。 |

<a id="semantic--the-binder"></a>

### `Semantic/` —— 绑定器

`UE::DreamShader::Lang`，一个头文件。绑定是介于“它能解析”和“它能被降级”之间的步骤：解析每个名称、为每个表达式确定类型、将每个调用与签名或目录匹配，并决定文件的产品。

| 头文件 | 用途 |
| :-- | :-- |
| `Semantic/LangBound.h` | `BindDreamShaderLang`、`FBindOptions`、`FLangBindResult` 和 `FBoundModule` —— AST 加上以 `const FNode*` 为键的辅助表：`FBoundStruct`、`FBoundGlobal`、`FBoundFunction`（其中 `EBoundFunctionKind`：entry、helper、export、extern、custom）、`FBoundProduct`、`FBoundExpr`（`EBoundExprKind`）、`FBoundDirectives` 以及 `Directive::` 键名、区域树、解析后的 include 路径，以及模块绑定时所依据的目录。还有 `BuildDreamShaderSymbolIndexJson` —— 语言服务的符号索引，schema `dreamshader-symbol-index` 版本 1。对于 `.dsp` *（自 2.1.0 起）*：`FBoundModule::Pipeline`、一个 `FBoundPipeline`（应用了所有默认值后的规范载荷，以及每个部分所来自的声明），以及宿主在绑定前解析出的内容 —— `FPipelineReferences`，每个 Material 对应一个 `FPipelineMaterialInfo`，每个 `FPipelineShaderInfo` 对应一个 `.usf`，项目的 pass layers 和 `bCustomPassAvailable` —— 作为 `FBindOptions::PipelineReferences` 传递。 |

已绑定的模块 **会保留指向其所绑定的 `FModule` 的指针，以及指向 include 解析器返回的任何内容的指针**；两者都必须比它活得更久。构建器下游的任何东西都不会这样 —— `FIRModule` 不会指向其中任何一个。

<a id="ir--the-graph-ir"></a>

### `IR/` —— 图 IR

`UE::DreamShader::IR`，十二个头文件。IR 是在成为材质图之前的材质图：节点、操作数、属性和产物，其中没有任何引擎类型。include 路径保留 `IR/` 前缀。

| 头文件 | 用途 |
| :-- | :-- |
| `IR/IRTypes.h` | `EIRTypeKind`、`FIRType`（种类 + 组件数 + 纹理种类 + 结构体索引）、`TypeFromBuiltinRef`、`EIRConversion` 和 `ClassifyConversion` —— 回答“这个值是否能放入那个引脚”这一问题的唯一位置。 |
| `IR/IRCoreOps.h` | `EIROp`、`EIRTypingRule`、`FIRCoreOpInfo` 以及各查找函数（`GetCoreOpInfo`、`FindCoreOpByHlslName`、`FindCoreOpByGlslAlias`、`FindCoreOpForBinary`、`FindCoreOpForUnary`、`IsCoreMathOp`）。一个表保存每个核心运算的元数、类型规则、引擎类和引脚名称；binder 读取类型信息，builder 据此构建，emitter 读取类和引脚。 |
| `IR/IRCatalog.h` | `FBuiltinCatalog` 及其 JSON schema：`FCatalogExpression`（class、pins、properties、outputs、aliases、`bIsCustomOutput`）、`FCatalogPin`、`FCatalogProperty`、`FCatalogMaterialAttribute`、`ECatalogValueType`、`LoadBuiltinCatalogFromJson` / `SaveBuiltinCatalogToJson`、`TypeFromCatalogValueType`。**引擎知识到达此模块的唯一通道。** |
| `IR/IR.h` | `FIRValue`、`FIRInput`、`FIRProperty` / `FIRPropertyValue`（`EIRPropertyKind`）、`Prop::` 属性名常量、`FIRSourceRef`、`FIRNode`、`FIRRegion`、`FIRLayoutHint`、`FIRGraph`、`FIRProduct`（`EIRProductKind`、`EIRBackend`）和 `FIRModule`。`.dsp` 的产物是 `EIRProductKind::PassPipeline` *（自 2.1.0 起）*：一个空图，以及 `FIRProduct::PassPipeline` —— 一个由 `FIRPassParameter`、`FIRPassBuffer` 和 `FIRPass` 组成的 `FIRPassPipeline`（带有 `FIRPassBinding`、`FIRPassParam`、`FIRPassFilterClause` / `FIRPassFilterTerm`），每个枚举值都是其 `.dsp` 拼写形式的字符串。各 pass 和验证器的图检查会跳过它（一个流水线必须是其模块的唯一产物，`DSH4334`）；比较器会比较载荷。 |
| `IR/IRBuilder.h` | `FIRBuildOptions`（`MaxUnrolledIterations`、`MaxInlineDepth`、`bKeepDebugNames`、可选的 `Catalog` 覆盖）和 `BuildDreamShaderIR`。 |
| `IR/IRPasses.h` | `FIRPassOptions`（`bFoldConstants`、`bDedupe`、`bPrune`）和 `RunDreamShaderIRPasses`。 |
| `IR/IRValidator.h` | `ValidateDreamShaderIR` —— 让一个 lowering 后的模块对照 IR 自身规则以及目录进行检查。空目录会跳过需要引擎知识的检查，而不是让它们失败，因此手工构建的图可以在单元测试中验证。 |
| `IR/IRDump.h` | `DumpDreamShaderIRText`（`dsc dump-ir` 渲染，以及 IR 语料库用于比对的黄金文件）和 `DumpDreamShaderIRJson`。 |
| `IR/IRCompare.h` | `AreDreamShaderIRModulesEquivalent` 和 `FIRCompareOptions` —— 从根开始比较两个模块，用文字给出第一处差异，并向下追踪到最深的不同节点。“反编译文本含义相同”和“迁移文本构建出相同的图”就是用这个来衡量的。选项说明哪些差异是同一资产以两种方式书写：调试名称、区域、Custom 代码标记和间距、常量与其 `Const*` 孪生体、恒等 swizzle。 |
| `IR/IRInstanceSchema.h` | `BuildParameterSchemaFromIR` —— `.dsi` 所绑定的参数 schema，取自其父级的 IR（编辑器侧从资产构建相同的 schema）。 |
| `IR/IRCustomHlsl.h` | `FCustomNodeCode`（`Code`、`IncludeFilePaths`、`InlinedFunctions`）、`BuildDreamShaderCustomNodeCode`，以及带 `TryParseCustomCodeBodyMarker` 的 `CustomCodeMarker` 语法 —— Custom 节点主体所携带的行标记，`dsc check -Shaders` 通过它将着色器编译错误映射回来。 |
| `IR/IRLayout.h` | 任何节点存在之前的图布局：`EIRLayoutStyle`（`Blocks`、`SourceBands`、`Layered`）及其 `TryParseIRLayoutStyle`、`FIRLayoutOptions`、`FIRLayoutResult` 和各部分、`LayoutDreamShaderIRGraph`、`FindDreamShaderIRLayoutOwner`、无引擎的尺寸估算，以及 `DumpDreamShaderIRLayoutSvg` / `…Json`（schema `dreamshader-ir-layout` 版本 1，即 `dsc dump-layout` 所写的内容）。确定性，且从不改变图。 |

> IR 的 **身份中不包含源位置**：`FIRSourceRef` 附着在节点上以供工具使用
> （资产的 `DreamShader.SourceSpans` 表、节点 ↔ 源代码导航），并且从不属于
> 去重键的一部分。两个结构相同的节点无论写在哪里都会合并。

<a id="decompile-and-migrate--the-way-back"></a>

### `Decompile/` 和 `Migrate/` —— 返回之路

`UE::DreamShader::Lang`，各一个头文件。两者都不了解资产：编辑器模块将图导入到 `FIRModule` 中并交出。

| 头文件 | 用途 |
| :-- | :-- |
| `Decompile/IRToAst.h` | `RaiseDreamShaderIR` —— emitter 的 lowering 后形状就地读回为源码级形状（对静态 bool 的 StaticSwitch 是 `if`，Set-attributes 链是 `m.X = …`）—— 以及带 `FIRToAstOptions` 的 `BuildDreamShaderAstFromIR`（`bEmitLayout`、`bEmitBackend`、`bReadable`、宿主所知的 `extern` 接口和产物路径、头注释）。结果是一个普通的 `FModule`；`PrintDreamShaderLang` 将其变为文本。 |
| `Migrate/LangMigrate.h` | `MigrateDreamShaderLegacyModule` —— 就地重写一个**已绑定**的 1.x 模块，使其在不含旧规则的情况下含义相同（头文件中的表列出了每项重写及其规则编号）—— 以及 `CollectDreamShaderComments`，宿主用它证明没有注释丢失。 |

<a id="module-root--what-moved-here-in-20"></a>

### 模块根目录 —— 2.0 中移到这里的内容

下面四个头文件中有三个在 1.9.x 之前一直位于 `DreamShader` 模块中，并在 2.0 中移入
`DreamShaderLang`。它们保留了其 `UE::DreamShader` 命名空间和 include 路径，因此
**现有调用者无需更改源代码** —— 只需在其 `Build.cs`
依赖列表中添加一个 `DreamShaderLang` 条目。`DreamShaderLangModule.h` 是 2.0 中新增的。

| 头文件 | 用途 |
| :-- | :-- |
| `DreamShaderPreprocessor.h` *（2.0 中移入）* | `PreprocessDreamShaderSource`、`FDreamShaderPreprocessResult`、`DreamShaderSourceHasPreprocessorDirectives`、`EvaluateDreamShaderConditionExpression`、`BuildDreamShaderDefineKeyFragment`。`#if` 解析，保持行数不变。参见 [预处理器](../../language/preprocessor.md)。 |
| `DreamShaderDefineTable.h` *（2.0 中移入）* | `EDreamShaderDefineSource`、`FDreamShaderDefineEntry`、区分大小写的 `FDreamShaderDefineMap` / `FDreamShaderDefineValueMap` 别名、`FDreamShaderDefineTable`、`IsReservedDreamShaderDefineName`、`IsValidDreamShaderDefineName`。纯文本；面向引擎的那一半（用于*填充*表格）留在了 `DreamShader` 模块中作为 `DreamShaderDefineResolution.h`。 |
| `DreamShaderDiagnostic.h` *（2.0 中移入）* | `FDreamShaderError` / `FDreamShaderTextError` —— 1.x 中携带代码的错误结构体，以及记录 `DSHnnnn` 前导数字含义的文档注释。 |
| `DreamShaderLangModule.h` | `FDreamShaderLangModule`。两个生命周期方法均为空：模块对象的存在只是为了让插件描述符能够命名它，并让依赖方能够链接它。没有单例访问器，也没有需要初始化的内容。 |

> `FDreamShaderError`（1.x，`FString`/`FText` 消息）与 `FLangDiagnostic`（2.0，代码 + 严重性 +
> `FText` + 范围）有意共存：1.x 管线仍携带前者，而 2.0 前端
> 从不产生它。它们不会被隐式转换。

<a id="entry-points"></a>

## 入口点

该模块是一条由六次调用组成的管线——预处理、词法分析、解析、绑定、降低、运行各个 pass——外加一个
验证器、两个转储、一个打印器，以及一个用于单独解析单个表达式的调用。

| 调用 | 头文件 | 约定 |
| :-- | :-- | :-- |
| `PreprocessDreamShaderSource(...)` | `DreamShaderPreprocessor.h` | 根据定义表解析 `#if` / `#define`，保持行数不变，使后续每个范围仍指向作者所在的行。 |
| `LexDreamShaderLang(Source, Options, OutTokens, Diagnostics)` | `Lang/LangLexer.h` | 将整段文本切分为词元。当记录了任何错误时返回 `false`；数组始终以 `EndOfFile` 结尾，因此调用方可以继续处理。 |
| `ParseDreamShaderLang(Source, Options)` | `Lang/LangParser.h` | 一段文本 → 一个 `FModule`。结果的 `Module` **即使出现错误后也会存在**，保存所有能干净解析的内容，因此语言服务可以在损坏的文件上继续工作；`Succeeded()` 为 `Module.IsValid() && !Diagnostics.HasErrors()`。词法分析器和解析器的诊断按源代码顺序合并，而范围落在 `/// @custom` 主体内的**词法**诊断会被丢弃——那段文本不属于本语言（参见下文*不透明主体*）。 |
| `ParseDreamShaderLangExpression(Source, Diagnostics)` | `Lang/LangParser.h` | 一段文本 → 一个 `FExpr`，用于测试、语言服务和 `#pragma` 值。表达式之后若残留词元，会使整个结果失败（`DSH3211`），而不是悄悄截断它；这是唯一能引发该代码的入口点。 |
| `PrintDreamShaderLang(Module, Options)` | `Lang/LangPrinter.h` | `FModule` → `.dss` 文本。 |
| `BindDreamShaderLang(Module, Options)` | `Semantic/LangBound.h` | 一个 `FModule` → 一个 `FBoundModule`。`FBindOptions::Catalog` 是**必需的**；没有 `IncludeResolver` 时每个 `#include` 都会被拒绝。与解析器一样，它在出错后仍会继续，因此应询问 `Succeeded()`，而不是询问接收器是否为空。结果借用自 `Module` 和解析器的模块；两者的生命周期都必须长于它。 |
| `BuildDreamShaderSymbolIndexJson(Bound)` | `Semantic/LangBound.h` | 声明、引用、已解析的包含以及参数模式，以 JSON 形式提供，供语言服务使用。可在有错误的模块上工作——这正是编辑器最需要导航的时候。 |
| `BuildDreamShaderIR(Bound, Options, Diagnostics)` | `IR/IRBuilder.h` | `FBoundModule` → `TUniquePtr<FIRModule>`，所有产物均被降低。**即使出错也会返回模块**（部分图正是 `dsc dump-ir` 所展示的），因此调用方应询问 `Diagnostics.HasErrors()`；null 表示已绑定的模块完全没有 `FModule`。结果中没有任何内容指回 AST。 |
| `RunDreamShaderIRPasses(Module, Options, Diagnostics)` | `IR/IRPasses.h` | 常量折叠、结构去重和死节点剪枝，就地进行。`FIRValue` 在其前后保持有效。其诊断仅供参考；pass 从不会让构建失败。 |
| `ValidateDreamShaderIR(Module, Catalog, Diagnostics)` | `IR/IRValidator.h` | 当模块可发射时返回 `true`。无论哪种情况诊断都会落入接收器，全部带有接收器自身的文件路径。 |
| `DumpDreamShaderIRText(Module)` / `…Json(Module)` | `IR/IRDump.h` | 人类可读渲染（`dsc dump-ir`，也是 IR 语料库比对的金标准）和机器可读渲染（`dsc dump-ir -Json`）。 |
| `BuildDreamShaderCustomNodeCode(Bound, FunctionIndex, Out, Diagnostics)` | `IR/IRCustomHlsl.h` | 一个 `/// @custom` 函数 → Custom 节点所携带的 HLSL，连同其提升的 `#include` 及其内嵌辅助函数。确定性的：同一函数的两次调用产生相同的字符串，这正是去重键能够在 `Prop::Code` 上运行的原因。 |
| `LoadBuiltinCatalogFromJson(...)` / `SaveBuiltinCatalogToJson(...)` | `IR/IRCatalog.h` | 目录的磁盘形式。`dsc export-catalog` 写入它；在编辑器之外绑定的工具读取它。 |
| `AreDreamShaderIRModulesEquivalent(A, B, Options, OutDifference)` | `IR/IRCompare.h` | 当两个模块以等价图描述相同产物时为真；否则给出第一处差异（用文字描述）。 |
| `RaiseDreamShaderIR(Module, Diagnostics)` → `BuildDreamShaderAstFromIR(Module, Catalog, Options, Diagnostics)` | `Decompile/IRToAst.h` | 一个 `FIRModule` → 一个 `FModule`，后者可降低回等价的 IR。语言无法表达的内容会成为诊断（`DSH9075`–`DSH9084`），而绝不会被悄悄丢弃。 |
| `MigrateDreamShaderLegacyModule(Module, Legacy, Bound, Options, Diagnostics)` | `Migrate/LangMigrate.h` | 就地重写 1.x 模块。`Bound` 是针对 `Module` 绑定的，且只被读取；**此函数返回后其节点指针即失效**。出错后为 False：此时模块不值得打印。 |
| `PrintDreamShaderInstance(Instance, FilePath, AssetPathOverride)` | `Lang/LangInstanceSource.h` | 实例负载的 `.dsi` 文本：可选的 `/// @name`、`#pragma instance(...)`、每个覆盖项一个 `uniform`。 |
| `CollectDreamShaderPipelineReferences(Module, OutMaterials, OutShaders)` *（自 2.1.0 起）* | `Lang/LangPipelineSource.h` | 已解析 `.dsp` 的每个 `Material = "..."` 和 `Shader = "..."`，各一次，按源代码顺序。宿主将它们解析为 `FPipelineReferences`，并以 `FBindOptions::PipelineReferences` 进行绑定；没有它时，绑定就是一次不依赖引擎的检查——引用按书写形式处理，所有需要引擎事实的检查都被跳过，并给出一条说明这一点的信息（`DSH7360`）。产物是 `FBoundModule::Pipeline` 的负载，以文件名命名；`BuildDreamShaderIR` 将其复制到 `FIRProduct::PassPipeline` 中，不降低任何内容。 |
| `PrintDreamShaderPipeline(Pipeline, FilePath, Options)` *（自 2.1.0 起）* | `Lang/LangPipelineSource.h` | 管线负载的 `.dsp` 文本——`#pragma pipeline`、uniform、缓冲区、pass，按负载顺序排列，默认值省略。反编译器所写入的内容。 |
| `RewriteDreamShaderPipelineSource(Original, Parsed, Bound, Desired, OutEdits, OutText, Diagnostics)` *（自 2.1.0 起）* | `Lang/LangPipelineSource.h` | 针对 `.dsp` 的采纳：通过范围编辑使现有文件状态满足 `Desired`，仅触及有差异的部分，从而保留注释和顺序。对于不是已绑定 `.dsp` 的文件（`DSH9109`）、与其他名称共享且必须变更的声明（`DSH9107`）或重叠的编辑（`DSH9108`），返回 False，并保留 `OutText` 为原值。 |
| `CompareDreamShaderPipelines(A, B, OutDifferences)` *（自 2.1.0 起）* | `Lang/LangPipelineSource.h` | 两个负载的结构相等性，忽略源引用，每处差异一行——`.dsp` → 资产 → `.dsp` 往返和 `AreDreamShaderIRModulesEquivalent` 正是以此衡量管线。 |

```cpp
// MyTooling.Build.cs:  PrivateDependencyModuleNames.AddRange(new[] { "Core", "DreamShaderLang" });

#include "Lang/LangParser.h"
#include "Lang/LangPrinter.h"

using namespace UE::DreamShader::Lang;

void Normalise(const FString& Path, const FString& Text)
{
    const FLangSourceText Source(Path, Text);
    const FLangParseResult Result = ParseDreamShaderLang(Source);

    for (const FLangDiagnostic& Diagnostic : Result.Diagnostics.GetDiagnostics())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s(%d,%d): %s"),
            *Diagnostic.FilePath, Diagnostic.Span.Line, Diagnostic.Span.Column,
            *FLangDiagnosticSink::ToWireString(Diagnostic));
    }

    if (Result.Succeeded())
    {
        const FString Printed = PrintDreamShaderLang(*Result.Module);
    }
}
```

<a id="front-end-selection"></a>

### 前端选择

`FLangParseOptions::Frontend` 默认使用 `ELangFrontend::Auto`，后者根据 `FLangSourceText::GetPath()` 所携带的文件
扩展名来决定：

| 扩展名 | `ELangFileKind` | 前端 |
| :-- | :-- | :-- |
| `.dss` | `Dss` | 2.0 |
| `.dsi` | `Dsi` | 2.0 — 一个材质实例：一个 `#pragma instance` 和 `uniform` 覆盖，仅此而已 |
| `.dsh` | `Dsh` | **按声明**决定：一个共享头文件可以在项目迁移期间同时容纳两种语法。`export` 和资产块会被拒绝 |
| `.dsp` *（自 2.1.0 起）* | `Dsp` | 2.0 — 一个 Custom Pass 管线：`#pragma pipeline`、`uniform` 和 `static const` 声明，`buffer` 和 `pass`。`buffer` 和 `pass` 仅在 `.dsp` 中是声明关键字（在其他位置为 `DSH3310`）；在 `.dsp` 中的函数、结构体或 `#include` 都是 `DSH3314` |
| `.dsm` | `Dsm` | 旧版 |
| `.dsf` | `Dsf` | 旧版 |

旧版前端将 1.x 构造降低到*相同*的 AST 节点种类上——一个 `Shader` 块
变为 uniforms、一个 `#pragma material` 和一个导出函数——因此下游无需知道
是哪个解析器生成了模块。1.x 文本除此之外所表达的内容（块名称、节、重命名、
输出选择）与树并排存放在 `FLangParseResult::Legacy` 中，并且声明携带
`bLegacy`，这正是让绑定器将已记录的 1.x 规则应用于它们而非其他任何内容的原因。

<a id="printer-contract"></a>

### 打印器契约

打印器的承诺是**结构保真，而非文本保真**：`parse(print(parse(X)))` 产生的
树与 `parse(X)` 相同。具体而言——圆括号在 `FParenExpr` 记录它们的位置
以及优先级要求的位置输出；`///` 块在自由
文本之后按每行一个指令重新输出；一个不透明（`/// @custom`）主体从 `RawBody` 逐字写出；pragmas 和 include 保持
其拼写；字面量保持所写的词素，因此 `1.0f`、`0x10` 和 `2u` 得以保留。除 `///` 之外的注释
在解析保留它们时被再现：使用 `FLangParseOptions::bKeepTrivia` 时，每个
注释和空行都附加到它所紧邻的声明或语句上
（`FModule::Trivia`），并且打印器将它们写回——这正是让 `dsc migrate` 承诺
不丢失任何注释的原因。没有该选项时，它们像以前一样被丢弃。

`FLangPrintOptions::NewLine` 默认为 `\n`，但不透明主体是其源文件的一个片段，
保留该文件的终止符，因此打印一个 CRLF 文件会产生混合终止符。这是稳定的：
下一次解析会切出相同的字节，因此固定点仍然成立。

<a id="opaque-bodies"></a>

### 不透明主体

其 `///` 块携带 `@custom` 的函数，其主体被逐字捕获：`FFunctionDecl::Body`
为 null，`bOpaqueBody` 为 true，`RawBody` 是大括号之间的文本，`BodySpan` 覆盖
包括大括号在内的范围。解析器只读取大括号——它们必须配对，并且其中的字符串、注释和
`#` 行无法使它们失配，因为词法分析器已将每一个变为单一词法单元。

由于该文本不是 DreamShaderLang，`ParseDreamShaderLang` **丢弃跨度起始于 `BodySpan` 内部的每一条词法诊断**。
其中的一个 `$`、一个 `@`、一个 `'` 或一个行中 `#` 并不是
关于程序的事实，而着色器编译器才是评判它的那一个。抑制
仅限于那些跨度：在外部一行处的相同字符仍然是 `DSH2101` / `DSH2106`。

<a id="diagnostics"></a>

## 诊断

每个抛出点都字面地命名其代码，因此 `.skill/gen-diagnostics.ps1` 可以找到它：

```cpp
Diagnostics.Error(TEXT("DSH2151"), Span, LOCTEXT("ExpectedExpression", "expected an expression"));
```

这些消息在每文件 `LOCTEXT_NAMESPACE` 下是 `LOCTEXT`，形式为
`"DreamShader.Lang.<Unit>"`，因此编辑器可以对它们进行本地化。**对代码进行断言，绝不要对
消息文本进行断言。**

| 范围 | 抛出者 | 固定锚点 |
| :-- | :-- | :-- |
| `DSH2101`–`DSH2119` | 词法分析器（已分配：`2101`–`2106`） | `2101` 未知字符 · `2102` 未终止的块注释 · `2103` 未终止的字符串 · `2104` 未知转义 · `2105` 格式错误的数字 · `2106` 一个不是其行首内容的 `#` |
| `DSH2150`–`DSH2189` | 表达式和语句（已分配：`2150`–`2155`，`2157`–`2165`） | `2150` 意外的文件结尾 · `2158` 命名参数之后的位置参数 · `2160` 不支持的语句（`switch`，以及主体内的一个 `#` 行） · `2162` 用作表达式的初始化列表 |
| `DSH2199` | 前端选择 | 在旧版前端到来时退役 |
| `DSH2200`–`DSH2258` | 旧版前端 — 块、特性列表、`import`、Graph 语句 | `2200` 一个 1.x 静默截断的运算符 · `2248` 在 `.dsm` / `.dsf` 中的 2.0 语法 · `2249` 在 `.dsh` 中的资产块 · `2252` 一个 `import`，include 解析器不会读取它 |
| `DSH2300`–`DSH2349` | `.dsp` 语法 — `buffer` 和 `pass` 声明、pass 主体、绑定 | |
| `DSH3250`–`DSH3278` | 旧版前端 — `Properties`、`Settings`、`Outputs`、`Inputs`、`Layout` 节 | |
| `DSH3300`–`DSH3349` | `.dsp` 声明 — `#pragma pipeline`、buffer 和 pass 键、另一源中的 `.dsp` 专属声明 | |
| `DSH6300`–`DSH6330` | 旧版前端和绑定器 — `Function` / `GraphFunction`、提升的 `UE.` 调用 | `6326` 一个提升的调用读取主体局部变量 |
| `DSH3200`–`DSH3249` | 声明、类型、指令、文档块（已分配：`3200`–`3208`，`3210`，`3211`，`3213`–`3218`，`3220`–`3222`） | `3201` 散落的预处理器指令 · `3203` `#include` / `import` 没有**双引号**路径 · `3204` 期望类型名 · `3208` 函数没有主体 · `3213` 错误的存储/链接组合 · `3220` 格式错误的 `@` 指令（**警告**） · `3221` 孤立的 `///` 块（**警告**） · `3222` 一个 1.x 声明词 |

绑定器、构建器、passes、验证器和自定义 HLSL 构建器通过
同一个接收器抛出，并带有它们自己的范围：

| 范围 | 引发者 |
| :-- | :-- |
| `DSH4200`–`DSH4299` | 绑定器 —— 名称、类型、表达式、语句、循环、区域 |
| `DSH4300`–`DSH4349` | IR 验证器 |
| `DSH4350`–`DSH4399` | 降级拒绝 —— 一个在折叠后仍然存在的矩阵、一个无法证明行程计数的循环、分支内部的 `discard`、在写入之前读取的材质属性 |
| `DSH4400`–`DSH4449` | `.dsp` 名称与引用 —— 缓冲区、材质、HLSL 文件、入口、层 |
| `DSH5200`–`DSH5299` | 反射调用、目录、材质属性 |
| `DSH5300`–`DSH5314` | `.dss` 中的 Custom Pass 材质节点 —— 引擎门控、它们可以出现的位置、新的翻译器 |
| `DSH6200`–`DSH6219` | 函数种类、入口、`export` / `extern` |
| `DSH6220`–`DSH6249` | 辅助函数内联 |
| `DSH6250`–`DSH6299` | `/// @custom` HLSL |
| `DSH7200`–`DSH7249` | uniform、`///` 指令、`#pragma material` |
| `DSH7250`–`DSH7270` | `.dsi`：`#pragma instance`，相对于父级模式的覆盖 |
| `DSH7300`–`DSH7379` | `.dsp` 检查（`V1`–`V13`）：pass 类型与注入点的匹配、按帧顺序的缓冲区使用、针对材质或 HLSL 文件的绑定、限制 |
| `DSH5250`–`DSH5292` | 已记录的 1.x 规则（`L1`–`L27`），当某条规则重写或丢弃某些内容时，每一条都对应一个诊断 |
| `DSH9042`–`DSH9044` | 格式化器：1.x 文件和包含预处理器行的文件会被跳过（信息），格式化后无法通过自身检查的文本会被拒绝 |
| `DSH9075`–`DSH9084` | IR → 源代码：写入器重命名了、无法放置的、或保留为默认值的内容 |
| `DSH9107`–`DSH9109` | `.dsi` 以及*自 2.1.0 起* `.dsp`（Adopt）的就地重写：共享声明、重叠编辑、错误的文件类型 |
| `DSH9091`、`DSH9094` | 迁移器（编辑器模块中围绕它的宿主证明构成了 `DSH9090`–`DSH9099` 的其余部分） |

`DSH1030`–`DSH1042`（预处理器自身的代码）已随预处理器移入此模块，并保留了原有的编号。每个代码的说明文字位于[诊断](../diagnostics/index.md)之下。

<a id="testing-the-lang-corpus"></a>

## 测试：`Lang` 语料库

前端由**数据驱动的语料库**覆盖，而不是由针对每个功能手工编写的测试覆盖。测试夹具位于 `Tests/Corpus/Lang/` 之下，每个功能对应一个 `.dss` 或 `.dsh`，每个都可选择性地搭配一个 `<name>.expected.json` 黄金文件；一个复杂的自动化测试会在运行时枚举该目录树，并将每个夹具作为独立的子测试呈现。

| | |
| :-- | :-- |
| 测试 | `DreamShader.Lang2.Corpus.<Area>.<Fixture>` |
| 运行器 | `RunDreamShaderLangCorpusCase`，位于 `Source/DreamShaderEditor/Private/Tests/DreamShaderTestCommon.h` |
| 注册 | `Source/DreamShaderEditor/Private/Tests/DreamShaderCorpusLangTests.cpp` |
| 夹具 | `Tests/Corpus/Lang/{Lexical,Expressions,Statements,Declarations,Examples}/` —— `.dss` 和 `.dsh`，外加一个用于固定前端选择的 `.dsm`：它会到达旧版前端并在那里失败（`DSH2241`） |
| 黄金模式 | `"entryPoint": "lang"` —— 参见 [`Tests/Corpus/README.md`](../../../Tests/Corpus/README.md) |

每个用例都会通过 `ParseDreamShaderLang` 解析一次，然后与其黄金文件进行比对：

- `outcome` —— `ok` 或 `error`。没有黄金文件时，默认为“正向夹具应解析成功，`*.bad.*` 夹具应失败”。
- `errorContains` / `warningsContain` —— 该严重级别下每条诊断的 `FLangDiagnosticSink::ToWireString` 的子字符串（大小写**敏感**）。实际上就是：`DSHnnnn` 代码。
- `module` —— 对解析后的 `FModule` 进行结构化计数：`declarations`（pragma 和 include 计入）、`pragmas`、`includes`、`structs`、`uniforms`（仅 `Uniform` 存储）、`constants`（仅 `StaticConst`）、`functions`（全部三种链接方式）、`exports`、`externs`、`opaqueBodies`、`entry`（签名以 `void (inout material)` 开头的第一个函数，**无论链接方式如何**）以及 `roundtrip`。
- `roundtrip` —— 打印 → 解析 → 打印的结果逐字节一致，这正是打印器契约以测试形式表达的结果。

为新的构造添加覆盖范围只需在该目录树中放入一个文件；无需修改 C++，无需重新编译。以无头方式运行整个层：

```
"F:\UnrealEngine\UE_Moon\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "I:\UnrealProject_Moon\DEV_58\MoonEngineSample\MoonEngineSample.uproject" ^
  -ExecCmds="Automation RunTests DreamShader.Lang2; Quit" ^
  -nullrhi -unattended -nopause -nosplash -log
```

添加 `-DreamShaderUpdateGolden` 会根据实际结果重写每个黄金文件，而不是进行断言。提交前请手动审查差异 —— 该开关正是回归变为基线的途径。

除语料库之外，每个前端单元都有自己专门的测试文件（`DreamShaderLangLexerTests.cpp`、`…DeclarationTests.cpp`、`…ExpressionTests.cpp`、`…PrinterTests.cpp`），全部位于 `DreamShader.Lang2.*` 之下。

<a id="see-also"></a>

## 另请参见

- [2.0 管线做了什么](../language-v2/index.md) —— 从作者角度介绍语言表面和管线的限制
- [C++ API 索引](index.md) —— 其他四个模块
- [Custom Pass 管线](../language-v2/passes.md) —— 从作者角度介绍 `.dsp` 语言
- [预处理器](../../language/preprocessor.md) —— `#if` 和 define 表，从语言角度进行记录
- [诊断索引](../diagnostics/index.md) —— 每个 `DSHnnnn` 及其原因和修复方法
- [测试](../../contributing/testing.md) —— 自动化测试套件和夹具语料库
