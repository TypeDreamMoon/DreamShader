---
title: Path 资产引用
description: Path(Root, "…") 与内容浏览器的 Copy Reference —— 每种 root 拼写、绝对路径与 root、对象路径补全、共用的解析器，以及它的错误码。
---

`Path(…)` 是源码引用现有 Unreal 资产的方式：一个包根加一段相对路径，在生成时解析为完整对象路径。

```c
Path( <root> , "<relative-path>" )
Path( "<absolute-path>" )
"<absolute-path>"
<Class>'<absolute-path>'
```

```c
Texture2D A = Path(Game, "Textures/T_X");
Texture2D B = Path("Plugin.MyPlugin", "Textures/T_X");
Texture2D C = Path("/Game/Textures/T_X");
Texture2D D = "/Game/Textures/T_X";                  // 裸引号形式
Texture2D E = Texture2D'/Game/Textures/T_X.T_X';     // Copy Reference
```

root 和路径都可以带引号也可以不带。裸引号路径形式自 （自 1.5.0 起） 起可用；`Plugin.` / `Plugins.` 两种 root 自
（自 1.2.0 起） 起就存在；`Class'…'` 引用自 （自 1.9.0 起） 起可用。前端把引用保持为写下的文本，在构建资产时
才解析 —— 只有编辑器知道哪些插件已挂载。

`Path(…)` 会出现在这些地方：

| 位置 | 示例 |
| :-- | :-- |
| 纹理属性的 `= <default>` | `Texture2D T = Path(Game, "T_X");` |
| 对象类型的[元数据](metadata.md#对象属性)条目 | `[Texture = Path(Game, "Textures/T_Mask")]` |
| `UE.CollectionParam` 的实参 | `UE.CollectionParam(Collection = Path(Game, "MPC_World"), Parameter = "Wind")` |
| `VirtualFunction` 的 `Options.Asset` | `Options = { Asset = Path(Game, "Functions/MF_X"); }` |
| 对象类型的[材质设置](../settings/material.md) | `PhysicalMaterial = Path(Engine, "EngineMaterials/DefaultPhysicalMaterial");` |

<a id="内容浏览器引用"></a>

## 内容浏览器引用

（自 1.9.0 起）

在资产上右键选 **Copy Reference**，剪贴板里是 Unreal 的*导出*形式：

```text
/Script/Engine.Texture2D'/Game/Cloud/Texture/T_VolumeCloud_03.T_VolumeCloud_03'
```

原样粘贴即可。解析器先剥掉 `<Class>'…'` 外壳，用引号里的对象路径；引号前的类只用于下面的检查。外壳认三种写法：

| 写法 | 读到的类 |
| :-- | :-- |
| `/Script/Engine.Texture2D'/Game/…'` | `Texture2D` |
| `Texture2D'/Game/…'` —— 旧版引擎的写法 | `Texture2D` |
| `'/Game/…'` —— 引号前没有类 | *（无）* |

外壳可以裸写、包在引号里，或交给 `Path(…)` —— 下面四个指向同一个资产：

```c
Texture2D A = Texture2D'/Game/Textures/T_X.T_X';
Texture2D B = "Texture2D'/Game/Textures/T_X.T_X'";
Texture2D C = Path("Texture2D'/Game/Textures/T_X.T_X'");
Texture2D D = Path(Game, "Texture2D'/Game/Textures/T_X.T_X'");   // root 被忽略
```

引号里的路径是**绝对**路径，所以旁边写的 root 被忽略，和其他绝对路径一样。只有格式完整的外壳才会被认出：文本以 `'` 结尾、
前面还有一个 `'`、两者之间非空且不含别的 `'`。其他情况原样交给解析器的普通语法 —— 以及它的普通诊断。

<a id="类检查"></a>

### 类检查

外壳里写的类会在加载资产之前，和它要赋给的槽位比对：

| 槽位 | 写的类 | 结果 |
| :-- | :-- | :-- |
| 声明了维度的纹理属性的默认值（`Texture2D`、`TextureCube`、`Texture2DArray`、`Texture3D`、`VolumeTexture`，以及 `TextureSampleParameter2D` / `2DArray` / `Cube` / `Volume` / `SubUV`） | 另一维度的纹理类 | `DSH1044`，两者都写出来 |
| 任何纹理属性的默认值 | DreamShader 知道不是纹理的类（`MaterialFunction`、`CurveLinearColor`、`Font`……） | `DSH1043` |
| 任何对象属性 —— [元数据](metadata.md#对象属性)条目、纹理采样默认值、`UE.CollectionParam` 集合、对象设置、`VirtualFunction` 的 `Options.Asset` | 与槽位要的类无关的类 | `DSH1045`，两者都写出来 |
| 任何槽位 | 这个构建里没有的类，或 DreamShader 不建模其维度的类（`SparseVolumeTexture`、`TextureCubeArray`、`TextureCollection`、`RuntimeVirtualTexture`） | 接受 —— 剥掉后忘掉 |

`DSH1043` 和 `DSH1044` 在文件解析完立即报告；`DSH1045` 在构建资产时报告。认不出的类永远不是错误 —— Unreal 解析导出路径时
本身就忽略类。`TextureObjectParameter`（自己不声明维度）和 `TextureSampleParameterCubeArray` 只做「是不是纹理」那一半检查。

<a id="root"></a>

## Root

root 实参会先去引号，`\` 折成 `/`，去掉首尾的 `/`，然后按 `/` 拆分。**第一段**就是 root。

| Root 拼写 | 解析为 |
| :-- | :-- |
| `Game` | `/Game` |
| `Engine` | `/Engine` |
| `Plugin.<Name>` | 该插件挂载的资产路径 |
| `Plugins.<Name>` | 该插件挂载的资产路径 |
| `Plugin/<Name>` | 该插件挂载的资产路径 —— 消耗两段 |
| `Plugins/<Name>` | 该插件挂载的资产路径 —— 消耗两段 |

所有拼写都不区分大小写；第一段是其他内容报 `DSH8126`。

**root 之后的段会作为文件夹追加。** `Path("Game/Textures", "T_X")` 解析为 `/Game/Textures/T_X`，
`Path("Plugin/MyPlugin/Materials", "MF_X")` 解析为 `/MyPlugin/Materials/MF_X`。

插件 root 通过插件自身挂载的资产路径解析，并做同样的规范化；该路径为空或只有 `/` 时改用 `/<PluginName>`。插件必须已知、
已启用、能包含内容、磁盘上有 `Content` 文件夹，并且 —— 在 UE 5.6 及以上 —— 已挂载（`DSH8118` … `DSH8122`）。

<a id="root-与绝对路径"></a>

### Root 与绝对路径

（自 1.9.0 起）

只有资产路径是**相对**路径时才看 root。写在绝对路径旁边 —— 以 `/` 开头的路径，包括 `Class'…'` 外壳里的路径 —— root
**被忽略**，也不做校验：`Path(Plugin.NotInstalled, "/Game/Textures/T_X")` 解析为 `/Game/Textures/T_X.T_X`，不会抱怨插件。

1.9.0 之前纹理默认值解析器照样把 root 拼上，`Texture2D T = Path(Game, "/Game/Textures/T_X");` 会解析成
`/Game/Game/Textures/T_X.T_X` 然后加载失败。现在 root 被丢掉。

**提示：**

如果源码依赖过旧的拼接行为 —— 故意带前导 `/` 写的相对路径，比如 `Path(Game, "/Textures/T_X")` —— 现在它解析为
`/Textures/T_X.T_X` 并加载失败。去掉前导 `/`：`Path(Game, "Textures/T_X")`。

<a id="对象路径补全"></a>

## 对象路径补全

root 和相对路径拼接之后，结果会补全成完整对象路径：如果最后一个 `/` 之后的文本不含 `.`，就在末尾追加 `.` 加资产名。

| 写法 | 解析结果 |
| :-- | :-- |
| `Path(Game, "Textures/T_X")` | `/Game/Textures/T_X.T_X` |
| `Path(Game, "Textures/T_X.T_X")` | `/Game/Textures/T_X.T_X` |
| `"/Game/Textures/T_X"` | `/Game/Textures/T_X.T_X` |
| `Texture2D'/Game/Textures/T_X.T_X'` | `/Game/Textures/T_X.T_X` |
| `Path(Game, "Texture2D'/Game/Textures/T_X.T_X'")` | `/Game/Textures/T_X.T_X` —— root 被忽略 |

补全后的路径最后交给 Unreal 自己的对象路径校验器；失败时它的消息（没有自己的码）被引用在槽位的诊断里。

<a id="解析器"></a>

## 解析器

（自 2.0.0 起） 现在只有**一套**解析器。1.x 有两套 —— 纹理默认值一套，元数据和其余一套 —— 接受的形式和消息都不同；
现在每个引用位置都走这套共享解析器，由槽位的诊断引用它的码。

| 引用出现的位置 | 报告为 |
| :-- | :-- |
| 紧凑纹理 token 或 `TextureObjectParameter` 的 `= <default>` | 解析不了 `DSH8271`，加载不了 `DSH8218` |
| `const` 纹理或 `TextureSampleParameter*` token 的 `= <default>` | 节点的 `Texture` 属性：解析不了 `DSH8213`；加载失败是静默的（见下） |
| 对象类型的[元数据](metadata.md#对象属性)条目 —— `[Texture=…]`、`[VirtualTexture=…]`…… | `DSH8213` |
| `UE.CollectionParam(Collection = …)` / `UE.CollectionParameter(Asset = …)` | `DSH8213`；参数名必须在集合里存在（`DSH8254`） |
| 对象类型的 `Settings` 值 —— `PhysicalMaterial = Path(…)` | `DSH8215`，引用 `DSH7125` |
| `VirtualFunction` 的 `Options = { Asset = …; }` | 解析不了 `DSH8270`，加载不了 `DSH8219` |

<a id="解析器接受什么"></a>

### 解析器接受什么

| 形式 | |
| :-- | :-- |
| `Path(root, "path")` | 接受 |
| `Path("/absolute/path")` | 接受 |
| `"/absolute/path"`（裸**引号**） | 接受 |
| `/absolute/path`（裸**无引号**） | 接受。作为纹理声明的默认值时按写下的加载 （自 2.0.0 起）（1.x 的纹理默认值解析器拒绝）；但还是加上引号 —— 不带引号时它按 token 读取，像 `1Folder` 这样的段是格式错误的数字（`DSH2105`） |
| `Class'/absolute/path'`，带不带引号都行 | 接受 （自 1.9.0 起） |
| 3 个及以上实参的 `Path(…)` | `DSH8130`（纹理默认值从 （自 2.0.0 起） 起也是，它以前读完第二个就停） |
| root **加上**绝对资产路径 | root **被忽略**；绝对路径胜出 |
| 插件名 | 必须原样通过 Unreal 的对象名净化器 |
| 插件 `Content` 目录必须存在 | 检查 |
| 插件内容必须已挂载 | 检查 （自 UE 5.6 起） |

根本不是引用的纹理声明默认值 —— `Texture2D T = Foo;` —— 不再按拼写拒绝，只是在构建材质时找不到（`DSH8218`）。

<a id="字符串转义"></a>

## 字符串转义

带引号的路径是字符串字面量，由词法分析器读取 （自 2.0.0 起）：识别 `\\`、`\"`、`\n`、`\r`、`\t` 和 `\0`，其他 `\X`
报 `DSH2104` —— 1.x 读作字面的 `X`。所以路径里的反斜杠要写成 `\\`，解析器随后把它折成 `/`。参见
[词法与大小写](../language/lexical.md#字符串字面量)。

<a id="加载是另一步"></a>

## 加载是另一步

解析路径只产出文本。资产能否加载是另一件事，结果取决于槽位：

| 槽位 | 资产加载失败时 |
| :-- | :-- |
| 紧凑纹理 token 或 `TextureObjectParameter` 的默认值 | `DSH8218` |
| `const` 纹理 token 的默认值 | **静默写入 `nullptr`** （自 2.0.0 起） |
| 纹理采样 token 的默认值 | **静默写入 `nullptr`**；节点随后取引擎默认纹理 （自 2.0.0 起） |
| 名为 `Texture` 或 `TextureObject`、类型为 `UTexture` 子类的元数据对象属性 | **静默写入 `nullptr`，并报告为成功** |
| 其他对象属性，包括 `UE.CollectionParam` 的集合 | `DSH8213`，引用 `DSH7138` |
| 对象设置 | `DSH8215`，引用 `DSH7138` |
| `VirtualFunction` 的资产 | `DSH8219` |

**警告：**

要当心的正是静默写 null：`[Texture = Path(Game, "Typo")]` —— （自 2.0.0 起） 起还有
`TextureSampleParameter2D T = Path(Game, "Typo");` —— 不产生任何诊断就生成完毕，采样器落在一张没人选的纹理上。
**能解析的路径不等于存在的路径。**

<a id="诊断"></a>

## 诊断

每个码都包在它所在槽位的诊断里到达你 —— 见[解析器](#解析器)。

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH8127: Asset reference cannot be empty.` | trim 之后值为空。 |  |
| `DSH8128: Asset Path(...) reference is missing a closing ')'.` | 以 Path( 开头但不以 ) 结尾。 |  |
| `DSH8129: Asset Path(...) contains an unterminated string literal.` | 实参列表里有未闭合的 "。 |  |
| `DSH8130: Asset Path(...) expects either 1 argument (/Game/... path) or 2 arguments (Game\|Engine\|Plugin.PluginName, asset path).` | 三个及以上实参。空的 Path() 算作一个空实参，报 DSH8131。 |  |
| `DSH8131: Asset reference requires a non-empty path.` | 资产路径实参为空。 |  |
| `DSH8123: Relative asset Path(...) references require a root such as Game, Engine, or Plugin.PluginName.` | 相对路径却没有 root，包括裸的带引号相对路径。 |  |
| `DSH8126: Unsupported asset Path root '%s'. Use Game, Engine, or Plugin.PluginName.` | root 第一段不属于六种可接受拼写。 |  |
| `DSH8124: Asset Path root '%s' has an invalid plugin name.` | Plugin.Name / Plugins.Name 的名字为空，或被 Unreal 的对象名净化器改写（Plugin/Name 形式报 DSH8125）。 |  |
| `DSH8118: Asset Path root '%s' references plugin '%s', but no enabled plugin with that name was found.` | 插件管理器不认识这个插件。 |  |
| `DSH8119: Asset Path root '%s' references plugin '%s', but the plugin is not enabled.` | 插件存在但未启用。 |  |
| `DSH8120: Asset Path root '%s' references plugin '%s', but the plugin cannot contain content.` | 插件声明了不含内容。 |  |
| `DSH8121: Asset Path root '%s' references plugin '%s', but its Content directory does not exist: '%s'.` | 插件的 Content 目录在磁盘上不存在。 |  |
| `DSH8122: Asset Path root '%s' references plugin '%s', but the plugin content is not mounted.` | 插件内容未挂载（UE 5.6+）。 |  |
| `DSH8132: Invalid asset path '%s'.` | 拼接后的路径没有 /，或以 / 结尾。 |  |
| `DSH1043: Asset reference is written as '{0}', which is not a texture class; a texture default requires {1}.` | 纹理默认值的 Class'…' 写的不是纹理类。 |  |
| `DSH1044: Asset reference is written as '{0}', but this property is declared as {1}.` | 纹理默认值的 Class'…' 写的是另一维度的纹理。 |  |
| `DSH1045: Asset reference is written as '%s', but this slot requires a '%s'.` | Class'…' 写的类与对象属性要的类无关。 |  |
| `DSH2104: Unknown escape sequence '\{0}' in a string literal.` | 带引号的路径里有未知转义。 | 反斜杠写成 \\\\，或直接用 /。 |

Unreal 对象路径校验器拒绝补全后的路径时没有自己的码，它的消息被引用在槽位的诊断里。完整清单见[错误速查](../diagnostics/index.md)。

<a id="示例"></a>

## 示例

```c
Shader(Name="Docs/M_Paths")
{
    Properties = {
        // root + 相对路径 —— 标准写法。
        Texture2D A = Path(Game, "Textures/T_White");

        // 带引号的 root，并由 root 携带额外的文件夹段。
        Texture2D B = Path("Game/Textures", "T_Noise");

        // 引擎内容。
        TextureCube C = Path(Engine, "EngineResources/DefaultTextureCube");

        // 插件内容：两种拼写解析结果一致。
        Texture2D D = Path(Plugin.DreamShader, "Textures/T_Probe");
        Texture2D E = Path("Plugins/DreamShader", "Textures/T_Probe");

        // 单实参与裸引号形式 —— 都必须是绝对路径。
        Texture2D F = Path("/Game/Textures/T_White");
        Texture2D G = "/Game/Textures/T_White";

        // 直接从内容浏览器的 Copy Reference 粘贴。
        Texture2D H = /Script/Engine.Texture2D'/Game/Textures/T_White.T_White';
        Texture2D I = "Texture2D'/Game/Textures/T_White.T_White'";

        // 对象类型的元数据条目。
        TextureSampleParameter2D Mask [Texture = Path(Game, "Textures/T_Mask")];

        // UE 内置节点的实参。
        UE.CollectionParam(Collection = Path(Game, "Collections/MPC_World"),
                           Parameter  = "WindStrength") Wind;
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }
    Outputs  = { vec3 Color; Base.EmissiveColor = Color; }

    Graph = {
        Color = vec3(Wind, Wind, Wind);
    }
}
```

解析出的对象路径：

```text
A  /Game/Textures/T_White.T_White
B  /Game/Textures/T_Noise.T_Noise
C  /Engine/EngineResources/DefaultTextureCube.DefaultTextureCube
D  /DreamShader/Textures/T_Probe.T_Probe          （插件挂载的资产路径）
E  /DreamShader/Textures/T_Probe.T_Probe
F  /Game/Textures/T_White.T_White
G  /Game/Textures/T_White.T_White
H  /Game/Textures/T_White.T_White
I  /Game/Textures/T_White.T_White
```

引用在它所属的 property 被构建时才解析。这个 `Graph` 只读 `Wind`，所以那些纹理作为未读取的 property 被丢弃（`DSH4390`，
提示），只有集合会被解析；读取它们，每个都会按上面解析。

<a id="下一步"></a>

## 下一步

- [Properties 类型](../language/properties.md) —— 哪些 token 接受 `= Path(…)`，哪些不接受
- [元数据与分组](metadata.md) —— 对象类型条目与静默写 null 的情况
- [资产路径](../generation/asset-paths.md) —— **输出**侧：`Name=` 加 `Root=` 如何变成包路径
- [UE.\* 节点](../builtins/ue.md) —— `UE.CollectionParam` 及其 `Collection` 实参
