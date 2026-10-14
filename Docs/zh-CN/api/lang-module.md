---
title: "`DreamShaderLang`"
description: "`DreamShaderLang`"
---

<a id="dreamshaderlang"></a>

# `DreamShaderLang`

> [DreamShader](../index.md) » [C++ API](index.md) » **`DreamShaderLang`**

`DreamShaderLang` 是前端**也是中端**：文本输入，输出的是降低后的图——反过来也一样。词法分析器、预处理器、两个解析器（2.0 和 1.x 旧版前端）、AST、打印器、绑定器、带各遍处理的图 IR、验证器和比较器、把 IR 变回源码的写出器、1.x → 2.0 迁移器，以及诊断类型都在这里——除此之外再无他物。它是为 2.0 从 `DreamShader` 中拆分出来的，这样语言就可以在没有编辑器的情况下被演练、被语言服务复用，并作为一门语言而非一个插件来被推演。

该模块*不*跨越的那条线是引擎：一个 `FIRModule` 不指名任何 `UObject`、任何资产，也不指名任何表达式类对象。引擎知识以纯数据的形式，在一个 [`FBuiltinCatalog`](#ir--the-graph-ir) 中抵达；后者由 Editor 模块通过反射填充，也可以保存到 JSON，以及从 JSON 加载。

| | |
| :-- | :-- |
| 模块 | `DreamShaderLang` |
| 类型 / 加载阶段 | `Runtime` / `PostConfigInit` |
| 依赖 | **仅 `Core`** |
| 导出宏 | `DREAMSHADERLANG_API` |
| 公共头文件 | 30 个：模块根目录下 4 个 + `Lang/` 下 11 个 + `Semantic/` 下 1 个 + `IR/` 下 12 个 + `Decompile/` 下 1 个 + `Migrate/` 下 1 个 |
| 命名空间 | `UE::DreamShader::Lang`（2.0 前端和绑定器）· `UE::DreamShader::IR`（图 IR 和内置目录）· `UE::DreamShader`（预处理器、定义表、`FDreamShaderError`） |
| 反射类型 | **无**——没有 `UCLASS`，没有 `USTRUCT`，没有 `UENUM` |
| 状态 | `2.0` 新增：词法分析器、2.0 解析器和打印器；绑定器、IR、其构建器、各遍处理与验证器，以及自定义 HLSL 构建器；旧版前端、`.dsi` 源码、注释杂项、IR 比较器、IR → 源码，以及迁移器。`.dsp` 管线 *(自 2.1.0 起)* |

<a id="the-core-only-rule"></a>

## 仅依赖 Core 的规则

`DreamShaderLang.Build.cs` 只声明一个依赖：

```csharp
PublicDependencyModuleNames.AddRange(new[] { "Core" });
```

这是一条硬性边界，而不是当前状况：

- **没有 `UObject`，没有 `Engine`，没有 `CoreUObject`。** 该模块中没有任何东西可以指名 `UMaterial`、`UMaterialExpression` 或资产路径类型。
- **没有 `IPluginManager`，没有 `GetDefault<>()`，没有配置访问。** 该模块无法查找任何关于其运行所在项目的信息。
- **引擎事实以参数形式抵达。** `DS_*` 预处理器常量、资产根和表达式类名由宿主以纯数据形式提供——`DreamShader` 模块中的 [`DreamShaderDefineResolution.h`](index.md#public-headers) 是通过询问引擎来*构建*定义表的那一半；`DreamShaderLang` 只*消费*它。
- **每个头文件都能在 commandlet、测试或未来的独立语言服务器中编译。** 前端不创建任何对象，也不触碰任何文件：`FLangSourceText` 拿到的是别人已经读取的文本。

<a id="public-headers"></a>

## 公共头文件

<a id="lang--the-20-front-end"></a>

### `Lang/` — 2.0 前端

这里的一切都在 `UE::DreamShader::Lang` 中。包含路径保留 `Lang/` 前缀，例如 `#include "Lang/LangParser.h"`。

| 头文件 | 用途 |
| :-- | :-- |
| `Lang/LangSource.h` | `FLangSpan`（偏移 + 长度 + 解析后的从 1 开始的行/列）、`ELangFileKind`（`Dss`、`Dsh`、`Dsm`、`Dsf`、`Dsi`、`Dsp` *(自 2.1.0 起)*、`Unknown`）、`GetLangFileKindFromPath`，以及 `FLangSourceText`——一份源文本及其行表，所有跨度都指向其中的东西。 |
| `Lang/LangDiagnostic.h` | `ELangSeverity`、`FLangDiagnostic`（一个 `DSHnnnn` 代码、一个严重级别、一条 `FText` 消息、一个跨度、一个文件路径）以及 `FLangDiagnosticSink`，后者为一次运行收集它们。`FLangDiagnosticSink::ToWireString` 以不变（源）形式渲染 `DSHnnnn: message`，用于日志、JSON 和测试金标准。 |
| `Lang/LangToken.h` | `ELangTokenKind`、`ELangKeyword`、`FLangToken`、`TryGetLangKeyword`、`GetLangTokenSpelling`。类型名**不是**关键字：`float3`、`Texture2D`、`material` 和用户结构体名都作为标识符抵达解析器。 |
| `Lang/LangLexer.h` | `LexDreamShaderLang` 和 `FLangLexOptions`。一个词法分析器同时服务于两种语法。 |
| `Lang/LangAst.h` | 两个前端都会生成的树：`FTypeRef`、`FDocBlock`/`FDocDirective`、`ENodeKind`、表达式 / 语句 / 声明节点结构体，以及 `FModule`。所有关系沿树向下为 `TUniquePtr`；子种类通过 `As<T>()` 区分——没有 RTTI，没有访问者。一个 `.dsp` 会增加 `FBufferDecl`、带其各 `FPassStmt` 的 `FPassDecl`（`EPassStmtKind`）、`FPipelineKeyValue` 以及 `EPragmaKind::Pipeline` *(自 2.1.0 起)*；它们的键和值都是普通表达式，其含义由绑定器决定。 |
| `Lang/LangParser.h` | `ParseDreamShaderLang`、`ParseDreamShaderLangExpression`、`ELangFrontend`、`FLangParseOptions`（`Frontend`、`bKeepTrivia`）、`FLangParseResult`（`Module`、`Diagnostics`，以及用于 1.x 源码的 `Legacy`）。 |
| `Lang/LangPrinter.h` | `PrintDreamShaderLang` 以及各节点的重载（`PrintDreamShaderLangExpr`、……），外加 `FLangPrintOptions`。 |
| `Lang/LangLegacy.h` | `FLegacyMigrationInfo`——一段 1.x 文本说了什么，就摆在它变成的 AST 旁边：`FLegacyBlock`、`FLegacySection`、`FLegacyParameterDeclaration`、`FLegacyAssetReference`、`FLegacyOutputSelection`、`FLegacyRename`、合成出的初始化器和指令。绑定器读取它以应用 1.x 规则；迁移器读取它以将其写出。 |
| `Lang/LangInstanceSource.h` | `.dsi` 文本：`BuildDreamShaderInstanceModule` / `PrintDreamShaderInstance`（一趟 `IR::FIRInstance` → 树 → 文本）、`RewriteDreamShaderInstanceSource` 和 `RewriteDreamShaderUniformDefaults`（对现有文件逐值编辑，以 `FLangSourceEdit` 形式进行，因此注释和顺序得以保留）、`FormatDreamShaderFloatLiteral`、`MakeDreamShaderIdentifier`。 |
| `Lang/LangPipelineSource.h` *(自 2.1.0 起)* | 不带编译器的 `.dsp` 文本：`CollectDreamShaderPipelineReferences`（一个已解析 `.dsp` 的每个 `Material = "..."` 和 `Shader = "..."`，各一次，按源码顺序——即宿主在绑定之前解析出的内容）、`BuildDreamShaderPipelineModule` / `PrintDreamShaderPipeline`（一趟 `IR::FIRPassPipeline` → 树 → 文本，省略默认值：反编译器的文本）、`RewriteDreamShaderPipelineSource`（Adopt：使现有文件陈述某个负载的编辑，只触碰有差异的声明和键）、`CompareDreamShaderPipelines`（两个负载的结构相等性，忽略源码引用，每处差异一行），以及 `GetDreamShaderPassInjectionNames` / `GetDreamShaderPassFormatNames`——即规范拼写，按运行时枚举的顺序排列。 |
| `Lang/LangFormat.h` | 不带编辑器的 `dsc fmt`：`FormatDreamShaderLangSource`、`FLangFormatOptions` 和 `ELangFormatOutcome`（`Unchanged`、`Changed`、`Skipped`、`Failed`）——在保留了注释杂项的解析结果之上运行打印器，拒绝写出它无法担保的文件。 |

<a id="semantic--the-binder"></a>

### `Semantic/` — 绑定器

`UE::DreamShader::Lang`，一个头文件。绑定是介于“它能解析”和“它能被降低”之间的步骤：每个名字都得到解析，每个表达式都得到类型，每次调用都与某个签名或目录相匹配，文件的产物也被决定。

| 头文件 | 用途 |
| :-- | :-- |
| `Semantic/LangBound.h` | `BindDreamShaderLang`、`FBindOptions`、`FLangBindResult` 和 `FBoundModule` —— AST 以及以 `const FNode*` 为键的附属表：`FBoundStruct`、`FBoundGlobal`、`FBoundFunction`（包含 `EBoundFunctionKind`：entry、helper、export、extern、custom）、`FBoundProduct`、`FBoundExpr`（`EBoundExprKind`）、`FBoundDirectives` 以及 `Directive::` 键名、区域树、解析后的 include 路径，以及该模块绑定时所依据的目录。还有 `BuildDreamShaderSymbolIndexJson` —— 语言服务的符号索引，模式 `dreamshader-symbol-index` 版本 1。对于 `.dsp` *（自 2.1.0 起）*：`FBoundModule::Pipeline`、一个 `FBoundPipeline`（应用了所有默认值后的规范载荷，以及每个部分来自的声明），以及宿主在绑定之前解析的内容 —— `FPipelineReferences`，每个材质一个 `FPipelineMaterialInfo`，每个 `.usf` 一个 `FPipelineShaderInfo`，项目的 pass 层和 `bCustomPassAvailable` —— 作为 `FBindOptions::PipelineReferences` 传入。 |

已绑定的模块**会保留指向它所绑定的 `FModule` 以及 include 解析器返回的任何内容的指针**；两者都必须比它存活更久。构建器下游的任何内容都不这样做 —— 一个 `FIRModule` 两者都不指回。

<a id="ir--the-graph-ir"></a>

### `IR/` —— 图 IR

`UE::DreamShader::IR`，十二个头文件。IR 是成为材质图之前的材质图：节点、操作数、属性和产物，其中没有任何引擎类型。include 路径保留 `IR/` 前缀。

| 头文件 | 用途 |
| :-- | :-- |
| `IR/IRTypes.h` | `EIRTypeKind`、`FIRType`（种类 + 组件数量 + 纹理种类 + 结构体索引）、`TypeFromBuiltinRef`、`EIRConversion` 和 `ClassifyConversion` —— 回答“此值是否适合该引脚”这一问题的唯一地方。 |
| `IR/IRCoreOps.h` | `EIROp`、`EIRTypingRule`、`FIRCoreOpInfo` 以及查找（`GetCoreOpInfo`、`FindCoreOpByHlslName`、`FindCoreOpByGlslAlias`、`FindCoreOpForBinary`、`FindCoreOpForUnary`、`IsCoreMathOp`）。一张表保存每个核心操作符的元数、类型规则、引擎类和引脚名称；绑定器读取类型，构建器据此构建，发射器读取类和引脚。 |
| `IR/IRCatalog.h` | `FBuiltinCatalog` 及其 JSON 模式：`FCatalogExpression`（类、引脚、属性、输出、别名、`bIsCustomOutput`）、`FCatalogPin`、`FCatalogProperty`、`FCatalogMaterialAttribute`、`ECatalogValueType`、`LoadBuiltinCatalogFromJson` / `SaveBuiltinCatalogToJson`、`TypeFromCatalogValueType`。**引擎知识到达此模块的唯一通道。** |
| `IR/IR.h` | `FIRValue`、`FIRInput`、`FIRProperty` / `FIRPropertyValue`（`EIRPropertyKind`）、`Prop::` 属性名常量、`FIRSourceRef`、`FIRNode`、`FIRRegion`、`FIRLayoutHint`、`FIRGraph`、`FIRProduct`（`EIRProductKind`、`EIRBackend`）和 `FIRModule`。一个 `.dsp` 的产物是 `EIRProductKind::PassPipeline` *（自 2.1.0 起）*：一个空图，以及 `FIRProduct::PassPipeline` —— 一个由 `FIRPassParameter`、`FIRPassBuffer` 和 `FIRPass` 组成的 `FIRPassPipeline`（包含 `FIRPassBinding`、`FIRPassParam`、`FIRPassFilterClause` / `FIRPassFilterTerm`），每个枚举值都是其 `.dsp` 拼写的字符串。pass 和验证器的图检查会跳过它（一个管线必须是其模块的唯一产物，`DSH4334`）；比较器比较载荷。 |
| `IR/IRBuilder.h` | `FIRBuildOptions`（`MaxUnrolledIterations`、`MaxInlineDepth`、`bKeepDebugNames`、可选的 `Catalog` 覆盖）和 `BuildDreamShaderIR`。 |
| `IR/IRPasses.h` | `FIRPassOptions`（`bFoldConstants`、`bDedupe`、`bPrune`）和 `RunDreamShaderIRPasses`。 |
| `IR/IRValidator.h` | `ValidateDreamShaderIR` —— 根据 IR 自身规则和目录验证一个已降低的模块。空目录会跳过需要引擎知识的检查，而不是使其失败，因此手工构建的图可以在单元测试中验证。 |
| `IR/IRDump.h` | `DumpDreamShaderIRText`（`dsc dump-ir` 渲染，以及 IR 语料库进行差异比较的黄金基准）和 `DumpDreamShaderIRJson`。 |
| `IR/IRCompare.h` | `AreDreamShaderIRModulesEquivalent` 和 `FIRCompareOptions` —— 从它们的根开始比较两个模块，用文字描述第一个差异，并一直追踪到最深的差异节点。“反编译文本含义相同”和“迁移文本构建相同的图”就是用这个来衡量的。选项说明哪些差异是同一资产以两种方式书写：调试名称、区域、Custom 代码标记和间距、常量与其 `Const*` 孪生、恒等 swizzle。 |
| `IR/IRInstanceSchema.h` | `BuildParameterSchemaFromIR` —— 一个 `.dsi` 所绑定的参数模式，取自其父级的 IR（编辑器端从资产构建相同的模式）。 |
| `IR/IRCustomHlsl.h` | `FCustomNodeCode`（`Code`、`IncludeFilePaths`、`InlinedFunctions`）、`BuildDreamShaderCustomNodeCode`，以及带 `TryParseCustomCodeBodyMarker` 的 `CustomCodeMarker` 语法 —— Custom 节点主体携带的行标记，`dsc check -Shaders` 通过它映射回着色器编译错误。 |
| `IR/IRLayout.h` | 任何节点存在之前的图布局：`EIRLayoutStyle`（`Blocks`、`SourceBands`、`Layered`）包含 `TryParseIRLayoutStyle`、`FIRLayoutOptions`、`FIRLayoutResult` 及其部分、`LayoutDreamShaderIRGraph`、`FindDreamShaderIRLayoutOwner`、无引擎的尺寸估计，以及 `DumpDreamShaderIRLayoutSvg` / `…Json`（模式 `dreamshader-ir-layout` 版本 1，`dsc dump-layout` 写入的内容）。确定性，且从不更改图。 |

> IR 在其标识中**不保存源位置**：`FIRSourceRef` 搭载在节点上供工具使用
> （资产的 `DreamShader.SourceSpans` 表、节点 ↔ 源导航），并且从不属于
> 去重键的一部分。两个结构相同的节点无论写在哪里都会合并。

<a id="decompile-and-migrate--the-way-back"></a>

### `Decompile/` 和 `Migrate/` —— 返回之路

`UE::DreamShader::Lang`，各一个头文件。两者都不知道资产：编辑器模块将图导入 `FIRModule` 并移交。

| 头文件 | 用途 |
| :-- | :-- |
| `Decompile/IRToAst.h` | `RaiseDreamShaderIR` —— 发射器的已降低形状就地读回为源级别形状（static bool 上的 StaticSwitch 是 `if`，Set-attributes 链是 `m.X = …`）—— 以及带 `FIRToAstOptions` 的 `BuildDreamShaderAstFromIR`（`bEmitLayout`、`bEmitBackend`、`bReadable`、宿主知道的 `extern` 接口和产品路径、标题注释）。结果是一个普通的 `FModule`；`PrintDreamShaderLang` 使其成为文本。 |
| `Migrate/LangMigrate.h` | `MigrateDreamShaderLegacyModule` —— 将**已绑定的** 1.x 模块就地重写为在不使用旧规则的情况下含义相同的模块（头文件的表格列出了每个重写及其规则编号）—— 以及 `CollectDreamShaderComments`，宿主用它来证明没有注释丢失。 |

<a id="module-root--what-moved-here-in-20"></a>

### 模块根 —— 2.0 中移到此处的部分

下面四个头文件中有三个在 1.9.x 之前位于 `DreamShader` 模块中，并在 2.0 中移入
`DreamShaderLang`。它们保留了 `UE::DreamShader` 命名空间和 include 路径，因此
**现有调用者无需更改源代码** —— 只需在其 `Build.cs`
依赖列表中添加一个 `DreamShaderLang` 条目。`DreamShaderLangModule.h` 是 2.0 中新增的。

| 头文件 | 用途 |
| :-- | :-- |
| `DreamShaderPreprocessor.h` *（2.0 中迁移）* | `PreprocessDreamShaderSource`、`FDreamShaderPreprocessResult`、`DreamShaderSourceHasPreprocessorDirectives`、`EvaluateDreamShaderConditionExpression`、`BuildDreamShaderDefineKeyFragment`。`#if` 解析，保持行数不变。见[预处理器](../language/preprocessor.md)。 |
| `DreamShaderDefineTable.h` *（2.0 中迁移）* | `EDreamShaderDefineSource`、`FDreamShaderDefineEntry`、区分大小写的 `FDreamShaderDefineMap` / `FDreamShaderDefineValueMap` 别名、`FDreamShaderDefineTable`、`IsReservedDreamShaderDefineName`、`IsValidDreamShaderDefineName`。纯文本；负责*填充*表格的面向引擎的那一半留在了 `DreamShader` 模块中，即 `DreamShaderDefineResolution.h`。 |
| `DreamShaderDiagnostic.h` *（2.0 中迁移）* | `FDreamShaderError` / `FDreamShaderTextError` —— 1.x 中携带代码的错误结构体，以及记录 `DSHnnnn` 首位数字含义的文档注释。 |
| `DreamShaderLangModule.h` | `FDreamShaderLangModule`。两个生命周期方法均为空：模块对象的存在只是为了让插件描述符能够命名它、让依赖方能够链接它。没有单例访问器，也没有任何需要初始化的内容。 |

> `FDreamShaderError`（1.x，`FString`/`FText` 消息）与 `FLangDiagnostic`（2.0，代码 + 严重级别 +
> `FText` + 跨度）是刻意共存的：1.x 管线仍然携带前者，而 2.0 前端
> 从不产生它。它们之间不会隐式转换。

<a id="entry-points"></a>

## 入口点

该模块是一条由六次调用组成的管线——预处理、词法分析、解析、绑定、降级、运行各趟处理——外加一个
验证器、两个转储、一个打印器，以及一个用于单独解析单个表达式的调用。

| 调用 | 头文件 | 约定 |
| :-- | :-- | :-- |
| `PreprocessDreamShaderSource(...)` | `DreamShaderPreprocessor.h` | 根据定义表解析 `#if` / `#define`，保持行数不变，因此之后每个跨度仍指向作者所写的行。 |
| `LexDreamShaderLang(Source, Options, OutTokens, Diagnostics)` | `Lang/LangLexer.h` | 将整段文本切分为词法单元。当记录到任何错误时返回 `false`；数组始终以 `EndOfFile` 结尾，因此调用方可以继续处理。 |
| `ParseDreamShaderLang(Source, Options)` | `Lang/LangParser.h` | 一段文本 → 一个 `FModule`。**即使在出错之后**，结果中仍会存在 `Module`，保留所有成功解析的内容，因此语言服务可以在损坏的文件上继续工作；`Succeeded()` 为 `Module.IsValid() && !Diagnostics.HasErrors()`。词法分析器和解析器的诊断按源顺序合并，而跨度落在 `/// @custom` 主体内部的**词法**诊断会被丢弃——那段文本不属于本语言（见下文*不透明主体*）。 |
| `ParseDreamShaderLangExpression(Source, Diagnostics)` | `Lang/LangParser.h` | 一段文本 → 一个 `FExpr`，用于测试、语言服务和 `#pragma` 值。表达式之后若还有剩余词法单元，则丢弃整个结果（`DSH3211`），而不是静默截断；这是唯一能引发该代码的入口点。 |
| `PrintDreamShaderLang(Module, Options)` | `Lang/LangPrinter.h` | `FModule` → `.dss` 文本。 |
| `BindDreamShaderLang(Module, Options)` | `Semantic/LangBound.h` | 一个 `FModule` → 一个 `FBoundModule`。`FBindOptions::Catalog` 是**必需**的；没有 `IncludeResolver` 时每个 `#include` 都会被拒绝。与解析器一样，它在出错后仍会继续，因此应询问 `Succeeded()`，而不是判断接收器是否为空。结果借用自 `Module` 以及解析器的模块；两者都必须比它存活更久。 |
| `BuildDreamShaderSymbolIndexJson(Bound)` | `Semantic/LangBound.h` | 声明、引用、已解析的包含项以及参数架构，以 JSON 形式提供，供语言服务使用。可在存在错误的模块上工作——这正是编辑器最需要导航的时候。 |
| `BuildDreamShaderIR(Bound, Options, Diagnostics)` | `IR/IRBuilder.h` | `FBoundModule` → `TUniquePtr<FIRModule>`，所有产物均已降级。**即使在出错之后也会返回模块**（部分图正是 `dsc dump-ir` 所展示的内容），因此调用方询问 `Diagnostics.HasErrors()`；null 表示绑定的模块根本没有 `FModule`。结果中没有任何内容指回 AST。 |
| `RunDreamShaderIRPasses(Module, Options, Diagnostics)` | `IR/IRPasses.h` | 常量折叠、结构去重和死节点剪除，原地进行。在此过程中 `FIRValue` 保持有效。其诊断仅供参考；一趟处理从不会使构建失败。 |
| `ValidateDreamShaderIR(Module, Catalog, Diagnostics)` | `IR/IRValidator.h` | 当模块可生成时返回 `true`。无论哪种情况诊断都会落到接收器中，全部标记为接收器自身的文件路径。 |
| `DumpDreamShaderIRText(Module)` / `…Json(Module)` | `IR/IRDump.h` | 人类可读的渲染形式（`dsc dump-ir`，也是 IR 语料库进行差异比对的黄金文件）和机器可读形式（`dsc dump-ir -Json`）。 |
| `BuildDreamShaderCustomNodeCode(Bound, FunctionIndex, Out, Diagnostics)` | `IR/IRCustomHlsl.h` | 一个 `/// @custom` 函数 → 自定义节点所携带的 HLSL，包括其提升的 `#include` 及内嵌辅助函数。具有确定性：对同一函数调用两次产生相同字符串，这正是去重键能够遍历 `Prop::Code` 的原因。 |
| `LoadBuiltinCatalogFromJson(...)` / `SaveBuiltinCatalogToJson(...)` | `IR/IRCatalog.h` | 目录的磁盘形式。`dsc export-catalog` 负责写入；在编辑器之外进行绑定的工具负责读取。 |
| `AreDreamShaderIRModulesEquivalent(A, B, Options, OutDifference)` | `IR/IRCompare.h` | 当两个模块以等价图描述相同产物时为真；否则给出第一处差异，以文字表述。 |
| `RaiseDreamShaderIR(Module, Diagnostics)` → `BuildDreamShaderAstFromIR(Module, Catalog, Options, Diagnostics)` | `Decompile/IRToAst.h` | 一个 `FIRModule` → 一个 `FModule`，并降级回等价的 IR。语言无法表达的内容会成为诊断（`DSH9075`–`DSH9084`），绝不会被静默丢弃。 |
| `MigrateDreamShaderLegacyModule(Module, Legacy, Bound, Options, Diagnostics)` | `Migrate/LangMigrate.h` | 原地重写 1.x 模块。`Bound` 是针对 `Module` 绑定的，且在此处仅被读取；**其节点指针在本次返回后即失效**。出错后返回 false：此时该模块已不值得打印。 |
| `PrintDreamShaderInstance(Instance, FilePath, AssetPathOverride)` | `Lang/LangInstanceSource.h` | 针对实例载荷的 `.dsi` 文本：可选的 `/// @name`、`#pragma instance(...)`，每个覆盖项对应一个 `uniform`。 |
| `CollectDreamShaderPipelineReferences(Module, OutMaterials, OutShaders)` *（自 2.1.0 起）* | `Lang/LangPipelineSource.h` | 已解析 `.dsp` 的每个 `Material = "..."` 和 `Shader = "..."`，各一次，按源顺序排列。宿主将它们解析为一个 `FPipelineReferences` 并以其作为 `FBindOptions::PipelineReferences` 进行绑定；没有它时，绑定是一种无引擎检查——引用按原文处理，所有需要引擎事实的检查被跳过，并会给出说明这一情况的信息（`DSH7360`）。产物是 `FBoundModule::Pipeline` 的载荷，以文件名命名；`BuildDreamShaderIR` 将其复制到 `FIRProduct::PassPipeline`，不进行任何降级。 |
| `PrintDreamShaderPipeline(Pipeline, FilePath, Options)` *（自 2.1.0 起）* | `Lang/LangPipelineSource.h` | 针对管线载荷的 `.dsp` 文本——按载荷顺序给出 `#pragma pipeline`、uniforms、缓冲区、各趟处理，省略默认值。这是反编译器所写的内容。 |
| `RewriteDreamShaderPipelineSource(Original, Parsed, Bound, Desired, OutEdits, OutText, Diagnostics)` *（自 2.1.0 起）* | `Lang/LangPipelineSource.h` | 针对 `.dsp` 的采用操作：通过跨度编辑使现有文件陈述 `Desired`，只触及有差异的部分，从而使注释和顺序得以保留。对于不是绑定的 `.dsp` 的文件（`DSH9109`）、与其他名称共享且必须随之更改的声明（`DSH9107`）或重叠的编辑（`DSH9108`），返回 false，`OutText` 保持为原值。 |
| `CompareDreamShaderPipelines(A, B, OutDifferences)` *（自 2.1.0 起）* | `Lang/LangPipelineSource.h` | 两个载荷的结构相等性，忽略源引用，每个差异一行——这正是 `.dsp` → 资产 → `.dsp` 往返流程和 `AreDreamShaderIRModulesEquivalent` 衡量管线所用的方式。 |

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

`FLangParseOptions::Frontend` 默认为 `ELangFrontend::Auto`，它根据
`FLangSourceText::GetPath()` 所携带的文件扩展名进行判断：

| 扩展名 | `ELangFileKind` | 前端 |
| :-- | :-- | :-- |
| `.dss` | `Dss` | 2.0 |
| `.dsi` | `Dsi` | 2.0 — 一个材质实例：一个 `#pragma instance` 和 `uniform` 覆盖，别无其他 |
| `.dsh` | `Dsh` | **按声明**决定：项目迁移期间，共享头文件可以同时包含两种语法。`export` 和资产块会被拒绝 |
| `.dsp` *（自 2.1.0 起）* | `Dsp` | 2.0 — 一个 Custom Pass 管线：`#pragma pipeline`、`uniform` 和 `static const` 声明，`buffer` 和 `pass`。`buffer` 和 `pass` 只是 `.dsp` 中的声明词（在其他位置是 `DSH3310`）；在 `.dsp` 中的函数、结构体或 `#include` 是 `DSH3314` |
| `.dsm` | `Dsm` | 旧版 |
| `.dsf` | `Dsf` | 旧版 |

旧版前端将 1.x 构造降到*相同的* AST 节点类型上——一个 `Shader` 块
变成 uniform、一个 `#pragma material` 和一个导出函数——因此下游无需知道
是哪个解析器生成了模块。1.x 文本在此之外所述内容（块名、节、重命名、
输出选择）随树一起保存在 `FLangParseResult::Legacy` 中，而声明携带
`bLegacy`，这正是绑定器对其（且仅对其）应用文档化 1.x 规则的原因。

<a id="printer-contract"></a>

### 打印器契约

打印器的承诺是**结构保真，而非文本保真**：`parse(print(parse(X)))` 产生的树与 `parse(X)` 相同。具体而言——在 `FParenExpr` 记录的位置以及优先级要求的位置发出括号；`///` 块在自由文本之后按每行一条指令重新发出；不透明（`/// @custom`）主体从 `RawBody` 原样写出；pragma 和 include 保留其拼写；字面量保留书写时的词素，因此 `1.0f`、`0x10` 和 `2u` 都能保留。除 `///` 以外的注释在解析保留它们时会被重现：使用 `FLangParseOptions::bKeepTrivia` 时，每条
注释和空行都附着到它旁边的声明或语句上
（`FModule::Trivia`），打印器会将它们写回——这正是 `dsc migrate` 能够承诺
不丢失任何注释的原因。不使用该选项时，它们会像以前一样被丢弃。

`FLangPrintOptions::NewLine` 默认为 `\n`，但不透明主体是其源文件的一个切片，
保留该文件的终止符，因此打印一个 CRLF 文件会产生混合终止符。这是稳定的：
下一次解析会切出相同的字节，因此固定点仍然成立。

<a id="opaque-bodies"></a>

### 不透明主体

一个函数的 `///` 块携带 `@custom` 时，其主体被原样捕获：`FFunctionDecl::Body`
为 null，`bOpaqueBody` 为 true，`RawBody` 是花括号之间的文本，`BodySpan` 覆盖
含花括号的范围。解析器只读取花括号——它们必须成对平衡，而其中的字符串、注释和
`#` 行不会破坏平衡，因为词法分析器已把每一个都变成单个 token。

由于该文本不是 DreamShaderLang，`ParseDreamShaderLang` **丢弃每一个跨度起始于 `BodySpan` 内部的词法诊断**。其中的 `$`、`@`、`'` 或行中 `#` 不是关于程序的事实，
由着色器编译器来判定。该抑制仅限于这些跨度：
在外部一行处的相同字符仍然是 `DSH2101` / `DSH2106`。

<a id="diagnostics"></a>

## 诊断

每个抛出位置都字面地写出其代码，因此 `.skill/gen-diagnostics.ps1` 可以找到它：

```cpp
Diagnostics.Error(TEXT("DSH2151"), Span, LOCTEXT("ExpectedExpression", "expected an expression"));
```

这些消息在每文件 `LOCTEXT_NAMESPACE` 下是 `LOCTEXT`，形式为
`"DreamShader.Lang.<Unit>"`，因此编辑器可以本地化它们。**针对代码断言，绝不要针对
消息文本断言。**

| 范围 | 抛出者 | 固定锚点 |
| :-- | :-- | :-- |
| `DSH2101`–`DSH2119` | 词法分析器（已分配：`2101`–`2106`） | `2101` 未知字符 · `2102` 未终止的块注释 · `2103` 未终止的字符串 · `2104` 未知转义 · `2105` 格式错误的数字 · `2106` 一个不在其行首的 `#` |
| `DSH2150`–`DSH2189` | 表达式和语句（已分配：`2150`–`2155`、`2157`–`2165`） | `2150` 意外的文件结尾 · `2158` 命名参数之后的位置参数 · `2160` 不支持的语句（`switch`，以及主体内的 `#` 行） · `2162` 用作表达式的初始化列表 |
| `DSH2199` | 前端选择 | 旧版前端到来时废弃 |
| `DSH2200`–`DSH2258` | 旧版前端——块、属性列表、`import`、Graph 语句 | `2200` 一个 1.x 静默截断的运算符 · `2248` 在 `.dsm` / `.dsf` 中的 2.0 语法 · `2249` 在 `.dsh` 中的资产块 · `2252` 一个 include 解析器不读取的 `import` |
| `DSH2300`–`DSH2349` | `.dsp` 语法——`buffer` 和 `pass` 声明、pass 主体、绑定 | |
| `DSH3250`–`DSH3278` | 旧版前端——`Properties`、`Settings`、`Outputs`、`Inputs`、`Layout` 节 | |
| `DSH3300`–`DSH3349` | `.dsp` 声明——`#pragma pipeline`、缓冲区与 pass 键、在另一源中仅限 `.dsp` 的声明 | |
| `DSH6300`–`DSH6330` | 旧版前端与绑定器——`Function` / `GraphFunction`、提升的 `UE.` 调用 | `6326` 一个提升调用读取了主体局部变量 |
| `DSH3200`–`DSH3249` | 声明、类型、指令、文档块（已分配：`3200`–`3208`、`3210`、`3211`、`3213`–`3218`、`3220`–`3222`） | `3201` 游离的预处理器指令 · `3203` `#include` / `import` 没有**双引号**路径 · `3204` 期望一个类型名 · `3208` 没有主体的函数 · `3213` 错误的存储/链接组合 · `3220` 格式错误的 `@` 指令（**警告**） · `3221` 孤立的 `///` 块（**警告**） · `3222` 一个 1.x 声明词 |

绑定器、构建器、各个 pass、验证器和自定义 HLSL 构建器都通过
同一个接收器抛出，各自带有自己的范围：

| 范围 | 引发者 |
| :-- | :-- |
| `DSH4200`–`DSH4299` | 绑定器 — 名称、类型、表达式、语句、循环、区域 |
| `DSH4300`–`DSH4349` | IR 验证器 |
| `DSH4350`–`DSH4399` | 降级拒绝 — 一个在折叠后仍存活的矩阵、一个无法证明行程次数的循环、分支内的 `discard`、在写入前读取的材质属性 |
| `DSH4400`–`DSH4449` | `.dsp` 名称与引用 — 缓冲区、材质、HLSL 文件、条目、层 |
| `DSH5200`–`DSH5299` | 反射调用、目录、材质属性 |
| `DSH5300`–`DSH5314` | `.dss` 中的 Custom Pass 材质节点 — 引擎门禁、它们可以出现的位置、新的翻译器 |
| `DSH6200`–`DSH6219` | 函数种类、条目、`export` / `extern` |
| `DSH6220`–`DSH6249` | 辅助函数内联 |
| `DSH6250`–`DSH6299` | `/// @custom` HLSL |
| `DSH7200`–`DSH7249` | uniform、`///` 指令、`#pragma material` |
| `DSH7250`–`DSH7270` | `.dsi`：`#pragma instance`、针对父级 schema 的覆盖 |
| `DSH7300`–`DSH7379` | `.dsp` 检查（`V1`–`V13`）：pass 种类与注入点的匹配、按帧顺序的缓冲区使用、针对材质或 HLSL 文件的绑定、限制 |
| `DSH5250`–`DSH5292` | 文档化的 1.x 规则（`L1`–`L27`），当某条规则重写或丢弃某些内容时，每条都是一个诊断 |
| `DSH9042`–`DSH9044` | 格式化器：1.x 文件和带预处理行的文件会被跳过（信息），格式化后的文本若无法通过自身检查则被拒绝 |
| `DSH9075`–`DSH9084` | IR → 源代码：写入器重命名了哪些内容、无法放置哪些内容，或将其保留为默认值 |
| `DSH9107`–`DSH9109` | `.dsi` 以及*自 2.1.0 起*的 `.dsp`（Adopt）的就地重写：共享声明、重叠编辑、错误类型的文件 |
| `DSH9091`、`DSH9094` | 迁移器（其周围宿主提供的证明位于编辑器模块中，是 `DSH9090`–`DSH9099` 的其余部分） |

`DSH1030`–`DSH1042`（预处理器的自身代码）随预处理器一起移入此模块，并
保留了其编号。每个代码的说明文字位于[诊断](../diagnostics/index.md)下。

<a id="testing-the-lang-corpus"></a>

## 测试：`Lang` 语料库

前端由**数据驱动的语料库**覆盖，而不是每个功能手写的测试。测试固件
位于 `Tests/Corpus/Lang/` 下，每个功能一个 `.dss` 或 `.dsh`，每个可选择性地配对一个
`<name>.expected.json` 黄金文件；一个复杂的自动化测试在运行时枚举该树，并将
每个固件暴露为其自身的子测试。

| | |
| :-- | :-- |
| 测试 | `DreamShader.Lang2.Corpus.<Area>.<Fixture>` |
| 运行器 | `RunDreamShaderLangCorpusCase`，位于 `Source/DreamShaderEditor/Private/Tests/DreamShaderTestCommon.h` |
| 注册 | `Source/DreamShaderEditor/Private/Tests/DreamShaderCorpusLangTests.cpp` |
| 测试固件 | `Tests/Corpus/Lang/{Lexical,Expressions,Statements,Declarations,Examples}/` — `.dss` 与 `.dsh`，外加一个 `.dsm`，用于固定前端选择：它会到达旧版前端并在那里失败（`DSH2241`） |
| 黄金 schema | `"entryPoint": "lang"` — 参见 [`Tests/Corpus/README.md`](../../../Tests/Corpus/README.md) |

每个用例都会通过 `ParseDreamShaderLang` 解析一次，然后与其黄金文件进行比对：

- `outcome` — `ok` 或 `error`。如果没有黄金文件，默认是“正向固件解析成功，
  `*.bad.*` 固件失败”。
- `errorContains` / `warningsContain` — 该严重级别下每条诊断的
  `FLangDiagnosticSink::ToWireString` 的子字符串（大小写**敏感**）。在实践中：
  `DSHnnnn` 代码。
- `module` — 对解析后的 `FModule` 进行结构性计数：`declarations`（pragma 和 include
  也会计数）、`pragmas`、`includes`、`structs`、`uniforms`（仅 `Uniform` 存储）、`constants`
  （仅 `StaticConst`）、`functions`（全部三种链接方式）、`exports`、`externs`、`opaqueBodies`、
  `entry`（其签名为 `void (inout material)` 的第一个函数，**无论链接方式如何**）
  以及 `roundtrip`。
- `roundtrip` — 打印 → 解析 → 打印逐字节相同，这就是打印器的契约以
  测试形式表达出来。

为新构造添加覆盖就是往树中放入一个文件；无需 C++ 更改，无需重新编译。
以无头模式运行整个层：

```
"F:\UnrealEngine\UE_Moon\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "I:\UnrealProject_Moon\DEV_58\MoonEngineSample\MoonEngineSample.uproject" ^
  -ExecCmds="Automation RunTests DreamShader.Lang2; Quit" ^
  -nullrhi -unattended -nopause -nosplash -log
```

添加 `-DreamShaderUpdateGolden` 会根据实际结果重写每个黄金文件，而不是进行断言。
提交前手动审查差异 — 这个开关正是回归变成基线的方式。

除了语料库之外，每个前端单元都有自己的专注测试文件
（`DreamShaderLangLexerTests.cpp`、`…DeclarationTests.cpp`、`…ExpressionTests.cpp`、
`…PrinterTests.cpp`），全部位于 `DreamShader.Lang2.*` 下。

<a id="see-also"></a>

## 另请参阅

- [2.0 管线做了什么](../language-v2/index.md) — 从作者视角看的语言表面与管线限制
- [C++ API 索引](index.md) — 其他四个模块
- [Custom Pass 管线](../language-v2/passes.md) — 从作者视角看的 `.dsp` 语言
- [预处理器](../language/preprocessor.md) — 从语言视角记录的 `#if` 和定义表
- [诊断索引](../diagnostics/index.md) — 每个 `DSHnnnn` 及其原因和修复
- [测试](../contributing/testing.md) — 自动化套件与固件语料库
