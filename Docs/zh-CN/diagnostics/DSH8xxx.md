---
title: "DSH8xxx --- 资源生成与保存"
description: "DSH8xxx --- 资源生成与保存"
---

<a id="dsh8xxx-----asset-generation-and-saving"></a>

# DSH8xxx --- 资源生成与保存

> 生成标记之间的内容由 `.skill/gen-diagnostics.ps1` 写入。
> 标记之下的所有内容均为手动写入，在重新生成后仍会保留。

<a id="dsh8088"></a>

## DSH8088

<!-- generated:begin DSH8088 -->
**严重性** 错误

**消息**

```
%s contains an invalid folder segment.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:42`
<!-- generated:end DSH8088 -->

**原因。** 源所声明的资源名称包含一个不能作为包路径一部分的文件夹段：为空（`A//B`）、`.` 或 `..`，或者包含引擎在路径中拒绝接受的字符。

**修复。** 将名称写为普通文件夹和一个资源名：`Characters/Hero/M_Hero`。

<a id="dsh8089"></a>

## DSH8089

<!-- generated:begin DSH8089 -->
**严重性** 错误

**消息**

```
DreamShader Root '%s' references project plugin '%s', but no enabled plugin with that name was found.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:59`
<!-- generated:end DSH8089 -->

**原因。** 源通过根指定其资源的位置：`Game`、`Engine` 或 `Plugin.<Name>`（在 1.x 中为块上的 `Root = "..."`；在 2.0 中为文件所在的源根，或带有对象路径的 `/// @name`）。此根所命名的插件不是项目已启用插件中的任何一个。

**修复。** 检查插件名称（`.uplugin` 文件名，而非其友好名称），并确认它已为此项目启用。

<a id="dsh8090"></a>

## DSH8090

<!-- generated:begin DSH8090 -->
**严重性** 错误

**消息**

```
DreamShader Root '%s' must reference a project plugin under '%s'.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:65`
<!-- generated:end DSH8090 -->

**原因。** 源通过根指定其资源的位置：`Game`、`Engine` 或 `Plugin.<Name>`（在 1.x 中为块上的 `Root = "..."`；在 2.0 中为文件所在的源根，或带有对象路径的 `/// @name`）。此根所命名的插件不属于项目自身（引擎插件或商城插件）。DreamShader 只会将资源写入项目 `Plugins` 文件夹下的插件中。

**修复。** 将目标设为项目插件，或 `Game`。

对于此类源，烘焙不会中止：它所描述的资源是该插件自身的，任何宿主项目都无法生成到该挂载点，甚至无法编辑该文件。该失败会记录为 `[Cook] Not this project's to build (plugin '<Name>')`，烘焙继续进行——该插件会附带这些资源，它们正是被打包的内容。请参阅[烘焙行为](../generation/ephemeral.md#cook-behaviour)。

<a id="dsh8091"></a>

## DSH8091

<!-- generated:begin DSH8091 -->
**严重性** 错误

**消息**

```
DreamShader Root '%s' references project plugin '%s', but the plugin is not enabled.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:70`
<!-- generated:end DSH8091 -->

**原因。** 源通过根指定其资源的位置：`Game`、`Engine` 或 `Plugin.<Name>`（在 1.x 中为块上的 `Root = "..."`；在 2.0 中为文件所在的源根，或带有对象路径的 `/// @name`）。该插件存在但已被禁用，因此其内容路径未被挂载。

**修复。** 在 `.uproject` 或插件窗口中启用该插件。

<a id="dsh8092"></a>

## DSH8092

<!-- generated:begin DSH8092 -->
**严重性** 错误

**消息**

```
DreamShader Root '%s' references project plugin '%s', but the plugin cannot contain content.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:75`
<!-- generated:end DSH8092 -->

**原因。** 源通过根指定其资源的位置：`Game`、`Engine` 或 `Plugin.<Name>`（在 1.x 中为块上的 `Root = "..."`；在 2.0 中为文件所在的源根，或带有对象路径的 `/// @name`）。该插件已启用，且其描述符标明 `CanContainContent: false`。

**修复。** 在 `.uplugin` 中设置 `"CanContainContent": true`。

<a id="dsh8093"></a>

## DSH8093

<!-- generated:begin DSH8093 -->
**严重性** 错误

**消息**

```
DreamShader Root '%s' references project plugin '%s', but its Content directory does not exist: '%s'.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:81`
<!-- generated:end DSH8093 -->

**原因。** 源通过根指定其资源的位置：`Game`、`Engine` 或 `Plugin.<Name>`（在 1.x 中为块上的 `Root = "..."`；在 2.0 中为文件所在的源根，或带有对象路径的 `/// @name`）。该插件可以存放内容，但其 `Content` 目录不在磁盘上。

**修复。** 创建消息中指定的文件夹。

<a id="dsh8094"></a>

## DSH8094

<!-- generated:begin DSH8094 -->
**严重性** 错误

**消息**

```
DreamShader Root '%s' references project plugin '%s', but the plugin content is not mounted.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:87`
<!-- generated:end DSH8094 -->

**原因。** 源通过根指定其资源的位置：`Game`、`Engine` 或 `Plugin.<Name>`（在 1.x 中为块上的 `Root = "..."`；在 2.0 中为文件所在的源根，或带有对象路径的 `/// @name`）。该插件的内容目录存在，但引擎尚未挂载它——通常是在编辑器运行期间启用或创建的插件。

**修复。** 重启编辑器（或重启命令行工具运行），以便注册挂载点。

<a id="dsh8095"></a>

## DSH8095

<!-- generated:begin DSH8095 -->
**严重性** 错误

**消息**

```
DreamShader Root '%s' has an invalid plugin name.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:164`
<!-- generated:end DSH8095 -->

**原因。** 源通过根指定其资源的位置：`Game`、`Engine` 或 `Plugin.<Name>`（在 1.x 中为块上的 `Root = "..."`；在 2.0 中为文件所在的源根，或带有对象路径的 `/// @name`）。此根的插件部分为空或不是名称（`Plugin.` 后面没有任何内容）。

**修复。** 写入 `Plugin.MyPlugin`。

<a id="dsh8096"></a>

## DSH8096

<!-- generated:begin DSH8096 -->
**严重性** 错误

**消息**

```
DreamShader Root '%s' has an invalid plugin name.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:178`
<!-- generated:end DSH8096 -->

**原因。** 与 DSH8095 相同，针对该根的 `Plugins.<Name>` 拼写形式。

**修复。** 写入 `Plugins.MyPlugin`。

<a id="dsh8097"></a>

## DSH8097

<!-- generated:begin DSH8097 -->
**严重性** 错误

**消息**

```
DreamShader Root '%s' has an invalid package root.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:191`
<!-- generated:end DSH8097 -->

**原因。** 源通过根指定其资源的位置：`Game`、`Engine` 或 `Plugin.<Name>`（在 1.x 中为块上的 `Root = "..."`；在 2.0 中为文件所在的源根，或带有对象路径的 `/// @name`）。此根不属于上述任何一种，或者解析为引擎未知的包根。

**修复。** 使用 `Game`、`Engine` 或 `Plugin.<Name>`。

<a id="dsh8098"></a>

## DSH8098

<!-- generated:begin DSH8098 -->
**严重性** 错误

**消息**

```
DreamShader asset name must resolve to a non-empty asset path.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:324`
<!-- generated:end DSH8098 -->

**原因。** 所声明的资源名称在去除空白后为空。

**修复。** 为块指定一个 `Name`，或为导出指定一个名称。

<a id="dsh8099"></a>

## DSH8099

<!-- generated:begin DSH8099 -->
**严重性** 错误

**消息**

```
DreamShader asset name must resolve to a non-empty asset path.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:331`
<!-- generated:end DSH8099 -->

**原因。** 所声明的资源名称仅由分隔符组成（`/`），因此没有留下任何资源名称。

**修复。** 让路径以资源名称结尾：`Folder/M_Example`。

<a id="dsh8100"></a>

## DSH8100

<!-- generated:begin DSH8100 -->
**严重性** 错误

**消息**

```
DreamShader asset name '%s' produced an invalid asset name.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:337`
<!-- generated:end DSH8100 -->

**原因。** 所声明的资源名称的最后一段不是有效的对象名称（它包含引擎拒绝接受的字符，例如 `.`、`:` 或空格）。

**修复。** 使用字母、数字和 `_` 重命名该资源。

<a id="dsh8101"></a>

## DSH8101

<!-- generated:begin DSH8101 -->
**严重性** 错误

**消息**

```
Asset '%s' is open in an asset editor, so it was NOT rebuilt. An open editor works on its own copy of the asset and writes that copy back when you press Apply or Save,
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:422`
<!-- generated:end DSH8101 -->

**原因。** 该资源已在资源编辑器中打开。打开的编辑器作用于该资源的副本，并在应用或保存时将其写回，这会撤销重建；带有未保存更改的材质编辑器也可能已与源产生分歧。

**修复。** 关闭该资产的编辑器标签页并重新编译。

<a id="dsh8102"></a>

## DSH8102

<!-- generated:begin DSH8102 -->
**严重性** 错误

**消息**

```
Asset '%s' already exists and is not a Material.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:516`
<!-- generated:end DSH8102 -->

**原因。** 材质将要生成的位置已存在一个包（Package），且其中的对象不是 UMaterial。

**修复。** 重命名源（Source）的产物（Product），或移动已有资产。

<a id="dsh8103"></a>

## DSH8103

<!-- generated:begin DSH8103 -->
**严重性** 错误

**消息**

```
Asset '%s' already exists and was not generated by DreamShader. Rename your shader or move/delete the existing asset before regenerating.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:525`
<!-- generated:end DSH8103 -->

**原因。** 目标路径已存在一个材质，但它不带 DreamShader 来源信息，因此是手工创建的。DreamShader 从不覆盖不是由它生成的资产。

**修复。** 重命名产物，移动或删除手工创建的资产，或先将其反编译为源。

<a id="dsh8104"></a>

## DSH8104

<!-- generated:begin DSH8104 -->
**严重性** 错误

**消息**

```
Failed to create package '%s'.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:534`
<!-- generated:end DSH8104 -->

**原因。** `CreatePackage` 对材质的包操作失败 -- 长包名无效，或引擎拒绝该挂载点。

**修复。** 检查产物的根路径和文件夹；其上方日志行中有引擎给出的原因。

<a id="dsh8105"></a>

## DSH8105

<!-- generated:begin DSH8105 -->
**严重性** 错误

**消息**

```
Failed to create material '%s'.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:550`
<!-- generated:end DSH8105 -->

**原因。** 无法在其包中创建 UMaterial 对象。

**修复。** 查看日志中紧邻其上的引擎错误（通常是与重定向器（redirector）的名称冲突）；修复该文件夹中的重定向器。

<a id="dsh8106"></a>

## DSH8106

<!-- generated:begin DSH8106 -->
**严重性** 错误

**消息**

```
Asset '%s' already exists and is not a DreamShader instance material. Delete it (or remove Backend="Instance") before switching backends.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:578`
<!-- generated:end DSH8106 -->

**原因。** `Backend = ThinCustom` 生成 DreamShader 实例材质，而该路径上的资产属于另一种类（通常是较早的 Graph 构建生成的普通材质）。

**修复。** 删除旧资产，或保留其构建时所用的后端。

<a id="dsh8107"></a>

## DSH8107

<!-- generated:begin DSH8107 -->
**严重性** 错误

**消息**

```
Asset '%s' already exists and was not generated by DreamShader. Rename your shader or move/delete the existing asset before regenerating.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:588`
<!-- generated:end DSH8107 -->

**原因。** 与 DSH8103 相同，但针对 ThinCustom 实例：已有资产并非由 DreamShader 生成。

**修复。** 重命名产物，或移动或删除已有资产。

<a id="dsh8108"></a>

## DSH8108

<!-- generated:begin DSH8108 -->
**严重性** 错误

**消息**

```
Failed to create package '%s'.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:598`
<!-- generated:end DSH8108 -->

**原因。** `CreatePackage` 对 ThinCustom 实例的包操作失败。

**修复。** 检查产物的根路径和文件夹。

<a id="dsh8109"></a>

## DSH8109

<!-- generated:begin DSH8109 -->
**严重性** 错误

**消息**

```
Failed to create instance material '%s'.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:610`
<!-- generated:end DSH8109 -->

**原因。** 无法创建 ThinCustom 实例材质对象。

**修复。** 查看日志中其上方引擎给出的错误。

<a id="dsh8110"></a>

## DSH8110

<!-- generated:begin DSH8110 -->
**严重性** 错误

**消息**

```
Asset '%s' already exists as '%s', but %s generation requires '%s'. Delete or move the existing asset and regenerate it.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:649`
<!-- generated:end DSH8110 -->

**原因。** 目标路径已存在一个函数资产，其种类与源所构建的不同：源是层（Layer）之处是普通 MaterialFunction，源是混合（Blend）之处是层。这三者是彼此不同的引擎种类，资产无法就地更改种类。

**修复。** 删除或移动已有资产并重新编译；对它的引用必须重新建立，因此当旧资产正在使用时，最好换用新名称。

<a id="dsh8111"></a>

## DSH8111

<!-- generated:begin DSH8111 -->
**严重性** 错误

**消息**

```
Asset '%s' already exists and is not a MaterialFunction asset.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:655`
<!-- generated:end DSH8111 -->

**原因。** 函数将要生成的位置已存在一个包，且其中的对象根本不是材质函数。

**修复。** 重命名产物，或移动已有资产。

<a id="dsh8112"></a>

## DSH8112

<!-- generated:begin DSH8112 -->
**严重性** 错误

**消息**

```
Asset '%s' already exists and was not generated by DreamShader. Rename your function or move/delete the existing asset before regenerating.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:662`
<!-- generated:end DSH8112 -->

**原因。** 目标路径已存在一个材质函数，且并非由 DreamShader 生成。

**修复。** 重命名该函数，移动或删除已有资产，或先将其反编译为源。

<a id="dsh8113"></a>

## DSH8113

<!-- generated:begin DSH8113 -->
**严重性** 错误

**消息**

```
Failed to create package '%s'.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:671`
<!-- generated:end DSH8113 -->

**原因。** `CreatePackage` 对函数的包操作失败。

**修复。** 检查产物的根路径和文件夹。

<a id="dsh8114"></a>

## DSH8114

<!-- generated:begin DSH8114 -->
**严重性** 错误

**消息**

```
Failed to create material function '%s'.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:684`
<!-- generated:end DSH8114 -->

**原因。** 无法创建材质函数对象。

**修复。** 查看日志中其上方引擎给出的错误。

<a id="dsh8115"></a>

## DSH8115

<!-- generated:begin DSH8115 -->
**严重性** 错误

**消息**

```
Asset '%s' was edited by hand since DreamShader generated it from '%s', so it was NOT rebuilt (rebuilding would destroy those edits). %s
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderGeneratedAssetMetadata.cpp:412`
<!-- generated:end DSH8115 -->

**原因。** 该资产与其上次生成时标记的输出摘要不再匹配 -- 有人手工编辑过它，重新构建会破坏这些改动

**修复。** 响应通知，或右键单击资产 > DreamShader：**还原到源**（源是正确的）、**采纳到源**（资产是正确的）、**从 DreamShader 分离**（两者都不对，停止管理它）。`-Force` 无法通过此检查

**另请参阅** [Divergence](../generation/divergence.md)

<a id="dsh8116"></a>

## DSH8116

<!-- generated:begin DSH8116 -->
**严重性** 错误

**消息**

```
Generated DreamShader asset '%s' could not be saved.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderGeneratedAssetMetadata.cpp:494`
<!-- generated:end DSH8116 -->

**原因。** 无法将生成的资产写入磁盘：`.uasset` 是只读的或已签入，其他进程占用了它，或磁盘已满。

**修复。** 签出该文件 / 清除只读标志并重新编译。在此之前，该资产仅存在于内存中。

<a id="dsh8117"></a>

## DSH8117

<!-- generated:begin DSH8117 -->
**严重性** 错误

**消息**

```
Generated DreamShader asset packages could not be saved.%s
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderGeneratedAssetMetadata.cpp:527`
<!-- generated:end DSH8117 -->

**原因。** 保存生成包的批处理失败；消息之后的列表会列出每个包及其原因。

**修复。** 修复列出的第一个原因（通常是只读文件）并重新编译。

<a id="dsh8118"></a>

## DSH8118

<!-- generated:begin DSH8118 -->
**严重性** 错误

**消息**

```
Asset Path root '%s' references plugin '%s', but no enabled plugin with that name was found.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:123`
<!-- generated:end DSH8118 -->

**原因。** `Path(Root, "Folder/Asset")` 将其第一个参数解析为挂载点：`Game`、`Engine` 或 `Plugin.<Name>` / `Plugins.<Name>`。此处指定的插件没有任何已启用的插件与之对应。

**修复。** 检查插件的名称以及它是否已启用。

<a id="dsh8119"></a>

## DSH8119

<!-- generated:begin DSH8119 -->
**严重性** 错误

**消息**

```
Asset Path root '%s' references plugin '%s', but the plugin is not enabled.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:128`
<!-- generated:end DSH8119 -->

**原因。** `Path(Root, "Folder/Asset")` 将其第一个参数解析为挂载点：`Game`、`Engine` 或 `Plugin.<Name>` / `Plugins.<Name>`。该插件存在但已被禁用。

**修复。** 启用它。

<a id="dsh8120"></a>

## DSH8120

<!-- generated:begin DSH8120 -->
**严重性** 错误

**消息**

```
Asset Path root '%s' references plugin '%s', but the plugin cannot contain content.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:133`
<!-- generated:end DSH8120 -->

**原因。** `Path(Root, "Folder/Asset")` 将其第一个参数解析为挂载点：`Game`、
`Engine`，或 `Plugin.<Name>` / `Plugins.<Name>`。插件无法包含内容
(`CanContainContent: false`)。

**修复。** 将引用指向包含该资产的插件，或在 `.uplugin` 中启用内容。

<a id="dsh8121"></a>

## DSH8121

<!-- generated:begin DSH8121 -->
**严重性** 错误

**消息**

```
Asset Path root '%s' references plugin '%s', but its Content directory does not exist: '%s'.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:139`
<!-- generated:end DSH8121 -->

**原因。** `Path(Root, "Folder/Asset")` 将其第一个参数解析为挂载点：`Game`、
`Engine`，或 `Plugin.<Name>` / `Plugins.<Name>`。插件的 `Content` 目录在
磁盘上缺失。

**修复。** 检查消息打印的路径。

<a id="dsh8122"></a>

## DSH8122

<!-- generated:begin DSH8122 -->
**严重性** 错误

**消息**

```
Asset Path root '%s' references plugin '%s', but the plugin content is not mounted.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:145`
<!-- generated:end DSH8122 -->

**原因。** `Path(Root, "Folder/Asset")` 将其第一个参数解析为挂载点：`Game`、
`Engine`，或 `Plugin.<Name>` / `Plugins.<Name>`。插件的内容未在此
会话中挂载。

**修复。** 重启编辑器以注册插件的挂载点。

<a id="dsh8123"></a>

## DSH8123

<!-- generated:begin DSH8123 -->
**严重性** 错误

**消息**

```
Relative asset Path(...) references require a root such as Game, Engine, or Plugin.PluginName.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:190`
<!-- generated:end DSH8123 -->

**原因。** 带一个参数的 `Path("Folder/Asset")` 必须是绝对对象路径（`/Game/...`）。
相对路径需要将根作为其第一个参数。

**修复。** 编写 `Path(Game, "Folder/Asset")`。

<a id="dsh8124"></a>

## DSH8124

<!-- generated:begin DSH8124 -->
**严重性** 错误

**消息**

```
Asset Path root '%s' has an invalid plugin name.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:210`
<!-- generated:end DSH8124 -->

**原因。** `Plugin.<Name>` 或 `Plugins.<Name>` 根的插件部分是空的或不是名称。

**修复。** 编写 `Path(Plugin.MyPlugin, "Folder/Asset")`。

<a id="dsh8125"></a>

## DSH8125

<!-- generated:begin DSH8125 -->
**严重性** 错误

**消息**

```
Asset Path root '%s' has an invalid plugin name.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:223`
<!-- generated:end DSH8125 -->

**原因。** 与 DSH8124 相同，针对斜杠拼写 `Plugin/<Name>` 和 `Plugins/<Name>`。

**修复。** 编写 `Path(Plugin.MyPlugin, "Folder/Asset")`。

<a id="dsh8126"></a>

## DSH8126

<!-- generated:begin DSH8126 -->
**严重性** 错误

**消息**

```
Unsupported asset Path root '%s'. Use Game, Engine, or Plugin.PluginName.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:233`
<!-- generated:end DSH8126 -->

**原因。** `Path(...)` 的第一个参数不是 DreamShader 已知的任何根。

**修复。** 使用 `Game`、`Engine` 或 `Plugin.<Name>`。

<a id="dsh8127"></a>

## DSH8127

<!-- generated:begin DSH8127 -->
**严重性** 错误

**消息**

```
Asset reference cannot be empty.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:318`
<!-- generated:end DSH8127 -->

**原因。** 资产引用为空：`Path()`、空字符串，或留空的默认值。

**修复。** 指定资产名称，或移除该引用。

<a id="dsh8128"></a>

## DSH8128

<!-- generated:begin DSH8128 -->
**严重性** 错误

**消息**

```
Asset Path(...) reference is missing a closing ')'.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:327`
<!-- generated:end DSH8128 -->

**原因。** `Path(` 从未闭合。

**修复。** 添加 `)`。

<a id="dsh8129"></a>

## DSH8129

<!-- generated:begin DSH8129 -->
**严重性** 错误

**消息**

```
Asset Path(...) contains an unterminated string literal.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:334`
<!-- generated:end DSH8129 -->

**原因。** `Path(...)` 中的字符串没有闭合引号。

**修复。** 闭合字符串。

<a id="dsh8130"></a>

## DSH8130

<!-- generated:begin DSH8130 -->
**严重性** 错误

**消息**

```
Asset Path(...) expects either 1 argument (/Game/... path) or 2 arguments (Game|Engine|Plugin.PluginName, asset path).
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:348`
<!-- generated:end DSH8130 -->

**原因。** `Path(...)` 没有参数或超过两个参数。

**修复。** 编写 `Path("/Game/Folder/Asset")` 或 `Path(Game, "Folder/Asset")`。

<a id="dsh8131"></a>

## DSH8131

<!-- generated:begin DSH8131 -->
**严重性** 错误

**消息**

```
Asset reference requires a non-empty path.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:378`
<!-- generated:end DSH8131 -->

**原因。** 资产引用的路径部分为空（`Path(Game, "")`）。

**修复。** 编写资产的文件夹和名称。

<a id="dsh8132"></a>

## DSH8132

<!-- generated:begin DSH8132 -->
**严重性** 错误

**消息**

```
Invalid asset path '%s'.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:402`
<!-- generated:end DSH8132 -->

**原因。** 引用解析出的内容不是有效的对象路径：它包含引擎拒绝的字符、重复的 `/`，或没有资产名称。

**修复。** 从内容浏览器复制引用（*Copy Reference*）。

<a id="dsh8133"></a>

## DSH8133

<!-- generated:begin DSH8133 -->
**严重性** 错误

**消息**

```
DSH8133: '{0}' is built from '{1}', a file of an installed package, which the package's installer replaces on every update; anything adopted into it would be lost then. Use Revert to Source to undo the edit, or Detach to keep the asset as it is.
```

**引发者** `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:444`
<!-- generated:end DSH8133 -->

**原因。** 对由包构建的资产请求了 *Adopt Into Source*（或 *Adopt Tweaks Into Source*）。包的文件属于其安装程序，每次更新时都会替换它们，因此 Adopt 写入其中的任何内容都会在下次更新时丢失——连同它本应保留的编辑一起丢失。未写入任何内容。

**修复。** 使用 *Revert to Source* 丢弃手动编辑，或使用 *Detach* 保持资产原样并停止生成它。要更改包构建的内容，请更改包（上游，或 `Packages` 之外的副本）。

<a id="dsh8149"></a>

## DSH8149

<!-- generated:begin DSH8149 -->
**严重性** 错误

**消息**

```
DSH8149: '{0}' uses conditional compilation, and '{1}' holds only the branch that was taken -- adopting it would write that one branch back over the file and delete the rest. Move the change into the matching branch of the source by hand, or use DreamShader > Detach first if this asset should stop being generated from it.
```

**引发者** `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:1166`、`Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:527`、`Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:726`、`Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:905`
<!-- generated:end DSH8149 -->

**原因。** **Adopt Into Source** 被要求重写使用条件编译（`#if` / `#ifdef` / `#ifndef`）的源。Adopt 从资产重建源文本，而资产只保存所采用的分支——因此采纳会将该分支写回文件，并静默删除所有其他分支

**修复。** 手动将更改移入源的匹配分支（资产的反编译器输出是一个很好的起点），或者如果此资产应停止从该源生成，请先使用 **Detach From DreamShader**。Revert 和 Detach 不受影响，因为两者都不写入源

**另见** [Preprocessor](../language/preprocessor.md)、[Divergence](../generation/divergence.md)

<a id="dsh8155"></a>

## DSH8155

<!-- generated:begin DSH8155 -->
**严重性** 警告

**消息**

```
rebuilding '%s' from '%s' dropped %d parameter override(s) the rebuilt material no longer declares: %s.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderThinCustomParameterOverrides.cpp:287`
<!-- generated:end DSH8155 -->

**原因。** 对 ThinCustom 材质的重建捕获了在生成的实例上设置的参数覆盖，并在之后将其放回，但其中一个或多个覆盖命名的参数在重建后的材质中不再声明——源重命名了它、移除了它，或更改了它的类型——因此这些值无处可去并被丢弃。恢复按名称和类型匹配，因为名称是生成器拆除并重建的图中唯一幸存的东西。同一行还报告了此构建无法重新应用的参数类型的覆盖（例如纹理集合）

**修复。** 没有损坏，重建成功；在新名称下再次在实例上设置该值，或将其作为 `Properties` 默认值移入源，这样就不需要覆盖。如果参数已重命名且你希望将值保留下来，请将其重命名回来，重建，然后在与覆盖相同的编辑中再次重命名

**另见** [Divergence](../generation/divergence.md#parameter-overrides-on-a-generated-thincustom-instance)

<a id="dsh8200"></a>

## DSH8200

<!-- generated:begin DSH8200 -->
**严重性** 错误

**消息**

```
'{0}' does not resolve to a valid asset path. {1}
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1287`、`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:1336`
<!-- generated:end DSH8200 -->

**原因。** 产品的资产名称，或 `/// @name /Game/...` 路径覆盖，无法解析为 Unreal 可接受的包路径。引用的 1.x 消息会说明违反了哪条规则：清理后叶节点为空、某个文件夹段清理后为空、`Root` 指定了未启用或无法承载内容的插件，或路径被 Unreal 本身拒绝。

**修复。** 先阅读引用的消息——它会指出有问题的段。作为裸叶节点的 `@name` 只替换资产名称并保留根规则；以 `/` 开头的 `@name` 是完整路径，其最后一段是资产，前面的段是根，因此 `/Game/X/M_Y` 会将 `M_Y` 放到 `/Game/X` 下。位于插件源根目录下的文件默认挂载到该插件的内容挂载点，因此完全不需要 `/Game` 前缀。

<a id="dsh8201"></a>

## DSH8201

<!-- generated:begin DSH8201 -->
**严重性** 错误

**消息**

```
The material for '{0}' could not be created or reused. {1}
```

**引发自** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:528`
<!-- generated:end DSH8201 -->

**原因。** 材质产品的目标位置已存在且不是 `UMaterial`，或它在磁盘上存在但没有 DreamShader 的源元数据，或无法创建该包。最后一种情况很少见，通常意味着路径不可写。

**修复。** 对于“已存在且不是由 DreamShader 生成”：正是这一防护使得同一路径下手动编写的材质永远不会被清除和覆盖。重命名着色器，或移动/删除现有资产，然后重新编译。

<a id="dsh8202"></a>

## DSH8202

<!-- generated:begin DSH8202 -->
**严重性** 错误

**消息**

```
The material function for '{0}' could not be created or reused. {1}
```

**引发自** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:749`
<!-- generated:end DSH8202 -->

**原因。** 与 DSH8201 相同，针对材质函数产品——多了一种失败方式：该路径下的资产存在，但函数类错误。`@layer` 需要 `UMaterialFunctionMaterialLayer`，`@layerblend` 需要 `UMaterialFunctionMaterialLayerBlend`，而普通导出需要 `UMaterialFunction`。改变导出函数的类型会改变它必须成为的类。

**修复。** 删除或移动现有资产并重新生成。类无法就地更改：每个调用该函数的材质都持有对它的类型化引用。

<a id="dsh8203"></a>

## DSH8203

<!-- generated:begin DSH8203 -->
**严重性** 错误

**消息**

```
The ThinCustom instance for '{0}' could not be created or reused. {1}
```

**引发自** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:604`
<!-- generated:end DSH8203 -->

**原因。** 与 DSH8201 相同，针对 ThinCustom 材质所变成的 `UDreamShaderMaterialInstance`。

**修复。** 与 DSH8201 相同。如果该资产作为普通 `UMaterial` 存在，则说明该文件过去使用 `Backend = "Graph"` 编译，现在要求 ThinCustom（或项目默认值已更改）；删除旧材质，或用 `#pragma material(Backend = "Graph")` 将后端固定回去。

<a id="dsh8204"></a>

## DSH8204

<!-- generated:begin DSH8204 -->
**严重性** 错误

**消息**

```
The emitter needs the builtin catalog the front end was bound against, but the emit context carries none.
```

**引发自** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1273`
<!-- generated:end DSH8204 -->

**原因。** 调用 `EmitDreamShaderIRProduct` 时未提供 `FIREmitContext::Catalog`，或提供的为空。目录是发射器将属性名称转换为 `EMaterialProperty` 的方式，也是它精确解析反射类的方式，因此没有它就无法继续。

**修复。** 内部问题：管线在每次运行时构建目录一次，并且必须传递与绑定器所使用的同一个目录。如果这到达了用户，说明目录清单加载失败或反射没有产生任何内容；反射填充器只记录该情况（`BuildBuiltinCatalogFromReflection` 在任何编译之前运行，且没有可引发诊断的接收器，因此它没有代码）。

<a id="dsh8205"></a>

## DSH8205

<!-- generated:begin DSH8205 -->
**严重性** 错误

**消息**

```
Product index {0} does not exist in this module, which has {1}.
```

**引发自** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1262`、`Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:253`
<!-- generated:end DSH8205 -->

**原因。** 内部不一致：产品索引超出模块的范围，或图的拓扑顺序指定了图中不存在的节点。

**修复。** 这不是创作错误。请连同源文件一起报告；对同一文件使用 `dsc dump-ir` 可以显示交给发射器的图。

<a id="dsh8206"></a>

## DSH8206

<!-- generated:begin DSH8206 -->
**严重性** 错误

**消息**

```
'{0}' is open in an asset editor, so it was not rebuilt. {1}
```

**引发自** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:390`
<!-- generated:end DSH8206 -->

**原因。** 资产已在资产编辑器中打开。打开的编辑器并不会编辑资产本身：它会复制资产，并在 Apply 或 Save 时将副本复制回去，因此在重建期间打开的编辑器持有重建前的副本，下一次 Apply 会静默还原重建所做的一切。

**修复。** 关闭资产编辑器并重新编译。拒绝是唯一可用的安全答案：该副本是否有未保存的编辑是 MaterialEditor 模块的私有信息，而盲目关闭它会在保存时编译的过程中弹出保存提示。

<a id="dsh8207"></a>

## DSH8207

<!-- generated:begin DSH8207 -->
**严重性** 错误

**消息**

```
'{0}' no longer holds what DreamShader generated into it, so it was not rebuilt. {1}
```

**引发自** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:400`
<!-- generated:end DSH8207 -->

**原因。** 资产不再保存 DreamShader 上次生成到其中的内容——有人手动编辑了图。上次成功生成结束时打上的输出摘要不再与资产的内容匹配。

**修复。** 引用的消息列出了三种解决方式：Revert（丢弃手动编辑并从源重建）、Adopt（将当前资产反编译回源文件）或 Detach（移除 DreamShader 的标记，使资产不再被生成）。在生成的 ThinCustom 实例上调整参数覆盖不属于偏离，绝不会引发此问题——实例正是为此而存在。

<a id="dsh8208"></a>

## DSH8208

<!-- generated:begin DSH8208 -->
**严重性** 警告

**消息**

```
'{0}' could not be snapshotted before rebuilding it, so a failed rebuild will not be rolled back.
```

**引发自** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:444`、`Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:799`
<!-- generated:end DSH8208 -->

**原因。** 原子回滚无法在重建开始前对资产进行快照，因此中途失败的重建会使资产被清空，而不是将其恢复。编译会继续；这是关于如果之后有东西失败时会发生什么的警告。

**修复。** 通常表明资产处于异常状态（没有仅编辑器数据）。如果编译随后成功，则没有丢失任何内容。如果失败，请在修复源后重新生成——资产将从头重建。

<a id="dsh8209"></a>

## DSH8209

<!-- generated:begin DSH8209 -->
**严重性** 信息

**消息**

```
'{0}' was left alone: another editor owns writing this project's generated assets to disk.
```

**引发自** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:365`
<!-- generated:end DSH8209 -->

**原因。** 另一个编辑器持有本项目的 DreamShader 桥接所有权锁，而只有该进程会将生成的资产写入磁盘。两个编辑器保存同一个包不是合并；而是包损坏或编辑器死亡。

**修复。** 无需修复。从拥有锁的编辑器进行编译，或关闭另一个编辑器。这会被报告为跳过，而不是失败。

<a id="dsh8210"></a>

## DSH8210

<!-- generated:begin DSH8210 -->
**严重性** 警告

**消息**

```
'{0}' has no property named '{1}', so that value was not written.
```

**引发自** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1795`、`Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:324`、`Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:361`
<!-- generated:end DSH8210 -->

**原因。** 在 uniform 上，或在 Group / Desc / SortPriority / 滑块上，`/// @key value` 的名称未在表达式类中反射。该值未被写入；参数上的其他所有内容都已写入。

**修复。** 对照节点的 Details 面板检查拼写——键是反射属性名称。某些参数类确实不携带组织字段（`Group`、`SortPriority`、`Desc`），因为它们不是 `UMaterialExpressionParameter` 的子类；对于这些，此警告是预期且无害的。为较新引擎版本准备的键也可以保留：当该属性出现时它就会开始生效。

<a id="dsh8211"></a>

## DSH8211

<!-- generated:begin DSH8211 -->
**严重性** 错误

**消息**

```
'{0}' is not a material expression class this engine has.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:510`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:666`
<!-- generated:end DSH8211 -->

**原因。** `UE.X(...)` 或 `UE.Expression(Class = "X")` 指定了此引擎构建中不存在的材质表达式类。绑定器已针对目录解析它，因此这通常意味着目录与正在运行的引擎不一致 —— 可能是导出的目录已过期，或该类位于某个构建标志之后。

**修复。** 重新导出目录（`dsc export-catalog`）并再次编译。如果该类确实不存在（例如在没有 Substrate 的引擎上使用 Substrate 节点，或插件类未被加载），请使用不同的节点，或用 `#if` 保护代码。

<a id="dsh8212"></a>

## DSH8212

<!-- generated:begin DSH8212 -->
**严重性** 错误

**消息**

```
MakeMaterialAttributes has no pin for the attribute '{0}'.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:124`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:156`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:220`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1774`
<!-- generated:end DSH8212 -->

**原因。** 某个值在创建的节点上没有可连接的引脚。对于反射节点，指定的输入在其表达式类上不匹配任何 `FExpressionInput`，也不匹配活动节点在引脚上显示的任何名称；与 DSH8211 一样，这通常意味着目录已过期，因为绑定器是依据目录检查引脚名称的。对于属性节点：`MakeMaterialAttributes` 有一个固定的引脚列表，而它没有对应引脚的属性（`FrontMaterial`、`SurfaceThickness`）由它上面的 `SetMaterialAttributes` 进行设置 —— 当该 Set 也无法接收该属性时、当该属性是整体材质伪属性（一组属性无法包含它）时，或当它自己的某个 `SetMaterialAttributes` 无法接收值时，会引发该代码。

**修复。** 重新导出目录。如果引脚在引擎版本之间被重命名，请使用新名称。对于 `MakeMaterialAttributes`，八个自定义 UV 是一个数组引脚，通过 `CustomizedUVs7` 写入 `CustomizedUVs0`。

<a id="dsh8213"></a>

## DSH8213

<!-- generated:begin DSH8213 -->
**严重性** 错误

**消息**

```
'{0}' has no property named '{1}'.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1802`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1815`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:315`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:372`
<!-- generated:end DSH8213 -->

**原因。** 反射属性拒绝了该值：类型的字面量形状错误。字面量写入器引用的消息会说明是哪种 —— 不是数字、不是有效的枚举值、对象路径无法加载或加载到了错误的类。

**修复。** 对于枚举，写入枚举项时不要带其前缀（应为 `Normal`，而非 `SAMPLERTYPE_Normal`；两者都可接受，短形式是规范形式）。对于资产，写入以 `/` 开头的完整对象路径。对于数字，移除任何单位后缀。

<a id="dsh8214"></a>

## DSH8214

<!-- generated:begin DSH8214 -->
**严重性** 错误

**消息**

```
Failed to create a FunctionInput node.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:123`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:152`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:281`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:40`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:142`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:173`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:239`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:339`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:75`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1019`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1137`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1170`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1209`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1244`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1276`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1313`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1365`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1769`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:360`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:371`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:383`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:398`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:518`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:675`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:743`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:777`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:797`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:818`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:839`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:874`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:896`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:915`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:940`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:971`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:136`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:150`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:170`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:208`
<!-- generated:end DSH8214 -->

**原因。** `UMaterialEditingLibrary` 拒绝在该资产中创建该类的表达式。常见的实际原因是该资产种类不允许使用该类 —— 有些表达式仅适用于材质，不能放入材质函数，反之亦然。

**修复。** 检查该节点在材质函数中是否合法。如果图表需要它，请将其放入调用该函数的材质中，并将值作为函数输入传入。

<a id="dsh8215"></a>

## DSH8215

<!-- generated:begin DSH8215 -->
**严重性** 错误

**消息**

```
A material setting on '{0}' was refused. {1}
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:460`
<!-- generated:end DSH8215 -->

**原因。** `#pragma material(...)` 键不是 `UMaterial` 上可设置的属性路径，或其值对该属性无效。引用的 1.x 消息会说明是其中哪一种情况。

**修复。** 键是 `UMaterial` 属性名，外加四个友好别名 `BlendMode` / `RenderType`、`ShadingModel`、`MaterialDomain` / `Domain`。数组属性需要显式的 `[index]`。`Backend` 在此处不适用 —— 它用于选择后端，并由绑定器消费。

<a id="dsh8216"></a>

## DSH8216

<!-- generated:begin DSH8216 -->
**严重性** 错误

**消息**

```
BreakMaterialAttributes does not publish the attribute '{0}', so it cannot be read from a material that came through a pin.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1602`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1845`
<!-- generated:end DSH8216 -->

**原因。** 两种情况之一。材质属性名称（`m.SomeThing`，一个 `MakeMaterialAttributes` 输入）不在此引擎的属性表中。或者，从通过引脚传来的材质（LayerBlend 输入、反射节点的材质输出、函数调用的材质结果）中读取了某个属性，而读取它的 `BreakMaterialAttributes` 节点没有对应的引脚：`MaterialAttributes` 本身、`FrontMaterial`、`SurfaceThickness`。第二种形式在读取处引发，绝不会在节点上引发：节点为每个属性都带有一个槽位，而无人读取的槽位不是错误。

**修复。** 对于第二种形式，请在材质仍在构建时、在它跨过引脚之前读取该属性，或将值随材质一起传递。对于第一种，请检查拼写。接受的别名有：MaterialAttributes 对应 `Attributes`，EmissiveColor 对应 `Emissive`，AmbientOcclusion 对应 `AO`，WorldPositionOffset 对应 `WPO`，PixelDepthOffset 对应 `PDO`，CustomData0 / CustomData1 对应 `ClearCoat` / `ClearCoatRoughness`，以及 CustomizedUVs0..7 对应 `CustomizedUV0`..`7`。

<a id="dsh8217"></a>

## DSH8217

<!-- generated:begin DSH8217 -->
**严重性** 错误

**消息**

```
This material has no input for the attribute '{0}'; check the material domain and shading model the file asks for.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:387`
<!-- generated:end DSH8217 -->

**原因。** 属性存在，但此材质没有对应的输入引脚。材质的可用输出取决于其域、混合模式和着色模型：后期处理材质没有 Roughness，不透明材质没有 Opacity。

**修复。** 在 `#pragma material(...)` 中设置该属性所需的域/混合模式/着色模型，或停止写入该属性。

<a id="dsh8218"></a>

## DSH8218

<!-- generated:begin DSH8218 -->
**严重性** 错误

**消息**

```
The default texture for parameter '{0}' could not be loaded from '{1}'.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:236`
<!-- generated:end DSH8218 -->

**原因。** 纹理参数的默认资产路径无法加载。

**修复。** 检查路径 —— 它必须是完整的对象路径（`/Game/Textures/T_X.T_X`，或 `/Game/Textures/T_X`）。被移动或重命名的纹理需要更新源；重定向器会被跟随，已删除的资产则不会。

<a id="dsh8219"></a>

## DSH8219

<!-- generated:begin DSH8219 -->
**严重性** 错误

**消息**

```
This function call names no material function asset.
```

**由以下项引发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:247`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:272`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:288`
<!-- generated:end DSH8219 -->

**原因。** `FunctionCall` 节点的材质函数资产无法加载，或引擎拒绝将其赋给调用节点。拒绝的情况是循环：材质函数不能直接或通过另一个函数调用自身。

**修复。** 对于加载失败，请检查 `extern` 声明上的 `@asset` 路径。对于拒绝，请打破循环 —— 相互调用的两个函数需要将共享部分提取到第三个函数中。

<a id="dsh8220"></a>

## DSH8220

<!-- generated:begin DSH8220 -->
**严重性** 错误

**消息**

```
'{0}' has no input named '{1}'.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:331`
<!-- generated:end DSH8220 -->

**原因。** 某次调用指定了一个函数资源并不具有的输入。可能是该函数以不同的参数列表被重新构建，
或者源中的 `extern` 原型与资源不匹配。

**修复。** 让 `extern` 原型与该资源的实际输入匹配。`dsc index` 会列出它们；
该函数自身的 Details 面板也会列出。

<a id="dsh8221"></a>

## DSH8221

<!-- generated:begin DSH8221 -->
**严重性** 错误

**消息**

```
'{0}' has no output named '{1}'.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:385`
<!-- generated:end DSH8221 -->

**原因。** 与 DSH8220 相同，但针对输出。函数的单个未命名返回值称为 `Result`。

**修复。** 让 `extern` 原型与该资源的输出匹配。

<a id="dsh8222"></a>

## DSH8222

<!-- generated:begin DSH8222 -->
**严重性** 错误

**消息**

```
This call targets product {0} of the same file, but that product has not been emitted yet; the pipeline must compile products in dependency order.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:229`
<!-- generated:end DSH8222 -->

**原因。** 对同一文件中另一个导出函数的调用在该函数的资源被发出之前就运行了，因此它的路径
尚未确定。内部性：管线按依赖顺序编译文件的产物，正是为了防止这种情况发生。

**修复。** 除非该文件的导出之间存在调用循环（这种情况绑定器本应先拒绝），否则这不是创作错误。
请连同源文件一起报告。

<a id="dsh8223"></a>

## DSH8223

<!-- generated:begin DSH8223 -->
**严重性** 错误

**消息**

```
The emitter has no rule for the IR operation '{0}'.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1062`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:335`
<!-- generated:end DSH8223 -->

**原因。** IR 携带了发射器没有规则可处理的操作：某个核心操作的表中条目未指定引擎表达式类，
且没有手写的降级实现。

**修复。** 这不是创作错误——而是核心操作表与发射器之间的缺口。请报告消息中指定的操作。

<a id="dsh8224"></a>

## DSH8224

<!-- generated:begin DSH8224 -->
**严重性** 错误

**消息**

```
A SetMaterialAttributes node has no MaterialAttributes input; there is nothing for it to modify.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:181`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:245`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1155`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1183`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1225`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1260`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1299`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1660`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:347`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:695`
<!-- generated:end DSH8224 -->

**原因。** 节点的操作数或输入数量与其操作不匹配——例如带两个操作数的 Select、带三个操作数的
Append、没有基础输入的 `SetMaterialAttributes`。IR 验证器本应捕获这一问题。

**修复。** 这不是创作错误。对文件使用 `dsc dump-ir` 可显示畸形节点。

<a id="dsh8225"></a>

## DSH8225

<!-- generated:begin DSH8225 -->
**严重性** 错误

**消息**

```
This node reads node {0}, which has not been emitted; the graph's topological order is inconsistent.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1587`
<!-- generated:end DSH8225 -->

**原因。** 节点从尚未被发出的节点读取值，这意味着图表的拓扑顺序并未真正将操作数置于其使用者之前。

**修复。** 这不是创作错误。请连同源文件一起报告；IR 中的循环是常见原因，`dsc dump-ir` 可显示它。

<a id="dsh8226"></a>

## DSH8226

<!-- generated:begin DSH8226 -->
**严重性** 错误

**消息**

```
A Swizzle node carries no Mask property.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1091`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1100`
<!-- generated:end DSH8226 -->

**原因。** Swizzle 节点的 `Mask` 属性缺失、为空、长度超过四个分量，或包含 `x`、`y`、`z`、`w` 之外的内容。规范掩码是按顺序的小写 `xyzw`；
`rgba` 在绑定时被重写为该形式。

**修复。** 在此阶段这不是创作错误——用户书写错误的掩码会在更早阶段被拒绝，并给出引用该掩码的
消息。请连同源文件一起报告。

<a id="dsh8227"></a>

## DSH8227

<!-- generated:begin DSH8227 -->
**严重性** 错误

**消息**

```
'{0}' is not a Custom node output type; expected Float1 through Float4 or MaterialAttributes.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1403`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1436`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1444`
<!-- generated:end DSH8227 -->

**原因。** `@custom` 函数的返回类型，或其某个 `out` 参数，无法映射到
Custom 节点输出类型。引擎恰好有五种：`Float1`、`Float2`、`Float3`、`Float4` 和
`MaterialAttributes`。

**修复。** 更改类型。矩阵根本没有 Custom 节点输出类型——请在函数体内计算它并返回一个向量。
注意 `material` 可以是自定义节点的输出，但绝不能作为输入。

<a id="dsh8228"></a>

## DSH8228

<!-- generated:begin DSH8228 -->
**严重性** 错误

**消息**

```
'{0}' is not a material function input type; write one of Scalar, Vector2, Vector3, Vector4, Texture2D, TextureCube, Texture2DArray, VolumeTexture, StaticBool, Bool, MaterialAttributes or Substrate.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:55`
<!-- generated:end DSH8228 -->

**原因。** 材质函数输入的类型无法映射到 `EFunctionInputType`。

**修复。** 引擎的列表为 Scalar、Vector2、Vector3、Vector4、Texture2D、TextureCube、
Texture2DArray、VolumeTexture、TextureExternal、StaticBool、Bool、MaterialAttributes 和 Substrate。
矩阵或用户结构体不能作为函数输入；请分别传递其各分量。

<a id="dsh8229"></a>

## DSH8229

<!-- generated:begin DSH8229 -->
**严重性** 错误

**消息**

```
'{0}' was built but could not be saved. {1}
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1219`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:571`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:720`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:875`
<!-- generated:end DSH8229 -->

**原因。** 资源已成功构建，但其包无法写入磁盘——文件只读、源控制，或路径已不存在。

**修复。** 签出该资源，或使文件可写，然后重新编译。保存失败会使源哈希跳过失效，因此下一次编译
会重新构建并在不触发 `-Force` 的情况下重试保存。失败后进行的手动编辑仍会触发通常的
偏差保护。

<a id="dsh8230"></a>

## DSH8230

<!-- generated:begin DSH8230 -->
**严重性** 错误

**消息**

```
Cannot create a ThinCustom base material without an instance.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:305`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:326`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:352`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:643`
<!-- generated:end DSH8230 -->

**原因。** ThinCustom 实例所父级化的隐藏基础 `UMaterial` 无法作为该实例的子对象创建。

**修复。** 通常不是创作错误。如果问题持续存在，请删除该实例资源并重新生成它——来自旧构建、
父级化到单独兄弟材质的实例不会被复用，而是会创建一对全新的资源。

<a id="dsh8232"></a>

## DSH8232

<!-- generated:begin DSH8232 -->
**严重性** 错误

**消息**

```
'{0}' has a product kind the emitter does not know how to materialize.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1316`
<!-- generated:end DSH8232 -->

**原因。** 产品类型没有发射器知道如何构建的资源形态。内部性。

**修复。** 请连同源文件一起报告。

<a id="dsh8233"></a>

## DSH8233

<!-- generated:begin DSH8233 -->
**严重性** 错误

**消息**

```
This graph carries a MaterialSink, which only a material product has; a material function drives FunctionOutput nodes instead.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:372`
<!-- generated:end DSH8233 -->

**原因。** 图表携带了 `MaterialSink`，但正被发出到材质函数中。只有材质产品才拥有自己的
输出引脚；函数驱动 `FunctionOutput` 节点。

**修复。** 这不是创作错误。绑定器根据函数签名决定产品类型
（`void (inout material)` 为入口）；请连同源文件一起报告。

<a id="dsh8234"></a>

## DSH8234

<!-- generated:begin DSH8234 -->
**严重性** 错误

**消息**

```
Function output '{0}' has {1} operands; it needs exactly the one value it returns.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:179`
<!-- generated:end DSH8234 -->

**原因。** `FunctionOutput` 节点没有操作数，因此输出引脚不会驱动任何东西。

**修复。** 这不是创作错误——输出从未被赋值的导出函数会在更早阶段被按名称拒绝。请连同源文件
一起报告。

<a id="dsh8235"></a>

## DSH8235

<!-- generated:begin DSH8235 -->
**严重性** 错误

**消息**

```
'{0}' is not a sampler type; write one of the EMaterialSamplerType names, such as Color, Normal or LinearColor.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:278`
<!-- generated:end DSH8235 -->

**原因。** 纹理 uniform 的 `@sampler` 指定的名称不是 `EMaterialSamplerType`。

**修复。** 写出引擎采样器类型名称之一，不带其前缀：`Color`、`Grayscale`、
`Alpha`、`Normal`、`Masks`、`DistanceFieldFont`、`LinearColor`、`LinearGrayscale`、`Data`、
`External`、`VirtualColor` 以及 `SAMPLERTYPE_*` 列表中的其余项。`Color` 是默认值。

<a id="dsh8237"></a>

## DSH8237

<!-- generated:begin DSH8237 -->
**严重性** 信息

**消息**

```
'{0}' was left alone: its source hash is unchanged since it was last built.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:377`
<!-- generated:end DSH8237 -->

**原因。** 本产品编译到的资源已带有此构建的源哈希：该文件及它包含的每个头文件的预处理文本，
以及它读取的宏定义。编译未被强制，因此没有任何内容被重新构建。编译结果会显示
`Skipped <asset> from <source>; source hash is unchanged (build key <hash>).`，即 1.x
生成器为同样的跳过写出的行，并且它不会将该资源列为已生成。

**修复。** 无需修复；这是一次跳过，不是失败。使用 `-Force`（或 Recompile）编译以无论如何都重新构建。
跳过会让手动编辑过的资源保持原样。一旦源发生更改，下一次编译就会到达分歧门（DSH8207）。

<a id="dsh8240"></a>

## DSH8240

<!-- generated:begin DSH8240 -->
**严重性** 错误

**消息**

```
The material instance for '{0}' could not be created or reused. {1}
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1030`
<!-- generated:end DSH8240 -->

**原因。** `.dsi` 的实例资源无法被创建或复用；其后的句子是原因（DSH8241、DSH8242 或 DSH8253 的文字说明）。

**修复。** 针对该原因采取行动。

<a id="dsh8241"></a>

## DSH8241

<!-- generated:begin DSH8241 -->
**严重性** 错误

**消息**

```
Asset '%s' already exists as a '%s'; a .dsi builds a plain MaterialInstanceConstant. Rename the instance file or move the existing asset.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:260`
<!-- generated:end DSH8241 -->

**原因。** 另一个类的资源已经位于 `.dsi` 将构建其实例的位置——一个
材质，或一个同名 `.dss` 的 ThinCustom 实例。

**修复。** 重命名 `.dsi`（其文件名就是资源的名称）或移动现有资源。

<a id="dsh8242"></a>

## DSH8242

<!-- generated:begin DSH8242 -->
**严重性** 错误

**消息**

```
Asset '%s' already exists and was not generated by DreamShader. Rename the instance file or move/delete the existing asset before building it.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:269`
<!-- generated:end DSH8242 -->

**原因。** 目标路径处已存在一个材质实例，且它不是由 DreamShader 生成的。

**修复。** 重命名 `.dsi`，或用 *DreamShader > Adopt
Into Source* / `dsc decompile` 将现有实例纳入源。

<a id="dsh8243"></a>

## DSH8243

<!-- generated:begin DSH8243 -->
**严重性** 错误

**消息**

```
The parent '{0}' of '{1}' does not load; compile the source that builds it, or correct the Parent key.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1057`、`Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1082`
<!-- generated:end DSH8243 -->

**原因。** `Parent = ...` 指定的父项无法加载，或它只存在于内存中——是本次运行的
产物——并且在需要它的实例写入磁盘之前无法被保存。裸名称会在同一根下的源的产物中查找；对象路径按
所写内容加载。

**修复。** 编译构建父项的源，或修正该键。对于内存中的情况，
消息会附带保存错误：释放它指定的包文件。

<a id="dsh8244"></a>

## DSH8244

<!-- generated:begin DSH8244 -->
**严重性** 信息

**消息**

```
'{0}' was memory-only, so it was saved to disk first: '{1}' parents to it.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1090`
<!-- generated:end DSH8244 -->

**原因。** 父项仅被构建到内存中（一次 Ephemeral 构建），而磁盘上的实例无法
指向一个不在磁盘上的对象。父项已被先保存。

**修复。** 无需修复。

<a id="dsh8245"></a>

## DSH8245

<!-- generated:begin DSH8245 -->
**严重性** 错误

**消息**

```
'{0}' cannot parent to '{1}': that parent already descends from this instance.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1101`
<!-- generated:end DSH8245 -->

**原因。** `Parent` 指定的实例自身派生自这个实例，因此父链会
形成循环。

**修复。** 将 `Parent` 指向一个材质或更上层的实例。

<a id="dsh8246"></a>

## DSH8246

<!-- generated:begin DSH8246 -->
**严重性** 错误

**消息**

```
The parent asset '{0}' has no parameter {1} of the kind this instance overrides; the asset is older than its source. Compile the parent source first.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1123`
<!-- generated:end DSH8246 -->

**原因。** 磁盘上的父项资源缺少实例所覆盖的某个参数，尽管父项的
源声明了它：父项资源比其源更旧。

**修复。** 先编译父项的源，再编译实例。

<a id="dsh8247"></a>

## DSH8247

<!-- generated:begin DSH8247 -->
**严重性** 错误

**消息**

```
The engine dropped '{0}' as the parent of '{1}' while applying the static overrides; that parent does not allow them.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1196`
<!-- generated:end DSH8247 -->

**原因。** 应用静态开关覆盖导致引擎清除了实例的父项。当父项可能无法获得新的着色器排列时
引擎会这样做：一个已烘焙的材质（未附带其编辑器数据而发布的内容），引擎会拒绝为它编译
实例自身的排列。

**修复。** 将父项设为一个未烘焙的材质，或去除 `/// @static` 覆盖，使实例共享
其父项的着色器。

<a id="dsh8248"></a>

## DSH8248

<!-- generated:begin DSH8248 -->
**严重性** 错误

**消息**

```
The parent '{0}' exists only in memory and no DreamShader source builds it, so '{1}' cannot be saved against it; save the parent first.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1070`
<!-- generated:end DSH8248 -->

**原因。** 父项仅存在于内存中，且没有任何 DreamShader 源构建它（一个在编辑器中创建
且从未保存的材质），因此实例在磁盘上没有任何可引用的内容。

**修复。** 先保存父项资源。

<a id="dsh8249"></a>

## DSH8249

<!-- generated:begin DSH8249 -->
**严重性** 错误

**消息**

```
'{0}' is not an instance key; a key is a material property an instance can override, such as BlendMode, TwoSided, OpacityMaskClipValue or PhysMaterial.
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderInstanceSettings.cpp:365`
<!-- generated:end DSH8249 -->

**原因。** `#pragma instance(...)` 的一个键不是材质实例可以覆盖的内容。这些
键是引擎基础属性覆盖的字段（`BlendMode`、`TwoSided`、
`ShadingModel`、`OpacityMaskClipValue`、`DitheredLODTransition`、……）以及一个固定的实例
属性表（`PhysMaterial`、`SubsurfaceProfile`、……）。

**修复。** 检查拼写，或在父材质上设置该属性，其他一切内容都位于那里。

<a id="dsh8250"></a>

## DSH8250

<!-- generated:begin DSH8250 -->
**严重性** 错误

**消息**

```
'{0}' is not a valid value for the instance key '{1}'. {2}
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderInstanceSettings.cpp:374`
<!-- generated:end DSH8250 -->

**原因。** 某个实例键的值不符合其属性；其后的句子说明了期望的
内容。

**修复。** 枚举值不带其前缀（`Translucent`），布尔值写作 `true` / `false`，资源写作
对象路径或 `None`。

<a id="dsh8251"></a>

## DSH8251

<!-- generated:begin DSH8251 -->
**严重性** 错误

**消息**

```
'{0}' cannot be set from an instance file: {1}
```

**引发者** `Source/DreamShaderCompiler/Private/Assets/DreamShaderInstanceSettings.cpp:341`
<!-- generated:end DSH8251 -->

**原因。** 该键命名了实例文件不会以这种方式设置的内容；其后的句子
说明了是哪一种：`Backend`（实例没有此项）、`UsageFlags`（引擎会与
父项的位掩码合并的位掩码）、`BasePropertyOverrides` 结构体整体或其某个 `bOverride...` 标志（一个标志
由其设置值的键产生），或 `...ParameterValues` 数组（参数是 `uniform`
覆盖）。

**修复。** 写出消息所指的单个键（`BlendMode = Translucent`）或 `uniform` 覆盖。

<a id="dsh8252"></a>

## DSH8252

<!-- generated:begin DSH8252 -->
**严重性** 错误

**消息**

```
The override of '{0}' could not be applied: {1}
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1141`
<!-- generated:end DSH8252 -->

**原因。** 某个覆盖无法写入实例；原因见后文。通常某个类纹理覆盖所指定的
资源无法加载或属于错误的类。

**修复。** 修正该覆盖的 `/// @default` 路径。

<a id="dsh8253"></a>

## DSH8253

<!-- generated:begin DSH8253 -->
**Severity** 错误

**Message**

```
Failed to create package '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:281`、`Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:288`
<!-- generated:end DSH8253 -->

**Cause.** 实例的包，或其中的 `UMaterialInstanceConstant`，无法创建：
对于消息所指的路径，`CreatePackage` 或 `NewObject` 失败了。

**Fix.** 检查 `.dsi` 所在的位置：它在源根下的文件夹会成为包路径，
而文件名会成为资产的名称。

<a id="dsh8254"></a>

## DSH8254

<!-- generated:begin DSH8254 -->
**Severity** 错误

**Message**

```
The material parameter collection '{0}' has no parameter called '{1}'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:579`
<!-- generated:end DSH8254 -->

**Cause.** 一个 `CollectionParameter` 节点指定了一个其材质参数集里没有的参数。引擎按 id 存储该参数，
并从中读回名称，所以一个没有 id 的名称会悄无声息地变成 `None`。

**Fix.** 使用参数集资产中已有的参数名（标量或向量），或者将其添加到
参数集中。

<a id="dsh8260"></a>

## DSH8260

<!-- generated:begin DSH8260 -->
**Severity** 错误

**Message**

```
The parent source '{0}' failed to compile, so this instance has no parent to build against. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:1126`、`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:534`、`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:561`、`Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:591`、`Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:620`
<!-- generated:end DSH8260 -->

**Cause.** 一个 `.dsi` 的父项无法使用。它由一个无法编译的 DreamShader 源构建，
所以没有参数列表可用于检查覆写，也没有资产可用于构建；
或者 `Parent` 未解析到资产路径；又或者它什么都不指向：那里不存在资产，
源根下也没有源构建它。

**Fix.** 编译父项的源并修复它所报告的问题，或者修正 `Parent`。

<a id="dsh8261"></a>

## DSH8261

<!-- generated:begin DSH8261 -->
**Severity** 错误

**Message**

```
No material or instance named '{0}' is built by a source under '{1}'; write the parent's asset path, or check the name.
```

**Raised by** `Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:656`
<!-- generated:end DSH8261 -->

**Cause.** `Parent` 是一个裸名称，并且此文件的源根下没有任何源构建该名称的材质或
实例。

**Fix.** 检查该名称，或者写出父项的对象路径（`/Game/...`）。

<a id="dsh8262"></a>

## DSH8262

<!-- generated:begin DSH8262 -->
**Severity** 错误

**Message**

```
'{0}' names more than one product under '{1}' ({2}); write the parent's asset path instead.
```

**Raised by** `Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:670`
<!-- generated:end DSH8262 -->

**Cause.** `Parent` 是一个被该根下多个源构建的裸名称。

**Fix.** 改为写出父项的对象路径。

<a id="dsh8263"></a>

## DSH8263

<!-- generated:begin DSH8263 -->
**Severity** 错误

**Message**

```
'{0}' is its own ancestor: following Parent from it comes back to it ({1}).
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:486`、`Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:602`、`Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:650`
<!-- generated:end DSH8263 -->

**Cause.** 实例是它自己的祖先：`Parent` 通过路径或名称指向实例自身，
或者从它出发跟随 `Parent` 会回到它自身（消息会列出该链）。

**Fix.** 打破循环：将 `Parent` 指向该链应终止于的材质。

<a id="dsh8264"></a>

## DSH8264

<!-- generated:begin DSH8264 -->
**Severity** 信息

**Message**

```
'{0}' was missing or older than its source, so '{1}' was compiled first.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:596`
<!-- generated:end DSH8264 -->

**Cause.** 在编译实例时，父项的资产缺失或早于其源，
所以父项被优先编译。

**Fix.** 无需修复。

<a id="dsh8265"></a>

## DSH8265

<!-- generated:begin DSH8265 -->
**Severity** 错误

**Message**

```
The Parent chain above '{0}' is more than {1} instances deep; a chain that long is almost always a mistake in a Parent key.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:494`
<!-- generated:end DSH8265 -->

**Cause.** 此实例之上的 `Parent` 键链比编译器所能跟随的更深。真实的
实例链只有几层深。

**Fix.** 查找指向错误方向的 `Parent`。

<a id="dsh8270"></a>

## DSH8270

<!-- generated:begin DSH8270 -->
**Severity** 错误

**Message**

```
The function reference '{0}' does not resolve to an asset path. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:259`
<!-- generated:end DSH8270 -->

**Cause.** 一个 `extern` 函数的 `/// @asset`（或 1.x VirtualFunction 的 `Asset`）
未解析到对象路径；原因随附其后（即以文字表述的 DSH8118-DSH8132 之一）。

**Fix.** 修正该引用；内容浏览器中的 *Copy Reference* 会给出一个总能
解析的路径。

<a id="dsh8271"></a>

## DSH8271

<!-- generated:begin DSH8271 -->
**Severity** 错误

**Message**

```
The default texture reference '{0}' of parameter '{1}' does not resolve to an asset path. {2}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:221`
<!-- generated:end DSH8271 -->

**Cause.** 参数的 `/// @default` 纹理未解析到对象路径；原因
随附其后。

**Fix.** 修正该引用。

<a id="dsh8273"></a>

## DSH8273

<!-- generated:begin DSH8273 -->
**Severity** 错误

**Message**

```
'{0}' holds a placeholder other than '`{Name`}', the only one a generate template has.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPackageAssets.cpp:160`
<!-- generated:end DSH8273 -->

**Cause.** 一个 `#pragma generate(...)` 的 `Functions` 模板在 `{Name}` 被替换为函数名后仍保留着一个 `{...}`。`{Name}` 是唯一的占位符。

**Fix.** 精确写出 `{Name}`（它区分大小写），或者在函数上使用 `/// @generate` 逐一写出每个名称。

<a id="dsh8274"></a>

## DSH8274

<!-- generated:begin DSH8274 -->
**Severity** 错误

**Message**

```
'{0}' cannot be generated as the material function '{1}': {2}.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPackageAssets.cpp:172`
<!-- generated:end DSH8274 -->

**Cause.** 一个包头从无法成为材质函数的函数请求材质函数：一个 `export` 函数，它会构建自己的资产；一个 `extern` 原型，它指向一个存在但没有函数体的资产；一个没有函数体的函数；或者一个带数组参数的函数，而任何材质函数引脚都无法接受数组参数。

**Fix.** 从一个有函数体且没有数组参数的辅助函数生成——如有需要，将函数包装进一个这样的函数——或者移除该注解。`#pragma generate` 模板会自动跳过导出和原型；此错误是针对在其中一个上显式使用 `@generate` 而引发的。

<a id="dsh8275"></a>

## DSH8275

<!-- generated:begin DSH8275 -->
**Severity** 错误

**Message**

```
'{0}' is generated a second time here; the line {1} already generates it from another function.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPackageAssets.cpp:201`
<!-- generated:end DSH8275 -->

**Cause.** 一个包头的两个函数请求同一个材质函数：相同的 `@generate` 名称，或者一个将两个函数映射到同一个名称的模板。第一个会被生成；第二个不会。

**Fix.** 给其中一个另取名称，并使用其自己的 `/// @generate`（或在 1.x 中的 `Generate =`）。

<a id="dsh8276"></a>

## DSH8276

<!-- generated:begin DSH8276 -->
**Severity** 警告

**Message**

```
'{0}' is not a key of '#pragma generate', and was ignored; it takes Functions, Library and Desc.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPackageAssets.cpp:112`
<!-- generated:end DSH8276 -->

**Cause.** 一个 `#pragma generate(...)` 指定的键不是 `Functions`、`Library`、`Desc` 和 `Description`。它被忽略。

**Fix.** 将该键拼写为其中之一，或将其移除。

<a id="dsh8277"></a>

## DSH8277

<!-- generated:begin DSH8277 -->
**Severity** 信息

**Message**

```
'{0}' was left alone: '{1}' marks it NoGeneration, and only a compile that names the file builds it.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:1186`
<!-- generated:end DSH8277 -->

**Cause.** 不是问题：未指定源的编译会让 [NoGeneration](../generation/no-generation.md) 资产保持其在磁盘上的原样，即使源自其构建以来已发生变化。其结果行会显示 `Skipped`。

**Fix.** 无需修复。要重建该资产，请按名称编译源：材质内容浏览器中的 *Compile*，或 `compile -Source`。

<a id="dsh8278"></a>

## DSH8278

<!-- generated:begin DSH8278 -->
**Severity** 警告

**Message**

```
'{0}' has never been built: '{1}' marks it NoGeneration, so only a compile that names the file builds it, and none has. Compile '{1}' by name once: Compile in the Material Content Browser, or `compile -Source`.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:1194`
<!-- generated:end DSH8278 -->

**原因。** 一个 [NoGeneration](../generation/no-generation.md) 资产从未被构建：只有命名其源文件的编译才会构建它，而这样的编译尚未发生。没有为它写入任何内容；在它存在之前，任何使用它的东西都会失败（[`DSH8279`](#dsh8279)）。

**修复。** 按名称编译一次源文件：在材质内容浏览器中点击 *Compile*，或 `compile -Source`。对于从不打算自动构建的文件，`#pragma disable_warning(DSH8278)` 可使此情况静默。

<a id="dsh8279"></a>

## DSH8279

<!-- generated:begin DSH8279 -->
**严重性** error

**消息**

```
The parent '{0}' comes from '{1}', which marks it NoGeneration, and it has never been built, so there is nothing to build this instance against. Compile '{1}' by name once: Compile in the Material Content Browser, or `compile -Source`.
```

**引发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:1216`、`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:575`、`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:473`
<!-- generated:end DSH8279 -->

**原因。** 某物需要 [NoGeneration](../generation/no-generation.md) 源所构建的资产，而该资产不存在：同一文件的导出调用它，某个 `.dsi` 将其命名为其 `Parent`，某个 `.dsp` pass 将其命名为其 `Material`，或者某个 `.dss` 读取该管线的缓冲区。NoGeneration 源绝不会替任何人先被编译。

**修复。** 按名称编译一次构建它的源：在材质内容浏览器中点击 *Compile*，或 `compile -Source`。然后它的使用者将基于该编译所写入的内容进行构建。或者去掉 NoGeneration 标记。

<a id="dsh8290"></a>

## DSH8290

<!-- generated:begin DSH8290 -->
**严重性** error

**消息**

```
'{0}' could not be read.
```

**引发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:702`
<!-- generated:end DSH8290 -->

**原因。** 编译器无法打开要求它编译的 `.dss`。路径已先被规范化，因此消息显示的是它实际尝试过的绝对路径。通常原因包括：文件在监视器注意到它与编译开始之间被删除或重命名；某个 `-Source=` 命名了不存在的文件（命令行工具先相对 `DShader/` 解析相对路径，然后相对项目目录解析，当两者都不存在时回退为字面文本）；或文件被另一个进程锁定。

**修复。** 检查消息中的路径。如果看起来正确，请检查文件是否存在且可读；在 Windows 上，一个持有独占锁的文本编辑器就足以导致此问题。

<a id="dsh8291"></a>

## DSH8291

<!-- generated:begin DSH8291 -->
**严重性** error

**消息**

```
'{0}' failed conditional compilation: {1}: {2}
```

**引发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:758`、`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:228`、`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPackageAssets.cpp:315`
<!-- generated:end DSH8291 -->

**原因。** 条件编译预处理器拒绝处理该文件。消息携带预处理器自身的 DSH103x 代码及其消息，因为那些才说明了实际出错的地方——不匹配的 `#if`、`#elif` 位于 `#else` 之后、它无法求值的表达式、对保留 `DS_` 名称的 `#define`。此代码补充了预处理器无法知道的唯一一件事：它是本次编译中的哪个文件，这很重要，因为头文件是单独进行预处理的，其失败会针对包含它的 `.dss` 报告。

**修复。** 阅读内部代码，而不是这一个：`Docs/diagnostics/DSH1xxx.md` 列出了每个代码所强制执行的规则。请记住，在 DreamShader 中 `#define` 是文件局部的——在 `.dsh` 中定义的名字对包含它的 `.dss` 不可见，因此依赖于某个宏的条件必须在测试它的文件中定义该宏，或者该定义应位于项目设置表中。

<a id="dsh8292"></a>

## DSH8292

<!-- generated:begin DSH8292 -->
**严重性** error

**消息**

```
'{0}', included from '{1}', could not be resolved: {2}.
```

**引发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:174`
<!-- generated:end DSH8292 -->

**原因。** 某个 `#include`（或 `import`）命名了一个任何源根目录都不包含的文件。解析规则按顺序如下：存在的绝对路径就是它本身；以 `/` 开头的说明符是根锚定的，会先针对包含文件自身的根目录尝试，然后针对每个其他根目录，`DShader/` 先于 `DShader/Packages/`；其他任何内容都走 1.x 导入规则——根限定符（`Plugin.MoonToon:Shared/Common.dsh`），然后是包含文件自身的目录，然后是它的根目录，然后是它的 Packages 树。不带扩展名的说明符会获得 `.dsh`。

未限定的 include 永远不会离开包含文件的根目录。这是有意为之：两个发布相同相对路径的插件不会相互遮蔽，启用或禁用插件也不会悄然改变另一个根目录的 include 的含义。

**修复。** 检查拼写和扩展名。要到达另一个根目录，请限定它——`#include "Plugin.MoonToon:Shared/Common.dsh"`。要锚定在根目录而不是包含文件旁边，请以 `/` 开始路径。

<a id="dsh8293"></a>

## DSH8293

<!-- generated:begin DSH8293 -->
**严重性** error

**消息**

```
'{0}', included from '{1}', resolved but could not be read.
```

**引发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:209`
<!-- generated:end DSH8293 -->

**原因。** include 解析到了一个真实路径，但打开它失败了。与 DSH8292 不同，文件已被找到，因此这是权限或锁定问题，而不是拼写问题。

**修复。** 检查文件是否可读。插件 `DShader/` 下以只读方式发布的 `.dsh` 仍然是可读的；而在另一个工具中被独占打开的文件则不是。

<a id="dsh8294"></a>

## DSH8294

<!-- generated:begin DSH8294 -->
**严重性** error

**消息**

```
'{0}', included from '{1}', could not be parsed; its own errors are above.
```

**引发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:258`
<!-- generated:end DSH8294 -->

**原因。** 被包含的头文件未能解析。它自身的解析错误已先针对该头文件自己的文件和行号报告；此代码是“因此”那一行，指出哪个 include 失败以及哪个文件将其引入，因此当某个头文件从二十处被包含时，失败仍可归因。

**修复。** 修复该头文件自身的错误——它们正是日志中此条正上方的内容，并带有该头文件的行号。无论多少个文件包含该头文件，每次编译它只报告一次。

<a id="dsh8295"></a>

## DSH8295

<!-- generated:begin DSH8295 -->
**严重性** error

**消息**

```
'{0}' is not a DreamShader header; an include names a '.dsh' (or a '.dss'), not a '{1}' file.
```

**引发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:198`
<!-- generated:end DSH8295 -->

**原因。** 某个 include 解析到了一个不是 DreamShader 头文件的文件。只有 `.dsh`（以及对于有意跨编译单元的 include，`.dss`）可以被包含；`.dsm` 或 `.dsf` 是语法不同的 1.x 源，包含其中之一会被当作 2.0 文本解析并逐行失败。

**修复。** 包含声明所在的 `.dsh`。1.x 的 `.dsm`/`.dsf` 无法与 2.0 源共享；请将共享声明移入 `.dsh`，两个前端都会读取它。

<a id="dsh8296"></a>

## DSH8296

<!-- generated:begin DSH8296 -->
**严重性** error

**消息**

```
'{0}' is not a source the compiler builds on its own; it builds '.dss', '.dsi', '.dsp', '.dsm' and '.dsf' files, and a '.dsh' header only through the source that includes it, unless it is a package's asset header.
```

**引发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:645`
<!-- generated:end DSH8296 -->

**原因。** 管线收到了一个它不会单独构建的文件：某个 `.dsh` 头文件，或其扩展名不属于 `.dss`、`.dsi`、`.dsp`、`.dsm` 和 `.dsf` 的文件。头文件不产生资产，只在包含或导入它的源中作为一部分被编译，因此直接编译头文件无论如何都只会产出空结果。

**修复。** 编译包含该头文件的源。其他所有种类——包括通过旧式前端的 `.dsm` 和 `.dsf`——都由同一管线构建。

<a id="dsh8297"></a>

## DSH8297

<!-- generated:begin DSH8297 -->
**严重性** error

**消息**

```
The builtin expression catalog came back empty, so nothing that names a 'UE.*' node can be bound. Reflection found no UMaterialExpression classes, which normally means the Engine module is not loaded.
```

**引发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:875`
<!-- generated:end DSH8297 -->

**原因。** 内置表达式目录——即取自 UE 反射的每个 `UE.*` 节点、其引脚、其属性以及材质属性表——返回时没有任何条目。在空目录下，任何为内置项命名的事物都无法进行绑定，因此编译会直接停止，而不是将文件中的每个 `UE.*` 都报告为未知名称。

在实践中，这意味着反射根本没有找到 `UMaterialExpression` 子类，这只有在 Engine 模块未加载时才会发生——即在不带编辑器模块的情况下启动的命令行工具，或在模块启动过程中非常早期的调用。

**修复。** 这不是源文件问题。请从编辑器或从 `-run=DreamShader` 运行编译，两者都带有 Engine 模块。如果发生在编辑器内，请运行 `dsc export-catalog` 并查看清单中包含什么——空清单证实了反射这一环节，而完整清单则指向缓存副本与 `InvalidateDreamShaderBuiltinCatalog`。

<a id="dsh8298"></a>

## DSH8298

<!-- generated:begin DSH8298 -->
**严重性** 错误

**消息**

```
Building '{0}' was cancelled; the asset is as it was before this compile.
```

**由以下触发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:316`、`Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:1183`、`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:684`
<!-- generated:end DSH8298 -->

**原因。** 用户在编译进度对话框上按了取消。没有任何内容被写入：emit（输出）是运行的最后一个阶段，并且在每个阶段之前以及每个产物之前都会检查取消。

**修复。** 无需处理。需要结果时重新编译即可。一次耗时长到值得取消的编译，通常在其背后已经开始了着色器编译；那一半会在引擎自己的队列中继续，且不会因此被取消。

<a id="dsh8299"></a>

## DSH8299

<!-- generated:begin DSH8299 -->
**严重性** 错误

**消息**

```
The exported functions {0} call one another in a cycle, so there is no order in which they can be built; an exported function may call another only in one direction.
```

**由以下触发** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:1146`
<!-- generated:end DSH8299 -->

**原因。** 同一个文件中有两个或更多导出函数直接或通过其他函数互相调用。每个导出函数都会成为各自独立的材质函数资产，而对其中一个的调用会绑定到 LIVE（实时）资产——emitter（发射器）从对象上读取被调用方的引脚，而不是从源代码读取——因此必须存在一种顺序，使得每个被调用方都在其调用方之前构建。而循环不存在这样的顺序。

该消息会列出循环中的每个产物。

**修复。** 打破循环。通常这两个函数中有一个不需要导出：`Helper`（没有 `export`）会在其调用点内联，并且完全不参与任何依赖关系，因此将内部的那个改为辅助函数即可移除该边。如果两者确实都必须成为资产，则将共享部分提取到第三个不回调任何一方的函数中。

<a id="dsh8300"></a>

## DSH8300

<!-- generated:begin DSH8300 -->
**严重性** 错误

**消息**

```
'{0}' is a Custom Pass pipeline, which needs Unreal Engine 5.8 or later; this engine has the DreamShaderPass asset types but no runtime to run them, so nothing was built.
```

**由以下触发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:1017`
<!-- generated:end DSH8300 -->

**原因。** 一个 `.dsp` 在早于 Unreal Engine 5.8 的引擎上被编译。DreamShaderPass 模块在那里会仅以其资产类型构建（`DREAMSHADER_WITH_CUSTOM_PASS` 是 `0`）：没有运行时来运行管线，也没有全局着色器持有 HLSL pass，因此 emitter（发射器）在创建或触碰任何内容之前就会停止。前半部分仍会运行——文件会被解析、格式化并绑定，只是跳过需要运行时的检查（DSH7360）——因此 `check` 可以通过，而 `compile` 不能。

**修复。** 在 Unreal Engine 5.8 或更高版本上构建该管线。在较旧的引擎上无需撤销任何内容：没有写入任何管线、渲染目标、槽位或快照。

<a id="dsh8301"></a>

## DSH8301

<!-- generated:begin DSH8301 -->
**严重性** 错误

**消息**

```
Failed to create package '%s'.
```

**由以下触发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:1027`、`Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:1057`、`Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:341`、`Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:346`
<!-- generated:end DSH8301 -->

**原因。** 无法在其 `.dsp` 解析到的路径上创建管线资产。消息 `The pass pipeline for '…' could not be created or reused.` 后面跟着带有其自身代码的原因：当 `CreatePackage` 或 `NewObject` 拒绝了该路径时为 DSH8301 again、`Failed to create package '…'` 或 `Failed to create the pass pipeline '…'`，这种情况很少见；或者当已经有东西位于那里时为 DSH8302 / DSH8303。第三种形式 `'…' does not resolve to a valid asset path` 是目标检查，编译会首先执行该检查，并将其报告为 DSH8200。

**修复。** 根据内部原因进行处理。对于无法创建的包，请检查 `.dsp` 所在的位置：`.dsp` 没有 `/// @name`，因此其在源根目录下的文件夹给出包路径，而其文件名给出资产的名称。

<a id="dsh8302"></a>

## DSH8302

<!-- generated:begin DSH8302 -->
**严重性** 错误

**消息**

```
Asset '%s' already exists as a '%s'; a .dsp builds a DreamPassPipeline. Rename the .dsp or move the existing asset.
```

**由以下触发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:324`
<!-- generated:end DSH8302 -->

**原因。** `.dsp` 要构建其管线的地方已经有另一个类的资产——例如同一文件夹中同名的材质。`.dsp` 会依据其文件来命名其管线。该内容被引用于 DSH8301 的消息中。

**修复。** 重命名 `.dsp`（其文件名即管线的名称），或移动现有的资产。

<a id="dsh8303"></a>

## DSH8303

<!-- generated:begin DSH8303 -->
**严重性** 错误

**消息**

```
Asset '%s' already exists and was not generated by DreamShader. Rename the .dsp or move/delete the existing asset before building it.
```

**由以下触发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:331`
<!-- generated:end DSH8303 -->

**原因。** 一个 `UDreamPassPipeline` 被保存在管线的路径上，但没有 DreamShader 的源戳记：可能是手工制作的，或是从 DreamShader 分离出来的。所有权保护机制从不接管不是它生成的资产。该内容被引用于 DSH8301 的消息中。

**修复。** 重命名 `.dsp`，或移动、删除现有管线，然后重新编译。要先将手工制作的管线设置带入 `.dsp`，请先反编译它（`./dsc.ps1 decompile <asset path>`），并复制你需要的内容。

<a id="dsh8304"></a>

## DSH8304

<!-- generated:begin DSH8304 -->
**严重性** 错误

**消息**

```
'{1}' is not a {0} the Custom Pass runtime knows; the pipeline was not built. This is a compiler gap: the binder should have refused it.
```

**由以下触发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:599`
<!-- generated:end DSH8304 -->

**原因。** 管线的某个值以 DreamShaderPass 运行时没有对应枚举值的拼写方式到达 emitter（发射器）；该消息会指出值的类型（注入点、视图类型、缓冲区格式、pass 类型、网格 pass 的模式、深度、剔除、混合、用途或 Nanite 策略，参数的来源或常量类型）以及该拼写。binder（绑定器）会先检查其中每一项，因此两者不一致，而这不是文件的问题。没有构建任何内容。

**修复。** 请使用 `.dsp` 报告它。在修复之前，请按照 [Custom Pass pipelines](../language-v2/passes.md) 所拼写的方式来写该值。

<a id="dsh8305"></a>

## DSH8305

<!-- generated:begin DSH8305 -->
**严重性** 错误

**消息**

```
The material '{0}' of pass '{1}' does not load; compile the source that builds it, or correct the Material key.
```

**由以下触发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:778`
<!-- generated:end DSH8305 -->

**原因。** 在组装管线时，全屏 pass 或网格 pass 所命名的材质未能从其 `Material` 解析到的对象路径加载。构建之前的检查发现了它，而且一次编译会先构建一个缺失或比其源更旧的材质（DSH8332），因此该资产是在这中间不见了：被删除、被重命名而没有重定向器，或者是一个不再能加载的包。

**修复。** 编译构建该材质的源，或更正 `Material` 键（裸名称，或材质的对象路径），然后再次编译 `.dsp`。

<a id="dsh8306"></a>

## DSH8306

<!-- generated:begin DSH8306 -->
**严重性** 错误

**消息**

```
The default of '{0}' could not be applied: {1}.
```

**由以下触发** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:660`
<!-- generated:end DSH8306 -->

**原因。** `Texture2D` uniform 的 `/// @default` 无法被写入管线：该引用没有解析为资产路径（原因紧随其后，是 DSH8118-DSH8132 之一的文字描述），或者没有纹理从它解析到的路径加载。binder（绑定器）会按所写的内容取用默认值，因此这是它第一次被加载。

**修复。** 更正路径——内容浏览器中的*复制引用*会给出一个始终能解析的路径——或者
为没有纹理的参数写入 `/// @default None`（或者不写 `@default`）。

<a id="dsh8307"></a>

## DSH8307

<!-- generated:begin DSH8307 -->
**严重性** error

**消息**

```
Pass '{0}' selects the layer '{1}', which is not one of the project's pass layers (Project Settings > DreamPlugin > DreamShader Custom Pass > Layer Names).
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:854`
<!-- generated:end DSH8307 -->

**原因。** 某个网格 Pass 的 `Filter = Layer(...)` 命名了一个在管线构建时不带位（bit）的层。
绑定器接受项目层表列出（未列出的名称是 DSH4412）的任何名称，但表中只有
前 32 个条目有位，因此在此处列出第 32 个之后的名称会被拒绝。未构建任何内容。

**修复。** 将该层移动到“项目设置 ▸ DreamPlugin ▸ DreamShader Custom
Pass ▸ Layer Names”的前 32 个条目中，或选择一个已在前 32 个中的层。移动层会重新指向所有名称中包含该层的已编译管线，直到该管线再次编译。

<a id="dsh8308"></a>

## DSH8308

<!-- generated:begin DSH8308 -->
**严重性** error

**消息**

```
Buffer '{0}' is {1} and cannot be exported: materials, Blueprints and UMG read an exported buffer as a float texture, which an integer or a depth buffer cannot be. Export a float or normalized buffer, or drop Export.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:705`
<!-- generated:end DSH8308 -->

**原因。** 一个声明为 `Export = true` 的缓冲区具有整数或深度格式（`R32U`、`RG32U`、`Depth32`）。
导出的缓冲区会成为渲染目标，材质、蓝图和 UMG 将其作为浮点纹理采样，
而这两者都不能是整数或深度格式。绑定器会先拒绝相同的声明（DSH7355）；这是发射器在将格式映射到渲染目标格式时的自身
检查。

**修复。** 导出浮点或归一化缓冲区，或去掉 `Export`。id 在不超过
16777216 时可精确放入 `R32F`。

<a id="dsh8309"></a>

## DSH8309

<!-- generated:begin DSH8309 -->
**严重性** error

**消息**

```
The render target of the exported buffer '{0}' could not be created or reused. {1}
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:1162`
<!-- generated:end DSH8309 -->

**原因。** 导出的缓冲区的渲染目标——`<Pipeline>_<Buffer>`，位于管线的文件夹中——
无法创建或接管；其后的句子是原因，带有其代码（DSH8313 或
DSH8314）。渲染目标在写入任何内容之前就已找到或创建，因此管线、槽位
注册表和快照保持原样。

**修复。** 依据该原因采取相应措施。

<a id="dsh8310"></a>

## DSH8310

<!-- generated:begin DSH8310 -->
**严重性** warning

**消息**

```
'{0}' was left in place although its buffer is no longer exported: {1}. Delete it by hand once nothing reads it.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:577`
<!-- generated:end DSH8310 -->

**原因。** 管线过去导出而现已不再导出的缓冲区——其 `Export` 被移除、缓冲区
被重命名或删除——因此在管线保存后其渲染目标本应被删除。它
被保留了：其他东西仍在引用它（消息列出了包，通常是某个材质，
其 `UE.DreamPassBuffer` 读取该缓冲区），或者，在未列出任何引用者时，删除操作在此
过程中被拒绝。管线本身已构建并保存。

**修复。** 将读取该缓冲区的对象指向管线仍然导出的某个缓冲区并重新构建，然后在
内容浏览器中删除该渲染目标。如果导出是被误移除的，请把 `Export = true` 放回：
下一次编译会再次接管同一个渲染目标，其读取者保持有效。

<a id="dsh8311"></a>

## DSH8311

<!-- generated:begin DSH8311 -->
**严重性** error

**消息**

```
'{0}' was built but could not be saved with its render targets; its slots are already in the registry. Make the packages writable and compile the source again to retry saving. {1}
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:1271`
<!-- generated:end DSH8311 -->

**原因。** 管线已构建且其渲染目标已配置，但保存包失败——例如文件因未签出而为
只读，或路径不再存在；资产层的原因随后给出。保存之前的一切仍然有效：槽位注册表和快照已
写入，在编辑器中槽位着色器已重新编译，内存中的管线已在运行
新版本。

**修复。** 使包可写并再次编译 `.dsp`。保存失败会使
内存中的源哈希跳过失效，因此普通编译会重试该管线及其未保存的渲染目标。
在部分保存并重启编辑器后，保存的配置与管线不匹配的渲染目标也会阻止
跳过。不需要 `-Force`。

<a id="dsh8312"></a>

## DSH8312

<!-- generated:begin DSH8312 -->
**严重性** info

**消息**

```
'{0}' was deleted: its buffer is no longer exported.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:565`
<!-- generated:end DSH8312 -->

**原因。** 信息性。管线过去导出而现已不再导出的缓冲区，除了管线之外没有任何东西引用其渲染目标，因此该渲染目标在管线不带它保存后被
删除。

**修复。** 无需操作。如果你提交生成的资产，请随管线一起提交该删除。

<a id="dsh8313"></a>

## DSH8313

<!-- generated:begin DSH8313 -->
**严重性** error

**消息**

```
Asset '%s' already exists as a '%s'; an exported buffer needs a TextureRenderTarget2D there. Move the existing asset or rename the buffer.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:372`、`Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:379`
<!-- generated:end DSH8313 -->

**原因。** 在导出的缓冲区的渲染目标所在处（`<Pipeline>_<Buffer>`，紧邻管线）存在
非本管线渲染目标的东西：另一类的资产，或一个没有 DreamShader 源标记的
`TextureRenderTarget2D`——手动创建的，永远不会被接管。
在 DSH8309 的消息内引用。

**修复。** 移动或重命名现有资产，或重命名该缓冲区。

<a id="dsh8314"></a>

## DSH8314

<!-- generated:begin DSH8314 -->
**严重性** error

**消息**

```
Failed to create package '%s'.
```

**引发者** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:389`、`Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:394`
<!-- generated:end DSH8314 -->

**原因。** 导出的缓冲区的渲染目标所在的包，或其中的 `TextureRenderTarget2D`，
无法创建：`CreatePackage` 或 `NewObject` 对于消息所命名的路径失败。罕见；在 DSH8309 的消息内
引用。

**修复。** 检查该路径——管线的文件夹和 `<Pipeline>_<Buffer>`——是否存在引擎在包名中
拒绝的内容。

<a id="dsh8315"></a>

## DSH8315

<!-- generated:begin DSH8315 -->
**严重性** error

**消息**

```
The Custom Pass slot registry cannot be read: {0}. Nothing was written, because writing over it would lose every slot it records; fix the file, or run 'dsc pass-registry -Rebuild'.
```

**引发者** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:897`
<!-- generated:end DSH8315 -->

**原因。** 每次 `.dsp` 编译都会读取项目的槽位注册表 `<DShader>/.dreampass/Registry.json`——
以给其 HLSL pass 分配槽位，并释放其丢弃的 pass 的槽位——而该文件存在但
无法使用：无法打开、不是有效的 JSON（其中留下的合并冲突是常见
原因），或某个条目没有槽位编号、管线或 pass。覆盖它会丢失其记录的每一个槽位，
因此未写入任何内容。

**修复。** 采用冲突的任一方并再次编译 `.dsp` 文件——绝不要手动合并 JSON——
或运行 `./dsc.ps1 pass-registry -Rebuild`，它会将该文件移到一旁并再次为每个 HLSL pass 分配
槽位（[`pass-registry`](../../tools/commandlet.md#pass-registry)）。

<a id="dsh8316"></a>

## DSH8316

<!-- generated:begin DSH8316 -->
**严重性** error

**消息**

```
Pass '{0}' needs a {1} slot and all {2} are taken. Merge passes, run 'dsc pass-registry -Gc' to free the slots of pipelines whose source is gone, or raise {3} in the project's Target.cs.
```

**引发者** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:980`
<!-- generated:end DSH8316 -->

**原因。** 新的 HLSL pass 需要一个槽位——`compute` pass 需要计算表中的一个，带 `fullscreen` 且
含 `Shader =` 的 pass 需要像素表中的一个——而该表的每个槽位在项目的注册表中都已被占用
（默认 32 个计算槽位和 16 个像素槽位）。管线丢弃的 pass 会在该管线再次编译时释放其槽位；被删除或重命名的 `.dsp` 的槽位在被回收之前
仍保持占用。

**修复。** `./dsc.ps1 pass-registry` 列出槽位并统计垃圾数量；`./dsc.ps1 pass-registry -Gc`
释放它。否则合并 pass，或者在项目的 Target.cs 文件（编辑器和游戏
皆是）中用 `GlobalDefinitions` 提高 `DREAMSHADER_PASS_COMPUTE_SLOTS` /
`DREAMSHADER_PASS_PIXEL_SLOTS`，并重新构建：每个槽位都会为每个平台编译，无论是否使用。

<a id="dsh8317"></a>

## DSH8317

<!-- generated:begin DSH8317 -->
**严重性** error

**消息**

```
Pass '{0}' cannot be mapped onto its HLSL slot: {1}
```

**引发者** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1060`
<!-- generated:end DSH8317 -->

**原因。** 该 pass 的绑定无法写入其槽位的 section —— `#define`将它们映射到槽位的固定参数上；原因如下。一个不是标识符或以 `DP_` 开头的名称；一个被定义两次的名称（`read` 同时也定义了 `<Name>Size` 和 `<Name>UVRect`，`write` `<Name>Size`，以及 `Entry` —— 而 `View` 在 `BeginView` —— 也是槽位中的名称）；超过 8 次读取或 4 次写入；不适合槽位 16 个 `float4` 的参数，或其中包含纹理。绑定器会先拒绝上述每一项（DSH7319、DSH7347），因此两者不一致。

**修复。** 按原因所述重命名或减少绑定，并用 `.dsp` 报告：绑定器本应拒绝该 pass。

<a id="dsh8318"></a>

## DSH8318

<!-- generated:begin DSH8318 -->
**严重性** 错误

**消息**

```
[{0}] pass '{1}' runs at BeginView, where the view uniform buffer does not exist yet, and its slot uses 'View' all the same: a function of the file's 'hlsl' block that the pass calls reads it. Give that function what it needs as a parameter, or move the pass to a later injection point.
```

**引发者** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1310`
<!-- generated:end DSH8318 -->

**原因。** 一个位于 `BeginView` 的 pass，其 HLSL 在 `.dsp` 中编译通过，并且其槽位使用了视图 uniform buffer，而该 buffer 在 `BeginView` 时还不存在。该 pass 自身的代码是受保护的 —— 那里的 `View` 会在其所在行产生编译错误 —— 因此该使用来自 pass 所调用的文件 `hlsl` 块中的某个函数。

**修复。** 让该函数以参数形式接收它从视图所需的内容，并传入 pass 在 `BeginView` 时拥有的值（时间则用 `DP_Time`），或将 pass 移到更晚的注入点。

<a id="dsh8319"></a>

## DSH8319

<!-- generated:begin DSH8319 -->
**严重性** 错误

**消息**

```
The HLSL that pass '{0}' has in its '.dsp' could not be put together for its slot (its entry is not in the file's 'hlsl' block); there is nothing to snapshot.
```

**引发者** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1005`、`Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1014`
<!-- generated:end DSH8319 -->

**原因。** pass 在 `Shader =` 中命名的 `.usf` 在构建其快照时无法被读取。绑定器找到了该文件（缺失的文件是 DSH4408），因此它在这期间丢失或变得不可读：被删除或移动、被另一个程序锁定，或本进程无法读取。对于 HLSL 在 `.dsp` 中的 pass，其快照的根无法由 `.dsp` 的文本拼装：其条目不在该文件的 `hlsl` 块中，绑定器会先报告这一点（DSH4410）。

**修复。** 检查文件是否在引用所指的位置 —— 相对于 `.dsp` 的文件夹，或一个虚拟路径 —— 且可读，然后再次编译 `.dsp`。

<a id="dsh8320"></a>

## DSH8320

<!-- generated:begin DSH8320 -->
**严重性** 错误

**消息**

```
'{0}' is included by a relative path and names no file, so the snapshot of pass '{1}' cannot be built.
```

**引发者** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1040`
<!-- generated:end DSH8320 -->

**原因。** 该 pass 的 `.usf`，或它包含的某个文件，有一个 `#include "<relative path>"`，其命名的文件相对于存放它的文件不存在。槽位的快照会复制通过相对路径包含的每个文件，因此每个文件都必须存在。该扫描是文本式的，不会求值 `#if`：位于从未被编译的分支中的 include 也会被计入。在 include 所在行报告。

**修复。** 更正路径、创建该文件，或移除该 include。`//` 注释会将其排除在扫描之外；其周围的 `#if 0` 则不会。

<a id="dsh8321"></a>

## DSH8321

<!-- generated:begin DSH8321 -->
**严重性** 警告

**消息**

```
'{0}' is included by a virtual path outside /Engine/, /Plugin/ and /ThirdParty/, so the snapshot of pass '{1}' keeps including the live file: an edit of it later reaches the global shaders without a pre-check. Include it by a relative path to have it copied into the snapshot.
```

**引发者** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1050`
<!-- generated:end DSH8321 -->

**原因。** 该 pass 的 `.usf`，或它包含的某个文件，通过 `/Engine/`、`/Plugin/` 和 `/ThirdParty/` 之外的虚拟路径包含了一个文件 —— 例如 `/Project/...` 这样的项目着色器目录。快照只复制通过相对路径包含的内容，因此该 include 仍指向活动文件。其文本是槽位哈希的一部分，而管线的下一次编译会预先检查对它的编辑；在此之前的编辑器启动或 cook 会在那时的文件状态下编译全局着色器，未经检查 —— 而全局着色器编译失败是致命的。

**修复。** 通过相对于包含它的文件的路径来包含该文件，使其被复制到快照中。将虚拟路径保留给 `/Engine/`、`/Plugin/` 和 `/ThirdParty/` 下的着色器代码。

<a id="dsh8322"></a>

## DSH8322

<!-- generated:begin DSH8322 -->
**严重性** 错误

**消息**

```
[{0}] pass '{1}' does not compile in its HLSL slot: {2}
```

**引发者** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1338`、`Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1346`、`Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1372`
<!-- generated:end DSH8322 -->

**原因。** 预检查在槽位中针对方括号内的着色器格式编译了该 pass，着色器编译器拒绝了它；消息的其余部分是编译器自己的内容。`.usf` 中或其快照的某个文件中的错误，会在该文件的行列处报告；写在 `.dsp` 中的 HLSL 中的错误，则在其 `.dsp` 行处报告。文件 `hlsl` 块中共享代码的错误会以相同方式使每个调用它的 pass 失败，并且只报告一次，并列出失败的 pass。没有入口到达的代码不会被编译：引擎在着色器编译器读取槽位之前就将其从槽位中丢弃（`r.Shaders.RemoveDeadCode`），因此尚无 pass 调用的辅助函数不会被检查，无论是在 `.dsp` 中还是在 `.usf` 中。其他地方的一个 —— 槽位的 section、槽位着色器、引擎头文件（此时消息会保留其路径）—— 会在该 pass 处报告，并附注绑定名称可能冲突：每个绑定都是在包含该文件之前创建的 `#define`。没有任何内容被写入：资产、注册表和快照都保持原样，该 pass 的先前版本继续运行。

已知存在一种误拒绝：某个此前没有任何槽位着色器引用的 uniform buffer，会在真实编译中被声明，但在预检查中不会，预检查随后将其报告为未声明。

**修复。** 修复错误指向的行并保存；编辑器会重新编译使用该文件的管线，`./dsc.ps1 check <file>.dsp -Shaders` 会再次预检查每个 HLSL pass。对于该 pass 处的错误，请重命名冲突的绑定（`InMask`，而不是 `Texture`）。报告 uniform buffer 的情况：这是预检查的限制，而非文件中的错误。

<a id="dsh8323"></a>

## DSH8323

<!-- generated:begin DSH8323 -->
**严重性** 错误

**消息**

```
The {0} slot shader cannot be pre-checked: the global shader type {1} is not registered, or its source no longer includes '{2}'. The DreamShaderPass module is out of step with this compiler; nothing was written.
```

**引发者** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1263`、`Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1382`
<!-- generated:end DSH8323 -->

**原因。** 预检查需要 DreamShaderPass 模块所注册的槽位着色器 —— 即全局着色器类型 `FDreamPassCS` / `FDreamPassPS`，其 `DreamPassCompute.usf` / `DreamPassPixel.usf` 包含注册表 —— 而该类型未在此进程中注册，或其源无法读取，或不再包含注册表文件。DreamShaderPass 模块未加载，或它与编译器来自插件的不同构建。没有写入任何内容。在 Unreal Engine 5.8 以下，同一段代码会提示 HLSL 槽位需要 5.8；编译和 `check -Shaders` 会在此之前停在 DSH8300。

**修复。** 完整地重新构建或重新安装插件，使 DreamShaderPass 模块、其 `Shaders` 文件夹和编译器保持一致，然后重启编辑器。

<a id="dsh8324"></a>

## DSH8324

<!-- generated:begin DSH8324 -->
**严重性** 警告

**消息**

```
The project targets the shader format {0}, which this machine has no shader compiler for, so the HLSL slots were not pre-checked for it; a cook for that platform compiles them unchecked.
```

**引发者** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1201`、`Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1223`
<!-- generated:end DSH8324 -->

**原因。** 一个预检查会为项目渲染和烘焙所用的每种格式编译发生变更的槽位——包括活动功能级别的格式，以及活动目标平台所针对的每种格式——而本机没有针对此格式的着色器编译器（其平台的 SDK 或着色器格式未安装）。其他格式的槽位已被检查；针对该平台的烘焙会在未检查的情况下编译它们，而一个在那里编译失败的全局着色器会导致烘焙失败。

`check -Shaders -Platform <format>` 也会出于本机没有编译器的、被请求的格式而引发此问题（`The shader format <format> was asked for, ...`）。这些槽位不会针对该格式被检查，并且当没有请求其他格式时，它们根本不会被检查：该警告防止一个从未运行的检查被读作已通过的检查。

**修复。** 在装有编译器的机器上进行预检查——`./dsc.ps1 check <file>.dsp -Shaders -Platform
<format>` — or install it here. `./dsc.ps1 pass-registry` 会按槽位列出其快照通过的格式。

<a id="dsh8325"></a>

## DSH8325

<!-- generated:begin DSH8325 -->
**严重性** 错误

**消息**

```
There is no shader format to pre-check the HLSL slots with: no active feature level and no target platform has a shader compiler on this machine. Nothing was written, because a slot that never compiled must not reach the global shaders.
```

**引发者** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1207`
<!-- generated:end DSH8325 -->

**原因。** 某个槽位必须被预检查，却没有可用来编译它的着色器格式：该进程不渲染任何内容——`-nullrhi`，`dsc.ps1` 会将其传递给除 `check -Shaders` 之外的每个动词——因此没有任何功能级别会提供一种格式，并且没有任何活动目标平台具有本机可编译的格式。一个从未编译过的槽位绝不能到达全局着色器，因此没有写入任何内容。

**修复。** 在编辑器中编译 `.dsp`，或使用 `./dsc.ps1 check <file>.dsp -Shaders` 编译，它无需 `-nullrhi` 即可运行，并像 `compile` 那样构建管线。

<a id="dsh8326"></a>

## DSH8326

<!-- generated:begin DSH8326 -->
**严重性** 错误

**消息**

```
'{0}' cannot be written or deleted, so nothing of the slot registry was changed: no snapshot written, no slot deleted, no registry file rewritten. The registry and its snapshots are committed files; a file that is read-only because it is not checked out is the usual reason. Check out the whole .dreampass folder and try again.
```

**引发者** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1454`、`Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1473`、`Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1652`
<!-- generated:end DSH8326 -->

**原因。** 槽位注册表的一个文件无法被写入或删除——`Slots/`、
`Registry.json`、`RegistryCompute.ush` 或 `RegistryPixel.ush` 下的快照文件——无论是被 `.dsp` 编译还是被
`pass-registry -Gc` / `-Rebuild` 操作。它们是已提交的文件；某个文件因为未被签出而成为只读，是常见原因。一次编译、`-Gc` 以及 `-Rebuild` 的每个步骤，都会在触碰任何文件之前检查它们将要写入或删除的每个文件，因此上面的消息意味着没有任何内容被更改：没有写入快照、没有删除槽位、没有重写注册表文件。较短的消息 `'<file>' could not be written. ...` 是在该检查通过之后写入失败——文件在此期间被锁定或更改——并且可能使该步骤半途而废：一次编译会在注册表文件之前写入其变更的快照，而 `-Gc` 会在注册表文件之前删除其释放的槽位的快照。

**修复。** 使整个 `.dreampass/` 文件夹可写（将其签出）并重复该步骤：再次编译
`.dsp`，或再次运行 `pass-registry` 动词。出现较短消息后，请在编辑器重启或烘焙运行之前执行此操作。当那次写入是 `RegistryCompute.ush` 或 `RegistryPixel.ush` 时，在写入 `Registry.json` 之后，再次编译 `.dsp` 也能修复它：一次编译会用 `Registry.json` 比较两个注册表文件，并重写不同的那个，无论它还更改了别的什么。

<a id="dsh8327"></a>

## DSH8327

<!-- generated:begin DSH8327 -->
**严重性** 信息

**消息**

```
{0} slot {1} of pass '{2}' was freed: the pipeline no longer runs that pass in HLSL there.
```

**引发者** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1530`
<!-- generated:end DSH8327 -->

**原因。** 信息性。一次 `.dsp` 编译释放了其管线持有的某个槽位：该通道被移除或重命名，被移到另一张表（一个 `compute` 通道变成了 `fullscreen` 通道，或反之），或不再运行 HLSL（它现在绘制一个 `Material`）。它的条目及其章节已消失，其快照也已消失，除非同一次编译的某个通道再次取得了该槽位。

**修复。** 无需操作。将变更后的 `.dreampass/` 文件夹与 `.dsp` 一起提交。

<a id="dsh8330"></a>

## DSH8330

<!-- generated:begin DSH8330 -->
**严重性** 错误

**消息**

```
'{0}' names more than one material under this source root ({1}); write the material's asset path instead.
```

**引发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:591`
<!-- generated:end DSH8330 -->

**原因。** 某个通道的 `Material = "<Name>"` 是一个裸名称，而 `.dsp` 的源根目录下有多个源构建了该名称的材质或材质实例——一个 `.dss` 和一个 `.dsi`，或不同文件夹中的两个 `.dss`；该消息会列出它们。

**修复。** 改为写入该材质的对象路径（`/Game/FX/PP_Outline`）；内容浏览器中的 *复制引用* 即可给出一个。

<a id="dsh8331"></a>

## DSH8331

<!-- generated:begin DSH8331 -->
**严重性** 错误

**消息**

```
The material '{0}' comes from '{1}', which failed to compile, so this pipeline has no material to check its pass against. {2}
```

**引发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:504`、`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:651`
<!-- generated:end DSH8331 -->

**原因。** 某个通道所指定的材质由源根目录下的一个 `.dss`（或一个 `.dsi`）构建，而该源无法编译。两条消息，每种运行各一条：

- 一次构建资产的运行发现该材质的资产缺失或旧于其源，因此先编译了该源（DSH8332），而该编译失败了（`... which failed to compile ...`）；
- 一次不构建任何内容的运行——即 `check`——没有找到材质资产，于是从其源读取该材质的信息，而该源无法编译（`... which does not compile ...`）。消息如此表述，而不是表述为无人构建的材质（DSH4406）。

管线没有可用于对照检查该通道的材质，因而未构建；消息以该源的第一个错误结尾。当该错误为 DSH5325 时，该材质读取了此管线导出的缓冲区：二者互相需要（DSH8333）。

**修复。** 单独编译该材质的源（`./dsc.ps1 compile <file>.dss`）并修复它报告的问题，然后编译 `.dsp`。对于 DSH5325，按 DSH8333 所述更改该读取。

<a id="dsh8332"></a>

## DSH8332

<!-- generated:begin DSH8332 -->
**严重性** 信息

**消息**

```
'{0}' was missing or older than its source, so '{1}' was compiled first.
```

**引发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:483`
<!-- generated:end DSH8332 -->

**原因。** 信息性。某个通道所指定的材质缺失或旧于构建它的源，因此该源在管线之前被编译——这是 `.dsi` 应用于其父项的规则（DSH8264）。只有构建资产的运行才会这样做；`check` 则改为从其源读取该材质的信息。

**修复。** 无需修复。当该编译失败时，会随之出现 DSH8331。

<a id="dsh8333"></a>

## DSH8333

<!-- generated:begin DSH8333 -->
**严重性** 错误

**消息**

```
'{0}' is built by '{1}', which reads this pipeline's exported buffer through UE.DreamPassBuffer (directly, or through a pipeline that needs this one), and a pipeline and its pass material that need each other can be built in no order. Inside its own pipeline a pass binds the buffer with 'read' -- a fullscreen material reads it as a UserSceneTexture input -- rather than the exported copy; a mesh pass's material cannot read its own pipeline's buffers.
```

**引发者** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:614`
<!-- generated:end DSH8333 -->

**原因。** 此管线的一个通道绘制了一个通过 `UE.DreamPassBuffer` 读取该管线导出缓冲区的材质——无论是直接读取，还是通过另一个需要此管线的管线读取。该管线没有其材质就无法构建，而该材质没有该管线的资产就无法构建，因此二者无法以任何顺序构建，永远无法引导；`.dsp` 无法编译。当此 `.dsp` 被编译，或在该材质的编译过程中被读取时，会引发此问题，后者会先构建缺失或过期的管线（DSH5320）；随后该材质会报告该管线失败（DSH5319）。从管线一侧来看，同一个循环是该材质的 DSH5325，管线将其报告为 DSH8331。

**修复。** 在其自身管线内，读取缓冲区本身而非其导出副本：在绘制材质的 pass 中用
`read` 绑定它，并在材质中将其采样为 `UE.UserSceneTexture`
该名称的输入，以替代 `UE.DreamPassBuffer` 调用 —— pass 中用 `read Blurred;`，
材质中用 `UE.UserSceneTexture(UserSceneTexture = "Blurred", Coordinates = UV)`。网格 pass 的
材质完全无法读取其自身管线的缓冲区：通常的循环是一个网格 pass，其覆盖
材质采样同一管线的导出缓冲区，而在管线的全屏 pass 中采样它，
或在管线不绘制的材质中采样，可以避免此问题。

<a id="dsh8334"></a>

## DSH8334

<!-- generated:begin DSH8334 -->
**严重性** 错误

**消息**

```
The material '{0}' does not resolve to an asset path. {1}
```

**引发自** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:584`、`Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:631`
<!-- generated:end DSH8334 -->

**原因。** pass 的 `Material` 无法按所写内容使用。要么它被写为路径（`/…`、
`Path(...)` 或 `Class'…'`），而该路径无法解析为资源路径 —— 原因随后给出，并附带其代码 ——
要么它解析到的资源不是材质或材质实例；此时消息会指出其
类。

**修复。** 写入材质的对象路径，或写入同一源根下 `.dss` 所构建材质的裸名称。内容浏览器中的 *Copy Reference* 给出的路径始终可以解析。

<a id="dsh8335"></a>

## DSH8335

<!-- generated:begin DSH8335 -->
**严重性** 错误

**消息**

```
The Custom Pass slot registry cannot be read: {0}. Nothing was changed; 'dsc pass-registry -Rebuild' replaces a registry that does not parse.
```

**引发自** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:355`、`Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:448`
<!-- generated:end DSH8335 -->

**原因。** `pass-registry -Gc`，或 `-Rebuild` 的最后两步（收集垃圾、重写
注册表文件），无法读取 `Registry.json`：它无法被打开，它不是有效的 JSON —— 其中遗留的合并
冲突是常见原因 —— 或者某个条目没有槽位编号、管线或 pass。未更改任何内容。`-Rebuild` 在编译任何内容之前会替换不可读的文件，因此从 `-Rebuild` 起，这
意味着在其运行期间有另一个进程更改或锁定了该文件。

**修复。** 运行 `./dsc.ps1 pass-registry -Rebuild`。如果它来自 `-Rebuild`，请关闭其他持有
或写入该文件的对象 —— 例如正在编译 `.dsp` 的编辑器 —— 然后再次运行。

<a id="dsh8336"></a>

## DSH8336

<!-- generated:begin DSH8336 -->
**严重性** 错误

**消息**

```
The Custom Pass slot registry cannot be read ({0}) and could not be moved aside to '{1}', so nothing was reset. Another process holding one of the two files open is the usual reason; a read-only one is reported before this.
```

**引发自** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:425`
<!-- generated:end DSH8336 -->

**原因。** `pass-registry -Rebuild` 发现 `Registry.json` 不可读（消息说明了原因），并且无法
将其移到 `Registry.json.unreadable`，因此它没有重置任何内容并停止了。只读的 `Registry.json`
或 `Registry.json.unreadable` 会在移动之前报告为 [`DSH8326`](#dsh8326)；剩下的是一种
因其他原因而失败的移动，最常见的是另一个进程打开了这两个文件之一。

**修复。** 关闭持有该文件的对象 —— 编辑器、差异工具、正在进行的同步 —— 然后再次运行
`./dsc.ps1 pass-registry -Rebuild`。

<a id="dsh8337"></a>

## DSH8337

<!-- generated:begin DSH8337 -->
**严重性** 警告

**消息**

```
{0} slot {1} ({2}, pass '{3}') names snapshot files that are not on disk, so it is now reserved and compiles to the empty stub: a registry that includes a missing file fails the global shader compile. Compile '{4}' to give the pass its snapshot back, and commit the Slots folder with the registry.
```

**引发自** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:462`
<!-- generated:end DSH8337 -->

**原因。** 在 `pass-registry -Rebuild` 结束时，`Registry.json` 仍记录此槽位的快照，
且其文件不在 `Slots/` 下：拥有该槽位的 `.dsp` 在此次重建中未编译
（DSH9204），因此没有任何内容写入它们。包含缺失文件的注册表文件会导致全局着色器
编译失败，因此该槽位被设为保留槽位 —— 它编译为空存根，其 pass 不执行任何操作。

**修复。** 修复消息所指出的 `.dsp` 并编译它：这会写入快照并让该槽位
恢复其章节。将 `Slots` 文件夹与注册表一起提交。

<a id="dsh8338"></a>

## DSH8338

<!-- generated:begin DSH8338 -->
**严重性** 错误

**消息**

```
HLSL slots need Unreal Engine 5.8 or later; this engine has no Custom Pass runtime to pre-check them for.
```

**引发自** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:502`、`Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:514`、`Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:533`
<!-- generated:end DSH8338 -->

**原因。** 对 `.dsp` 运行 `check -Shaders` 时，无法进行到预检查其 HLSL pass 的地步；消息
说明了三者中哪一项阻止了它。引擎早于 Unreal Engine 5.8，没有可用于检查的槽位 ——
不过 `check -Shaders` 首先运行的构建在那里会停在 DSH8300。或者该文件在构建后
为检查而再次读取时，未能到达其管线，且未说明原因。或者管线的资源路径无法解析（原因在括号中）。

**修复。** 对于第一种情况，请在 Unreal Engine 5.8 或更高版本上检查。另外两种情况需要该文件在通过第一次读取后
未通过第二次读取：再次运行检查，如果问题
重复出现，请连同文件一起报告。

<a id="dsh8339"></a>

## DSH8339

<!-- generated:begin DSH8339 -->
**严重性** 信息

**消息**

```
'{0}' has no HLSL pass, so there is no slot to pre-check; its materials are checked by the sources that build them.
```

**引发自** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:559`
<!-- generated:end DSH8339 -->

**原因。** 信息性。`check -Shaders` 在无 HLSL pass 的 `.dsp` 上运行 —— 没有 `compute` pass，也没有
带 `Shader =` 的 `fullscreen` pass —— 因此没有可预检查的槽位。其 pass 绘制材质，
这些材质的着色器属于构建它们的源。

**修复。** 无需操作。要检查这些着色器，请对材质的
源运行 `./dsc.ps1 check <file>.dss -Shaders`。
