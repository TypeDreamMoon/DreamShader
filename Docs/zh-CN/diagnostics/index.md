---
title: 错误速查
description: DreamShader 的诊断出现在哪里、怎么读一条、DSHnnnn 错误码怎么分段、严重级别，一张按阶段分组的常见诊断表，以及 1.x 消息到错误码的对照。
---

编译时报出的每一条 **诊断** 都带一个稳定的 `DSHnnnn` 码 （自 2.0.0 起）：一个码、一条消息、一个严重级别，
归属于一个源文件，带着出问题那个构造的行号和列号。**码是契约，措辞不是。** 本页讲清楚诊断出现在哪里、怎么读，
按阶段列出最常撞上的那些，并把 1.x 生成器的老消息对照到今天报的码上。

| 项目 | 值 |
| :-- | :-- |
| 产生者 | `DreamShaderLang` —— 预处理器、两个前端、绑定器、IR 及其校验器 · `DreamShaderCompiler` —— 流水线、include 解析、发射器、资产层 · `DreamShaderEditor` —— bridge、commandlet 命令和工具 |
| 日志分类 | `LogDreamShader` |
| 线上格式 | `<file>(<line>,<column>): DSHnnnn: <message>`，无论编辑器是什么语言都是英文 |
| 严重级别 | `error`、`warning` 或 `info` —— 一次编译的记录保留各自的级别 （自 2.1.0 起）；编译解释不了的少数记录是 `error` |
| Bridge 产物 | `<Project>/Saved/DreamShader/Bridge/diagnostics.json`、`.../Bridge/diagnostics/`、`.../Bridge/bridge.db` |
| 完整目录 | 插件仓库的 [`Docs/diagnostics/README.md`](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/README.md) —— 每个码的严重级别和消息；每个码在 `DSH1xxx.md` … `DSH9xxx.md` 里各有一节，写着它的全部消息、原因和处理 |

**提示：**

**1.9.x 及以前，大多数消息没有码。** 1.x parser 给自己的消息编了码，1.x 生成器约 560 个报错点只报裸文本，本页当时也是
逐字收录那些文本。生成器和 parser 在 2.0.0 已经删掉，那些字符串一条都不会再出现。如果你在旧日志或搜索结果里看到一条，
[1.x 消息对照](#1x-消息对照)给出了常见的那些今天报什么码。

<a id="诊断出现在哪里"></a>

## 诊断出现在哪里

| 界面 | 显示什么 |
| :-- | :-- |
| **Output Log** | 分类 `LogDreamShader` 下的编译报告：失败时记 `Error` —— 每条记录一行线上格式，先错误、再警告、再 note；成功时记 `Display` —— `Generated …` / `Skipped …` 行，再跟一个 `Warnings:` 块 |
| **[Material Content Browser](../tools/editor-integration.md#material-content-browser)** | 每个源文件的记录，从 bridge 的诊断存储读出；源文件那一行显示第一个错误 |
| **`Bridge/diagnostics.json`** | `{ version, updatedAtUtc, files[] }`，每个文件一项 |
| **`Bridge/diagnostics/`** | 每个文件一个 `<md5-of-normalized-path>.json` 分片，外加 `index.json`；每次写入都会删掉过期分片 |
| **`Bridge/bridge.db`** | SQLite 表 `diagnostics(path, json, updated_at_utc)`，在一个事务里整体替换 |
| **`-DiagnosticsOut=<file>`** | 一次 commandlet 运行的诊断，JSON（schema `dreamshader-diagnostics`）—— 见[命令行](../tools/commandlet.md#诊断) |
| **VSCode / Rider 波浪线** | 由扩展从上面三份 bridge 产物渲染 —— 这是**扩展侧**的事，不是插件 |

三个 bridge sink 每次更新都一起写，所以它们永远不会互相矛盾。Output Log 是另一条独立路径，也是唯一会显示 *成功* 行的界面。

诊断归属于产生它的源文件和产生它的那次 **运行**。运行有三种：一个源文件的编译、它生成的某个材质的 shader 编译、
启动时对一个文件的 VirtualFunction 扫描。

| 运行 | 它替换什么 |
| :-- | :-- |
| `A.dsm` 的编译 | 恰好是 A 上一次编译登记的那些 —— 登记在 A 上的，和登记在 A 导入的头文件上的。`B.dsm` 登记在同一个头文件上的记录保留 （自 2.1.1 起）。它也会撤掉启动扫描对 A 及其头文件说过的话 |
| 一个材质的 shader 编译 | 这个材质的 shader 编译记录，在 shader 编译结束时 —— 比生成它的那次编译晚几帧；不会动编译自己的警告 |
| 启动时的 VirtualFunction 扫描 | 它对所扫文件的记录 |
| 删除一个文件 | 这个文件拥有的一切记录，以及任何人登记在它上面的记录 |

同一个文件里，记录按「编译、shader 编译、VirtualFunction 扫描」排序，各自内部按源文件排，所以一个文件的第一个错误总是
它编译时的错误。

**警告：**

bridge 从不在 commandlet 里运行。`-run=DreamShader`、`-run=Cook` 以及任何其他 commandlet 进程都不写 `diagnostics.json`、
不写分片、不写 `bridge.db` 行；消息只存在于日志里（以及 `-DiagnosticsOut` 文件里）。`-NoDreamShaderEditorBridge`
同样会关掉 bridge。见[命令行](../tools/commandlet.md)。

<a id="严重级别"></a>

## 严重级别

（自 2.1.0 起）

一次编译按每条记录被报出时的级别登记它自己的记录 —— 失败的编译登记全部，成功的编译登记它的警告和 note。在此之前，
每条入库的诊断都是 `error`，成功编译的警告只出现在 Output Log 里，编辑器扩展看不到。

| 级别 | 什么时候 | 例子 |
| :-- | :-- | :-- |
| `error` | 编译失败；这个阶段之后的阶段都不跑 | `DSH4200` 名字未声明 |
| `warning` | 编译照样成功，但有你该知道的事 | `DSH2251` 旧拼写、`DSH7233` 滑条范围被丢弃、`DSH5277` GLSL 拼写 |
| `info` | 编译照样成功，只是把 1.x 默默做的事说出来 | `DSH5283` 调用声明了一个 out 目标、`DSH5289` 宽值被截成前几个分量 |

编译解释不了的记录 —— `generate-error`、`material-compile`、`virtual-function-sync` —— 是 `error`。不属于任何编译、
只写日志的警告（`UE_LOG(LogDreamShader, Warning, …)`）从不入库。客户端遇到缺失或未知的 severity 时，按 error 处理。

<a id="读懂一条消息"></a>

## 读懂一条消息

诊断按 MSVC 风格打印，带着码：

```text
I:/Project/DShader/Materials/M_Sample.dsm(8,19): DSH4200: 'Tin' is not declared in this scope.
```

| 诊断 | 位置从哪来 |
| :-- | :-- |
| 一次编译的记录 | 构造本身的行和列，取自报错时所在的 token —— 在 section 体里、在 `Graph` 里、在被 import 的头文件里都一样。在头文件里报的记录指向**头文件**，不是导入它的文件 |
| 关于整个文件的 —— 解析不了的 include、编译器不构建的源文件 | 文件的第 1 行第 1 列 |
| 材质编译 | 引擎自己的 `<path>(<line>,<col>): ` 前缀，被重新解析并归回它来自的源码行 |
| `generate-error` —— 记录解释不了的失败 | 从一行 `<path>(<line>,<column>): <message>` 里取；取不到就是 `1,1` |

（自 2.0.0 起） 起每个文件单独做词法和语法分析，位置不再从拼起来的文本映射回去。1.9.x 及以前，解析错误带的是一个指向
「展开 import 之后的文本」的 `near index {Index}` 偏移，section 体里的位置是错的，大多数语句级消息根本没有位置。

一个阶段会报出它的**全部**错误 —— parser 和绑定器遇到一个错误后会继续往下走 —— 而有错误的阶段会在下一阶段开始前停下
流水线：有语法错误的文件不会进入绑定，绑定不过的文件不会被发射。

<a id="dshnnnn-错误码"></a>

## `DSHnnnn` 错误码

首位数字是报出它的阶段。每一段在插件仓库里有一个页面，点段名打开：

| 段 | 阶段 |
| :-- | :-- |
| [`DSH1xxx`](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/DSH1xxx.md) | 预处理器（`#if`、`#define`），1.x 纹理默认值里的资产引用 |
| [`DSH2xxx`](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/DSH2xxx.md) | 词法器和两个 parser —— `DSH2200`–`DSH2258` 是 legacy 前端的：1.x `Graph` 语句和顶层块 |
| [`DSH3xxx`](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/DSH3xxx.md) | 声明、`#pragma`、`///` 块；`DSH3250`–`DSH3278` 是 legacy 前端的 section |
| [`DSH4xxx`](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/DSH4xxx.md) | 绑定器（名字、类型、语句）、IR 校验器、降级时的拒绝 |
| [`DSH5xxx`](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/DSH5xxx.md) | `UE.*` / `Substrate.*` 调用和节点目录；`DSH5250`–`DSH5292` 是 1.x 调用写法和编了号的 **legacy 规则** |
| [`DSH6xxx`](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/DSH6xxx.md) | 函数种类、内联、`/// @custom` HLSL；`DSH6300`–`DSH6330` 是 1.x 的 `Function` / `GraphFunction` / `Namespace` / `VirtualFunction` |
| [`DSH7xxx`](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/DSH7xxx.md) | uniform、指令、`#pragma material`、材质设置写入器、`.dsi`、`.dsp` 检查 |
| [`DSH8xxx`](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/DSH8xxx.md) | 发射器、资产层、include 解析（`DSH8291`–`DSH8295`）、Custom Pass 发射 |
| [`DSH9xxx`](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/DSH9xxx.md) | 工具 —— `check`、`dump-ir`、反编译器、`migrate`、导航、`pass-registry` |

每个码的那一节写着它的全部消息、报出它的源码位置、原因和处理。站点不逐码建页；遇到下面没收的码，就到对应段的页面里
按码搜。

<a id="legacy-规则"></a>

### legacy 规则

**legacy 规则**（`DSH5275`–`DSH5292`）是 1.x 做过、而 1.x 源文件今天仍然得到的行为，只是现在会说出来：只靠大小写才对得上的
名字（`DSH5275`、`DSH5276`）、GLSL 拼写（`DSH5277`）、宽向量被截到窄位置（`DSH5289`）、一条赋值顺带声明了变量
（`DSH5292`）。它们在 `.dsm` / `.dsf` 上是 warning 或 info，在 `.dss` 里从不出现 —— `.dss` 里同样的写法是错误（例如 GLSL
拼写在 `.dss` 里是 `DSH4250`）。[`dsc migrate`](../tools/migrate.md) 会把每一条都写成显式的 2.0 文本。

<a id="常见诊断"></a>

## 常见诊断

下面是实际项目里最常撞上的码，按阶段分组。消息是插件当前的模板，`{…}` 是运行时填进去的部分；同一个码可能还有别的措辞，
以插件页面为准。

<a id="解析与文件"></a>

### 解析与文件

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH2101: Unexpected character '{Char}' in source.` | 不属于任何 token 的字符：@、$、反引号、从文档里粘进来的全角标点、浏览器复制带进来的不换行空格。词法器只认 ASCII。 | 删掉它；看起来普通的字符多半是不可见的那种，整行重打。 [详解](../language/lexical.md) |
| `DSH2102: Unterminated block comment; expected a closing '*/'.` | /\* 之后没有 \*/。块注释不嵌套。 | 补上 \*/。报的位置是开头的 /\*。 [详解](../language/lexical.md#注释) |
| `DSH2103: Unterminated string literal; expected a closing '"'.` | 字符串到行尾都没有闭合 —— 字符串不跨行。以反斜杠结尾的路径会把结尾的引号读成转义字符。 | 闭合字符串；路径用正斜杠。 [详解](../language/lexical.md#字符串字面量) |
| `DSH2105: Malformed number literal '{Text}'.` | 像数字但不是数字：1.2.3、0x、1e、2abc。 | 按语言的拼法写字面量。 [详解](../language/lexical.md#数字字面量) |
| `DSH2150: Unexpected end of file while parsing {What}.` | 文件在某个构造没写完时就结束了，几乎总是前面少了一个 }、) 或 \]。 | 补上缺的闭合符号；报的位置是文件末尾，不是错处。 [详解](../language/lexical.md) |
| `DSH2240: Expected a top-level Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend, Function, GraphFunction, Namespace, VirtualFunction or import, found {Token}.` | 1.x 文件顶层出现了不是块的东西。 | 检查这行上面有没有多余的 token 或没闭合的块；块关键字区分大小写。 [详解](../language/blocks.md) |
| `DSH2242: Expected a 'Name = "..."' attribute on '{Block}', found none.` | 块头有属性列表，但里面没有 Name。 | 补上 Name = "…"。 [详解](../language/blocks.md) |
| `DSH2245: Expected a Shader section (Properties, Settings, Outputs, Graph or Layout), found '{Word}'.` | 块里某一行以一个不是 section 名的词开头。section 名忽略大小写，所以错的是字母本身。 | 检查拼写，以及前一个 section 是否已闭合。 [详解](../language/sections.md) |
| `DSH2248: Expected a 1.x block in a '.{Ext}' file, found {Token}, which is 2.0 syntax; 2.0 declarations belong in a .dss file or a .dsh header.` | .dsm / .dsf 里出现了 2.0 写法（uniform、export 函数、#pragma）。 | 把 2.0 声明放进 .dss 或 .dsh；dsc migrate 能整体改写一个 1.x 文件。 [详解](../language/source-files.md) |
| `DSH2249: Expected only Function, GraphFunction, Namespace and VirtualFunction blocks in a '.dsh' header, found the asset block '{Block}', which belongs in a .dsm or .dsf file.` | 头文件里有会生成资产的块。按声明判定 —— 注释里提到 Shader( 没问题。 | 把这个块挪到自己的 .dsm / .dsf。 [详解](../language/source-files.md#这条限制是怎么执行的) |
| `DSH2250: Expected one Shader block in a file, found a second one.` | 一个文件里有两个 Shader 块。规则按文件算，不再按 import 闭包。 | 每个材质一个文件。 [详解](../language/blocks.md) |
| `DSH2252: Expected a double-quoted path after 'import', found {Token}.` | import 后面不是带引号的路径；或者路径指向 .dsf / .dsm（只能 import .dsh）；或者是 1.6.0 的根限定写法。 | 写成 import "Shared/Common.dsh";。 [详解](../language/import.md) |
| `DSH2255: Expected a Graph section in the Shader '{Name}', found none.` | Shader 没有 Graph，Outputs 里也没有自己算值的语句。 | 加上 Graph = { … }。 [详解](../language/blocks.md) |
| `DSH2259: Expected ShaderFunction, ShaderLayer and ShaderLayerBlend blocks in a '.dsf' file, found a Shader block, which belongs in a .dsm file.` | .dsf 里有 Shader 块。这个块被跳过，文件其余部分照读。 | 把 Shader 挪进 .dsm；.dsm 可以在 Shader 旁边保留函数和图层块。 [详解](../language/source-files.md) |
| `DSH8292: '{File}', included from '{Includer}', could not be resolved: {Detail}.` | #include / import 指向的文件不在任何源码根里。 | 检查拼写和扩展名；跨根请用 / 开头的路径。 [详解](../language/import.md#解析) |
| `DSH4211: Including '{File}' from '{Includer}' closes a cycle; a header may not include itself, directly or through another header.` | 沿着 include / import 走回了一个正在读的文件。菱形没问题，环不行。 | 把两边都要用的东西挪进第三个头文件。 [详解](../language/import.md) |
| `DSH1035: {File}({Line}): unknown preprocessor directive '#{Name}'. {Detail}` | 函数体之外的 # 行不是八个指令之一，或者大小写不对（#IF）。 | 改正拼写；HLSL 指令放进 Function 体里。 [详解](../language/lexical.md#预处理器) |

<a id="graph-语句1x"></a>

### `Graph` 语句（1.x）

legacy 前端用 2.0 的语句和表达式解析器读 1.x 的 `Graph` 体，再拒绝 1.x 没有的写法。这一组只出现在 `.dsm` / `.dsf` 里；
同样的写法在 `.dss` 里是合法的。完整的写法清单见[不支持的写法](../graph/unsupported.md)。

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH2200 / DSH2201: Expected only '+', '-', '*', '/', calls and members in a 1.x Graph expression, found '{Op}', where 1.x silently dropped the rest of the expression; move this code to a .dss file.` | 比较、逻辑运算或 ?: 出现在 if 条件之外（DSH2200）；%、移位或位运算（DSH2201）。1.x 会在这里静默截断表达式。 | 用 if / else、fmod(a, b)，或把代码搬进 Function / .dss。 [详解](../graph/unsupported.md#1x-会截断的运算符) |
| `DSH2202: Expected no '[ ]' in a 1.x Graph expression, found '{Text}', where 1.x silently dropped the index and everything after it; use a swizzle such as '.r' or move this code to a .dss file.` | 用 \[ \] 取下标。 | 用 swizzle（.r、.xy）；按序号取节点输出写 OutputIndex = k。 [详解](../graph/unsupported.md) |
| `DSH2205: Expected '=' in a 1.x Graph assignment, found '{Op}', which 1.x read as the declaration of a variable named '{Name}'; write 'x = x + y' or move this code to a .dss file.` | 复合赋值 +=、\*= 等。 | 写成 x = x + y;。 [详解](../graph/unsupported.md#复合赋值与自增自减) |
| `DSH2208: Expected a declaration, an assignment, a call or 'if' in a 1.x Graph body, found '{Token}', which 1.x did not have; move this code to a .dss file.` | for、while、do、return、break、continue、discard。switch 更早一步报 DSH2160。 | 循环和提前退出放进 Function，或者迁到 .dss。 [详解](../graph/unsupported.md#语句层面的写法) |
| `DSH2209: Expected braces around the body of a 1.x Graph 'if', found a single statement.` | if / else 的体没有花括号。 | 加上 { }，哪怕只有一条语句。 [详解](../graph/if.md) |
| `DSH2210: Expected at most one comparison in a 1.x Graph 'if' condition, found '{Text}' as well, where 1.x silently dropped the rest of the condition; move this code to a .dss file.` | 条件里有不止一个比较（&amp;&amp;、\|\|）。1.x 只测第一个。 | 嵌套两个 if。 [详解](../graph/if.md) |
| `DSH2215: Expected an initializer on the 1.x Graph variable '{Name}' of type '{Type}', found none, and 1.x had no zero value for that type.` | 纹理、Substrate 或采样器类型的变量没有初始化式。数字从 0 开始，MaterialAttributes 从空集开始。 | 给它一个初始化式。 [详解](../graph/statements.md) |
| `DSH2218: Expected '#EndRegion' to close the region '{Name}' before the end of the Graph body, found none.` | Graph 体结束时 #Region 还没关。没名字的 #Region 是 DSH2216，多出来的 #EndRegion 是 DSH2217。 | 在体的 } 之前写 #EndRegion。 [详解](../language/layout.md) |

<a id="名字类型与表达式"></a>

### 名字、类型与表达式

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH4200: '{Name}' is not declared in this scope.` | 这个名字不是可见的局部变量、参数或文件级声明。只差大小写时，消息改为 did you mean '{Other}'? —— 1.x 源文件里只差大小写仍能解析，带警告 DSH5275。 | 改正拼写，或在使用之前声明。裸读 StaticSwitchParameter 也会报它 —— 要以调用形式使用。 [详解](../graph/expressions.md#标识符解析) |
| `DSH4201: '{Name}' is not a type; did you mean '{Other}'? Type names are case-sensitive.` | 类型拼写既不是内置类型，也不是在这行之前声明的 struct。 | 检查拼写和大小写。 [详解](../language/types.md) |
| `DSH4208: A value of type {Type} has no method '{Name}'.` | 调用了不可调用的东西：不是函数、内置或节点的名字，或者纹理没有的方法。对纹理对象参数写 NoiseTex(Coordinates = UV) 也落在这里。 | 改正拼写，在调用前声明函数，或用 UE.Expression(Class = …) 触达引擎节点。 [详解](../graph/calls.md) |
| `DSH4210: '{Name}' is already declared in this file; one name declares one thing.` | 文件级同名声明出现了两次；struct、文件级变量和函数共用一个命名空间，没有重载。 | 改掉其中一个名字。 [详解](../graph/calls.md) |
| `DSH4220: '{Name}' is already declared in this block.` | 同一个块里重复声明，或遮蔽了外层块或参数的名字。 | 换个名字；兄弟块里同名没问题。 [详解](../graph/statements.md) |
| `DSH4222: '{Type}' has {N} fields and this list has {M}.` | 构造器各参数的分量数加起来对不上目标。float3() 什么都不写是 DSH4221。 | 数分量：float4(v3, w) 是 3 + 1；float3(v4) 要写 v4.xyz。 [详解](../graph/expressions.md#构造器) |
| `DSH4226: {Context} expects {Expected}, and this is {Actual}.` | 操作数放不进它所在的位置：宽度不匹配（float3 \* float2），或者根本不是数字（纹理、MaterialAttributes 参与算术）。 | 用 swizzle 或构造器把宽度对齐；标量可以广播到任意宽度。 [详解](../graph/expressions.md#操作数规则) |
| `DSH4228: {Context} expects {Expected}, and this is {Actual}.` | 值存不进目标位置：= 右边、初始化式、参数默认值。在 1.x 源文件里，宽值写进窄位置只是 info DSH5289。 | 用 swizzle 或构造器调整宽度。 [详解](../graph/expressions.md#转换) |
| `DSH4230: '.{Mask}' is not a swizzle; a swizzle is one to four of 'xyzw' or 'rgba'.` | 字母不对、超过四个、越界（float3 上的 .w）、两套混用（.xg），或赋值目标里有重复分量。 | 只用一套、别越界；赋值时每个分量单独一行。 [详解](../graph/expressions.md#swizzle) |
| `DSH4243: Both sides of this '/' are integers, and the material graph has no integer division; write 'float(a) / b' for the fraction, or 'floor(float(a) / b)' for the whole part.` | 两边都是整数。1.x Graph 里只有两个整数构造器相除（int(7) / int(2)）才报；7 / 2 是 3.5（2.1.1 起）。 | 写清楚要哪一个。 [详解](../graph/expressions.md#整数除法) |
| `DSH4250: '{Name}' is the GLSL spelling; this language is HLSL, so write '{Hlsl}'.` | .dss 里用了 mix、fract、mod。1.x 源文件里同样的写法是 warning DSH5277。 | 写 HLSL 名：lerp、frac、fmod。 [详解](https://shader.toolchain.64hz.cn/docs/shader-languages/glsl) |
| `DSH4260: {Context} expects {Expected}, and this is {Actual}.` | if 条件不是单个真假值 —— 比如比较了两个向量。 | 比较一个分量。 [详解](../graph/if.md) |
| `DSH4372: '{Name}' is assigned in only one arm of this 'if' and has no value before it; assign it in both arms, or give it a value before the 'if'.` | 变量只在一个分支里赋值，而分支之前它没有值。 | 在 if 之前给它一个值，或者两个分支都赋值。 [详解](../graph/if.md) |

<a id="内置节点"></a>

### 内置节点

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH5200: A material has no '{Name}' pin; did you mean '{Other}'? Attribute names are case-sensitive.` | Base.&lt;Name&gt; / m.&lt;Name&gt; 不是材质 pin。1.x 的短名（Emissive、AO、WPO、PDO）作为别名保留。 | 用引擎的精确拼写。 [详解](../generation/outputs.md) |
| `DSH5210: '{Ns}.{Name}' is not a node; did you mean '{Ns}.{Other}'? Node names are case-sensitive.` | UE.X / Substrate.X 不是这个引擎有的类。UE 5.3 上没有任何 Substrate 类。 | 改正拼写，或用 UE.Expression(Class = "MaterialExpressionX")。目录认得的节点不再需要 OutputType。 [详解](../builtins/ue-expression.md) |
| `DSH5212: '{Class}' is not a material expression class this engine has.` | Class 参数不是材质表达式类。带不带 MaterialExpression 前缀、带不带 U 前缀、完整 /Script/Engine.… 路径都接受。 | 对照节点目录检查拼写。 [详解](../builtins/ue-expression.md) |
| `DSH5213: '{Ns}.{Name}' has no pin or property called '{Arg}'.` | 命名参数既不是输入 pin 也不是反射属性。先试 pin，再试属性。 | 改正名字；要写进节点的字面量通常是 Const 前缀的属性（A pin 旁边的 ConstA）。 [详解](../builtins/ue-expression.md) |
| `DSH5218: 'UE.Expression' reaches a node this language has no name for, so it needs 'Class = "MaterialExpressionName"'.` | UE.Expression(…) 没有 Class。 | 补上 Class，或者用短名 UE.&lt;Class&gt;(…) 调用。 [详解](../builtins/ue-expression.md) |
| `DSH5220: '{Ns}.{Name}' takes named arguments: write 'Pin = value'.` | 给没有规范参数顺序的节点传了位置参数。 | 写成 Pin = value。 [详解](../builtins/ue-expression.md) |
| `DSH5254: '{Name}' takes no positional argument here; 1.x ignored it, so it is dropped.` | 1.x 的 27 个 UE.\* 名字收到了它不读的参数。这是 warning：参数被丢掉，编译继续。 | 按它读的名字传参，或删掉。 [详解](../builtins/ue.md) |
| `DSH5256: Expected 'SampleTexture2D' to take exactly two positional arguments, a texture and coordinates, found {N} argument(s).` | 1.x 简写 SampleTexture2D(Texture, UV) 只接受纹理和坐标。 | 两个都传、别的不传；要 mip 或导数用 UE.Expression(Class = "TextureSample", …)。 [详解](../builtins/ue-expression.md) |
| `DSH5258: Expected the static switch '{Name}' to be called with a 'True = ...' and a 'False = ...' input, found at most one of them.` | 静态开关调用缺了一边。 | 传 True = … 和 False = …。 [详解](../graph/calls.md#staticswitchparameter) |
| `DSH5277: '{Name}' is the GLSL spelling of '{Hlsl}'; a 1.x source may use it and it is read as '{Hlsl}', and a '.dss' writes '{Hlsl}'.` | 1.x 源文件里的 GLSL 拼写（mix、fract、mod、vec3）。warning，照常编译。 | 写 HLSL 名；dsc migrate 会替你写。 [详解](https://shader.toolchain.64hz.cn/docs/shader-languages/glsl) |

<a id="函数与调用"></a>

### 函数与调用

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH4216: '{Callee}' takes its arguments in order and has no argument called '{Arg}'.` | 命名参数在被调方那里找不到。 | 改正名字，或按位置传。 [详解](../graph/calls.md#位置参数与命名参数) |
| `DSH4217: '{Callee}' is missing its '{Param}' argument.` | 某个参数既没传、也没有默认值、也不是 opt。 | 传上它，或者给它默认值 / 标成 opt。 [详解](../graph/calls.md) |
| `DSH4224: '{Callee}' takes at most {N} arguments.` | 参数个数不对。数学内置的命名参数现在选 pin，未知的名字是 DSH4216。 | 对照声明。 [详解](../builtins/math.md#命名参数) |
| `DSH6206: '{Name}' is a builtin operation and cannot be redeclared; rename the function.` | 函数用了内置运算的名字（dot、lerp、saturate……）。1.x 里它会被静默遮蔽、永远调不到。 | 改名。 [详解](../builtins/math.md#名称解析) |
| `DSH6208: '{Name}' is a {Kind} asset, not a function this file may call.` | 从 Graph 里调用 ShaderLayer / ShaderLayerBlend 或材质入口。 | 把共享代码提成 helper，两边都调它。 [详解](../language/blocks.md) |
| `DSH6304: Expected either a return type or 'out' parameters on '{Name}', found both.` | 1.x Function 同时有返回类型和 out 参数。没有返回值也没有 out 是 DSH6305。 | 二选一。 [详解](../language/functions.md) |
| `DSH6302: Expected 'in' or 'out' before the parameter '{Param}' of '{Name}', found '{Word}'; a 1.x function has no 'inout'.` | 1.x 函数参数用了 inout 或别的限定符。 | 拆成一个 in 和一个 out；.dss 函数可以用 inout。 [详解](../language/functions.md) |
| `DSH6260: The '@custom' functions {Chain} call each other in a cycle; HLSL has no recursion, so their bodies cannot be embedded in a custom node.` | Function / GraphFunction 体互相调用成环。经由提升出来的 UE. 调用成环是 DSH6330。 | 把共享部分提进第三个谁也不调的函数。 [详解](../language/functions.md) |
| `DSH6312: Expected an 'Asset = Path(...)' option on the VirtualFunction '{Name}', found none.` | VirtualFunction 没有指明它代表哪个资产。没有输出是 DSH6313，没有 Name 是 DSH6311。 | 加上 Options = { Asset = Path(Game, "Functions/MF\_Example"); }。 [详解](../language/blocks.md) |
| `DSH8219: This function call names no material function asset.` | 被调用的材质函数资产加载不到，或者引擎拒绝了它（环）。.dsf 里的函数要先构建。 | 先编译函数源文件；检查 @asset / Options.Asset 路径。 [详解](../graph/calls.md) |

<a id="properties元数据与-settings"></a>

### Properties、元数据与 Settings

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH3252: Expected a property type such as 'float', 'float4', 'Texture2D' or 'ScalarParameter', found '{Word}'.` | 类型位置上的词不是 1.x property 能用的类型。MaterialAttributes 不能作 property。 | 用值类型、纹理类型或参数节点类名。 [详解](../language/properties.md) |
| `DSH3253: Expected a property type with a 2.0 spelling, found '{Word}', which has none; move this material to a .dss file and write the node with UE.Expression.` | 这个节点类 1.x 能声明、2.0 没有对应的声明；或者 const 写在了不能是常量的 property 上。 | 声明成普通值或参数；别的节点类在使用处写 UE.&lt;Class&gt;(…)。 [详解](../language/properties.md) |
| `DSH3257: Expected 'Slider(min, max)' with two numbers, found '{Text}'.` | Slider 不是两个数字，或者范围写了两遍。 | 写成 Slider(0.0, 4.0)。 [详解](../parameters/metadata.md) |
| `DSH7233: A slider range ('@slider', or 'Slider(min, max)' in a 1.x property) is for a scalar parameter; this one is '{Type}', which has no slider, so the range is ignored.` | 滑条范围写在向量、纹理或 bool 参数上。warning，范围被丢掉。 | 删掉范围，或放到标量参数上。 [详解](../parameters/metadata.md) |
| `DSH3262: The setting '{Key}' is written twice; the later value wins, as it did in 1.x.` | 同一个 Settings 键写了两遍。warning。 | 删掉一个。 [详解](../settings/material.md) |
| `DSH7200: '{Key}' is set twice by '#pragma material'; it was already set on line {Line}.` | .dss 里 #pragma material 的同一个键设了两次（多行会合并成一张表）。 | 删掉一个。 [详解](../settings/material.md) |
| `DSH8215: A material setting on '{Asset}' was refused. {Detail}` | 设置键不是 UMaterial 上可写的属性路径（附带 DSH7118: Unsupported material setting '{Key}'.），或者值对这个属性无效。 | 对照 UMaterial 的属性名；数组属性要写显式 \[index\]。 [详解](../settings/material.md) |
| `DSH7129: Unsupported ShadingModel '{Value}'.` | 值不是这个引擎 EMaterialShadingModel 的成员。BlendMode 是 DSH7127，Domain 是 DSH7130。 | 用去掉 MSM\_ 前缀的引擎名。 [详解](../settings/material-enums.md) |
| `DSH7201: 'Backend' has no value; write 'Backend = Graph' or 'Backend = ThinCustom'. An empty value meant Graph in 1.x and means nothing now.` | Backend = ""。 | 写一个值，或删掉这个键以采用项目默认值。不是 backend 的值是 DSH7202，Instance 是 warning DSH7204。 [详解](../settings/backend.md) |

<a id="资产生成"></a>

### 资产生成

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH8201: The material for '{Name}' could not be created or reused. {Detail}` | 目标路径上已经有一个不是 UMaterial 的资产（DSH8102），或者有一个不是 DreamShader 生成的材质（DSH8103）。 | 给产物改名，或移走 / 删掉已有资产。 [详解](../generation/regeneration.md#归属守卫) |
| `DSH8206: '{Asset}' is open in an asset editor, so it was not rebuilt. {Detail}` | 资产在资产编辑器里开着（附带 DSH8101）。编辑器改的是副本，下次 Apply 会把重建悄悄撤回去。 | 关掉那个编辑器标签页，再编译一次。 [详解](../generation/regeneration.md#资产开着编辑器时拒绝重建) |
| `DSH8207: '{Asset}' no longer holds what DreamShader generated into it, so it was not rebuilt. {Detail}` | 资产生成后被手改过（附带 DSH8115）。-Force 也绕不过去。 | Revert to Source、Adopt Into Source 或 Detach From DreamShader 三选一。 [详解](../generation/regeneration.md#手改检测) |
| `DSH8090: DreamShader Root '{Root}' must reference a project plugin under '{Dir}'.` | Root 指向引擎插件或市场插件。2.1.2 起，cook 遇到别的插件自带的源文件时不再中止，只记一行 \[Cook\] Not this project's to build。 | 改成项目插件或 Game。 [详解](../generation/asset-paths.md) |

<a id="1x-消息对照"></a>

## 1.x 消息对照

1.x parser 和生成器的常见消息，以及今天报同一种情况的码。`{…}` 是消息里填进去的部分。

| 1.x 消息 | 今天 |
| :-- | :-- |
| `Unknown Graph identifier '{Name}'.` | `DSH4200` —— 声明里有只差大小写的名字时带 *did you mean* |
| `Unknown Graph function '{Name}'.` | `DSH4208` |
| `Math function '{Name}' expects exactly {N} argument(s).` | `DSH4224`；命名参数现在选 pin，未知的名字是 `DSH4216` —— 见[数学内置](../builtins/math.md#命名参数) |
| `Math function '{Name}' only accepts numeric scalar/vector arguments.` · `Operator '{Op}' requires matching vector sizes …` · `Arithmetic operators cannot be applied to …` | `DSH4226` |
| `Integer division is not supported by the material graph …` | `DSH4243` |
| `Expected {N} component(s) but got {M}.` | `DSH4228`；1.x 源文件里宽值写进窄位置是 note `DSH5289` |
| `Swizzle '{Swizzle}' is invalid …` · `Unsupported swizzle '{Swizzle}'.` | `DSH4230` |
| `Constructor '{Name}' expects {N} total components but got {M}.` | `DSH4222`；什么都不写的 `float3()` 是 `DSH4221` |
| `Graph variable '{Name}' is declared more than once.` | 同一个块里是 `DSH4220`；文件级是 `DSH4210` |
| `Graph variable type '{Type}' requires an explicit initializer.` | `DSH2215` |
| `Unsupported Graph variable type '{Type}'.` | 未知类型是 `DSH4201`；`return`、`for`、`while`、`switch` 是 `DSH2208` |
| `Graph if statement is missing a '{ ... }' body.` | `DSH2209` |
| `Graph if statement could not resolve both branch values for '{Name}'.` | `DSH4372` |
| `Graph if statement cannot select texture value '{Name}'.` · `… Substrate value …` | `DSH4379` · `DSH4378` |
| `Unsupported material output '{Name}'.` | `DSH5200` |
| `Unsupported property type '{Type}'.` | `DSH3252`；七个没有 2.0 写法的节点 token 之一是 `DSH3253` |
| `Metadata 'Slider(min, max)' requires exactly two numeric bounds …` · `Metadata key '{Key}' is declared more than once.` · `Metadata SortPriority value '{Value}' is not an integer.` | `DSH3257` · `DSH3256` · `DSH3258` |
| `Unsupported UE builtin call '{Name}' in Graph. …` | `DSH5210` —— 目录认得的节点不需要 `OutputType` |
| `UE.{Name} could not resolve MaterialExpression class '{Class}'.` | `DSH5212` |
| `UE.Expression requires Class="MaterialExpressionName".` | `DSH5218` |
| `UE.{Name}: '{Argument}' is not a property on '{Class}'.` | `DSH5213` |
| `Generic {Namespace}.{Name} calls require named arguments.` | `DSH5220` / `DSH5221` |
| `UE.SceneTexture expects exactly Id="..." …` · `SampleTexture2D expects exactly two positional arguments …` | `DSH5255` · `DSH5256` |
| `UE.StaticSwitchParameter requires Name="ParameterName".` · `StaticSwitchParameter '{Name}' requires True=... and False=... inputs.` | `DSH5257` · `DSH5258` |
| `{Kind} '{Name}' is missing required input '{Input}'.` · `… does not have an input named '{Input}'.` | `DSH4217` · `DSH4216` |
| `GraphFunction cycle detected: {Chain}.` · `SelfContained Function cycle detected: …` | `Function` / `GraphFunction` 体互相调用是 `DSH6260`；环经过从体里提升出来的调用是 `DSH6330`；被内联的 `.dss` helper 是 `DSH6220` |
| `Function '{Name}' has a return type and cannot also declare out parameters. …` · `Function '{Name}' must declare at least one out parameter.` | `DSH6304` · `DSH6305` |
| `Function '{Name}' parameter '{Parameter}' uses unsupported qualifier '{Qualifier}'. …` · `… '__return' is reserved …` | `DSH6302` · `DSH6303` |
| `Namespace(Name="...") is required.` · `Namespace '{Name}' may only contain Function or GraphFunction blocks.` | `DSH6309` · `DSH6310` |
| `VirtualFunction(Name="...") is required.` · `… must provide Options = { Asset = Path(...); }.` · `… must declare at least one output.` | `DSH6311` · `DSH6312` · `DSH6313` |
| `Shader(Name="...") is required.` · `{Block}(Name="...") is required.` | `DSH2242` |
| `Shader must provide a Graph block.` | `DSH2255` |
| `{File}: Outputs block is required.` · `No Outputs block was provided. …` | warning `DSH2256`；材质照样构建，只是什么都没接上 |
| `Unknown shader section '{Section}'.` · `Unknown material function section '{Section}'.` · `Unknown VirtualFunction section '{Section}'.` | `DSH2245` |
| `Shader graph sections now use Graph = { ... }. …` | `DSH2246` |
| `MaterialLayer is deprecated; use ShaderLayer instead.` · `MaterialLayerBlend is deprecated; …` | warning `DSH2251` |
| `Only one top-level Shader block is currently supported.` | `DSH2250` —— 现在按**文件**算；原来跨整个 import 闭包 |
| `DreamShader header '{File}' may only declare …` | `DSH2249`，按声明判定：注释里提到 `Shader(` 没问题 |
| `DreamShader function file '{File}' may only declare …` · `{File}: .dsf files cannot define top-level Shader blocks.` | `DSH2259` —— 见[文件模型](../language/source-files.md#这条限制是怎么执行的) |
| `A top-level Shader, Function, … block was not found.` | `DSH2254` |
| `Unexpected token near index {Index}.` | `DSH2240`；`.dsm` / `.dsf` 里的 2.0 写法是 `DSH2248` |
| `Expected '{' near index {Index}.` · `Expected ',' or ')' near index {Index}.` · `Expected identifier near index {Index}.` · `Expected value near index {Index}.` | `DSH2257` · `DSH2243` |
| `Unterminated block.` · `Unterminated string literal.` | `DSH2150` · `DSH2103` |
| `DreamShader import '{Specifier}' referenced from '{File}' could not be resolved.` | `DSH8292`；import `.dsf` / `.dsm` 或根限定写法是 `DSH2252` |
| `DreamShader could not read '{File}'.` · `DreamShader import cycle detected at '{File}'.` | `DSH8293` · `DSH4211` |
| `Unsupported Backend '{Value}'. Supported values: Graph, Instance, ThinCustom.` | `DSH7202`；空值是 `DSH7201`，`Instance` 是 warning `DSH7204` |
| `Unsupported BlendMode/RenderType '{Value}'.` · `Unsupported ShadingModel '{Value}'.` · `Unsupported MaterialDomain '{Value}'.` · `Unsupported material setting '{Key}'.` | `DSH7127` · `DSH7129` · `DSH7130` · `DSH7118` —— 材质设置写入器保留了它的 1.x 消息 |
| `Asset '{ObjectPath}' already exists and is not a Material.` · `… was not generated by DreamShader. …` | `DSH8201`，原因附带 `DSH8102` / `DSH8103` |
| `Asset '{ObjectPath}' is open in an asset editor, so it was NOT rebuilt. …` | `DSH8206`，附带 `DSH8101` |
| `Asset '{ObjectPath}' was edited by hand since DreamShader generated it …` | `DSH8207`，附带 `DSH8115` —— 见[重新生成](../generation/regeneration.md#手改检测) |

不在表里的消息：到[完整目录](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/README.md)里搜它所描述情况的一个词，
或者把文件编译一遍、读它现在报的码。

<a id="1x-静默接受的写法"></a>

## 1.x 静默接受的写法

1.x 生成器一声不吭地接受、然后构建出与文本不符的东西（或者把文本说的东西丢掉）的情况。（自 2.0.0 起） 起每一种都有诊断。

| 1.x 文本 | 1.x 的做法 | 今天 |
| :-- | :-- | :-- |
| `Graph` 表达式里的 `a % b`、`a & b`、`a \| b`、`a ^ b`、`a << b` | 读到运算符就停：表达式就是 `a` | `DSH2201` |
| `a ? b : c`，或 `if` 条件之外的 `a < b` | 一样：表达式就是 `a` | `DSH2200` |
| `v[0]` | 丢掉下标和它后面的一切 | `DSH2202` |
| `if (a > 0 && b > 0)` | 只测 `a > 0` | `DSH2210` |
| `(float3)x` | 没有强制转换 | `DSH2203` |
| `a += b` | 读成一个以运算符命名的变量的声明 | `DSH2205` |
| `x++` | 没有这个运算符 | `DSH2207` |
| 作为语句的 `a + b;` | 建出没人读的节点 | `DSH2211` |
| `Graph` 表达式里的 `0x10` | 读成了另一个数 | `DSH2222` |
| 当作数字的 `1abc` 或 `0.5.5` | 取前面的数字 | `DSH2105` |
| 没闭合的 `/* …` | 吃掉文件剩下的部分 | `DSH2102` |
| 属性写了两遍：`Shader(Name="A", Name="B")` | 保留后一个值 | warning `DSH2244`，仍保留后一个值 |
| `Settings` 键写了两遍 | 保留后一个值 | warning `DSH3262`，仍保留后一个值 |
| 材质函数 `Settings` 里的未知键 | 忽略 | warning `DSH3263`，仍然忽略 |
| 不收位置参数的 `UE.*` 调用上的位置参数 | 忽略 | warning `DSH5254`，仍然丢掉 |
| 函数 `Outputs` 条目上的默认值 | 忽略 | warning `DSH3273`，仍然丢掉 |
| 第二个 `Graph` 或 `Layout` section | 保留后一个 | warning `DSH2258`，仍保留后一个 |
| 与内置同名、从 `Graph` 调用的 `ShaderFunction` | 调到了内置 | 在声明处报 `DSH6206` |
| `dot(vec3Value, vec2Value)` | 接受；后来 Unreal 自己的材质翻译失败 | `DSH4226` |

1.x 写进文档、1.x 源文件仍在依赖的写法 —— 换了大小写的名字、GLSL 拼写、写进 `float3` 位置的 `float4` —— 照样构建，
作为一条把它说出来的 [legacy 规则](#legacy-规则)。

<a id="virtualfunction-同步"></a>

## VirtualFunction 同步

启动时重新读取每个 `VirtualFunction` 声明、并从它的 `UMaterialFunction` 资产刷新它的那个服务，以及 Material Function
工具栏和右键菜单上的编辑器动作。这些消息没有 `DSHnnnn` 码，唯一例外是用了 `#if` 的源文件报的 `DSH9001` —— 同步按字节偏移
原地改写源文件，分不清一个定义属于哪个分支，所以拒绝动它。

同步诊断以 `stage = virtualFunctionSync`、`code = virtual-function-sync`、`source = DreamShader VirtualFunction` 入库。
编辑器动作的消息是 toast 通知和日志行，不入库。

| 消息 | 原因 | 处理 |
| :-- | :-- | :-- |
| `VirtualFunction '{Name}' references missing MaterialFunction '{Path}'.` | 资产加载失败 | 恢复资产，或者更新 `Options.Asset` |
| `VirtualFunction '{Name}' could not be refreshed from MaterialFunction '{Path}': {Detail}` | 声明构建器失败 | 看内层消息 |
| `VirtualFunction '{Name}' asset reference is invalid: {Detail}` | `Options.Asset` 解析不了 | 改正 `Path( … )` |
| `DreamShader could not read VirtualFunction source file '{File}'.` | 源文件不可读；报在第 1 行第 1 列 | 检查文件锁和权限 |
| `DreamShader failed to update VirtualFunction source file '{File}'.` | 刷新后的声明写不回去；报在第 1 行第 1 列 | 检查文件锁和版本控制 |
| `MaterialFunction '{Name}' does not expose any outputs.` | 资产没有 `FunctionOutput` 节点 | 给材质函数加一个输出 |
| `DreamShader could not find a VirtualFunction definition for {Asset}.` | 对没有声明的资产执行 *Open VirtualFunction* | 改用 *CreateVirtualFunction* |

**提示：**

同步服务按裸关键字 `VirtualFunction` 找声明，**区分大小写**，两边要有标识符边界 —— 和 legacy 前端对块关键字的要求一样。
小写的 `virtualfunction` 两边都看不见。

<a id="其他日志行"></a>

## 其他日志行

不属于任何编译、永远不入库的行。

| 消息 | 含义 |
| :-- | :-- |
| `Failed to open DreamShader bridge database for diagnostics: {Path}` | `bridge.db` 打不开；JSON sink 仍然会写 |
| `'{ObjectPath}' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory. Run Tools > DreamShader > Make Ephemeral to make it Ephemeral again.` | 编译落到了一个背后有文件的 ThinCustom 产物上；由存储决定，所以它被重建并存盘 |
| `Skipping automatic layout for large DreamShader graph ({Count} nodes). Existing generated positions will be used.` | 记在 `Display` 级别；1.x 的 `Classic` 布局跳过了大图 |

commandlet 自己的运行行、用法横幅和退出码在[命令行](../tools/commandlet.md)；反编译器的在[反编译导出](../tools/decompiler.md#诊断)。

<a id="成功消息"></a>

## 成功消息

成功时 Output Log 每个产物一行 —— 在脚本里编译并匹配输出时有用。完整的表在[命令行 ▸ 结果消息](../tools/commandlet.md#结果消息)。

| 消息 | 含义 |
| :-- | :-- |
| `Generated {Kind} {AssetPath} from {SourceFile}.` | 构建出一个产物；`{Kind}` 是 `Material`、`MaterialFunction`、`MaterialLayer`、`MaterialLayerBlend`、`MaterialInstance` 或 `PassPipeline` |
| `Skipped {AssetPath} from {SourceFile}; source hash is unchanged (build key {BuildKey}).` | 构建键一致，没有重建；用 `-Force` 覆盖 |
| `Compiled {SourceFile}; it declares no material and no exported function, so no asset was written.` | 成功，没有东西要写 |
| `Warnings:` 后跟每行一条诊断 | 编译有警告时追加在后面 |

<a id="一条错误四个界面"></a>

## 一条错误，四个界面

`Graph` 块里的一个拼写错误，到达各个界面时的样子。

```c
// DShader/Materials/M_Sample.dsm
Shader(Name="Materials/M_Sample")
{
    Properties = { vec3 Tint = vec3(1.0, 0.4, 0.1); }
    Outputs    = { vec3 Color; Base.EmissiveColor = Color; }
    Graph      = {
        vec2 UV = UE.TexCoord(Index = 0);
        Color   = Tin * UV.x;          // typo: Tin, not Tint
    }
}
```

Output Log：

```text
LogDreamShader: Error: I:/Project/DShader/Materials/M_Sample.dsm(8,19): DSH4200: 'Tin' is not declared in this scope.
```

`Saved/DreamShader/Bridge/diagnostics.json`：

```json
{
  "version": 1,
  "updatedAtUtc": "2026-10-04T11:04:22Z",
  "files": [
    {
      "path": "I:/Project/DShader/Materials/M_Sample.dsm",
      "diagnostics": [
        {
          "message": "'Tin' is not declared in this scope.",
          "detail": "I:/Project/DShader/Materials/M_Sample.dsm(8,19): DSH4200: 'Tin' is not declared in this scope.",
          "stage": "bind",
          "code": "DSH4200",
          "line": 8,
          "column": 19,
          "severity": "error",
          "source": "DreamShader Lang2"
        }
      ]
    }
  ]
}
```

同一条记录还会写进 `Bridge/diagnostics/<md5>.json` 和 `Bridge/bridge.db` 的 `diagnostics` 表，并出现在 Material Content
Browser 里这个源文件的那一行 —— 这也正是 VSCode 和 Rider 扩展用来画波浪线的数据。`stage` 由码的段决定：`preprocess`、
`parse`、`bind`、`ir`、`generate` 或 `tools`。

<a id="按顺序修"></a>

## 按顺序修

一个文件报出一大堆消息时，从上往下修。有错误的阶段会挡住后面的阶段，所以修完一层，下一层的消息才会出来。

**先修预处理和解析（`DSH1xxx`、`DSH2xxx`）。** 配平花括号、引号和圆括号，检查关键字大小写。文件解析不过，绑定一个都不会跑。

**然后是 include 和未知名字（`DSH8292`、`DSH4200`、`DSH4208`）。** 通常就是少了一条 `import`，或者拼错了一个字 ——
2.0 的名字区分大小写。

**然后是类型与签名（`DSH42xx`、`DSH5xxx`、`DSH6xxx`）。** 分量数不匹配、调用参数、节点参数。

**然后是生成（`DSH8xxx`）。** 归属守卫、开着的编辑器、手改检测和保存失败 —— 这些说明源码是对的，目标不对。

**最后是 Unreal 自己的材质编译。** 这些消息带着引擎的 `<path>(<line>,<col>)` 前缀，被重新归属到你的源文件上，code 是
`material-compile`。它们是唯一不是 DreamShader 产生的消息。

<a id="继续阅读"></a>

## 继续阅读

- [当前限制](limitations.md) — DreamShaderLang 还做不到的事，以及各自报什么码。
  - [不支持的写法](../graph/unsupported.md) — 1.x Graph 不实现的每一种写法和它的码。
  - [dsc migrate](../tools/migrate.md) — 把 legacy 规则写成显式的 2.0 文本。
  - [生成流程](../generation/index.md) — 这些码所依据的阶段划分。
  - [命令行](../tools/commandlet.md) — 无头编译、-DiagnosticsOut，以及它为什么不写 bridge 产物。
  - [完整目录](https://github.com/TypeDreamMoon/DreamShader/blob/main/Docs/diagnostics/README.md) — 插件仓库里每个 DSHnnnn 码的严重级别、消息、原因和处理。
