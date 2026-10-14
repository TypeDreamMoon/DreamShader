---
title: "DSH6xxx —— 函数与 HLSL 代码生成"
description: "DSH6xxx —— 函数与 HLSL 代码生成"
---

<a id="dsh6xxx-----functions-and-hlsl-codegen"></a>

# DSH6xxx —— 函数与 HLSL 代码生成

> 生成标记之间的区块由 `.skill/gen-diagnostics.ps1` 写入。
> 标记之下的所有内容均为手工编写，在重新生成后仍会保留。

<a id="dsh6200"></a>

## DSH6200

<!-- generated:begin DSH6200 -->
**严重级别** 错误

**消息**

```
'{0}' is a second material entry; '{1}' above it is already the entry, and one file makes one material.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1235`
<!-- generated:end DSH6200 -->

**原因。** 同一文件中有两个导出函数具有签名 `void (inout material)`。该签名*就是*材质入口的声明，因此包含两个此类签名的文件描述了两个材质。消息中会同时列出这两个函数。

**修复。** 保留其中一个；将另一个移到自己的文件中，或者去掉 `export` 使其成为一个接收材质的辅助函数——具有该签名的内部函数不是入口。

<a id="dsh6201"></a>

## DSH6201

<!-- generated:begin DSH6201 -->
**严重级别** 错误

**消息**

```
'{0}' is exported from a file whose entry is '{1}'; a file makes a material or it makes functions, not both. Move it to its own file, or drop 'export' to make it a helper.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1330`
<!-- generated:end DSH6201 -->

**原因。** 一个文件同时包含材质入口**和**导出函数、层或层混合。一个文件只能产出一种类型的产品。1.x 的 `.dsm` 是例外：它的 `Shader` 以及旁边的 `ShaderFunction` 和层区块都会被构建，正如 1.x 构建它们那样*(2.0.0 – 2.1.0 在此处同样拒绝它们)*——这也是 `dsc migrate` 无法将此类文件转换为单个 `.dss` 的原因。

**修复。** 将这些函数移到它们自己的文件中并 `#include` 该文件，或者去掉 `export` 使其成为内联到材质中的辅助函数。

<a id="dsh6202"></a>

## DSH6202

<!-- generated:begin DSH6202 -->
**严重级别** 错误

**消息**

```
'extern {0}' has nothing to bind to; add '/// @asset /Game/.../MF_Name' above it.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1163`
<!-- generated:end DSH6202 -->

**原因。** 一个 `extern` 原型上方没有 `/// @asset`，因此没有任何可供其绑定的对象。

**修复。** 添加 `/// @asset /Game/.../MF_Name` 并命名现有的材质函数。

<a id="dsh6203"></a>

## DSH6203

<!-- generated:begin DSH6203 -->
**严重级别** 错误

**消息**

```
'@layer' makes '{0}' a material layer asset, so it has to be 'export'.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1177`、`Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1205`
<!-- generated:end DSH6203 -->

**原因。** `@layer` 或 `@layerblend` 作用于一个并非 `export` 的函数。这两个指令都命名了文件产出的资产，而只有导出函数才会产出一个资产。

**修复。** 添加 `export`，或删除该指令。

<a id="dsh6204"></a>

## DSH6204

<!-- generated:begin DSH6204 -->
**严重级别** 错误

**消息**

```
A '@layer' function is written 'export void {0}(inout material m)'.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1187`
<!-- generated:end DSH6204 -->

**原因。** 一个 `@layer` 函数不具有签名 `void Name(inout material m)`。

**修复。** 按该签名原样编写。层接收其所应用的材质并就地修改它。

<a id="dsh6205"></a>

## DSH6205

<!-- generated:begin DSH6205 -->
**严重级别** 错误

**消息**

```
A '@layerblend' function is written 'export void {0}(material Base, material Top, ..., inout material Result)': at least one 'material' input and a final 'inout material'.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1215`
<!-- generated:end DSH6205 -->

**原因。** 一个 `@layerblend` 函数不具有 `void Name(material A, material B, …, inout material Result)` 的形状：至少一个 `material` 输入，其余每个参数都是输入，以及一个位于末尾的 `inout material`。

**修复。** 重新排列参数，使结果位于最后且为 `inout material`。

<a id="dsh6206"></a>

## DSH6206

<!-- generated:begin DSH6206 -->
**严重级别** 错误

**消息**

```
'{0}' is a builtin operation and cannot be redeclared; rename the function.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:840`
<!-- generated:end DSH6206 -->

**原因。** 一个函数使用了某个内置操作的名字进行声明（`dot`、`lerp`、`saturate`、……，以及该语言所拒绝的 GLSL 拼写）。此处没有重载，因此两者在每个调用点都会产生歧义。

**修复。** 重命名该函数。

<a id="dsh6208"></a>

## DSH6208

<!-- generated:begin DSH6208 -->
**严重级别** 错误

**消息**

```
'{0}' is a {1} asset, not a function this file may call.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3582`
<!-- generated:end DSH6208 -->

**原因。** 对材质入口、`@layer` 或 `@layerblend` 的调用。那些是资产，而非此文件可以调用的函数——层由其使用它的材质来应用。

**修复。** 将共享代码提取到一个辅助函数中，并从两处调用它。

<a id="dsh6209"></a>

## DSH6209

<!-- generated:begin DSH6209 -->
**严重级别** 错误

**消息**

```
'{0}' has no body; a prototype has to be 'extern' and carry '/// @asset'.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1264`
<!-- generated:end DSH6209 -->

**原因。** 一个函数没有主体且并非 `extern`。通常解析器会先捕获此问题（DSH3208）；这是针对以其他方式到达绑定器的语法树的语义兜底检查。

**修复。** 为它提供一个主体，或使用 `/// @asset` 将其声明为 `extern`。

<a id="dsh6210"></a>

## DSH6210

<!-- generated:begin DSH6210 -->
**严重级别** 错误

**消息**

```
'{0}' is '@custom', so '{1}' becomes an input pin of a Custom node, and a Custom node cannot take a material; pass the fields it needs instead.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1284`
<!-- generated:end DSH6210 -->

**原因。** 一个 `/// @custom` 函数将 `material` 作为输入接收（`material m` 或 `inout material m`）。`@custom` 主体会成为一个 Custom 节点，其输入是材质转换器为其指定类型的引脚，并且没有可供其使用的 MaterialAttributes 输入类型。

**修复。** 传入主体所需的字段——`float3 BaseColor, float Roughness`——并在调用点写回结果。`material` 的 **return** 和 `out material` 参数仍然合法：Custom 节点可以产出属性，即便它无法消费属性。

<a id="dsh6211"></a>

## DSH6211

<!-- generated:begin DSH6211 -->
**严重级别** 错误

**消息**

```
'{0}' is an 'out' parameter of '{1}' but the body never assigns it, so a caller would read a value nothing produced. Assign it before the function returns, or remove the parameter.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:392`
<!-- generated:end DSH6211 -->

**原因。** 一个函数声明了 `out` 参数，而其主体从未写入它——无论是整体、其某个 swizzle、元素或字段，还是将其传递给另一个调用的 `out` / `inout` 参数。调用方读取该输出会得到一个没有任何东西产出的值；在图中它是一个未连接的引脚。仅在某一分支上的写入就足以满足此检查：该检查是“从未”，而非“并非在每条路径上”。

**修复。** 在函数返回前为该参数赋值，或将其移除。具有不透明主体的函数（`/// @custom`、1.x `Function`）以及 `extern` 原型不会被检查；它们的主体不是 DreamShaderLang 语句。

<a id="dsh6220"></a>

## DSH6220

<!-- generated:begin DSH6220 -->
**严重级别** 错误

**消息**

```
'{0}' calls itself, and an inlined function has no stack to recurse on; rewrite it as a loop with a constant trip count, or as a '/// @custom' function.
```

**触发者** `Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:427`、`Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:436`
<!-- generated:end DSH6220 -->

**原因。** 一个内联函数直接或通过其他函数调用自身。内联没有栈：调用会被被调用者的主体替换，因此递归调用会无限展开。

**修复。** 将递归改写为具有常量迭代次数的循环（会被展开），或将其移入 `/// @custom` 函数，在那里适用 HLSL 自身的规则——不过 HLSL 也没有递归，所以循环通常是真正的答案。两个辅助函数之间的相互递归会在闭合该循环的那次调用处报告；在两个报告中，消息会同时列出这两个函数。

<a id="dsh6221"></a>

## DSH6221

<!-- generated:begin DSH6221 -->
**严重级别** 错误

**消息**

```
Resolving the defaults of '{0}' would go {1} calls deep, past the limit of {2}; pass explicit values or flatten the default call chain.
```

**触发者** `Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:282`、`Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:444`
<!-- generated:end DSH6221 -->

**原因。** 内联深度超过了 `MaxInlineDepth`（默认为 32）而没有发生递归。三十二个辅助函数依次调用下一个的链条是合法的，只是非常深；该限制的存在是为了让病态文件以一条消息失败，而不是耗尽内存。

**修复。** 展平调用链，或将中间某个函数转换为 `export` 的材质函数，它将成为自己的资产和单个 `FunctionCall` 节点，而不是更多内联节点。提高该限制是管线选项，而非源文件选项。

<a id="dsh6222"></a>

## DSH6222

<!-- generated:begin DSH6222 -->
**严重性** 错误

**消息**

```
'{0}' is an '{1}' parameter of {2}, so the argument has to be something that can be assigned to; this expression cannot.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:231`
<!-- generated:end DSH6222 -->

**原因。** 传递给 `out` 或 `inout` 参数的实参不是可以赋值的内容。该参数会在调用结束时写回调用方，因此实参必须是一个变量、字段、材质属性或它们的 swizzle，而不能是一个计算出的值。

**修复。** 将结果放入局部变量并传递它：`float3 Value; Compute(Value); Use(Value);`。
将字面量、调用结果或算术表达式传递给 `out` 参数无法工作。

<a id="dsh6223"></a>

## DSH6223

<!-- generated:begin DSH6223 -->
**严重性** 错误

**消息**

```
'{0}' is this file's material entry and is called by the engine, not by the shader.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:403`、`Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:453`
<!-- generated:end DSH6223 -->

**原因。** 要么是调用了此文件的材质入口——引擎调用的 `void (inout material)` 函数，而不是着色器——要么是调用了没有可内联主体的函数：一个既不是带 `/// @asset` 的 `extern`，也不是 `/// @custom` 的原型。

**修复。** 对于入口，应调用入口所调用的辅助函数，而不是入口本身。对于没有主体的函数，为它提供主体，将其标记为 `extern` 并将 `/// @asset` 指向它所代表的材质函数资产，或者将其标记为 `/// @custom` 并编写其 HLSL。

<a id="dsh6224"></a>

## DSH6224

<!-- generated:begin DSH6224 -->
**严重性** 错误

**消息**

```
The default value of '{0}' in '{1}' depends on itself; pass an explicit value for a parameter in the cycle.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:148`、`Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:166`、`Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:274`、`Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:322`
<!-- generated:end DSH6224 -->

**原因。** 省略参数的默认值依赖于自身（直接依赖、通过其他默认值依赖，或通过另一次省略实参的调用依赖），或者读取了没有值的输入。同一代码还会报告 Custom 函数的默认表达式写入了 `out` 参数：该输出没有输入引脚，图计算出的初始值无法通过它到达 HLSL 主体。

**修复。** 传入显式值以打破依赖循环，或为所引用的输入提供可用的默认值。对于 Custom 输出，通过单独的输入传入其初始值，并在 HLSL 主体中赋值该输出。普通辅助函数的默认值可以初始化 `out` 参数；这些值会被带入辅助函数主体并正常复制回来。

<a id="dsh6250"></a>

## DSH6250

<!-- generated:begin DSH6250 -->
**严重性** 错误

**消息**

```
'{0}' has no verbatim HLSL body, so it cannot become a custom node; only a '/// @custom' function can.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1148`、`Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:2054`、`Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:2066`、`Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:2077`
<!-- generated:end DSH6250 -->

**原因。** 对不存在的函数索引调用了 `BuildDreamShaderCustomNodeCode`，或者对绑定器未归类为 `EBoundFunctionKind::Custom` 的函数调用了它，或者——当通过调用到达某个辅助函数时——对一个 `@custom` 函数调用了它，而解析器未逐字捕获该函数的主体（`FFunctionDecl::bOpaqueBody` 为 false，因此 `RawBody` 为空）。

**修复。** 在一次编译中，这是内部错误，而不是创作错误：IR 构建器决定为非自定义函数的内容创建了 Custom 节点，或者解析器和绑定器对哪些主体是不透明的主体意见不一致。请附上源文件报告此问题。在直接调用入口点的工具中，请传入其 `///` 块带有 `@custom` 的函数的索引。

<a id="dsh6251"></a>

## DSH6251

<!-- generated:begin DSH6251 -->
**严重性** 错误

**消息**

```
'{0}' declares '{1}' as 'inout', which a custom node cannot carry; split it into an 'in' parameter and an 'out' parameter.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1189`
<!-- generated:end DSH6251 -->

**原因。** 一个 `@custom` 参数被声明为 `inout`。自定义节点将其 `out` 参数作为*附加输出*携带，而引擎已经将其声明为它所生成的函数的 `inout` 参数；同名输入引脚会第二次声明该名称，着色器将无法编译。不存在既能读取又能写入同一引脚的自定义节点形式。

**修复。** 将其拆分：通过一个 `in` 参数接收值，并通过单独的 `out` 参数返回结果。

```hlsl
/// @custom
void Adjust(float3 InColor, out float3 OutColor) { OutColor = InColor * 2; }
```

请注意，`inout` 在材质入口（`void M(inout material m)`）上仍然是正确的写法，材质入口不是自定义节点。

<a id="dsh6252"></a>

## DSH6252

<!-- generated:begin DSH6252 -->
**严重性** 错误

**消息**

```
'{0}' takes the material '{1}' as an input; a custom node cannot accept material attributes on a pin, so read the fields the body needs and pass them as floats.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1226`
<!-- generated:end DSH6252 -->

**原因。** 一个 `@custom` 函数将 `material`（材质属性）作为输入。引擎的转换器会为每个 Custom 节点输入引脚指定类型，但没有针对材质属性的情况——着色器编译以 `Bad type MaterialAttributes for <node> input <pin>` 结束。材质属性作为自定义节点的*输出*是可以的；它们不会在输入方向上被接受。

**修复。** 在图中读取所需的属性并将它们作为浮点数传递：

```hlsl
/// @custom
float3 Tint(float3 BaseColor, float Roughness) { ... }
```

被嵌入到另一个函数节点中的辅助 `@custom` 函数仍然可以接受 `FMaterialAttributes` 实参——这一限制针对的是引脚，而不是 HLSL——因此只会为正在为其构建节点的函数报告此问题。

<a id="dsh6253"></a>

## DSH6253

<!-- generated:begin DSH6253 -->
**严重性** 错误

**消息**

```
'{0}' uses Substrate on '{1}'; a custom node has no Substrate pins, so build that part of the material out of reflected Substrate nodes.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1200`、`Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1244`
<!-- generated:end DSH6253 -->

**原因。** 一个 `@custom` 签名提到了 `Substrate`。在任何引擎版本中，自定义节点都没有 Substrate 引脚；1.x 在 `Function` 上拒绝了同样的内容。

**修复。** 使用反射的 `Substrate.*` / `UE.*` 节点构建材质的 Substrate 部分，并让 `@custom` 函数只处理其周围的浮点数学运算。

<a id="dsh6254"></a>

## DSH6254

<!-- generated:begin DSH6254 -->
**严重性** 错误

**消息**

```
'{0}' returns a texture through '{1}'; a custom node output carries float1..4 or material attributes, never a texture object.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1212`、`Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1254`
<!-- generated:end DSH6254 -->

**原因。** 一个 `@custom` 函数返回纹理或采样器，或者通过 `out` 参数返回其中之一。自定义节点的输出是 `float1`..`float4` 或材质属性；没有携带纹理对象的引脚，HLSL 也不会从函数返回纹理对象。

**修复。** 在主体内对纹理进行采样并返回采样值。纹理可以作为*输入*；没有形式的是输出方向。

<a id="dsh6255"></a>

## DSH6255

<!-- generated:begin DSH6255 -->
**严重性** 错误

**消息**

```
'{0}' declares '{1}' twice in the HLSL it generates; a texture parameter also claims '{1}Sampler', which the engine declares alongside it.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1165`
<!-- generated:end DSH6255 -->

**原因。** 一个 `@custom` 函数的两个参数在为其生成的 HLSL 中最终具有相同的名称。常见原因是采样器配对：一个 `Texture2D Tex` 参数还会占用 `TexSampler`，因为那是引擎在它旁边声明的名称，因此第二个实际名为 `TexSampler` 的参数会发生冲突。名称比较区分大小写，与 HLSL 的比较方式一致。

**修复。** 重命名其中一个。`SamplerState` 参数永远不会被自动配对，因此当纹理和显式采样器相遇时，应重命名它。

<a id="dsh6256"></a>

## DSH6256

<!-- generated:begin DSH6256 -->
**严重性** 错误

**消息**

```
'{0}' returns void but its body uses 'return;'; a custom node always returns its first output, so give the function a return type or restructure the body.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1267`
<!-- generated:end DSH6256 -->

**原因。** 一个声明为 `void` 的 `@custom` 函数在其主体中使用了裸 `return;`。引擎围绕该主体生成的函数总是返回节点的第一个输出，因此没有值的 `return;` 无法编译——在任何大括号深度下都是如此，而不只是在顶层。

**修复。** 为函数指定返回类型并返回值，或者重构函数体，使提前退出变为一个 `if`。`void` `@custom` 是合法的——它的节点只输出 0，并通过 `out` 参数完成其工作——它只是不能 `return`。

<a id="dsh6257"></a>

## DSH6257

<!-- generated:begin DSH6257 -->
**严重性** 警告

**消息**

```
'{0}' declares a return type but its body never returns a value; the node's first output will be 0.
```

**由以下代码引发** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1280`
<!-- generated:end DSH6257 -->

**原因。** 一个 `@custom` 函数声明了返回类型，但其函数体中任何位置都没有 `return`（对于函数体是单个裸表达式的情况不会触发该警告，该表达式会自动获得其 `return`）。该节点的第一个输出将是常量 0。

**修复。** 返回签名所承诺的值，或者将函数声明为 `void`，并在函数体仅用于其副作用时使用 `out` 参数。

<a id="dsh6258"></a>

## DSH6258

<!-- generated:begin DSH6258 -->
**严重性** 错误

**消息**

```
'{0}' has an '#include' with an empty path; write the virtual shader path the header lives at, for example "/Engine/Private/Common.ush".
```

**由以下代码引发** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1009`
<!-- generated:end DSH6258 -->

**原因。** `@custom` 函数体顶部的 `#include` 有一个空路径（`#include ""`）。开头的 include 会被提升到节点的 include 列表中，而空条目会在着色器编译器中变成 `#include ""`。

**修复。** 写入该头文件所在的虚拟着色器路径——`/Engine/Private/Common.ush`、`/Plugin/DreamShader/DreamShaderBuiltins.ush`、`/Plugin/<Name>/...`。只有位于函数体最顶部的 `#include` 行（空白和注释可以在它们之前）才会被提升；跟在语句后面的那一行会留在函数体中，落入某个函数内部，仅对宏头文件有效。

<a id="dsh6259"></a>

## DSH6259

<!-- generated:begin DSH6259 -->
**严重性** 警告

**消息**

```
'{0}' differs from the function '{1}' only in case; HLSL is case-sensitive, so this call is left for the shader compiler. Did you mean '{1}'?
```

**由以下代码引发** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1376`
<!-- generated:end DSH6259 -->

**原因。** `@custom` 函数体中的一次调用所指定的名称与本次模块的某个函数仅大小写不同——`remap01(x)` 而该模块声明的是 `Remap01`。DreamShaderLang 1.x 以不区分大小写的方式匹配函数名；2.0 的函数体是 HLSL，而 HLSL 区分大小写，因此该调用被留给着色器编译器去解析。

**修复。** 按照函数声明的方式拼写名称。如果该调用确实是针对该函数体自身某个 `#include` 中的符号，而冲突只是巧合，则重命名两者之一——在此层面上无法将它们区分开。

<a id="dsh6260"></a>

## DSH6260

<!-- generated:begin DSH6260 -->
**严重性** 错误

**消息**

```
The '@custom' functions {0} call each other in a cycle; HLSL has no recursion, so their bodies cannot be embedded in a custom node.
```

**由以下代码引发** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1415`
<!-- generated:end DSH6260 -->

**原因。** 两个或多个 `@custom` 函数直接或通过其他函数相互调用。它们的函数体会作为一个结构体的成员被嵌入调用节点，而 HLSL 不支持递归，因此不存在可行的顺序。消息中会指出该循环。

**修复。** 打破循环：将共享部分提取到第三个不调用任何函数的 `@custom` 函数中，或者将递归移入函数体所 `#include` 的手写 `.ush` 头文件（着色器编译器仍会拒绝真正的递归，但可以手写有界展开的形式）。

<a id="dsh6261"></a>

## DSH6261

<!-- generated:begin DSH6261 -->
**严重性** 错误

**消息**

```
'{0}' is not a '@custom' function and cannot be called from the HLSL body of '{1}'; a custom node sees no graph values, so mark '{0}' '@custom' as well or move the call out of the body.
```

**由以下代码引发** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1361`
<!-- generated:end DSH6261 -->

**原因。** `@custom` 函数体调用了本模块中一个本身并非 `@custom` 的函数——一个辅助函数、一个被 `export` 的材质函数或一个 `extern`。这些都会成为图节点；自定义节点的函数体是着色器编译器在图构建完成后很久才会看到的 HLSL，它无法访问图的值。

**修复。** 将所调用的函数也标记为 `/// @custom`，以便其函数体可以一并嵌入；或者将该值作为自定义函数的参数传入，并在图中计算它：

```hlsl
/// @custom
float3 Shade(float3 Color, float Mask) { ... }   // Mask computed by the caller, in the graph
```

<a id="dsh6262"></a>

## DSH6262

<!-- generated:begin DSH6262 -->
**严重性** 错误

**消息**

```
'{0}' takes {1} argument(s) but this call passes {2}; a call that carries a texture cannot be matched up by position otherwise.
```

**由以下代码引发** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1670`
<!-- generated:end DSH6262 -->

**原因。** 对接受纹理的 `@custom` 函数的调用传入了错误数量的参数。配套采样器必须拼接在每个纹理参数旁边，这意味着参数必须按位置与形参对齐；数量错误时，就无法知道哪个参数是纹理。

**修复。** 每个形参恰好传入一个实参。形参上的默认值并不能让你在这里省略它——采样器配对需要每个位置。

<a id="dsh6263"></a>

## DSH6263

<!-- generated:begin DSH6263 -->
**严重性** 错误

**消息**

```
The texture argument for '{0}' of '{1}' has to be a plain texture name, because the sampler that goes with it is named after it.
```

**由以下代码引发** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1755`
<!-- generated:end DSH6263 -->

**原因。** 对 `@custom` 函数的调用中，纹理位置处的实参不是简单名称（`Pick(a, b)`、`bUseA ? A : B`、`Textures[i]`）。与纹理配套的采样器以该纹理命名——`Tex` 会带来 `TexSampler`——因此实参必须是名称，这样才有意义。1.x 通过将 `Sampler` 粘贴到实参文本上来构建采样器名称，这会把 `a ? b : c` 变成 `a ? b : cSampler`；2.0 则拒绝这样做，而不是生成它。

**修复。** 先将纹理赋值给一个名称，或者在图中选择纹理，并将所选纹理作为外层自定义函数的参数传入。

<a id="dsh6264"></a>

## DSH6264

<!-- generated:begin DSH6264 -->
**严重性** 警告

**消息**

```
'{0}' is 'selfcontained', so the '@custom' function '{1}' it calls is not embedded in its node; the call is left for the shader compiler to resolve out of this body's own includes.
```

**由以下代码引发** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1476`
<!-- generated:end DSH6264 -->

**原因。** `@custom selfcontained` 函数体调用了本模块中的另一个 `@custom` 函数。`selfcontained` 函数体会被完全按原样采用——模块中的任何内容都不会被嵌入其中——因此该调用被留给着色器编译器，而只有当该函数体自身的某个 `#include` 定义了该符号时，编译器才能找到它。

**修复。** 如果该调用本意是要被嵌入的（这是默认行为，闭包会为你拉入），则去掉 `selfcontained`。如果该符号确实来自被包含的头文件，则保留 `selfcontained`，并重命名本地 `@custom` 函数，使两者看起来不再是同一事物。

<a id="dsh6265"></a>

## DSH6265

<!-- generated:begin DSH6265 -->
**严重性** 错误

**消息**

```
'{0}' takes {1} argument(s) in the HLSL it becomes, but this call passes {2}.
```

**由以下代码引发** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1682`
<!-- generated:end DSH6265 -->

**原因。** `@custom` 函数体调用了另一个 `@custom` 函数，传入的参数数量是该函数所变成的 HLSL 所不接受的。HLSL 没有默认参数，而 `@custom` 函数没有重载，因此着色器编译器会拒绝该调用；这里改为在调用处拒绝。数量是针对该 HLSL 函数的：一个不返回任何内容的 1.x 函数返回其第一个 `out`（规则 L9），因此对它的一次调用会传入除该参数之外的所有参数——或者传入所有参数，这会被读作 `r = F(a, o)`。

**修复。** 为被调用者的每个形参传入一个实参，包括 `out` 参数。由 `dsc migrate` 编写的文件在这里失败，是迁移的过错；请说明这一点。

<a id="dsh6266"></a>

## DSH6266

<!-- generated:begin DSH6266 -->
**严重性** 错误

**消息**

```
'{0}' returns nothing in the HLSL it becomes, so this call has no value to read; pass a variable for each of its 'out' parameters and read those.
```

**由以下代码引发** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1698`
<!-- generated:end DSH6266 -->

**原因。** `@custom` 函数体读取了对 `@custom` 函数的调用值，而该函数不返回任何内容——
`x = F(a, o)`、`return F(a)`、`F(a) + 1`。其 HLSL 为 `void`，因此没有值。在 1.x 下，每个此类函数
都返回其第一个 `out`（规则 L9），而 1.x 函数体会读取该值；`.dss` 函数则按其声明方式生成。

**修复。** 为每个 `out` 参数传入一个变量，并在调用后读取它，或声明该函数返回
该值。`dsc migrate` 使 1.x 函数返回其第一个 `out`；`Namespace` 函数保留其 `out`，而
其文件中读取某个调用的函数体会调用一个返回该值的适配器。

<a id="dsh6300"></a>

## DSH6300

<!-- generated:begin DSH6300 -->
**严重性** 错误

**消息**

```
Expected a function name after '{0}', found '('.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2042`、`Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2064`、`Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2093`
<!-- generated:end DSH6300 -->

**原因。** `Function` 或 `GraphFunction`（及其可选修饰符和返回类型）后面跟着
`(`，而此处本应是函数名。

**修复。** 编写 `Function float3 MyFunction(in float3 value) { ... }`。

<a id="dsh6301"></a>

## DSH6301

<!-- generated:begin DSH6301 -->
**严重性** 错误

**消息**

```
Expected '[in|out] Type Name' in the parameter list of '{0}', found '{1}'.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2152`
<!-- generated:end DSH6301 -->

**原因。** 1.x 函数的某个参数不是 `[in|out] Type Name`。1.x 参数没有
默认值，也没有元数据；那些属于 ShaderFunction 的 `Inputs`。

**修复。** 将每个参数编写为 `in float3 color` 或 `out float result`。

<a id="dsh6302"></a>

## DSH6302

<!-- generated:begin DSH6302 -->
**严重性** 错误

**消息**

```
Expected 'in' or 'out' before the parameter '{0}' of '{1}', found '{2}'; a 1.x function has no 'inout'.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2165`
<!-- generated:end DSH6302 -->

**原因。** 1.x 函数的某个参数带有 `inout`，或在 `in` / `out` 应在的位置带有其他词。
1.x 函数有输入和结果，从没有两者兼具的参数。

**修复。** 将其拆分为一个 `in` 参数和一个 `out` 参数。`.dss` 函数可以使用 `inout`。

<a id="dsh6303"></a>

## DSH6303

<!-- generated:begin DSH6303 -->
**严重性** 错误

**消息**

```
Expected a parameter name other than '__return', which 1.x reserved, in '{0}'.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2178`
<!-- generated:end DSH6303 -->

**原因。** 某个参数名为 `__return`。1.x 会将函数的返回值降级为一个该名称的 `out`
参数，因此该名称已被占用。

**修复。** 重命名该参数。

<a id="dsh6304"></a>

## DSH6304

<!-- generated:begin DSH6304 -->
**严重性** 错误

**消息**

```
Expected either a return type or 'out' parameters on '{0}', found both.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2210`
<!-- generated:end DSH6304 -->

**原因。** 某个 1.x 函数既有返回类型又有 `out` 参数。1.x 只允许二者之一：
返回的值是 Custom 节点的输出，第一个 `out` 也是。

**修复。** 返回第一个结果并保留零个 `out` 参数，或者去掉返回类型并将每个
结果都变为 `out`。

<a id="dsh6305"></a>

## DSH6305

<!-- generated:begin DSH6305 -->
**严重性** 错误

**消息**

```
Expected '{0}' to return a value or to have at least one 'out' parameter, found neither.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2217`
<!-- generated:end DSH6305 -->

**原因。** 某个 1.x 函数不返回任何内容，且没有 `out` 参数，因此其 Custom 节点
将没有可供图表读取的值。

**修复。** 为其指定返回类型或一个 `out` 参数。

<a id="dsh6306"></a>

## DSH6306

<!-- generated:begin DSH6306 -->
**严重性** 警告

**消息**

```
'Inline' is the old spelling of 'SelfContained', and both read the same: the function's node embeds the functions it calls.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2052`
<!-- generated:end DSH6306 -->

**原因。** 使用了 `Function Inline`。在 1.x 中，`Inline` 是 `SelfContained` 的完全同义词：
函数的 Custom 节点会内嵌其函数体所调用的函数。2.0 会将它们内嵌到每个 Custom 节点中，因此
两种拼写读起来都等同于完全没有修饰符，函数会变成普通的 `@custom`。它
绝不会变成 `@custom selfcontained`，后者在 2.0 中含义相反——不内嵌任何内容（`DSH6264`）。
*（2.0.0 – 2.1.0 将两种拼写都读作 `@custom selfcontained`。）*

**修复。** 去掉该修饰符，或编写 `SelfContained`；两种方式都不会有任何变化。

<a id="dsh6307"></a>

## DSH6307

<!-- generated:begin DSH6307 -->
**严重性** 错误

**消息**

```
Expected a return type or a name after 'GraphFunction', found the modifier '{0}', which only a Function takes.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2033`
<!-- generated:end DSH6307 -->

**原因。** `GraphFunction` 后面跟着 `SelfContained` 或 `Inline`。这些修饰符属于
`Function`；GraphFunction 始终是每次调用一个 Custom 节点。

**修复。** 移除该修饰符。

<a id="dsh6308"></a>

## DSH6308

<!-- generated:begin DSH6308 -->
**严重性** 错误

**消息**

```
Expected a value after 'return' in '{0}', which returns '{1}', found a bare 'return;'.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2262`
<!-- generated:end DSH6308 -->

**原因。** 某个带返回类型的 1.x 函数包含一个裸 `return;`。1.x 会将 `return x;` 降级为
对其结果的赋值，而没有值的 `return` 没有什么可赋值。

**修复。** 在每条路径上都返回一个值。

<a id="dsh6309"></a>

## DSH6309

<!-- generated:begin DSH6309 -->
**严重性** 错误

**消息**

```
Expected a 'Name = "..."' attribute with a name on 'Namespace', found none.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2426`
<!-- generated:end DSH6309 -->

**原因。** `Namespace` 没有 `Name = "..."` 属性，或者该名称不是标识符。该名称
会成为块中每个函数的前缀（`N::F`，展平为 `N_F`）。

**修复。** 编写 `Namespace(Name = "BL") { ... }`。

<a id="dsh6310"></a>

## DSH6310

<!-- generated:begin DSH6310 -->
**严重性** 错误

**消息**

```
Expected only Function and GraphFunction blocks inside the namespace '{0}', found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2492`
<!-- generated:end DSH6310 -->

**原因。** 某个 `Namespace` 块中包含 `Function` 和 `GraphFunction` 块以外的内容。
命名空间只对 HLSL 函数进行分组，别无其他：没有资产、没有 VirtualFunction、没有嵌套
命名空间。

**修复。** 将其他块移出该命名空间。

<a id="dsh6311"></a>

## DSH6311

<!-- generated:begin DSH6311 -->
**严重性** 错误

**消息**

```
Expected a 'Name = "..."' attribute with a name on 'VirtualFunction', found none.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2557`
<!-- generated:end DSH6311 -->

**原因。** `VirtualFunction` 没有 `Name = "..."` 属性。该名称是 Graph 代码所调用的名称。

**修复。** 添加该名称。

<a id="dsh6312"></a>

## DSH6312

<!-- generated:begin DSH6312 -->
**严重性** 错误

**消息**

```
Expected an 'Asset = Path(...)' option on the VirtualFunction '{0}', found none.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2696`
<!-- generated:end DSH6312 -->

**原因。** 某个 VirtualFunction 在其 `Options` 中没有 `Asset = Path(...)`。该
块的全部意义就在于说明该名称代表哪个现有材质函数。

**修复。** 添加 `Options = { Asset = Path(Game, "Functions/MF_Example"); }`。

<a id="dsh6313"></a>

## DSH6313

<!-- generated:begin DSH6313 -->
**严重性** 错误

**消息**

```
Expected at least one output on the VirtualFunction '{0}', found none.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2703`
<!-- generated:end DSH6313 -->

**原因。** 某个 VirtualFunction 未声明任何输出。对它进行调用将成为一个无人能读取的节点。

**修复。** 在 `Outputs` 下按资产自身的顺序声明该资产的输出。

<a id="dsh6314"></a>

## DSH6314

<!-- generated:begin DSH6314 -->
**严重性** 错误

**消息**

```
Expected a ')' to close the 'UE.' call in the body of '{0}', found the end of the body.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2316`
<!-- generated:end DSH6314 -->

**原因。** 逐字函数体中的 `UE.` 调用——即 1.x GraphFunction 或 `/// @custom` 函数——
打开了一个函数体从未闭合的括号，因此将被提升为节点输入的那个调用
没有结尾。

**修复。** 闭合该调用。如果该文本并非要作为节点调用，那么它不能以 `UE.`
后跟名称和 `(` 开头。

<a id="dsh6316"></a>

## DSH6316

<!-- generated:begin DSH6316 -->
**严重性** 警告

**消息**

```
A 'Substrate.' call in the body of '{0}' is not lifted into a node, because no custom node input carries a Substrate value; it reaches the shader compiler as text.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2323`
<!-- generated:end DSH6316 -->

**原因.** 逐字体调用了 `Substrate.<Node>(...)`. `UE.` 在此类体中的调用会被提升为 Custom 节点的输入；Substrate 值无法通过 Custom 节点输入传递，因此该调用被留在原处，着色器编译器将无法识别该名称。

**修复.** 在图体中构建 Substrate 节点，并仅在 HLSL 中保留数字。

<a id="dsh6319"></a>

## DSH6319

<!-- generated:begin DSH6319 -->
**严重性** 错误

**消息**

```
Expected '(' to open the parameter list of '{0}', found {1}.
```

**由以下抛出** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2102`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2226`
<!-- generated:end DSH6319 -->

**原因.** 1.x 函数的名称后面没有 `(`，或其参数列表后面没有打开其体的 `{`。

**修复.** 编写参数列表（如有需要可为空）和体：`Function float Pi() { return
3.14159; }`。

<a id="dsh6325"></a>

## DSH6325

<!-- generated:begin DSH6325 -->
**严重性** 错误

**消息**

```
'{0}' lifts a 'UE.' call out of its body that reads '{1}', which 1.x took from the caller's scope and 2.0 does not; pass '{1}' to '{0}' as a parameter.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:780`
<!-- generated:end DSH6325 -->

**原因.** 规则 L8. 从 GraphFunction 体中提升出来的 `UE.` 调用会读取调用方的变量。1.x 在调用方的作用域中求值被提升的调用，因此这种写法由于命名巧合而能正常工作；被提升的调用现在绑定在函数自身的作用域中——即其参数和文件的全局变量——从而使函数在任何被调用的地方都具有相同含义。

**修复.** 为该值添加一个参数，并在调用处传递它。

<a id="dsh6326"></a>

## DSH6326

<!-- generated:begin DSH6326 -->
**严重性** 错误

**消息**

```
'{0}' lifts a 'UE.' call out of its body that reads '{1}', which is neither a parameter of '{0}' nor declared at file scope.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:791`
<!-- generated:end DSH6326 -->

**原因.** 从逐字体中提升出来的 `UE.` 调用读取了一个既不是函数参数、也未在文件作用域中声明的名称。被提升的调用在图中的 HLSL 之外求值，因此无法看到体声明的变量。

**修复.** 将该值作为参数传入，或在图中计算该值并将结果交给该函数。

<a id="dsh6327"></a>

## DSH6327

<!-- generated:begin DSH6327 -->
**严重性** 错误

**消息**

```
'{0}' takes the 'UE.' calls lifted out of its body as inputs of its own custom node, so the HLSL body of '{1}' cannot call it; call it from a graph body instead.
```

**由以下抛出** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1345`
<!-- generated:end DSH6327 -->

**原因.** 一个 HLSL 体调用了这样一个函数：其自身的体中有被提升出来的 `UE.` 调用。这些调用是该函数自己的 Custom 节点的输入；如果作为辅助函数嵌入另一个节点的代码中，就没有人来连接它们。

**修复.** 从图体中调用该函数并传入其结果，或将其中的 `UE.` 调用移出为参数。

<a id="dsh6328"></a>

## DSH6328

<!-- generated:begin DSH6328 -->
**严重性** 错误

**消息**

```
'{0}' lifts the call behind its input '{1}' out of a place its body does not have, so its custom node's code cannot be built.
```

**由以下抛出** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1068`
<!-- generated:end DSH6328 -->

**原因.** 被提升调用的记录位置位于其所属的体文本之外。前端记录每个 `UE.` 调用在体中的位置，而代码构建器将该范围替换为输入的名称；只有在两者之间声明被改动时它们才会不一致。内部错误。

**修复.** 连同源码一起报告。

<a id="dsh6329"></a>

## DSH6329

<!-- generated:begin DSH6329 -->
**严重性** 错误

**消息**

```
'{0}' carries 'UE.' calls lifted out of its body, and only a '@custom' function with a verbatim body lifts calls into its node's inputs.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:695`, `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:727`
<!-- generated:end DSH6329 -->

**原因.** 一个不是带逐字体的 `/// @custom` 函数的函数携带了被提升的调用，或者一个被提升的调用产生了没有 Custom 输入承载的值（材质、Substrate 值）。第一种情况是内部不一致；第二种情况意味着体调用的节点其结果不是数字或纹理。

**修复.** 对于第二种情况，将该节点构建在图体中，而不是构建在 HLSL 内部。

<a id="dsh6330"></a>

## DSH6330

<!-- generated:begin DSH6330 -->
**严重性** 错误

**消息**

```
'{0}' reaches itself through the 'UE.' calls lifted out of its body, and each call makes a new custom node, so the graph would never end.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1512`
<!-- generated:end DSH6330 -->

**原因.** 一个函数通过从体中提升出来的调用到达自身：被提升的 `UE.` 调用的参数再次调用了该函数。对此类函数的每次调用都是一个带有自己提升输入的新 Custom 节点，因此展开将不会结束。

**修复.** 打破循环：在图中计算内部值并将其传入。

<a id="dsh6331"></a>

## DSH6331

<!-- generated:begin DSH6331 -->
**严重性** 警告

**消息**

```
'{0}' is not an attribute of a '{1}' block, and was ignored; the header takes Generate, Library and Description.
```

**由以下抛出** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1933`
<!-- generated:end DSH6331 -->

**原因.** 1.x 的 `Function` 或 `GraphFunction` 头 *(2.1.2 之后)* 命名了一个并非 `Generate`、`Library` 和 `Description` 的键——通常是拼写错误。其他 1.x 块会像 1.x 那样静默忽略未知键；此头是新增的，因此会提示拼写错误。参见 [Packages ▸ Asset headers](../../tools/packages.md#asset-headers)。

**修复.** 将该键拼写为三者之一，或删除它。

<a id="dsh6332"></a>

## DSH6332

<!-- generated:begin DSH6332 -->
**严重性** 错误

**消息**

```
'{0}' holds a placeholder other than '`{Name`}', the only one a Generate template has.
```

**由以下抛出** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1962`
<!-- generated:end DSH6332 -->

**原因.** 一个 `Generate` 值——1.x 函数自身的，或 `Namespace` 应用于成员的模板——在 `{Name}` 被替换后仍保留着一个 `{...}`。`{Name}` 是模板唯一拥有的占位符。

**修复.** 准确书写 `{Name}`（它区分大小写），或写出名称。
