---
title: "DSH3xxx —— 分节与声明"
description: "DSH3xxx —— 分节与声明"
---

<a id="dsh3xxx-----sections-and-declarations"></a>

# DSH3xxx —— 分节与声明

> 生成标记之间的块由 `.skill/gen-diagnostics.ps1` 写入。
> 标记下方的所有内容均为手写，并在重新生成后保留。

<a id="dsh3200"></a>

## DSH3200

<!-- generated:begin DSH3200 -->
**严重性** 错误

**消息**

```
Unexpected {0} at file scope; expected a declaration, '#pragma', '#include' or 'import'.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1401`
<!-- generated:end DSH3200 -->

**原因。** 解析器在文件作用域遇到了一个无法作为任何内容开头的标记：一个游离的 `)`、`}`、
运算符、数字或字符串，位于任何声明之外。常见来源是函数之后多出的一个右花括号、
写在文件作用域中的语句（`x = 1;` 前面需要一个类型），或
一个丢失了关键字的 1.x 块。

**修复。** 删除游离的标记，或将该行改为声明。恢复过程会跳到下一个
`;`、匹配的 `}` 或开始声明的行，因此其后的声明仍能解析；请先修复
第一个 DSH3200 并重新编译，然后再去处理后续的那些，它们通常是从更下方看到的
同一个错误。

<a id="dsh3201"></a>

## DSH3201

<!-- generated:begin DSH3201 -->
**严重性** 错误

**消息**

```
Preprocessor directive '#{0}' reached the parser; only '#pragma' and '#include' belong here, and '#if' / '#define' lines must be resolved by the preprocessor first.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:648`
<!-- generated:end DSH3201 -->

**原因。** 一行 `#`（而非 `#pragma` 或 `#include`（`#if`、`#define`、`#endif`、`#error`……））
到达了解析器。2.0 解析器期望的是预处理后的文本：`#if` 和 `#define` 会在解析之前
由 DreamShader 预处理器解析，且保留行数，因此在这里看到其中一个，说明
文本是原始解析的，或某个指令拼写错误（`#pragam`）。

**修复。** 在一次编译内部，这不可能发生；对于直接调用 `ParseDreamShaderLang`
的工具，请先运行预处理器。如果该指令是拼写错误，请更正它。在 `/// @custom`
主体内部，每一行 `#` 都会原样保留，永远不会到达此检查。

<a id="dsh3202"></a>

## DSH3202

<!-- generated:begin DSH3202 -->
**严重性** 错误

**消息**

```
'#pragma' needs a name: material, instance, pipeline, layout, region or endregion.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:668`、`Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:734`
<!-- generated:end DSH3202 -->

**原因。** 一行 `#pragma` 的形态不是解析器所读取的形态：

- 完全没有名称（仅有 `#pragma`，或 `#pragma (`），
- `#pragma material` / `#pragma layout` 的参数列表不是 `(Key = Value, ...)`
  （缺少 `)`、一个裸 `=`、未终止的字符串、不是标识符、数字或
  带引号字符串的值），
- `#pragma material` 带有位置参数：材质设置是以键索引的，只有 `layout` 接受
  前导的位置类型（`#pragma layout(Node, ...)`）。

**修复。** 请写入 `#pragma material(ShadingModel = Unlit, BlendMode = Additive)`、
`#pragma layout(Node, Var = UV, X = -1100, Y = -120)`、`#pragma region Title` 或
`#pragma endregion`。行尾的 `//` 注释是可以的，会被剥离。未知的
pragma 名称（`#pragma once`）不是错误：它会连同其文本被保留为 `EPragmaKind::Unknown`，
并被编译器忽略。

<a id="dsh3203"></a>

## DSH3203

<!-- generated:begin DSH3203 -->
**严重性** 错误

**消息**

```
'#include' needs a quoted path: #include "/Game/Shared/Common.dsh".
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:627`、`Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:824`
<!-- generated:end DSH3203 -->

**原因。** `#include` 后面没有双引号中的 `"path"`，或 `import` 后面没有
带引号的字符串。未加引号的路径（`#include /Game/X.dsh`）、只有一侧带引号的路径，
以及 HLSL 系统包含的写法 `#include <path>` 都是常见情况。尖括号
不被接受：DreamShader 没有系统包含目录，因此该写法毫无意义，
打印器也将不得不凭空编造一个。

**修复。** 给路径加引号：`#include "/Game/Shared/Common.dsh"` 或 `import "Hash.dsh";`。`import`
需要结尾的 `;`；`#include` 则不能有。两种写法产生相同的
`FIncludeDecl`；只有 `bImportSpelling` 不同，因此打印器可以按原样将文件写回。

<a id="dsh3204"></a>

## DSH3204

<!-- generated:begin DSH3204 -->
**严重性** 错误

**消息**

```
Expected a type name, found {0}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:415`
<!-- generated:end DSH3204 -->

**原因。** 期望一个类型名称，却发现了其他内容：`uniform = 1;`（缺少类型）、
前缀关键字吃掉了唯一标识符后的 `float3 = ...`、参数列表中的 `out x`，或
被用作类型的关键字（参数列表中的 `struct`）。

**修复。** 补上类型：`uniform float Intensity = 1;`。在 2.0 中，每个声明都是
`[uniform|static|const|extern|export] Type Name`，不存在无类型的 `var`。

<a id="dsh3205"></a>

## DSH3205

<!-- generated:begin DSH3205 -->
**严重性** 错误

**消息**

```
Expected a name after 'struct', found {1}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1137`、`Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1300`、`Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:859`、`Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:894`、`Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:953`
<!-- generated:end DSH3205 -->

**原因。** 期望一个名称，而那里的标记不是标识符。会针对变量或函数（`float ;`）、结构体（`struct {`）、
字段或参数（`float f(float)` —— 即使未使用，参数也必须命名）的名称引发。

**修复。** 添上名称。即使某个参数不使用也需要一个名称；它正是 `@param` 以及
生成的材质函数输入引脚所用的名称。

<a id="dsh3206"></a>

## DSH3206

<!-- generated:begin DSH3206 -->
**严重性** 错误

**消息**

```
Expected '`{' or ';' after the parameter list of '{0}', found {1}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1227`、`Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:997`
<!-- generated:end DSH3206 -->

**原因。** 在函数的参数列表之后，解析器既未找到 `{`，也未找到 `;`。通常是
`)` 与 `{` 之间有一个游离标记（`float f() const {`、`float f() 5`），或缺少一个 `{`。

**修复。** 让主体直接跟在参数列表之后，如果函数是 `extern`，则
让 `;` 直接跟在之后。2.0 中，参数列表与主体之间没有限定符。

<a id="dsh3207"></a>

## DSH3207

<!-- generated:begin DSH3207 -->
**严重性** 错误

**消息**

```
'{0}' is 'extern' and binds to an existing asset, so it cannot have a body; write a prototype ending in ';'.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1181`
<!-- generated:end DSH3207 -->

**原因。** 一个 `extern` 函数带有主体。`extern` 的意思是“将此名称绑定到现有的
材质函数资产”（通过 `/// @asset` 或库查找），因此主体将
无处安放。

**修复。** 要么去掉 `extern` 并保留主体（此时该函数将从源代码编译），要么
去掉主体并以 `;` 结束原型。恢复期间主体会被整体跳过，因此
其后的声明仍能解析。

<a id="dsh3208"></a>

## DSH3208

<!-- generated:begin DSH3208 -->
**严重性** 错误

**消息**

```
'{0}' has no body. Only an 'extern' prototype may end in ';'; a function you define needs '`{...`}'.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1218`
<!-- generated:end DSH3208 -->

**原因。** 一个不是 `extern` 的函数以 `;` 而不是主体结尾。2.0 源文件
没有前向声明：辅助函数可以按任意顺序定义，并会按名称
跨文件及其包含进行解析。

**修复。** 给该函数提供主体，如果它指向现有资产，则将其标记为 `extern`。如果这个声明
原本是前向声明，请删除它；它并不需要。

<a id="dsh3210"></a>

## DSH3210

<!-- generated:begin DSH3210 -->
**严重性** 错误

**消息**

```
A '.dsh' header cannot export '{0}'; only a '.dss' file produces assets.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParser.cpp:74`
<!-- generated:end DSH3210 -->

**原因。** 一个 `.dsh` 头文件包含了一个 `export` 函数。头文件会被包含进材质和
函数中，从不单独编译，因此其中的 `export` 没有可以成为的资产，
并且会被复制到每个包含者中。

**修复。** 移除 `export`（该函数将成为一个普通辅助函数，对所有包含该头文件的文件可见），或将其移入其专属的 `.dss` 文件中。每个导出的声明触发一次，来自 `LangParser.cpp` 中的模块循环；该声明保留在语法树中，以便工具仍能看到它。

<a id="dsh3211"></a>

## DSH3211

<!-- generated:begin DSH3211 -->
**严重性** 错误

**消息**

```
Expected the end of the expression, found {0}.
```

**触发自** `Source/DreamShaderLang/Private/Lang/LangParser.cpp:572`
<!-- generated:end DSH3211 -->

**原因。** `ParseDreamShaderLangExpression`（测试、语言服务、`#pragma` 值）接收到的文本在一个完整表达式之后还有其他内容：`a + b c`、`x = 1;`（`;` 不是表达式的一部分）。

**修复。** 仅传入一个表达式。在编辑器中，这仅出现在 `#pragma` 参数本应是表达式却并非如此的情况下。

<a id="dsh3213"></a>

## DSH3213

<!-- generated:begin DSH3213 -->
**严重性** 错误

**消息**

```
'{0}' cannot be combined with the keywords before it; a declaration is 'uniform', 'static const', 'static', 'const', 'extern' or 'export', not a mix.
```

**触发自** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1087`、`Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1155`、`Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1241`
<!-- generated:end DSH3213 -->

**原因。** 声明的前缀关键字不搭配，或与错误类型的声明搭配：

- 重复或冲突的前缀：`uniform static`、`const uniform`、`export extern`、
  `uniform uniform`；
- 函数上的存储类：`uniform float f()` — `uniform`、`static` 和 `const` 描述的是
  变量；
- 变量上的链接性：`export float x`、`extern float3 Dir` — `extern` 和 `export` 描述的是
  函数。

**修复。** 使用一种可接受的形式：变量使用 `uniform T x`、`static const T x`、`static T x`、
`const T x`、`T x`；函数使用 `T f()`、`export T f()`、`extern T f();`。由多个材质共享的
常量是 `static const`；材质实例可编辑的值是
`uniform`。

<a id="dsh3214"></a>

## DSH3214

<!-- generated:begin DSH3214 -->
**严重性** 错误

**消息**

```
Parameter '{0}' is 'out' and cannot have a default value; only inputs are optional.
```

**触发自** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:972`
<!-- generated:end DSH3214 -->

**原因。** 一个 `out` 参数带有默认值：`out float Alpha = 1.0`。输出由函数写入，对调用方而言不可能是可选的，因此默认值没有意义。

**修复。** 移除默认值。如果调用方应能够不连接该引脚，请将参数改为 `in` 并以其他方式返回值，或者给调用方一个可传入的 `uniform`。`inout`
参数也不能有默认值。

<a id="dsh3215"></a>

## DSH3215

<!-- generated:begin DSH3215 -->
**严重性** 错误

**消息**

```
Expected ']' to close the array dimension, found {1}.
```

**触发自** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:453`
<!-- generated:end DSH3215 -->

**原因。** 数组维度以 `[` 开始但未闭合：`float Weights[4;`、
`float3 P[;`。该维度必须是 `[` 与 `]` 之间的单个表达式。

**修复。** 闭合方括号。大小是表达式，可以引用 `static const` 值；
`float x[]`（无大小）会被解析器接受，留给编译器拒绝或推断。

<a id="dsh3216"></a>

## DSH3216

<!-- generated:begin DSH3216 -->
**严重性** 错误

**消息**

```
Expected ';' after the import path, found {1}.
```

**触发自** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1277`、`Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1324`、`Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:838`、`Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:902`、`Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:911`
<!-- generated:end DSH3216 -->

**原因。** 某个声明缺少其 `;`：在变量及其初始化器之后、在
`import "x.dsh"` 之后、在结构体字段之后、在结构体的闭合 `}` 之后，或在
`uniform float a, b` 中两个声明符之间。

**修复。** 添加 `;`。解析器会告诉你它正在闭合什么（消息会说明“在声明之后”、“在导入路径之后”、“在字段之后”、“在结构体的闭合 '}' 之后”）。
最常见的形式是 `struct X { ... }` 之后缺少 `;` —— HLSL 和 C 都需要它。

<a id="dsh3217"></a>

## DSH3217

<!-- generated:begin DSH3217 -->
**严重性** 错误

**消息**

```
Expected '{' after the struct name, found {1}.
```

**触发自** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:863`
<!-- generated:end DSH3217 -->

**原因。** `struct Name` 之后未跟随 `{`。通常是前向声明（`struct X;`），2.0 不支持；或基类冒号（`struct X : Y`），它也不支持。

**修复。** 就地定义结构体：`struct X { float a; };`。类型在整个文件中按名称解析，
因此结构体可以在使用它的函数之后声明。

<a id="dsh3218"></a>

## DSH3218

<!-- generated:begin DSH3218 -->
**严重性** 错误

**消息**

```
Expected ')' to close the parameter list, found {1}.
```

**触发自** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:986`
<!-- generated:end DSH3218 -->

**原因。** 参数列表未以 `)` 闭合。要么缺少 `)`，要么参数之间使用了 `,` 以外的分隔符（`float a; float b`），要么参数带有解析器不认识的限定符（`const float a` —— 2.0 参数只接受 `in`、`out`、`inout`）。

**修复。** 用逗号分隔参数并闭合列表。从参数中移除 `const`；在生成的图中输入
已是只读的。

<a id="dsh3219"></a>

## DSH3219

<!-- generated:begin DSH3219 -->
**严重性** 警告

**消息**

```
'#pragma nogeneration' takes no arguments, and '{0}' was ignored: the file is still marked. Remove the line to have it built automatically again.
```

**触发自** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:691`
<!-- generated:end DSH3219 -->

**原因。** `#pragma nogeneration` *（2.1.2 之后）* 后面写有其他内容，例如 `(false)`。它不接受任何内容，且文件**会保持**标记状态：除了移除该行外无法关闭它。

**修复。** 单独写 `#pragma nogeneration`，或移除该行以让文件重新自动构建。

<a id="dsh3220"></a>

## DSH3220

<!-- generated:begin DSH3220 -->
**严重性** 警告

**消息**

```
A '@' in a '///' line must be followed by a directive name; the text is kept as description.
```

**触发自** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:577`
<!-- generated:end DSH3220 -->

**原因。** *（警告）* 一行 `///` 中，`@` 位于指令位置（文本开头或空格之后）且未跟随指令名称：`/// see @ the docs`、`/// @ desc Foo`、
`/// @123`。该行会作为描述文本保留，因此不会丢失任何内容，但如果本意是指令则未被读取。

**修复。** 将指令写为 `@name value`，且 `@` 后无空格：`/// @desc Overall gain`。
单词内部（`user@host`）的 `@` 或其后跟空格的均视为纯文本，仅当它位于指令起始位置时才会发出警告。读取时指令键会转为小写，因此 `@Desc`
和 `@desc` 是同一个键。

<a id="dsh3221"></a>

## DSH3221

<!-- generated:begin DSH3221 -->
**严重性** 警告

**消息**

```
This '///' block is not followed by a field and is ignored.
```

**触发自** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1352`、`Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:884`
<!-- generated:end DSH3221 -->

**原因。** *（警告）* 一个 `///` 块未附加到任何内容：它位于文件末尾且之后没有任何声明，或位于结构体主体末尾且之后没有任何字段。文档块属于其后的声明，因此后面没有内容的块会被丢弃。

**修复。** 将该块移到它所描述的声明之上；如果它只是注释，则改为 `//` 注释。位于 `#pragma` 或 `#include` 行之上的块会附加到该指令并保留，
因此写在第一个 `#pragma` 之上、形式为 `///` 的文件头注释不会被孤立。

<a id="dsh3222"></a>

## DSH3222

<!-- generated:begin DSH3222 -->
**严重性** 错误

**消息**

```
Expected a 2.0 declaration, found the 1.x declaration '{0}'; 1.x declarations belong in a .dsh header or in a .dsm or .dsf file.
```

**触发自** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1119`
<!-- generated:end DSH3222 -->

**原因。** 在 2.0 声明应开始的位置发现了 1.x 声明词——`Function`、`GraphFunction`、`Namespace`、`VirtualFunction`、
`Shader`、`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend`。2.0 前端不读取 1.x 块语法：由扩展选择前端，而
`.dss` 要求使用这一前端。

**修复。** 将 1.x 源保留在由 1.x 前端编译的 `.dsm` / `.dsf` / `.dsh` 文件中，或将
声明移植到 2.0：`Function float Luma(in vec3 c) { ... }` 变为
`float Luma(float3 c) { ... }`，`Shader(Name = "X") { ... }` 变为 `export void X(inout material m)
{ ... }` with `#pragma material(...)` 以表示设置。消息会指出它看到的那个词；
恢复期间整个块被跳过，因此文件的其余部分仍会报告自身的错误。

<a id="dsh3250"></a>

## DSH3250

<!-- generated:begin DSH3250 -->
**严重性** 错误

**消息**

```
Expected a property type and a name, found {0}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1042`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:719`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:731`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:773`
<!-- generated:end DSH3250 -->

**原因。** `Properties` 节的某一行没有以类型和名称开头。每个属性均为
`<Type> <Name> [= default] [ [metadata] ];`。

**修复。** 检查上一行是否缺少 `;`，并确认类型是单个词（`float3`、
`Texture2D`、`ScalarParameter`……）。

<a id="dsh3251"></a>

## DSH3251

<!-- generated:begin DSH3251 -->
**严重性** 错误

**消息**

```
Expected a property type after 'const', found {0}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:674`
<!-- generated:end DSH3251 -->

**原因。** `const` 后面没有跟随属性类型。

**修复。** 写入 `const float K = 1.0;` 或 `const Texture2D Noise = Path(...);`。

<a id="dsh3252"></a>

## DSH3252

<!-- generated:begin DSH3252 -->
**严重性** 错误

**消息**

```
Expected a property type such as 'float', 'float4', 'Texture2D' or 'ScalarParameter', found '{0}'.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:839`
<!-- generated:end DSH3252 -->

**原因。** 类型位置上的词不是 1.x 属性可以具有的类型：既不是 HLSL 值类型，
也不是纹理类型，也不是 1.x 按名称识别的某个参数节点类。

**修复。** 使用值类型（`float` …… `float4`、`vec*`）、纹理类型，或诸如
`ScalarParameter`、`VectorParameter`、`TextureObjectParameter`、`StaticSwitchParameter` 的参数类。1.x
属性类型匹配时忽略大小写，正如 1.x 所做的那样，因此出错的是这个词本身。

<a id="dsh3253"></a>

## DSH3253

<!-- generated:begin DSH3253 -->
**严重性** 错误

**消息**

```
Expected a property type with a 2.0 spelling, found '{0}{1}', which has none; move this material to a .dss file and write the node with UE.Expression.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:851`
<!-- generated:end DSH3253 -->

**原因。** 该属性是 1.x 可以声明而 2.0 没有对应声明的节点类，或 `const`
作用于一个不能作为常量的属性（参数节点）。两个前端构建同一个 AST，因此
属性需要一种 2.0 形式：`uniform`、`static const`，或在读取处展开的节点。

**修复。** 将其声明为普通值或参数；对于任何其他节点类，将材质迁移到
`.dss` 文件，并在使用处写入该节点，如 `UE.<Class>(...)`。

<a id="dsh3254"></a>

## DSH3254

<!-- generated:begin DSH3254 -->
**严重性** 错误

**消息**

```
Expected a default value after '{0} =', found {1}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:753`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:869`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:911`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:935`
<!-- generated:end DSH3254 -->

**原因。** 某属性具有 `=`，且其后没有任何前端可读作默认值的内容。默认值可以是
数字、构造函数（`float4(1, 0, 0, 1)`）、`true` / `false`、资源引用（`Path(...)`、
带引号的对象路径、内容浏览器引用），或内建属性的 `UE.*` 节点。

**修复。** 以上述形式之一书写默认值，或移除 `=`。

<a id="dsh3255"></a>

## DSH3255

<!-- generated:begin DSH3255 -->
**严重性** 错误

**消息**

```
Expected ']' to close the metadata block, found {0}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:186`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:199`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:253`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:273`
<!-- generated:end DSH3255 -->

**原因。** 属性或参数之后的元数据块 `[ ... ]` 未闭合，或其中的某一项
不是 `Key = value;`。

**修复。** 用 `]` 闭合块，并以 `;` 结束每一项。

<a id="dsh3256"></a>

## DSH3256

<!-- generated:begin DSH3256 -->
**严重性** 错误

**消息**

```
Expected the metadata key '{0}' once, found it again.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:307`
<!-- generated:end DSH3256 -->

**原因。** 同一个 `[ ... ]` 块中某个元数据键被写了两次。

**修复。** 保留其中之一。

<a id="dsh3257"></a>

## DSH3257

<!-- generated:begin DSH3257 -->
**严重性** 错误

**消息**

```
Expected 'Slider(min, max)' with two numbers, found '{0}'.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:236`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:299`
<!-- generated:end DSH3257 -->

**原因。** `Slider` 不具有带有两个数字的 `Slider(min, max)` 形式，或范围
给出了两次：`Slider(...)` 与同一元数据
块中的 `SliderMin` / `SliderMax` 风格边界同时出现。

**修复。** 只写一次范围，例如 `Slider(0.0, 4.0)`。

<a id="dsh3258"></a>

## DSH3258

<!-- generated:begin DSH3258 -->
**严重性** 错误

**消息**

```
Expected 'SortPriority' to be a whole number, found '{0}'.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1797`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:473`
<!-- generated:end DSH3258 -->

**原因。** `SortPriority`（或其简写形式 `Sort`）不是整数。引擎
按整数对参数排序。

**修复。** 写入整数：`SortPriority = 10;`。

<a id="dsh3259"></a>

## DSH3259

<!-- generated:begin DSH3259 -->
**严重性** 错误

**消息**

```
Expected ')' to close the arguments of 'UE.{0}', found {1}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:703`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:817`
<!-- generated:end DSH3259 -->

**原因。** 某内建属性（`float3 Cam = UE.CameraPositionWS();`）打开了其
`UE.*` 节点的参数列表且从未闭合，或将节点的参数作为默认值写在名称之后，
而不是写在括号内。

**修复。** 闭合 `)`，并将节点的参数写在其中：`float2 UV = UE.TexCoord(Index =
1);`。

<a id="dsh3260"></a>

## DSH3260

<!-- generated:begin DSH3260 -->
**严重性** 错误

**消息**

```
Expected a name inside 'Group("...")', found an empty string.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:635`
<!-- generated:end DSH3260 -->

**原因。** `Group("") { ... }` 的名称为空。此类块的成员会将此名称作为其
参数组。

**修复。** 为该组指定名称，或删除该块并让属性保持未分组。

<a id="dsh3261"></a>

## DSH3261

<!-- generated:begin DSH3261 -->
**严重性** 错误

**消息**

```
Expected a setting name such as 'BlendMode', found {0}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1110`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1135`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1147`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1161`
<!-- generated:end DSH3261 -->

**原因。** `Settings` 节的某一行没有以设置名称开头。每项设置均为
`<Name> = <value>;`。

**修复。** 检查上一行是否缺少 `;`。

<a id="dsh3262"></a>

## DSH3262

<!-- generated:begin DSH3262 -->
**严重性** 警告

**消息**

```
The setting '{0}' is written twice; the later value wins, as it did in 1.x.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1181`
<!-- generated:end DSH3262 -->

**原因。** 某项设置被写了两次。1.x 保留后出现的值，此前端也如此。

**修复。** 移除其中之一。

<a id="dsh3263"></a>

## DSH3263

<!-- generated:begin DSH3263 -->
**严重性** 警告

**消息**

```
'{0}' is not a setting of '{1}'; 1.x ignored it and so does this front end.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1199`
<!-- generated:end DSH3263 -->

**原因。** 该设置不属于块的类别所拥有的设置（ShaderFunction 中的材质设置、
拼写错误的函数设置）。1.x 会忽略它不认识的内容，因此源会像以往一样构建
——只是没有该设置。

**修复。** 对照 `Docs/settings` 检查拼写，或删除该行。

<a id="dsh3264"></a>

## DSH3264

<!-- generated:begin DSH3264 -->
**严重性** 警告

**消息**

```
'UserExposedCaption' has no 2.0 spelling and is not applied; its value is kept for migration.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1187`
<!-- generated:end DSH3264 -->

**原因。** `UserExposedCaption` 被设置在某个函数上。2.0 没有针对它的指令，编译器也不会写入它：新资源没有标题，而早期版本已设置过标题的资源会保留其原有标题。该值会保留在迁移记录中。

**修复。** 对构建而言无需处理。如果标题很重要，请在迁移后手动在资源上设置它。

<a id="dsh3265"></a>

## DSH3265

<!-- generated:begin DSH3265 -->
**严重性** 警告

**消息**

```
Expected 'true' or 'false' for 'ExposeToLibrary', found '{0}'; 1.x ignored the setting and so does this front end.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1173`
<!-- generated:end DSH3265 -->

**原因。** `ExposeToLibrary` 既不是 `true` 也不是 `false`。1.x 会忽略此类的值，并将函数留在原处。

**修复。** 写入 `ExposeToLibrary = true;`。

<a id="dsh3266"></a>

## DSH3266

<!-- generated:begin DSH3266 -->
**严重性** 错误

**消息**

```
Expected ';' after the output declaration '{0}', found {1}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1202`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1322`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1627`
<!-- generated:end DSH3266 -->

**原因。** 一个 `Outputs` 节不是前端所读取的内容：该节未以 `{` 开头，某个声明之后没有 `;`，或者某行既不是输出声明、`Base.<Attribute> =
<source>;`, nor `Expression(...).Pin[<index>] = <source>;`。

**修复。** 在每行末尾加上 `;`，并使用三种形式之一。

<a id="dsh3267"></a>

## DSH3267

<!-- generated:begin DSH3267 -->
**严重性** 错误

**消息**

```
Expected 'Pin[<index>] = <source>' for an Expression(...) output target, found {0}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1230`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1415`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1425`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1437`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1448`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1464`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1493`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1559`
<!-- generated:end DSH3267 -->

**原因。** 绑定到自定义输出节点的 `Outputs` 不是 `Expression(Class =
"...").Pin[<index>] = <source>;` (or, in block form, `Pin[<index>] = <source>;`（位于花括号内）这一形式。

**修复。** 通过索引来命名引脚：`Pin[0] = Color;`。

<a id="dsh3268"></a>

## DSH3268

<!-- generated:begin DSH3268 -->
**严重性** 错误

**消息**

```
Expected each pin of Expression(Class = "{0}") to be bound once, found Pin[{1}] bound again.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1606`
<!-- generated:end DSH3268 -->

**原因。** `Expression(...)` 输出目标的块形式对同一个引脚绑定了两次。

**修复。** 每个引脚只保留一个绑定。

<a id="dsh3269"></a>

## DSH3269

<!-- generated:begin DSH3269 -->
**严重性** 错误

**消息**

```
Expected at least one 'Pin[<index>] = <source>;' in the Expression(...) block, found none.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1549`
<!-- generated:end DSH3269 -->

**原因。** `Expression(...)` 输出目标的块形式中没有任何引脚绑定，因此该节点将在没有任何连线的情况下构建。

**修复。** 至少添加一个 `Pin[<index>] = <source>;`。

<a id="dsh3270"></a>

## DSH3270

<!-- generated:begin DSH3270 -->
**严重性** 错误

**消息**

```
Expected a source after 'Pin[{0}] =', found none.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1246`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1302`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1367`
<!-- generated:end DSH3270 -->

**原因。** `Outputs` 节中的 `=` 后面没有任何内容：`Pin[<index>] =`、`Base.<Attribute>
=`，或输出声明的初始化器。

**修复。** 指定 Graph 的变量或常量作为源。

<a id="dsh3271"></a>

## DSH3271

<!-- generated:begin DSH3271 -->
**严重性** 错误

**消息**

```
Expected a parameter type and name such as 'float Amount', found {0}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1659`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1694`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1730`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1756`
<!-- generated:end DSH3271 -->

**原因。** 函数的 `Inputs` 或 `Outputs` 节中的某行不是 `[opt] <Type> <Name> [=
default] [ [metadata] ];`。

**修复。** 检查类型拼写以及上一行的 `;`。

<a id="dsh3272"></a>

## DSH3272

<!-- generated:begin DSH3272 -->
**严重性** 警告

**消息**

```
'opt' on the output '{0}' means nothing; 1.x ignored it and so does this front end.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1770`
<!-- generated:end DSH3272 -->

**原因。** `opt` 位于输出上。只有输入可以是可选的；1.x 会读取该词但不做任何处理。

**修复。** 将其移除。

<a id="dsh3273"></a>

## DSH3273

<!-- generated:begin DSH3273 -->
**严重性** 警告

**消息**

```
A default on the output '{0}' means nothing; 1.x ignored it, so it is dropped.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1778`
<!-- generated:end DSH3273 -->

**原因。** 输出带有默认值。输出是 Graph 所赋的值；1.x 会读取默认值并将其丢弃。

**修复。** 移除默认值，或在 Graph 中赋值。

<a id="dsh3274"></a>

## DSH3274

<!-- generated:begin DSH3274 -->
**严重性** 错误

**消息**

```
Expected 'Node(...)' or 'Comment(...)' in the Layout section, found {0}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1838`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1863`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1884`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1894`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1906`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1922`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1938`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1957`
<!-- generated:end DSH3274 -->

**原因。** `Layout` 节中包含的内容不是 `Node(...)` 和 `Comment(...)` 条目。

**修复。** 写入 `Node(Var = "Name", X = 0, Y = 0);` 或 `Comment(Name = "Title", X = 0, Y = 0, W =
400, H = 200);`。

<a id="dsh3275"></a>

## DSH3275

<!-- generated:begin DSH3275 -->
**严重性** 错误

**消息**

```
Expected the argument '{0}' in '{1}(...)', found none.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1998`、`Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:2010`
<!-- generated:end DSH3275 -->

**原因。** `Node(...)` 或 `Comment(...)` 条目缺少一个它不能缺少的参数——对于节点是 `Var`、
`X`、`Y`；对于注释是 `Name`、`X`、`Y`、`W`、`H`——或者某个位置或大小不是整数。

**修复。** 添加该参数；位置和大小是以图单位为单位的整数。

<a id="dsh3276"></a>

## DSH3276

<!-- generated:begin DSH3276 -->
**严重性** 错误

**消息**

```
Expected 'Color' to be a vector literal such as '(0.1, 0.16, 0.22, 0.35)', found '{0}'.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:2036`
<!-- generated:end DSH3276 -->

**原因。** `Comment(...)` 的 `Color` 不是括号中的四个数字。

**修复。** 写入 `Color = (0.1, 0.16, 0.22, 0.35)`——红、绿、蓝、alpha。

<a id="dsh3277"></a>

## DSH3277

<!-- generated:begin DSH3277 -->
**严重性** 警告

**消息**

```
The layer input '{0}' becomes the 'inout material' parameter named after the output '{1}', so the input pin changes its name.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1252`
<!-- generated:end DSH3277 -->

**原因。** 1.x 图层为其 MaterialAttributes 输入和输出使用了不同的名称。2.0 图层有一个 `inout material` 参数，它同时是两个引脚，并采用输出的名称——因此图层资源的输入引脚被重命名。在主体内部，旧的输入名称仍读取传入的材质。

**修复。** 源码中无需更改。图层堆栈按位置连接图层的唯一输入，因此现有堆栈仍可正常工作；通过引脚名称连接函数调用的图需要重新连接一次。

<a id="dsh3278"></a>

## DSH3278

<!-- generated:begin DSH3278 -->
**严重性** 错误

**消息**

```
Expected a MaterialAttributes output on '{0}', found none.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1222`
<!-- generated:end DSH3278 -->

**原因。** `ShaderLayer` 或 `ShaderLayerBlend` 没有类型为 `MaterialAttributes` 的输出。图层堆栈从图层或混合中恰好读取一个此类输出。

**修复。** 在 `Outputs` 中声明它：`MaterialAttributes Result;`。

<a id="dsh3300"></a>

## DSH3300

<!-- generated:begin DSH3300 -->
**严重性** 错误

**消息**

```
Expected the buffer's name after 'buffer', found {1}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:122`
<!-- generated:end DSH3300 -->

**原因。** `buffer` 开始了 `.dsp` 的声明，但其后没有名称：名称缺失
（`buffer : R8;`）、被引号括起（`buffer "Mask" : R8;`），或者是该语言的关键字（`buffer in : R8;`）。

**修复。** 用普通标识符写入 `buffer <Name> : <Format>;`：`buffer Mask : R8;`。该名称是
通过绑定判定的依据（区分大小写）、`r.DreamPass.Visualize <Pipeline>.<Buffer>` 所采用的内容，并且
对于导出的缓冲区，它是其渲染目标名称的一部分，`<Pipeline>_<Buffer>`。

<a id="dsh3301"></a>

## DSH3301

<!-- generated:begin DSH3301 -->
**严重性** 错误

**消息**

```
Expected ':' and a format after 'buffer {0}', found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:126`
<!-- generated:end DSH3301 -->

**原因。** 缓冲区的名称之后没有跟随 `:` 及其格式：格式缺失
（`buffer Mask;`）、`:` 为（`buffer Mask R8;`、`buffer Mask = R8;`）、参数列表出现在
格式之前（`buffer Mask(Clear = 0) : R8;`），或两个名称共用一个声明
（`buffer Mask, Edge : R8;`）。

**修复。** 单独声明每个缓冲区，格式在前，参数（如果有）在其后：

```hlsl
buffer Mask : R8(Clear = 0);
buffer Edge : R8;
```

<a id="dsh3302"></a>

## DSH3302

<!-- generated:begin DSH3302 -->
**严重性** 错误

**消息**

```
Expected a buffer format such as 'R8' or 'RGBA16F', found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:130`
<!-- generated:end DSH3302 -->

**原因。** 缓冲区名称之后的 `:` 没有跟随格式词：格式缺失
（`buffer Mask : ;`、`buffer Mask : (Clear = 0);`）或被引用（`buffer Mask : "R8";`）。此处任何名称
都可解析；不是格式的名称（`RGBA16`、`rgba16f`）是绑定器的 DSH7305。

**修复。** 写入格式时不加引号：`R8`、`RG8`、`RGBA8`、`R16F`、`RG16F`、`RGBA16F`、`R32F`、
`RG32F`、`RGBA32F`，或 `Depth32` 用于网格通道自身的深度。`R32U` 和 `RG32U` 是保留的：目前没有
通道读取或写入整数缓冲区（DSH7321），而一个 id 恰好能容纳 `R32F` 直至 16777216。

<a id="dsh3303"></a>

## DSH3303

<!-- generated:begin DSH3303 -->
**严重性** 错误

**消息**

```
Expected the pass's name after 'pass', found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:226`
<!-- generated:end DSH3303 -->

**原因。** `pass` 开始一个 `.dsp` 的声明，且其后没有名称：`pass : mesh { ... }`、
`pass "DrawMask" : mesh { ... }`，或一个关键字出现在名称应处的位置。

**修复。** 使用普通标识符为通道命名：`pass DrawMask : mesh { ... }`。GPU 捕获和
Insights 将该通道显示为 `<Pipeline>.<Pass>`，因此在一条管线中每个名称只能使用一次（DSH4401）。

<a id="dsh3304"></a>

## DSH3304

<!-- generated:begin DSH3304 -->
**严重性** 错误

**消息**

```
Expected ':' and a pass kind after 'pass {0}', found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:230`
<!-- generated:end DSH3304 -->

**原因。** 通道的名称之后没有跟随 `:` 及其种类：种类缺失
（`pass Blur { ... }`）、`:` 为（`pass Blur compute { ... }`），或种类被写成了
参数（`pass Blur(compute) { ... }`）。

**修复。** 在块之前写入 `pass <Name> : <kind>`：`pass Blur : compute { ... }`。种类有
`fullscreen`、`compute`、`mesh`、`clear` 和 `copy`。

<a id="dsh3305"></a>

## DSH3305

<!-- generated:begin DSH3305 -->
**严重性** 错误

**消息**

```
Expected a pass kind: fullscreen, compute, mesh, clear or copy, found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:234`
<!-- generated:end DSH3305 -->

**原因。** 通道名称之后的 `:` 没有跟随种类：它缺失
（`pass Blur : { ... }`）或被引用（`pass Blur : "compute" { ... }`）。此处任何名称都可解析；不是
种类的名称（`Compute`、`postprocess`）是绑定器的 DSH7304。

**修复。** 写入种类时不加引号，使用小写：`fullscreen`（一个后处理材质，或一个
作为像素着色器运行的 `.usf`）、`compute`（一个 `.usf` 计算着色器）、`mesh`（选中的图元被再次
绘制）、`clear` 或 `copy`。

<a id="dsh3310"></a>

## DSH3310

<!-- generated:begin DSH3310 -->
**严重性** 错误

**消息**

```
'buffer {0}' declares a buffer of a Custom Pass pipeline, which only a '.dsp' file holds; move it into the pipeline's '.dsp'.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:99`
<!-- generated:end DSH3310 -->

**原因。** 一个 `.dss`、`.dsh` 或 `.dsi` 声明了 `buffer <Name> : ...` 或 `pass <Name> : ...`（
消息随后显示 `'pass ...' declares a pass`）。缓冲区和通道属于自定义通道管线，
而 `buffer` 和 `pass` 是 `.dsp` 中的声明词；在其他任何地方它们都是普通名称，
而这段文本一直是语法错误——消息现在会指出它应属之处。一个仅仅
叫做 `buffer` 或 `pass` 的变量没有问题：是名称之后的 `:` 使其成为管线
声明。

**修复。** 将该声明移入管线的 `.dsp`。材质不声明它所
读取的缓冲区：全屏通道绘制时通过 `UE.UserSceneTexture` 读取它们，由通道的 `read`
行绑定，而任何其他材质通过
`UE.DreamPassBuffer(Pipeline = "...", Buffer = "...")` 读取导出的缓冲区。

<a id="dsh3311"></a>

## DSH3311

<!-- generated:begin DSH3311 -->
**严重性** 错误

**消息**

```
'#pragma pipeline' configures a Custom Pass pipeline and belongs in a '.dsp' file of its own, and this line is in '{0}'; move it, with the pipeline's buffers and passes, into a '.dsp'.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4818`
<!-- generated:end DSH3311 -->

**原因。** `#pragma pipeline(...)` 出现在 `.dss`、`.dsh` 或 `.dsi` 中——消息会指出
文件名。自定义通道管线是一个独立的源文件：`.dsp` 持有该 pragma 以及
管线的参数、缓冲区和通道，并成为一个以该文件命名的 `UDreamPassPipeline`。

**修复。** 将该 pragma 及其配置的缓冲区和通道移入一个 `.dsp`；材质浏览器中的 **New Source**
提供三个管线模板供起步使用。通道所绘制的材质
保留在其 `.dss` 文件中，并带有其 `#pragma material(...)`。

<a id="dsh3312"></a>

## DSH3312

<!-- generated:begin DSH3312 -->
**严重性** 错误

**消息**

```
An 'hlsl' block at file scope is the HLSL of a Custom Pass pipeline, which only a '.dsp' file holds, and '{0}' is not one; a pipeline cannot be included.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4831`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4855`
<!-- generated:end DSH3312 -->

**原因。** 一个 `buffer` 或 `pass` 声明，或一个文件级 `hlsl` 块，到达了 `.dss` 或
`.dsh` 的绑定器——即，一个 `.dsp` 被包含了。解析器仅在 `.dsp` 中进行这些声明（在其他地方该文本是
DSH3310），因此这里的唯一途径是某个宿主的包含解析器解析了一个 `.dsp`；编辑器与 `dsc` 的
解析器在此发生之前拒绝包含除 `.dsh` 或 `.dss` 之外的任何内容（DSH8295）。它会在 `.dsp` 中报告，每个缓冲区和通道一次，并指出正在编译的文件。

**修复。** 移除该包含。管线不通过包含来共享：材质通过
`UE.DreamPassBuffer`（一个导出的缓冲区）读取其结果，或通过绘制它的通道的 `read` 和 `param`
行读取。

<a id="dsh3313"></a>

## DSH3313

<!-- generated:begin DSH3313 -->
**严重性** 错误

**消息**

```
'#pragma pipeline' is written a second time, and one '.dsp' is one pipeline; the line {0} already configures it.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:691`
<!-- generated:end DSH3313 -->

**原因。** 一个 `.dsp` 有第二个 `#pragma pipeline(...)`。第一个配置管线；
这一个被整体拒绝，其所有键都不会合并到第一个中。

**修复。** 将键合并到一个 pragma 中，如下所示
`#pragma pipeline(Order = 100, Views = Game | Editor | SceneCapture)`。应当各自独立运行的
两条管线是两个 `.dsp` 文件。

<a id="dsh3314"></a>

## DSH3314

<!-- generated:begin DSH3314 -->
**严重性** 错误

**消息**

```
'#pragma material' configures a material, and a '.dsp' is configured by '#pragma pipeline(...)'; the material a pass draws with is a '.dss' of its own.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:708`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:741`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:751`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:761`
<!-- generated:end DSH3314 -->

**原因。** 一个 `.dsp` 持有只有材质源才能使用的声明。管线持有
`#pragma pipeline`、`uniform`、`static const`、`buffer` 和 `pass` 声明以及一个 `hlsl` 块；另外四种
种类属于此错误，各有一条消息：

- `#pragma material(...)` -- 材质的设置，属于材质某个 pass 所绘制内容的 `.dss`；
- 位于 `hlsl` 块之外的函数 -- HLSL pass 的代码是一个 `hlsl { }` 块或其 `.usf`，材质的代码是其 `.dss`，而 `param` 值则由管线的参数折叠而来；
- 位于 `hlsl` 块之外的 `struct`；
- 位于 `hlsl` 块之外的 `#include` 或 `import` -- `.dsp` 中的任何内容都无法使用它所声明的东西；`#include` HLSL 所需的内容要放在该块内部。

**修复。** 把每一项移到它该在的位置：材质设置移到该材质 `.dss` 的 `#pragma material(...)` 中，函数移到 `.dss` 或 `.dsh`（图代码）中，或移到 `hlsl` 块或 pass 的 `.usf` 中（HLSL），`struct` 移到 `.dsh` 或 `hlsl` 块中；删除该 include，或将其移到 `hlsl` 块中。管线自身的设置就是
`#pragma pipeline(...)` 的键：`Order`、`Injection`、`Views`、`Requires` 和 `Enabled`。

<a id="dsh3315"></a>

## DSH3315

<!-- generated:begin DSH3315 -->
**严重程度** 警告

**消息**

```
'#pragma {0}' boxes or places graph nodes, and a '.dsp' has no graph; the line was ignored.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:722`
<!-- generated:end DSH3315 -->

**原因。** `#pragma layout`、`#pragma region` 或 `#pragma endregion` 位于 `.dsp` 中。在 `.dss` 中，这些内容会放置生成的节点并用注释将其框起；而管线不生成图，因此该行会被忽略，管线会当作它不存在一样进行构建。

**修复。** 将其删除。要对较长的 `.dsp` 中的声明进行分组，请使用 `//` 注释，它们能在
`dsc fmt` 和 Adopt 后保留；要对细节面板中的参数进行分组，请在 `uniform` 上使用 `/// @group`。

<a id="dsh3316"></a>

## DSH3316

<!-- generated:begin DSH3316 -->
**严重程度** 错误

**消息**

```
'{0}' is a file-scope variable of a '.dsp', which is a 'uniform' (a parameter an activation may override) or a 'static const' (a compile-time value).
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:826`
<!-- generated:end DSH3316 -->

**原因。** `.dsp` 的文件作用域变量在声明时没有 `uniform` 或 `static const`：
`float Radius = 2.0;`、`static float Scale = 0.5;`。管线有两种值：参数 -- 激活项（体积、组件、项目设置、蓝图）可以覆盖它 -- 以及常量，常量在使用处折叠（普通的 `const` 会当作常量）。普通变量两者都不是。该声明会被丢弃，因此命名它的 `param` 或 `Enabled` 也会 DSH4404。

**修复。** 当激活项应当能够更改该值时写 `uniform`，当该值固定时写 `static const`：

```hlsl
/// @group Blur   @slider 1 8
uniform float BlurRadius = 3.0;
static const float HalfRes = 0.5;
```

<a id="dsh3317"></a>

## DSH3317

<!-- generated:begin DSH3317 -->
**严重程度** 警告

**消息**

```
'@{0}' has no effect on {1} in a '.dsp'; remove it.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:793`
<!-- generated:end DSH3317 -->

**原因。** 位于 `.dsp` 声明上方的 `///` 指令对管线没有意义。每种声明接受的内容：

| 上方为 | 指令 |
| :-- | :-- |
| 一个 `uniform` | `@group`、`@desc`、`@slider`、`@sort`；`Texture2D` 上的 `@default` |
| 一个 `static const`、一个 `buffer`、一个 `pass` | `@desc` |
| `#pragma pipeline` | 无；其 `///` 块用于记录该文件 |

在 `uniform` 或 `static const` 上，该警告针对 `@name`、`@sampler`、`@static` 和 `@page`，这些指令对材质或材质实例的参数有意义，也针对该语言未定义的键，`.dss` 会按引擎名称将其交给参数节点 -- 管线参数不是节点。该语言的其他指令若放错位置，在任何文件中都会 DSH7224。

**修复。** 移除该指令。`///` 块中的纯文本永远不会被警告：在参数、缓冲区或 pass 上方，它就是描述，就如同 `@desc` 一样。
