---
title: "DSH4xxx --- 图语句与表达式"
description: "DSH4xxx --- 图语句与表达式"
---

<a id="dsh4xxx-----graph-statements-and-expressions"></a>

# DSH4xxx --- 图语句与表达式

> 生成的标记之间的内容由 `.skill/gen-diagnostics.ps1` 写入。
> 标记下方的所有内容均为手工编写，并在重新生成后保留。

<a id="dsh4200"></a>

## DSH4200

<!-- generated:begin DSH4200 -->
**严重级别** 错误

**消息**

```
'{0}' is not declared; did you mean '{1}'? Names are case-sensitive.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:757`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:768`
<!-- generated:end DSH4200 -->

**原因。** 一个标识符并未命名此处可见的局部变量、参数或文件作用域声明。在 2.0 中名称是**区分大小写**的（1.x 不区分），因此 `intensity` 和 `Intensity` 是两个不同的名称；当唯一的区别只是大小写时，消息会说明这一点，并给出它找到的声明名称。

**修复。** 更正拼写，或在使用该名称的行之前声明它。来自头文件的名称只有在 `#include` 引入该头文件之后才存在：写在文件底部的 `#include` 不会为其上方的代码声明任何内容。

<a id="dsh4201"></a>

## DSH4201

<!-- generated:begin DSH4201 -->
**严重级别** 错误

**消息**

```
'{0}' is not a type; did you mean '{1}'? Type names are case-sensitive.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:560`、`Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:571`、`Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:585`、`Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:707`、`Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:980`、`Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:544`
<!-- generated:end DSH4201 -->

**原因。** 一个类型拼写既不是内置类型（`float3`、`Texture2D`、`material`、`Substrate`、……），也不是在此行**之前**声明的 `struct`。对于需要值类型的 `void`——字段、参数或变量——也会引发此错误。

**修复。** 检查拼写及其大小写；将该 `struct` 移到其首次使用之前；对于来自头文件的类型，检查该头文件是否已在此行之上包含。

<a id="dsh4202"></a>

## DSH4202

<!-- generated:begin DSH4202 -->
**严重级别** 错误

**消息**

```
A string has no value in an expression; it is only ever the value of a reflected property, as in 'UE.Expression(Class = "...")'.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:625`
<!-- generated:end DSH4202 -->

**原因。** 一个带引号的字符串进入了表达式。该语言没有字符串值：字符串只能是反射字面量属性的值，如 `UE.Expression(Class = "…")` 或 `Desc = "…"`。

**修复。** 移除该字符串，或将它移到它本应作为的属性参数中。

<a id="dsh4203"></a>

## DSH4203

<!-- generated:begin DSH4203 -->
**严重级别** 错误

**消息**

```
'{0}' is a namespace, not a value; write '{0}.SomeNode(...)'.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:717`
<!-- generated:end DSH4203 -->

**原因。** `UE` 或 `Substrate` 被当作变量使用。它们是命名空间；跟在它们后面的只能是节点名称和调用。

**修复。** 应写 `UE.TexCoord(0)` 而非 `UE`。如果本意是使用该名称的变量，请声明它——已声明的 `UE` 优先于命名空间，此后失效的则是那个反射调用。

<a id="dsh4204"></a>

## DSH4204

<!-- generated:begin DSH4204 -->
**严重级别** 错误

**消息**

```
'{0}' is a type, not a value; write '{0}(...)' to construct one.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:512`
<!-- generated:end DSH4204 -->

**原因。** 在需要值的地方出现了类型名称：`float3;`、`x = float3`、`f(float3)`。

**修复。** 构造该值——`float3(0.0)`——或编写强制转换，`(float3)x`。

<a id="dsh4205"></a>

## DSH4205

<!-- generated:begin DSH4205 -->
**严重级别** 错误

**消息**

```
'{0}' has no field called '{1}'.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1125`
<!-- generated:end DSH4205 -->

**原因。** 向一个 `struct` 值请求了它没有的字段。消息会指出该结构体，并在存在仅大小写不同的字段时建议该字段。

**修复。** 更正字段名，或将该字段添加到结构体。字段区分大小写。

<a id="dsh4206"></a>

## DSH4206

<!-- generated:begin DSH4206 -->
**严重级别** 错误

**消息**

```
'{0}' on a texture is a call: write 'Tex.{0}(UV)'.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1264`
<!-- generated:end DSH4206 -->

**原因。** `Tex.Sample`（或 `Tex.SampleLevel`）出现时没有其参数列表。纹理的方法都是调用；方法本身没有可取的值。

**修复。** 应写 `Tex.Sample(UV)`、`Tex.Sample(TexSampler, UV)` 或 `Tex.SampleLevel(UV, Level)`。

<a id="dsh4207"></a>

## DSH4207

<!-- generated:begin DSH4207 -->
**严重级别** 错误

**消息**

```
A value of type {0} has no member '{1}'.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1274`
<!-- generated:end DSH4207 -->

**原因。** 在类型没有该类成员的某个值上编写了成员访问：对纹理的 swizzle、对数字的字段、对并非 `material` 的对象的属性。

**修复。** 检查该表达式实际具有的类型——消息会指出它。注意 `.rgb` 仅对至少三分量的数值有效，而 `m.BaseColor` 仅对 `material` 有效。

<a id="dsh4208"></a>

## DSH4208

<!-- generated:begin DSH4208 -->
**严重级别** 错误

**消息**

```
A value of type {0} has no method '{1}'.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2985`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3103`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3118`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4909`
<!-- generated:end DSH4208 -->

**原因。** 对并非可调用的对象编写了调用：不是函数的名称、内置函数、`struct` 或后接节点的 `UE.`/`Substrate.`；或纹理没有的方法。与某个已声明名称仅大小写不同的情况会在消息中指出。

**修复。** 更正拼写（名称区分大小写）、在调用之前声明该函数，或通过 `UE.Expression(Class = "…")` 访问引擎节点。

<a id="dsh4210"></a>

## DSH4210

<!-- generated:begin DSH4210 -->
**严重级别** 错误

**消息**

```
'{0}' is already declared in this file; one name declares one thing.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:482`、`Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:492`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:861`
<!-- generated:end DSH4210 -->

**原因。** 同一个名称被声明了两次。结构体、文件作用域变量和函数共用一个命名空间，因为结构体的构造函数和函数调用的写法相同。当两个声明位于不同文件时，消息会指出两者——常见原因是某个头文件声明了该文件也声明的名称，或两个头文件都声明了它。

在 `.dsp` 中，对于名称仅大小写不同的两个 `uniform`（`The parameter 'radius' differs from 'Radius', declared on line 3, in case only; ...`）也会引发此错误：管线资产将参数名称保存为 Unreal 名称，其比较时忽略大小写，因此在那里它们会是同一个参数。`static const` 会被折叠消除，永远不会到达资产，因此不会按这种方式比较。

**修复。** 重命名其中之一。没有重载：即使参数列表不同，同名的两个函数也会产生此错误。

<a id="dsh4211"></a>

## DSH4211

<!-- generated:begin DSH4211 -->
**严重级别** 错误

**消息**

```
Including '{0}' from '{1}' closes a cycle; a header may not include itself, directly or through another header.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:445`
<!-- generated:end DSH4211 -->

**原因。** 沿着 `#include` / `import` 追踪会回到一个已在读取的文件。消息会指出闭合该循环的文件以及包含它的文件。

**修复。** 打破该循环。两个头文件都需要的共享类型或常量应放在它们都包含的第三个头文件中；DreamShaderLang 没有使循环无害的包含保护。

<a id="dsh4212"></a>

## DSH4212

<!-- generated:begin DSH4212 -->
**严重级别** 错误

**消息**

```
'{0}' cannot be read: this front end was started without an include resolver, so nothing a header declares is visible.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:409`
<!-- generated:end DSH4212 -->

**原因。** 该文件带有 `#include` / `import`，而此前端在启动时没有包含解析器，因此头文件声明的任何内容都不可见。在一次编译内部不会发生这种情况；这说明某个工具调用 `BindDreamShaderLang` 时 `FBindOptions::IncludeResolver` 为空。

**修复。** 在工具中提供该解析器。如果本意是单独绑定一个文件，请预期该头文件本应声明的每个名称也同样会是 `DSH4200`。

<a id="dsh4213"></a>

## DSH4213

<!-- generated:begin DSH4213 -->
**严重级别** 错误

**消息**

```
'{0}' is declared twice in struct '{1}'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:688`
<!-- generated:end DSH4213 -->

**原因。** 一个 `struct` 声明了两个同名字段。

**修复。** 重命名其中一个。字段名区分大小写，因此 `Colour` 和 `colour` 不会触发此错误。

<a id="dsh4214"></a>

## DSH4214

<!-- generated:begin DSH4214 -->
**严重性** 错误

**消息**

```
'{0}' is declared twice in the parameter list of '{1}'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:957`
<!-- generated:end DSH4214 -->

**原因。** 参数列表中将同一个名字声明了两次。

**修复。** 重命名其中一个。

<a id="dsh4215"></a>

## DSH4215

<!-- generated:begin DSH4215 -->
**严重性** 错误

**消息**

```
The '{0}' argument of '{1}' is given twice.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3461`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3791`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4458`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4537`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4575`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:697`
<!-- generated:end DSH4215 -->

**原因。** 调用中同一个参数槽被填充了两次：同一个参数被命名了两次；某个命名参数重复了位置参数已经填充的内容；或者某个引脚及其字面量副本同时被提供。

**修复。** 删除重复项。位置参数按顺序填充参数，因此第一个命名了这些参数之一的命名参数就是重复项。

<a id="dsh4216"></a>

## DSH4216

<!-- generated:begin DSH4216 -->
**严重性** 错误

**消息**

```
'{0}' takes its arguments in order and has no argument called '{1}'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3426`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3769`
<!-- generated:end DSH4216 -->

**原因。** 命名参数在被调方中找不到任何对应项：该函数没有此名称的参数，或该内建项没有此名称的引脚。消息会列出内建项的参数名，并会为函数的参数建议一个仅大小写不同的匹配项。

**修复。** 更正名称，或以位置方式传递该参数。

<a id="dsh4217"></a>

## DSH4217

<!-- generated:begin DSH4217 -->
**严重性** 错误

**消息**

```
'{0}' is missing its '{1}' argument.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3489`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3843`
<!-- generated:end DSH4217 -->

**原因。** 调用使某个参数既没有实参也没有默认值，或某个内建项缺少其中一个实参。

**修复。** 传入该实参，或在参数的声明中为其提供默认值——默认值会使导出函数的输入变为可选。

<a id="dsh4218"></a>

## DSH4218

<!-- generated:begin DSH4218 -->
**严重性** 错误

**消息**

```
'{0}' writes back {1}, and this variable is {2}; an out argument has to match exactly.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3904`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3949`
<!-- generated:end DSH4218 -->

**原因。** 传递给 `out` / `inout` 参数的实参是不同类型的变量。与输入不同，`out` 实参会被写回，而转换在返回途中无处安放。

**修复。** 使用与参数完全相同的类型声明该变量，然后在调用之后进行转换。

<a id="dsh4220"></a>

## DSH4220

<!-- generated:begin DSH4220 -->
**严重性** 错误

**消息**

```
'{0}' is already declared in this block.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:149`、`Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:159`、`Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:169`
<!-- generated:end DSH4220 -->

**原因。** 局部变量在同一代码块中重新声明了某个名称，或隐藏了外层代码块或参数中的某个名称。HLSL 允许这种隐藏；本语言不允许，因为在图中这两个变量会成为两个同名但互不相关的节点，此后每个下游节点描述都会搞错自己引用的是哪一个。

**修复。** 为其中一个另取名称。**同级**代码块中两个同名变量没有问题——它们彼此不可见，且各自拥有自己的槽位。

<a id="dsh4221"></a>

## DSH4221

<!-- generated:begin DSH4221 -->
**严重性** 错误

**消息**

```
{0}() has no components; write the value, as in 'float3(0.0)'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3176`
<!-- generated:end DSH4221 -->

**原因。** 构造函数在编写时没有提供任何实参：`float3()`。

**修复。** 写入具体值：`float3(0.0)` 会广播，`float3(x, y, z)` 会一次构建一个分量。

<a id="dsh4222"></a>

## DSH4222

<!-- generated:begin DSH4222 -->
**严重性** 错误

**消息**

```
'{0}' has {1} fields and this list has {2}.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2704`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2809`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2842`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3242`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3314`
<!-- generated:end DSH4222 -->

**原因。** 各分量对不上：构造函数的实参分量总数与目标不符、初始化列表长度错误，或构造结构体时字段数量错误。

**修复。** 数一数分量。`float4(v3, w)` 是 3 + 1；`float3(v4)` 是把 4 放进 3，会被拒绝——请写成 `v4.xyz`。唯一的例外是单个标量，它可以广播到任意宽度。

<a id="dsh4223"></a>

## DSH4223

<!-- generated:begin DSH4223 -->
**严重性** 错误

**消息**

```
{0} expects {1}, and this is {2}.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1917`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2603`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2615`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2630`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3155`
<!-- generated:end DSH4223 -->

**原因。** 图无法承载的强制转换：转换的目标或来源不是数字或布尔类型，或该转换会丢弃分量（`(float3)v4`）。

**修复。** 对于收窄转换，请写出表明意图分量的 swizzle——`.xyz`。对于纹理、采样器、`material` 或 `Substrate` 值，根本不存在强制转换。

<a id="dsh4224"></a>

## DSH4224

<!-- generated:begin DSH4224 -->
**严重性** 错误

**消息**

```
'{0}' takes at most {1} arguments.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3447`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3507`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3717`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4953`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:5020`
<!-- generated:end DSH4224 -->

**原因。** 调用的实参数量多于或少于被调方所需的参数数量。对于纹理采样，消息会给出确切的写法，因为采样器参数是可选的，参数数量取决于是否写出了它。

**修复。** 与声明保持一致。有默认值的参数可以省略；没有默认值的则不能。

<a id="dsh4225"></a>

## DSH4225

<!-- generated:begin DSH4225 -->
**严重性** 错误

**消息**

```
A constructor takes its components in order; named arguments belong on 'UE.' nodes and on function calls.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3167`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4921`
<!-- generated:end DSH4225 -->

**原因。** 在只接受位置参数的地方使用了命名参数：构造函数、纹理采样。

**修复。** 去掉名称。命名参数适用于 `UE.`/`Substrate.` 节点（此时它们匹配某个引脚或属性）以及已声明函数的调用（此时它们匹配某个参数）。

<a id="dsh4226"></a>

## DSH4226

<!-- generated:begin DSH4226 -->
**严重性** 错误

**消息**

```
{0} expects {1}, and this is {2}.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1951`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1999`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2542`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3210`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3222`
<!-- generated:end DSH4226 -->

**原因。** 某个操作数与其使用位置不匹配。两种常见情形是宽度不匹配（`float3 * float2`——向量绝不会静默加宽）以及类型根本不是数字（在算术中出现纹理、`material`、`struct`）。

**修复。** 用 swizzle 或构造函数使宽度一致。**标量**可以广播到任意宽度，因此 `v3 * 2.0` 没问题，而 `v3 * v2` 则不行。

<a id="dsh4227"></a>

## DSH4227

<!-- generated:begin DSH4227 -->
**严重性** 错误

**消息**

```
'{0}' is not an operation this language spells as a call.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2103`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2219`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2262`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2299`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2458`
<!-- generated:end DSH4227 -->

**原因。** 某个运算符没有对应的图形式：`&`、`|`、`^`、`<<`、`>>`、`~` 及其赋值形式。图承载的是浮点数，而不是位模式。

**修复。** 用算术来表达它，或将代码移入 `/// @custom` 函数，其函数体是着色器编译器能完整看到的 HLSL。

<a id="dsh4228"></a>

## DSH4228

<!-- generated:begin DSH4228 -->
**严重性** 错误

**消息**

```
{0} expects {1}, and this is {2}.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1906`
<!-- generated:end DSH4228 -->

**原因。** 某个值无法放入其所存储的位置：``=`` 的右侧、初始化器、参数默认值或 ``return``。消息中会给出这两种类型。

**修复。** 同 `DSH4226` —— 用 swizzle 或构造函数调整宽度。将 ``int`` 用在期望 ``float`` 的位置会静默转换，绝不会产生此错误。

<a id="dsh4229"></a>

## DSH4229

<!-- generated:begin DSH4229 -->
**严重级别** 错误

**消息**

```
A 'uniform' is an input and a 'static const' is a constant; neither can be assigned to. Copy it into a local first.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2392`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2433`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2441`
<!-- generated:end DSH4229 -->

**原因。** 赋值语句的左侧不可赋值。共有三种消息：它是 ``uniform``（输入）或 ``static const``（常量）；它是正在构建的 Substrate 值成员的某一个分量（``S.DiffuseAlbedo.r = 1;``）—— 这样的成员是一个引脚，而引脚是整体连接的；或者它根本不是变量、结构体字段、材质引脚或其中之一的 swizzle（如调用、字面量、算术结果）。

**修复。** 先将 uniform 复制到局部变量再修改。对于 Substrate 值，先构建向量，再赋值给整个成员。否则请赋值给一个有名称的变量。

<a id="dsh4230"></a>

## DSH4230

<!-- generated:begin DSH4230 -->
**严重级别** 错误

**消息**

```
'.{0}' is not a swizzle; a swizzle is one to four of 'xyzw' or 'rgba'.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1291`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1310`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1326`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1346`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1713`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2409`
<!-- generated:end DSH4230 -->

**原因。** 以下五种情况之一：

- 一个不是 ``xyzw`` / ``rgba`` 的字母，
- 分量数超过四个，
- 分量超出了值的末尾（对 ``float3`` 使用 ``.w``，对 ``float2`` 使用 ``v[3]``），
- 同一个掩码中混用了两个族系（``.xg``），
- **赋值目标中**出现重复分量（``v.xx = f``）。

读取重复分量是可以的：``v.xxx`` 是 HLSL 的复制，绑定方式与其他掩码相同。写入则不行 —— 两次写入落在同一个分量上，且它们之间没有顺序，因此结果将取决于图的构建方式。

**修复。** 只使用一个族系并保持在范围内。对于赋值，请将每个分量写在单独的一行上。

<a id="dsh4231"></a>

## DSH4231

<!-- generated:begin DSH4231 -->
**严重级别** 错误

**消息**

```
'{0}.{1}' is an output node, not a value: write it as a statement on a line of its own.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4802`
<!-- generated:end DSH4231 -->

**原因。** 一个没有值的节点被当作值使用。自定义输出类（``ClearCoatBottomNormal``、``ThinTranslucentMaterialOutput``、体积高级输出等……）会写入材质，而不是产生可供读取的内容，没有输出引脚的类也属于同一情况。

**修复。** 将该调用作为语句写在独立的一行上。

<a id="dsh4233"></a>

## DSH4233

<!-- generated:begin DSH4233 -->
**严重级别** 错误

**消息**

```
A component index must be a compile-time constant; write a swizzle such as '.z', or select with 'lerp'.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1704`
<!-- generated:end DSH4233 -->

**原因。** ``v[i]`` 使用了绑定器无法求值的索引。分量索引会变成 swizzle，而 swizzle 在图构建时即已固定。

**修复。** 直接写出 swizzle（``v.z``），或使用 ``lerp`` / ``?:`` 在分量之间进行选择。

<a id="dsh4234"></a>

## DSH4234

<!-- generated:begin DSH4234 -->
**严重级别** 错误

**消息**

```
A value of type {0} cannot be indexed.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1740`
<!-- generated:end DSH4234 -->

**原因。** ``[...]`` 被用在了既不是数组、也不是向量或矩阵的值上。

**修复。** 检查类型 —— 消息中已给出。

<a id="dsh4235"></a>

## DSH4235

<!-- generated:begin DSH4235 -->
**严重级别** 错误

**消息**

```
'++' and '--' write back into what they read, so they need a variable.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2233`
<!-- generated:end DSH4235 -->

**原因。** ``++`` 或 ``--`` 被应用于不是变量的内容。它们会读取并写回，因此需要有可写入的位置。

**修复。** 将它们应用于局部变量、``out``/``inout`` 参数、结构体字段或材质引脚。

<a id="dsh4236"></a>

## DSH4236

<!-- generated:begin DSH4236 -->
**严重级别** 错误

**消息**

```
An 'in' parameter is a function input pin and cannot be written to; declare it 'out' or 'inout', or copy it into a local.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2400`
<!-- generated:end DSH4236 -->

**原因。** 对一个 ``in`` 参数进行了赋值。在图中，输入参数是函数输入引脚，而引脚不能被写入。

**修复。** 如果调用方应看到该更改，请将其声明为 ``out`` 或 ``inout``；否则将其复制到局部变量中再修改该局部变量。

<a id="dsh4237"></a>

## DSH4237

<!-- generated:begin DSH4237 -->
**严重级别** 错误

**消息**

```
An initializer list only has a meaning against a declared type; it cannot stand on its own.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2681`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2748`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2832`
<!-- generated:end DSH4237 -->

**原因。** 初始化列表出现在没有任何内容指明其应成为什么的场合 —— 或者目标无法作为列表写入（如纹理、``material``），或者用不是列表的内容初始化了数组。

**修复。** 为它指定一个声明的类型：``float4 v = { 1, 0, 0, 1 };``。数组只能用 ``= { … }`` 初始化，不能用其他内容。

<a id="dsh4238"></a>

## DSH4238

<!-- generated:begin DSH4238 -->
**严重级别** 错误

**消息**

```
'{0}' returns nothing, so its call has no value; its results come back through its out parameters.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3978`
<!-- generated:end DSH4238 -->

**原因。** 对返回 ``void`` 的函数的调用被当作值使用。

**修复。** 将其作为语句调用。带有多个结果的函数通过 ``out`` 参数返回结果，调用方将这些参数声明为变量，并在调用后读取。

<a id="dsh4239"></a>

## DSH4239

<!-- generated:begin DSH4239 -->
**严重级别** 错误

**消息**

```
'{0}' is an out parameter of '{1}', so its argument has to be a variable.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3875`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3931`
<!-- generated:end DSH4239 -->

**原因。** 传给 ``out`` / ``inout`` 参数的参数不是变量，因此没有可写入结果的位置。

**修复。** 声明一个该参数类型的局部变量并将其传入。

<a id="dsh4240"></a>

## DSH4240

<!-- generated:begin DSH4240 -->
**严重级别** 错误

**消息**

```
'#pragma endregion' closes a box that was never opened.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:869`、`Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:888`、`Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:896`
<!-- generated:end DSH4240 -->

**原因。** ``#pragma region`` 和 ``#pragma endregion`` 不成对 —— 有 ``endregion`` 却没有打开任何区域，或者有 ``region`` 从未关闭。区域可以嵌套，并且按文件、按函数体分别计数，因此在头文件中打开的区域无法由包含它的文件来关闭。

**修复。** 补上缺失的那一行。在函数体内打开的区域在该函数体内关闭。

<a id="dsh4241"></a>

## DSH4241

<!-- generated:begin DSH4241 -->
**严重级别** 错误

**消息**

```
A multi-dimensional array has no graph form; declare one dimension, or move the code into a '/// @custom' function.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:607`
<!-- generated:end DSH4241 -->

**原因。** 一个声明有多个 ``[n]``。图中完全没有数组；唯一受支持的一个维度只是为了能在编译时对 ``static const`` 表进行索引。

**修复。** 将其展平为一维，或将代码移入 ``/// @custom`` 函数中。

<a id="dsh4242"></a>

## DSH4242

<!-- generated:begin DSH4242 -->
**严重级别** 错误

**消息**

```
An array index must be a compile-time constant: the graph has no arrays, so every element is read at compile time.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1639`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1648`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1676`
<!-- generated:end DSH4242 -->

**原因。** 无法在编译时解析数组元素：非常量索引、索引超出范围，或者数组的初始化器不是常量。图中没有数组，因此每次元素读取都必须在编译期间折叠为一个值。

**修复。** 将该数组声明为 ``static const`` 并使用常量初始化器，然后用字面量或 ``static const`` 对其进行索引。对于需要在运行时读取的表，请使用纹理或 ``@custom`` 函数。

<a id="dsh4243"></a>

## DSH4243

<!-- generated:begin DSH4243 -->
**严重性** 错误

**消息**

```
Both sides of this '/' are integers, and the material graph has no integer division; write 'float(a) / b' for the fraction, or 'floor(float(a) / b)' for the whole part.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2052`
<!-- generated:end DSH4243 -->

**原因。** `/` 的两个操作数都是整数。HLSL 会将它们作为整数相除并舍去小数部分；而材质图根本没有整数除法——其中的每个值都是浮点数——因此两者会产生分歧，编译器不会默默选择其中一种。在 1.x 的 `Graph` 中，只有两个整数构造函数调用（`int(7) / int(2)`）的除法会被拒绝，正如 1.x 拒绝它一样（`DSH4063`）；1.x 会将每个数字字面量都视为浮点数，因此那里的 `7 / 2` 为 3.5（旧规则 L27；*2.0.0 – 2.1.0 拒绝它*）。

**修复。** 说明想要的是哪一种：`float(a) / b` 表示小数部分，`floor(float(a) / b)` 表示整数部分。

<a id="dsh4244"></a>

## DSH4244

<!-- generated:begin DSH4244 -->
**严重性** 错误

**消息**

```
A matrix row cannot be read: the graph has no matrices. Move the code into a '/// @custom' function, where the matrix is an input.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1691`
<!-- generated:end DSH4244 -->

**原因。** 读取了矩阵的一行（`M[1]`）。矩阵没有图值；它仅作为 `/// @custom` 的输入或输出存在，或者作为完全折叠掉的常量存在。

**修复。** 将代码移到 `/// @custom` 函数中，在那里矩阵是普通的 HLSL 值。

<a id="dsh4245"></a>

## DSH4245

<!-- generated:begin DSH4245 -->
**严重性** 错误

**消息**

```
An unsized array needs an initializer list to take its length from.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:626`、`Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:649`、`Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:659`
<!-- generated:end DSH4245 -->

**原因。** 数组大小不是介于 1 到 4096 之间的整数字面量，或者未指定大小的 `[]` 没有初始化列表来确定其长度。大小在绑定任何内容之前就被读取，因此表达式——即使是常量表达式——也无法使用。

**修复。** 写入字面量，或使用带初始化列表的 `[]`。

<a id="dsh4246"></a>

## DSH4246

<!-- generated:begin DSH4246 -->
**严重性** 错误

**消息**

```
The material graph has no hyperbolic node, so '{0}' cannot be lowered; write it in a '/// @custom' body, where the shader compiler has it.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3366`
<!-- generated:end DSH4246 -->

**原因。** 调用了 `sinh`、`cosh` 或 `tanh`。核心表包含这三者，以便 `/// @custom` 主体与类型检查器对它们达成一致，但材质图没有双曲节点，并且发射器不会从 `exp` 中合成一个——否则看起来正确的链会与着色器编译器自身的实现悄然不同。

**修复。** 将表达式移到 `/// @custom` 函数中，该函数的主体是着色器编译器完整可见的 HLSL，且三者都存在。如果只需要一个值，用 `exp` 显式写出恒等式也是可以的——关键在于由文件而非发射器来说明这一点。

<a id="dsh4250"></a>

## DSH4250

<!-- generated:begin DSH4250 -->
**严重性** 错误

**消息**

```
'{0}' is the GLSL spelling; this language is HLSL, so write '{1}'.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3034`
<!-- generated:end DSH4250 -->

**原因。** 使用了 GLSL 拼写：`mix`、`fract`、`mod`。1.x 会将这些静默重写为对应的 HLSL 等价物；2.0 则不会，因为将一个标识符静默重写为另一个正是此前端旨在阻止的那类错误。

**修复。** 写入消息给出的 HLSL 名称：`lerp`、`frac`、`fmod`。

<a id="dsh4251"></a>

## DSH4251

<!-- generated:begin DSH4251 -->
**严重性** 警告

**消息**

```
'{0}' hides the file-scope declaration of the same name for the rest of this function.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:181`
<!-- generated:end DSH4251 -->

**原因。** 局部变量与文件作用域声明同名，通常是来自头文件的声明。这是允许的——不能指望文件了解其头文件声明的每个名称——但从那一刻起，该名称的 uniform 或常量在此函数中将无法访问。

**修复。** 如果本意是文件作用域的那个，请重命名局部变量。否则忽略它，或重命名以使意图对下一位读者显而易见。

<a id="dsh4260"></a>

## DSH4260

<!-- generated:begin DSH4260 -->
**严重性** 错误

**消息**

```
{0} expects {1}, and this is {2}.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1928`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2511`、`Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:628`
<!-- generated:end DSH4260 -->

**原因。** `if`、`for`、`while`、`do` 或 `?:` 的条件不是单个真或假值：而是向量（`if (v3 < 1.0)`）、纹理、`material`、`struct`。

**修复。** 将其归约为单个值：比较一个分量，或手动写出 `all()` / `any()` 语义。数字是被接受的，表示“非零”。

<a id="dsh4261"></a>

## DSH4261

<!-- generated:begin DSH4261 -->
**严重性** 错误

**消息**

```
'{0}' returns {1}, so this 'return' needs a value.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:735`
<!-- generated:end DSH4261 -->

**原因。** 在声明了返回类型的函数中出现了裸 `return;`。

**修复。** 返回一个值，或将函数声明为 `void` 并通过 `out` 参数传回其结果。

<a id="dsh4262"></a>

## DSH4262

<!-- generated:begin DSH4262 -->
**严重性** 错误

**消息**

```
'{0}' returns nothing, so this 'return' cannot carry a value; extra results are written to 'out' parameters.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:751`
<!-- generated:end DSH4262 -->

**原因。** 在 `void` 函数中出现了 `return <value>;`。

**修复。** 去掉该值，或为函数指定返回类型。

<a id="dsh4263"></a>

## DSH4263

<!-- generated:begin DSH4263 -->
**严重性** 错误

**消息**

```
'discard' belongs in a material entry or a layer; a material function has no pixel of its own to drop. Write the mask into 'm.OpacityMask' instead.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:493`
<!-- generated:end DSH4263 -->

**原因。** 在不是材质入口、`@layer` 或 `@layerblend` 的函数中出现了 `discard`。材质函数是调用它的任何内容的子图；它没有自己的像素可以丢弃。

**修复。** 将遮罩写入 `m.OpacityMask` 并让材质进行丢弃，或将 `discard` 移到入口中。

<a id="dsh4264"></a>

## DSH4264

<!-- generated:begin DSH4264 -->
**严重性** 错误

**消息**

```
'break' leaves a loop, and this one is not inside a 'for', 'while' or 'do'.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:472`
<!-- generated:end DSH4264 -->

**原因。** `break` 或 `continue` 出现在任何循环之外。

**修复。** 删除它，或将其放入所属的循环中。注意此语言没有 `switch`，因此 `break` 只属于循环。

<a id="dsh4300"></a>

## DSH4300

<!-- generated:begin DSH4300 -->
**严重性** 错误

**消息**

```
Node {0} reads {1} from node %{2}, which does not exist; the graph has {3} nodes.
```

**由以下抛出** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:221`
<!-- generated:end DSH4300 -->

**原因。** 节点的操作数或命名输入引用了节点 `%N`，而图中没有该索引的节点。IR 按在 `FIRGraph::Nodes` 中的位置寻址值，因此这意味着构建器或某个 pass 针对不同（或更旧）的图生成了索引——最常见的是某个 pass 移除了节点却没有重新指向其使用者，或者某个值跨越两个产品传递。

**修复。** 编译器侧：检查最后运行的 pass。`RunDreamShaderIRPasses` prune 和 dedupe 是重新编号的两个 pass；两者都必须在节点消失前重新指向每个使用者。作者侧：你写的任何内容都不会导致此问题；请报告该文件。

<a id="dsh4301"></a>

## DSH4301

<!-- generated:begin DSH4301 -->
**严重性** 错误

**消息**

```
Node {0} reads {1} from node {2}, which is a statement and produces no value.
```

**由以下抛出** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:240`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:253`
<!-- generated:end DSH4301 -->

**原因。** 有两种情况。要么某个节点读取了输出少于 *k*+1 个的节点的输出 *k*——多输出节点（`TextureSample`、具有多个输出的类上的 `Reflected`、带附加输出的 `Custom`、`GetMaterialAttributes`）是唯一具有非 0 输出的节点，因此这通常是解析到错误输出索引的成员访问——要么某个节点读取了**语句**，而语句根本不产生值。语句是材质汇点、函数输出，或反射的自定义输出类，例如 `UE.VolumetricAdvancedMaterialOutput`（`FIRNode::IsStatement()`）。

**修复。** 作者侧：自定义输出节点是一条语句，而不是一个值——将其写在
`UE.ClearCoatNormalCustomOutput(Input = n);` 单独一行，而不是将其赋值给某个东西。
绑定器将该拼写报告为 DSH4231，因此在此处看到它意味着该节点无论如何都被构建了。
编译器侧：将 `NodeOutput` 访问的 `FBoundExpr::FieldIndex` 与
构建器赋予该节点的 `Outputs` 进行比较；对于反射类，目录条目的 `Outputs` 数组是权威依据。

<a id="dsh4302"></a>

## DSH4302

<!-- generated:begin DSH4302 -->
**严重性** 错误

**消息**

```
Node {0} has {1} operand(s); this op takes exactly {2}.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:382`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:393`
<!-- generated:end DSH4302 -->

**原因。** 该节点的操作数比其 op 所接受的更多或更少。元数来自
`GetCoreOpInfo`，这是唯一拥有该信息的表，因此不匹配意味着构建器是
手动组装该节点，而不是从该表构建。

**修复。** 编译器侧：使用 `FIRCoreOpInfo::MinArity`/`MaxArity` 构建核心节点，而不是使用
字面量计数。作者侧：报告该文件，并附上诊断所指向的表达式。

<a id="dsh4303"></a>

## DSH4303

<!-- generated:begin DSH4303 -->
**严重性** 错误

**消息**

```
Node {0} has nothing connected to {1}; every operand of this op must carry a value.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:209`
<!-- generated:end DSH4303 -->

**原因。** 一个必须携带值的操作数槽为空。整个 IR 中只有两个槽可以
保持未设置：`Compare` 的操作数 3（引擎 If 节点的可选 `AEqualsB` 引脚）以及
`TextureSample` 的操作数 2 和 3（可选的采样器和 mip 值）。其他所有内容都是发射器
无法填补的漏洞。

**修复。** 编译器侧：一个表达式被降低为 `FIRValue::None()` 但没有诊断——查找
在未报告的错误上提前返回的降低路径。作者侧：报告该文件。

<a id="dsh4304"></a>

## DSH4304

<!-- generated:begin DSH4304 -->
**严重性** 错误

**消息**

```
Node {0} carries {1} positional operand(s); this op takes {2}.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:347`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:364`
<!-- generated:end DSH4304 -->

**原因。** 该节点为其 op 使用了错误的连接风格。`IR.h` 针对每个 op 修正了这一点：核心
数学 op、`Swizzle`、`Append`、`Select`、`Compare`、`StaticSwitch`、`TextureSample`、
`FunctionOutput`、`Convert` 和 `Broadcast` 使用位置操作数；`Reflected`、`FunctionCall`、
`Custom`、三个材质属性 op 和 `MaterialSink` 使用命名输入；四个叶子
（`Constant`、`Parameter`、`TextureParameter`、`FunctionInput`）两者都不使用。

**修复。** 编译器侧：一个使用 `Operands` 构建的节点，而发射器将在 `Inputs` 中查找（或
反之），会静默地发射一个未连接任何内容的引擎节点，这就是为什么这会被检查
而不是被容忍。作者侧：报告该文件。

<a id="dsh4305"></a>

## DSH4305

<!-- generated:begin DSH4305 -->
**严重性** 错误

**消息**

```
Node {0} has a named input with no pin name.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:948`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:961`
<!-- generated:end DSH4305 -->

**原因。** 命名输入的引脚名称为空，或者同一引脚名称在一个节点上出现两次。一个
引擎 `FExpressionInput` 恰好接受一个连接，因此对同一引脚的第二次绑定将
意味着两个值中的一个被无声地丢弃。

**修复。** 编译器侧：这通常是一个参数被匹配了两次——一次是通过
`PositionalParameters` 按位置匹配，一次是按名称匹配。`FBoundArgument::Target` 是去重点。
作者侧：检查你是否在 `UE.*`
调用中同时按位置和按名称传递了同一参数；如果你没有，请报告该文件。

<a id="dsh4306"></a>

## DSH4306

<!-- generated:begin DSH4306 -->
**严重性** 错误

**消息**

```
Node {0} does arithmetic on operand {1}, which is a {2}; the graph carries only float1 to float4, so a matrix, a texture or a material cannot reach a math node.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:690`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:704`
<!-- generated:end DSH4306 -->

**原因。** 一个数学节点读取或产生了一个材质图表没有连线对应的值。该图表
携带 `float1` 到 `float4` 以及除此之外什么都没有：矩阵、纹理对象、采样器、一个 `material`
以及一个 `Substrate` 值对 `FIRType::GraphComponentCount()` 都回答 0。

**修复。** 作者侧：这是 DSH4361 的 IR 级回显——一个矩阵进入了算术。将
表达式移入一个 `/// @custom` 函数，其主体是逐字 HLSL，可以自由使用矩阵。
编译器侧：绑定器本应在降低之前拒绝这一点；如果它没有，则它
赋予操作数的类型是错误的。

<a id="dsh4307"></a>

## DSH4307

<!-- generated:begin DSH4307 -->
**严重性** 错误

**消息**

```
Node {0} is a statement and must have no outputs, but it declares {1}.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:289`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:300`
<!-- generated:end DSH4307 -->

**原因。** 要么一个值节点没有声明任何输出——没有任何东西可以读取它，因此它不可能
是为了某个原因而被构建的——要么一个语句节点声明了一些输出。

语句是 `MaterialSink`、`FunctionOutput`，或一个具有零个输出的 `Reflected` 节点，这
就是自定义输出类（`UE.VolumetricAdvancedMaterialOutput`、`UE.ClearCoatNormalCustomOutput`）
被建模的方式。第三种形态是合法的根，并且**不会**在此处被报告；
测试是 `FIRNode::IsStatement()`，从不是 op 列表。

**修复。** 编译器侧：每个添加节点的构建器路径都必须在返回
该值之前填充 `Outputs`；一个先添加后定型的节点是常见的来源。作者侧：报告该文件。

<a id="dsh4308"></a>

## DSH4308

<!-- generated:begin DSH4308 -->
**严重性** 错误

**消息**

```
Node {0} has {1} output name(s) for {2} output(s); the two lists are either parallel or the names are omitted.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:311`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:331`
<!-- generated:end DSH4308 -->

**原因。** `OutputNames` 的存在是为了命名多输出节点，它要么为空（该节点的
输出是按位置的），要么恰好与 `Outputs` 一样长。列表过短意味着发射器会读取
超过其末尾；两个具有相同名称的输出意味着成员访问无法在它们之间进行选择。

**修复。** 编译器侧：在同一位置填充两个列表。对于反射节点，逐字复制目录
条目的 `Outputs` 名称。作者侧：报告该文件。

<a id="dsh4309"></a>

## DSH4309

<!-- generated:begin DSH4309 -->
**严重性** 错误

**消息**

```
Node {0} is a reflected node with catalog index {1}, which is not an entry of the builtin catalog.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1038`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:721`
<!-- generated:end DSH4309 -->

**原因。** 同一错误的两种形态。要么一个 `Reflected` 节点的 `CatalogIndex` 没有
指向内置目录中的某个条目——因此发射器无法知道要创建哪个类——要么一个
*并非*反射的节点携带了目录索引，这会使发射器对 `Multiply` 走
反射路径。

**修复。** 编译器侧：`CatalogIndex` 仅在构建 `Reflected` 节点时设置，来自
绑定器记录的 `FBoundExpr::Index`。过期的索引通常意味着节点被复制且其 op
被更改。作者侧：如果目录是从导出的清单加载的，请重新导出它——索引
是按位置的，旧清单与新引擎不匹配。

<a id="dsh4310"></a>

## DSH4310

<!-- generated:begin DSH4310 -->
**严重性** 错误

**消息**

```
Node {0} connects pin '{1}', which '{2}' does not have.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:758`
<!-- generated:end DSH4310 -->

**原因。** 一个反射节点连接了该类没有的引脚。目录的 `Inputs` 列表
是从该类的 `FExpressionInput` 成员构建的，因此不在其中的引脚在该引擎类上
不存在。

**修复。** 作者侧：检查拼写——引脚匹配区分大小写，并且名称是 C++
成员名称（在 TextureSample 上是 `Coordinates`，而不是 `UVs`；在 Normalize 上是 `VectorInput`）。
材质编辑器的节点工具提示显示引脚名称。编译器侧：如果该引脚*确实*存在，则
反射导出器跳过了它——检查 `IsMaterialExpressionInputProperty`。

<a id="dsh4311"></a>

## DSH4311

<!-- generated:begin DSH4311 -->
**严重性** 错误

**消息**

```
Node {0} sets property '{1}', which '{2}' does not have.
```

**抛出位置** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:800`
<!-- generated:end DSH4311 -->

**原因。** 一个被反射的节点设置了该类不具有的属性。`ClassSpecifier`（IR 中对作者所写内容的自身记录）和引脚的 `Const*` 孪生项是被接受的；其他所有内容必须是该类的被反射属性。

**修复。** 作者侧：`UE.*` 调用中的未知 `Key = value`，通常是拼写错误或存在于不同表达式类上的属性。编译器侧：如果该属性确实存在但不在目录中，则反射导出器对其进行了过滤——请检查 `BuildBuiltinCatalogFromReflection` 中的属性标志测试。

<a id="dsh4312"></a>

## DSH4312

<!-- generated:begin DSH4312 -->
**严重性** 错误

**消息**

```
Node {0} names class '{1}' but its catalog entry is '{2}'; the two must agree.
```

**抛出位置** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:735`
<!-- generated:end DSH4312 -->

**原因。** 该节点的 `ClassName` 与其 `CatalogIndex` 所指向的目录条目命名了两个不同的类。发射器使用两者之一，而验证器拒绝猜测使用哪一个。

**修复。** 编译器侧：在构建节点时从 `Catalog.Expressions[CatalogIndex].ShortName` 设置 `ClassName`，绝不从源文本设置；作者的拼写应归入 `Prop::ClassSpecifier`。作者侧：如果您使用的是目录清单，请重新导出它。

<a id="dsh4313"></a>

## DSH4313

<!-- generated:begin DSH4313 -->
**严重性** 错误

**消息**

```
Material product {0} has {1} MaterialSink node(s); a material has exactly one.
```

**抛出位置** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1072`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1082`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1096`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1107`
<!-- generated:end DSH4313 -->

**原因。** 产品的汇点与其种类不匹配。`Material` 产品恰好有一个 `MaterialSink` 节点，并且 `FIRGraph::Sink` 指向它；而 `MaterialFunction`、`MaterialLayer` 或 `MaterialLayerBlend` 没有汇点，并且 `Sink` 为 `INDEX_NONE`。

**修复。** 编译器侧：汇点在 Entry 末尾根据材质字段映射创建一次——两个汇点意味着该条目被降低了两次。作者侧：包含两个 `export void M(inout material)` 条目的文件是 DSH6200，本应更早被拒绝；请报告该文件。

<a id="dsh4314"></a>

## DSH4314

<!-- generated:begin DSH4314 -->
**严重性** 错误

**消息**

```
Product {0} lists node %{1} in its {2}, and that node does not exist.
```

**抛出位置** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1125`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1137`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1158`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1168`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1182`
<!-- generated:end DSH4314 -->

**原因。** 图的 `FunctionInputs` / `FunctionOutputs` 列表与其中的 FunctionInput / FunctionOutput 节点不一致：列出的索引不存在、列出的节点操作符错误、正确操作符的节点未被列出，或者 `Material` 产品声明了签名。这些列表赋予了资产输入和输出的顺序（它们密集的 SortPriority），因此未列在其中的节点最终会无序。

**修复。** 编译器侧：在添加节点的同一条语句中追加到列表。作者侧：报告该文件。

<a id="dsh4315"></a>

## DSH4315

<!-- generated:begin DSH4315 -->
**严重性** 错误

**消息**

```
Product {0} is a {1} and produces nothing; a material function needs at least one FunctionOutput.
```

**抛出位置** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1192`
<!-- generated:end DSH4315 -->

**原因。** 材质函数、层或层混合没有 `FunctionOutput` 节点，因此该资产将没有可连接的对象。导出的函数为其返回值（名为 `Result`）贡献一个输出，并为每个 `out` 参数贡献一个输出；两者都没有的函数不产生任何资产。

**修复。** 作者侧：为导出的函数提供一个非 `void` 的返回类型或一个 `out` 参数。`@layer` 函数的输出就是其 `inout material` 参数，因此请检查该参数确实拼写为 `inout`。编译器侧：检查 `FBoundFunction::MaterialResultParam`。

<a id="dsh4316"></a>

## DSH4316

<!-- generated:begin DSH4316 -->
**严重性** 错误

**消息**

```
Product {0} cannot be ordered: these nodes feed themselves, directly or through others -- {1}.
```

**抛出位置** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1516`
<!-- generated:end DSH4316 -->

**原因。** 图无法排列成每个操作数都先于其使用者的顺序，这意味着某个节点直接地或通过链间接地馈送到自身。材质图是一个 DAG；发射器遍历 `TopologicalOrder()`，否则会在其读取的节点之前写入该节点。

**修复。** 编译器侧：常见的来源是某个 pass 将使用者重新指向其下游的节点——即去重 pass 合并了两个节点，而其中一个是另一个的祖先。诊断会列出它无法排序的节点；循环就在其中。作者侧：携带节点来源函数的文件一起报告。

<a id="dsh4317"></a>

## DSH4317

<!-- generated:begin DSH4317 -->
**严重性** 错误

**消息**

```
Node {0} needs a string Mask property holding the component letters.
```

**抛出位置** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:512`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:524`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:554`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:567`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:585`
<!-- generated:end DSH4317 -->

**原因。** `Swizzle` 节点的掩码缺失、不是字符串、为空、长度超过四个字符、包含 `x`、`y`、`z` 和 `w` 以外的内容、两次命名同一组件，或者**未按严格升序排列**（`xz` 是掩码；`zx` 不是）。规范掩码是小写的 `xyzw`；`rgba` 在绑定时被重写为它，因此到达 IR 的任何其他内容都是构建器的错误。

排序规则并非吹毛求疵：ComponentMask 携带四个独立的 R/G/B/A 标志，完全无法表达顺序或重复，因此将 `.yx` 作为单个节点发出会静默地变成 `.xy`。

**修复。** 编译器侧：`FBoundExpr::Swizzle` 已经持有规范掩码——复制它，而不是从源文本重新推导。重排或重复的 swizzle 会降低为逐通道掩码加上一个 `AppendVector`，这正是 1.x 所发出的：构建器会将其拆分，只有升序的剩余部分会成为 `Swizzle` 节点。作者侧：`.yx` 和 `.xxz` 是合法的源，因此看到此情况意味着拆分未发生——请报告该文件。

<a id="dsh4318"></a>

## DSH4318

<!-- generated:begin DSH4318 -->
**严重性** 错误

**消息**

```
Node {0} masks '{1}' out of a {2}, which has only {3} component(s).
```

**抛出位置** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:615`
<!-- generated:end DSH4318 -->

**原因。** 掩码命名了操作数不具有的组件——`float3` 上的 `.w`，`float2` 上的 `.z`。在 HLSL 中这是一个错误；在图中它会静默地读取连线并未携带的通道。

**修复。** 作者侧：扩展该值（`float4(v, 1)`）或缩短掩码。编译器侧：绑定器在源级别为此抛出 DSH4230；到达 IR 意味着操作数的类型在绑定和降低之间发生了变化——通常是插入在错误一侧的转换。

<a id="dsh4319"></a>

## DSH4319

<!-- generated:begin DSH4319 -->
**严重性** 错误

**消息**

```
Node {0} switches on a {1}; a StaticSwitch condition is a single static bool.
```

**抛出位置** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:648`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:661`
<!-- generated:end DSH4319 -->

**原因。** 分支节点的条件类型错误。`StaticSwitch` 条件必须是单个静态布尔值——一个 `StaticBoolParameter`，或者对静态布尔值的 `!`、`&&`、`||`。`Select` 条件必须是一个分量，即降低后的 If 与 0.5 进行比较的 0/1 值。

**修复。** 编译器侧：如果条件是宽向量，则某次比较在降低时未折叠为标量。如果 `StaticSwitch` 具有非布尔条件，则静态和动态降低之间的决定是基于绑定器未标记为静态的值做出的。作者侧：`#pragma` 级别的静态开关需要一个带 `/// @static` 的 `uniform bool`；没有它该分支就是动态的，这也没问题。

<a id="dsh4320"></a>

## DSH4320

<!-- generated:begin DSH4320 -->
**严重性** 错误

**消息**

```
Node {0} sits in region {1}, which does not exist; the graph has {2} region(s).
```

**抛出者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1211`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1228`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:408`
<!-- generated:end DSH4320 -->

**原因。** 区域引用无法解析：某个节点位于图中不存在的区域索引中，某个区域指定了一个不存在的父级，或者父级链回环到自身。区域来自 `#pragma region` / `#pragma endregion`，并严格嵌套。

**修复。** 编译器侧：`FBoundModule::Regions` 被整体复制到图中，因此索引也必须随之复制——若图保留自己的区域列表，则必须重新映射 `FIRNode::Region`。作者侧：不配对的 `#pragma region` 属于 DSH4240，会在源码级别报告；如果你看到的是这一条而非那条，请报告该文件。

<a id="dsh4321"></a>

## DSH4321

<!-- generated:begin DSH4321 -->
**严重性** 错误

**消息**

```
Product {0} has dedupe keys on {1} of its {2} nodes; the key is either computed for the whole graph or for none of it.
```

**抛出者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1537`
<!-- generated:end DSH4321 -->

**原因。** 图中的部分节点有 `DedupeKey`，而其他节点没有。该键是结构性的：它要么在一次遍历中为整张图计算，要么完全不为任何节点计算，因为由没有自身键的操作数构建出的键根本不是结构性的。

**修复。** 编译器侧：在 `TopologicalOrder()` 中计算键，使每个操作数都已经拥有一个键，并且只有在重新运行整个遍历时，才用空键添加在去重遍历*之后*创建的节点。作者侧：报告该文件。

<a id="dsh4322"></a>

## DSH4322

<!-- generated:begin DSH4322 -->
**严重性** 错误

**消息**

```
Nodes %{0} and %{1} of product {2} have the same dedupe key, so the dedupe pass should have merged them into one.
```

**抛出者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1586`
<!-- generated:end DSH4322 -->

**原因。** 同一张图的两个节点具有相同的去重键，因此它们描述的是同一个表达式，去重遍历本应将它们合并为一个节点，并使用两方使用者的集合。

**修复。** 编译器侧：合并步骤会重新指向使用者并丢弃后一个节点；若仍有一对存活，说明重新指向循环漏掉了一个引用——请同时检查命名输入和操作数，以及图自身的 `Sink`、`FunctionInputs` 和 `FunctionOutputs`。作者侧：报告该文件；输出仍然正确，只是比应有的更大。

<a id="dsh4323"></a>

## DSH4323

<!-- generated:begin DSH4323 -->
**严重性** 错误

**消息**

```
Node {0} needs property '{1}' and does not have it.
```

**抛出者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:492`
<!-- generated:end DSH4323 -->

**原因。** 某个节点缺少其运算不可或缺的属性：`Constant` 缺少 `Value`，`Parameter` 或 `TextureParameter` 缺少 `ParameterName`，`FunctionInput` 缺少 `InputName`，`FunctionOutput` 缺少 `OutputName`，`Custom` 缺少 `Code`。

**修复。** 编译器侧：在创建该节点的同一个函数中设置这些属性。作者侧：没有名称的 `uniform` 是无法写出的，因此请报告该文件。

<a id="dsh4324"></a>

## DSH4324

<!-- generated:begin DSH4324 -->
**严重性** 错误

**消息**

```
Node {0} carries a property with no name.
```

**抛出者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:432`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:445`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:467`
<!-- generated:end DSH4324 -->

**原因。** 有三种形态：属性没有名称，同一属性在同一节点上被设置两次，或者属性的存储种类不是 IR 为该名称定义的种类（`Value` 和 `DefaultValue` 是 `Float4`，`SortPriority` 是 `Int`，`Code` 是 `String`，`SamplerType` 是 `Enum`，依此类推——`IR.h` 中的 `Prop::` 逐一列出了每一种）。种类表只应用于 IR 自身的属性名称，绝不应用于 `Reflected` 节点，后者的属性是直接透传的引擎名称。

**修复。** 编译器侧：使用匹配的 `FIRPropertyValue::Make*` 工厂来构建属性。以 `MakeFloat` 存储的 `SortPriority` 在发射器中读回为 0。作者侧：报告该文件。

<a id="dsh4325"></a>

## DSH4325

<!-- generated:begin DSH4325 -->
**严重性** 错误

**消息**

```
Node {0} writes attribute '{1}', which is not in the engine's material attribute table.
```

**抛出者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:886`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:904`
<!-- generated:end DSH4325 -->

**原因。** 写入 `MakeMaterialAttributes`、`SetMaterialAttributes`、`GetMaterialAttributes` 或 `MaterialSink` 节点的某个属性名称——或者列在 `AttributeSetTypes` 中的属性名称——不在引擎的材质属性表中。该表来自目录，且匹配区分大小写。

**修复。** 作者侧：检查你写入的 `m.<Attribute>` 拼写；可接受的名称及其别名就是材质编辑器的 Break Material Attributes 节点所列出那些（`EmissiveColor`，而不是 `Emissive Color`）。编译器侧：如果该属性确实存在，说明目录的 `MaterialAttributes` 数组缺少它——请重新导出清单。

<a id="dsh4326"></a>

## DSH4326

<!-- generated:begin DSH4326 -->
**严重性** 错误

**消息**

```
Product {0} has no name; the asset name comes from the exported function or from '/// @name'.
```

**抛出者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1641`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1654`
<!-- generated:end DSH4326 -->

**原因。** 某个产物具有空的 `Name`，或者某个文件的两个产物会被赋予相同的名称。资源名称来自导出函数的名称或来自 `/// @name`，而同一目标文件夹中两个同名的资源会互相覆盖。

**修复。** 作者侧：为两个导出函数之一取一个不同的名称，或使用不同的 `/// @name`。请注意，带有完整 `/Game/...` 路径的 `@name` 会设置整个路径，只有叶子部分必须唯一。编译器侧：产物名称由绑定器决定；请检查 `FBoundProduct::AssetName`。

<a id="dsh4327"></a>

## DSH4327

<!-- generated:begin DSH4327 -->
**严重性** 错误

**消息**

```
Node {0} calls local product {1}, which this module does not have; it has {2}.
```

**抛出者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:825`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:837`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:853`
<!-- generated:end DSH4327 -->

**原因。** 某个 `FunctionCall` 节点没有说明它调用什么。它需要 `Prop::LocalFunction`（本模块另一个产物的索引，用于调用同一文件中的 `export`），或者一个非空的函数资源路径——`Prop::FunctionPath`，或 `ClassName`。超出范围的局部索引，或者指向该节点自身所属产物的局部索引，都是同样的失败。

**修复。** 编译器侧：对同一文件的 `ExportFunction` 的调用会获得 `LocalFunction`；对 `Extern` 的调用会获得 `@asset` 路径。产物调用自身意味着递归越过了 DSH6200。作者侧：检查 `extern` 原型是否有其 `/// @asset /Game/...` 行。

<a id="dsh4328"></a>

## DSH4328

<!-- generated:begin DSH4328 -->
**严重性** 警告

**消息**

```
Layout hint {0} of product {1} has kind '{2}'; a hint is spelled exactly 'Node' or 'Comment', so this one places nothing.
```

**抛出者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1261`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1273`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1283`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1294`
<!-- generated:end DSH4328 -->

**原因。** 某个 `#pragma layout(...)` 提示是布局遍历无法应用的：其种类既不是 `Node` 也不是 `Comment`，`Node` 提示未指定任何变量，或者 `Comment` 提示没有标题。这是一个警告而非错误：产物仍会编译，自动布局仍会运行；只是此提示不放置任何内容。

**修复。** 作者侧：写入 `#pragma layout(Node, Var = Tint, X = 10, Y = 20)` 或 `#pragma layout(Comment, Name = "Lighting", X = 0, Y = 0, W = 400, H = 200)`。编译器侧：提示从 `FBoundModule::LayoutHints` 原样传递，因此解析处才是需要检查的地方。

<a id="dsh4329"></a>

## DSH4329

<!-- generated:begin DSH4329 -->
**严重性** 错误

**消息**

```
Product {0} is a material instance, which assigns its parent's parameters and has no graph, and it carries {1} node(s), a sink or a function signature.
```

**抛出者** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1366`
<!-- generated:end DSH4329 -->

**原因。** 某个种类为 MaterialInstance 的 IR 产物携带了图节点、汇点或函数签名。实例只赋值其父级的参数。这是 IR 验证器的内部一致性检查：任何源文本都无法导致它。

**修复。** 连同产生它的 `.dsi` 一起报告；这是编译器的缺陷。

<a id="dsh4330"></a>

## DSH4330

<!-- generated:begin DSH4330 -->
**严重性** 错误

**消息**

```
Material instance product {0} needs the material it is an instance of, and its parent reference is empty.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1377`
<!-- generated:end DSH4330 -->

**原因。** 某 MaterialInstance 产物没有父引用。绑定器在缺少
`Parent`（DSH7250、DSH7252）时会拒绝 `.dsi`，
因此触发此错误属于内部错误。

**修复。** 连同产生它的源一同上报。

<a id="dsh4331"></a>

## DSH4331

<!-- generated:begin DSH4331 -->
**严重性** error

**消息**

```
Override '{0}' of product {1} is a {2} override holding a {3} value, and a {2} override holds {4}.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1403`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1416`
<!-- generated:end DSH4331 -->

**原因。** 某实例覆写所持有的值类型与其参数类型不符 ——
Scalar 是一个数，Vector 是四个，StaticSwitch 是 bool，资源类则是对象路径 —— 或者某
Font 覆写指定了负数页。这是内部一致性检查：绑定器在源中会拒绝二者
（`DSH7259`、`DSH7266`）。

**修复。** 连同产生它的源一同上报。

<a id="dsh4332"></a>

## DSH4332

<!-- generated:begin DSH4332 -->
**严重性** error

**消息**

```
Product {0} overrides '{1}' twice, and an instance sets each parameter once.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1433`
<!-- generated:end DSH4332 -->

**原因。** 某个参数在某实例产物中被覆写了两次。绑定器会在源上报告同样的问题
（DSH7261）；这是验证器的兜底。

**修复。** 从 `.dsi` 中移除该名称的两个 `uniform` 行之一。

<a id="dsh4333"></a>

## DSH4333

<!-- generated:begin DSH4333 -->
**严重性** error

**消息**

```
Override '{0}' of product {1} names a parameter, and the parent schema the product carries has no such parameter.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1449`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1459`
<!-- generated:end DSH4333 -->

**原因。** 某实例产物覆写了一个其父架构未列出的参数，或以另一种类型列出了该参数。绑定器会
针对同一架构检查二者（`DSH7258`、`DSH7259`），因此触发此错误意味着二者
看到的是不同的架构。

**修复。** 先重建父，再重建实例；若问题依旧，请上报。

<a id="dsh4334"></a>

## DSH4334

<!-- generated:begin DSH4334 -->
**严重性** error

**消息**

```
This module holds a material instance and {0} other product(s), and a material instance is the only product of its file.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1613`、`Source/DreamShaderLang/Private/IR/IRValidator.cpp:1627`
<!-- generated:end DSH4334 -->

**原因。** 某模块将材质实例与其他产物放在一起。一个 `.dsi` 就是一个实例，
别无其他；前端会拒绝其中的函数与其他声明，因此这是验证器的兜底。

**修复。** 将每个实例放在其各自独立的 `.dsi` 中。

<a id="dsh4335"></a>

## DSH4335

<!-- generated:begin DSH4335 -->
**严重性** error

**消息**

```
Material instance product {0} carries backend {1}, and an instance is a plain material instance with the default backend.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRValidator.cpp:1387`
<!-- generated:end DSH4335 -->

**原因。** 某 MaterialInstance 产物携带了后端。`Backend` 是材质的属性；实例
始终是普通的 UMaterialInstanceConstant。

**修复。** 从 `#pragma instance(...)` 中移除 `Backend`。

<a id="dsh4350"></a>

## DSH4350

<!-- generated:begin DSH4350 -->
**严重性** error

**消息**

```
The IR builder cannot read '{0}' here: the binder typed what it is read from as a material, but that did not lower to one.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRBuilderExpressions.cpp:386`、`Source/DreamShaderLang/Private/IR/IRBuilderExpressions.cpp:516`
<!-- generated:end DSH4350 -->

**原因。** IR 构建器遇到了一个其 `EBoundExprKind` 没有对应分支的已绑定表达式。
这属于内部错误，而非编写错误：绑定器产出了构建器未曾针对其编写的记录，
当 `LangBound.h` 新增了一种表达式种类而只有两侧之一被更新时，就会发生
这种情况。

此代码下的第二条消息是同一类接缝：某字段读取，例如 `X.Roughness`，
绑定器将其对象类型定为 `material`，但它并未被降级为该类型。每个 `material`
值的产生者 —— `material` 参数、反射节点的 MaterialAttributes 输出
（`UE.BlendMaterialAttributes(...)`）、函数调用的 `material` 结果、写回的 `out material`
—— 都以属性映射的形式交出该值并以该输出作为其源，因此触发此消息意味着
某产生者只在一侧被添加或修改。

**修复。** 作者在源中无能为力。请上报文件和行号；修复方法是在 `FIRBuilder::LowerExpr` 中
新增分支（对于字段读取，则是那个交回裸值的产生者）。
在此之前，以更简单的形式重写表达式通常可避开没有降级的那种。

<a id="dsh4352"></a>

## DSH4352

<!-- generated:begin DSH4352 -->
**严重性** error

**消息**

```
This module was bound without a builtin catalog, so no 'UE.*' call and no material attribute can be named; run the bind with a catalog, or pass one in FIRBuildOptions.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRBuilder.cpp:121`、`Source/DreamShaderLang/Private/IR/IRBuilderExpressions.cpp:1582`、`Source/DreamShaderLang/Private/IR/IRBuilderExpressions.cpp:915`、`Source/DreamShaderLang/Private/IR/IRBuilderMaterial.cpp:420`、`Source/DreamShaderLang/Private/IR/IRBuilderMaterial.cpp:532`
<!-- generated:end DSH4352 -->

**原因。** 有三种形态，可通过消息指向的位置加以区分。

文件顶部：`BuildDreamShaderIR` 在完全没有内建目录的情况下运行 —— 既没有
`FBoundModule::Catalog`（由绑定器设置），也没有 `FIRBuildOptions::Catalog` 覆写。目录是引擎知识 ——
表达式类、其插针、材质属性表 —— 而 `DreamShaderLang` 无法自行查询其中任何一项，
因此任何需要命名的内容都无法降级：没有 `UE.*` 或 `Substrate.*` 调用，没有诸如
`m.BaseColor` 的材质属性。它针对整个模块报告一次，而非每一行报告一次。

在某个特定属性或 `UE.*` 调用处：目录**确实**存在，但不含绑定器所记录的属性或
表达式索引，这意味着绑定与构建读取了不同的目录 —— 最常见的情况是过期的导出清单
对上较新的引擎。

在材质的入口函数处：入口的 `material` 被整体替换了 —— `m =
UE.BlendMaterialAttributes(...)`, or `m` 被交给了某个会写回
`inout material` 的材质函数 —— 因此到达材质输出的是一个属性集，它通过材质的
`MaterialAttributes` 输入进入，而目录的属性表中没有
`MaterialAttributes` 条目来为该输入命名。

**修复。** 在一次编译内，这三种情况都不可能发生；管线针对同一份由反射填充的目录进行
绑定与构建，而反射总会添加 `MaterialAttributes` 条目。从工具调用时，请传入该目录：`dsc export-catalog` 写入 JSON 形式，
`LoadBuiltinCatalogFromJson` 将其读回，且**同一个**目录必须同时到达
`FBindOptions::Catalog` 与构建。引擎升级后请重新导出清单。目录
存在但为空，意味着反射过程在引擎的材质表达式类加载之前就运行了。

<a id="dsh4360"></a>

## DSH4360

<!-- generated:begin DSH4360 -->
**严重性** error

**消息**

```
This loop runs a number of times the compiler cannot fix at {0} or fewer, and a material graph has no loops; give it a constant trip count, or move it into a '/// @custom' function.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRBuilderStatements.cpp:425`
<!-- generated:end DSH4360 -->

**原因。** 某个 `for`、`while` 或 `do`，其循环次数绑定器无法证明为常量，
或证明其大于 `MaxUnrolledIterations`（默认为 64）。材质图完全没有循环：
其中唯一能存在循环的方式是在编译期展开，而这需要一个在着色器运行之前
就已知的计数。

**修复。** 给循环一个常量边界 —— `for (int i = 0; i < 8; ++i)`，其中 `8` 是字面量或
`static const`，以及一个绑定器能够遵循的步长。以 `uniform` 计数进行的循环从定义上
就无法展开；请将其移入 `/// @custom` 函数，那里的主体是 HLSL，由着色器
编译器来正确地处理循环。非常长的展开（数百次迭代）即使合法，通常也是图中的
错误：它会为每次迭代在资源中放入一个节点。

<a id="dsh4361"></a>

## DSH4361

<!-- generated:begin DSH4361 -->
**严重性** error

**消息**

```
This expression is {0} and the material graph has no matrices; compute it inside the custom body instead.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRBuilderExpressions.cpp:254`
<!-- generated:end DSH4361 -->

**原因。** 某个表达式是矩阵（`float3x3`、`float4x4`），而有东西需要它的值。
材质图没有矩阵插针：引擎的变换节点接受向量和基，而非矩阵，因此没有地方
可以放置它。

**修复。** 对于坐标空间变换，请使用 `UE.Transform` / `UE.TransformPosition`，这是图提供的替代矩阵的方式。对于真正的矩阵运算，请将整个计算移入 `/// @custom` 函数，并将向量传入传出；这样矩阵就存在于 HLSL 内部，在那里它不受限制。

<a id="dsh4362"></a>

## DSH4362

<!-- generated:begin DSH4362 -->
**严重性** 错误

**消息**

```
'discard' has no material-graph form; set the material's OpacityMask to zero instead, or move the branch into a '/// @custom' function.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRBuilderStatements.cpp:125`
<!-- generated:end DSH4362 -->

**原因。** `discard` 出现在一个会成为图的函数体中。材质图没有提前退出：材质是每个输出引脚一个表达式，并且每个引脚都会被求值。

**修复。** 将剔除写为不透明度遮罩——在原本要丢弃的地方将 `m.OpacityMask` 设为 `0`，在其他地方设为 `1`，并将材质的混合模式设为 Masked——这也是引擎对 `discard` 的处理方式。`/// @custom` 函数也不能使用 `discard`，原因相同：它的代码是更大着色器中的一个表达式。

<a id="dsh4363"></a>

## DSH4363

<!-- generated:begin DSH4363 -->
**严重性** 错误

**消息**

```
'{0}' inside an 'if' cannot be unrolled, because both arms of the 'if' become nodes and only one of them may leave the loop; move it to the top of the loop body, or write the loop in a '/// @custom' function.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRBuilderStatements.cpp:114`
<!-- generated:end DSH4363 -->

**原因。** `break` 或 `continue` 位于构建器正在展开的循环内部的 `if` 中。展开会将循环体转换为节点，并将 `if` 转换为覆盖两个分支的 `Select`——因此两个分支都会执行，而“离开循环”不是其中任何一个分支能做到的事情。

**修复。** 将 `break` 移到循环体的顶层，在那里展开器可以直接停止生成迭代（其条件必须是编译器能够确定的内容，而不是着色器计算的内容）。如果退出确实取决于着色器计算的值，那么该循环不是图循环：将其移入 `/// @custom` 函数。

<a id="dsh4364"></a>

## DSH4364

<!-- generated:begin DSH4364 -->
**严重性** 错误

**消息**

```
A parameter of type {0} cannot be a material function input; the graph has no pin that carries one.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRBuilderMaterial.cpp:251`
<!-- generated:end DSH4364 -->

**原因。** 一个导出函数的参数类型不能作为材质函数输入。`SamplerState` 和用户 `struct` 参数是常见的两种：引擎的 `UMaterialExpressionFunctionInput` 有一个固定的类型列表（标量、三种向量宽度、各种纹理类型、static bool、MaterialAttributes、Substrate），而两者都不在其中。

**修复。** 对于采样器，请去掉该参数：纹理输入自带采样器。对于结构体，请将字段作为单独的参数传递——在 DreamShaderLang 中结构体是编译期聚合体，没有运行期形式可以跨资源边界传递。如果该函数仅从此文件调用，请去掉 `export`：内部辅助函数可以接受任何类型，因为它会被内联。

<a id="dsh4365"></a>

## DSH4365

<!-- generated:begin DSH4365 -->
**严重性** 错误

**消息**

```
A struct has no graph form; pass one of its fields, or move the whole thing into a '/// @custom' function.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRBuilderExpressions.cpp:183`、`Source/DreamShaderLang/Private/IR/IRBuilderExpressions.cpp:536`
<!-- generated:end DSH4365 -->

**原因。** 一个结构体值到达了需要单个图值的位置——一个引脚、一个操作数、一个材质属性。DreamShaderLang 中的 `struct` 是编译期聚合体：它的存在是为了在文件编译期间对值进行分组，随后会被展平，因此它没有单一的节点。

**修复。** 使用你本意所指的字段（`Inputs.Roughness`，而不是 `Inputs`）。若要将整个结构体传递给某个对象，请将被调用方设为内部辅助函数，它会被内联并可以接受该结构体。

<a id="dsh4366"></a>

## DSH4366

<!-- generated:begin DSH4366 -->
**严重性** 错误

**消息**

```
'{0}' is a constant texture, which is a TextureObject node, and this engine has no such material expression; declare it 'uniform'.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRBuilderMaterial.cpp:640`
<!-- generated:end DSH4366 -->

**原因。** `static const` 纹理（在 1.x 中为 `const Texture2D T = Path(...)`）被构建为 TextureObject 节点，而此引擎的内置目录中没有此类。原版引擎都有它；精简过的目录，或从不完整清单加载的目录，可能没有。

**修复。** 将纹理声明为 `uniform`（纹理参数），或者重新导出目录（`dsc export-catalog`），如果使用的是过期的清单。

<a id="dsh4370"></a>

## DSH4370

<!-- generated:begin DSH4370 -->
**严重性** 错误

**消息**

```
'{0}' is read before anything wrote it; a material attribute has no value until this function assigns one.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRBuilderMaterial.cpp:453`
<!-- generated:end DSH4370 -->

**原因。** 在没有任何写入之前读取了材质属性：在某个函数中 `float r = m.Roughness;`，而此前没有 `m.Roughness = ...`。在文件编译期间，`material` 是一个先写后读的记录——没有人连接的引脚不存在“当前值”。

**修复。** 在读取属性之前先赋值。在 `/// @layer` 或 `/// @layerblend` 函数中，传入的材质是真实值，读取它的属性是可以的——这正是 `BreakMaterialAttributes` 的用途——因此层中出现此消息意味着读取的是*结果*材质，而不是输入材质。从节点产生的材质同样也是真实值：`UE.BlendMaterialAttributes(...).Roughness`、材质函数调用的 `material` 结果，或 `m = UE.BlendMaterialAttributes(...)` 之后的 `m`，都是读取该节点的输出，因此此消息从不涉及这些情况。复合赋值（`m.Roughness *= 0.5`）和对单个分量的写入（`m.Normal.z = 1`）会先读取该属性——其他分量必须来自某处——因此对未设置任何值的属性执行其中任一操作也会触发此消息：请先设置整个属性。

<a id="dsh4371"></a>

## DSH4371

<!-- generated:begin DSH4371 -->
**严重性** 错误

**消息**

```
A 'material' value reached a pin that carries {0}; only a MaterialAttributes pin accepts one.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRBuilderExpressions.cpp:173`、`Source/DreamShaderLang/Private/IR/IRBuilderExpressions.cpp:222`
<!-- generated:end DSH4371 -->

**原因。** 一个 `material` 值被传递给了无法接受它的对象：反射节点中携带数字的引脚，或声明为数值类型的函数参数。只有 MaterialAttributes 引脚才接受材质。

**修复。** 传递属性而不是材质（`m.BaseColor`，而不是 `m`），或者将参数类型改为 `material`。当调用确实接受 MaterialAttributes 的材质函数资产时，请检查 `extern` 原型是否将该参数声明为 `material`。

<a id="dsh4372"></a>

## DSH4372

<!-- generated:begin DSH4372 -->
**严重性** 错误

**消息**

```
'{0}' is assigned in only one arm of this 'if' and has no value before it; assign it in both arms, or give it a value before the 'if'.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRBuilder.cpp:624`、`Source/DreamShaderLang/Private/IR/IRBuilder.cpp:648`
<!-- generated:end DSH4372 -->

**原因。** 变量或材质属性在 `if` 或 `?:` 的一个分支中被赋值，而在另一分支中没有，并且在分支之前没有值。两个分支都会变成节点，并由条件在它们之间进行选择，因此未赋值的分支仍然必须有可供选择的内容。

**修复。** 在 `if` 之前为变量赋值（`float x = 0;`），或在两个分支中都赋值。对于材质属性，请在 `if` 之前设置它——先 `m.Roughness = 0.5;`，然后在需要它的分支中覆盖它。在层函数中，读取传入材质会自行提供缺失的分支，因此仅当该属性也没有传入值时才会出现此消息。

当编译器已经能够判断哪个分支会执行时——例如展开的 `for (int i = 0; ...)` 的第一次迭代中的 `if (i > 0)`——而没有值的分支正是不会执行的那个，那么就没有缺失，也不会报告任何内容：在着色器所走的每条路径上该值都来自另一个分支。

<a id="dsh4373"></a>

## DSH4373

<!-- generated:begin DSH4373 -->
**严重性** 错误

**消息**

```
'{0}' is a property of {1} and needs a literal; this argument is computed at run time.
```

**引发自** `Source/DreamShaderLang/Private/IR/IRBuilderExpressions.cpp:1014`
<!-- generated:end DSH4373 -->

**原因。** 绑定到反射节点*属性*上的实参不是字面量。属性在资产生成时就被写入其中（一个 `SceneTextureId`、一个 `ParameterName`、一个坐标索引）；它不是引脚，没有任何地方可以连接计算出的值。

**修复。** 写入字面量：`UE.TexCoord(Index = 0)`，而不是 `UE.TexCoord(Index = SomeUniform)`。一个 `static const` 也可以，因为绑定器会在构建器看到它之前将其折叠。如果值必须变化，请寻找同名引脚——许多类同时拥有两者，而引脚才是接受表达式的那一个。

<a id="dsh4374"></a>

## DSH4374

<!-- generated:begin DSH4374 -->
**严重性** 错误

**消息**

```
This builds a {0}-component value, but a material graph carries at most four components.
```

**由以下抛出** `Source/DreamShaderLang/Private/IR/IRBuilder.cpp:1120`
<!-- generated:end DSH4374 -->

**原因。** 一个构造函数、一个 swizzle 或一次强制转换请求了超过四个分量：`float4(A.rgb, B.rg)` 是五个。材质图的值是 float1 到 float4，`AppendVector` 无法构造出更宽的任何东西。

**修复。** 数一数分量。通常的原因是本意是标量时追加了一个向量——当 `Alpha` 是 float2 时用了 `float4(Colour, Alpha.r)` 而不是 `float4(Colour, Alpha)`。

<a id="dsh4375"></a>

## DSH4375

<!-- generated:begin DSH4375 -->
**严重性** 错误

**消息**

```
The two sides of this branch end with a different whole material, and DreamShader chooses between attribute values, not between whole materials; assign the attributes one at a time on both sides, or mix the two materials with UE.BlendMaterialAttributes.
```

**由以下抛出** `Source/DreamShaderLang/Private/IR/IRBuilderStatements.cpp:846`、`Source/DreamShaderLang/Private/IR/IRBuilderStatements.cpp:850`
<!-- generated:end DSH4375 -->

**原因。** 一个 `if` 的两个分支——或一个 `?:` 的两侧，或一个 `if` 内部的 `return` 及其之后的代码——在同一个变量中留下了不同的*完整*材质。一个分支替换了该材质（`m = UE.BlendMaterialAttributes(...)`、`R = A`，或一次会写回 `inout material` 的材质函数调用），而另一个分支没有，或者每个分支用不同的材质替换了它。分支的降级方式是通过逐个属性在两条分支留下的内容之间进行选择；完整材质不是属性值，而在没有替换材质的那一侧，没人写入的属性没有值可供编译器命名并与之比较。这在下移一个属性时同样成立：`m.MaterialAttributes` 持有一个完整材质，因此在每个分支中向其写入不同的材质同样会触发此错误——绝不是对两个材质进行数值条件判断。

**修复。** 在两条分支中都写入你需要的属性，而不是在其中一条中替换材质。要在两个完整材质之间选择，请混合它们：`m = UE.BlendMaterialAttributes(A = First,
B = Second, Alpha = Mask);`，掩码设为 0 或 1 即可实现该分支的意图。在两个完整材质之间的静态开关（一个针对 MaterialAttributes 的 `StaticSwitch`）在此版本中不会生成。

<a id="dsh4376"></a>

## DSH4376

<!-- generated:begin DSH4376 -->
**严重性** 错误

**消息**

```
'{0}' is read here, but nothing gives it a value on any path that reaches this line; assign it first, or give it an initializer where it is declared.
```

**由以下抛出** `Source/DreamShaderLang/Private/IR/IRBuilderExpressions.cpp:1459`
<!-- generated:end DSH4376 -->

**原因。** 在没有任何通过函数的路径为其赋值的点上读取了局部变量——或对其进行了复合赋值（`x += 1`、`++x`，这会先读取它）：`float x; m.Opacity = x;`。材质图没有“未初始化”这回事：该读取会降级为空，而它所馈入的任何内容都会悄无声息地不被连接。读取没人写入过的结构体局部变量字段（`P.B`）是同样的错误，只是以字段命名。

有意不报告的情况：仅在 `if` 的一个分支中赋值的局部变量（那是 DSH4372，仅一次）；一个参数（没人赋值的 `out` 是绑定器的 DSH6211）；传给 `out` 或 `inout` 实参的局部变量，因为被调用方可能只写入它；以及在编译器可判断永不执行的 `if` 分支内部的读取，例如在展开循环的第一次迭代中的 `if (i > 0)`。每个读取点只报告一次，同一路径上对同一局部变量的后续读取不会再次报告。

**修复。** 在读取之前给局部变量赋值：在声明处使用初始化器（`float x = 0;`），或在每条到达读取的路径上赋值。一次一个分量地写入向量局部变量是可以的——`float3 v; v.x = a; v.y = b;`——因为分量写入从零开始，也就是 1.x 生成器给没有初始化器的声明所赋的值；被拒绝的是在写入任何内容之前读取 `v` 或其某个分量。

<a id="dsh4377"></a>

## DSH4377

<!-- generated:begin DSH4377 -->
**严重性** 错误

**消息**

```
'{0}' writes into the material held in an attribute, and an attribute takes one whole value, not a write to part of it; assign that attribute a whole material, or set the attribute on the material itself.
```

**由以下抛出** `Source/DreamShaderLang/Private/IR/IRBuilderExpressions.cpp:1475`
<!-- generated:end DSH4377 -->

**原因。** 一个赋值写入了某个属性所持有的材质：`m.MaterialAttributes.Roughness = 0.5`。在文件编译期间，`material` 是一个从每个属性到单个值的映射。`m.MaterialAttributes` 所持有的是一个完整的属性集，而该映射在记录内部没有记录，因此对其一部分的写入没有可降级的目标。读取这样的字段是可以的：`=` 右侧的 `m.MaterialAttributes.Roughness` 会从所存储的集合中读取它。

**修复。** 在材质本身之上设置该属性（`m.Roughness = 0.5`），或给该属性一个完整材质，例如 `UE.BlendMaterialAttributes(...)` 的结果。写入普通属性的一个分量（`m.BaseColor.x = 1`）不是此错误：那是对该属性的读取-修改-写入，与局部变量的一个分量相同。

<a id="dsh4378"></a>

## DSH4378

<!-- generated:begin DSH4378 -->
**严重性** 错误

**消息**

```
A branch chooses between two {0} values or between two numbers, and this one has one of each.
```

**由以下抛出** `Source/DreamShaderLang/Private/IR/IRBuilder.cpp:1538`、`Source/DreamShaderLang/Private/IR/IRBuilder.cpp:1556`
<!-- generated:end DSH4378 -->

**原因。** 一个针对 Substrate 值、无法构建的分支。有两条消息：分支的一侧是 Substrate 值，另一侧是数字；或者该分支在运行时决定，这会变成一个 `Substrate.Select` 节点，而此引擎没有该节点（该节点从 Unreal Engine 5.6 起存在）。

**修复。** 给两侧赋予同一种值。在没有 `Substrate.Select` 的引擎上，将条件设为 `/// @static` 的 uniform bool——StaticSwitch 会携带 Substrate 值——或用 `lerp()` 混合这两个值。

<a id="dsh4379"></a>

## DSH4379

<!-- generated:begin DSH4379 -->
**严重性** 错误

**消息**

```
A branch can only choose between numbers, bools and static Substrate values, and these are {0} objects, which no material graph node switches; sample each texture first and branch on the samples.
```

**由以下抛出** `Source/DreamShaderLang/Private/IR/IRBuilder.cpp:1527`
<!-- generated:end DSH4379 -->

**原因。** 一个分支将纹理对象或采样器留在变量中，且每一侧都不相同。没有任何材质图节点能在两个纹理对象之间进行选择，无论条件是否为静态。

**修复。** 对两个纹理都进行采样，并基于采样颜色进行分支；在 `/// @static` 条件下，只有被选中的一侧会进入着色器。

<a id="dsh4380"></a>

## DSH4380

<!-- generated:begin DSH4380 -->
**严重性** 警告

**消息**

```
This branch becomes a 'Substrate.Select', which parameter-blends its two inputs; they are a '{0}' and a '{1}', and the engine may refuse to blend unlike BSDFs.
```

**由以下抛出** `Source/DreamShaderLang/Private/IR/IRBuilder.cpp:1571`
<!-- generated:end DSH4380 -->

**原因。** 运行时的 `if` 或 `?:` 在两种不同类型的一个 Substrate 值之间进行选择——例如一侧是 slab，另一侧是 Unlit。该分支会变成一个 `Substrate.Select` 节点，而 Select 总是对其两个输入进行参数混合；引擎能混合哪些 BSDF 对是引擎的事，它会在材质编译时报告无法混合的那些。这是一个警告：节点已被创建。

**修复。** 如果材质能编译，则无需处理。如果引擎拒绝该对组合，请改为静态选择（在条件上使用 `/// @static` 会创建一个 StaticSwitch，它可以携带任何 Substrate 值），或从同一节点构建两侧。

<a id="dsh4381"></a>

## DSH4381

<!-- generated:begin DSH4381 -->
**严重性** 错误

**消息**

```
'{0}' is converted by the node 'Substrate.{1}', and this engine has no such node; give the pins it feeds directly.
```

**由以下抛出** `Source/DreamShaderLang/Private/IR/IRBuilderSubstrate.cpp:507`
<!-- generated:end DSH4381 -->

**原因。** Substrate 调用的一个虚参 —— `BaseColor` / `Metallic` / `Specular`、
`Haziness`、`Transmittance` / `Thickness` —— 代表 BSDF 前面的一个转换节点
（`Substrate.MetalnessToDiffuseAlbedoF0`、`Substrate.HazinessToSecondaryRoughness`、
`Substrate.TransmittanceToMFP`），而此引擎的目录中没有这样的节点。绑定器
接受了这个参数，因为节点名称是已知的；但构建器找不到任何东西可做。

**修复。** 将转换本应馈入的引脚直接接上（`DiffuseAlbedo` 和 `F0`、
`SecondRoughness` 和 `SecondRoughnessWeight`、`SSSMFP`）。如果引擎确实有这个节点，那么
目录已过期：再次运行 `dsc export-catalog`。

<a id="dsh4382"></a>

## DSH4382

<!-- generated:begin DSH4382 -->
**严重性** 错误

**消息**

```
This material drives FrontMaterial and says 'Substrate = Native', and Substrate is off in this project; the engine could not compile the asset. Turn Substrate on, or write 'Substrate = Bridge' and the legacy attributes.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRBuilderSubstrate.cpp:416`
<!-- generated:end DSH4382 -->

**原因。** 材质写着 `#pragma material(Substrate = Native)`、驱着 `FrontMaterial`，而
此项目中的 Substrate 处于关闭状态（`r.Substrate`，即内置定义 `DS_SUBSTRATE`）。引擎
无法编译这样的资产，因此在这里被拒绝，此处的消息可以说出原因，而不是作为
稍后的着色器错误。

**修复。** 在项目中开启 Substrate，或者针对旧版属性编写材质并
写入 `Substrate = Bridge`：这样在 Substrate 关闭时按原样构建，在开启时构建为一个
`Substrate.ShadingModels` 节点。

<a id="dsh4383"></a>

## DSH4383

<!-- generated:begin DSH4383 -->
**严重性** 错误

**消息**

```
'{0}' is not the Substrate value being built any more when a loop comes round to '{0}.{1}': an earlier trip assigned it. Declare the value inside the loop, or finish it before the loop.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRBuilderSubstrate.cpp:201`、`Source/DreamShaderLang/Private/IR/IRBuilderSubstrate.cpp:247`、`Source/DreamShaderLang/Private/IR/IRBuilderSubstrate.cpp:277`、`Source/DreamShaderLang/Private/IR/IRBuilderSubstrate.cpp:304`
<!-- generated:end DSH4383 -->

**原因。** 一个逐成员构建的 Substrate 值（`Substrate S = Substrate.Slab();
S.Roughness = r;`）遇到了会转回来的循环。绑定器按源代码顺序检查构建器，
看不到循环的第二轮，因此图形的构建器会在四种情况下报告：在循环的早先一轮已经使用了该值之后某个成员被写入（其节点是当时的样子）；
在早先一轮将局部变量赋为整值时某个成员被访问，而整值没有成员；
在写入该成员的行未运行的路径上，某个成员被读取，或作为 `*=` 及其同类表达式的左侧。

**修复。** 在循环内声明该值——在循环体中声明的构建器每一轮
都是新值——或者在循环开始前完成写入。对于后两种情况，在读取成员之前
在其所经的每条路径上都为其赋值。

<a id="dsh4390"></a>

## DSH4390

<!-- generated:begin DSH4390 -->
**严重性** 信息

**消息**

```
'{0}' is declared but nothing reads it, so it is not in the generated material.
```

**引发者** `Source/DreamShaderLang/Private/IR/IRPasses.cpp:872`
<!-- generated:end DSH4390 -->

**原因。** 信息。声明了一个 `uniform` 但文件中没有任何地方读取它，因此没有生成参数
节点，材质也就没有这样的参数。这些遍历在剪除之后报告，因此
它也会对只被本身被证明是死代码的代码读取的 uniform 触发。

**修复。** 删除声明，或使用它。如果本意是让材质实例驱动该参数，
请注意：不馈入任何东西的参数什么也不做：该实例会显示一个没有
效果的旋钮，这就是它不被生成的原因。如果它只在当前关闭的 `#if` 文本内被读取，
该消息是预期的。

<a id="dsh4400"></a>

## DSH4400

<!-- generated:begin DSH4400 -->
**严重性** 错误

**消息**

```
Buffer '{0}' is declared twice; the declaration on line {1} already makes it.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1541`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1555`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1568`
<!-- generated:end DSH4400 -->

**原因。** 一个缓冲区以一个已被占用的名称声明。三条消息：

- 相同的名称之前已作为缓冲区声明过；第一个声明有效，此声明被
  丢弃；
- 该名称与之前声明的缓冲区仅在大小写上不同
  （`Buffer 'mask' differs from 'Mask', declared on line 2, in case only; ...`）：在源代码中名称区分大小写，但管线资产将缓冲区名称保存为 Unreal 名称，
  比较时忽略大小写，因此二者在那里会是同一个缓冲区。第一个声明有效；
- 该名称是管线的 `uniform` 或 `static const`，或是 `DreamPassWeight`
  （`'Mask' already names a parameter or a constant of this pipeline; ...`）：缓冲区、参数和
  常量共用一个命名空间，无论缓冲区在文件中处于什么位置，被拒绝的都是缓冲区。

被拒绝的声明什么也不产生：同名绑定绑定第一个缓冲区；对仅大小写不同的被拒绝拼写的绑定，
或对某参数已有名称的绑定，为 DSH4403。

**修复。** 重命名二者之一，或合并同一缓冲区的两个声明——它们的参数放入一个
列表：`buffer Mask : R8(Clear = 0, Scale = 0.5);`。

<a id="dsh4401"></a>

## DSH4401

<!-- generated:begin DSH4401 -->
**严重性** 错误

**消息**

```
A pass named '{0}' is already declared on line {1}; RDG events and stats are named after passes, so each name is used once.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1811`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1824`
<!-- generated:end DSH4401 -->

**原因。** 同一条管线的两个通道名称相同，或名称仅在大小写上不同
（`Pass 'blur' differs from 'Blur', declared on line 7, in case only; ...`）：管线资产将
通道名称保存为 Unreal 名称，比较时忽略大小写，因此 `Blur` 和 `blur` 在那里会是同一个通道。
运行时会以通道命名其 GPU 事件和统计（`<Pipeline>.<Pass>`），因此一个名称
必须指向一个通道。第二个通道会被整体跳过：在重命名之前，其中的任何内容都不会被检查或构建。

**修复。** 重命名二者之一，最好按其功能命名：`BlurH` 和 `BlurV`，而不是两个
`Blur`。

<a id="dsh4402"></a>

## DSH4402

<!-- generated:begin DSH4402 -->
**严重性** 错误

**消息**

```
'{0}' is a built-in texture a pass binds without declaring it, and cannot be declared as a buffer.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1530`
<!-- generated:end DSH4402 -->

**原因。** 一个 `buffer` 声明使用了内置纹理的名称：`SceneColor`、`SceneDepth`、
`CustomDepth`、`CustomStencil`、`GBufferA` 到 `GBufferF`、`Velocity` 或 `Translucency`。通道无需声明
即按名称绑定这些纹理，因此同名的缓冲区无法与
渲染器的纹理区分开。该声明被丢弃，对该名称的绑定仍然绑定内置纹理。

**修复。** 给缓冲区一个自己的名称。若要复制内置纹理，声明该缓冲区并用一个 `copy` 通道
填充它（`read SceneColor; write Grab;`）；若要网格通道的私有深度，声明
`buffer MaskDepth : Depth32;` 并写入 `Depth = Own(MaskDepth);`。

<a id="dsh4403"></a>

## DSH4403

<!-- generated:begin DSH4403 -->
**严重性** 错误

**消息**

```
'{0}' is neither a buffer of this pipeline nor a built-in texture; declare it with 'buffer {0} : <Format>;'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1509`
<!-- generated:end DSH4403 -->

**原因。** 通道指定了一个不存在的缓冲区：在 `read` 或 `write` 绑定中，在
`Dispatch = <Buffer>`（或 `<Buffer> / n`、`<Buffer> * n`）中，或在 `Depth = Own(<Buffer>)` 中。名称
区分大小写；当已声明的缓冲区或内置纹理仅大小写不同时，消息
会提示它（`read sceneColor;`）。通道只绑定管线自身的缓冲区和内置纹理：
`Texture2D` 参数，或另一条管线的缓冲区，在这里都不是缓冲区。声明被拒绝的缓冲区
（DSH4400）也未被声明，因此其绑定也会落到这里。

**修复。** 声明该缓冲区——`buffer Blurred : R8(Scale = 0.5);`——或更正名称。若要将
纹理参数交给通道，请用材质绘制该通道并通过 `param` 将其交给材质。

<a id="dsh4404"></a>

## DSH4404

<!-- generated:begin DSH4404 -->
**严重性** 错误

**消息**

```
'{0}' is not a 'uniform' or 'static const' of this pipeline; 'Enabled' takes a 'uniform bool' or 'true'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1056`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2608`
<!-- generated:end DSH4404 -->

**原因。** 管线值中的某个名称未声明。有两个地方会引发它：

- `Enabled = <Name>`，在 `#pragma pipeline(...)` 中或在某个 pass 中，既不指向 `uniform`，也不指向
  `static const`。`true` 和 `false` 为小写：`Enabled = True` 是一个名称。
- 一个 `param` 表达式使用了不是 `uniform`、`static const` 或 `DreamPassWeight` 的名称
  （`'Radius' is not a 'uniform' or 'static const' of this pipeline, nor 'DreamPassWeight'.`）：拼错的参数、一个缓冲区（`param Size = Mask;`）、只有该 pass 的着色器或材质
  才知道的名称。

被 DSH3316 拒绝的变量同样不会被声明。当某个已声明的名称仅大小写不同时，消息会
给出建议。

**修复。** 更正名称，或在文件作用域声明该值：为 `Enabled` 声明一个 `uniform bool`，为
`param` 声明一个 `uniform` 或 `static const`。缓冲区通过 `read` 到达 pass，在 HLSL
pass 中它还会将其大小定义为 `<Name>Size`。

<a id="dsh4405"></a>

## DSH4405

<!-- generated:begin DSH4405 -->
**严重程度** 错误

**消息**

```
'{0}' is a '{1}', and 'Enabled' takes a 'uniform bool' or 'true'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1070`
<!-- generated:end DSH4405 -->

**原因。** `Enabled` 在 `#pragma pipeline(...)` 中或在某个 pass 中指向了一个已声明的值，而该值不是
`bool`：一个 `uniform float`、一个 `int`、一个向量。管线或 pass 仅由 bool 切换。

**修复。** 声明一个 `uniform bool` 并为其命名：

```hlsl
#pragma pipeline(Enabled = bShowOutline)

uniform bool bShowOutline = true;
```

管线不需要 `Enabled` 来淡出：它不会以权重 0 运行，并且每个 pass 都会收到该
权重（在材质中为 `DreamPassWeight`，在 `.usf` 中为 `DP_Weight`）。

<a id="dsh4406"></a>

## DSH4406

<!-- generated:begin DSH4406 -->
**严重程度** 错误

**消息**

```
No material '{0}' was found: a bare name is the material a '.dss' under the same source root builds, an object path any material or material instance.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3530`
<!-- generated:end DSH4406 -->

**原因。** 全屏 pass 或网格 pass 的 `Material` 解析不到任何内容。裸名称
（`"PP_Outline"`）会在 `.dsp` 的源根目录下的 DreamShader 源产物中查找——
`.dss` 导出的材质，或一个 `.dsi`——必须完全按拼写匹配，包括大小写；材质
函数不算。对象路径（`"/Game/FX/PP_Outline.PP_Outline"`）必须指向一个存在的资产，
或源根目录下的某个源所构建的资产。该检查需要宿主来查找
材质，编辑器中的编译或通过 `dsc` 的编译会进行此操作；否则会跳过
（DSH7360）。

**修复。** 对照构建该材质的 `export` 检查名称，或写出对象路径，
这是此源根目录之外的材质所需要的——比如手工制作的材质，或另一个插件的
源所构建的材质。由多个源构建的裸名称属于 DSH8330。

<a id="dsh4407"></a>

## DSH4407

<!-- generated:begin DSH4407 -->
**严重程度** 错误

**消息**

```
'{0}' is not a '.usf' or '.ush' file; the engine compiles shader files of those two kinds only.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2026`
<!-- generated:end DSH4407 -->

**原因。** `Shader =` 指向的文件扩展名不是 `.usf` 或 `.ush`（任意大小写）：完全没有扩展名
（`"Passes/Blur"`）、`.hlsl`，或 DreamShader 源（`"PP_Blur.dss"`）。引擎的着色器编译器
只读取这两种文件。

**修复。** 为该着色器文件指定 `.usf` 扩展名，并在路径中写出：
`Shader = "BoxBlur.usf";`。材质不是着色器文件：全屏 pass 使用
`Material = "..."` 绘制一个 `.dss`。

<a id="dsh4408"></a>

## DSH4408

<!-- generated:begin DSH4408 -->
**严重程度** 错误

**消息**

```
The shader file '{0}' does not exist; a path starting with '/' is a virtual shader path, and any other is read from the folder of this '.dsp'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3735`
<!-- generated:end DSH4408 -->

**原因。** `Shader =` 指向的文件不存在。不以 `/` 开头的路径相对于
`.dsp` 所在的文件夹——`"BoxBlur.usf"` 与它相邻，`"../Shared/Blur.usf"` 位于其上一级
文件夹；以 `/` 开头的路径是虚拟着色器路径，只能通过引擎映射的目录
解析（`/Engine/...`、`/Plugin/<Name>/...`，或模块通过
`AddShaderSourceDirectoryMapping` 添加的映射）。在 DSH4406 所在之处检查。

**修复。** 更正路径或移动文件。放在 `.dsp` 旁边是常见做法，且不需要
映射目录：引擎编译的是该文件的快照，而非原始文件。仅对已经位于映射目录中的
着色器使用虚拟路径。

<a id="dsh4410"></a>

## DSH4410

<!-- generated:begin DSH4410 -->
**严重程度** 错误

**消息**

```
'{0}' does not define a function '{1}'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3802`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3830`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3842`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3989`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4023`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4082`
<!-- generated:end DSH4410 -->

**原因。** HLSL pass 的 `Entry` 未指向该 pass 可运行的函数。共有两种消息：

- `'<file>' does not define a function '<Entry>'.` —— 在检查所读取的内容中未定义该名称的函数：
  `.usf`、它通过相对路径包含的每个文件，以及它通过
  映射的着色器目录解析的虚拟路径包含的每个文件（`/Project/...`、插件的 `/Plugin/...`），每个
  文件再依次跟进其自身的包含。引擎自带的头文件（`/Engine/`、`/ThirdParty/`）
  不会被读取。注释和 `#` 行会被跳过，函数在有函数体时才算数，并且名称
  区分大小写。当文件包含了检查无法跟进的内容时——没有
  映射目录覆盖的虚拟路径、不指向任何文件的相对路径、`#include MACRO`——未找到的条目
  不会被报告，因为它可能存在于未被读取的内容中；此时 compute pass 需要
  `Threads`（DSH4411）。
- `'<Entry>' in '<file>' is a compute shader entry ...` —— 一个 `fullscreen` pass 指向的函数
  前面带有 `[numthreads(...)]`。全屏 pass 将其 `Entry` 作为其插槽的
  像素着色器运行，而计算着色器的入口不是像素着色器。

对于写在 `.dsp` 中的 HLSL，会对该块的文本进行同样的检查，无需读取文件：

- `The 'hlsl' block of pass '<Pass>' does not define a function '<Entry>' ...` —— 该 pass 自身的块包含完整的
  函数，但没有一个具有 `Entry` 给出的名称；或者，在没有 `Entry` 时，没有一个函数是 `Main`，即
  块默认的入口。入口是该块自身的函数，而不是该块所包含文件的函数；
- `The file's 'hlsl' block does not define a function '<Entry>' ...`，以及 `... names a function of the file's 'hlsl'
  block, and this '.dsp' has none` -- a pass with no `Shader`，并且它自身没有块来运行该文件的
  块中 `Entry` 所指的入口；
- `'<Entry>' is a compute shader entry ...` —— 同上，针对全屏 pass。

在 DSH4406 所在之处检查，在找到文件后执行；对于 `.dsp` 中的代码，则在管线绑定时执行。

**修复。** 按 HLSL 中函数的拼写来拼写 `Entry` —— `Entry = BlurCS;` —— 并在
`.usf` 或它包含的文件中定义该函数；对于 `.dsp` 中的代码，在查找它所在的块中定义——
即该 pass 自身的块，或当该 pass 既没有 `Shader` 也没有块时该文件的块。全屏 pass 指向其像素着色器函数，
即返回 `SV_Target0` 的那个；要运行 `[numthreads]` 函数，请将该 pass 设为 `compute`。

<a id="dsh4411"></a>

## DSH4411

<!-- generated:begin DSH4411 -->
**严重程度** 错误

**消息**

```
'{0}' in '{1}' has no '[numthreads(x, y, z)]' the compiler can read; write 'Threads = uint3(x, y, z)' in the pass.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3790`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3816`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4069`
<!-- generated:end DSH4411 -->

**原因。** 某个 compute pass 的 `Entry` 具有编译器无法读取的线程组大小，并且该 pass
未提供 `Threads` 来说明这一点。调度按该大小的组来计数。共有两种消息：

- `'<Entry>' in '<file>' has no '[numthreads(x, y, z)]' ...` -- 函数存在，但其前面没有
  `[numthreads(...)]`，或者它的三个值不是普通的整数 -- 通常
  是宏 `[numthreads(THREADGROUP_SIZE, THREADGROUP_SIZE, 1)]`。
- `'<Entry>' is not in what the compiler could read of '<file>', which includes a file it cannot
  follow ...` -- 未找到该函数，并且文件中包含了检查无法读取的内容
  （DSH4410 列出了具体内容），因此该函数可能定义在那里，其组大小无人读取。
- `'<Entry>' has no '[numthreads(x, y, z)]' in front of it ...` -- 与第一个相同，针对以
  `.dsp` 编写的入口。

**修复。** 在着色器中以数字形式写入组大小 -- `[numthreads(8, 8, 1)]` -- 或者保留宏，
并在 pass 中给出相同的大小：`Threads = uint3(8, 8, 1);`。对于第二条消息，同时给出
`Threads`，或者以编译器能够跟随的路径包含该文件。

<a id="dsh4412"></a>

## DSH4412

<!-- generated:begin DSH4412 -->
**严重性** 错误

**消息**

```
'{0}' is not a pass layer of this project; the layers are the first {1} names of Project Settings > DreamPlugin > DreamShader Custom Pass > Layer Names.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3510`
<!-- generated:end DSH4412 -->

**原因。** 网格 pass 的 `Filter` 通过 `Layer(...)` 选择了一个项目并不具有的名称。Pass
层是项目自己的：Project Settings ▸
DreamPlugin ▸ DreamShader Custom Pass 的 **Layer Names** 列表的前 32 个名称，即消息中所指出的；只有这些名称具有位，因此列表中更靠后的名称也不是层。名称的比较区分大小写，当某个层仅在其大小写上与之不同时，消息会改为建议那一个
（`'<Layer>' is not a pass layer of this project; did you mean '<Name>'?`）。每个
未知名称产生一个错误，位于 `Filter` 行。在 DSH4406 处检查。

**修复。** 将该层添加到该列表中，位于其前 32 个名称之内，或者更正该名称。列表的
顺序即层的位顺序，因此请将新名称添加到末尾：移动某个名称会重新定位每个已编译的
、引用它的管线，直到该管线再次编译。`List(Name)` 在此处不检查 -- 列表
在运行时由 `UDreamPassSubsystem::AddToList` 填充。

<a id="dsh4413"></a>

## DSH4413

<!-- generated:begin DSH4413 -->
**严重性** 错误

**消息**

```
'Threads = uint3({0}, {1}, {2})' disagrees with '[numthreads({3}, {4}, {5})]' of '{6}', and the dispatch would be sized for groups the shader does not have; leave 'Threads' out, or write the same numbers.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3775`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4056`
<!-- generated:end DSH4413 -->

**原因。** 一个计算 pass 写入 `Threads = uint3(...)`，而其入口的 `[numthreads(...)]`
（编译器可以读取 -- 在 `.usf` 中，或者当入口编写在那里时在 `.dsp` 中）表示的是其他
数字（消息中给出了两者）。组数是
根据 `Threads` 计算出来的，而着色器以 `[numthreads]` 的组运行，因此该 pass 会覆盖
其缓冲区过多或过少的内容。`Threads = uint2(x, y)` 计为 `z = 1`。

**修复。** 省略 `Threads`：组大小从 `[numthreads]` 读取。`Threads` 用于组大小是宏的入口
（DSH4411），此时它声明宏所给出的大小。

<a id="dsh4414"></a>

## DSH4414

<!-- generated:begin DSH4414 -->
**严重性** 错误

**消息**

```
'DreamPassWeight' is the pipeline's weight in a view, which every pass can read as 'param P = DreamPassWeight'; it cannot be declared.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:838`
<!-- generated:end DSH4414 -->

**原因。** `.dsp` 的 `uniform` 或 `static const` 被命名为 `DreamPassWeight`。该名称是
管线在视图中的权重 -- 其激活中的最大权重，也就是体积的淡入
到达特效的方式 -- 由运行时提供。该声明会被丢弃。该名称的缓冲区
DSH4400。

**修复。** 重命名该声明。权重不需要：每个 pass 都会收到它，材质以标量参数 `DreamPassWeight` 的形式接收，
`.usf` 以 `DP_Weight` 的形式接收，
`param Fade = DreamPassWeight * 0.5;` 则以另一个名称、按比例将其传递下去。

<a id="dsh4415"></a>

## DSH4415

<!-- generated:begin DSH4415 -->
**严重性** 错误

**消息**

```
'{0}.Previous': a built-in texture keeps no history.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2497`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2507`
<!-- generated:end DSH4415 -->

**原因。** `read` 命名了 `.Previous`，即某个不保留内容的纹理的上一帧内容。两
条消息：

- 内置纹理，如 `read SceneColor.Previous;` 中那样 -- 渲染器的纹理仅保留当
  前帧；
- 管线中未声明 `History = true` 的缓冲区
  （`'Mask.Previous' reads last frame's 'Mask', and 'Mask' keeps none; ...`）。

**修复。** 对于缓冲区，在其声明中添加 `History = true`。对于内置纹理，请保留一份
自己的、带历史的副本，并读取该副本的 `.Previous`：

```hlsl
buffer LastColor : RGBA16F(History = true);

pass Keep : copy
{
    Injection = BeforePostProcess;
    read  SceneColor;
    write LastColor;
}
```

之后，pass 使用 `read Prev = LastColor.Previous;` 读取上一帧的画面。
