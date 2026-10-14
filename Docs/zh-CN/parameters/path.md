---
title: "Path(…) 资产引用"
description: "Path(…) 资产引用"
---

<a id="path-asset-references"></a>

# Path(…) 资产引用

> [DreamShader](../index.md) » [参数](index.md) » **Path(…)**

资产引用形式：一个包根加一段相对路径，在生成时解析为完整的 Unreal 对象路径。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf`、`.dsh` —— 纹理属性的 `= <default>`、对象类型的元数据条目、`UE.CollectionParam` 实参、对象类型的 `Settings` 值，或 `VirtualFunction` 的 `Options.Asset` |
| 类型 | 值形式 |
| 生成 | 形如 `/Game/Textures/T_X.T_X` 的 `FSoftObjectPath` 风格对象路径 |
| 自 | `1.2.0`（`Plugin.` / `Plugins.` 两种 root）；裸引号路径自 `1.5.0` 起；`Class'…'` 引用自 `1.9.0` 起 |

<a id="synopsis"></a>

## 概要

```c
Path( <root> , "<relative-path>" )
Path( "<absolute-path>" )
"<absolute-path>"
<Class>'<absolute-path>'
```

`<root>` 和路径都可以带引号也可以不带。

```c
Texture2D A = Path(Game, "Textures/T_X");
Texture2D B = Path("Plugin.MyPlugin", "Textures/T_X");
Texture2D C = Path("/Game/Textures/T_X");
Texture2D D = "/Game/Textures/T_X";
Texture2D E = Texture2D'/Game/Textures/T_X.T_X';
```

前端把引用保持为写下的文本；它在资产构建时才解析，因为只有编辑器知道哪些插件已挂载。

<a id="content-browser-references"></a>

## 内容浏览器引用

*（自 1.9.0 起）*

在资产上右键选择 **Copy Reference**，剪贴板里是 Unreal 的*导出*形式：

```text
/Script/Engine.Texture2D'/Game/Cloud/Texture/T_VolumeCloud_03.T_VolumeCloud_03'
```

原样粘贴即可。解析器会先剥掉 `<Class>'…'` 外壳，使用引号内的对象路径；引号前的类只用于下面的检查。外壳认三种写法：

| 写法 | 读到的类 |
| :-- | :-- |
| `/Script/Engine.Texture2D'/Game/…'` | `Texture2D` |
| `Texture2D'/Game/…'` —— 旧版引擎的写法 | `Texture2D` |
| `'/Game/…'` —— 引号前没有类 | *（无）* |

外壳可以裸写、包在引号里，或交给 `Path(…)` —— 下面四个都指向同一个资产：

```c
Texture2D A = Texture2D'/Game/Textures/T_X.T_X';
Texture2D B = "Texture2D'/Game/Textures/T_X.T_X'";
Texture2D C = Path("Texture2D'/Game/Textures/T_X.T_X'");
Texture2D D = Path(Game, "Texture2D'/Game/Textures/T_X.T_X'");   // the root is ignored
```

引号里的路径是**绝对**路径，所以旁边写的 root 会被忽略，和其他任何绝对路径一样。参见 [Root 与绝对路径](#roots-and-absolute-paths)。

只有当外壳格式完整时才会被识别：文本必须以 `'` 结尾，前面必须有一个 `'`，两者之间的内容必须非空且不含别的 `'`。其他任何情况都原样保留，交给解析器的普通语法 —— 以及它的普通诊断。

<a id="class-checking"></a>

### 类检查

外壳里写的类会在资产加载之前，与它要赋给的槽位比对：

| 槽位 | 写的类 | 结果 |
| :-- | :-- | :-- |
| 声明了维度的纹理属性的默认值（`Texture2D`、`TextureCube`、`Texture2DArray`、`Texture3D`、`VolumeTexture`，以及 `TextureSampleParameter2D` / `2DArray` / `Cube` / `Volume` / `SubUV`） | 另一维度的纹理类 | [`DSH1044`](../diagnostics/DSH1xxx.md#dsh1044)，两者都写出来 |
| 任何纹理属性的默认值 | DreamShader 知道不是纹理的类（`MaterialFunction`、`CurveLinearColor`、`Font`……） | [`DSH1043`](../diagnostics/DSH1xxx.md#dsh1043) |
| 任何对象属性 —— [元数据](metadata.md#object-properties)条目、纹理采样默认值、`UE.CollectionParam` 集合、对象设置、`VirtualFunction` 的 `Options.Asset` | 与槽位要的类无关的类 | [`DSH1045`](../diagnostics/DSH1xxx.md#dsh1045)，两者都写出来 |
| 任何槽位 | 这个构建里没有的类，或 DreamShader 不建模其维度的类（`SparseVolumeTexture`、`TextureCubeArray`、`TextureCollection`、`RuntimeVirtualTexture`） | 接受 —— 剥掉后忘掉 |

`DSH1043` 和 `DSH1044` 在文件解析完立即报告；`DSH1045` 在构建资产时报告。

> [!NOTE]
> 认不出的类永远不是错误。Unreal 自己在解析导出路径时也会忽略类，而且项目可以自由地写 DreamShader 从未听说过的类，所以无法被识别的类前缀会被丢弃而不是拒绝。

`TextureObjectParameter`（自己不声明维度）和 `TextureSampleParameterCubeArray` 只做表中「是不是纹理」那一半的检查；`TextureObjectParameter` 的真实维度来自加载的资产。

<a id="roots"></a>

## Root

root 实参不带引号，`\` 折成 `/`，去掉首尾的 `/`；结果再按 `/` 拆分。**第一段**就是 root。

| Root 拼写 | 解析为 |
| :-- | :-- |
| `Game` | `/Game` |
| `Engine` | `/Engine` |
| `Plugin.<Name>` | 该插件挂载的资产路径 |
| `Plugins.<Name>` | 该插件挂载的资产路径 |
| `Plugin/<Name>` | 该插件挂载的资产路径 —— 消耗两段 |
| `Plugins/<Name>` | 该插件挂载的资产路径 —— 消耗两段 |

所有拼写都不区分大小写。第一段是其他任何内容都报 [`DSH8126`](../diagnostics/DSH8xxx.md#dsh8126)。

**root 之后的段会作为文件夹追加。** `Path("Game/Textures", "T_X")` 解析为 `/Game/Textures/T_X`，`Path("Plugin/MyPlugin/Materials", "MF_X")` 解析为 `/MyPlugin/Materials/MF_X`。

插件 root 通过插件自身挂载的资产路径解析，并做同样的规范化（反斜杠折算、去掉末尾的 `/`、加上前导的 `/`）。如果那个路径为空或只有 `/`，则改用 `/<PluginName>`。插件必须已知、已启用、能包含内容、磁盘上有 `Content` 文件夹，并且 —— 在 UE 5.6 及以上 —— 已挂载（`DSH8118` … `DSH8122`）。

<a id="roots-and-absolute-paths"></a>

### Root 与绝对路径

*（1.9.0 变更）*

只有资产路径是**相对**路径时才看 root。写在绝对路径旁边 —— 以 `/` 开头的路径，包括 `Class'…'` 外壳里的路径 —— root 会被**忽略**，也不做校验：`Path(Plugin.NotInstalled, "/Game/Textures/T_X")` 解析为 `/Game/Textures/T_X.T_X`，不会抱怨插件。

1.9.0 之前，纹理默认值解析器照样把 root 拼在前面，所以 `Texture2D T = Path(Game, "/Game/Textures/T_X");` 解析为 `/Game/Game/Textures/T_X.T_X` 然后加载失败。自 1.9.0 起 root 被丢弃。

> [!NOTE]
> 如果源码依赖过旧的拼接行为 —— 故意带前导 `/` 写的相对路径，比如 `Path(Game, "/Textures/T_X")` —— 现在它解析为 `/Textures/T_X.T_X` 并加载失败。去掉前导的 `/`：`Path(Game, "Textures/T_X")`。

<a id="object-path-completion"></a>

## 对象路径补全

root 和相对路径拼接之后，结果会补全为完整对象路径：如果最后一个 `/` 之后的文本不含 `.`，就在末尾追加 `.` 加资产名。

| 写法 | 解析结果 |
| :-- | :-- |
| `Path(Game, "Textures/T_X")` | `/Game/Textures/T_X.T_X` |
| `Path(Game, "Textures/T_X.T_X")` | `/Game/Textures/T_X.T_X` |
| `"/Game/Textures/T_X"` | `/Game/Textures/T_X.T_X` |
| `Texture2D'/Game/Textures/T_X.T_X'` | `/Game/Textures/T_X.T_X` |
| `Path(Game, "Texture2D'/Game/Textures/T_X.T_X'")` | `/Game/Textures/T_X.T_X` —— root 被忽略 |

补全后的路径最后交给 Unreal 自己的对象路径校验器；它失败时，其消息（没有自己的码）被报告在槽位的诊断里。

<a id="two-resolvers"></a>

## 解析器

*（自 2.0.0 起）* 现在只有**一套**解析器。1.x 有两套 —— 纹理默认值一套，元数据和其余一套 —— 接受的形式和消息都不同；现在每个引用位置都走这套共享解析器，由槽位的诊断引用它的码。

| 引用出现的位置 | 报告为 |
| :-- | :-- |
| 紧凑纹理 token 或 `TextureObjectParameter` 的 `= <default>` | 解析不了时为 [`DSH8271`](../diagnostics/DSH8xxx.md#dsh8271)，加载不了时为 [`DSH8218`](../diagnostics/DSH8xxx.md#dsh8218) |
| `const` 纹理的 `= <default>`，或 `TextureSampleParameter*` token 的该项 | 节点的 `Texture` 属性：解析不了时为 [`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213)；加载失败是静默的（见下） |
| 对象类型的 [元数据](metadata.md#object-properties) 条目 —— `[Texture=…]`、`[VirtualTexture=…]`…… | `DSH8213` |
| `UE.CollectionParam(Collection = …)` / `UE.CollectionParameter(Asset = …)` | `DSH8213`；命名参数必须在集合中存在（[`DSH8254`](../diagnostics/DSH8xxx.md#dsh8254)） |
| 对象类型的 `Settings` 值 —— `PhysicalMaterial = Path(…)` | [`DSH8215`](../diagnostics/DSH8xxx.md#dsh8215)，引用 `DSH7125` |
| `VirtualFunction` 的 `Options = { Asset = …; }` | 解析不了时为 [`DSH8270`](../diagnostics/DSH8xxx.md#dsh8270)，加载不了时为 [`DSH8219`](../diagnostics/DSH8xxx.md#dsh8219) |

<a id="what-the-resolver-accepts"></a>

### 解析器接受什么

| 形式 | |
| :-- | :-- |
| `Path(root, "path")` | 接受 |
| `Path("/absolute/path")` | 接受 |
| `"/absolute/path"`（裸**引号**） | 接受 |
| `/absolute/path`（裸**无引号**） | 接受。作为纹理声明的默认值时按写下的加载 *（自 2.0.0 起；1.x 的纹理默认值解析器拒绝它）*；但还是加上引号，因为不带引号时它按 token 读取，像 `1Folder` 这样的段是格式错误的数字（[`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105)） |
| `Class'/absolute/path'`，带不带引号都行 | 接受 *（自 1.9.0 起）* |
| 3 个及以上实参的 `Path(…)` | [`DSH8130`](../diagnostics/DSH8xxx.md#dsh8130) *（纹理默认值自 2.0.0 起也是，它以前读完第二个就停）* |
| root **加上**绝对资产路径 | root **被忽略**；绝对路径胜出 |
| 插件名 | 必须原样通过 Unreal 的对象名净化器 |
| 插件 `Content` 目录必须存在 | 检查 |
| 插件内容必须已挂载 | 检查 *（自 UE 5.6 起）* |

纹理声明中根本不是引用的默认值 —— `Texture2D T = Foo;` —— 不再按拼写拒绝，
只是在构建材质时找不到（`DSH8218`）。

<a id="string-escapes"></a>

## 字符串转义

带引号的路径是字符串字面量，由词法分析器读取 *（自 2.0.0 起）*：识别 `\\`、`\"`、`\n`、`\r`、`\t` 和
`\0`，其他 `\X` 报 [`DSH2104`](../diagnostics/DSH2xxx.md#dsh2104) —— 1.x 读作字面的 `X`。因此路径里的反斜杠要写成 `\\`，解析器随后把它折成
`/`。参见[词法元素](../language/lexical.md)。

<a id="load-behaviour"></a>

## 加载行为

解析路径只产出文本。资产能否加载是另一回事，结果取决于槽位：

| 槽位 | 资产加载失败时 |
| :-- | :-- |
| 紧凑纹理 token 或 `TextureObjectParameter` 的默认值 | `DSH8218` |
| `const` 纹理 token 的默认值 | **静默写入 `nullptr`** *（自 2.0.0 起）* |
| 纹理采样 token 的默认值 | **静默写入 `nullptr`**；节点随后取引擎默认纹理 *（自 2.0.0 起）* |
| 名为 `Texture` 或 `TextureObject`、类型为 `UTexture` 子类的元数据对象属性 | **静默写入 `nullptr`，并报告为成功** |
| 其他任何对象属性，包括 `UE.CollectionParam` 集合 | `DSH8213`，引用 [`DSH7138`](../diagnostics/DSH7xxx.md#dsh7138) |
| 对象设置 | `DSH8215`，引用 `DSH7138` |
| `VirtualFunction` 的资产 | `DSH8219` |

> [!WARNING]
> 要当心的正是静默写 null 的情况：`[Texture = Path(Game, "Typo")]` —— 以及自 2.0.0 起还有
> `TextureSampleParameter2D T = Path(Game, "Typo");` —— 不产生任何诊断就生成完毕，让
> 采样器落在一张没人选的纹理上。能解析的路径不等于存在的路径。

<a id="diagnostics"></a>

## 诊断

| 码 | 触发条件 |
| :-- | :-- |
| [`DSH8127`](../diagnostics/DSH8xxx.md#dsh8127) | trim 之后引用为空 |
| [`DSH8128`](../diagnostics/DSH8xxx.md#dsh8128) | 以 `Path(` 开头却不以 `)` 结尾的值 |
| [`DSH8129`](../diagnostics/DSH8xxx.md#dsh8129) | 实参列表里有未闭合的 `"` |
| `DSH8130` | 三个及以上实参 —— 空的 `Path()` 算作一个空实参，报 `DSH8131` |
| [`DSH8131`](../diagnostics/DSH8xxx.md#dsh8131) | 资产路径实参为空 |
| [`DSH8123`](../diagnostics/DSH8xxx.md#dsh8123) | 相对路径却没有 root，包括裸的带引号相对路径 |
| `DSH8126` | root 第一段不属于六种可接受拼写 |
| [`DSH8124`](../diagnostics/DSH8xxx.md#dsh8124) | `Plugin.<Name>` / `Plugins.<Name>` root 的名字为空，或被 Unreal 的对象名净化器改写 |
| [`DSH8125`](../diagnostics/DSH8xxx.md#dsh8125) | 同上，针对 `Plugin/<Name>` / `Plugins/<Name>` root |
| [`DSH8118`](../diagnostics/DSH8xxx.md#dsh8118) | 插件管理器不认识该插件 |
| [`DSH8119`](../diagnostics/DSH8xxx.md#dsh8119) | 插件存在但被禁用 |
| [`DSH8120`](../diagnostics/DSH8xxx.md#dsh8120) | 插件不能包含内容 |
| [`DSH8121`](../diagnostics/DSH8xxx.md#dsh8121) | 插件的 `Content` 文件夹在磁盘上不存在 |
| [`DSH8122`](../diagnostics/DSH8xxx.md#dsh8122) | 插件的内容未挂载 *（自 UE 5.6 起）* |
| [`DSH8132`](../diagnostics/DSH8xxx.md#dsh8132) | 拼接后的路径没有 `/`，或以它结尾 |
| *（无码）* | Unreal 的对象路径校验器拒绝了补全后的路径；它的消息被槽位的诊断引用 |
| `DSH1043` / `DSH1044` | 纹理默认值的 `Class'…'` 写的不是纹理 / 是另一维度的纹理 |
| `DSH1045` | `Class'…'` 写的类与对象属性无关 |
| `DSH2104` | 带引号的路径里有未知转义 |

这些码都包在它所在槽位的诊断里到达你 —— 见[解析器](#two-resolvers)。完整清单见[错误速查](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_Paths")
{
    Properties = {
        // Root + relative path — the canonical form.
        Texture2D A = Path(Game, "Textures/T_White");

        // Quoted root, and extra folder segments carried by the root.
        Texture2D B = Path("Game/Textures", "T_Noise");

        // Engine content.
        TextureCube C = Path(Engine, "EngineResources/DefaultTextureCube");

        // Plugin content: both spellings resolve identically.
        Texture2D D = Path(Plugin.DreamShader, "Textures/T_Probe");
        Texture2D E = Path("Plugins/DreamShader", "Textures/T_Probe");

        // Single-argument and bare quoted forms — both must be absolute.
        Texture2D F = Path("/Game/Textures/T_White");
        Texture2D G = "/Game/Textures/T_White";

        // Pasted straight from the Content Browser's "Copy Reference".
        Texture2D H = /Script/Engine.Texture2D'/Game/Textures/T_White.T_White';
        Texture2D I = "Texture2D'/Game/Textures/T_White.T_White'";

        // An object-valued metadata entry.
        TextureSampleParameter2D Mask [Texture = Path(Game, "Textures/T_Mask")];

        // A UE builtin argument.
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
D  /DreamShader/Textures/T_Probe.T_Probe          (plugin mounted asset path)
E  /DreamShader/Textures/T_Probe.T_Probe
F  /Game/Textures/T_White.T_White
G  /Game/Textures/T_White.T_White
H  /Game/Textures/T_White.T_White
I  /Game/Textures/T_White.T_White
```

引用在它所属的 property 被构建时才解析。这个 `Graph` 只读 `Wind`，所以那些纹理作为未读取的 property 被丢弃（[`DSH4390`](../diagnostics/DSH4xxx.md#dsh4390)，提示），
只有集合会被解析；读取它们，每个都会按上面解析。

<a id="see-also"></a>

## 另见

- [参数](index.md) —— 中枢与决策表
- [紧凑类型 token](compact-types.md) —— 默认值使用此形式的纹理 token
- [参数节点 token](parameter-nodes.md) —— 哪些 token 接受 `= Path(…)`，哪些不接受
- [元数据块](metadata.md) —— 对象类型的元数据条目与静默写 null 的纹理情况
- [UE 内置节点](../builtins/ue.md) —— `UE.CollectionParam` 及其 `Collection` 实参
- [VirtualFunction](../language/virtual-function.md) —— `Options = { Asset = Path(…); }`
- [词法元素](../language/lexical.md) —— 字符串字面量与转义序列
- [资产路径](../generation/asset-paths.md) —— **输出**侧：`Name=` + `Root=` → 包路径
- [错误速查](../diagnostics/index.md) —— 每个码
