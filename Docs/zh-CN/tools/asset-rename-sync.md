---
title: "资产重命名同步"
description: "资产重命名同步"
---

<a id="asset-rename-sync"></a>

# 资产重命名同步

> [DreamShader](../index.md) » [工具](index.md) » **资产重命名同步**

在编辑器中重命名或移动资产时，会重写引用它的 `.dsm` / `.dsf` / `.dsh` 文件，这样源文件引用的就始终是资产的名称，而不是它曾经使用的路径。

| | |
| :-- | :-- |
| 类型 | 编辑器服务，无界面 |
| 触发条件 | `IAssetRegistry::OnAssetRenamed` —— 内容浏览器中的重命名、拖拽移动、*修复重定向器*，或任何资产工具的重命名 |
| 写入内容 | 每个文本发生变化的可写项目源文件，UTF-8 无 BOM；每个文件先备份到 `<file>.bak` |
| 设置 | *资产重命名时同步源引用* —— `bSyncSourceReferencesOnAssetRename`，类别 *Editor*，默认**开启** |
| 起始版本 | `1.9.0` |

在此功能出现之前，重命名一张纹理**会悄无声息地**破坏每一个引用它的源文件。重命名的那一刻并不会出问题；失败会在下一次重新构建时显现——一张无法加载的纹理，而距离引发它的操作已经过去很久。

<a id="what-triggers-it"></a>

## 触发条件

资产注册表通告的每次重命名都会被收集到一个批次中。该批次在**其中最后一次重命名后 0.5 秒**被刷新，因此一次突发只会产生一趟处理，而不是每个资产一趟——对某个内容文件夹执行*修复重定向器*会在单帧内引发数百个事件，若逐个处理，就意味着要对源文件树进行数百次完整扫描。

| 条件 | 行为 |
| :-- | :-- |
| 设置为关闭 | 不收集任何内容，也不写入任何内容；该设置是实时读取的，因此关闭后会在下一次重命名时生效 |
| 此编辑器不拥有[桥接](bridge.md#one-editor-owns-the-bridge) | 不写入任何内容——同一项目上的两个编辑器都会听到这次重命名，两者同时写入会在同一个文件上产生竞争 |
| 新路径与旧路径相同 | 忽略 |
| 重命名发生在命令小程序（commandlet）内，或设置了 `-NoDreamShaderEditorBridge` | 服务永远不会启动 |

<a id="what-it-rewrites"></a>

## 它会重写什么

[资产引用的每一种可接受写法](../parameters/path.md)，外加内容浏览器*复制引用*产生的两种外壳形式。

| 写入形式 | 在 `/Game/Textures/T_X` 重命名为 `T_Y` 之后 |
| :-- | :-- |
| `Path(Game, "Textures/T_X")` | `Path(Game, "Textures/T_Y")` |
| `Path(Game, "Textures/T_X.T_X")` | `Path(Game, "Textures/T_Y.T_Y")` |
| `Path("/Game/Textures/T_X")` | `Path("/Game/Textures/T_Y")` |
| `"/Game/Textures/T_X"` | `"/Game/Textures/T_Y"` |
| `"Texture2D'/Game/Textures/T_X.T_X'"` | `"Texture2D'/Game/Textures/T_Y.T_Y'"` |
| `/Script/Engine.Texture2D'/Game/Textures/T_X.T_X'` | `/Script/Engine.Texture2D'/Game/Textures/T_Y.T_Y'` |

该表有两个性质是至关重要的。

**写入形式会被保留。** 以包路径（`/Game/A/T_X`）写出的引用会以包路径形式返回；以对象路径（`/Game/A/T_X.T_X`）写出的引用会保留 `.Name`，并携带新资产的名称。带根的相对引用保持相对，只有它的第二个参数发生变化——包括两个参数之间的空白。外壳引用的类前缀永远不会被改动：它属于作者，重写它就等于插件在凭空捏造一个类型名称。

**引用按精确相等匹配，绝不按前缀匹配。** 重命名 `/Game/Foo` 不会影响 `/Game/FooBar` 和 `/Game/Foo/T_X`。若在此处使用前缀测试，每当有人重命名某个名称恰好是另一个资产名称开头的资产时，都会损坏相邻资产的引用。

<a id="when-a-rooted-reference-cannot-stay-relative"></a>

### 当带根引用无法保持相对时

`Path(root, "relative")` 只有在资产仍位于该根之下时才能保持其形式。该根的
前缀是通过从旧路径中减去相对文本来还原的，然后用它来测试新路径：

```c
// The asset moved within the root: only the tail changes.
Texture2D A = Path("Game/Textures", "T_X");   ->   Path("Game/Textures", "T_Y")

// The asset moved out of the root: there is no relative spelling left.
Texture2D B = Path("Game/Textures", "T_X");   ->   Path("/Game/Props/T_Y")
```

折叠后的形式始终带引号，因为带引号的绝对路径是**两种**解析器都接受的一种写法——参见[两种解析器](../parameters/path.md#two-resolvers)。

<a id="what-it-never-rewrites"></a>

## 它永远不会重写什么

| 区域 | 原因 |
| :-- | :-- |
| `Function` 和 `GraphFunction` 主体 | 主体是直接交给着色器编译器的原始 HLSL。任何资产路径都不可能合法地存在于其中，而一次误命中会把一个 `/Game/...` 字符串拼接进着色器代码。该扫描使用与[预处理器](../language/preprocessor.md)相同的、能感知大括号、注释和字面量的跟踪器来判断同一问题 |
| `//` 行注释和 `/* */` 块注释 | 注释中的引用不是编译器会读取的引用，且置之不理会意味着该服务永远不会因编辑文字内容而被指责 |
| `Path(…)` 之外且外壳形式之外的未加引号的裸绝对路径 | 资产引用解析器接受它，但它与普通文本无法区分；请写出 `"/Game/…"` 或 `Path("/Game/…")` 以便被跟踪 |
| 跨越换行符的引用 | 匹配是按行进行的 |
| 插件[源根目录](../language/source-files.md#source-roots)下的文件 | 插件按其创作时的样子提供源文件；只有项目根目录是可写的 |
| `DShader/Packages` | 第三方包源文件，被排除在所有编辑器侧扫描之外 |
| 在未拥有桥接的编辑器中，任何内容 | 参见上表 |

> [!NOTE]
> 包含 [`#if` 及类似内容](../language/preprocessor.md)的源文件**会**被重写，这一点与
> [VirtualFunction 同步](virtual-function-tools.md#conditional-sources-are-refused-since-190)不同。后者
> 按字节偏移进行拼接，无法应对经过预处理的字符串；而本服务只是逐行读写原始文件，
> 因此即使引用位于本次构建裁掉的分支中，也会与其余部分一起更新。

<a id="backups"></a>

## 备份

每个发生变化的文件都会在写入**之前**复制到 `<file>.bak`——文件名和机制与
[*采纳进源文件*](../generation/divergence.md#adopt-into-source)所用的相同，因此任何
DreamShader 操作重写过源文件后，都只需到一个地方去查找。

| 失败情况 | 结果 |
| :-- | :-- |
| 备份复制失败 | `DSH9020`，且该文件**不会**被写入 |
| 备份完成之后写入失败 | `DSH9021`；文件原来的样子位于 `.bak` 中 |

> [!WARNING]
> `<file>.bak` 只保留**一代**。第二个触及同一文件的批次会覆盖它，第一次重命名
> 之前的 `.bak` 就不复存在了。它是针对最后一次操作的安全网，而非
> 历史记录。

<a id="order-text-first-rebuild-second"></a>

## 顺序：先文本，后重建

批次中的每个文件都会在任何内容被编译之前先落盘。若在最后一个文件仍命名旧资产时就编译第一个文件，
失败原因等到有人阅读错误信息时早已不复存在，因此该服务会先写入整个批次，然后才请求桥接
进行重建。

重建请求是显式的，其分派方式与源目录监视器完全一致——`.dsm`
文件会被排队，`.dsh` 和 `.dsf` 文件会通过依赖图将其依赖项排队，该依赖图
会为该批次重建一次——但它**不会**遵循*保存时自动编译*。那道关卡是针对你所作编辑的；
插件自身重写过的文件无论如何都会重建，因为让资产与其声称来源的文本处于不一致状态，
正是该服务存在所要防止的失败。监视器会看到相同的写入并排队相同的文件；排队以路径为键，因此不会有任何内容
被编译两次。

<a id="the-log"></a>

## 日志

每个批次在 `LogDreamShader` 中记录一行 `Display`，列出所有被重写的文件、其备份，
以及每次更改背后的旧 → 新配对：

```text
LogDreamShader: Display: DreamShader rewrote 2 source file(s) after 3 asset rename(s):
  C:/Projects/MyGame/DShader/M_Cloud.dsm  (2 reference(s), backup 'C:/Projects/MyGame/DShader/M_Cloud.dsm.bak')
      Path(Game, "Texture/Volume/T_Volume_03")  ->  Path(Game, "Texture/Volume/T_Volume_Cloud")
      /Game/Curves/CV_Ramp  ->  /Game/Curves/CV_Falloff
  C:/Projects/MyGame/DShader/MF_Fog.dsf  (1 reference(s), backup 'C:/Projects/MyGame/DShader/MF_Fog.dsf.bak')
      /Game/Textures/T_Noise.T_Noise  ->  /Game/Textures/T_Noise_01.T_Noise_01
```

当你的编辑器告诉你某个打开的文件在磁盘上发生了变化时，这就是你要寻找的记录。

<a id="limits"></a>

## 限制

- **删除不在范围之内。** 已删除的资产没有可写入的新路径，而由此产生的编译错误正是正确的结果——引用确实已损坏，只有人才能决定应当用什么来替代它。
- **只写入项目源码根目录。** 插件根目录对编辑器是只读的，因此引用了已重命名项目资产的插件源码会在下次编译时被报告，并需手动修复。
- **在 VSCode 中打开的文件会提示重新加载。** 编辑器底下的内容已发生变化。这是预期行为；上面的日志行会准确说明哪些文件被改动。编辑器中的未保存更改*不会*被此服务看到——它直接读写磁盘上的文件——因此在重命名一批资产前请先保存，或者选择编辑器的"保留我的版本"并手动重新应用这些路径。
- **多行引用不会被匹配。** 跨行断开的 `Path(` … `)` 会被原样保留。
- **会跟随重命名，但不会验证资产。** 该服务写入注册表报告的路径。该路径下的资产能否加载，稍后由生成过程决定。

<a id="see-also"></a>

## 另请参阅

- [`Path(…)` 资产引用](../parameters/path.md) — 所有被接受的拼写形式，以及两个解析器
- [项目设置](../settings/project.md) — *Sync Source References On Asset Rename* 及其工具提示
- [Bridge](bridge.md) — 桥接所有权、源目录监视器和编译队列
- [VirtualFunction 工具](virtual-function-tools.md#startup-sync-service) — 另一个就地重写源码的服务，以及它为何拒绝条件文件
- [Divergence](../generation/divergence.md#adopt-into-source) — 此功能复用的 `.bak` 机制
- [源文件](../language/source-files.md#source-roots) — 存在哪些根目录以及哪些可写
- [诊断索引](../diagnostics/index.md) — `DSH9020` 和 `DSH9021`
