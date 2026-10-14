---
title: "资产路径"
description: "资产路径"
---

<a id="asset-paths"></a>

# 资产路径

> [DreamShader](../index.md) » [生成流程](index.md) » **资产路径**

把块的 `Name` 和 `Root` 头部属性变成 Unreal package 路径、object 路径和磁盘上文件的规则。

| | |
| :-- | :-- |
| 适用于 | `Shader`、`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend` —— 同一个解析器，四者完全一致 |
| 种类 | 头部属性 |
| 起始版本 | `Root="Plugin.X"` —— `1.2.0`；插件源码根的默认值 —— `1.6.0` |

<a id="synopsis"></a>

## 语法概览

```c
Shader        (Name = "<name-path>" [, Root = "<root>"]) { … }
ShaderFunction(Name = "<name-path>" [, Root = "<root>"]) { … }

<name-path> := [<folder> /] … <leaf>
<root>      := "" | Game | Plugin.<PluginName> | Plugins.<PluginName>
             | Plugin / <PluginName> | Plugins / <PluginName>
             | / <MountRoot> [/ <folder>] …
             | <folder> [/ <folder>] …
```

`Name` 是 **必填** 的：没有或为空是
[`DSH2242`](../diagnostics/DSH2xxx.md#dsh2242)。`Root` 可选，默认是空字符串。
属性键按不区分大小写匹配，所以 `name=` 和 `ROOT=` 都能解析。

*（自 2.0.0 起）* 1.x 的块完全保留这个目标（legacy 规则 L10）：它的 `Name=` 从不像 `.dss` 产物的名字那样被放到
源文件所在的文件夹下。解析不出来的目标会让这个产物以 [`DSH8200`](../diagnostics/DSH8xxx.md#dsh8200) 失败，消息里引用
资产层自己的码和文本 —— 见[诊断](#diagnostics)。

<a id="resolution-order"></a>

## 解析顺序

1. **`Root` → 根 package 路径。** 去空白；`\` → `/`；记住原串是否以 `/` 开头；去掉
   首尾的 `/`；按 `/` 切分。任何一步得到空结果都退回 `/Game`。
2. **`Name` → 若干文件夹加一个 leaf。** 去空白；`\` → `/`；去掉所有首尾 `/`；按 `/` 切分。
   **最后一段**是资产名；前面每一段都是追加到根后面的文件夹。
3. 每一段都经过 `ObjectTools::SanitizeObjectName`。空段被静默跳过。
4. `PackageName = <root>/<folders…>/<Leaf>` 和 `ObjectPath = <PackageName>.<Leaf>`。
5. `PackageName` 由 `FPackageName::IsValidObjectPath` 校验。

<a id="root-dispatch"></a>

## `Root=` 的分派

分派看的是 **第一段**，比较时不区分大小写。

| 第一段 | 附加条件 | 根 package | 从段中取走的文件夹段数 |
| :-- | :-- | :-- | :-- |
| *（省略、为空、`/`，或全是空白）* | — | `/Game` | — |
| `Game` | — | `/Game` | 1 |
| `Plugin.<Name>` | — | 该插件的挂载点 | 1 |
| `Plugins.<Name>` | — | 该插件的挂载点 | 1 |
| `Plugin` | 存在第二段 | 由第 1 段命名的插件 | 2 |
| `Plugins` | 存在第二段 | 由第 1 段命名的插件 | 2 |
| 其它任意值 | `Root` 串以 `/` 开头 | 原样使用 `/<segment 0>` | 1 |
| 其它任意值 | 不以 `/` 开头 | `/Game`，第 0 段变成文件夹 | 0 |

<a id="branch--omitted-or-empty"></a>

### 分支 —— 省略或为空

| `Root` | `Name` | Package | Object 路径 |
| :-- | :-- | :-- | :-- |
| *（省略）* | `M_Flat` | `/Game/M_Flat` | `/Game/M_Flat.M_Flat` |
| `""` | `Materials/M_Flat` | `/Game/Materials/M_Flat` | `/Game/Materials/M_Flat.M_Flat` |
| `"/"` | `M_Flat` | `/Game/M_Flat` | `/Game/M_Flat.M_Flat` |

<a id="the-plugin-source-root-default"></a>

#### 插件源码根的默认值

上表是 **工程根** 的答案。住在插件源码根 —— `<Plugin>/DShader`，见[源代码文件](../language/source-files.md#source-roots) —— 下的源文件，则默认落到
那个插件的挂载点，就像它写了 `Root="Plugin.<PluginName>"` 一样。

| 源文件 | `Root` | `Name` | Package |
| :-- | :-- | :-- | :-- |
| `<Project>/DShader/M.dsm` | *（省略）* | `M_Flat` | `/Game/M_Flat` |
| `Plugins/MoonToon/DShader/M.dsm` | *（省略）* | `M_Flat` | `/MoonToon/M_Flat` |
| `Plugins/MoonToon/DShader/M.dsm` | `"/"` | `M_Flat` | `/Game/M_Flat` |
| `Plugins/MoonToon/DShader/M.dsm` | `Game/Legacy` | `M_Flat` | `/Game/Legacy/M_Flat` |

只有 **省略或全是空白** 的 `Root` 才取默认值，所以 `Root="/"`（或任何显式写法）
就是插件根里的文件退回 `/Game` 的办法。默认值按块应用：一个文件可以让它的 `Shader` 块取默认，
同时把某个 `ShaderFunction` 钉在 `/Game`。

> [!NOTE]
> 所属插件无法容纳生成内容时 —— 它不是 `<Project>/Plugins` 下的工程插件、已禁用、没声明
> `CanContainContent`、没有 `Content` 目录或没挂载 —— 默认值被跳过，块退回 `/Game`。*（自 2.0.0 起）*
> 这个回退是静默的：不记录任何日志。解析不出来的推断 `Root` 永远不会作为错误发出，因为
> 作者从来没写过它。

<a id="the-package-asset-root"></a>

#### package 资产根

*（2.1.2 之后）* package 构建的源 —— 位于其 `Assets` 文件夹下的源，或资产头 —— 默认
落到 package 的 **资产根**，无论哪个源码根持有该 package，上面的插件默认值都不适用：

| 源文件 | `Root` | `Name` / export | Package |
| :-- | :-- | :-- | :-- |
| `DShader/Packages/@scope/lib/Assets/MF_A.dss` | *（省略）* | `MF_A` | `/Game/DreamShaderPackages/scope/lib/MF_A` |
| `DShader/Packages/@scope/lib/Assets/Noise/MF_B.dss` | *（省略）* | `MF_B` | `/Game/DreamShaderPackages/scope/lib/Noise/MF_B` |
| `DShader/Packages/lib/Assets/MF_C.dsf` | *（省略）* | `MF_C` | `/Game/DreamShaderPackages/lib/MF_C` |
| `Plugins/MoonToon/DShader/Packages/lib/Assets/MF_D.dss` | *（省略）* | `MF_D` | `/MoonToon/DreamShaderPackages/lib/MF_D` |

2.0 的源把它的文件夹加在 `Assets` 下，就像工程源把它的文件夹加在源码根下一样；
1.x 的 `Name=` 则说出它想要的每一个文件夹。显式的 `Root` 仍然优先。见
[Packages ▸ package 资产](../../tools/packages.md#package-assets)。

默认值只在产物变成资产路径的那一处应用，所以编译、产物解析（Material Content Browser、`dsc list-generated`）
和预览渲染器指的是同一个资产。

<a id="branch--game"></a>

### 分支 —— `Game`

| `Root` | `Name` | Package |
| :-- | :-- | :-- |
| `Game` | `M_Flat` | `/Game/M_Flat` |
| `/Game` | `Materials/M_Flat` | `/Game/Materials/M_Flat` |
| `Game/Materials` | `M_Flat` | `/Game/Materials/M_Flat` |
| `game/materials` | `M_Flat` | `/Game/Materials/M_Flat` |

比较不区分大小写，但输出的根永远是字面量 `/Game`。

<a id="branch--pluginname--pluginsname"></a>

### 分支 —— `Plugin.<Name>` / `Plugins.<Name>`

点号形式用一段就指定了插件；它后面的全部是文件夹。

| `Root` | `Name` | Package | 磁盘位置 |
| :-- | :-- | :-- | :-- |
| `Plugin.MoonToon` | `Mat/Test` | `/MoonToon/Mat/Test` | `<Project>/Plugins/MoonToon/Content/Mat/Test.uasset` |
| `Plugins.MoonToon` | `Test` | `/MoonToon/Test` | `<Project>/Plugins/MoonToon/Content/Test.uasset` |
| `Plugin.MoonToon/Shared` | `Test` | `/MoonToon/Shared/Test` | `<Project>/Plugins/MoonToon/Content/Shared/Test.uasset` |

<a id="branch--pluginname--pluginsname-1"></a>

### 分支 —— `Plugin/<Name>` / `Plugins/<Name>`

斜杠形式要花两段来指定插件引用。

| `Root` | `Name` | Package |
| :-- | :-- | :-- |
| `Plugin/MoonToon` | `Test` | `/MoonToon/Test` |
| `Plugins/MoonToon` | `Mat/Test` | `/MoonToon/Mat/Test` |
| `Plugins/MoonToon/Shared` | `Test` | `/MoonToon/Shared/Test` |

> [!WARNING]
> `Root="Plugin"` 和 `Root="Plugins"` **没有**第二段时并不指代插件。它们会落到分派表最后一行，被当成普通文件夹名，
> 生成 `/Game/Plugin` 和 `/Game/Plugins`。不会发出任何诊断。

<a id="branch--explicit-mount-root"></a>

### 分支 —— 显式挂载根

以 `/` 开头、又不属于上面任何形式的 `Root`，会被原样当作一个挂载点。

| `Root` | `Name` | Package |
| :-- | :-- | :-- |
| `/MyMount` | `Test` | `/MyMount/Test` |
| `/MyMount/Sub` | `Test` | `/MyMount/Sub/Test` |
| `/Engine` | `Test` | `/Engine/Test` |

第一段必须原样通过 `SanitizeObjectName`，否则目标以
[`DSH8097`](../diagnostics/DSH8xxx.md#dsh8097) 失败。挂载点本身在这里**不**检查是否存在 ——
未挂载的根会在后面的 `IsValidObjectPath` 闸门处失败。

<a id="branch--bare-relative-path"></a>

### 分支 —— 裸相对路径

其余不以 `/` 开头的 `Root` 都变成 `/Game` 下的文件夹。

| `Root` | `Name` | Package |
| :-- | :-- | :-- |
| `Foo` | `Test` | `/Game/Foo/Test` |
| `Foo/Bar` | `Test` | `/Game/Foo/Bar/Test` |
| `Foo/Bar` | `Deep/Test` | `/Game/Foo/Bar/Deep/Test` |

这一分支和上一分支的差别只在开头那个斜杠：
`Root="Foo/Bar"` 是 `/Game/Foo/Bar`，`Root="/Foo/Bar"` 是 `/Foo/Bar`。

<a id="plugin-root-requirements"></a>

## 插件根的要求

两种插件写法走同一个校验器。下面每一道闸门都必须按顺序通过；
各有自己的码，由产物的 `DSH8200` 引用。

| # | 要求 | 失败时的码 |
| :-- | :-- | :-- |
| 1 | 插件名非空，且经过 `SanitizeObjectName` 后不变 | [`DSH8095`](../diagnostics/DSH8xxx.md#dsh8095)（`Plugin.X`）、[`DSH8096`](../diagnostics/DSH8xxx.md#dsh8096)（`Plugin/X`） |
| 2 | 插件管理器认识这个名字 | [`DSH8089`](../diagnostics/DSH8xxx.md#dsh8089) |
| 3 | 它是 **工程** 插件，且基目录位于工程的 `Plugins` 目录下 | [`DSH8090`](../diagnostics/DSH8xxx.md#dsh8090) |
| 4 | 它已启用 | [`DSH8091`](../diagnostics/DSH8xxx.md#dsh8091) |
| 5 | 它可以包含内容 | [`DSH8092`](../diagnostics/DSH8xxx.md#dsh8092) |
| 6 | 磁盘上存在它的 `Content` 目录 | [`DSH8093`](../diagnostics/DSH8xxx.md#dsh8093) |
| 7 | 它的内容已挂载 *（仅 UE 5.6+）* | [`DSH8094`](../diagnostics/DSH8xxx.md#dsh8094) |

第 7 道闸门在 UE 5.3 – 5.5 上不存在；在那些引擎上，未挂载的插件会在稍后由
object 路径校验抓到。

根 package 路径就是插件自己的挂载资产路径 —— 规范化成 `\` → `/`，不带结尾
斜杠，强制加上开头斜杠。如果结果退化成空或者 `/`，则使用 `/<PluginName>`。

> [!NOTE]
> 引擎插件、以及安装在引擎目录下的商城插件，即使已启用、已挂载，也会被第 3 道闸门拒绝。只有物理位置在 `<Project>/Plugins` 下的插件才
> 被接受。要写进引擎侧的挂载点，请用显式挂载根分支
>（`Root="/SomeMount"`），它会完全跳过插件校验器。
>
> 这样一个插件自带的源文件不会让 cook 中止。它的产物是那个插件自己的 —— 插件自己会带着它的源文件描述的
> 资产 —— 而宿主工程既不能往那个挂载点生成，也不能改那个文件（插件根是只读的），所以那里的生成失败只记一条日志，cook 继续。
> 只有工程自己的源文件，以及其自身位于 `<Project>/Plugins` 下的插件的源文件，才会让 cook 失败。见 [Cook 行为](ephemeral.md#cook-behaviour)。

<a id="per-asset-kind"></a>

## 各资产类型

四种块的路径解析完全一致。不同的是创建的类，以及
函数类块被打上的 material function usage。

| 源块 | 资产类 | Material function usage |
| :-- | :-- | :-- |
| `Shader` — `Graph` backend | `UMaterial` | — |
| `Shader` — `ThinCustom` backend *（自 1.5.0 起）* | `UDreamShaderMaterialInstance` 加一个隐藏的 `UMaterial` 子对象 | — |
| `ShaderFunction` | `UMaterialFunction` | `Default` |
| `ShaderLayer` *（自 1.3.0 起）* | `UMaterialFunctionMaterialLayer` | `MaterialLayer` |
| `ShaderLayerBlend` *（自 1.3.0 起）* | `UMaterialFunctionMaterialLayerBlend` | `MaterialLayerBlend` |

当解析出来的路径上已经存在资产时，它的类必须匹配：

| 类型 | 匹配规则 |
| :-- | :-- |
| `ShaderFunction` | **精确** 类匹配 —— 该路径上是 `UMaterialFunctionMaterialLayer` 会被拒绝 |
| `ShaderLayer`、`ShaderLayerBlend` | `IsA` 期望的类即可 |
| `Shader` — `Graph` | 已有对象必须是 `UMaterial` |
| `Shader` — `ThinCustom` | 已有对象必须是 `UDreamShaderMaterialInstance` |

<a id="custom-pass-pipelines"></a>

## Custom Pass 管线

*（自 2.1.0 起）* [`.dsp`](../language-v2/passes.md) 没有 `Name=`、没有 `Root=`、也没有 `/// @name`。它唯一的
产物 `UDreamPassPipeline` 以文件命名，按 2.0 的默认规则放置：文件相对它的 [源码根](../language/source-files.md#source-roots) 所在的文件夹，
放在 `/Game` 下 —— 插件源码根里
的文件则放在插件挂载点下，规则和回退与下面的
[插件源码根默认值](#the-plugin-source-root-default) 相同，同样回退到 `/Game`。每个导出 buffer 的
render target 放在管线所在的文件夹里，叫
`<Pipeline>_<Buffer>` 到 `SanitizeObjectName`。

| 源文件 | 管线 | 导出 buffer 的 render target `Mask` |
| :-- | :-- | :-- |
| `<Project>/DShader/Passes/CP_Highlight.dsp` | `/Game/Passes/CP_Highlight` | `/Game/Passes/CP_Highlight_Mask` |
| `<Project>/DShader/CP_Wind.dsp` | `/Game/CP_Wind` | `/Game/CP_Wind_Mask` |
| `Plugins/MoonToon/DShader/Passes/CP_Toon.dsp` | `/MoonToon/Passes/CP_Toon` | `/MoonToon/Passes/CP_Toon_Mask` |

| 在该路径上 | 拒绝条件 |
| :-- | :-- |
| 管线的 | 那里的对象不是 `UDreamPassPipeline`（`DSH8302`），或者它是 DreamShader 没有生成的已保存资产（`DSH8303`） |
| render target 的 | 那里的对象不是 `UTextureRenderTarget2D`，或者它是 DreamShader 没有生成的已保存 render target（`DSH8313`） |

想换管线位置，唯一的办法就是挪 `.dsp` 文件本身，这也是为什么反编译出来的
`.dsp` 无法保留它资产原本在别处的路径（`DSH9224`）。移动或重命名文件会在新路径上新建一条
管线；旧的管线及其 render target 留在原地，它的 HLSL 槽位会一直占用，直到
[`dsc pass-registry -Gc`](../../tools/commandlet.md#pass-registry) 回收它们。

<a id="on-disk-mapping"></a>

## 磁盘映射

| Package 根 | 磁盘目录 |
| :-- | :-- |
| `/Game/…` | `<Project>/Content/…` |
| `/<PluginName>/…` | `<Project>/Plugins/<PluginName>/Content/…` |
| `/<MountRoot>/…` | 该挂载点注册到的任何位置 |

文件是 `<directory>/<Leaf>.uasset`。`Graph` backend 的材质以及每个函数、layer 和
blend 资产，每次允许写入的构建都会保存；ThinCustom 产物只有在
Materialized 之后才有文件 —— 见 [内存材质](ephemeral.md)。

<a id="notes"></a>

## 说明

- **`Name` 可以包含文件夹；`Root` 只是前缀。** `Root="Game"` 下的 `Name="A/B/C"` 会得到
  `/Game/A/B/C`，资产名为 `C`。
- **重复的属性键会覆盖前一个。** `Shader(Name="A", Name="B")` 解析为
  `B`，并给出警告 [`DSH2244`](../diagnostics/DSH2xxx.md#dsh2244) *（自 2.0.0 起；1.x 期间静默）*。
- 每一段都会独立进行净化。`SanitizeObjectName` 拒绝的字符会被替换，因此
  `Name="My Mat"` 解析为一个名为 `My_Mat` 的 leaf，且没有任何诊断。
- 空段会被丢弃：`Name="A//B"` 就是 `A/B`，`Root="Game//Sub"` 就是 `/Game/Sub`。
- Content Browser 的状态列和 *Materialize* 操作使用的也是同一个解析器，因此在这里失败的
  路径在 [Material Content
  Browser](../../tools/material-browser.md) 中也会显示为无法解析。

<a id="diagnostics"></a>

## 诊断

资产层的码经由发射器的码到达编译结果：解析不出来的目标是 [`DSH8200`](../diagnostics/DSH8xxx.md#dsh8200)，无法创建或复用的资产
是 `DSH8201`（`Graph` 材质）、`DSH8203`（ThinCustom 实例）或 `DSH8202`（函数、layer、
blend），每条消息都引用下面的码。

| 码 | 触发条件 |
| :-- | :-- |
| `DSH2242` | `Shader` / `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` 头部没有 `Name`，或为空 |
| `DSH2244` | *（警告）* 同一个属性键被写了两次；后写的值生效 |
| [`DSH8098`](../diagnostics/DSH8xxx.md#dsh8098)，`DSH8099` | `Name` 在去空白和去斜杠后为空 |
| [`DSH8100`](../diagnostics/DSH8xxx.md#dsh8100) | leaf 段在净化后为空 |
| [`DSH8088`](../diagnostics/DSH8xxx.md#dsh8088) | `Name` 或 `Root` 的某个非空文件夹段被净化成了空 |
| — | 组装出的路径未通过 `IsValidObjectPath`；`DSH8200` 引用引擎给出的原因，或说明该路径不是有效的 object 路径，自身没有码 |
| `DSH8095`，`DSH8096` | 插件名为空，或被净化改动过 |
| `DSH8097` | 显式挂载根被净化改动过 |
| `DSH8089`–`DSH8094` | 插件闸门 —— 见 [插件根的要求](#plugin-root-requirements) |
| [`DSH8104`](../diagnostics/DSH8xxx.md#dsh8104)，`DSH8108`，`DSH8113` | package 创建失败（材质、实例、函数） |
| [`DSH8105`](../diagnostics/DSH8xxx.md#dsh8105)，`DSH8109`，`DSH8114` | 对象创建失败（材质、实例、函数） |
| [`DSH8102`](../diagnostics/DSH8xxx.md#dsh8102) | `Graph` backend，路径上的类不对 |
| [`DSH8106`](../diagnostics/DSH8xxx.md#dsh8106) | ThinCustom backend，路径上的类不对 |
| [`DSH8111`](../diagnostics/DSH8xxx.md#dsh8111) | 函数类块，路径上的对象不是材质函数 |
| [`DSH8110`](../diagnostics/DSH8xxx.md#dsh8110) | 函数类块，material function 子类不对 |
| [`DSH8103`](../diagnostics/DSH8xxx.md#dsh8103)，[`DSH8107`](../diagnostics/DSH8xxx.md#dsh8107) | 材质上的归属守卫（`Graph`、ThinCustom）—— 见 [重新生成](regeneration.md#ownership-guard) |
| [`DSH8112`](../diagnostics/DSH8xxx.md#dsh8112) | 材质函数上的归属守卫 |

<a id="example"></a>

## 示例

```c
Shader(Name="Mat/Test", Root="Plugin.MoonToon")
{
    Properties { vec3 Tint = vec3(1.0, 0.2, 0.2); }
    Settings   { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }
    Graph      { Color = Tint; }
}
```

解析出的目标：

```text
Root      "Plugin.MoonToon"  ->  /MoonToon                       (plugin mount point)
Name      "Mat/Test"         ->  folders "Mat", leaf "Test"
package                          /MoonToon/Mat/Test
object path                      /MoonToon/Mat/Test.Test
on disk                          <Project>/Plugins/MoonToon/Content/Mat/Test.uasset
```

<a id="see-also"></a>

## 另见

- [Shader](../language/shader.md) —— 头部属性作为块语法的一部分
- [ShaderFunction](../language/shader-function.md) —— 函数块上的相同属性
- [ShaderLayer / ShaderLayerBlend](../language/shader-layer.md) —— layer 资产种类
- [内存材质](ephemeral.md) —— `.uasset` 实际写入的时机
- [重新生成](regeneration.md) —— 归属守卫及其不适用的情况
- [缓存](caching.md) —— 标记资产为 DreamShader 生成的来源元数据
- [Path(Root, "…")](../parameters/path.md) —— *另一套* 路径语法，用于引用已有资产
- [Backend](../settings/backend.md) —— `Shader` 会生成哪个类
- [Custom Pass 管线 —— `.dsp`](../language-v2/passes.md) —— 资产以文件名命名的源文件
- [诊断索引](../diagnostics/index.md) —— 每个消息，按阶段排列
