---
title: 资产路径
description: Name= 与 Root= 如何变成 Unreal package 路径、object 路径和磁盘文件 —— 包括 Plugin.X 根和插件源码根的默认值。
---

每个会生成资产的块都带同样两个头部属性，由同一个解析器把它们变成一个目标位置。

| 项目 | 说明 |
| :-- | :-- |
| 适用于 | `Shader`、`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend` —— 四者完全一致 |
| 种类 | 头部属性 |
| 起始版本 | `Root="Plugin.X"` （自 1.2.0 起） · 插件源码根的默认值 （自 1.6.0 起） |

<a id="语法概览"></a>

## 语法概览

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

```c
Shader        (Name = "<name-path>" [, Root = "<root>"]) { … }
ShaderFunction(Name = "<name-path>" [, Root = "<root>"]) { … }

<name-path> := [<folder> /] … <leaf>
<root>      := "" | Game | Plugin.<PluginName> | Plugins.<PluginName>
             | Plugin / <PluginName> | Plugins / <PluginName>
             | / <MountRoot> [/ <folder>] …
             | <folder> [/ <folder>] …
```

`Name` 是 **必填** 的：没有或为空是 `DSH2242`。`Root` 可选，默认是空字符串。属性名按不区分大小写匹配，所以 `name=` 和
`ROOT=` 都能识别。

（自 2.0.0 起） 1.x 的块完全保留这个目标（legacy 规则 L10）：它的 `Name=` 从不像 `.dss` 产物的名字那样被放到源文件
所在的文件夹下。解析不出来的目标会让这个产物以 `DSH8200` 失败，消息里引用资产层自己的码和文本 —— 见[诊断](#诊断)。

<a id="解析顺序"></a>

## 解析顺序

1. **`Root` 变成根 package 路径。** 去空白；`\` 换成 `/`；记住原串是否以 `/` 开头；去掉首尾的 `/`；按 `/`
   切分。任何一步得到空结果都退回 `/Game`。
2. **`Name` 变成若干文件夹加一个 leaf。** 去空白；`\` 换成 `/`；去掉所有首尾 `/`；按 `/` 切分。**最后一段**
   是资产名，前面每一段都是追加到根后面的文件夹。
3. 每一段都经过 `ObjectTools::SanitizeObjectName`。空段被静默跳过。
4. `PackageName = <root>/<folders…>/<Leaf>`，`ObjectPath = <PackageName>.<Leaf>`。
5. `PackageName` 由 `FPackageName::IsValidObjectPath` 校验。

<a id="root-的分派"></a>

## `Root=` 的分派

分派看的是 `Root` 的 **第一段**，比较时不区分大小写。

| 第一段 | 附加条件 | 根 package | 从 `Root` 取走的文件夹段数 |
| :-- | :-- | :-- | :-- |
| *（省略、为空、`/`、或全是空白）* | — | `/Game` | — |
| `Game` | — | `/Game` | 1 |
| `Plugin.<Name>` | — | 该插件的挂载点 | 1 |
| `Plugins.<Name>` | — | 该插件的挂载点 | 1 |
| `Plugin` | 存在第二段 | 由第 1 段命名的插件 | 2 |
| `Plugins` | 存在第二段 | 由第 1 段命名的插件 | 2 |
| 其它任意值 | `Root` 串以 `/` 开头 | 原样使用 `/<第 0 段>` | 1 |
| 其它任意值 | 不以 `/` 开头 | `/Game`，第 0 段变成文件夹 | 0 |

<a id="省略或为空"></a>

### 省略或为空

| `Root` | `Name` | Package | Object 路径 |
| :-- | :-- | :-- | :-- |
| *（省略）* | `M_Flat` | `/Game/M_Flat` | `/Game/M_Flat.M_Flat` |
| `""` | `Materials/M_Flat` | `/Game/Materials/M_Flat` | `/Game/Materials/M_Flat.M_Flat` |
| `"/"` | `M_Flat` | `/Game/M_Flat` | `/Game/M_Flat.M_Flat` |

<a id="插件源码根的默认值"></a>

#### 插件源码根的默认值

（自 1.6.0 起）

上表是**工程源码根**的答案。住在插件源码根 —— `<Plugin>/DShader` —— 下的源文件，默认落到那个插件的挂载点，
就像它写了 `Root="Plugin.<PluginName>"`。

| 源文件 | `Root` | `Name` | Package |
| :-- | :-- | :-- | :-- |
| `<Project>/DShader/M.dsm` | *（省略）* | `M_Flat` | `/Game/M_Flat` |
| `Plugins/MoonToon/DShader/M.dsm` | *（省略）* | `M_Flat` | `/MoonToon/M_Flat` |
| `Plugins/MoonToon/DShader/M.dsm` | `"/"` | `M_Flat` | `/Game/M_Flat` |
| `Plugins/MoonToon/DShader/M.dsm` | `Game/Legacy` | `M_Flat` | `/Game/Legacy/M_Flat` |

只有**省略或全是空白**的 `Root` 才取默认值，所以 `Root="/"`（或任何显式写法）就是插件根里的文件退回 `/Game` 的办法。
默认值按块应用：一个文件可以让 `Shader` 块取默认、同时把某个 `ShaderFunction` 钉在 `/Game`。

**提示：**

所属插件无法容纳生成内容时 —— 不是 `<Project>/Plugins` 下的工程插件、已禁用、没声明 `CanContainContent`、
没有 `Content` 目录或没挂载 —— 默认值被跳过，块退回 `/Game`。这个回退是**静默**的 （自 2.0.0 起）：作者没写过
`Root`，推断出来的 `Root` 解析不了就不该报错。

默认值只在产物变成资产路径的那一处应用，所以编译、产物解析（Material Content Browser、`dsc list-generated`）
和预览渲染器指的是同一个资产。

<a id="game"></a>

### `Game`

| `Root` | `Name` | Package |
| :-- | :-- | :-- |
| `Game` | `M_Flat` | `/Game/M_Flat` |
| `/Game` | `Materials/M_Flat` | `/Game/Materials/M_Flat` |
| `Game/Materials` | `M_Flat` | `/Game/Materials/M_Flat` |
| `game/materials` | `M_Flat` | `/Game/Materials/M_Flat` |

比较不区分大小写，但输出的根永远是字面量 `/Game`。

<a id="pluginname-与-pluginsname"></a>

### `Plugin.<Name>` 与 `Plugins.<Name>`

点号形式用一段就指定了插件，后面的全部是文件夹。

| `Root` | `Name` | Package | 磁盘位置 |
| :-- | :-- | :-- | :-- |
| `Plugin.MoonToon` | `Mat/Test` | `/MoonToon/Mat/Test` | `<Project>/Plugins/MoonToon/Content/Mat/Test.uasset` |
| `Plugins.MoonToon` | `Test` | `/MoonToon/Test` | `<Project>/Plugins/MoonToon/Content/Test.uasset` |
| `Plugin.MoonToon/Shared` | `Test` | `/MoonToon/Shared/Test` | `<Project>/Plugins/MoonToon/Content/Shared/Test.uasset` |

<a id="pluginname-与-pluginsname-1"></a>

### `Plugin/<Name>` 与 `Plugins/<Name>`

斜杠形式要花两段来指定插件。

| `Root` | `Name` | Package |
| :-- | :-- | :-- |
| `Plugin/MoonToon` | `Test` | `/MoonToon/Test` |
| `Plugins/MoonToon` | `Mat/Test` | `/MoonToon/Mat/Test` |
| `Plugins/MoonToon/Shared` | `Test` | `/MoonToon/Shared/Test` |

**警告：**

`Root="Plugin"` 和 `Root="Plugins"` 在 **没有** 第二段时并不指代插件。它们会落到分派表最后一行，被当成普通文件夹名，
生成 `/Game/Plugin` 和 `/Game/Plugins`。没有任何诊断 —— 资产只是安静地落到了你没打算放的地方。

<a id="显式挂载根"></a>

### 显式挂载根

以 `/` 开头、又不属于上面任何形式的 `Root`，会被原样当作一个挂载点。

| `Root` | `Name` | Package |
| :-- | :-- | :-- |
| `/MyMount` | `Test` | `/MyMount/Test` |
| `/MyMount/Sub` | `Test` | `/MyMount/Sub/Test` |
| `/Engine` | `Test` | `/Engine/Test` |

第一段必须原样通过 `SanitizeObjectName`，否则目标以 `DSH8097` 失败。挂载点本身在这里 **不** 检查是否存在 ——
未挂载的根会在后面的 `IsValidObjectPath` 闸门处失败。

<a id="裸相对路径"></a>

### 裸相对路径

其余不以 `/` 开头的 `Root` 都变成 `/Game` 下的文件夹。

| `Root` | `Name` | Package |
| :-- | :-- | :-- |
| `Foo` | `Test` | `/Game/Foo/Test` |
| `Foo/Bar` | `Test` | `/Game/Foo/Bar/Test` |
| `Foo/Bar` | `Deep/Test` | `/Game/Foo/Bar/Deep/Test` |

这一分支和上一分支的差别只在开头那个斜杠：`Root="Foo/Bar"` 是 `/Game/Foo/Bar`，`Root="/Foo/Bar"` 是
`/Foo/Bar`。

<a id="插件根的要求"></a>

## 插件根的要求

两种插件写法走同一个校验器。下面每一道闸门都必须按顺序通过，各有自己的码，由产物的 `DSH8200` 引用。

| # | 要求 | 失败时 |
| :-- | :-- | :-- |
| 1 | 插件名非空，且经过 `SanitizeObjectName` 后不变 | `DSH8095`（`Plugin.X`）、`DSH8096`（`Plugin/X`） |
| 2 | 插件管理器认识这个名字 | `DSH8089` |
| 3 | 它是 **工程** 插件，基目录位于工程的 `Plugins` 目录下 | `DSH8090` |
| 4 | 它已启用 | `DSH8091` |
| 5 | 它可以包含内容 | `DSH8092` |
| 6 | 磁盘上存在它的 `Content` 目录 | `DSH8093` |
| 7 | 它的内容已挂载 （自 UE 5.6 起） | `DSH8094` |

第 7 道闸门在 UE 5.3 – 5.5 上不存在；在那些引擎上，未挂载的插件会在稍后的 object 路径校验中被抓到。

根 package 路径就是插件自己的挂载资产路径 —— 规范化成正斜杠、去掉结尾斜杠、强制加上开头斜杠。如果结果退化成空或者
`/`，则使用 `/<PluginName>`。

**提示：**

引擎插件、以及安装在引擎目录下的商城插件，即使已启用、已挂载，也会被第 3 道闸门拒绝。只有物理位置在
`<Project>/Plugins` 下的插件才被接受。要写进引擎侧的挂载点，请用显式挂载根分支（`Root="/SomeMount"`），它会完全跳过插件校验器。

这样一个插件自带的源文件**不会让 cook 中止** （自 2.1.2 起）：它的产物是那个插件自己的，宿主工程既不能往那个挂载点生成，
也不能改那个文件，所以那里的生成失败只记一条日志，cook 继续 —— 插件自己会带着它的源文件描述的资产。只有工程自己的源文件，
以及 `<Project>/Plugins` 下插件的源文件，才会让 cook 失败。见[内存材质](ephemeral.md#cook-行为)。

<a id="各资产类型"></a>

## 各资产类型

四种块的路径解析完全一致。不同的是创建的类，以及函数类块被打上的 material function usage。

| 源块 | 资产类 | Material function usage |
| :-- | :-- | :-- |
| `Shader`，`Graph` backend | `UMaterial` | — |
| `Shader`，`ThinCustom` backend （自 1.5.0 起） | `UDreamShaderMaterialInstance` 加一个隐藏的 `UMaterial` 子对象 | — |
| `ShaderFunction` | `UMaterialFunction` | `Default` |
| `ShaderLayer` （自 1.3.0 起） | `UMaterialFunctionMaterialLayer` | `MaterialLayer` |
| `ShaderLayerBlend` （自 1.3.0 起） | `UMaterialFunctionMaterialLayerBlend` | `MaterialLayerBlend` |

当解析出来的路径上已经存在资产时，它的类必须匹配：

| 类型 | 匹配规则 |
| :-- | :-- |
| `ShaderFunction` | **精确** 类匹配 —— 该路径上是 `UMaterialFunctionMaterialLayer` 会被拒绝 |
| `ShaderLayer`、`ShaderLayerBlend` | `IsA` 期望的类即可 |
| `Shader`，`Graph` backend | 已有对象必须是 `UMaterial` |
| `Shader`，`ThinCustom` backend | 已有对象必须是 `UDreamShaderMaterialInstance` |

类匹配不等于归属检查。在每一种 backend、每一种块上 —— ThinCustom 实例路径自 `1.8.0` 起 —— DreamShader 还会拒绝覆盖
不是自己生成的资产，见[重新生成](regeneration.md#归属守卫)。

<a id="custom-pass-管线"></a>

## Custom Pass 管线

（自 2.1.0 起） `.dsp` 没有 `Name=`、没有 `Root=`。它唯一的产物 `UDreamPassPipeline` 以文件命名，按 2.0 的默认规则放置：
文件相对它的源码根所在的文件夹，放在 `/Game` 下 —— 插件源码根里的文件则放在插件挂载点下，规则和回退与上面的
[插件源码根默认值](#插件源码根的默认值)相同。每个导出 buffer 的 render target 放在管线旁边，叫 `<Pipeline>_<Buffer>`。
想换位置只能挪 `.dsp` 文件；细节见 [Custom Pass（.dsp）](../language-v2/passes.md)。

<a id="磁盘映射"></a>

## 磁盘映射

| Package 根 | 磁盘目录 |
| :-- | :-- |
| `/Game/…` | `<Project>/Content/…` |
| `/<PluginName>/…` | `<Project>/Plugins/<PluginName>/Content/…` |
| `/<MountRoot>/…` | 该挂载点注册到的任何位置 |

文件是 `<directory>/<Leaf>.uasset`。`Graph` backend 的材质以及每个函数、layer 和 blend，每次允许写入的构建都会保存；
ThinCustom 产物只有在 Materialized 之后才有文件 —— 见[内存材质](ephemeral.md)。

<a id="说明"></a>

## 说明

- **`Name` 可以带文件夹，`Root` 只是前缀。** `Root="Game"` 下的 `Name="A/B/C"` 得到 `/Game/A/B/C`，资产名是 `C`。
- **重复的属性名会覆盖前一个。** `Shader(Name="A", Name="B")` 解析成 `B`，并给出警告 `DSH2244` （自 2.0.0 起）
  （1.x 是静默的）。
- 每一段都独立做净化，所以 `Name="My Mat"` 会得到叫 `My_Mat` 的 leaf，没有诊断。
- 空段会被丢弃：`Name="A//B"` 就是 `A/B`，`Root="Game//Sub"` 就是 `/Game/Sub`。
- Material Content Browser 的状态列和 *Materialize* 操作用的是同一个解析器，所以在这里失败的路径，在
  [编辑器工具](../tools/editor-integration.md#material-content-browser) 里也会显示为无法解析。
- 不要和 `Path(Root, "…")` 混淆，那是 *另一套* 路径语法 —— 它用来 **引用** 已有资产，而不是指定生成目标。见
  [Path 资产引用](../parameters/path.md)。

<a id="诊断"></a>

## 诊断

资产层的码经由发射器的码到达编译结果：解析不出来的目标是 `DSH8200`，资产无法创建或复用是 `DSH8201`（`Graph` 材质）、
`DSH8203`（ThinCustom 实例）或 `DSH8202`（函数、layer、blend），每条消息都引用下面的某个码。

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH2242: Expected a 'Name = "..."' attribute on '{0}', found none.` | Shader / ShaderFunction / ShaderLayer / ShaderLayerBlend 头部没有 Name，或为空。 | 加上 Name = "..."。 |
| `DSH2244: The attribute '{0}' is written twice; the later value wins, as it did in 1.x.` | 同一个属性写了两次（警告）。 | 删掉其中一个。 |
| `DSH8200: '{0}' does not resolve to a valid asset path. {1}` | 产物的目标解析不成合法 package 路径；{1} 是资产层下面某个码的原文，说明是哪条规则。 | 先读被引用的那条消息，它会指出出问题的那一段。 |
| `DSH8098: DreamShader asset name must resolve to a non-empty asset path.` | Name 去空白、去斜杠后为空（只有斜杠时是 DSH8099）。 |  |
| `DSH8100: DreamShader asset name '%s' produced an invalid asset name.` | leaf 段净化后为空，或含引擎拒绝的字符。 | 用字母、数字和 \_ 重命名。 |
| `DSH8088: %s contains an invalid folder segment.` | Name 或 Root 中某个非空文件夹段被净化成了空、或是 . / ..。 | 写成普通文件夹加资产名：Characters/Hero/M\_Hero。 |
| `DSH8095: DreamShader Root '%s' has an invalid plugin name.` | Plugin. 后面的插件名为空或被净化改动过（Plugin/X 写法是 DSH8096）。 | 写 Plugin.MyPlugin。 |
| `DSH8097: DreamShader Root '%s' has an invalid package root.` | 显式挂载根被净化改动过。 |  |
| `DSH8089: DreamShader Root '%s' references project plugin '%s', but no enabled plugin with that name was found.` | 没有哪个插件叫这个名字。 | 核对插件名（.uplugin 文件名，不是显示名）。 |
| `DSH8090: DreamShader Root '%s' must reference a project plugin under '%s'.` | 引擎插件，或位于工程 Plugins 目录之外的插件。 | 改用 Root="/SomeMount"，或者指向工程插件 / Game。 |
| `DSH8091: DreamShader Root '%s' references project plugin '%s', but the plugin is not enabled.` | 插件已禁用。 |  |
| `DSH8092: DreamShader Root '%s' references project plugin '%s', but the plugin cannot contain content.` | 纯代码插件。 | 在 .uplugin 里设 "CanContainContent": true。 |
| `DSH8093: DreamShader Root '%s' references project plugin '%s', but its Content directory does not exist: '%s'.` | 缺少 Content 目录。 | 创建消息里那个文件夹。 |
| `DSH8094: DreamShader Root '%s' references project plugin '%s', but the plugin content is not mounted.` | 插件内容未挂载 —— 通常是编辑器运行期间才启用或创建的插件。仅 UE 5.6+。 | 重启编辑器（或 commandlet）。 |
| `DSH8104: Failed to create package '%s'.` | package 创建失败（实例和函数分别是 DSH8108、DSH8113；对象创建失败是 DSH8105 / DSH8109 / DSH8114）。 |  |
| `DSH8102: Asset '%s' already exists and is not a Material.` | Graph backend，路径上的类不对。 |  |
| `DSH8106: Asset '%s' already exists and is not a DreamShader instance material. Delete it (or remove Backend="Instance") before switching backends.` | ThinCustom backend，路径上的类不对 —— 通常是切换了源文件的 backend。 |  [详解](../settings/backend.md) |
| `DSH8111: Asset '%s' already exists and is not a MaterialFunction asset.` | 函数类块，路径上不是材质函数。 |  |
| `DSH8110: Asset '%s' already exists as '%s', but %s generation requires '%s'. Delete or move the existing asset and regenerate it.` | 函数类块，material function 子类不对。 |  |
| `DSH8103: Asset '%s' already exists and was not generated by DreamShader. Rename your shader or move/delete the existing asset before regenerating.` | 材质上的归属守卫（ThinCustom 实例是 DSH8107）。 |  [详解](regeneration.md#归属守卫) |
| `DSH8112: Asset '%s' already exists and was not generated by DreamShader. Rename your function or move/delete the existing asset before regenerating.` | 材质函数上的归属守卫。 |  [详解](regeneration.md#归属守卫) |

<a id="完整示例"></a>

## 完整示例

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

<a id="继续阅读"></a>

## 继续阅读

- [生成流程](index.md)
  - [内存材质](ephemeral.md)
  - [重新生成](regeneration.md)
  - [Path 资产引用](../parameters/path.md)
