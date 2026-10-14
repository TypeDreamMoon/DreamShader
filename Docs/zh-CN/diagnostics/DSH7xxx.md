---
title: "DSH7xxx --- 属性、参数与设置"
description: "DSH7xxx --- 属性、参数与设置"
---

<a id="dsh7xxx-----properties-parameters-and-settings"></a>

# DSH7xxx --- 属性、参数与设置

> 生成标记之间的内容由 `.skill/gen-diagnostics.ps1` 写入。
> 标记以下的所有内容均为手动撰写，在重新生成后仍会保留。

<a id="dsh7111"></a>

## DSH7111

<!-- generated:begin DSH7111 -->
**严重性** 错误

**消息**

```
Invalid boolean value '%s' for %s.
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:120`
<!-- generated:end DSH7111 -->

**原因。** 某个布尔类型的设置被赋予了 `true` / `false` 之外的值（不区分大小写）。

**修复。** 请写入 `true` 或 `false`。

<a id="dsh7112"></a>

## DSH7112

<!-- generated:begin DSH7112 -->
**严重性** 错误

**消息**

```
Setting path segment cannot be empty.
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:187`
<!-- generated:end DSH7112 -->

**原因。** 某个材质设置键既不属于已知的那几个（`BlendMode`、
`ShadingModel`、`Domain`、...），又被作为反射路径读入 `UMaterial`：即由 `.` 分隔的属性名，
在固定大小数组上使用 `[index]`。该键包含一个空段：开头或结尾处有 `.`，或者连续出现两个。

**修复。** 请移除多余的 `.`。

<a id="dsh7113"></a>

## DSH7113

<!-- generated:begin DSH7113 -->
**严重性** 错误

**消息**

```
Invalid array setting segment '%s'.
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:200`
<!-- generated:end DSH7113 -->

**原因。** 某个材质设置键既不属于已知的那几个（`BlendMode`、
`ShadingModel`、`Domain`、...），又被作为反射路径读入 `UMaterial`：即由 `.` 分隔的属性名，
在固定大小数组上使用 `[index]`。该键的某个段带有 `[`，但其前面没有属性名，
或者缺少其 `]`。

**修复。** 请将该段写成 `Name[2]`。

<a id="dsh7114"></a>

## DSH7114

<!-- generated:begin DSH7114 -->
**严重性** 错误

**消息**

```
Invalid array index '%s' in setting segment '%s'.
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:207`
<!-- generated:end DSH7114 -->

**原因。** 某个材质设置键既不属于已知的那几个（`BlendMode`、
`ShadingModel`、`Domain`、...），又被作为反射路径读入 `UMaterial`：即由 `.` 分隔的属性名，
在固定大小数组上使用 `[index]`。`[` 与 `]` 之间的文本不是零或更大的整数。

**修复。** 请使用字面量索引：`Name[2]`。

<a id="dsh7115"></a>

## DSH7115

<!-- generated:begin DSH7115 -->
**严重性** 错误

**消息**

```
Invalid array setting segment '%s'.
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:215`
<!-- generated:end DSH7115 -->

**原因。** 某个材质设置键既不属于已知的那几个（`BlendMode`、
`ShadingModel`、`Domain`、...），又被作为反射路径读入 `UMaterial`：即由 `.` 分隔的属性名，
在固定大小数组上使用 `[index]`。某一段的 `]` 之后出现了 `.` 之外的内容。

**修复。** 请为每个段只写一个索引：`Name[2].Field`。

<a id="dsh7116"></a>

## DSH7116

<!-- generated:begin DSH7116 -->
**严重性** 错误

**消息**

```
Invalid material setting target.
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:300`
<!-- generated:end DSH7116 -->

**原因。** 设置未应用于任何材质。内部错误：在写入其设置之前，资产尚未创建。

**修复。** 请将其与源文件一同上报；同一次构建的第一个错误通常会指出真正的起因。

<a id="dsh7117"></a>

## DSH7117

<!-- generated:begin DSH7117 -->
**严重性** 错误

**消息**

```
Invalid material setting path '%s'.
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:306`
<!-- generated:end DSH7117 -->

**原因。** 某个材质设置键既不属于已知的那几个（`BlendMode`、
`ShadingModel`、`Domain`、...），又被作为反射路径读入 `UMaterial`：即由 `.` 分隔的属性名，
在固定大小数组上使用 `[index]`。该键完全无法拆分为段（为空，或者只有点号）。

**修复。** 请使用该材质在 Details 面板中的属性在 C++ 中的名称来写入属性名
（`TranslucencyLightingMode`，而不是其显示名称）。

<a id="dsh7118"></a>

## DSH7118

<!-- generated:begin DSH7118 -->
**严重性** 错误

**消息**

```
Unsupported material setting '%s'.
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:323`
<!-- generated:end DSH7118 -->

**原因。** 某个材质设置键既不属于已知的那几个（`BlendMode`、
`ShadingModel`、`Domain`、...），又被作为反射路径读入 `UMaterial`：即由 `.` 分隔的属性名，
在固定大小数组上使用 `[index]`。该材质（或该路径已到达的结构体）上不存在该名称的属性。显示名称不算数；
仅编辑器使用的类别也不算数。

**修复。** 请在 `Material.h` 中查找该属性的 C++ 名称。具有自身含义的键
列于 `Docs/settings` 中。

<a id="dsh7119"></a>

## DSH7119

<!-- generated:begin DSH7119 -->
**严重性** 错误

**消息**

```
Setting '%s' is not an indexed array property.
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:330`
<!-- generated:end DSH7119 -->

**原因。** 某个设置键将 `[index]` 用在了非固定大小数组的属性上。

**修复。** 请移除该索引。

<a id="dsh7120"></a>

## DSH7120

<!-- generated:begin DSH7120 -->
**严重性** 错误

**消息**

```
Array index %d is out of range for setting '%s' (max %d).
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:335`
<!-- generated:end DSH7120 -->

**原因。** 某个设置键索引固定大小数组时超出了其末尾。消息中给出了最后一个有效索引。

**修复。** 请使用数组范围内的索引。

<a id="dsh7121"></a>

## DSH7121

<!-- generated:begin DSH7121 -->
**严重性** 错误

**消息**

```
Setting '%s' requires an explicit [index].
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:340`
<!-- generated:end DSH7121 -->

**原因。** 某个设置键指名了某个固定大小数组，但未说明是哪个元素。

**修复。** 请添加 `[index]`。

<a id="dsh7122"></a>

## DSH7122

<!-- generated:begin DSH7122 -->
**严重性** 错误

**消息**

```
Setting path '%s' cannot continue through '%s'.
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:356`
<!-- generated:end DSH7122 -->

**原因。** 某个设置键在非结构体的属性之后继续使用 `.Field`，因此没有可供查找 `Field` 的对象。

**修复。** 请在该属性处结束路径，或修正 `.` 之前的属性名。

<a id="dsh7123"></a>

## DSH7123

<!-- generated:begin DSH7123 -->
**严重性** 错误

**消息**

```
Invalid material setting path '%s'.
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:363`
<!-- generated:end DSH7123 -->

**原因。** 某个设置路径在未到达属性之前就用完了段。这是路径遍历器的内部兜底机制。

**修复。** 请连同导致此问题的键一同上报。

<a id="dsh7124"></a>

## DSH7124

<!-- generated:begin DSH7124 -->
**严重性** 错误

**消息**

```
Failed to create a transient material for Settings validation.
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:371`
<!-- generated:end DSH7124 -->

**原因。** 在构建任何内容之前，设置会先针对一个临时材质进行校验，而该临时对象无法创建。
这表明引擎处于异常状态（内存不足、正在关闭），而非源文件的问题。

**修复。** 请重试；如果问题持续存在，请重启编辑器。

<a id="dsh7125"></a>

## DSH7125

<!-- generated:begin DSH7125 -->
**严重性** 错误

**消息**

```
Invalid value '%s' for setting '%s'. %s
```

**引发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:384`
<!-- generated:end DSH7125 -->

**原因。** 反射设置的某个值不符合其属性的类型；其后的句子说明了该类型期望什么（布尔值、整数、枚举值、对象路径……）。
此错误在设置校验期间、触碰任何资产之前引发。

**修复。** 请按照该句子所要求的形式写入值。枚举值请使用其短名称
（`TLM_Surface` 或 `Surface`）。

<a id="dsh7126"></a>

## DSH7126

<!-- generated:begin DSH7126 -->
**严重性** 错误

**消息**

```
Invalid value '%s' for setting '%s'. %s
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:402`
<!-- generated:end DSH7126 -->

**原因。** 与 DSH7125 相同，是在该设置被应用到真实材质时触发的——只有在验证通过、但资产随后拒绝该值时才可能出现，这意味着该属性在临时材质与资产的类之间存在差异。

**修复。** 对照该属性的类型检查该值；如果 DSH7125 没有先触发，请上报。

<a id="dsh7127"></a>

## DSH7127

<!-- generated:begin DSH7127 -->
**严重性** 错误

**消息**

```
Unsupported BlendMode/RenderType '%s'.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:420`
<!-- generated:end DSH7127 -->

**原因。** `BlendMode`（或其 1.x 别名 `RenderType`）不是引擎 `EBlendMode` 的某个值。匹配值时会去掉 `BLEND_` 前缀，并忽略大小写、空格与下划线；隐藏的枚举项不计入。

**修复。** 使用 `Opaque`、`Masked`、`Translucent`、`Additive`、`Modulate`、`AlphaComposite`、`AlphaHoldout`，或此引擎枚举中具有的其他值。

<a id="dsh7128"></a>

## DSH7128

<!-- generated:begin DSH7128 -->
**严重性** 错误

**消息**

```
ShadingModel="Substrate" requires Unreal Engine 5.4 or newer.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:431`
<!-- generated:end DSH7128 -->

**原因。** `ShadingModel = Substrate` 被设置在不低于 5.4 的引擎上，而该引擎没有 Substrate 前向材质。

**修复。** 在那里使用经典着色模型，或使用 5.4 或更高版本构建。

<a id="dsh7129"></a>

## DSH7129

<!-- generated:begin DSH7129 -->
**严重性** 错误

**消息**

```
Unsupported ShadingModel '%s'.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:437`
<!-- generated:end DSH7129 -->

**原因。** `ShadingModel` 不是引擎 `EMaterialShadingModel` 的某个值（匹配时会去掉 `MSM_`，并忽略大小写、空格与下划线）。

**修复。** 使用引擎名称之一，不带 `MSM_` 前缀（`DefaultLit`、`Unlit`、`Subsurface`、`ClearCoat`、`TwoSidedFoliage`、`Hair`、`Cloth`、`Eye`、`SingleLayerWater`、`ThinTranslucent`、`Substrate`……）；引擎分支可能会添加自己的名称。

<a id="dsh7130"></a>

## DSH7130

<!-- generated:begin DSH7130 -->
**严重性** 错误

**消息**

```
Unsupported MaterialDomain '%s'.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:446`
<!-- generated:end DSH7130 -->

**原因。** `Domain` 不是引擎 `EMaterialDomain` 的某个值（匹配时会去掉 `MD_`，并忽略大小写）。

**修复。** 使用 `Surface`、`DeferredDecal`、`LightFunction`、`Volume`、`PostProcess` 或 `UI`。

<a id="dsh7131"></a>

## DSH7131

<!-- generated:begin DSH7131 -->
**严重性** 错误

**消息**

```
Invalid reflected property target.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:75`
<!-- generated:end DSH7131 -->

**原因。** 一个字面量将被写入对象上不存在的属性。内部错误：目录列出了当前类没有的属性。

**修复。** 上报；重新导出目录（`dsc export-catalog`）可排除清单过期的情况。

<a id="dsh7132"></a>

## DSH7132

<!-- generated:begin DSH7132 -->
**严重性** 错误

**消息**

```
'%s' is not a valid boolean value for '%s'.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:103`
<!-- generated:end DSH7132 -->

**原因。** 节点的布尔属性（或布尔设置）被赋予了 `true` / `false` 之外的其他值。

**修复。** 写入 `true` 或 `false`。

<a id="dsh7133"></a>

## DSH7133

<!-- generated:begin DSH7133 -->
**严重性** 错误

**消息**

```
'%s' is not a valid integer value for '%s'.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:114`
<!-- generated:end DSH7133 -->

**原因。** 整数属性被赋予了不是整数的文本。

**修复。** 写入一个整数，不带小数点。

<a id="dsh7134"></a>

## DSH7134

<!-- generated:begin DSH7134 -->
**严重性** 错误

**消息**

```
'%s' is not a valid unsigned integer value for '%s'.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:125`
<!-- generated:end DSH7134 -->

**原因。** 无符号整数属性被赋予了负数或不是数字的文本。

**修复。** 写入一个大于等于零的整数。

<a id="dsh7135"></a>

## DSH7135

<!-- generated:begin DSH7135 -->
**严重性** 错误

**消息**

```
'%s' is not a valid numeric value for '%s'.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:136`
<!-- generated:end DSH7135 -->

**原因。** 浮点属性被赋予了不是数字的文本。

**修复。** 写入一个数字。向量属性采用引擎自身的文本形式接收 `(X=..,Y=..,Z=..)`，而不是 HLSL 构造函数。

<a id="dsh7136"></a>

## DSH7136

<!-- generated:begin DSH7136 -->
**严重性** 错误

**消息**

```
'%s' is not a valid numeric value for '%s'.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:147`
<!-- generated:end DSH7136 -->

**原因。** 双精度属性被赋予了不是数字的文本。

**修复。** 写入一个数字。

<a id="dsh7137"></a>

## DSH7137

<!-- generated:begin DSH7137 -->
**严重性** 错误

**消息**

```
Object property '%s' expects Path(...) or an absolute Unreal object path.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:169`
<!-- generated:end DSH7137 -->

**原因。** 对象属性（纹理、函数、集合）被赋予了既不是 `Path(Root, "...")` 也不是绝对对象路径（如 `/Game/Textures/T_Noise`）的文本。

**修复。** 将资产写为 `Path(Game, "Textures/T_Noise")` 或其对象路径。

<a id="dsh7138"></a>

## DSH7138

<!-- generated:begin DSH7138 -->
**严重性** 错误

**消息**

```
Failed to load asset '%s' for '%s'.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:184`
<!-- generated:end DSH7138 -->

**原因。** 对象属性所命名的资产无法加载：路径错误、资产已被移动，或其插件未在此项目中挂载。

**修复。** 从内容浏览器复制引用（*复制引用*）并粘贴进来；检查拥有该资产的插件是否已启用。

<a id="dsh7139"></a>

## DSH7139

<!-- generated:begin DSH7139 -->
**严重性** 错误

**消息**

```
Asset '%s' is not compatible with '%s'. Expected '%s'.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:189`
<!-- generated:end DSH7139 -->

**原因。** 资产已加载，但它不是该属性所要求的类：期望 TextureCube 却给了 Texture2D，期望函数却给了材质。

**修复。** 将该属性指向消息中所指定类的资产。

<a id="dsh7140"></a>

## DSH7140

<!-- generated:begin DSH7140 -->
**严重性** 错误

**消息**

```
'%s' is not a valid enum value for '%s'.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:203`
<!-- generated:end DSH7140 -->

**原因。** 枚举属性（`FEnumProperty`）被赋予了不属于该枚举值的名称。匹配值时带或不带枚举前缀均可，且忽略大小写。

**修复。** 使用内置目录为该属性列出的值之一（`dsc export-catalog`）。

<a id="dsh7141"></a>

## DSH7141

<!-- generated:begin DSH7141 -->
**严重性** 错误

**消息**

```
'%s' is not a valid enum value for '%s'.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:218`
<!-- generated:end DSH7141 -->

**原因。** 与 DSH7140 相同，针对较旧的 `TEnumAsByte` 类型的枚举属性。

**修复。** 使用该枚举的某个值。

<a id="dsh7142"></a>

## DSH7142

<!-- generated:begin DSH7142 -->
**严重性** 错误

**消息**

```
'%s' is not a valid byte value for '%s'.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:228`
<!-- generated:end DSH7142 -->

**原因。** 非枚举的字节属性被赋予了不是 0 到 255 之间数字的文本。

**修复。** 写入一个 0 到 255 之间的数字。

<a id="dsh7143"></a>

## DSH7143

<!-- generated:begin DSH7143 -->
**严重性** 错误

**消息**

```
Property '%s' on '%s' is not a supported literal type yet.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:242`
<!-- generated:end DSH7143 -->

**原因。** 该属性存在，但其类型不是可写入字面量的类型：写入器不认识的文本形式的结构体、数组、映射、委托。

**修复。** 将该属性从调用中移除，并在资产上手动设置，或请求支持该类型；消息中会指明该属性及其类。

<a id="dsh7144"></a>

## DSH7144

<!-- generated:begin DSH7144 -->
**严重性** 错误

**消息**

```
Invalid reflected property target.
```

**触发者** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:249`
<!-- generated:end DSH7144 -->

**原因。** 尝试将字面量写入空对象或空属性。内部错误。

**修复。** 连同源文件一起报告。

<a id="dsh7200"></a>

## DSH7200

<!-- generated:begin DSH7200 -->
**严重级别** 错误

**消息**

```
'{0}' is set twice by '#pragma material'; it was already set on line {1}.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:551`
<!-- generated:end DSH7200 -->

**原因。** 同一个 `#pragma material` 键被设置了两次，可能位于两行不同的代码上——这两行
会被合并到同一个设置表中。键的比较不区分大小写，与 1.x 的设置行为一致。

**修复。** 删除其中一个。消息中会指出第一个键所在的行。

<a id="dsh7201"></a>

## DSH7201

<!-- generated:begin DSH7201 -->
**严重级别** 错误

**消息**

```
'Backend' has no value; write 'Backend = Graph' or 'Backend = ThinCustom'. An empty value meant Graph in 1.x and means nothing now.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:599`
<!-- generated:end DSH7201 -->

**原因。** 为 `Backend` 赋予了空值。1.x 会将 `Backend = ""` 读取为 `Graph`，从而在
无人写入的情况下静默覆盖项目默认值；2.0 拒绝这种做法，而不是把这个陷阱
延续下去。

**修复。** 写入 `Backend = Graph` 或 `Backend = ThinCustom`，或者删除该键以采用项目
默认值。

<a id="dsh7202"></a>

## DSH7202

<!-- generated:begin DSH7202 -->
**严重级别** 错误

**消息**

```
'Backend = {0}' is not a backend; the backends are 'Graph' and 'ThinCustom'.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:628`
<!-- generated:end DSH7202 -->

**原因。** `Backend` 指定的名称不是后端。

**修复。** 后端包括 `Graph`（可见的材质图表）与 `ThinCustom`（隐藏的基础
材质加一个轻量级实例）。`Instance` 是 `ThinCustom` 的旧写法，仍然
可用，但会产生一条警告。

<a id="dsh7203"></a>

## DSH7203

<!-- generated:begin DSH7203 -->
**严重级别** 警告

**消息**

```
'#pragma material' configures a material, and this file has no 'export void Name(inout material m)' entry to configure.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1367`
<!-- generated:end DSH7203 -->

**原因。** 该文件包含 `#pragma material(...)`，但没有可供配置的材质条目。函数
库不会成为 `UMaterial`，因此这些设置没有可应用的对象。

**修复。** 删除该 pragma，或者添加该文件本来应有的
`export void Name(inout material m)`。

<a id="dsh7204"></a>

## DSH7204

<!-- generated:begin DSH7204 -->
**严重级别** 警告

**消息**

```
'Backend = Instance' is the old spelling of 'Backend = ThinCustom'; write the new one.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:620`
<!-- generated:end DSH7204 -->

**原因。** `Backend = Instance`——即 `ThinCustom` 在 1.x 弃用过渡期内的写法。

**修复。** 改写为 `Backend = ThinCustom`。生成的内容不会有任何变化。

<a id="dsh7205"></a>

## DSH7205

<!-- generated:begin DSH7205 -->
**严重级别** 错误

**消息**

```
'#pragma material' takes 'Key = Value' pairs; '{0}' has no key.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:535`
<!-- generated:end DSH7205 -->

**原因。** 为 `#pragma material` 赋予了不带键的内容。

**修复。** 写入 `Key = Value`。键就是 `UMaterial` 属性名；未知的键会通过反射
传递到材质，因此拼错的键会由发射器报告，而不是在这里报告。

<a id="dsh7210"></a>

## DSH7210

<!-- generated:begin DSH7210 -->
**严重级别** 错误

**消息**

```
'{0}' is a compile-time constant, and this initializer is not one; a constant is built from literals and other constants.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:290`、`Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:607`
<!-- generated:end DSH7210 -->

**原因。** 某个 `static const`（或 `const`）的初始化值无法由绑定器折叠为一个值。常量
在图表中会变成一个 Constant 节点，因此它的值必须在任何内容运行之前就存在。

**修复。** 用字面量和其他常量来构建它。依赖于 `uniform`、纹理
采样或节点的值不是常量——应将其声明为普通的局部变量。

<a id="dsh7211"></a>

## DSH7211

<!-- generated:begin DSH7211 -->
**严重级别** 错误

**消息**

```
'{0}' is a file-scope variable with no storage class; write 'uniform' for a material parameter or 'static const' for a compile-time constant.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:753`
<!-- generated:end DSH7211 -->

**原因。** 文件作用域变量没有存储类别：`float Gain = 1.0;` 或 `static float …`。
两者都没有对应的节点。文件作用域的值要么是材质的输入（`uniform`），要么是折叠进材质的常量
（`static const`）。

**修复。** 写入 `uniform` 或 `static const`。单独使用 `const` 也可被接受，其含义与
`static const` 相同。

<a id="dsh7212"></a>

## DSH7212

<!-- generated:begin DSH7212 -->
**严重级别** 错误

**消息**

```
A file-scope variable of type {0} has no node; a 'uniform' or 'static const' must be numeric, bool, a texture or a sampler.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:780`
<!-- generated:end DSH7212 -->

**原因。** 文件作用域变量的类型没有对应的节点：`material`、`Substrate` 值、用户
`struct`、`void`。

**修复。** `material` 由入口生成，而不是声明出来的；`struct` 是编译期
聚合，存在于函数内部；`Substrate` 值来自 `Substrate.` 节点。

<a id="dsh7213"></a>

## DSH7213

<!-- generated:begin DSH7213 -->
**严重级别** 错误

**消息**

```
A texture uniform has no HLSL initializer; write its default asset as '/// @default /Game/...'.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:792`
<!-- generated:end DSH7213 -->

**原因。** 某个纹理 `uniform` 带有 HLSL 初始化值。纹理没有字面量值。

**修复。** 在声明上方以 `/// @default /Game/Textures/T_Name` 写入其默认资产，
并让声明本身保持为普通合法的 HLSL。

<a id="dsh7214"></a>

## DSH7214

<!-- generated:begin DSH7214 -->
**严重级别** 错误

**消息**

```
'{0}' is a compile-time constant and must be initialised where it is declared.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:803`、`Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:569`
<!-- generated:end DSH7214 -->

**原因。** 某个 `static const`——无论是在文件作用域还是函数内部——没有初始化值。

**修复。** 在声明处初始化它，或者去掉 `const` 使其成为普通变量。

<a id="dsh7215"></a>

## DSH7215

<!-- generated:begin DSH7215 -->
**严重级别** 错误

**消息**

```
'@layer' and '@layerblend' make two different assets; a function is one or the other.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:503`
<!-- generated:end DSH7215 -->

**原因。** 同一个函数同时带有 `@layer` 和 `@layerblend`。它们选择的是两个不同的资产
类别。

**修复。** 保留与签名匹配的那一个：`@layer` 用于 `void (inout material)`，
`@layerblend` 用于 `void (material …, inout material)`。

<a id="dsh7216"></a>

## DSH7216

<!-- generated:begin DSH7216 -->
**严重级别** 错误

**消息**

```
A 'uniform' array has no parameter node; declare one uniform per element, or make it 'static const'.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:825`
<!-- generated:end DSH7216 -->

**原因。** 某个 `uniform` 被声明为数组。不存在数组参数节点。

**修复。** 为每个元素声明一个 uniform，或者在值固定的情况下将其改为 `static const`，或者对需要在运行时
读取的表使用纹理。

<a id="dsh7220"></a>

## DSH7220

<!-- generated:begin DSH7220 -->
**严重级别** 错误

**消息**

```
'@slider' takes two numbers, a minimum and a maximum; '{0}' is not that.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:189`、`Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:199`
<!-- generated:end DSH7220 -->

**原因。** `@slider` 不是两个数字，或者其最小值不小于最大值。

**修复。** 写入 `/// @slider 0 4`。

<a id="dsh7221"></a>

## DSH7221

<!-- generated:begin DSH7221 -->
**严重级别** 错误

**消息**

```
'@sort' takes one whole number; '{0}' is not that.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:235`
<!-- generated:end DSH7221 -->

**原因。** `@sort` 不是一个整数。

**修复。** 写入 `/// @sort 3`。如果不写，参数将按声明顺序排序。

<a id="dsh7222"></a>

## DSH7222

<!-- generated:begin DSH7222 -->
**严重级别** 错误

**消息**

```
'@sampler' needs a sampler type after it, such as 'Color', 'Normal' or 'LinearColor'.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:271`
<!-- generated:end DSH7222 -->

**原因。** `@sampler` 没有值。

**修复。** 指定一个采样器类型：`Color`、`Normal`、`LinearColor`、`Grayscale`、`Masks`、…… 该列表
来自引擎，因此未知的写法会由发射器报告，而不是在这里报告。

<a id="dsh7223"></a>

## DSH7223

<!-- generated:begin DSH7223 -->
**严重级别** 错误

**消息**

```
'@static' asks for a static switch and is only meaningful on a 'uniform bool'.
```

**触发者** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:814`
<!-- generated:end DSH7223 -->

**原因。** `@static` 位于非 `uniform bool` 之上。它要求使用静态开关，
而只有布尔参数才能成为静态开关。

**修复。** 删除该指令，或将声明改为 `uniform bool`。

<a id="dsh7224"></a>

## DSH7224

<!-- generated:begin DSH7224 -->
**严重性** 警告

**消息**

```
'@{0}' means nothing here; it belongs on {1}.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:929`、`Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:138`
<!-- generated:end DSH7224 -->

**原因。** `///` 指令被写在了一个它没有任何意义的声明上：`@slider` 写在
函数上，`@library` 写在 uniform 上，`@sampler` 写在非纹理的对象上，`@asset` 写在并非 `extern`
原型的函数上，`@library` 或 `@name` 写在并非 `export` 的函数上。该指令会被丢弃。

**修复。** 将其移动到它所属的声明上，或删除它。未知键 **不是** 此警告 —— 它们会通过反射
传递给节点。

<a id="dsh7225"></a>

## DSH7225

<!-- generated:begin DSH7225 -->
**严重性** 警告

**消息**

```
'@static {0}' does not name a parameter of '{1}'.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1035`、`Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1057`、`Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1083`
<!-- generated:end DSH7225 -->

**原因。** `@param <name>` 所指定的名称并非函数的参数。通常是因为参数被重命名了，
而注释没有。

**修复。** 更正名称，或删除该行。

<a id="dsh7226"></a>

## DSH7226

<!-- generated:begin DSH7226 -->
**严重性** 警告

**消息**

```
'@custom {0}' is not a modifier this language knows; the only one is 'selfcontained'.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:467`
<!-- generated:end DSH7226 -->

**原因。** `@custom` 后跟的单词不是它所认识的修饰符。

**修复。** 唯一的修饰符是 `selfcontained`。`Inline`，即 1.x 的拼写，已被移除。

<a id="dsh7227"></a>

## DSH7227

<!-- generated:begin DSH7227 -->
**严重性** 错误

**消息**

```
'@{0}' needs a value after it.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:152`、`Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:303`、`Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:355`、`Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:382`、`Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:365`、`Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:475`
<!-- generated:end DSH7227 -->

**原因。** 需要值的指令没有值：`@name`、`@asset`、`@library`、`@default`，或
不带参数名称的 `@param`。

**修复。** 在键后写出值。指令的值一直延伸到下一个 ` @` 或该行的
末尾。

<a id="dsh7228"></a>

## DSH7228

<!-- generated:begin DSH7228 -->
**严重性** 警告

**消息**

```
'@custom' on '{0}' did not make its body opaque; the directive has to sit in the '///' block directly above the declaration.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1301`
<!-- generated:end DSH7228 -->

**原因。** `@custom` 被记录在一个函数上，而该函数体是按 DreamShaderLang 解析的，
而不是逐字捕获的。解析器只有在 `///`
块中 **紧邻** 声明上方看到 `@custom` 时，才会使函数体不透明。

**修复。** 移动 `@custom` 行，使其与声明之间没有任何内容。

<a id="dsh7229"></a>

## DSH7229

<!-- generated:begin DSH7229 -->
**严重性** 警告

**消息**

```
'@{0}' is written twice in this block; the last one wins.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:122`、`Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:402`
<!-- generated:end DSH7229 -->

**原因。** 同一个键在一个 `///` 块中出现了两次。最后一个生效。

**修复。** 删除较早的那个。`@param` 是唯一可以合法重复且从不
报告此问题的键。

<a id="dsh7230"></a>

## DSH7230

<!-- generated:begin DSH7230 -->
**严重性** 警告

**消息**

```
'#pragma layout' expects a whole number for '{0}'; '{1}' was ignored.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:702`、`Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:722`、`Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:784`、`Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:795`、`Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:807`、`Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:818`、`Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:835`
<!-- generated:end DSH7230 -->

**原因。** 有一行 `#pragma layout(...)` 无法读取：没有 `Node`/`Comment` 选择器、未知
键，或坐标不是整数。该行会被忽略，其他内容不受影响。

**修复。** 布局是由反编译器写入的，很少手工编辑；其形式为
`#pragma layout(Node, Var = UV, X = -1100, Y = -120)` 和
`#pragma layout(Comment, Text = "Sampling", X = …, Y = …, Width = …, Height = …)`。没有
布局条目的节点会由布局过程直接放置。

<a id="dsh7231"></a>

## DSH7231

<!-- generated:begin DSH7231 -->
**严重性** 错误

**消息**

```
'@static {0}' makes a parameter a static bool pin, which only a 'bool' input can be, and '{0}' is {1}.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1010`
<!-- generated:end DSH7231 -->

**原因。** 函数上的 `/// @static <Parameter>` 会使该参数成为 StaticBool 引脚 —— 一个
其值用于选择着色器排列的引脚 —— 而所命名的参数不是 `bool`。

**修复。** 指定一个 `bool` 参数，或删除该指令。

<a id="dsh7232"></a>

## DSH7232

<!-- generated:begin DSH7232 -->
**严重性** 错误

**消息**

```
'Substrate = {0}' is not a Substrate mode; the modes are 'Legacy', 'Bridge' and 'Native'.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:677`
<!-- generated:end DSH7232 -->

**原因。** `#pragma material(Substrate = ...)`（或 1.x `Shader` 的 `Substrate` 设置）指定的
不是这三种模式之一。

**修复。** 写入 `Legacy`（默认：按所写内容构建，由引擎进行转换）、`Bridge`（旧版
属性在 Substrate 项目中变成单个 `Substrate.ShadingModels` 节点）或 `Native`（源
为 Substrate 源）。

<a id="dsh7233"></a>

## DSH7233

<!-- generated:begin DSH7233 -->
**严重性** 警告

**消息**

```
A slider range ('@slider', or 'Slider(min, max)' in a 1.x property) is for a scalar parameter; this one is '{0}', which has no slider, so the range is ignored.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:210`
<!-- generated:end DSH7233 -->

**原因。** 滑块范围 —— `uniform` 上的 `/// @slider min max`，或 1.x
属性元数据中的 `Slider(min, max)` —— 位于并非标量的参数上：向量（`float3`、`vec3`、`float4`）、纹理，或 `bool`
（静态开关）。该范围是 ScalarParameter 的 `SliderMin` / `SliderMax`，而这些参数都没有
滑块，因此它无处可去并被丢弃。编译会继续。

**修复。** 删除该范围，或将其放到标量 `float`、`half` 或 `int` 参数上 —— 对于应保持在
某个范围内的颜色，可为每个通道使用一个标量，或使用一个标量来缩放该颜色。

<a id="dsh7234"></a>

## DSH7234

<!-- generated:begin DSH7234 -->
**严重性** 警告

**消息**

```
'{0}' asks to generate '{1}', and only a package's header generates material functions; this file is not in one, so nothing is generated. Write an 'export' function (or a 1.x ShaderFunction) to make a material function of your own.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:893`
<!-- generated:end DSH7234 -->

**原因。** 一个函数带有 `@generate` —— 即 2.0 `/// @generate` 或 1.x `Generate =` 头 —— 但该文件不在包中。只有包的头才会生成材质函数（[包 ▸ 资源头](../../tools/packages.md#asset-headers)）；在其他任何位置，该注解都不起任何作用。

**修复。** 要创建项目自有的材质函数，请在 `.dss` 中编写 `export` 函数（或在 `.dsf` 中编写 1.x `ShaderFunction`）。如果该头本应属于某个包，请将其安装到 `DShader/Packages` 下。

<a id="dsh7235"></a>

## DSH7235

<!-- generated:begin DSH7235 -->
**严重性** 警告

**消息**

```
'#pragma nogeneration' in '{0}' was ignored: it marks the file it is written in, a header builds nothing, and the files that include it are not marked by it. Write it in each of those files instead.
```

**引发自** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1353`
<!-- generated:end DSH7235 -->

**原因。** `#pragma nogeneration` *（2.1.2 之后）* 出现在头文件中。它标记的是写入它的文件；头文件不构建任何内容，而包含它的文件也不会被它标记。

**修复。** 在每个应为 NoGeneration 的源中写入该 pragma，或在其某个导出上方写入 `/// @nogeneration`。

<a id="dsh7236"></a>

## DSH7236

<!-- generated:begin DSH7236 -->
**严重性** 警告

**消息**

```
A 'disable_warning' or 'disable_error' list names no code, and disables nothing; write the codes after it, such as 'DSH7224'.
```

**引发自** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:741`
<!-- generated:end DSH7236 -->

**原因。** `disable_warning` 或 `disable_error` 列表 *（2.1.2 之后）* 中包含的单词不是诊断代码（`DSH` 加四位数字），或者根本没有代码。该单词会被忽略；列表从不会表示“全部”。参见[禁用诊断](../language-v2/disable-diagnostics.md)。

**修复。** 按报告时的原样写入代码，例如 `DSH7224`，用逗号或空格分隔。

<a id="dsh7250"></a>

## DSH7250

<!-- generated:begin DSH7250 -->
**严重性** 错误

**消息**

```
A '.dsi' needs one '#pragma instance(Parent = "...")' naming the material it is an instance of, and this file has none.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:315`
<!-- generated:end DSH7250 -->

**原因。** 一个 `.dsi` 文件没有 `#pragma instance(...)`。pragma 正是使该文件成为一个实例并说明它是什么的
东西。

**修复。** 在顶部添加 `#pragma instance(Parent = "M_Parent")`：对于与本文件并存的源文件，写一个裸名称；
对于任何材质，写一个对象路径。

<a id="dsh7251"></a>

## DSH7251

<!-- generated:begin DSH7251 -->
**严重性** 错误

**消息**

```
'#pragma instance' is written a second time, and one '.dsi' is one material instance; the line {0} already declares it.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:346`
<!-- generated:end DSH7251 -->

**原因。** 一个 `.dsi` 有两行 `#pragma instance`。一个文件就是一个实例。

**修复。** 将这些键合并为一个 pragma，或拆分该文件。

<a id="dsh7252"></a>

## DSH7252

<!-- generated:begin DSH7252 -->
**严重性** 错误

**消息**

```
'Parent' in '#pragma instance' names the material this is an instance of, and it is empty.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:430`、`Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:445`
<!-- generated:end DSH7252 -->

**原因。** `#pragma instance` 没有 `Parent`，或者 `Parent` 存在但为空。一个实例
不过是对某个父项的覆盖。

**修复。** 指明父项：DreamShader 材质用其名称，任何材质或实例用对象
路径 -- `#pragma instance(Parent = "M_Skin")`。

<a id="dsh7253"></a>

## DSH7253

<!-- generated:begin DSH7253 -->
**严重性** 错误

**消息**

```
'{0}' is set twice by '#pragma instance'; it was already set on line {1}.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:411`
<!-- generated:end DSH7253 -->

**原因。** 在 `#pragma instance(...)` 中某个键被写了两次。

**修复。** 保留一个。

<a id="dsh7254"></a>

## DSH7254

<!-- generated:begin DSH7254 -->
**严重性** 错误

**消息**

```
A '.dsi' is configured by '#pragma instance', and '#pragma material' configures a material; write these keys in '#pragma instance(...)'.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:213`、`Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:258`、`Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:272`、`Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:285`、`Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:298`
<!-- generated:end DSH7254 -->

**原因。** 一个 `.dsi` 包含了实例用不到的东西：`#pragma material(...)`（该 pragma
用于配置正在构建的材质）、一个不是 `uniform` 的声明、一个函数、一个 `struct`，
或一个 `#include`。实例没有图；它只赋值其父项的参数。

**修复。** 将 pragma 键移入 `#pragma instance(...)` -- 那里只接受实例可以覆盖的属性
（`BlendMode`、`TwoSided`、`ShadingModel`、`OpacityMaskClipValue`、...；参见
`Docs/language-v2/instances.md`）。将函数和类型移入 `.dss` 或 `.dsh`，并移除
包含。

<a id="dsh7255"></a>

## DSH7255

<!-- generated:begin DSH7255 -->
**严重性** 错误

**消息**

```
'#pragma instance' declares a material instance and belongs in a '.dsi' file of its own, and this line is in '{0}'; move it and its overrides into a '.dsi'.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:329`
<!-- generated:end DSH7255 -->

**原因。** `#pragma instance` 出现在 `.dss`（或头文件）中。材质及其实例是
彼此独立的资产和文件。

**修复。** 将 pragma 及其下方的覆盖移入它们自己的 `.dsi`。

<a id="dsh7256"></a>

## DSH7256

<!-- generated:begin DSH7256 -->
**严重性** 错误

**消息**

```
'{0}' overrides a {1} parameter and needs the value it is set to, as 'uniform {2} {0} = ...;', and it has no initializer.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:670`
<!-- generated:end DSH7256 -->

**原因。** 对标量、向量或开关参数的覆盖没有初始化器，因此没有
可设置的值。

**修复。** 写出该值：`uniform float Gain = 2.0;`。

<a id="dsh7257"></a>

## DSH7257

<!-- generated:begin DSH7257 -->
**严重性** 错误

**消息**

```
'{0}' overrides a {1} parameter, which takes an asset rather than an HLSL value; write '/// @default /Game/...' (or '/// @default None') above it instead of an initializer.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:645`、`Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:656`
<!-- generated:end DSH7257 -->

**原因。** 对纹理（或字体、运行时虚拟纹理、集合 ...）参数的覆盖被
赋予了初始化器，或者没有 `/// @default`。资产不是 HLSL 值；它由该
指令命名，与父项中完全相同。

**修复。** 在 `uniform Texture2D Name;` 上方写出 `/// @default /Game/Textures/T_Override`，或用 `///
@default None` 清除它。

<a id="dsh7258"></a>

## DSH7258

<!-- generated:begin DSH7258 -->
**严重性** 错误

**消息**

```
'{0}' is not a parameter of the parent '{1}'.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:856`、`Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:870`
<!-- generated:end DSH7258 -->

**原因。** `.dsi` 覆盖了一个不是其父项参数的名称。要么名称
错误，要么父项声明了该 uniform 但从不读取它，这种情况下该材质没有
这样的参数。

**修复。** 使用父项的 Details 面板所显示的参数名称（当
覆盖不是标识符时，在其上添加 `/// @name`）。`dsc index` 列出父项的参数。

<a id="dsh7259"></a>

## DSH7259

<!-- generated:begin DSH7259 -->
**严重性** 错误

**消息**

```
'{0}' is a {1} parameter of type '{2}' in the parent, and this override declares '{3}'.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:929`
<!-- generated:end DSH7259 -->

**原因。** 该覆盖声明的类型与父项参数的类型不同：对标量
参数使用 `float3`，对静态开关使用不带 `@static` 的 `bool`，对立方体纹理使用 `Texture2D`。

**修复。** 完全按照父项声明该 uniform 的方式来声明该覆盖。

<a id="dsh7260"></a>

## DSH7260

<!-- generated:begin DSH7260 -->
**严重性** 错误

**消息**

```
'/// @static' on an instance override means a static switch ('uniform bool') or a static component mask ('uniform bool4'), and '{0}' is declared '{1}'.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:585`、`Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:616`、`Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:903`、`Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:914`
<!-- generated:end DSH7260 -->

**原因。** `/// @static` 与父项不一致。该指令所在的覆盖
既不是 `uniform bool`（静态开关）也不是 `uniform bool4`（静态组件掩码）；或者
父项的参数是静态的而该覆盖缺少 `/// @static`；或者父项在运行时
设置该参数而该覆盖要求静态参数。

**修复。** 按照父项声明参数的方式来声明该覆盖：添加或移除 `///
@static`，或修正类型。

<a id="dsh7261"></a>

## DSH7261

<!-- generated:begin DSH7261 -->
**严重性** 错误

**消息**

```
'{0}' is overridden a second time, and one instance sets a parameter once; the override on line {1} already sets it.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:886`
<!-- generated:end DSH7261 -->

**原因。** 在 `.dsi` 中某个参数被覆盖了两次。

**修复。** 保留两行中的一行。

<a id="dsh7262"></a>

## DSH7262

<!-- generated:begin DSH7262 -->
**严重性** 警告

**消息**

```
'@{0}' has no effect above '#pragma instance', where only '@name' is read; remove it.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:379`、`Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:519`、`Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:682`
<!-- generated:end DSH7262 -->

**原因。** 描述参数的指令（`@group`、`@sort`、`@slider`、`@desc`、`@default`
作用于某个值）出现在实例覆盖上或 pragma 上方。该元数据属于父项；
实例只设置值。

**修复。** 将其移除。`@name`、`@static`、`@default`（作用于资产覆盖）和 `@page` 是
`.dsi` 会读取的指令。

<a id="dsh7263"></a>

## DSH7263

<!-- generated:begin DSH7263 -->
**严重性** 信息

**消息**

```
The parameters of the parent are not available here, so the names and types of these overrides are checked only for their shape; compile the parent first, or check the file in the editor.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:791`
<!-- generated:end DSH7263 -->

**原因。** 无法在该文件绑定的位置查找父项的参数——父项是
资产路径且未附加编辑器（语言服务检查），或父项源文件尚未
编译——因此只检查覆盖名称和类型的形态。

**修复。** 无需修复。先编译父项，或在编辑器运行时检查该文件，
以验证所有名称。

<a id="dsh7264"></a>

## DSH7264

<!-- generated:begin DSH7264 -->
**严重性** 错误

**消息**

```
'{0}' matches the parent parameter '{1}' only in case, and DreamShader names are case-sensitive; write '{1}'.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:845`
<!-- generated:end DSH7264 -->

**原因。** 覆盖只有在忽略大小写时才匹配父项参数。参数名称在
引擎中是大小写敏感的，因此该覆盖会静默地什么都不设置。

**修复。** 完全按照父项的拼写来拼写该名称。

<a id="dsh7265"></a>

## DSH7265

<!-- generated:begin DSH7265 -->
**严重性** 错误

**消息**

```
'{0}' is set to a value the compiler can fold, a literal or an expression over literals, and this initializer is not one.
```

**抛出者** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:778`
<!-- generated:end DSH7265 -->

**原因。** 覆盖的值是编译器无法求值的内容：它读取变量、调用节点或使用运行时函数。实例存储的是常量。

**修复。** 使用字面量或对字面量进行算术运算：`uniform float3 Tint = float3(1, 0.5, 0) *
0.5;`。

<a id="dsh7266"></a>

## DSH7266

<!-- generated:begin DSH7266 -->
**严重性** 错误

**消息**

```
'@page' takes one whole number of zero or more, and '{0}' is not that.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:506`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:695`
<!-- generated:end DSH7266 -->

**原因。** `/// @page` 不是零或更大的整数，或者它位于一个不是 Font 的覆盖上。

**修复。** 将字体页索引写在 Font 覆盖上——`/// @page 2`——而不要写在其他任何地方。

<a id="dsh7267"></a>

## DSH7267

<!-- generated:begin DSH7267 -->
**严重性** 警告

**消息**

```
'#pragma {0}' boxes or places graph nodes, and a '.dsi' has no graph; the line was ignored.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:222`
<!-- generated:end DSH7267 -->

**原因。** `#pragma region` 或 `#pragma layout` 位于 `.dsi` 中。两者都用于排列图节点，而实例没有图。

**修复。** 删除该行。

<a id="dsh7268"></a>

## DSH7268

<!-- generated:begin DSH7268 -->
**严重性** 错误

**消息**

```
'{0}' exists in the parent only as a layer or blend parameter, and a '.dsi' overrides global parameters only.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:828`
<!-- generated:end DSH7268 -->

**原因。** 该名称在父级中仅作为材质层或混合的参数存在。`.dsi` 设置全局参数；层参数关联的层索引，文件没有相应的语法。

**修复。** 在实例资产上手动覆盖它，或者在父级中将该值暴露为全局参数。

<a id="dsh7269"></a>

## DSH7269

<!-- generated:begin DSH7269 -->
**严重性** 错误

**消息**

```
'{0}' is an array, and an instance override assigns one parameter; override each element's parameter on its own.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:572`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:628`
<!-- generated:end DSH7269 -->

**原因。** 覆盖被声明为数组，或者使用了不是材质参数类型的类型。材质的每个参数都是一个名称和一个值：一个数字、一个布尔值、一个纹理，或者 `RuntimeVirtualTexture`、`SparseVolumeTexture`、`TextureCollection`、`ParameterCollection` 和 `Font` 之一。

**修复。** 在各自的行上覆盖每个元素的参数，并使用父级声明时所用的类型。

<a id="dsh7270"></a>

## DSH7270

<!-- generated:begin DSH7270 -->
**严重性** 错误

**消息**

```
'#pragma instance' takes 'Key = Value' pairs, and '{0}' has no key.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:395`
<!-- generated:end DSH7270 -->

**原因。** `#pragma instance(...)` 包含一个裸值。每个条目都是 `Key = Value`。

**修复。** 例如，写成 `#pragma instance(Parent = "M_Base", TwoSided = true)`。

<a id="dsh7300"></a>

## DSH7300

<!-- generated:begin DSH7300 -->
**严重性** 错误

**消息**

```
'{0}' is not a key of '#pragma pipeline'; the keys are {1}.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1153`
<!-- generated:end DSH7300 -->

**原因。** `#pragma pipeline(...)` 有一个键不属于它的五个键之一：`Order`、`Injection`、`Views`、`Requires` 和 `Enabled`。键区分大小写，如果只是大小写不同的拼写，会给出建议。缓冲区或通道的键（`Scale`、`Material`）也不是管线键。

**修复。** 更正该键，或将其移到所属位置：缓冲区键放入 `buffer Name : Format(Key = Value)`，通道键放入 `pass` 块内。

<a id="dsh7301"></a>

## DSH7301

<!-- generated:begin DSH7301 -->
**严重性** 错误

**消息**

```
'{0}' is set twice by '#pragma pipeline'; it was already set on line {1}.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1137`
<!-- generated:end DSH7301 -->

**原因。** 在 `#pragma pipeline(...)` 中一个键被写了两次。键按区分大小写进行比较，因此 `order` 与 `Order` 并列属于 DSH7300，而不是本条。

**修复。** 保留两者之一。

<a id="dsh7302"></a>

## DSH7302

<!-- generated:begin DSH7302 -->
**严重性** 错误

**消息**

```
(built at runtime)
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1348`
<!-- generated:end DSH7302 -->

**原因。** `#pragma pipeline` 的某个键取了它不能取的值；消息会说明是哪一个：

- `Order` 不是整数，或者被引号括起。它排列在某一注入点具有通道的管线，较小的在前。
- `Views` 或 `Requires` 被引号括起（`Views = "Game"`）：两者都接受不带引号书写的名称，而带引号的值会被拒绝而不是被丢弃，否则管线会停留在其默认视图或需求上。`Views = ""` 有它自己的消息：它不指定任何视图，因此管线将无处运行。
- `Views` 或 `Requires` 指定了一个不属于它自身的词。`Views` 接受 `Game`、`Editor`、`SceneCapture`、`PlanarReflection`、`ReflectionCapture` 和 `Thumbnail`；`Requires` 接受 `PostProcess`、`SceneResolve` 和 `CustomStencil`。仅大小写不同的匹配会给出建议。
- `Enabled` 不是名称：它接受文件的 `uniform bool`，或 `true`。

**修复。** 不带引号书写值，`Views` 和 `Requires` 的词由 `|` 连接：

```hlsl
#pragma pipeline(Order = 100, Views = Game | Editor, Requires = PostProcess, Enabled = bShowOutline)
```

不应运行的管线通过 `Enabled` 和一个 `uniform bool` 关闭，而不是通过一个空的 `Views`。

<a id="dsh7303"></a>

## DSH7303

<!-- generated:begin DSH7303 -->
**严重性** 错误

**消息**

```
'{0}' is not an injection point; the points are {1}.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1304`
<!-- generated:end DSH7303 -->

**原因。** `Injection`——属于 `#pragma pipeline` 或某个通道——未指定任何注入点。名称区分大小写，后处理链上的点带有其前缀：`AfterDOF` 被建议为 `PostProcess.AfterDOF`。

**修复。** 使用消息列出的某个点，按列出的拼写书写，并且在通道中不带引号：`Injection = PostProcess.AfterDOF;`。每个点能看到的内容见 `Docs/language-v2/passes.md`。

<a id="dsh7304"></a>

## DSH7304

<!-- generated:begin DSH7304 -->
**严重性** 错误

**消息**

```
'{0}' is not a pass kind; a pass is {1}.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1840`
<!-- generated:end DSH7304 -->

**原因。** `pass` 头指定了一个不存在的种类。种类有 `fullscreen`、`compute`、`mesh`、`clear` 和 `copy`，以小写书写；`Fullscreen` 被建议为 `fullscreen`。没有种类时，通道的键以及依赖该种类的规则不会被检查。

**修复。** 书写这五个之一：`pass Blur : compute { ... }`。

<a id="dsh7305"></a>

## DSH7305

<!-- generated:begin DSH7305 -->
**严重性** 错误

**消息**

```
'{0}' is not a buffer format; the formats are {1}.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1588`
<!-- generated:end DSH7305 -->

**原因。** `buffer` 声明指定了一个不存在的格式。格式有 `R8`、`RG8`、`RGBA8`、`R16F`、`RG16F`、`RGBA16F`、`R32F`、`RG32F`、`RGBA32F`、`R32U`、`RG32U` 和 `Depth32`，区分大小写；诸如 `PF_R8` 或 `R8_UNORM` 之类的引擎拼写不在其中。

**修复。** 书写其中之一：`buffer Mask : R8;`。`R32U` 和 `RG32U` 是保留的（DSH7321），而 `Depth32` 只是网格通道自身的深度（DSH7330）。

<a id="dsh7306"></a>

## DSH7306

<!-- generated:begin DSH7306 -->
**严重性** 错误

**消息**

```
'{0}' is not a key of a buffer; the keys are {1}.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1644`
<!-- generated:end DSH7306 -->

**原因。** 缓冲区的参数列表中有一个不是缓冲区键的键。这些键是 `Scale`、`Size`、`Resolution`、`Clear`、`Mips`、`History` 和 `Export`，区分大小写。格式不是键：它写在冒号之后。

**修复。** 更正该键：`buffer Blurred : R8(Scale = 0.5, Export = true);`。

<a id="dsh7307"></a>

## DSH7307

<!-- generated:begin DSH7307 -->
**严重性** 错误

**消息**

```
'{0}' is set twice for buffer '{1}'; it was already set on line {2}.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1627`
<!-- generated:end DSH7307 -->

**原因。** 一个键在缓冲区的参数列表中写了两次。

**修复。** 保留两者中的一个。

<a id="dsh7308"></a>

## DSH7308

<!-- generated:begin DSH7308 -->
**严重性** 错误

**消息**

```
(built at runtime)
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1351`
<!-- generated:end DSH7308 -->

**原因。** 某个缓冲区键的值不是它能够接受的值；消息会说明是哪一个。管线编译时数值会被折叠，因此 `static const` 可以，而 `uniform` 不行。

- `Scale` 不是数字，或超出 0.0625 到 4 的范围。
- `Size` 不是两个整数，或某一边超出 1 到 16384 的范围。
- `Resolution` 不是 `Render` 或 `Output`。`Resolution = Fixed` 也会被拒绝：固定大小写作 `Size = int2(w, h)`。
- `Clear` 不是数字，不是 `float2` 到 `float4`，也不是 `None`。
- `Mips` 不是整数 `1`。Pass 目前无法初始化或更新 mip 链。
- `History` 或 `Export` 不是 `true` 或 `false`。

**修复。** 按要求写入消息所要求的值：

```hlsl
buffer Wind : RG16F(Size = int2(256, 256), Clear = 0, History = true, Export = true);
```

<a id="dsh7309"></a>

## DSH7309

<!-- generated:begin DSH7309 -->
**严重性** 错误

**消息**

```
'Size' gives buffer '{0}' a fixed size, so '{1}' has nothing to say; remove one of them.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1765`
<!-- generated:end DSH7309 -->

**原因。** 某个缓冲区同时具有 `Size` 和 `Scale`（或 `Resolution`）。`Size` 使它成为固定数量的纹素；`Scale` 和 `Resolution` 则按视图来确定其大小，因此两者之一没有意义。

**修复。** 从跟随视图的缓冲区中移除 `Size`，或从固定大小的缓冲区中移除 `Scale` 和 `Resolution`。

<a id="dsh7310"></a>

## DSH7310

<!-- generated:begin DSH7310 -->
**严重性** 错误

**消息**

```
'{0}' is not a key of a {1} pass; its keys are {2}.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1960`
<!-- generated:end DSH7310 -->

**原因。** 某个 pass 具有任何类型的 pass 都不具有的键。每个 pass 都接受 `Injection` 和 `Enabled`；除此之外，`fullscreen` 接受 `Material`、`Shader`、`Entry`；`compute` 接受 `Shader`、`Entry`、`Threads`、`Dispatch`；`mesh` 接受 `Filter`、`Material`、`Mode`、`Depth`、`Cull`、`Blend`、`Usage`、`Nanite`、`NaniteValue`；`clear` 接受 `Value`；`copy` 不接受任何键。键区分大小写，若仅大小写不同会给出建议。

**修复。** 更正该键。绑定不是键：它以 `read`、`write` 或 `param` 开头。

<a id="dsh7311"></a>

## DSH7311

<!-- generated:begin DSH7311 -->
**严重性** 错误

**消息**

```
'{0}' is a key of {1} passes, and '{2}' is a {3} pass.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1937`
<!-- generated:end DSH7311 -->

**原因。** 某个 pass 具有属于另一种类型 pass 的键——`Threads` 出现在 `fullscreen` pass 中，`Filter` 出现在 `compute` 中，`Value` 出现在 `copy` 中。消息会指明该键所属的类型。

**修复。** 移除该键，或者如果该键正是本意，则更改 pass 的类型。

<a id="dsh7312"></a>

## DSH7312

<!-- generated:begin DSH7312 -->
**严重性** 错误

**消息**

```
'{0}' is set twice in pass '{1}'; it was already set on line {2}.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1906`
<!-- generated:end DSH7312 -->

**原因。** 一个键在同一个 pass 中写了两次。键按区分大小写进行比较，因此 `injection` 与 `Injection` 并列属于 DSH7310，而非此错误。

**修复。** 保留两者中的一个。

<a id="dsh7313"></a>

## DSH7313

<!-- generated:begin DSH7313 -->
**严重性** 错误

**消息**

```
(built at runtime)
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1354`
<!-- generated:end DSH7313 -->

**原因。** 某个 pass 键的值不是它能够接受的值；消息会说明是哪一个。管线编译时数值会被折叠，因此 `static const` 可以，而 `uniform` 不行。

- `Injection` 被加了引号或不是名称；`Enabled` 既不是 `true` 也不是名称（一个数字、一个带引号的字符串）。
- `Material` 或 `Shader` 不是带引号的非空字符串；`Entry` 不是函数名。
- `Threads` 不是两个或三个整数，每个方向至少为 1，z 方向至多 64，总共至多 1024。
- `Dispatch` 不是缓冲区，不是具有正 `n` 的 `<buffer> * n` 或 `<buffer> / n`，也不是两个或三个至少为 1 的整数。
- `Mode`、`Depth`、`Cull`、`Blend` 或 `Nanite` 不是其可选值之一，或者被加了引号；`Depth = Own(...)` 没有恰好包含一个缓冲区名称。
- `Usage` 不是用 `|` 连接的用法，或指定了不存在的用法（建议 `Splines` 应为 `SplineMesh`）。
- `Nanite = AssignStencil(n)` 没有单一的 1 到 255 的 `n`；`Nanite = StencilMask` 位于某个 pass 中，而该 pass 的 `Filter` 没有可供遮蔽的 `Stencil(...)` 项。
- `NaniteValue`（mesh）或 `Value`（clear）不是一到四个数字。
- `Stencil(...)` 过滤项的值或掩码不是整数。

**修复。** 按消息给出的形式写入该值。若某个 pass 按层或列表进行选择，并希望其 Nanite 成员也被覆盖，请使用 `Nanite = AssignStencil(n)`，它会为它们提供自定义模板 `n`。

<a id="dsh7314"></a>

## DSH7314

<!-- generated:begin DSH7314 -->
**严重性** 错误

**消息**

```
A filter joins its terms with '|' (either) and '&' (both), and nothing else.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2336`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2360`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2371`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2386`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2402`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2430`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2455`
<!-- generated:end DSH7314 -->

**原因。** 某个 mesh pass 的 `Filter` 不是过滤器；消息会说明是哪一部分：

- 各项的连结方式不是 `|`（任一）和 `&`（两者）——`||`、`&&` 和 `+` 不是过滤运算符；
- 某个项不是调用——`Filter = Highlight;` 用于 `Filter = Layer(Highlight);`——或调用了 `Stencil`、`Layer` 和 `List` 之外的东西；
- 某个项按名称传递参数：`Stencil(Value = 1)`；
- `Stencil` 没有参数或多于两个参数，或者值或掩码超出 0 到 255；
- `Layer` 为空或包含的不是用 `|` 连接的名称；`List` 包含的不是一个名称。

**修复。** 用这三个项构建过滤器；`&` 的优先级高于 `|`：

```hlsl
Filter = Stencil(4, 0x0F) | Layer(Enemies | Allies) & List(Visible);
```

<a id="dsh7315"></a>

## DSH7315

<!-- generated:begin DSH7315 -->
**严重性** 错误

**消息**

```
Mesh pass '{0}' needs 'Filter = ...' to say which primitives it draws: 'Stencil(1)', 'Layer(Name)', 'List(Name)'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3095`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3109`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3921`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4004`
<!-- generated:end DSH7315 -->

**原因。** 某个 pass 缺少其类型所需的键；消息会说明是哪一个：

- `fullscreen` pass 没有代码：没有 `Material`（一种后处理材质）、没有 `Shader`、没有它自己的 `hlsl` 块，也没有 `Entry` 指定该文件 `hlsl` 块中的某个函数；
- 带 `Shader` 的 `fullscreen` pass，或带 `Shader` 的 `compute` pass，没有 `Entry`；
- `compute` pass 没有代码：没有 `Shader`、没有 `hlsl` 块，也没有该文件块的 `Entry`；
- `mesh` pass 没有 `Filter`；
- `mesh` pass 声明了 `Mode = Override` 或 `Mode = OwnOrOverride`，却没有指定用于绘制的 `Material`。

**修复。** 添加键：`Material = "PP_Composite";`，或 `Shader = "BoxBlur.usf";` 与
`Entry = BlurCS;`，或将 pass 的 HLSL 放在它自己的 `hlsl { ... }` 块中，或 `Filter = Layer(Highlight);`。以自身材质绘制每个图元的
mesh pass 写作 `Mode = Own`（没有 `Material` 时的默认值）。

<a id="dsh7316"></a>

## DSH7316

<!-- generated:begin DSH7316 -->
**严重性** 错误

**消息**

```
Fullscreen pass '{0}' draws a 'Material' or runs a 'Shader', and it names both.
```

**由以下项引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2952`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2977`
<!-- generated:end DSH7316 -->

**原因。** 一个 `fullscreen` pass 的两个键相互矛盾。两条消息：该 pass 同时指定了
`Material` 和 `Shader`，而它要么绘制其中一个，要么运行另一个；或者它指定了 `Material` 和
`Entry`，而后者是着色器文件的函数。

**修复。** 对于后处理材质保留 `Material`，或对于 HLSL 保留 `Shader` 与 `Entry`，并移除
另一个。

<a id="dsh7317"></a>

## DSH7317

<!-- generated:begin DSH7317 -->
**严重性** 错误

**消息**

```
A fullscreen material pass writes exactly one buffer, as 'write Buffer;', and '{0}' writes {1}.
```

**由以下项引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2990`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3001`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3027`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3059`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3141`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3151`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3172`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3184`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3205`
<!-- generated:end DSH7317 -->

**原因。** 某个 pass 读取或写入了其类型所不具备的缓冲区数量或形状；消息会
说明是哪种：

- 带有 `Material` 的 `fullscreen` pass 写入的不是恰好一个缓冲区，或为其写入
  (`write Out = Mask;`) 命名：材质只有一个输出且没有名称；
- 带有 `Shader` 的 `fullscreen` pass，或 `compute` pass 不写入任何内容；
- `mesh` pass 读取一个缓冲区（其材质自行采样所需内容），写入零个或超过
  四个，或在写入时没有为它所采用的 `UE.DreamPassOutput` 输出命名（`Output0` 到 `Output3`），
  或写入整数缓冲区；
- `clear` pass 写入的不是恰好一个缓冲区且不读取任何缓冲区，或 `copy` pass 读取的不是一个
  且写入的不是一个。

**修复。** 绑定该类型所采用的：

```hlsl
pass Composite : fullscreen { Material = "PP_Composite"; read Mask; write SceneColor; }
pass DrawMask  : mesh       { Filter = Layer(Highlight); Material = "M_Mask"; write Output0 = Mask; }
pass Grab      : copy       { read SceneColor; write Half; }
```

<a id="dsh7318"></a>

## DSH7318

<!-- generated:begin DSH7318 -->
**严重性** 错误

**消息**

```
'{0}.Previous' is last frame's contents, which nothing writes any more; write '{0}'.
```

**由以下项引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2487`
<!-- generated:end DSH7318 -->

**原因。** 某个 pass 写入了 `B.Previous`。那是 `History = true` 缓冲区上一帧的内容，
只有 `read` 可以指定它；本帧的写入进入 `B`，即下一帧的 `.Previous`。

**修复。** 写入 `B`：`write Result = Wind;` 紧邻 `read Previous = Wind.Previous;`。

<a id="dsh7319"></a>

## DSH7319

<!-- generated:begin DSH7319 -->
**严重性** 错误

**消息**

```
'{0}' is bound twice in pass '{1}'; inside a pass every input, output and parameter has a name of its own.
```

**由以下项引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3256`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3339`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3430`
<!-- generated:end DSH7319 -->

**原因。** 该 pass 绑定的某个名称已被占用。在单个 pass 内，`read`、`write` 或
`param` 绑定的每个名称只使用一次。代码为 `.usf` 的 pass（`compute`，或 `fullscreen` 与
`Shader`）三者共用一个命名空间，因为每个绑定都会成为其 HLSL
槽位的 `#define`；在那里消息还涵盖：

- 以 `DP_` 开头的名称，即槽位自身参数的前缀；
- 等于该 pass 的 `Entry` 的名称，或槽位自身的入口点 (`DreamPassMainCS`、
  `DreamPassMainPS`)；
- 在 `BeginView` 处的 `View`，其中槽位自身定义了 `View`；
- 槽位从另一个绑定派生的名称：`read X` 还会定义 `XSize` 和 `XUVRect`，
  `write Y` 定义 `YSize`。

在材质或 mesh pass 中，读取、写入和参数各自在内部进行检查——
且忽略大小写：该 pass 将它们作为 Unreal 名称与其材质的 UserSceneTexture 输入和参数
匹配，因此 `read Mask` 和 `read mask` 会将同一个输入绑定两次。`.usf` pass 的名称保持
其大小写，正如它们所变成的 `#define` 一样。

**修复。** 重命名两者之一：

```hlsl
read  Source = Mask;
param Width  = OutlineWidth;   // not 'SourceSize', which the slot gives the size of 'Source'
```

<a id="dsh7320"></a>

## DSH7320

<!-- generated:begin DSH7320 -->
**严重性** 错误

**消息**

```
'{0}' is an array, and a pipeline has no array parameters or constants; declare one per element.
```

**由以下项引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:898`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:910`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:921`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:934`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:944`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:995`
<!-- generated:end DSH7320 -->

**原因。** `.dsp` 的 `uniform` 或 `static const` 不是管线能持有的内容。六条
消息：

- 它是一个数组；
- `uniform` 不是 `float` 到 `float4`、`int`、`bool` 或 `Texture2D`；
- `static const` 不是一至四个分量的数字或布尔值；
- `Texture2D` uniform 具有初始化器，而纹理接受资源；
- `static const` 没有初始化器；
- `uniform` 的默认值无法折叠（例如它读取了另一个 `uniform`），而该默认值会被写入
  管线资源。

**修复。** 为每个元素声明一个值，使用管线接受的类型，其默认值由
字面量和 `static const` 构建；纹理的默认值是 `/// @default`：

```hlsl
static const float HalfWidth = 1.5;

/// @default /Game/Textures/T_Noise
uniform Texture2D Noise;
uniform float     Radius = HalfWidth * 2.0;
```

<a id="dsh7321"></a>

## DSH7321

<!-- generated:begin DSH7321 -->
**严重性** 错误

**消息**

```
'{0}' is an integer format, which no pass can read or write yet: HLSL passes see float4 textures, and materials and mesh passes write floats. Use 'R32F' or 'RG32F' (an id is exact up to 16777216).
```

**由以下项引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1605`
<!-- generated:end DSH7321 -->

**原因。** 缓冲区被声明为 `R32U` 或 `RG32U`。目前没有 pass 能使用整数缓冲区：HLSL 槽位
将其输入声明为 `Texture2D<float4>`，将其计算输出声明为 `RWTexture2D<float4>`，这与
整数视图不匹配，而材质、mesh pass、导出和可视化器读取和写入的
都是浮点数。

**修复。** 使用 `R32F` 或 `RG32F`。浮点数可以精确表示整数，上限为 16777216 (2^24)，足以
用于对象 id。

<a id="dsh7322"></a>

## DSH7322

<!-- generated:begin DSH7322 -->
**严重性** 错误

**消息**

```
'Enabled' is false, so this would never run, and a pipeline asset has no switch that is always off; drive it with a 'uniform bool', or comment the declaration out.
```

**由以下项引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1099`
<!-- generated:end DSH7322 -->

**原因。** `Enabled` —— `#pragma pipeline` 或某个 pass 的 —— 是 `false`，或是
折叠为 `false` 的 `static const bool`。管线资源没有始终关闭的开关：管线或该 pass 将
永远不会运行。

**修复。** 用 `uniform bool` 来驱动它，激活可以将其打开，且其默认值可以为
`false`，或将声明注释掉：

```hlsl
uniform bool bDebugView = false;

pass Debug : fullscreen
{
    Enabled = bDebugView;
    ...
}
```

<a id="dsh7325"></a>

## DSH7325

<!-- generated:begin DSH7325 -->
**严重性** 错误

**消息**

```
A fullscreen material pass needs the scene textures, which do not exist yet at {0}; run '{1}' at AfterBasePass or later.
```

**由以下项引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4396`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4407`
<!-- generated:end DSH7325 -->

**原因。** 某种类型的 pass 在无法工作的注入点运行。三条消息：

- 在 `BeginView` 或 `BeforeBasePass` 处带有 `Material` 的 `fullscreen` pass：后处理材质
  需要场景纹理，而这些纹理尚不存在；
- 在 `BeginView` 处的 `mesh` pass，此时视图尚无深度；
- 在 `EndOfView` 处的 `mesh` pass，此时视图处于输出分辨率且没有可用于绘制的
  深度。

**修复。** 移动该 pass：将全屏材质移至 `AfterBasePass` 或更晚（`BeforePostProcess` 是
已知可工作的位置），将 mesh pass 移至从 `BeforeBasePass` 到 `PostProcess.AfterFXAA` 的某个点。
必须在 `BeginView` 运行的工作应作为 `compute` pass，或带有 `Shader` 的 `fullscreen` pass。

<a id="dsh7326"></a>

## DSH7326

<!-- generated:begin DSH7326 -->
**严重性** 信息

**消息**

```
At AfterBasePass scene colour holds the emissive light only; nothing is lit yet, so pass '{0}' {1} that.
```

**触发自** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4371`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4419`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4432`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4442`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4452`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4463`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4560`
<!-- generated:end DSH7326 -->

**原因。** 通道在其运行位置所看到的内容；文件本身没有错误。消息会说明是哪一种：

- 该通道在 `AfterBasePass` 处读取或写入 `SceneColor`，而在该处它仅保存自发光光照；
- 带有 `Material` 的 `fullscreen` 通道在 `AfterBasePass` 或 `AfterOpaque` 处运行，而这些位置尚未在
  此引擎上验证过；`BeforePostProcess` 是已知可正常工作的位置；
- 一个 HLSL 通道——带有 `Shader` 的 `fullscreen` 通道，或一个 `compute` 通道——在 `BeginView` 处运行，
  而此处尚不存在任何场景纹理（场景纹理是占位符，且 `View` 完全无法使用：
  `DP_Time` 代替其时序），或在 `BeforeBasePass` 处运行，而此处只有场景深度；
- 在 `BeforeBasePass` 处的 `mesh` 通道针对场景深度（`Depth = TestScene`）进行测试，而场景深度
  仅保存深度预通道所绘制的内容；
- 在放大器之后（`PostProcess.ReplaceTonemapper` 到 `PostProcess.AfterFXAA`）的 `mesh` 通道
  针对场景深度进行测试，而场景深度仍处于渲染分辨率——无论是直接测试，还是通过一份被带入
  另一尺寸输出的像素中的副本来测试——因此其深度测试的精细程度仅与渲染分辨率相当；
- 一个 `mesh` 通道写入场景的某个纹理（`SceneColor`，位于 `AfterBasePass` 的 GBuffer，
  `Translucency`）并针对其自身深度 `Depth = Own(...)` 进行测试，而该深度是从其角落绑定的：
  在不从场景纹理的角落开始的视图中——分屏的第二个视图、立体声中的右眼——该深度无法到达该视图的像素，
  运行时会在该处跳过此通道。

**修复。** 如果这正是该通道需要看到的内容，则无需更改。否则将其移到更晚的
位置，或在 `BeforeBasePass` 处为网格通道提供 `Depth = None` 或 `Depth = Own(...)`。需要在分屏或立体声中运行且
拥有自身深度的网格通道应写入管线的缓冲区而不是场景的纹理，以便后续通道将其带入场景，或针对
场景深度进行测试。

<a id="dsh7327"></a>

## DSH7327

<!-- generated:begin DSH7327 -->
**严重性** 错误

**消息**

```
'CustomStencil' is the stencil half of the custom depth texture, which no pass can bind as a texture of its own, and pass '{0}' {1} it; read it through the scene textures instead: a SceneTexture node in the material, CalcSceneCustomStencil in a '.usf'.
```

**触发自** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4270`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4330`
<!-- generated:end DSH7327 -->

**原因。** 通道在无法拥有某个内置纹理的位置绑定了它。共有三类消息：

- `CustomStencil`，任何位置：它是自定义深度纹理的模板部分，不是通道可以自行绑定的
  纹理；
- `Translucency` 出现在后处理链之外（`PostProcess.*`），而后者是它唯一存在的位置；
- 另一个内置纹理在其存在之前被绑定：`SceneDepth` 从 `BeforeBasePass` 起；`SceneColor`、GBuffer、
  `Velocity` 以及 `CustomDepth` 从 `AfterBasePass` 起（`CustomDepth` 从 `AfterOpaque` 起可确定，
  DSH7329）。

**修复。** 在该纹理存在的位置运行通道。自定义模板通过场景纹理读取：材质中的
`SceneTexture` 节点（`UE.SceneTexture(SceneTextureId = CustomStencil)`），
`CalcSceneCustomStencil` 位于 `.usf` 中。

<a id="dsh7328"></a>

## DSH7328

<!-- generated:begin DSH7328 -->
**严重性** 错误

**消息**

```
(built at runtime)
```

**触发自** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4359`
<!-- generated:end DSH7328 -->

**原因。** 通道在其运行位置写入了无法写入的内置纹理；消息会说明是哪一种：

- `SceneColor` 位于 `PostProcess.TranslucencyAfterDOF`，此处后处理链承载
  半透明，且场景颜色为只读；
- `Translucency` 位于除 `PostProcess.TranslucencyAfterDOF` 以外的任何位置；
- 属于渲染器的纹理：`SceneDepth`、`CustomDepth` 和 `Velocity` 在任何位置，`GBufferA` 到
  `GBufferF` 在任何位置，但 `AfterBasePass` 除外，此处 GBuffer 是原地写入的。

**修复。** 改为写入管线的缓冲区，或将通道移到该纹理可写入的位置：
`Translucency` 位于 `PostProcess.TranslucencyAfterDOF`，`SceneColor` 位于从
`AfterBasePass` 起的任何其他位置。

<a id="dsh7329"></a>

## DSH7329

<!-- generated:begin DSH7329 -->
**严重性** 信息

**消息**

```
'{0}' exists at AfterBasePass only when r.CustomDepth.Order draws custom depth before the base pass; where it does not, pass '{1}' reads a cleared placeholder. From AfterOpaque on it always exists.
```

**触发自** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4302`
<!-- generated:end DSH7329 -->

**原因。** 位于 `AfterBasePass` 的通道读取 `CustomDepth`，只有在 `r.CustomDepth.Order` 如此指示时，它才会在基础通道之前
被绘制：`0`，或是在启用 DBuffer 贴花时的默认值 `2`。使用 `1`，或使用不带 DBuffer 贴花的
`2` 时，该通道读取的是已清除的占位符。

**修复。** 如果项目先绘制自定义深度，则无需修复。否则在
`AfterOpaque` 或更晚的位置运行该通道，此处自定义深度始终存在。

<a id="dsh7330"></a>

## DSH7330

<!-- generated:begin DSH7330 -->
**严重性** 错误

**消息**

```
'Depth = Own({0})' tests against a depth of this pipeline's own, and '{0}' is a built-in texture; declare 'buffer MyDepth : Depth32;', or write 'Depth = TestScene'.
```

**触发自** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2153`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2168`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2523`
<!-- generated:end DSH7330 -->

**原因。** 某个 `Depth32` 缓冲区被用作网格通道自身深度以外的东西，或者自身深度
不是 `Depth32` 缓冲区。共有三类消息：

- `Depth = Own(SceneDepth)`，或另一个内置纹理：`Own` 针对管线自身的深度
  在通道的图元之间进行测试；
- `Depth = Own(B)` 位于 `B` 不是 `Depth32` 的位置；
- `read` 或 `write` 绑定了 `Depth32` 缓冲区——在 `copy` 通道中也是如此。深度缓冲区
  只会被测试和重置：它所接受的唯一绑定是 `clear` 通道的 `write`，其
  `Value` 随后即为它所清除到的深度。

**修复。** 为 `Own` 声明一个深度缓冲区，并在其他所有位置绑定颜色缓冲区——重置深度的
`clear` 通道（`write ObjDepth;`）是唯一的例外；对于场景深度，请写入
`Depth = TestScene`：

```hlsl
buffer ObjDepth   : Depth32;
buffer Silhouette : RG16F;

pass DrawObjects : mesh
{
    Filter   = List(Enemies);
    Material = "M_XRayDepth";
    Depth    = Own(ObjDepth);
    write Output0 = Silhouette;
}
```

<a id="dsh7331"></a>

## DSH7331

<!-- generated:begin DSH7331 -->
**严重性** 错误

**消息**

```
Pass '{0}' reads '{1}', which no pass writes, and '{1}' is 'Clear = None', so what it reads is undefined.
```

**触发自** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4723`
<!-- generated:end DSH7331 -->

**原因。** 某个通道在任何通道写入它之前读取了声明为 `Clear = None` 的缓冲区，这是按帧内顺序
（注入点，然后是声明顺序）而言的。共有两类消息：没有任何通道写入它，或者它的第一个
写入者在该读取者之后运行。若没有清除值，这种读取所看到的内容是未定义的。

**修复。** 将读取者移到写入者之后，为该缓冲区提供一个 `Clear` 值（`Clear = 0` 是
默认值），或读取 `History = true` 缓冲区上一帧的 `B.Previous`。

<a id="dsh7332"></a>

## DSH7332

<!-- generated:begin DSH7332 -->
**严重性** 警告

**消息**

```
Pass '{0}' reads '{1}' before '{2}' writes it in the frame, so it reads the buffer's 'Clear' value; move the reader after the writer, or read '{1}.Previous'.
```

**触发自** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4733`
<!-- generated:end DSH7332 -->

**原因。** 某个通道在第一个写入它的通道之前读取了缓冲区，这是按帧内顺序
（注入点，然后是声明顺序）而言的，因此它读取的是该缓冲区的 `Clear` 值，而非该写入者
所绘制的内容。通常这两个通道位于顺序错误的注入点上。

**修复。** 将读取者移到写入者之后——文件中的更晚位置，或更晚的注入点。如果读取者
想要的是上一帧的内容，则读取 `History = true` 缓冲区的 `B.Previous`。

<a id="dsh7333"></a>

## DSH7333

<!-- generated:begin DSH7333 -->
**严重性** 错误

**消息**

```
Pass '{0}' reads and writes '{1}', and one pass cannot have one texture as its input and its output; write another buffer, or read '{1}.Previous' of a 'History = true' buffer.
```

**触发自** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4497`
<!-- generated:end DSH7333 -->

**原因。** 一个通道读取并写入同一张纹理，而一个 GPU 通道不能将同一张纹理同时作为其输入和输出。`B.Previous` 和 `B` 是两张纹理，不计入。链的颜色也不计入：任何类型的通道——`fullscreen` 或 `compute` 通道，或 `copy`——都可以在写入 `SceneColor` 的同时读取它，并在 `PostProcess.TranslucencyAfterDOF` 处 `Translucency`，因为写入它的通道会绘制到一张临时纹理，之后再被取回。

**修复。** 写入另一个缓冲区，并在后续通道中读取它，或者读取 `History = true` 缓冲区的 `B.Previous`。

<a id="dsh7334"></a>

## DSH7334

<!-- generated:begin DSH7334 -->
**严重性** error

**消息**

```
'{0}' reads no UserSceneTexture of '{1}': its inputs are {2}.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3563`
<!-- generated:end DSH7334 -->

**原因。** 一个 `fullscreen` 通道绑定了 `read X = B`，而其材质没有名为 `X` 的 `UE.UserSceneTexture`；该消息列出了它具有的名称。`X` 是材质中的名称，`B` 是管线的缓冲区，`read B;` 是 `read B = B;` 的简称。实例重命名的输入以新名称计入。对材质从未构建过的 `.dsp` 的 `dsc check` 会从其源读取材质的输入，而看不到它所调用的材质函数内部的 `UE.UserSceneTexture`。

**修复。** 绑定到材质读取的名称——`read Mask = Blurred;`——或者将该节点添加到材质中：

```hlsl
float Halo = UE.UserSceneTexture(UserSceneTexture = "Blurred", Coordinates = UV).Color.r;
```

<a id="dsh7335"></a>

## DSH7335

<!-- generated:begin DSH7335 -->
**严重性** warning

**消息**

```
'{0}' reads the UserSceneTexture '{1}', and pass '{2}' binds nothing to it, so it samples black; add 'read {1} = <Buffer>;'.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3578`
<!-- generated:end DSH7335 -->

**原因。** 某个 `fullscreen` 通道的材质读取了该通道未绑定缓冲区的 `UE.UserSceneTexture`。引擎仍会为该输入提供一个槽位，它会采样黑色。

**修复。** 绑定它——`read <Name> = <Buffer>;`——或者从材质中移除该节点。

<a id="dsh7336"></a>

## DSH7336

<!-- generated:begin DSH7336 -->
**严重性** error

**消息**

```
Fullscreen material pass '{0}' reads {1} buffers, and a post-process material has {2} input slots, shared with its own SceneTexture nodes; split the pass in two, or write it with 'Shader =' (a '.usf' pass reads up to {3}).
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3011`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3597`
<!-- generated:end DSH7336 -->

**原因。** 一个带有 `Material` 的 `fullscreen` 通道所需的输入多于后处理材质所拥有的数量。引擎为它提供五个共享的后处理输入槽位：每个读取 `PostProcessInput0` 到 `PostProcessInput4` 的 `SceneTexture` 节点占用一个，而材质的每个 `UE.UserSceneTexture` 输入，无论是否绑定，都占用其余槽位中的一个。有两种消息：该通道读取超过五个缓冲区；或者材质的 `UserSceneTexture` 输入无法与它自己的 `SceneTexture` 节点并存——一个为场景颜色读取 `PostProcessInput0` 的材质只剩下四个。

**修复。** 在单个通道中读取更少的缓冲区：将其拆分为两个，或者将多个通道打包到一个缓冲区中。或者用 `Shader =` 编写该通道；一个 `.usf` 通道最多可读取八个。

<a id="dsh7337"></a>

## DSH7337

<!-- generated:begin DSH7337 -->
**严重性** error

**消息**

```
'{0}' is a {1} material, and a fullscreen pass draws a Post Process one: '#pragma material(Domain = PostProcess)'.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3547`
<!-- generated:end DSH7337 -->

**原因。** 一个 `fullscreen` 通道绘制了一个域不是 Post Process 的材质。该通道通过引擎的后处理材质通道运行该材质，而该通道只绘制该域。

**修复。** 为该材质指定 `#pragma material(Domain = PostProcess)`。

<a id="dsh7338"></a>

## DSH7338

<!-- generated:begin DSH7338 -->
**严重性** error

**消息**

```
Pass '{0}' writes a data buffer, and '{1}' scales what it reads and writes by the exposure; give it '#pragma material(bDisablePreExposureScale = true)'.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3614`
<!-- generated:end DSH7338 -->

**原因。** 一个带有 `Material` 的 `fullscreen` 通道写入的是管线的缓冲区而不是场景颜色，而该材质保持预曝光开启。引擎随后会按曝光缩放该材质读取和写入的内容，因此存储在缓冲区中的遮罩或向量会随场景的亮度而变化。

**修复。** 在该材质的 `.dss` 中将其关闭：

```hlsl
#pragma material(Domain = PostProcess, bDisablePreExposureScale = true)
```

<a id="dsh7339"></a>

## DSH7339

<!-- generated:begin DSH7339 -->
**严重性** warning

**消息**

```
'{0}' is compiled for BlendableLocation {1}, and pass '{2}' runs it at {3}, {4} tonemapping, so its colours are in another space than it expects; use {5}.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3630`
<!-- generated:end DSH7339 -->

**原因。** 一个 `fullscreen` 通道在其材质编译时所针对的 `BlendableLocation` 的色调映射另一侧运行该材质，因此其颜色所处的空间与它预期的不同：一个为 `SceneColorAfterTonemapping`（引擎的默认值）或 `ReplacingTonemapper` 编译的材质位于色调映射之前的某个点，或者一个为 `PostProcess.ReplaceTonemapper`、`PostProcess.AfterTonemap`、`PostProcess.AfterFXAA` 或 `EndOfView` 处的另一位置编译的材质。

**修复。** 为该材质指定与该通道注入点匹配的位置——色调映射之前的 `SceneColorAfterDOF` 或 `SceneColorBeforeDOF`，色调映射之后的 `SceneColorAfterTonemapping`：

```hlsl
#pragma material(Domain = PostProcess, BlendableLocation = SceneColorAfterDOF)
```

<a id="dsh7340"></a>

## DSH7340

<!-- generated:begin DSH7340 -->
**严重性** error

**消息**

```
'{0}' has no UE.DreamPassOutput in its graph, so a mesh pass has nothing to write; add 'UE.DreamPassOutput(Output0 = ...);' to the material.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3664`
<!-- generated:end DSH7340 -->

**原因。** 一个 `mesh` 通道的覆盖 `Material` 在其自身的图表中没有 `UE.DreamPassOutput`。该节点是网格通道据以写入其输出的节点，也是引擎为该材质编译网格通道着色器的原因；材质所调用的材质函数内部的那个节点不计入（DSH5302）。在 Unreal Engine 5.8 及更高版本（即存在 Custom Pass 运行时的版本）上会进行检查。

**修复。** 将该节点添加到材质中：

```hlsl
UE.DreamPassOutput(Output0 = float4(1, 0, 0, 0));
```

<a id="dsh7341"></a>

## DSH7341

<!-- generated:begin DSH7341 -->
**严重性** error

**消息**

```
'{0}' leaves Output{1} of its UE.DreamPassOutput unconnected, so '{2}' would receive nothing.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3681`
<!-- generated:end DSH7341 -->

**原因。** 一个 `mesh` 通道写入了 `UE.DreamPassOutput` 的一个输出——`write Output1 = Id;`——其引脚在材质中未连接，因此该缓冲区将收不到任何内容。

**修复。** 在材质中连接该引脚——`UE.DreamPassOutput(Output0 = Mask, Output1 = Id);`——或者移除该材质未填充的写入。

<a id="dsh7342"></a>

## DSH7342

<!-- generated:begin DSH7342 -->
**严重性** error

**消息**

```
Mesh pass '{0}' draws {1} primitives, and '{2}' is not compiled for them; give the material its usage flag ('#pragma material({3} = true)' in its '.dss'), or leave {1} out of 'Usage'.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3702`
<!-- generated:end DSH7342 -->

**原因。** 一个 `mesh` 通道绘制了其覆盖材质未针对编译的某种图元。该通道的 `Usage`——未指定时为 `StaticMesh | InstancedStaticMeshes | SkeletalMesh`——列出了它绘制的内容，每种类型都需要材质的用法标志；`Landscape` 不需要任何标志。该消息会指出缺少的标志。在 Unreal Engine 5.8 及更高版本（即存在 Custom Pass 运行时的版本）上会进行检查。

**修复。** 在该材质的 `.dss` 中设置该标志，或者将 `Usage` 缩小到该通道所遇到的内容（`Usage = StaticMesh | InstancedStaticMeshes;`）：

```hlsl
#pragma material(ShadingModel = Unlit, bUsedWithSkeletalMesh = true, bUsedWithInstancedStaticMeshes = true)
```

<a id="dsh7343"></a>

## DSH7343

<!-- generated:begin DSH7343 -->
**严重性** error

**消息**

```
Mesh pass '{0}' writes '{1}', a texture of the scene, together with '{2}', a buffer of the pipeline. A mesh pass draws all its targets through one viewport, so they have to be one size holding the view at one place; the scene's textures are usually larger than the view they hold (rounded up, and in the editor grown to the largest view so far) while a buffer is the view's size, and the runtime skips the pass whenever the two differ. Write them in two mesh passes.
```

**由以下引发** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4525`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4542`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4575`
<!-- generated:end DSH7343 -->

**原因。** 一个 `mesh` 通道的目标会通过一个视口一起绘制。有三条消息：

- 该 pass 会写入场景的一个纹理 —— `SceneColor`、位于 `AfterBasePass` 的 GBuffer、
  `Translucency` —— 以及管线的一个 buffer。场景的纹理通常比它们所承载的视图更大
  （会向上取整，在编辑器中会增长到目前最大的视图），而 buffer 是视图的大小，并且当两者不一致时
  运行时就会跳过该 pass；
- 它的两个输出大小不同（`Render` 与 `Render x 0.5`，一个固定的 `256 x 256` 与
  视图）；
- 它自身的深度（`Depth = Own(...)`）小于它的输出。自身深度会按其本身被绑定，因此
  它必须至少与输出同样大：采用与输出相同的分辨率且缩放不小于输出，或采用一个不小于输出的固定
  大小。若深度与输出的分辨率不同（`Output` 与
  `Render`，一个固定大小与一个跟随视图的大小），则无法在帧之前进行比较，
  同样会被拒绝。

`Depth = TestScene` 对输出的大小没有任何要求：当输出在场景深度的像素处不承载视图时，
运行时会针对一份被引入到输出尺寸中的场景深度副本进行测试。

**修正。** 为输出指定一个 `Scale`、`Resolution` 或 `Size`，并让自身深度采用它们的分辨率以及一个
不小于它们的缩放（或固定大小）；若要更小的结果，请以完整大小绘制，然后 `copy`
到一个更小的 buffer 中。将场景的一个纹理与管线的一个 buffer 写入两个
网格 pass 中。

<a id="dsh7344"></a>

## DSH7344

<!-- generated:begin DSH7344 -->
**严重性** 错误

**消息**

```
'{0}' is a {1} material, and a mesh pass draws primitives with a Surface one.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3652`
<!-- generated:end DSH7344 -->

**原因。** 一个 `mesh` pass 的 override `Material` 不是 Surface 材质 —— 而是一个后处理、一个贴花、一个
UI 材质。网格 pass 会用它来绘制图元，就像图元自身材质绘制它们那样。

**修正。** 将 override 设为 Surface 材质（默认域：从其
`#pragma material` 中移除 `Domain`）。后处理材质由 `fullscreen` pass 绘制。

<a id="dsh7345"></a>

## DSH7345

<!-- generated:begin DSH7345 -->
**严重性** 错误

**消息**

```
'{0}' asks for the new material translator, which UE.DreamPassOutput does not support; remove 'bEnableNewHLSLGenerator' from it.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3716`
<!-- generated:end DSH7345 -->

**原因。** 一个 `mesh` pass 的 override `Material` 设置了 `bEnableNewHLSLGenerator`，而 `UE.DreamPassOutput`
未实现新的材质转换器；该材质自身的 `.dss` 也报告了同样的情况，即
DSH5301。在 Unreal Engine 5.8 及更高版本上检查，也就是 Custom Pass 运行时所在之处。

**修正。** 从该材质的 `#pragma material` 中移除 `bEnableNewHLSLGenerator`。

<a id="dsh7346"></a>

## DSH7346

<!-- generated:begin DSH7346 -->
**严重性** 错误

**消息**

```
Fullscreen pass '{0}' reads {1} and writes {2} buffers, and the pixel slot a '.usf' pass runs in has {3} inputs and {4} outputs.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3038`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3069`
<!-- generated:end DSH7346 -->

**原因。** 一个 HLSL pass 绑定的数量超过了其槽位所能容纳的数量。一个 `compute` pass 以及一个带有
`Shader` 的 `fullscreen` pass 会运行在一个固定的着色器槽位中 —— 对于全屏 `.usf` 来说是像素槽位，
无论其输出数量如何 —— 具有 8 个输入（`DP_Input0` 到 `DP_Input7`）和 4 个输出。

**修正。** 将该 pass 拆分为两个，或将值打包进更少的 buffer（一个 `RGBA16F` buffer 容纳四个
通道）。

<a id="dsh7347"></a>

## DSH7347

<!-- generated:begin DSH7347 -->
**严重性** 错误

**消息**

```
'{0}' is a {1}, and the parameter block of a shader slot holds numbers only, so no texture parameter reaches a '.usf' pass. A material takes one through 'param': draw this pass with a material ('Material = ...'), or let a fullscreen material pass that takes the texture by 'param' write it into a buffer this pass reads.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3460`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3475`
<!-- generated:end DSH7347 -->

**原因。** 一个 HLSL pass 的 `param` 不适合该槽位的参数块 `DP_Params`：16 个 `float4`，
且仅为数字。有两条消息：

- `param` 传递了一个 `Texture2D` uniform，而它在数字中没有位置；
- 参数按顺序打包 —— 一个 `float4` 占用一个自己的向量，一个 `float3` 占用一个
  空向量的 `xyz`，一个 `float2` 占用一个空闲的一半，一个标量占用下一个空闲分量 —— 需要超过 16 个向量。

**修正。** 纹理参数通过 `param` 到达材质，而绝不通过槽位：使用
`Material = ...` 绘制该 pass，并用 `param` 将纹理交给材质，或者让一个全屏材质 pass
通过 `param` 接收它，并将其写入一个 buffer，随后由 `.usf` pass `read` 它。对于数字过多的情况，
减少传递的参数，或自行将它们打包进 `float4` uniform 中。

<a id="dsh7350"></a>

## DSH7350

<!-- generated:begin DSH7350 -->
**严重性** 错误

**消息**

```
Pass '{0}' writes scene colour, which is {1} at {2}, and '{3}' with it, which is {4}; targets drawn together have one size.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4605`
<!-- generated:end DSH7350 -->

**原因。** 一个带有 `Shader` 和多个输出的 `fullscreen` pass 写入了 `SceneColor` 以及一个
其他大小的 buffer，而一起绘制的目标只有一个大小。场景颜色处于缩放 1：
在上采样器之前是渲染分辨率，从 `PostProcess.ReplaceTonemapper` 起是输出分辨率。一个
`mesh` pass 不在此处检查：它完全可能既写入管线的一个 buffer，又写入场景的
一个纹理（DSH7343）。

**修正。** 让该 buffer 具有该点处场景颜色的大小：`Scale = 1`（默认值）以及
与之匹配的 `Resolution`，当 buffer 未指定时将从其第一个写入者处获取。在其他大小的 buffer 请
在其自己的 pass 中写入。

<a id="dsh7351"></a>

## DSH7351

<!-- generated:begin DSH7351 -->
**严重性** 错误

**消息**

```
Pass '{0}' replaces the tonemapper, and so does '{1}'; one view runs one tonemapper, so a pipeline replaces it at most once.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4642`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4656`
<!-- generated:end DSH7351 -->

**原因。** 有两条消息，都与 `PostProcess.ReplaceTonemapper` 有关：管线的第二个 pass 在此处运行，
而一个视图只运行一个色调映射器；或者在此处运行的 pass 没有写入 `SceneColor`，而
色调映射器的替代者必须产生该帧的最终颜色。

**修正。** 在 `PostProcess.ReplaceTonemapper` 处保留一个 pass，并使用 `write SceneColor;`。跟随
色调映射器之后的工作则交给 `PostProcess.AfterTonemap`。

<a id="dsh7352"></a>

## DSH7352

<!-- generated:begin DSH7352 -->
**严重性** 错误

**消息**

```
A 'param' is a parameter of the pipeline (times a number, plus a number), 'DreamPassWeight' (the same), or a value the compiler can fold, and this is none of them.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2683`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2726`
<!-- generated:end DSH7352 -->

**原因。** 一个 `param` 值无法存储在管线资产中，该资产保存的是一个常量，或
管线的一个参数（或 `DreamPassWeight`）乘以一个数再加上一个数：该值是在 CPU 上按视图计算一次的，
而不是在着色器中。有两条消息：一个编译器无法折叠的常量表达式；
或一个以另一种形式作用于参数的表达式 —— `Radius * Gust`（两个参数）、
`1.0 / Radius`、`sin(Radius)`、`Radius > 2 ? 1 : 0`。

**修正。** 写入 `Source * a + b`，并带有编译器可以折叠的 `a` 和 `b` 数字，任何
其他内容都在着色器或材质中基于普通参数进行计算：

```hlsl
param Radius = OutlineWidth * 2 + 1;
```

<a id="dsh7353"></a>

## DSH7353

<!-- generated:begin DSH7353 -->
**严重性** 错误

**消息**

```
What scales or offsets a parameter in a 'param' is one number the compiler can fold; a vector, or a value known only at run time, has no place in the asset's 'Source * a + b'.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2794`、`Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2885`
<!-- generated:end DSH7353 -->

**原因。** 一个 `param` 用资产无法存储的东西来缩放或偏移参数。有两条消息：
该因子或偏移不是编译器可以折叠的单个数字 —— 例如一个向量，如
`OutlineColor * float4(1, 1, 1, 0.5)`，或一个它无法求值的表达式；或者该参数是一个
`bool` 或一个 `Texture2D`，它会按原样传递，无法被缩放。

**修正。** 用一个数字进行缩放 —— `param Color = OutlineColor * 0.5;` —— 并在
着色器或材质中进行逐分量处理。原样传递一个 `bool` 或纹理。

<a id="dsh7354"></a>

## DSH7354

<!-- generated:begin DSH7354 -->
**严重性** 信息

**消息**

```
Buffer '{0}' is exported and last written at {1}: opaque and translucent materials that sample it see the previous frame's contents, UI sees this frame's.
```

**由以下抛出** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4774`
<!-- generated:end DSH7354 -->

**原因。** 导出的缓冲区的最后一个写入者在 `AfterBasePass` 或更晚运行，因此其渲染目标在
某些采样它的材质已绘制之后才被更新。最后写入于 `AfterBasePass` 或
`AfterOpaque`：不透明材质看到上一帧的内容，而半透明和后期处理
材质以及 UI 看到本帧的内容。最后写入于 `BeforePostProcess` 或更晚：不透明和
半透明材质看到上一帧的，UI 看到本帧的。

**修复。** 如果可接受一帧的延迟，则无需修复。要让每个读取者都看到本帧的
内容，请在 `BeginView` 或 `BeforeBasePass` 写入缓冲区——一个 `compute` 通道，或一个 `mesh` 通道，
位于 `BeforeBasePass` 并使用 `Depth = None` 或 `Depth = Own(...)`。

<a id="dsh7355"></a>

## DSH7355

<!-- generated:begin DSH7355 -->
**严重性** 错误

**消息**

```
Buffer '{0}' is exported, and an exported buffer becomes a render target asset that materials sample, which a '{1}' buffer cannot be; export a float format.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1777`
<!-- generated:end DSH7355 -->

**原因。** 缓冲区以无法作为被采样渲染目标的格式导出：`Depth32`，或一个
整数格式。导出的缓冲区会成为一个渲染目标资产，材质、UMG 和 Niagara
将其作为浮点纹理采样。

**修复。** 导出浮点或规范化格式。对于深度，让某个通道将其写入一个 `R32F` 缓冲区并
导出该缓冲区；一个网格通道可通过 `UE.DreamPassOutput` 写入 `UE.PixelDepth()`。

<a id="dsh7356"></a>

## DSH7356

<!-- generated:begin DSH7356 -->
**严重性** 警告

**消息**

```
No pass writes buffer '{0}', so whoever reads it reads its 'Clear' value.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4749`
<!-- generated:end DSH7356 -->

**原因。** 没有通道写入该缓冲区——没有 `write`，也没有任何网格通道的 `Depth = Own(...)`——因此每次
读取它都获得其 `Clear` 值。通常是因为某个写入者被重命名或移除了。

**修复。** 在某个通道中写入它，或移除它及其读取者。一个处处相同的值
会作为 `param` 到达通道，而不是作为缓冲区。

<a id="dsh7357"></a>

## DSH7357

<!-- generated:begin DSH7357 -->
**严重性** 警告

**消息**

```
No pass reads buffer '{0}' and it is not exported, so writing it is wasted work; read it, export it, or remove it.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4759`
<!-- generated:end DSH7357 -->

**原因。** 某个通道写入该缓冲区而没有任何东西读取它——没有对它的或将它的 `.Previous` 的 `read`，也没有
网格通道的 `Depth = Own(...)`——并且它未被导出，因此写入它是在浪费 GPU 工作。

**修复。** 在后续通道中读取它，导出它（`Export = true`）以供材质采样，或移除它及其
写入。

<a id="dsh7359"></a>

## DSH7359

<!-- generated:begin DSH7359 -->
**严重性** 警告

**消息**

```
'Mode = Own' draws every primitive with its own material, so the 'Material' of mesh pass '{0}' is never used.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3120`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3215`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3232`
<!-- generated:end DSH7359 -->

**原因。** 某个通道的键或绑定没有到达任何地方。有三条消息：

- `Mode = Own` 中的一个 `mesh` 通道命名了一个 `Material`，并用图元自己的
  材质绘制每个图元，因此该覆盖从未被使用；
- `Mode = Own` 中的一个 `mesh` 通道有 `param` 行，并且没有覆盖材质可以给它们；
- 一个 `clear` 或 `copy` 通道有 `param` 行，并且完全没有着色器或材质。

每一条都是警告：该通道仍会构建。

**修复。** 移除未使用的内容，或将网格通道切换为 `Mode = Override`（所有内容都用
`Material` 绘制）或 `Mode = OwnOrOverride`（图元在拥有
`UE.DreamPassOutput` 时使用自己的材质，其他地方使用覆盖）。

<a id="dsh7360"></a>

## DSH7360

<!-- generated:begin DSH7360 -->
**严重性** 信息

**消息**

```
The materials, shader files and pass layers this pipeline names are not available here, so they were taken as written and the checks that need them were skipped; compile the pipeline in the editor to have them checked.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:664`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:672`
<!-- generated:end DSH7360 -->

**原因。** 管线的某些检查被跳过了。有两条消息：

- 管线由某个在没有编辑器的情况下运行的工具绑定，因此它命名的材质、`.usf` 文件和
  通道层被按原样采用：材质是否存在以及它为通道提供什么、
  着色器文件及其 `Entry` 是否存在，以及某个层是否在项目设置中，都不会被
  检查。
- 引擎早于 Unreal Engine 5.8 且没有 Custom Pass 运行时：材质为
  通道提供什么——它的 `UserSceneTexture` 输入、它的 `UE.DreamPassOutput` 引脚、它的使用标志、它的
  预曝光和翻译器设置——不会被读取或检查，且管线不会在此引擎上
  构建。

**修复。** 无需修复。在 5.8 或更高版本的编辑器中编译该管线，以让每个引用
都被检查。

<a id="dsh7361"></a>

## DSH7361

<!-- generated:begin DSH7361 -->
**严重性** 错误

**消息**

```
Pass '{0}' holds a second 'hlsl' block, and the code of a pass is one block; the one on line {1} is it.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3885`
<!-- generated:end DSH7361 -->

**原因。** 一个通道持有两个 `hlsl` 块。一个通道的代码是一个块：整个函数，或其
入口的语句。

**修复。** 将它们合并为一个块。两个通道共享的函数放在该文件的 `hlsl` 块中。

<a id="dsh7362"></a>

## DSH7362

<!-- generated:begin DSH7362 -->
**严重性** 错误

**消息**

```
This file already has an 'hlsl' block, on line {0}, and a '.dsp' has one: write every shared function and entry in it.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3862`
<!-- generated:end DSH7362 -->

**原因。** 该文件在文件作用域有第二个 `hlsl` 块。一个 `.dsp` 只有一个：其 HLSL
在该文件中的所有通道编译所用的代码，以及各通道用 `Entry = X;` 选取的入口。

**修复。** 将第二个块的文本移入第一个。

<a id="dsh7364"></a>

## DSH7364

<!-- generated:begin DSH7364 -->
**严重性** 错误

**消息**

```
Pass '{0}' runs the shader file '{1}' and holds an 'hlsl' block as well, and the code of a pass is in one place: remove 'Shader' to run the block, or the block to run the file.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3910`
<!-- generated:end DSH7364 -->

**原因。** 一个通道命名了一个着色器文件（`Shader = "..."`）并且还持有一个 `hlsl` 块。一个通道的代码
只在一处。

**修复。** 移除 `Shader`——以及 `Entry`，除非它命名该块中的一个函数——以运行该块，或移除该
块以运行该文件。

<a id="dsh7365"></a>

## DSH7365

<!-- generated:begin DSH7365 -->
**严重性** 错误

**消息**

```
The 'hlsl' block of pass '{0}' holds the statements of its entry, whose function the compiler writes, so 'Entry' has no function to name; remove it, or write whole functions in the block: '{1}'.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3948`
<!-- generated:end DSH7365 -->

**原因。** 该通道的 `hlsl` 块只包含语句——在其顶层没有函数定义、`groupshared`、`struct`、
`cbuffer` 或 `[numthreads(...)]`——因此它是某个入口的语句，其函数由编译器
编写，而 `Entry` 没有函数可命名。

**修复。** 移除 `Entry`，或在该块中编写整个函数（当未编写时为 `[numthreads(8, 8, 1)] void Main(uint3 Id :
SV_DispatchThreadID) { ... }` for a compute pass). With whole functions `Entry` names one of them, and is `Main`）。

<a id="dsh7366"></a>

## DSH7366

<!-- generated:begin DSH7366 -->
**严重性** 错误

**消息**

```
'#include' cannot stand among the statements of a function, and the 'hlsl' block of pass '{0}' holds the statements of its entry; include the file in the file's 'hlsl' block, or write whole functions in the pass's block.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3962`
<!-- generated:end DSH7366 -->

**原因。** 一个持有某个入口语句的块中有一个 `#include`。编译器会将这些
语句编写在一个函数内，而 HLSL 不允许在那里使用 include。

**修复。** 将该文件包含在该文件的 `hlsl` 块中，该块中的代码是所有 HLSL 位于 `.dsp` 中的通道编译
所用的，或者在该通道的块中编写整个函数并在其顶部包含该文件。

<a id="dsh7367"></a>

## DSH7367

<!-- generated:begin DSH7367 -->
**严重性** 错误

**消息**

```
'{0}' is a name of pass '{1}', and its 'hlsl' block holds the statements of a function the compiler writes, which names its own values {2}; rename the binding.
```

**引发者** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3372`
<!-- generated:end DSH7367 -->

**原因。** 一个其块持有入口语句的通道绑定了一个名称，而编译器编写的函数会为该名称赋予
自己的值：在计算通道中为 `Id`、`GroupId`、`LocalId` 和 `LocalIndex`；在全屏
通道中为 `SvPosition`、`Pixel` 和 `UV`。每个绑定都是该通道槽位的 `#define`，会替换该参数或局部变量。

**修复。** 重命名绑定（用 `read Scene = SceneColor;` 而不是 `read UV = SceneColor;`），或在代码块中编写完整函数，由函数自身命名其参数。

<a id="dsh7368"></a>

## DSH7368

<!-- generated:begin DSH7368 -->
**Severity** error

**Message**

```
'{0}' is in the shared code of the file's 'hlsl' block, and is the entry of pass '{1}' as well, which the pass's slot renames to the slot's entry point with a #define; the shared code is compiled into that slot, after it. Rename it in the block.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4220`
<!-- generated:end DSH7368 -->

**Cause.** 文件 `hlsl` 块共享代码中的某个名称——即其中所有未被任何 pass 用 `Entry` 命名的内容——同时也是某个 pass 的槽位名称，而该 pass 的 HLSL 位于 `.dsp` 定义中：可能是 `read`、`write` 或 `param` 名称，其 `Size` 或 `UVRect` 名称，或该 pass 的入口，而该 pass 的槽位会将其重命名为槽位的入口点。共享代码会在这些 `#define` 之后被编译进每个此类 pass 的槽位，因此该名称会在那里被替换——包括作为辅助函数的参数或局部变量。

**修复。** 在代码块中重命名它：辅助函数的参数 `Spread`，而不是 pass 的 `SigmaTexels`。使用 pass 名称的代码应放在入口中，入口只会被编译进命名它的那些 pass 的槽位。

<a id="dsh7369"></a>

## DSH7369

<!-- generated:begin DSH7369 -->
**Severity** error

**Message**

```
'{0}' is the entry of pass '{1}' in the file's 'hlsl' block, and is called here; an entry is compiled only into the slots of the passes that name it, so nothing else can call it. Move what it shares into a function of its own in the block, and call that.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4120`
<!-- generated:end DSH7369 -->

**Cause.** 文件 `hlsl` 块中的一个入口——即某个 pass 仅用 `Entry = X;` 命名的函数——被调用：来自共享代码、来自另一个入口，或来自某个 pass 自己的代码块。在未命名它的每个 pass 的槽位中，该入口会从共享代码中被抹除，因此该调用在那里无法编译。

**修复。** 将入口与其调用方共享的内容移入代码块中一个独立的函数，并从两者调用它。

<a id="dsh7370"></a>

## DSH7370

<!-- generated:begin DSH7370 -->
**Severity** error

**Message**

```
A {0} pass runs no HLSL of its own, so pass '{1}' cannot hold an 'hlsl' block.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2934`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2962`
<!-- generated:end DSH7370 -->

**Cause.** 某个 pass 中存在 `hlsl` 块，但该 pass 不运行自己的 HLSL：例如绘制 `Material` 的全屏 pass，或 `mesh`、`clear` 或 `copy` pass。

**修复。** 移除该代码块。全屏 pass 在没有 `Material` 时会运行 HLSL：改为移除它即可运行该代码块。
