---
title: "DSH1xxx --- 驱动、源文件和导入"
description: "DSH1xxx --- 驱动、源文件和导入"
---

<a id="dsh1xxx-----driver-source-files-and-imports"></a>

# DSH1xxx --- 驱动、源文件和导入

> 生成标记之间的块由 `.skill/gen-diagnostics.ps1` 写入。
> 标记下方的一切均为手动编写，会在重新生成后保留。

<a id="dsh1030"></a>

## DSH1030

<!-- generated:begin DSH1030 -->
**严重级别** 错误

**消息**

```
{0}({1}): this '#if' is never closed; the file ends with {2} conditional block(s) still open.
```

**引发者** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:1305`
<!-- generated:end DSH1030 -->

**原因。** 一个 `#if` / `#ifdef` / `#ifndef` 链已打开，但文件在其 `#endif` 之前结束；计数表示还有多少个链仍处于打开状态。`Function` 或 `GraphFunction` 主体内的指令不计入 -- 它们属于 HLSL 编译器 -- 因此无法从主体内部关闭链

**修复。** 在条件区域应结束的位置添加缺失的 `#endif`；如果 `#if` 是给着色器编译器用的，请将整个链移到函数体内

**参见** [预处理器](../language/preprocessor.md)

<a id="dsh1031"></a>

## DSH1031

<!-- generated:begin DSH1031 -->
**严重级别** 错误

**消息**

```
{0}({1}): '#endif' without a matching '#if'.
```

**引发者** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:1205`
<!-- generated:end DSH1031 -->

**原因。** 在没有打开任何 DreamShader 链时发现了 `#endif`。通常的原因是某个 `#if` 位于 `Function` 主体内（对此预处理器不透明），而其 `#endif` 位于右花括号之后

**修复。** 删除多余的 `#endif`，或将 `#if` 移出主体，使两端位于花括号的同一侧

**参见** [预处理器](../language/preprocessor.md)

<a id="dsh1032"></a>

## DSH1032

<!-- generated:begin DSH1032 -->
**严重级别** 错误

**消息**

```
{0}({1}): '#{2}' without a matching '#if'.
```

**引发者** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:1129`
<!-- generated:end DSH1032 -->

**原因。** 在没有打开任何 `#if` 链时出现了 `#elif` 或 `#else`

**修复。** 先用 `#if` / `#ifdef` / `#ifndef` 打开链，或移除该分支

**参见** [预处理器](../language/preprocessor.md)

<a id="dsh1033"></a>

## DSH1033

<!-- generated:begin DSH1033 -->
**严重级别** 错误

**消息**

```
{0}({1}): '#{2}' after the '#else' on line {3}, which already closed this chain.
```

**引发者** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:1144`
<!-- generated:end DSH1033 -->

**原因。** 链已在所示行上取用了其 `#else`，而 `#else` 会关闭链：其后只能跟随 `#endif`

**修复。** 将额外的分支移到 `#else` 之上，或用 `#endif` 关闭链并开始一个新的链

**参见** [预处理器](../language/preprocessor.md)

<a id="dsh1034"></a>

## DSH1034

<!-- generated:begin DSH1034 -->
**严重级别** 错误

**消息**

```
{0}({1}): invalid '{2}' condition: {3}
```

**引发者** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessorExpression.cpp:57`
<!-- generated:end DSH1034 -->

**原因。** 条件文本在表达式完成之前就结束了 -- 未闭合的括号、悬空运算符（`#if 1 &&`），或缺少操作数的运算符；冒号后的详细信息是表达式解析器自身的说明

**修复。** 补全表达式。条件支持 `defined(NAME)`、`!`、`&&`、`||`、比较、`+ - * / %`、括号和字符串相等；未定义的名称为 `0`

**参见** [预处理器](../language/preprocessor.md)

<a id="dsh1035"></a>

## DSH1035

<!-- generated:begin DSH1035 -->
**严重级别** 错误

**消息**

```
{0}({1}): unknown preprocessor directive '#{2}'. {3}
```

**引发者** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:1012`
<!-- generated:end DSH1035 -->

**原因。** 函数体之外的 `#` 行未拼写出八个指令之一（`#if #ifdef #ifndef #elif #else #endif #define #undef`）或直通对 `#Region` / `#EndRegion`。指令区分大小写，因此 `#IF` 也会落到此处，而 `#include` 不在其中（导入应拼写为 `import`）。这是错误而非警告，因为被静默跳过的指令 -- 例如 `#endfi` 这样的拼写错误 -- 会使其下方每一行都无条件编译

**修复。** 修正拼写或大小写；包含请写 `import "...";`；如果该行是 HLSL 指令，它必须位于 `Function` 主体内，此预处理器不会查看那里

**参见** [预处理器](../language/preprocessor.md)

<a id="dsh1036"></a>

## DSH1036

<!-- generated:begin DSH1036 -->
**严重级别** 错误

**消息**

```
{0}({1}): '#{2}' requires a define name.
```

**引发者** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:1074`、`Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessorExpression.cpp:67`
<!-- generated:end DSH1036 -->

**原因。** `#ifdef`、`#ifndef`、`#define` 或 `#undef` 后没有任何内容

**修复。** 为该指令提供一个定义名称

**参见** [预处理器](../language/preprocessor.md)

<a id="dsh1037"></a>

## DSH1037

<!-- generated:begin DSH1037 -->
**严重级别** 错误

**消息**

```
{0}({1}): '#{2}' nesting is deeper than the limit of {3}.
```

**引发者** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:1027`
<!-- generated:end DSH1037 -->

**原因。** 条件链的嵌套深度超过了所示的固定限制

**修复。** 展平嵌套：用 `&&` 将内部测试合并为一个条件，或拆分文件

**参见** [预处理器](../language/preprocessor.md)

<a id="dsh1038"></a>

## DSH1038

<!-- generated:begin DSH1038 -->
**严重级别** 错误

**消息**

```
{0}({1}): '#{2}' needs a name made of letters, digits and underscores and not starting with a digit; got '{3}'.
```

**引发者** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:1083`、`Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:1241`
<!-- generated:end DSH1038 -->

**原因。** 指令后的名称不是标识符：它必须由字母、数字和下划线组成，且不能以数字开头

**修复。** 将定义重命名以符合 `[A-Za-z_][A-Za-z0-9_]*`

**参见** [预处理器](../language/preprocessor.md)

<a id="dsh1039"></a>

## DSH1039

<!-- generated:begin DSH1039 -->
**严重级别** 错误

**消息**

```
{0}({1}): '{3}' is a read-only built-in constant, so '#{2}' cannot change it. The 'DS_' prefix is reserved by DreamShader.
```

**引发者** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:1255`
<!-- generated:end DSH1039 -->

**原因。** `DS_` 前缀保留用于关于编译过程的只读内置事实（`DS_ENGINE_MAJOR`、`DS_ENGINE_MINOR`、`DS_ENGINE_PATCH`、`DS_SUBSTRATE`、`DS_PLATFORM`、`DS_PLUGIN_VERSION`、...）。它们无法从源文件中定义或取消定义，并且保留是按前缀进行的，这样以后添加的内置项永远不会败给项目先注册的名称

**修复。** 使用不带 `DS_` 前缀的名称。要尝试假设的引擎或平台，请定义你自己的开关（在项目设置中或使用 `dsc -Define`）并据此分支

**参见** [预处理器](../language/preprocessor.md)

<a id="dsh1040"></a>

## DSH1040

<!-- generated:begin DSH1040 -->
**严重级别** 错误

**消息**

```
{0}({1}): type mismatch in '{2}' condition: {3}
```

**引发者** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessorExpression.cpp:76`
<!-- generated:end DSH1040 -->

**原因。** 运算符两侧的类型不同：解析为整数的值是数字，其他任何内容都是字符串，而两者只在 `==` / `!=` 中相遇。对字符串进行算术或排序，或将字符串与数字比较，都会引发此错误

**修复。** 同类比较：为定义提供数值，或仅使用 `==` / `!=` 比较两个字符串

**参见** [预处理器](../language/preprocessor.md)

<a id="dsh1041"></a>

## DSH1041

<!-- generated:begin DSH1041 -->
**严重级别** 错误

**消息**

```
{0}({1}): the right operand of '{2}' in this '{3}' condition is zero.
```

**引发者** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessorExpression.cpp:86`
<!-- generated:end DSH1041 -->

**原因。** `/` 或 `%` 的除数求值为零。请记住，未定义的名称为 `0`，因此 `#if TOTAL / COUNT` 搭配从未定义的 `COUNT` 是常见的情形

**修复。** 定义该名称，或用 `defined(COUNT) && COUNT != 0 && ...` 保护表达式 -- `&&` 会短路，因此除法永远不会被求值

**参见** [预处理器](../language/preprocessor.md)

<a id="dsh1042"></a>

## DSH1042

<!-- generated:begin DSH1042 -->
**严重级别** 错误

**消息**

```
{0}({1}): '{2}' is already complete before '{3}'. Nothing may follow a directive but a '//' comment.
```

**触发者** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessorExpression.cpp:1097`
<!-- generated:end DSH1042 -->

**原因。**该指令已经完整，其后又出现了更多词元（`#if 1 2`、`#ifdef A B`、`#endif LABEL`）。指令之后只能跟随一个 `//` 注释；未完成的表达式属于 [DSH1034](#dsh1034)。`#define` 是例外，因为它的值一直延伸到行尾

**修复。**删除多余的词元，或将它们转换为 `//` 注释

**参见**[预处理器](../language/preprocessor.md)

<a id="dsh1043"></a>

## DSH1043

<!-- generated:begin DSH1043 -->
**严重性** 错误

**消息**

```
Asset reference is written as '{0}', which is not a texture class; a texture default requires {1}.
```

**触发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderLegacyTextureDefaults.cpp:86`
<!-- generated:end DSH1043 -->

**原因。**某项纹理默认值被写成了一个 `Class'/Game/Folder/Asset.Asset'` 引用，而该引用的类属于 DreamShader 已知不是纹理的那些 -- `MaterialFunction`、`CurveLinearColor`、`Font`、`MaterialParameterCollection` 等。从错误的 Content Browser 行复制引用恰好会导致这种情况：路径格式正确、资产存在，但它根本不是纹理槽位能容纳的内容。只有 DreamShader 那份简短的“确定不是纹理”列表中的类才会触发此错误；插件从未听说过的类会被剥离并遗忘，而不会被拒绝

**修复。**从纹理资产本身重新复制引用，或者删除 `Class'` 前缀以及引号，只保留裸对象路径 -- 引号前的类只是装饰，Unreal 在解析路径时会将其忽略，因此移除它总是安全的

**参见**[路径](../parameters/path.md)

<a id="dsh1044"></a>

## DSH1044

<!-- generated:begin DSH1044 -->
**严重性** 错误

**消息**

```
Asset reference is written as '{0}', but this property is declared as {1}.
```

**触发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderLegacyTextureDefaults.cpp:75`
<!-- generated:end DSH1044 -->

**原因。**某项纹理默认值被写成了一个 `Class'/Game/Folder/Asset.Asset'` 引用，而该引用的类是维度与属性声明不同的纹理 -- 例如将 `Texture2D'...'` 赋给了 `VolumeTexture` 或 `TextureSampleParameterVolume`。仅当声明的类型本身指明了其维度时才会运行此检查；`TextureObjectParameter` 及其同类自身不携带维度，因此不会按这种方式进行判断

**修复。**如果属性类型正确，则从该维度的资产复制引用；如果资产正确，则更改声明的类型以与其匹配。DreamShader 所认为的维度来自声明，而绝非来自粘贴的前缀

**参见**[路径](../parameters/path.md)

<a id="dsh1045"></a>

## DSH1045

<!-- generated:begin DSH1045 -->
**严重性** 错误

**消息**

```
Asset reference is written as '%s', but this slot requires a '%s'.
```

**触发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:292`、`Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:301`
<!-- generated:end DSH1045 -->

**原因。**元数据条目、`UE.CollectionParam` 参数或 `VirtualFunction` 的 `Options.Asset` 中的资产引用被写成了一个 `Class'/Game/Folder/Asset.Asset'` 引用，而该引用的类与该槽位声明的类无关 -- 例如将 `Texture2D'...'` 写入 `Curve` 条目。这两个类会进行双向比较，因此在需要子类的位置写入基类（将 `Texture'...'` 写入 `UTexture2D` 属性）会被接受，并留待加载时判定

**修复。**从该槽位所需类的资产复制引用，或去掉 `Class'` 前缀和引号，只保留裸对象路径

**参见**[路径](../parameters/path.md)
