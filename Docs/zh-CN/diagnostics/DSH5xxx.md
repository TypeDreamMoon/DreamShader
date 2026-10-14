---
title: "DSH5xxx --- 内置项 -- UE.*、数学、Substrate"
description: "DSH5xxx --- 内置项 -- UE.*、数学、Substrate"
---

<a id="dsh5xxx-----builtins----ue-math-substrate"></a>

# DSH5xxx --- 内置项 -- UE.*、数学、Substrate

> 生成标记之间的块由 `.skill/gen-diagnostics.ps1` 写入。
> 标记下方的所有内容均为手工编写，重新生成后仍会保留。

<a id="dsh5200"></a>

## DSH5200

<!-- generated:begin DSH5200 -->
**严重性** 错误

**消息**

```
A material has no '{0}' pin; did you mean '{1}'? Attribute names are case-sensitive.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1076`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1087`
<!-- generated:end DSH5200 -->

**原因。** `m.<Name>` 未指定任何材质引脚。该表来自引擎，因此消息可能会推荐一个仅在大小写上不同的拼写——这是最常见的原因，因为 1.x 对此类匹配不区分大小写，而 2.0 区分。

**修复。** 使用精确的拼写。`EmissiveColor`、`BaseColor`、`Roughness`、`Normal`、`WorldPositionOffset`、`FrontMaterial` 等保留其引擎名称；1.x 的简写形式（`Emissive`、`AO`、`WPO`、`PDO`）在目录中作为别名保留。

<a id="dsh5201"></a>

## DSH5201

<!-- generated:begin DSH5201 -->
**严重性** 错误

**消息**

```
This Custom node declares no output called '{0}'; its outputs are '{1}'. An output is declared by 'AdditionalOutputs'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1169`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1223`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1429`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1832`
<!-- generated:end DSH5201 -->

**原因。** 三种形态，同一个代码——全都表示“此节点的输出需要命名”。消息列出了该类所拥有的输出。

- `.Name` 在多输出节点上未指定该类的任何输出。
- 多输出节点被用作值而未指定输出，并且它不是该位置所需的值。节点在其**默认**（第一个）输出的类型恰好为所需类型时采用该输出。其输出全为同一值的**通道视图**的节点——`UE.VertexColor()` 的 `RGB`、`R`、`G`、`B`、`A`，全部截取自同一个 `float4`——在该值的类型恰好为所需类型时，也代表该整个值，因此 `float4 VC = UE.VertexColor();` 的绑定方式与 1.x 相同（当默认输出位于该值之前时，如同 `RGB` 那样）。其输出为不同值的节点——`UE.SceneTexture(...)` 的 `Color`、`Size`、`InvSize`——没有可代表的整体：除其默认输出的精确类型之外的任何内容都必须指定输出。
- 在通道视图节点上的 swizzle 请求了位于不同输出上的通道（`UE.VertexColor().ga`：`G` 和 `A` 是独立的引脚）。某个视图精确发布的 swizzle 就是该视图（`.a` 是 `A`），而默认输出内的 swizzle 是对其的 swizzle（`.rg`）；跨视图则没有单一的引脚可读取。

**修复。** 指定所列输出之一——`UE.SceneTexture(SceneTextureId = PostProcessInput0).Color`、`UE.VertexColor().A`——或一次读取一个输出的通道并将它们合并（`float2(UE.VertexColor().g, UE.VertexColor().a)`；两次相同的调用即为一个节点）。`dsc export-catalog` 写出每个类及其引脚和输出；未命名的掩码输出列在其保留的通道之下。

<a id="dsh5202"></a>

## DSH5202

<!-- generated:begin DSH5202 -->
**严重性** 警告

**消息**

```
The builtin catalog is empty, so no 'UE.' expression and no material attribute can be resolved; export it with 'dsc export-catalog'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:287`
<!-- generated:end DSH5202 -->

**原因。** 绑定器被赋予了空的内置项目录，因此无法解析任何 `UE.` 节点和材质引脚，所有指定它们的项也会报告。对于在编辑器之外运行的工具而言这是正常的；在编译内部则意味着目录未被加载。

**修复。** 使用 `dsc export-catalog` 导出目录，并让工具指向它。文件中的其他内容不受影响——符号索引仍会生成。

<a id="dsh5210"></a>

## DSH5210

<!-- generated:begin DSH5210 -->
**严重性** 错误

**消息**

```
'{0}.{1}' is not a node; did you mean '{0}.{2}'? Node names are case-sensitive.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4270`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4282`
<!-- generated:end DSH5210 -->

**原因。** `UE.X` / `Substrate.X` 未指定此引擎拥有的任何类。节点名称**区分大小写**；会推荐仅大小写匹配的名称。

**修复。** 更正拼写，或使用 `UE.Expression(Class = "MaterialExpressionX")` 通过其引擎名称访问该类。`Substrate.` 仅解析 Substrate 表。

<a id="dsh5211"></a>

## DSH5211

<!-- generated:begin DSH5211 -->
**严重性** 错误

**消息**

```
'{0}.{1}' is a node and has to be called: write '{0}.{1}(...)'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1019`
<!-- generated:end DSH5211 -->

**原因。** `UE.X` 在编写时没有参数列表。节点通过调用它来创建，即使它不接受任何内容。

**修复。** 添加括号：`UE.Time()`。

<a id="dsh5212"></a>

## DSH5212

<!-- generated:begin DSH5212 -->
**严重性** 错误

**消息**

```
'{0}' is not a material expression class this engine has.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4204`
<!-- generated:end DSH5212 -->

**原因。** `Class` 参数未指定任何材质表达式类。类名可带或不带 `MaterialExpression` 前缀，也可作为完整的 `/Script/Engine.…` 路径。

**修复。** 对照导出的目录检查拼写。

<a id="dsh5213"></a>

## DSH5213

<!-- generated:begin DSH5213 -->
**严重性** 错误

**消息**

```
'{0}.{1}' has no pin or property called '{2}'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4708`
<!-- generated:end DSH5213 -->

**原因。** 反射节点的命名参数既不匹配输入引脚，也不匹配该类的反射属性。先尝试引脚，再尝试属性；对任一者仅大小写匹配的名称会被推荐。

**修复。** 更正名称。应写入节点而非连接到节点的字面量通常具有以 `Const` 为前缀的属性名——`ConstA` 与 `A` 引脚相邻。

<a id="dsh5214"></a>

## DSH5214

<!-- generated:begin DSH5214 -->
**严重性** 错误

**消息**

```
{0} expects {1}, and this is {2}.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1939`
<!-- generated:end DSH5214 -->

**原因。** 值不适合其所馈入的引脚：宽度错误，或引脚不接受某种类型（将数字馈入 MaterialAttributes 引脚，将纹理馈入数值引脚）。对于纹理采样的坐标和 mip 级别也会引发。

**修复。** 匹配引脚的类型——消息中已指明。不约束其宽度的引脚接受到达的任何内容。

<a id="dsh5215"></a>

## DSH5215

<!-- generated:begin DSH5215 -->
**严重性** 错误

**消息**

```
'{0}' is not a value of '{1}' on '{2}'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4066`
<!-- generated:end DSH5215 -->

**原因。** 枚举属性的值不是该枚举的值之一。会推荐仅大小写匹配的名称。

**修复。** 使用所列拼写之一，书写时不带枚举的前缀（`SamplerType = Normal`，而非 `SAMPLERTYPE_Normal`）。

<a id="dsh5216"></a>

## DSH5216

<!-- generated:begin DSH5216 -->
**严重性** 错误

**消息**

```
'Substrate.' already names the node, so it takes no 'Class' argument.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4176`
<!-- generated:end DSH5216 -->

**原因。** 向 `Substrate.` 调用提供了 `Class` 参数。`Substrate.` 前缀已经指定了该类。

**修复。** 去掉 `Class` 参数，或将调用改写为 `UE.Expression(Class = "…")`。

<a id="dsh5217"></a>

## DSH5217

<!-- generated:begin DSH5217 -->
**严重性** 错误

**消息**

```
'Class' takes the expression class as a quoted string.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4184`
<!-- generated:end DSH5217 -->

**原因。** `Class` 参数不是绑定器可读取的字面量。它接受带引号的字符串、裸标识符和点分名称。

**修复。** 书写为 `Class = "MaterialExpressionConstant3Vector"`。

<a id="dsh5218"></a>

## DSH5218

<!-- generated:begin DSH5218 -->
**严重性** 错误

**消息**

```
'UE.Expression' reaches a node this language has no name for, so it needs 'Class = "MaterialExpressionName"'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4217`
<!-- generated:end DSH5218 -->

**原因。** `UE.Expression(...)` 没有 `Class`。`Expression` 是该语言没有名称的节点的逃生舱口，因此类是其唯一无法推断的东西。

**修复。** 添加 `Class = "MaterialExpressionName"`，或使用节点的短名称调用它。

<a id="dsh5219"></a>

## DSH5219

<!-- generated:begin DSH5219 -->
**严重性** 警告

**消息**

```
'{0}.{1}' leaves its required '{2}' pin unconnected; unless the node reads a default for it, the engine reports it when the material compiles.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4777`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:416`
<!-- generated:end DSH5219 -->

**原因。** 引擎绘制为必需的一个引脚未连接，且其字面量对应项也未设置。这是一个警告而非错误：`required` 仅表示该属性没有 `RequiredInput = "false"`。大多数节点在该引脚悬空时确实无法编译，但有些会改为读取默认值（MakeMaterialAttributes、切换器各层级输入、从未设置该元数据的引擎分支节点），只有材质编译才知道。若某类的节点按设置显示引脚，则其完全没有静态必需的引脚。

**修复。** 连接该引脚，或将其 `Const...` 属性设为字面量。如果该节点本来就应当在引脚悬空的情况下运行，则无需更改；`dsc check -Shaders` 会编译材质并报告一个真正的 `missing input`。

<a id="dsh5220"></a>

## DSH5220

<!-- generated:begin DSH5220 -->
**严重性** 错误

**消息**

```
'{0}.{1}' takes named arguments: write 'Pin = value'.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4401`
<!-- generated:end DSH5220 -->

**原因。** 向一个没有规范参数顺序的节点传递了位置参数。大多数反射类都是仅具名参数——引擎类恰好具有的引脚顺序并非契约。

**修复。** 命名该参数：`Pin = value`。

<a id="dsh5221"></a>

## DSH5221

<!-- generated:begin DSH5221 -->
**严重性** 错误

**消息**

```
'{0}.{1}' takes {2} arguments in order; name the rest.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4418`
<!-- generated:end DSH5221 -->

**原因。** 位置参数多于节点规范顺序所定义的数量。

**修复。** 为其余参数命名。

<a id="dsh5223"></a>

## DSH5223

<!-- generated:begin DSH5223 -->
**严重性** 错误

**消息**

```
'{0}' is abstract and cannot be made into a node.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4310`
<!-- generated:end DSH5223 -->

**原因。** 该类为抽象类，无法实例化。

**修复。** 使用其某个具体子类。

<a id="dsh5224"></a>

## DSH5224

<!-- generated:begin DSH5224 -->
**严重性** 错误

**消息**

```
'{0}' is an enumerated property; write one of its values, as in 'SamplerType = Normal'.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4026`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4097`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4139`
<!-- generated:end DSH5224 -->

**原因。** 向字面量属性赋予了非字面量的内容：绑定器无法折叠的表达式，或需要数字处却用了单词。属性在构建图时即被写入节点，因此其值必须在那一刻就已存在。

**修复。** 写字面量，或写一个可折叠为字面量的 `static const` 表达式。必须计算的值属于引脚，而非属性。

<a id="dsh5230"></a>

## DSH5230

<!-- generated:begin DSH5230 -->
**严重性** 错误

**消息**

```
The first argument of a texture sample is the texture, and this is {0}.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:5033`
<!-- generated:end DSH5230 -->

**原因。** `Texture2DSample` 的第一个参数不是纹理。

**修复。** 传入该纹理——一个 `uniform Texture2D`，或该函数的纹理参数。

<a id="dsh5231"></a>

## DSH5231

<!-- generated:begin DSH5231 -->
**严重性** 错误

**消息**

```
The second argument of 'Texture2DSample' is the sampler, and this is {0}.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:5049`
<!-- generated:end DSH5231 -->

**原因。** `Texture2DSample` 的第二个参数不是采样器。

**修复。** 传入 `<TextureName>Sampler`，即翻译器与该纹理配对的采样器，或使用更短的 `Tex.Sample(UV)`，它会从纹理本身获取。

<a id="dsh5250"></a>

## DSH5250

<!-- generated:begin DSH5250 -->
**严重性** 错误

**消息**

```
Expected 'OutputIndex' to be a whole number of zero or more, found '{0}'.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1044`
<!-- generated:end DSH5250 -->

**原因。** `OutputIndex` 按位置选择 1.x 调用的输出，而其值不是从 0 开始的整数。

**修复。** 写入 `OutputIndex = 1`，或命名该输出：`Output = "Alpha"`。

<a id="dsh5251"></a>

## DSH5251

<!-- generated:begin DSH5251 -->
**严重性** 错误

**消息**

```
Expected 'Output' to name an output with a quoted name or an identifier, found '{0}'.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1065`
<!-- generated:end DSH5251 -->

**原因。** `Output` 按名称选择 1.x 调用的输出，而其值既不是带引号的字符串也不是标识符。

**修复。** 写入 `Output = "RGB"`。

<a id="dsh5252"></a>

## DSH5252

<!-- generated:begin DSH5252 -->
**严重性** 错误

**消息**

```
Expected either 'Output' or 'OutputIndex' on this call, found both.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1033`
<!-- generated:end DSH5252 -->

**原因。** 一个 1.x 调用同时携带 `Output` 和 `OutputIndex`。它们是选择同一个输出的两种方式。

**修复。** 保留其中之一。

<a id="dsh5253"></a>

## DSH5253

<!-- generated:begin DSH5253 -->
**严重性** 错误

**消息**

```
Expected an output selector only on a call that has outputs, found one on the constructor '{0}'.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1081`
<!-- generated:end DSH5253 -->

**原因。** 输出选择器（`Output` / `OutputIndex`）位于诸如 `float3(...)` 之类的构造函数上，而该类没有输出。

**修复。** 移除选择器，或使用 swizzle 取出该分量。

<a id="dsh5254"></a>

## DSH5254

<!-- generated:begin DSH5254 -->
**严重性** 警告

**消息**

```
'{0}' takes no positional argument here; 1.x ignored it, so it is dropped.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1096`、`Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1105`
<!-- generated:end DSH5254 -->

**原因。** 向某个 1.x 调用写法传递了它不会读取的参数：向仅接受具名参数的写法传递了位置参数，或传递了不属于它的具名参数。1.x 会无声地跳过两者，因此节点在构建时未包含该值；前端也会将其丢弃并给出提示。

**修复。** 按该写法读取参数的方式命名它（`Pin = value`），或将其删除。

<a id="dsh5255"></a>

## DSH5255

<!-- generated:begin DSH5255 -->
**严重性** 错误

**消息**

```
Expected 'UE.SceneTexture' to take exactly one argument, 'Id = ...', found {0} argument(s).
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1287`
<!-- generated:end DSH5255 -->

**原因。** 1.x 简写 `UE.SceneTexture(Id = ...)` 仅接受其场景纹理 id，别无其他。

**修复。** 写入 `UE.SceneTexture(Id = PPI_SceneColor)`；该节点的任何其他属性都需要 `UE.Expression(Class = "SceneTexture", ...)`。

<a id="dsh5256"></a>

## DSH5256

<!-- generated:begin DSH5256 -->
**严重性** 错误

**消息**

```
Expected 'SampleTexture2D' to take exactly two positional arguments, a texture and coordinates, found {0} argument(s).
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1214`
<!-- generated:end DSH5256 -->

**原因。** 1.x 简写 `SampleTexture2D(Texture, UV)` 恰好接受一个纹理和坐标。

**修复。** 仅传入这两者，别无其他；若要指定 mip 级别或导数，请使用 `UE.Expression(Class = "TextureSample", ...)`。

<a id="dsh5257"></a>

## DSH5257

<!-- generated:begin DSH5257 -->
**严重性** 错误

**消息**

```
Expected 'UE.StaticSwitchParameter' to name its parameter with 'Name = "..."', found no name.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1319`
<!-- generated:end DSH5257 -->

**原因。** `UE.StaticSwitchParameter(...)` 没有 `Name`。该名称是实例所覆盖的参数，也是使两次调用成为同一个切换器的依据。

**修复。** 添加 `Name = "Use Detail"`。

<a id="dsh5258"></a>

## DSH5258

<!-- generated:begin DSH5258 -->
**严重性** 错误

**消息**

```
Expected the static switch '{0}' to be called with a 'True = ...' and a 'False = ...' input, found at most one of them.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1157`、`Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1344`
<!-- generated:end DSH5258 -->

**原因。** 调用静态切换器时未提供其两侧。该节点在 `True` 和 `False` 输入之间进行选择；若缺少其中之一，便无从选择。

**修复。** 传入 `True = ...` 和 `False = ...`。

<a id="dsh5259"></a>

## DSH5259

<!-- generated:begin DSH5259 -->
**严重性** 错误

**消息**

```
Expected every argument of the parameter call '{0}' to name an input pin, found a positional argument.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1185`
<!-- generated:end DSH5259 -->

**原因。** 一个作为参数节点（`TextureSampleParameter2D`、`StaticSwitchParameter`……）的属性被以位置参数调用。此类调用就是该节点及其已连线的引脚，而引脚按名称区分。

**修复。** 为每个参数命名：`MyTex(Coordinates = uv)`。

<a id="dsh5260"></a>

## DSH5260

<!-- generated:begin DSH5260 -->
**严重性** 错误

**消息**

```
Expected a name after '::', found {0}.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:560`
<!-- generated:end DSH5260 -->

**原因。** `Namespace::` 后面没有跟函数名。

**修复。** 写入 `N::F(...)`。

<a id="dsh5261"></a>

## DSH5261

<!-- generated:begin DSH5261 -->
**严重性** 错误

**消息**

```
Expected 'OutputType' of a Custom expression to be float1 to float4 or MaterialAttributes, found '{0}'.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1563`
<!-- generated:end DSH5261 -->

**原因。** `UE.Expression(Class = "Custom", ...)` 调用的 `OutputType` 不是 Custom
节点可以返回的类型。

**修复。** 使用 `float1` .. `float4`（或 `float` .. `float4`），或 `MaterialAttributes`。

<a id="dsh5263"></a>

## DSH5263

<!-- generated:begin DSH5263 -->
**严重性** 错误

**消息**

```
Expected 'Default' of 'UE.StaticSwitchParameter' to be true or false, found '{0}'.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1364`
<!-- generated:end DSH5263 -->

**原因。** `UE.StaticSwitchParameter(...)` 调用的 `Default` 不是 `true` 或 `false`。

**修复。** 编写 `Default = true`。

<a id="dsh5264"></a>

## DSH5264

<!-- generated:begin DSH5264 -->
**严重性** 错误

**消息**

```
Expected 'SortPriority' of 'UE.StaticSwitchParameter' to be a whole number, found '{0}'.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1378`
<!-- generated:end DSH5264 -->

**原因。** `UE.StaticSwitchParameter(...)` 调用的 `SortPriority` 不是整数。

**修复。** 编写一个整数。

<a id="dsh5265"></a>

## DSH5265

<!-- generated:begin DSH5265 -->
**严重性** 错误

**消息**

```
Expected the parameter call '{0}' to take only its inputs, found an output selector.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1125`
<!-- generated:end DSH5265 -->

**原因。** 对参数节点属性的调用带有 `Output` / `OutputIndex`。这种调用会被
重写为引脚已连线的节点，而选择器无处可去。

**修复。** 在调用后用成员选择输出：`MyTex(Coordinates = uv).rgb`。

<a id="dsh5275"></a>

## DSH5275

<!-- generated:begin DSH5275 -->
**严重性** 警告

**消息**

```
'{0}' matches '{1}' only in case; a 1.x source is read ignoring case, so this is '{1}', and a '.dss' needs the exact spelling.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:83`
<!-- generated:end DSH5275 -->

**原因。** 规则 L19。1.x 源码中的名称仅在忽略大小写时才匹配声明。1.x
在比较大多数名称时忽略大小写，因此源码可以构建；而 `.dss` 区分大小写。

**修复。** 按声明的方式拼写名称。`dsc migrate` 会为每个此类名称执行此操作。

<a id="dsh5276"></a>

## DSH5276

<!-- generated:begin DSH5276 -->
**严重性** 警告

**消息**

```
'{0}' matches the engine name '{1}' only in case; 1.x matched engine names ignoring case, so this is '{1}', and a '.dss' needs the exact spelling.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:95`
<!-- generated:end DSH5276 -->

**原因。** 规则 L19，针对引擎名称：`UE.` 类、引脚、属性或输出仅在
忽略大小写时才匹配。1.x 在查找这些内容时忽略大小写。

**修复。** 使用引擎的拼写，消息中会显示。`dsc migrate` 会写出它。

<a id="dsh5277"></a>

## DSH5277

<!-- generated:begin DSH5277 -->
**严重性** 警告

**消息**

```
'{0}' is the GLSL spelling of '{1}'; a 1.x source may use it and it is read as '{1}', and a '.dss' writes '{1}'.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3020`
<!-- generated:end DSH5277 -->

**原因。** 规则 L2。1.x 源码中使用了 GLSL 函数或类型拼写（`mix`、`fract`、`mod`、
`vec3`）。1.x 会重写它们；而 `.dss` 仅接受 HLSL。

**修复。** 编写消息给出的 HLSL 名称。`dsc migrate` 会执行此操作。

<a id="dsh5278"></a>

## DSH5278

<!-- generated:begin DSH5278 -->
**严重性** 警告

**消息**

```
'{0}' is the 1.x name of the '{1}' space of '{2}'; a '.dss' writes '{1}'.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1492`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4052`
<!-- generated:end DSH5278 -->

**原因。** 规则 L12。枚举值按引擎的方式编写（`PPI_SceneColor`、
`TRANSFORM_Tangent`、`SAMPLERTYPE_Color`），或带有其 `Enum::` 作用域。1.x 匹配枚举项时
较为宽松——不考虑前缀、作用域和大小写——而此前端解析相同的值；`.dss`
采用目录列出的简短形式。第二条消息是由仅 1.x 拥有的名称表示的变换空间
（`AbsoluteWorld`、`CameraRelativeWorld`、`FirstPerson`、`Particle`、`InstanceParticle`），
前端会将其写为引擎的形式——参见[变换内置函数](../builtins/transform.md)。

**修复。** 按消息中的拼写编写该值。

<a id="dsh5279"></a>

## DSH5279

<!-- generated:begin DSH5279 -->
**严重性** 警告

**消息**

```
'{0}.{1}' leaves its required '{2}' pin unconnected, which 1.x allowed and the engine reports when the material compiles.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4761`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:403`
<!-- generated:end DSH5279 -->

**原因。** 规则 L13。某个 `UE.` 节点在 1.x 主体中构建时，有一个引脚保持打开，而引擎将该引脚绘制为
必需。1.x 仅针对少数手工注册的内置函数检查这一点（变换的 `Input`
就是其中之一），许多此类源码能够编译，因为节点会读取默认值。在
`.dss` 中，同样的情况是 DSH5219，也是警告。

**修复。** 如果材质因“缺少输入”而编译失败，则连接该引脚；否则无需
更改。

<a id="dsh5280"></a>

## DSH5280

<!-- generated:begin DSH5280 -->
**严重性** 错误

**消息**

```
'{0}' has no output to select, and this call selects '{1}'.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:592`
<!-- generated:end DSH5280 -->

**原因。** 1.x 调用在不具有输出的对象（无结果的辅助函数，或既非节点也非
函数的调用）上选择输出（`Output = ...`、`OutputIndex = ...`，或最后传入的
接收者）。

**修复。** 移除选择器，或调用具有该输出的对象。

<a id="dsh5281"></a>

## DSH5281

<!-- generated:begin DSH5281 -->
**严重性** 错误

**消息**

```
An output is selected by a whole number the compiler knows, and this index is computed.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1547`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1582`
<!-- generated:end DSH5281 -->

**原因。** `node[k]` 读取节点的输出 `k`，而 `k` 不是编译器能够
求值的常量。连接哪条连线不能取决于运行时值。

**修复。** 使用字面量索引，或命名该输出。

<a id="dsh5282"></a>

## DSH5282

<!-- generated:begin DSH5282 -->
**严重性** 错误

**消息**

```
This Custom node declares {0} output(s), counted from 0, and this selects output {1}.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1593`、`Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1613`、`Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:611`
<!-- generated:end DSH5282 -->

**原因。** 输出索引超出了节点或函数声明的输出数量。输出从 0 开始计数；
Custom 节点的返回值位于索引 0，其额外输出位于其后。

**修复。** 使用范围内的索引，或命名该输出。

<a id="dsh5283"></a>

## DSH5283

<!-- generated:begin DSH5283 -->
**严重性** 信息

**消息**

```
'{0}' is not declared, and as in 1.x it is declared here as a local of type {1} receiving '{2}' of '{3}'.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:652`
<!-- generated:end DSH5283 -->

**原因。** 规则 L5。1.x 语句调用传入了一个未在任何地方声明的接收者（`F(a, Out)`）。
1.x 会就地用它所接收输出的类型声明它，前端也做同样的
事。

**修复。** 无需更改。`dsc migrate` 会写出该声明。

<a id="dsh5284"></a>

## DSH5284

<!-- generated:begin DSH5284 -->
**严重性** 错误

**消息**

```
'{0}' becomes an input of the custom node, which carries a number or a texture, and this argument is {1}.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4601`
<!-- generated:end DSH5284 -->

**原因。** `Custom` 节点调用的某个参数是 Custom 节点输入无法携带的值：
材质、Substrate 值、采样器。Custom 输入接受数字或纹理。

**修复。** 传入代码所需的组件（`m.BaseColor`），而不是整个材质。

<a id="dsh5285"></a>

## DSH5285

<!-- generated:begin DSH5285 -->
**严重性** 错误

**消息**

```
'{0}.{1}' has {2} input pin(s), counted from 0, and this argument connects pin {3}.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4376`
<!-- generated:end DSH5285 -->

**原因。** `Pin[k] = value` 通过引擎索引命名输入引脚，而该类的引脚数量
少于该索引。

**修复。** 使用该类具有的索引，或命名该引脚。

<a id="dsh5286"></a>

## DSH5286

<!-- generated:begin DSH5286 -->
**严重性** 错误

**消息**

```
'Pin[{0}] = ...' connects a node's input pin by its engine index, and only a 'UE.' or 'Substrate.' node call has one; pass this argument by name or by position.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2925`
<!-- generated:end DSH5286 -->

**原因。** `Pin[k] = value` 被用于既非 `UE.` / `Substrate.` 节点的调用：用户函数、
内置函数、构造函数。只有反射节点才具有引擎引脚索引。

**修复。** 按名称或位置传入参数。

<a id="dsh5287"></a>

## DSH5287

<!-- generated:begin DSH5287 -->
**严重性** 信息

**消息**

```
'{0}' has more than one output and is read as its first, '{1}', which is what 1.x did; a '.dss' names the output.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1814`
<!-- generated:end DSH5287 -->

**原因。** 规则 L3c。一个具有多个输出的节点在 1.x 主体中被用作值时，没有说明
使用哪个输出。1.x 读取第一个输出；本前端也是如此，并且会说明是哪一个。它会在
目录未给该输出指定宽度时这样做，并且在该节点被直接传递到引脚时
（`UE.TransformPosition(UE.WorldPosition(), ...)` 接收 `XYZ`）也会这样做，因为 1.x 原本就是这样连接它的，
而引脚会接收它使用的内容 *（2.0.0 – 2.1.0 以 `DSH5201` 拒绝了引脚情况）*。

**修复。** 无需更改。`.dss` 会指定输出（`UE.ScreenPosition().ViewportUV`），并且
`dsc migrate` 会写入它。

<a id="dsh5288"></a>

## DSH5288

<!-- generated:begin DSH5288 -->
**严重性** warning

**消息**

```
'{0}.{1}' has no 'DefaultValue', so the default written for this parameter is dropped, as 1.x dropped it.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4628`
<!-- generated:end DSH5288 -->

**原因。** 1.x 属性是一个不带 `DefaultValue` 属性的参数节点（例如集合参数），
并且在 `=` 之后写有默认值。1.x 丢弃了它。

**修复。** 移除默认值；在节点接收它的位置（集合资产）设置该值。

<a id="dsh5289"></a>

## DSH5289

<!-- generated:begin DSH5289 -->
**严重性** info

**消息**

```
{0} expects {1}, and this is {2}: its leading components are taken, which is what 1.x did; a '.dss' writes the swizzle.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1877`
<!-- generated:end DSH5289 -->

**原因。** 规则 L22。一个值比它所去的位置更宽——例如将 `float4` 参数赋给
`vec3` 变量、作为 `float3` 实参传递——而 1.x 将其裁剪为其前导分量，
没有任何提示。仅限将较宽的值放入较窄的位置：不同宽度的两个操作数仍是
错误（DSH4226）。

**修复。** 无需更改。写入 swizzle（`Tint.rgb`）即可表达意图，并且 `dsc
migrate` 会写入它。

<a id="dsh5290"></a>

## DSH5290

<!-- generated:begin DSH5290 -->
**严重性** info

**消息**

```
The asset of this block and a function this file can call are both named '{0}', which 1.x kept apart; the block is declared as '{1}', and a '.dss' writes that name.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:866`
<!-- generated:end DSH5290 -->

**原因。** 规则 L23。一个块构建了一个以此文件可调用的函数命名的资产（例如
`ShaderFunction(Name = ".../MF_X")` 旁边的 `Function MF_X`）。1.x 将资产和函数保存在两个
表中，因此两者都存在；2.0 有一个名称表，并且该块的函数被声明为
`<Name>_Asset`，而资产保留其名称。

**修复。** 无需更改。迁移后的文件会在新标识符下声明导出，并为该资产
使用 `/// @name`。

<a id="dsh5291"></a>

## DSH5291

<!-- generated:begin DSH5291 -->
**严重性** info

**消息**

```
'{0}.{1}' lists no pin called '{2}'; it is connected by that name once the node exists, because a node may name its pins after its properties.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4657`
<!-- generated:end DSH5291 -->

**原因。** 规则 L24。命名参数不是目录为该类列出的引脚或属性。目录是
从每个类的默认对象读取的，并且某些节点会根据属性命名其引脚
（MoonToonModifier 根据修改器显示其他引脚，TextureSample 仅在
`MipValueMode = Derivative` 下显示 `CoordinatesDX`，Landscape 图层混合根据
其 `Layers` 将其引脚称为 `Layer Grass` / `Height Rock`）。节点一旦存在，引脚就按该名称
连接到节点上；如果节点未显示此类引脚，则构建会在该处失败（DSH8212）。在 `.dss` 中，这仅适用于
目录标记为按节点命名引脚的类；在其他情况下，未知名称是 DSH5213。在 `.dss` 中，这样的
引脚也不固定任何宽度：数值输出的宽度与引脚上的最宽值相同，而引脚上的 `material`
会使输出成为 `material`。

**修复。** 如果该名称就是节点所显示的名称，则无需处理。引脚自身的属性名称（`ChannelW`）
始终可解析，不需要查找。

<a id="dsh5292"></a>

## DSH5292

<!-- generated:begin DSH5292 -->
**严重性** info

**消息**

```
'{0}' is not declared, and as in 1.x this assignment declares it, as a local of type {1}.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2345`
<!-- generated:end DSH5292 -->

**原因。** 规则 L26。在 1.x 主体中，`x = value;` 会对一个未在任何位置声明的名称进行赋值。1.x 会在该处声明
该变量，并使用值的类型。

**修复。** 无需更改；`dsc migrate` 会写入 `T x = value;`。在 `.dss` 中，未声明的名称
是错误。

<a id="dsh5293"></a>

## DSH5293

<!-- generated:begin DSH5293 -->
**严重性** error

**消息**

```
A Substrate value has no compound assignment; write 'S = S + T' (Substrate.Add) or 'S = S * w' (Substrate.Weight).
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2377`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:274`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:302`
<!-- generated:end DSH5293 -->

**原因。** 一个 Substrate 值不具备的运算符。它们接受 `A + B`（`Substrate.Add`）、`A *
w` and `w * A` with a scalar (`Substrate.Weight`) and `lerp(A, B, t)` (`Substrate.HorizontalMix`），
除此之外没有别的。三条消息：另一个运算符（`A - B`、`A * B`、`-A`）；一个 `lerp`，其前
两个实参并非都是 Substrate 值或都是数字；一个复合赋值（`A += B`），
这会使 `A` 的声明表示两个节点。

**修复。** 写出你想要的节点：使用 `Substrate.Layer(Top, Base, Thickness)` 进行分层，使用 `lerp` 进行
混合，并使用 `A = A + B;` 代替 `A += B;`。

<a id="dsh5294"></a>

## DSH5294

<!-- generated:begin DSH5294 -->
**严重性** error

**消息**

```
'Substrate.{0}' is a node Unreal Engine has from {1} on; this engine does not have it.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4242`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:148`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:161`
<!-- generated:end DSH5294 -->

**原因。** 一段 Substrate 语法糖需要此引擎不具备的节点。三条消息：一个
`Substrate.X(...)` 调用，其节点从之后的 Unreal Engine 版本起存在（消息会说明
该版本）；一个运算符、一个运行时分支或一个虚拟实参，表示此类节点；以及在
完全没有 Substrate 节点的引擎上（5.4 之前）出现同样的情况。

**修复。** 针对具有该节点的引擎进行构建，或使用此引擎拥有的
节点编写语法糖所表示的内容——对于运行时分支，使用 `/// @static` 条件或 `lerp`。当同一份源需要
在两者上构建时，用 `#if DS_ENGINE_MINOR >= ...` 守护代码。

<a id="dsh5295"></a>

## DSH5295

<!-- generated:begin DSH5295 -->
**严重性** error

**消息**

```
'{0}.{1}': '{2}' and '{3}' parameterize the same pins; give one of them.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:600`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:631`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:825`
<!-- generated:end DSH5295 -->

**原因。** 两个实参参数化同一组引脚。`BaseColor` / `Metallic` / `Specular` 通过转换节点
成为 `DiffuseAlbedo` 和 `F0`，因此同时给出 `DiffuseAlbedo` 或 `F0` 会为一个引脚留下
两个值；`IOR` 计算 `F0`，因此它与 `F0` 以及金属度
实参冲突；`Haziness` 成为 `SecondRoughness` 和 `SecondRoughnessWeight`；`Transmittance` 成为
`SSSMFP`。对于逐成员构建的值同样适用此规则，此时这两次写入位于
不同行。

**修复。** 只保留一种参数化方式：要么使用旧方式（`BaseColor`、`Metallic`、`Specular`），要么
使用引脚（`DiffuseAlbedo`、`F0`）。

<a id="dsh5296"></a>

## DSH5296

<!-- generated:begin DSH5296 -->
**严重性** error

**消息**

```
'{0}.Thickness' is how deep 'Transmittance' is measured, and '{0}' was given no 'Transmittance' before this use.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:454`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:464`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:784`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:840`
<!-- generated:end DSH5296 -->

**原因。** 一个虚拟实参缺少其对照测量所依据的实参。`Haziness` 会将
BSDF 自身的 `Roughness` 转换为第二个波瓣，因此它需要 `Roughness`；`Thickness` 是
`Transmittance` 的测量深度，因此它需要 `Transmittance`。对于逐成员构建的值，
问题会在该值首次使用时提出：届时必须已经写入其伴随项。

**修复。** 在同一调用中，或在首次使用该值之前，给出伴随项——`Roughness` 搭配
`Haziness`，`Transmittance` 搭配 `Thickness`。

<a id="dsh5297"></a>

## DSH5297

<!-- generated:begin DSH5297 -->
**严重程度** 错误

**消息**

```
'{0}' has been assigned a whole Substrate value since it was declared, and that value has no members; build a new value, or write the members before the assignment.
```

**抛出位置** `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:506`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:555`
<!-- generated:end DSH5297 -->

**原因。** 在 Substrate 值不再是构建器之后，仍对其成员进行了写入。有两种消息：该值已被使用过（被传递、赋值、返回、与另一个值组合），这使它的节点定型，之后的写入无法改变该节点；或者该局部变量自声明以来被赋予了一个完整的 Substrate 值，而完成后的值没有成员。

**修复。** 在该值第一次使用之前写入所有成员。若要在使用后改变值，请构建第二个值。

<a id="dsh5298"></a>

## DSH5298

<!-- generated:begin DSH5298 -->
**严重程度** 错误

**消息**

```
A member of the Substrate value '{0}' cannot be written inside an 'if': that would be one node in two versions. Build two values and choose between them.
```

**抛出位置** `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:569`
<!-- generated:end DSH5298 -->

**原因。** 在某个 `if` 内部写入了 Substrate 值的成员，而该值的声明位于该结构之外。该值是一个节点，而仅在某一条路径上连接的引脚会使该节点出现两个版本。（在分支内部声明的构建器属于该分支自身，可以在其中写入。）

**修复。** 用分支计算该引脚的值并写入一次 -- `S.Roughness = wet ? 0.1 : 0.8;` -- 或者构建两个值并在它们之间选择。

<a id="dsh5299"></a>

## DSH5299

<!-- generated:begin DSH5299 -->
**严重程度** 错误

**消息**

```
'{0}' builds a '{1}.{2}', which has no pin called '{3}'.
```

**抛出位置** `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:536`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:581`、`Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:649`
<!-- generated:end DSH5299 -->

**原因。** 对正在构建的 Substrate 值进行成员访问无法成立。有三种消息：该值所构建的节点没有该名称的引脚或虚拟参数；在写入之前就读取了该成员 -- 成员读取回的是写入其中的内容，而节点自身的默认值并不是图中的值；在写入之前，该成员位于 `*=`、`+=`、`++` 及其同类表达式的左侧。

**修复。** 对照节点检查引脚的名称（`dsc export-catalog`，或在编辑器扩展中悬停查看），并在读取成员之前先为其赋值。

<a id="dsh5300"></a>

## DSH5300

<!-- generated:begin DSH5300 -->
**严重程度** 错误

**消息**

```
'UE.{0}' needs {1}, and this engine's node catalog does not have it.
```

**抛出位置** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4256`
<!-- generated:end DSH5300 -->

**原因。** 调用了 `UE.DreamPassOutput`、`UE.DreamPassBuffer` 或引擎的 `UE.UserSceneTexture`，而此文件所绑定的节点目录中没有该类。两个 Custom Pass 节点仅在 Unreal Engine 5.8 及更高版本中列出：在此之下它们的类仍然存在，但会编译为材质错误，因此目录将它们排除在外，此处会拒绝该调用，而不是将其构建到无法编译的材质中。`UE.UserSceneTexture` 随 5.6 引入。从这样的引擎用 `dsc export-catalog` 导出的目录会给出相同的回答。

**修复。** 使用具有该节点的引擎进行构建。如果源文件还必须在较旧的引擎上构建，则对该调用进行保护：

```hlsl
#if DS_ENGINE_MAJOR > 5 || (DS_ENGINE_MAJOR == 5 && DS_ENGINE_MINOR >= 8)
    UE.DreamPassOutput(Output0 = float4(1, 0, 0, 0));
#endif
```

<a id="dsh5301"></a>

## DSH5301

<!-- generated:begin DSH5301 -->
**严重程度** 错误

**消息**

```
Material '{0}' asks for the new material translator ('bEnableNewHLSLGenerator = true'), and uses {1} (line {2}), which only the classic translator compiles; remove the setting.
```

**抛出位置** `Source/DreamShaderLang/Private/Semantic/LangBinderPassNodes.cpp:251`
<!-- generated:end DSH5301 -->

**原因。** 材质在其 `#pragma material` 中设置了 `bEnableNewHLSLGenerator = true`，而其图中包含一个 `UE.DreamPassOutput` 或 `UE.DreamPassBuffer` 节点，写在入口中或入口内联的辅助函数中。这两个节点都没有实现新的材质转换器，因此材质无法编译。该消息停留在该设置上，并给出第一个节点的行号。

**修复。** 从 `#pragma material` 中移除 `bEnableNewHLSLGenerator`，或将其设置为 `false`。需要新转换器的材质不能使用 Custom Pass 节点。

<a id="dsh5302"></a>

## DSH5302

<!-- generated:begin DSH5302 -->
**严重程度** 警告

**消息**

```
UE.DreamPassOutput does nothing in '{0}', a {1}: the engine compiles custom outputs from a material's own graph only, and only there does the node give the material its DreamPass shader tag. A material using '{0}' writes no pass outputs, and a mesh pass in 'Mode = Own' does not draw it; write UE.DreamPassOutput in the material itself.
```

**抛出位置** `Source/DreamShaderLang/Private/Semantic/LangBinderPassNodes.cpp:271`
<!-- generated:end DSH5302 -->

**原因。** 在材质函数、材质层或层混合（一个 `export` 的函数，或标记为 `@layer` / `@layerblend` 的函数）中调用了 `UE.DreamPassOutput`，无论是直接调用还是通过其内联的辅助函数调用。引擎仅从材质自身的图中编译自定义输出，因此在函数中该节点不写入任何内容，也不会将 DreamPass 着色器标签赋予调用它的材质：仅在函数中拥有该节点的材质不会由 `Mode = Own` 中的网格通道绘制，并且对于将其命名为 `Material`（DSH7340）的网格通道也没有输出。之所以是警告，是因为该函数本身仍然可以构建。

**修复。** 在材质中写入该节点：在其入口中，或在入口调用的普通辅助函数中（该辅助函数会内联到材质的图中）。让该函数返回输出所需的值，并在那里将它们传递给 `UE.DreamPassOutput`。

<a id="dsh5303"></a>

## DSH5303

<!-- generated:begin DSH5303 -->
**严重程度** 错误

**消息**

```
Material '{0}' gets a second UE.DreamPassOutput node here (the first is on line {1}), and a material compiles one custom output of a class; write all four outputs in one call. A call inside a loop, or in a helper called twice, makes more than one node.
```

**抛出位置** `Source/DreamShaderLang/Private/Semantic/LangBinderPassNodes.cpp:235`
<!-- generated:end DSH5303 -->

**原因。** 一个材质的图会得到两个 `UE.DreamPassOutput` 节点，而一个材质每个类只能编译一个自定义输出 -- 引擎会使其失败。两次调用会产生两个节点。循环内的一次调用也是如此，它会展开为多个节点；材质调用两次（或从循环中调用）的辅助函数中的一次调用也是如此，因为辅助函数在每次调用处都会内联。该消息停留在第二个节点处，并给出第一个节点的行号。

**修复。** 先计算各个值，然后只写入一次节点，并填入它所填充的每个输出：

```hlsl
float4 Mask = ...;
float  Id   = ...;
UE.DreamPassOutput(Output0 = Mask, Output1 = Id);
```

<a id="dsh5315"></a>

## DSH5315

<!-- generated:begin DSH5315 -->
**严重程度** 错误

**消息**

```
UE.DreamPassBuffer names no Pipeline: write the name of the '.dsp' (Pipeline = "CP_Highlight") or the pipeline asset's path.
```

**抛出位置** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:847`、`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:868`
<!-- generated:end DSH5315 -->

**原因。** `UE.DreamPassBuffer` 没有说明它读取的是哪条管线。有两种消息：该调用没有 `Pipeline` 参数，或该参数为空；或者 `Pipeline` 被写成一个路径 -- `/Game/...`、`Path(...)`、粘贴的 `Class'/Game/...'` 引用 -- 而该路径无法解析为对象路径，消息中会引用其原因（`Path(...)` 格式错误，粘贴的引用所属类不是管线）。

**修复。** 用构建该管线的 `.dsp` 的名称来命名该管线，或用管线资产的路径：

```hlsl
float Glow = UE.DreamPassBuffer(Pipeline = "CP_Highlight", Buffer = "Blurred").r;
```

<a id="dsh5316"></a>

## DSH5316

<!-- generated:begin DSH5316 -->
**严重程度** 错误

**消息**

```
No pipeline named '{0}' is built by a .dsp under '{1}'; write the pipeline asset's path, or check the name.
```

**抛出位置** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:877`
<!-- generated:end DSH5316 -->

**原因。** `Pipeline` 是一个裸名称，而此 `.dss` 的源根目录下没有任何 `.dsp` 构建该名称的管线。裸名称的查找方式与 `.dsi` 的 `Parent` 相同：在同一源根目录的产物中查找。管线以其 `.dsp` 文件命名。

**修复。** 对照 `.dsp` 的文件名检查该名称。位于另一个源根目录下的管线，或没有任何 `.dsp` 构建的管线，以其资产路径命名：`Pipeline = "/Game/Passes/CP_Highlight"`。

<a id="dsh5317"></a>

## DSH5317

<!-- generated:begin DSH5317 -->
**严重程度** 错误

**消息**

```
'{0}' names more than one pipeline under this source root ({1}); write the pipeline asset's path instead.
```

**抛出位置** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:885`
<!-- generated:end DSH5317 -->

**原因。** `Pipeline` 是一个裸名称，而此源根目录下有多个 `.dsp` 构建该名称的管线 -- 不同文件夹中有两个同名文件。消息会列出它们。

**修复。** 写出你想要的管线资产路径，或重命名其中一个文件。

<a id="dsh5318"></a>

## DSH5318

<!-- generated:begin DSH5318 -->
**严重程度** 错误

**消息**

```
The pipeline '{0}' names nothing: no pipeline asset exists at '{1}', and no .dsp under the source roots builds it.
```

**抛出位置** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:962`
<!-- generated:end DSH5318 -->

**原因。** 该引用解析到了一个不存在任何管线资产的物件路径，且源根目录下没有任何 `.dsp` 会构建该管线，因此没有任何内容能替代它。通常该路径什么都不指向：拼写错误，或资产已被移动或重命名。由 `.dsp` 构建的管线不属于此错误，即使 `.dsp` 无法编译：一个什么都不构建的 `dsc check` 会读取该 `.dsp` 以替代缺失的资产，而失败的那个则是 DSH5319，伴随其第一个错误。

**修复。** 更正路径，或以管线所属 `.dsp` 的裸名称来命名该管线。

<a id="dsh5319"></a>

## DSH5319

<!-- generated:begin DSH5319 -->
**严重性** 错误

**消息**

```
The pipeline '{0}' comes from '{1}', which failed to compile, so the buffer this material reads cannot be checked. {2}
```

**引发自** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:510`、`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:951`
<!-- generated:end DSH5319 -->

**原因。** 该管线来自一个无法编译的 `.dsp`，因此此材质读取的缓冲区无法被检查。有两条消息，每种运行各一条：

- 构建资产的运行发现管线资产缺失或比其 `.dsp` 更旧，于是先构建了
  `.dsp`（DSH5320 如此说明），而该编译失败了（`... which failed to compile ...`）；
- 什么都不构建的运行——即 `dsc check`——未找到任何管线资产，于是读取了其位置上的 `.dsp`，
  而该 `.dsp` 无法编译（`... which does not compile ...`）。

消息以 `.dsp` 的第一个错误结尾。当该错误为 DSH8333 时，该管线与此材质互相需要对方：即 DSH5325 所描述的循环，从材质这一侧遭遇。

**修复。** 修复该 `.dsp`——单独编译它以查看其全部诊断——然后再次编译该材质。对于 DSH8333，按 DSH5325 所述更改该读取。

<a id="dsh5320"></a>

## DSH5320

<!-- generated:begin DSH5320 -->
**严重性** 信息

**消息**

```
The pipeline '{0}' was missing or older than its source, so '{1}' was compiled first.
```

**引发自** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:490`
<!-- generated:end DSH5320 -->

**原因。** 此材质读取的管线缺失或比其 `.dsp` 更旧，因此编译先构建了 `.dsp`，就像 `.dsi` 的父项会先被构建一样。只有会构建资产的运行才会这样做；`dsc check` 则保持管线原样。在该编译运行之前如此说明；若编译失败，随后会出现 DSH5319。

**修复。** 无需修复。

<a id="dsh5321"></a>

## DSH5321

<!-- generated:begin DSH5321 -->
**严重性** 错误

**消息**

```
(built at runtime)
```

**引发自** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:1002`、`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:986`
<!-- generated:end DSH5321 -->

**原因。** 该节点自身的规则拒绝了该缓冲区——其措辞与材质编辑器所用的一致。在管线资产存在的情况下，消息会说明是哪种情况：

- `Buffer` 指定了一个内置纹理（`SceneColor`、`CustomDepth`……），它仅存在于帧的
  渲染图中；没有任何内容会将它复制出来。
- 该管线没有该名称的缓冲区。
- 该缓冲区未被导出。
- 该缓冲区为 `Depth32` 或整数格式，材质无法对其进行采样。
- 该缓冲区已被导出，而资产没有为其准备的渲染目标。

内置纹理在两条路径上都是此错误：当该管线尚未构建、其 `.dsp` 被读取以替代它时（在全新检出上的 `dsc check`），它在查找任何缓冲区之前就被拒绝，措辞相同，因此一次检查所报告的内容与一次构建所报告的相同。`.dsp` 路径的其他拒绝情形有其自己的代码（DSH5322、DSH5323）。

`dsc check` 读取的是磁盘上原样的管线资产，因此自上次编译以来在 `.dsp` 中添加或导出的缓冲区尚不在此处。

**修复。** 指定一个该管线用 `Export = true` 声明的、采用浮点或归一化格式的缓冲区。当 `.dsp` 已经如此说明时，编译它——这也会生成缺失的渲染目标——然后编译该材质。

<a id="dsh5322"></a>

## DSH5322

<!-- generated:begin DSH5322 -->
**严重性** 错误

**消息**

```
The pipeline '{0}' declares no buffer '{1}'.
```

**引发自** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:1016`、`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:1024`
<!-- generated:end DSH5322 -->

**原因。** 该管线尚未构建，因此其 `.dsp` 被读取以替代它（在全新检出上的 `dsc check`），而该缓冲区不匹配。有两条消息：`.dsp` 未声明该名称的缓冲区——比较时忽略大小写，与构建后资产的比较方式一致——或者该缓冲区未被 `Export = true`。内置纹理不会到此为止：它先被拒绝，如 DSH5321 所述，使用该节点自身的措辞。

**修复。** 指定一个 `.dsp` 所声明的缓冲区，并用 `Export = true` 声明它：

```hlsl
buffer Blurred : R8(Scale = 0.5, Export = true);
```

<a id="dsh5323"></a>

## DSH5323

<!-- generated:begin DSH5323 -->
**严重性** 错误

**消息**

```
Buffer '{1}' of the pipeline '{0}' is {2}, which a material cannot sample. Export a float or normalized buffer instead.
```

**引发自** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:1032`
<!-- generated:end DSH5323 -->

**原因。** 该管线尚未构建，其 `.dsp` 被读取以替代它，而所导出的缓冲区为 `Depth32`、`R32U` 或 `RG32U`。材质将该纹理声明为浮点 `Texture2D` 并使用过滤采样器对其进行采样，而这些格式均不允许这样做。这是一道后备防线：`.dsp` 本身会先拒绝这样的导出（DSH7355、DSH7321），而无法编译的 `.dsp` 不会在此被读取（DSH5319）。

**修复。** 改为导出浮点或归一化格式——`R8`、`RG16F`、`R32F`。

<a id="dsh5324"></a>

## DSH5324

<!-- generated:begin DSH5324 -->
**严重性** 错误

**消息**

```
UE.DreamPassBuffer names no Buffer of '{0}': write the exported buffer's name (Buffer = "Blurred").
```

**引发自** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:853`
<!-- generated:end DSH5324 -->

**原因。** `UE.DreamPassBuffer` 指定了其管线，却没有指定其中的任何缓冲区：`Buffer` 缺失、为空或 `None`。

**修复。** 写入所导出缓冲区的名称：`Buffer = "Blurred"`。

<a id="dsh5325"></a>

## DSH5325

<!-- generated:begin DSH5325 -->
**严重性** 错误

**消息**

```
The pipeline '{0}' needs this material to be built -- it is one of its pass materials, or the material of a pipeline that one needs -- so this material cannot read its exported buffer: the two can be built in no order. Inside its own pipeline a pass binds the buffer with 'read' -- a fullscreen material reads it as a UserSceneTexture input -- rather than the exported copy; a mesh pass's material cannot read its own pipeline's buffers.
```

**引发自** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:907`
<!-- generated:end DSH5325 -->

**原因。** 此材质通过 `UE.DreamPassBuffer` 读取了某个管线的导出缓冲区，而该管线需要此材质先被构建：该管线将它作为其某个通道材质绘制，或其某个通道材质读取了绘制它的管线。两者无法按任何顺序构建——先编译材质时，材质没有管线资产可读取；后编译材质时，管线没有材质可绘制（DSH4406）——因此这一对永远无法引导启动，于是该读取被拒绝。当在管线编译内部编译或读取该材质时会引发此错误，因为管线编译会先构建缺失或过期的通道材质；随后 `.dsp` 会将其材质报告为失败（DSH8331）。从材质这一侧遭遇时，同一循环就是 `.dsp` 的 DSH8333，材质会将其报告为 DSH5319。

**修复。** 在其自身管线内部，读取缓冲区本身而非其导出副本：在绘制此材质的通道中用 `read` 绑定它，并在材质中将它作为该名称的 `UE.UserSceneTexture` 输入进行采样，而不是调用 `UE.DreamPassBuffer`：

```hlsl
// in the .dsp
pass Composite : fullscreen
{
    ...
    read Blurred;
}

// in the material
float Halo = UE.UserSceneTexture(UserSceneTexture = "Blurred", Coordinates = UV).Color.r;
```

网格通道的材质完全无法读取其自身管线的缓冲区：请在该管线的全屏通道中采样该缓冲区，或在管线不绘制的材质中采样。当循环经过另一条管线时，同样要在其某个 `UE.DreamPassBuffer` 读取处打断它。

<a id="dsh5326"></a>

## DSH5326

<!-- generated:begin DSH5326 -->
**严重性** 错误

**消息**

```
'{0}' is a {1}, not a DreamShader pass pipeline.
```

**引发自** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:923`
<!-- generated:end DSH5326 -->

**原因。** `Pipeline` 命名了一个存在且并非通道管线的资产：一个材质、一个纹理——
通常是一个导出的缓冲区所变成的渲染目标，`CP_Highlight_Blurred`，被选中而不是
管线 `CP_Highlight`。该消息会指出其类。

**修复。** 将 `Pipeline` 指向该管线资产，或指向其 `.dsp` 的裸名称，并用 `Buffer` 命名该缓冲区。
