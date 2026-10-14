---
title: "VirtualFunction 工具"
description: "VirtualFunction 工具"
---

<a id="virtualfunction-tools"></a>

# VirtualFunction 工具

> [DreamShader](../index.md) » [工具](index.md) » **VirtualFunction 工具**

为现有 `UMaterialFunction` 资源生成、打开和刷新
[`VirtualFunction`](../language/virtual-function.md) 声明的编辑器操作，以及使这些声明与其指定资源保持同步的启动服务。

| | |
| :-- | :-- |
| 附加到 | `UMaterialFunction`, `UMaterialFunctionMaterialLayer`, `UMaterialFunctionMaterialLayerBlend` |
| 菜单位置 | 内容浏览器上下文菜单 ▸ *DreamShader* ▸ **VirtualFunction**，以及材质编辑器 **DreamShader** 工具栏组合框 |
| 写入 | `<SourceDirectory>/VirtualFunctions/<Name>.dsh`，UTF-8 无 BOM |
| 起始版本 | `1.2.0`（声明）· `1.2.1`（菜单和 `.dsh` 写入器）· `1.2.2`（复用、*OpenVirtualFunction*、启动同步） |

<a id="context-menu"></a>

## 上下文菜单

每次右键单击时都会重新构建 **VirtualFunction** 部分。在构建它之前，编辑器
会在每个项目源文件中搜索 `VirtualFunction` 声明，其解析后的资源与
所选 `UMaterialFunction` 匹配。**你获得的条目取决于搜索结果。**

<a id="when-a-declaration-already-exists"></a>

### 当声明已存在时

| 条目名称 | 标签 | 工具提示 | 图标 | 效果 |
| :-- | :-- | :-- | :-- | :-- |
| `DreamShader.OpenVirtualFunction` | **OpenVirtualFunction** | “在 VSCode 中打开现有的 DreamShader VirtualFunction 定义。” | `Icons.OpenInExternalEditor` | 打开包含该声明的文件，并定位到声明的行和列 |
| `DreamShader.CopyVirtualFunctionReference` | **Copy Virtual Function Reference** | “复制一个引用此现有 VirtualFunction 的 DreamShader Graph 调用。” | `GenericCommands.Copy` | 复制基于**声明的**名称而非资源名称构建的调用 |

不提供其他条目——特别是不提供 *Create*，因为已经存在一个声明。

<a id="when-no-declaration-exists"></a>

### 当声明不存在时

| 条目名称 | 标签 | 工具提示 | 图标 | 效果 |
| :-- | :-- | :-- | :-- | :-- |
| `DreamShader.CopyVirtualFunction` | **CopyVirtualFunction** | “复制此材质函数的完整 DreamShader VirtualFunction 声明。” | `GenericCommands.Copy` | 将整个声明复制到剪贴板 |
| `DreamShader.CreateVirtualFunction` | **CreateVirtualFunction** | “创建一个包含 VirtualFunction 声明的 .dsh 文件。” | `Icons.Save` | 将声明写入新的 `.dsh` 并打开它 |
| `DreamShader.CopyVirtualFunctionPrototype` | **Copy extern Prototype (2.0)** *（自 2.0.0 起）* | “复制此材质函数的 2.0 extern 原型，包含其 /// @asset 行，用于 .dss 或 .dsh。” | `GenericCommands.Copy` | 复制声明的 `.dss` 拼写——参见 [编辑器集成](editor-integration.md) |
| `DreamShader.CreateVirtualFunctionPrototype` | **Create extern Prototype (2.0)** *（自 2.0.0 起）* | “创建一个包含此材质函数 2.0 extern 原型的 .dsh 文件，供 .dss 源 #include。” | `Icons.Save` | 将该原型写入新的 `.dsh` 并打开它 |
| `DreamShader.CopyVirtualFunctionCall` | **CopyVirtualFunctionCall** | “复制此 VirtualFunction 的 DreamShader Graph 调用示例。” | `GenericCommands.Copy` | 将调用示例复制到剪贴板 |

匹配依据是声明解析后的资源对象路径，比较时不区分大小写；第一个
匹配项胜出。有两种拼写形式算作声明：任何源文件中的 1.x `VirtualFunction` 块，以及
*（自 2.0.0 起）* `.dsh` 或 `.dss` 中带有 `/// @asset` 行的 `extern` 原型。由于
查找会读取文件，刚刚手写的声明会立即被找到——启动时不会
缓存任何内容。

> [!WARNING]
> 该查找**会重新枚举并重新读取每个源文件**（`.dsm`、`.dsh`、`.dsf`、`.dss`、`.dsi`、
> `.dsp`），位于每个源根目录下、每个根目录的 `Packages` 文件夹之外，从磁盘读取，在每次
> 右键单击材质函数资源时都会执行；提及 `extern` 和
> `@asset` 的 `.dsh` 或 `.dss` 也会被解析。开销随源文件数量增长，并在
> 上下文菜单出现之前产生。没有缓存，也无法禁用它，除非
> `-NoDreamShaderEditorBridge`，而那样会完全移除菜单。

<a id="actions"></a>

## 操作

本页中运行时替换项显示为 `{Placeholder}`。

| 操作 | 成功提示 | 失败提示 |
| :-- | :-- | :-- |
| **CopyVirtualFunction** | `Copied VirtualFunction definition for {Asset}.` —— 完整文本也会以 Display 级别记录到日志 | `DreamShader could not find the selected Material Function.` · `DreamShader failed to build VirtualFunction: {Error}` |
| **CreateVirtualFunction** | `Created VirtualFunction file: {File}` | 资源已不存在 · 构建失败 · `DreamShader failed to create directory: {Directory}` · `DreamShader failed to write VirtualFunction file: {File}` · `Created VirtualFunction file but could not open it: {File}` |
| **OpenVirtualFunction** | `Opened VirtualFunction definition: {File}` | 资源已不存在 · `DreamShader could not find a VirtualFunction definition for {Asset}.` · `DreamShader could not open VirtualFunction file: {File}` |
| **Copy Virtual Function Reference** | `Copied VirtualFunction reference for {Name}.` —— 使用**声明的**名称，而非资源名称 | 资源已不存在 · 未找到 · `DreamShader failed to build VirtualFunction reference: {Error}` |
| **CopyVirtualFunctionCall** | `Copied VirtualFunction call for {Asset}.` | 资源已不存在 · `DreamShader failed to build VirtualFunction call: {Error}` |

> [!NOTE]
> **对已有声明的资源执行 CreateVirtualFunction 会静默重定向到
> OpenVirtualFunction。** 它绝不会写入重复文件。你收到的提示是
> `Opened VirtualFunction definition: {File}`，而不是“已创建”消息。当任一拼写形式存在时，
> *Create extern Prototype* 也会如此。

<a id="where-the-file-is-written"></a>

## 文件写入位置

| 方面 | 值 |
| :-- | :-- |
| 目录 | `<SourceDirectory>/VirtualFunctions/` —— 默认为 `DShader/VirtualFunctions/` |
| 首选文件名 | `<sanitized MaterialFunction name>.dsh` |
| 发生冲突时 | `<sanitized name>_<crc32 of the asset's object path>.dsh`，CRC 打印为八位小写十六进制数字 |
| 编码 | UTF-8 无 BOM |
| 写入后 | 通过首选编辑器链打开——VSCode，然后是操作系统默认编辑器，最后是记事本 |

在此写入的 `.dsh` 会像任何其他头文件一样被源目录监视器拾取，因此
导入它的材质会自动重新编译。

<a id="the-generated-declaration"></a>

## 生成的声明

```c
VirtualFunction(Name="<SanitizedName>")
{
    Options = {
        Asset = <asset-literal>;
        Description = "Generated from <full object path>";
    }

    Inputs = {
        [opt ]<type> <Name>[ = <default>][ [ Description="…"; SortPriority=<n>; ] ];
        …
    }

    Outputs = {
        <type> <Name>[ [ Description="…"; SortPriority=<n>; ] ];
        …
    }
}
```

正文使用制表符缩进。仅当资源至少公开一个输出时才会生成声明。

<a id="asset-literal"></a>

### 资源字面量

| 对象路径开头为 | 输出为 |
| :-- | :-- |
| `/Game/` | `Path(Game, "<rest>")` |
| `/Engine/` | `Path(Engine, "<rest>")` |
| 内容插件的挂载点（在已启用的内容插件中取最长匹配） | `Path(Plugins.<PluginName>, "<rest>")` |
| 其他任何内容 | 原始对象路径，不包装 |

有关可接受的根拼写，请参见 [路径](../parameters/path.md)。

<a id="input-types"></a>

### 输入类型

从 Unreal 的函数输入类型到 DreamShaderLang 标记的映射是穷尽的：

| `EFunctionInputType` | 标记 |
| :-- | :-- |
| `FunctionInput_Scalar` | `float` |
| `FunctionInput_Vector2` | `float2` |
| `FunctionInput_Vector3` | `float3` |
| `FunctionInput_Vector4` | `float4` |
| `FunctionInput_Texture2D` | `Texture2D` |
| `FunctionInput_TextureCube` | `TextureCube` |
| `FunctionInput_Texture2DArray` | `Texture2DArray` |
| `FunctionInput_VolumeTexture` | `VolumeTexture` |
| `FunctionInput_MaterialAttributes` | `MaterialAttributes` |
| `FunctionInput_Substrate` | `Substrate` |
| `FunctionInput_StaticBool` | `StaticBool` —— 保持区分，以便调用方可以传入静态 bool |
| `FunctionInput_Bool` | `bool` |
| *（任何其他值）* | `float4` |

<a id="optional-inputs-and-defaults"></a>

### 可选输入与默认值

当资源将某个输入标记为使用其预览值作为默认值时，该输入会加上 `opt` 前缀。
默认字面量根据该预览值渲染：

| 输入类型 | 默认字面量 |
| :-- | :-- |
| Scalar | 浮点值 |
| StaticBool、Bool | `true` / `false` |
| Vector2 | `float2(x, y)` |
| Vector3 | `float3(x, y, z)` |
| Vector4 | `float4(x, y, z, w)` |
| 其他任何类型 | *（空——` = ` 后缀被完全省略）* |

> [!WARNING]
> 被标记为可选的纹理或材质属性输入会获得 `opt` 前缀，但**没有默认
> 值**，因为不存在可用于它的字面量语法。请在调用处提供该参数，或编辑
> 声明。

<a id="metadata-suffix"></a>

### 元数据后缀

| 键 | 触发条件 |
| :-- | :-- |
| `Description` | 该参数在资源中的描述非空 |
| `SortPriority` | 该资源的排序优先级与参数自身的索引不同 |

<a id="identifier-sanitization"></a>

### 标识符清理

| 规则 | 详情 |
| :-- | :-- |
| 首字符 | 必须是字母或 `_`；其他任何字符都会替换为 `_` |
| 后续字符 | 必须是字母数字或 `_`；其他任何字符都会替换为 `_` |
| 连续 `_` | 连续下划线会合并为一个 |
| 全下划线结果 | 变为 `DreamShaderSymbol` |
| 空输入 | 变为 `DreamShaderSymbol` |
| 名称退化为 `DreamShaderSymbol` 时 | 回退为 `Input<N>`、`Output<N>` 或 `VirtualFunction<N>`，其中 `<N>` 为从 1 开始的索引 |
| 一个声明中的重名 | 使用 `_2`、`_3`……进行唯一化 |

非 ASCII 字母会被保留——该清理器支持 Unicode，而非仅 ASCII。

在输出的字符串字面量内部，`\`、`"`、回车符、换行符和制表符都会被转义。

<a id="the-call-example"></a>

## 调用示例

**CopyVirtualFunctionCall** 和 **Copy Virtual Function Reference** 都会生成：

```c
<FunctionName>(<arg>, <arg>, …, OutputIndex=0)
```

| 规则 | 详情 |
| :-- | :-- |
| 每个输入对应一个参数 | 该输入经过清理和唯一化后的名称 |
| 可选输入 | 字面量标记 `default` |
| 末尾参数 | `OutputIndex=0`，始终追加，即使对于单输出函数也是如此 |

| 错误 | 原因 |
| :-- | :-- |
| `VirtualFunction name cannot be empty.` | 声明的名称为空 |
| `VirtualFunction '{Name}' does not expose any outputs.` | 该函数不暴露任何输出 |
| `No MaterialFunction asset was provided.` | 传入构建器的资源为空 |
| `MaterialFunction '{Name}' does not have a valid package path.` | 该资源没有可用于构建字面量的包 |
| `MaterialFunction '{Name}' does not expose any outputs.` | 声明构建器未找到任何输出 |

复制的文本是 1.x 文本的模板。请用真实的 `Graph` 值替换占位参
数名，并在需要不同输出时更改 `OutputIndex`。参见[调用](../graph/calls.md)。
将 `default` 作为参数是遗留规则（L25）：仅适用于 1.x 文本，且
[`dsc migrate`](migrate.md#what-the-rewrite-does) 会将其改写为具名参数。

<a id="startup-sync-service"></a>

## 启动同步服务

每个编辑器会话中，在桥接启动时，DreamShader 会重新读取项目中的每个 `VirtualFunction` 声
明，从实时资源重建它，并在文本发生变化时将文件写回。

> [!WARNING]
> **没有针对此操作的监视器。** 编辑 `UMaterialFunction` 的输入或输出不会
> 刷新声明；保存 `.dsh` 也不会。声明只会在下一次编辑器启动时
> 重新同步。

| 方面 | 行为 |
| :-- | :-- |
| 扫描的文件 | **可写**源根目录下的每个源文件——即项目自身的；插件根目录会被跳过——排除每个根目录的 `Packages` 文件夹 |
| 被拒绝的文件 | 任何包含[预处理器指令](../language/preprocessor.md)的源文件——`DSH9001` *（自 1.9.0 起）* |
| 关键字匹配 | 裸词 `VirtualFunction`，**区分大小写**匹配，并进行标识符边界检查 |
| 跳过的区域 | 带 `\` 转义的引号字符串、`//` 行注释和 `/* */` 块注释 |
| 块提取 | 一个配对平衡的 `( … )` 后跟一个配对平衡的 `{ … }`；一个可选的尾部 `;` 会被吸收进该块的范围 |
| 验证 | 每个提取出的块都会由旧版前端单独解析 *（自 2.0.0 起；之前是 1.x 解析器）*，并且必须恰好产生一个 `VirtualFunction`，其 `Options.Asset` 必须能够解析 |
| 不同步 | 带 `/// @asset` 的 2.0 `extern` 原型：查找会找到它，同步永远不会改写它 *（自 2.0.0 起）* |
| 比较 | 基于规范化文本——CRLF 和 CR 变为 LF，然后整个文本被修剪——因此行尾和周围空白的差异永远不会触发改写 |
| 改写顺序 | 替换按起始偏移量从大到小、从后往前应用，因此较早的偏移量保持有效 |
| 写回 | 整个文件，UTF-8 无 BOM |

由于扫描器是手写词法分析器而非前端，它能在文件合法存放声明的任何位置找到声明
——包括在 `.dsm` 内部、与 `Shader` 块并存的情况。

<a id="conditional-sources-are-refused-since-190"></a>

### 条件源会被拒绝 *（自 1.9.0 起）*

包含任何[预处理器指令](../language/preprocessor.md)的源文件不会被同步。它会被
报告一次，为 `DSH9001`，并完全保持原样。

原因在于写回，而非读取。同步会将 `StartIndex` / `EndIndex` 记录为**字节**
偏移量，并将重建的声明拼接到该范围上。预处理器保证行数
守恒——这只能换来**行**对齐，别无其他，因为被切掉的行会被替换为
*更短的*空行。因此，第一个切割点之后的每个字节偏移量都指向错误的位置，而
契约中没有可用于修正的偏移映射。如果经过预处理的字符串到达了
`SaveStringToFile`，文件中的每个死分支都会被永久压平。

这与 [Adopt](../generation/divergence.md#adopt-into-source) 是同一类意外，而且在某一点上更为严重：Adopt 是
由人选择的菜单项，而此服务会在桥接启动时**无人值守地运行**，
遍历项目中的每个可写源文件。

> [!NOTE]
> 只有在 `VirtualFunction` 子字符串测试之后才会到达该门控，因此一个不包含声明
> 的条件材质——同步本来也永远不会改写它——不会在每次编辑器启动时都报告错误。
>
> 同一扫描器的**只读**使用者不受影响，会继续扫描原始文本：[上下文菜
> 单](#context-menu)、*Open Virtual Function* 以及复制的[调用示例](#the-call-example)
> 都会看到所有分支的并集。这与依赖图所遵循的规则一致，而且出于同样的
> 原因它是安全的——读取所有分支会过度报告，而过度报告是无害的。

<a id="sync-diagnostics"></a>

### 同步诊断

下面的每条消息都会记录阶段 `virtualFunctionSync`、代码 `virtual-function-sync`、
源 `DreamShader VirtualFunction` 和严重性 `error`——这是同步产生的唯一严重性。
它们会到达 `diagnostics.json`、分片的 `diagnostics/` 目录和 `bridge.db`。

| 消息 | 原因 |
| :-- | :-- |
| `DreamShader could not read VirtualFunction source file '{File}'.` | 文件无法读取；在第 1 行第 1 列报告 |
| `VirtualFunction attributes are missing a closing ')'.` | 属性列表括号不平衡 |
| `VirtualFunction body is missing a closing '}'.` | 主体大括号不平衡 |
| `VirtualFunction declaration is invalid: {ParserError}` | 提取的代码块无法解析 —— `{ParserError}` 是旧前端第一个错误的文本，不含其错误代码 —— 或者未包含恰好一个 `VirtualFunction` |
| `VirtualFunction '{Name}' asset reference is invalid: {Error}` | `Options.Asset` 无法解析；原始字面量被记录为资源路径 |
| `VirtualFunction '{Name}' references missing MaterialFunction '{Path}'.` | 资源路径已解析，但对象加载失败 |
| `VirtualFunction '{Name}' could not be refreshed from MaterialFunction '{Path}': {Error}` | 声明构建器对实时资源失败 |
| `DreamShader failed to update VirtualFunction source file '{File}'.` | 重写后的文件无法写入；在第 1 行第 1 列报告 |

`Expected exactly one VirtualFunction block.` 是当代码块包含零个或多个声明时第四行携带的解析器错误文本。

[条件源拒绝](#conditional-sources-are-refused-since-190) 与其它所有行一样遵循上述段落：其记录仍携带阶段 `virtualFunctionSync`、代码 `virtual-function-sync` 和严重级别 `error`。其 `DSH9001` 在 **消息文本** 中传递，与 `DSH8149` 在 Adopt 闸门上的行为完全一致 —— 线上契约不变。

尽管如此，它是通过编码式诊断所使用的同一个 `FailWith` 辅助函数抛出的，这是有意的权衡，而非不一致：[`gen-diagnostics.ps1`](../diagnostics/README.md) 通过扫描完全相同的调用形态来发现代码，因此以任何其它方式编写的拒绝将存在于源代码中，而不存在于本手册中。`DSH9001` 是 `DSH9xxx` 工具范围的第一个条目，此前该范围为空白。

<a id="reporting"></a>

### 报告

| 日志 | 时机 |
| :-- | :-- |
| `DreamShader refreshed {Count} VirtualFunction definition(s) in '{File}'.` | 文件已被重写 |
| `DreamShader scanned {Scanned} VirtualFunction definition(s), refreshed {Refreshed}, reported {Issues} issue(s).` | 在结束时，仅当发生了某些事情 |

至少包含一个声明且未产生诊断的文件，其诊断会被清除；产生了诊断的文件，其诊断会被设置。

<a id="example"></a>

## 示例

右键点击 `/Game/Functions/MF_Weathering`（一个含有一个标量和一个纹理输入、以及一个输出的 `UMaterialFunction`）▸ *DreamShader* ▸ **CreateVirtualFunction**。编辑器会写入 `<Project>/DShader/VirtualFunctions/MF_Weathering.dsh` 并打开它：

```c
VirtualFunction(Name="MF_Weathering")
{
	Options = {
		Asset = Path(Game, "Functions/MF_Weathering");
		Description = "Generated from /Game/Functions/MF_Weathering.MF_Weathering";
	}

	Inputs = {
		opt float Amount = 0.5 [
			Description="Weathering strength";
		];
		Texture2D Mask;
	}

	Outputs = {
		float3 Result;
	}
}
```

**CopyVirtualFunctionCall** 在同一资源上复制：

```c
MF_Weathering(default, Mask, OutputIndex=0)
```

从材质中使用它：

```c
import "VirtualFunctions/MF_Weathering.dsh";

Shader(Name="Materials/M_Rock")
{
    Properties = {
        Texture2D MaskTex = Path(Game, "Textures/T_Mask");
    }

    Outputs = {
        float3 Color;
        Base.BaseColor = Color;
    }

    Graph = {
        Color = MF_Weathering(0.8, MaskTex, OutputIndex=0);
    }
}
```

<a id="see-also"></a>

## 另请参阅

- [VirtualFunction](../language/virtual-function.md) —— 声明的语法及其 `Options` 键
- [选项](../language/options.md) —— `Options` 区段接受的所有键
- [编辑器集成](editor-integration.md) —— 菜单附加到何处以及那里还有什么
- [调用](../graph/calls.md) —— 从 `Graph` 调用 `VirtualFunction`
- [输入 / 输出 / 结果](../language/inputs-outputs.md) —— 声明中的 `opt`、默认值和元数据
- [路径](../parameters/path.md) —— `Asset` 键使用的 `Path(Root, "…")` 字面量
- [导入](../language/import.md) —— 材质如何获取生成的 `.dsh`
- [预处理器](../language/preprocessor.md) —— 为什么条件源会以 `DSH9001` 被拒绝
- [分歧](../generation/divergence.md) —— Adopt，另一个源写入器，因相同理由被拒绝
- [反编译器](decompiler.md) —— 同一个声明构建器，用于每个被调用的函数
- [桥接](bridge.md) —— 同步诊断发布的位置
- [诊断索引](../diagnostics/index.md) —— 按阶段列出的每条消息
