---
title: 重新生成
description: 一次重建会毁掉什么、什么能留下来，手改检测与三个出口，归属守卫，以及决定要不要重建的构建键。
---

**生成出来的资产是源码的产物，不是文档。** 当源文件被再次编译时，生成的图会被拆掉，然后从 `.dsm` 或 `.dsf` 重建。
下表标为"不能"的每一处手改都会被这次重建毁掉。

把这句话记住，本页剩下的内容基本都能顺推出来。

| 项目 | 说明 |
| :-- | :-- |
| 适用于 | 所有生成出来的 `UMaterial`、`UDreamShaderMaterialInstance`、`UMaterialFunction`、`UMaterialFunctionMaterialLayer`、`UMaterialFunctionMaterialLayerBlend`。`UDreamPassPipeline` 及其 render target 没有图，重建方式不同 （自 2.1.0 起） —— 见 [Custom Pass 管线](#custom-pass-管线) |
| 触发于 | 任何没有被 source hash 跳过的编译 —— 见[构建键](#构建键) |
| 效果 | 生成的图被拆掉并从源码重建 |

**警告：**

**一次会毁掉手改的重建，不会发生。** （自 1.8.0 起） 每次成功生成都会给资产打一个指纹，
而发现指纹对不上的重建会停下来、让你在三个动作里选一个 —— 见下面的[手改检测](#手改检测)。

所以下面这张表描述的，是一次重建对**仍然没被动过**的资产做了什么，以及 **Revert to Source**
对一个被动过的资产做了什么。

<a id="什么能留下来"></a>

## 什么能留下来

| 手改内容 | 能否在重新生成后留存 |
| :-- | :-- |
| 文本 **不以** `DreamShader: ` 开头的注释框 | **能** |
| 文本以 `DreamShader: ` 开头的注释框 | 不能 —— 被删除 |
| 你手动加的节点 | 不能 —— 被删除 |
| 对生成节点做的属性微调 | 不能 —— 节点被删掉并重建 |
| 节点位置 | 不能，除非被 [`Layout`](../language/layout.md) section 钉住 |
| 在编辑器里改过的材质设置 | 不能 —— [重置属性](#重置属性)里的每一项都会恢复默认值，然后重新应用 `Settings` |
| 生成的 ThinCustom 实例上的参数覆盖 | **能**，按名字 （自 1.9.0 起） —— 先清空再放回，见[下面](#生成实例上的参数覆盖) |
| 材质函数上的 `FunctionInput` / `FunctionOutput` pin 身份 | **能** （自 1.3.2 起） |
| 命名 reroute 的变量 GUID | 能 —— 只有在无效时才重新生成 |

<a id="执行顺序"></a>

## 执行顺序

0. 另一个编辑器持有本工程生成资产的写入权时**跳过**（`DSH8209`），source hash 未变时**跳过**（`DSH8237`）；然后
   **资产开着编辑器就拒绝**，再然后**指纹对不上就拒绝** —— 全都在下面任何一步之前，也在任何一步变得不可逆之前。
   见[资产开着编辑器时拒绝重建](#资产开着编辑器时拒绝重建)和[手改检测](#手改检测)。
1. 对目标对象调用 `Modify()`。
2. **清除生成的注释** —— 每一个文本以字面量 `DreamShader: ` 开头的 `UMaterialExpressionComment`。随后读下材质函数的 pin GUID。
3. **摘下整张图**：给资产打快照，清空每一个材质属性输入，清空表达式集合 —— 旧节点保持存活，不被销毁。材质还会清空它的编辑器
   参数缓存。快照打不出来时重建照样进行，但不受保护，并给出警告 `DSH8208`。
4. **把材质重置为默认值** —— 见[重置属性](#重置属性)。材质函数改为重新应用它的 usage、描述和库字段。
5. 应用 `Settings`（材质）。
6. 发射图：参数、`Graph` 语句、每次 `Function` 调用一个 Custom 节点、`Outputs` 绑定。
7. 按工程的 **Graph Layout Style** 布局 —— 每次构建都运行 （自 2.0.0 起）。
8. **提交**：被摘下的旧节点到这时才销毁。
9. 重新编译。

拆除只有一条路径 （自 2.0.0 起）。1.x 在 1200 个表达式以下逐个删节点、以上整体清扫；整张图作为一个整体摘下来之后，
逐节点删除（它的连线扫描是平方复杂度）就没有意义了。

<a id="重建是原子的"></a>

### 重建是原子的

（自 1.8.0 起）

第 1–8 步要么全部生效、要么一条都不生效。旧图是被**摘下来**而不是被销毁的，失败时在编译返回之前把它放回去 ——
节点、连线、渲染状态、材质函数 usage 和 pin GUID 一并回来。**一次失败的编译留下的资产，就是它开始时的那个资产。**

这件事非做不可，是因为**不是所有失败都能提前拦住**。（自 2.0.0 起） 解析、绑定、降级和 IR 校验都在资产被碰之前跑完，
但发射在拆除之后仍有失败的可能：材质拒绝的设置（`DSH8215`）、引擎没有的节点、pin 或属性（`DSH8210`–`DSH8228`）、
一次取消（`DSH8298`）。`1.8.0` 之前，这类失败会留下一个**被清空的**资产：材质如此已经很糟，材质函数更糟 —— 它的调用点是从
活着的资产上读 pin 的，一个坏掉的 `.dsf` 会把每一个调用它的材质一起带走，而且没有 undo（生成资产刻意不是 `RF_Transactional` 的）。

**提示：**

回滚唯一恢复不了的是第 2 步删掉的 `DreamShader: ` 注释框 —— 它们必然删在快照之前，因为先取快照就会捕获到
一批已经消失的注释的引用。它们是生成出来的装饰，下一次成功编译会重新造出来。你自己写的注释从来不带这个前缀，
一开始就不会被删。

<a id="唯一能留下来的手改"></a>

### 唯一能留下来的手改

图上唯一能留下来的手改，是文本不带 DreamShader 前缀的注释框。

| 项目 | 值 |
| :-- | :-- |
| 前缀 | `DreamShader: ` —— 单词、冒号，加一个尾随空格 |
| 比较 | **区分大小写** |
| 效果 | 文本以该前缀开头的注释在重建前被删除；其它注释一概不动 |

`dreamshader: Notes`、`DREAMSHADER: Notes` 和 `DreamShader:Notes`（没有空格）都通不过前缀检测，因此都会 **留下来**。
这是官方支持的、手工标注生成材质的方式。

**提示：**

推论：把生成的注释框从 `DreamShader: Sampling` 改名成 `Sampling`，它就变成永久的了，而下一次重新生成会在它上面再创建
*第二个* 叫 `DreamShader: Sampling` 的框。想让 DreamShader 自己的框保持同步，就别动它们的文本，改源码
[`Layout`](../language/layout.md) section 里的 `Comment(Name=…)` 条目。

<a id="被刻意保留的身份"></a>

### 被刻意保留的身份

有两种身份被刻意保留下来，好让已有的调用点不被打断。

- **材质函数 pin。** 在图被清空之前，每个 `UMaterialExpressionFunctionInput` 和
  `UMaterialExpressionFunctionOutput` 的 `Id` GUID 会按名字缓存起来，然后恢复到同名的新建 pin 上。项目里别处的
  `MaterialFunctionCall` 节点因此能在该函数重新生成后保住连线。（自 1.3.2 起）
- **命名 reroute。** 声明的变量 GUID 只有在现有的那个无效时才重新生成。

因此在源码里重命名一个输入或输出，对它的调用点来说是 **破坏性变更**：旧名字的 GUID 没有可以恢复的目标了。

<a id="生成实例上的参数覆盖"></a>

## 生成实例上的参数覆盖

在 **ThinCustom** backend 下，重新生成仍然会对发出的 `UDreamShaderMaterialInstance` 调用
`ClearParameterValuesEditorOnly()` —— 实例必须和一个参数集合可能已经变了的 base 保持一致。（自 1.9.0 起） 起这不再丢任何东西：
覆盖值在重建之前从实例上读下来，重建之后按**名字和种类**写回去，再更新静态排列，让恢复出来的 static switch 得到对应的 shader map。
这样的实例读作 **Tweaked** 而不是已分歧，重建不会被拒绝。

**警告：**

重建是把图拆掉再从头建，能活过它的只有名字。**在源码里改名或删掉一个参数，它的覆盖值就没了** —— 被丢掉的参数会在一条 `DSH8155`
日志里一次列全。**Adopt Into Source** 仍然拒绝带覆盖值的实例：反编译器写的是 base 的图，没有地方放实例级的覆盖。

不想让任何重建碰到的值，老办法仍然更稳：

- 把值挪进源码，作为 `Properties` 的默认值，这样生成的实例自带它；或者
- 创建一个以生成实例为父级的 **子** `UMaterialInstanceConstant`，在子实例上覆盖。子实例是普通资产，重新生成永远不会碰它；
  而且由于生成实例持有静态排列，子实例共享它的 shader map，不额外增加编译开销。

Material Content Browser 的实例创建操作生成的正是这样一个子实例，位置在 `<parent directory>/<Instance Subfolder>` ——
见[内存材质](ephemeral.md#落盘materialize)。

<a id="手改检测"></a>

## 手改检测

（自 1.8.0 起）

source hash 给编译**读的源码**打指纹，它对那次编译**产出的资产**一无所知。所以一个被你手动改过的资产
看上去仍然是干净的：下一次它的 `.dsm` 一动，重建就把图拆了、重建了，手改没了，没有诊断，也没有 undo。

**输出指纹**是缺的另一半：每次成功生成的末尾，把资产实际持有的内容写成 `DreamShader.OutputDigest`；
下一次重建在清空资产**之前**拿它比一次，回答 source hash 回答不了的那个问题 —— *有人在这里面动过手吗？*

`DreamShader.SourceFile` 决定归属，指纹决定内容是否还是我们的：

| 状态 | 含义 | 对重建的影响 |
| :-- | :-- | :-- |
| **外来（Foreign）** | 没有 `DreamShader.SourceFile` —— 不是 DreamShader 生成的 | [归属守卫](#归属守卫)拒绝，什么都不碰 |
| **未打标（Unstamped）** | 是我们的，但没有可比对的指纹 | 正常重建，并补上指纹 |
| **已生成（Generated）** | 是我们的，内容与指纹相符 | 正常重建 |
| **已调参（Tweaked）** （自 1.9.0 起） | 是我们的，内容相符，而 ThinCustom 实例带着参数覆盖 | 正常重建，覆盖值被[放回去](#生成实例上的参数覆盖) |
| **已分歧（Diverged）** | 是我们的，内容与指纹不符 | **拒绝** —— 见下 |

「未打标」覆盖两种无害情况：指纹机制存在之前生成的资产，以及指纹带着不同 schema 标签的资产。标签是指纹格式版本、引擎版本，
加上资产所用每个表达式类的反射布局指纹（形如 `DSD4-5.8-1a2b3c4d`）（自 1.9.0 起），因为指纹遍历的属性集合本来就是引擎的 ——
没有版本，升一次引擎会把整个工程的资产报成"被手改过"；没有布局指纹，一个源码引擎在两次会话之间给某个表达式类加个 pin，
也会让所有用到它的资产落入同样的境地。

参与指纹的类就是生成器放下的那些类，记在旁边的第二个元数据键 `DreamShader.OutputDigestClasses` 里。检查按**这份**清单重算标签，
而不是按图现在的样子 —— 所以你手加的节点（通常是生成图里从没有过的类）会被比较并读作 `Diverged`，而不是挪动标签、
被当成 schema 变化混过去。

**提示：**

**指纹与编辑器语言无关** （自 2.0.0 起）。`BreakMaterialAttributes`、`GetMaterialAttributes` 和 `SetMaterialAttributes` 的输入名
是翻译过的文本，所以到 1.9 为止，英文编辑器打的 layer 指纹在中文编辑器里读作 `Diverged`。现在这些输入按属性本名命名，
格式标签从 `DSD3` 变成 `DSD4` —— 升级之后每个资产都会读作 `Unstamped` 一次：正常重建、重新打标，不拒绝任何东西，
但在那第一次重建之前做的手改不受这道闸门保护。

<a id="什么算手改"></a>

### 什么算手改

规则是**指纹恰好覆盖一次重建会摧毁的东西**。重建本来就不碰的属性，能原样活过重建，因此拿它挡住一次重建，
等于为了保护一个从来没有危险的东西而白白拒掉一次编译。

| 改动 | 算分歧吗 |
| :-- | :-- |
| 加、删或重连一个节点 | **算** |
| 改生成节点上的属性（包括参数的默认值） | **算** |
| `GetMaterialAttributes` 发布哪些属性、`SetMaterialAttributes` 设置哪些属性 | **算**（Get （自 2.0.0 起）） |
| 重连一个材质属性输入（`Outputs` 绑定） | **算** |
| [重置属性](#重置属性)里的任何一项 | **算** |
| 材质函数的 `Description`、`UserExposedCaption`、`ExposeToLibrary`、`LibraryCategories`、usage | **算** |
| 实例的父级，或隐藏 base 材质的图 | **算** |
| 生成的 ThinCustom 实例上的参数覆盖 | 不算 —— 改读作 `Tweaked`，重建会[放回去](#生成实例上的参数覆盖) （自 1.9.0 起） |
| 生成实例上参数覆盖的 `ExpressionGUID` —— UE 在加载 package 时自己填上 | 不算 （自 2.1.2 起）（到 2.1.1 为止，一个带覆盖值的 `.dsi` 实例第一次重新加载就读作手改，它的重建和 cook 都被拒绝） |
| 把节点拖到别的位置 | 不算 |
| 手加一个注释框（任何文本） | 不算 |
| 节点的注释气泡、折叠状态、预览可见性、`Desc` | 不算 |
| 命名 reroute 的显示颜色 | 不算 |
| 重置列表之外的材质属性 —— 预览网格、缩略图、物理材质 | 不算 |
| pin `Id` GUID 与命名 reroute 的变量 GUID | 不算 —— 这是身份，刻意跨重建保留 |

**提示：**

命名 reroute 的显示颜色是从它自己的对象路径名派生的，而这个名字每次节点被重建都会变。把它排除在外，
既因为它是装饰，也因为留着它会让**同一份未改源码的两次重建互相不一致**。

<a id="拒绝长什么样"></a>

### 拒绝长什么样

生成失败，资产保持你离开时的样子 —— 没有清空，也没有半成品。编译报 `DSH8207`，它的消息里带着资产层 `DSH8115` 的原文：

```text
Asset '/Game/Materials/M_Emissive.M_Emissive' was edited by hand since DreamShader generated it from
'DShader/Materials/M_Emissive.dsm', so it was NOT rebuilt (rebuilding would destroy those edits).
Use the notification, or right-click the asset > DreamShader, and choose one: Revert to Source,
Adopt Into Source, or Detach From DreamShader.
```

（`1.9.0` 之前这条消息有四句，在日志里逐个解释三个出口；现在是两句，解释挪到了本页和下面通知按钮的 tooltip 里。）

检查跑在任何东西改动资产**之前**。对材质函数来说这个顺序就是全部意义所在：闸门在 usage 被重新打标之前就开火，
而不只是在拆图之前。

这道闸门和上面的**原子重建**回答的是两个不同的问题，而且两个你都想要：闸门拦住一次*本来会成功*的重建覆盖掉你手写的
东西；原子性拦住一次*失败*的重建把资产留成碎片。谁也盖不住谁的场景。

**警告：**

`-Force` **过不了这道闸门**。`bForce` 只表示"忽略 source hash"，而 *Recompile DSM*、Material Content Browser 的 Compile、
*Materialize* 和 cook 都会置它 —— 认它就等于在整个工程重建时让闸门失效。只有 **Revert** 能覆盖一次分歧判定，
因为只有人能做这个决定。

不强制的编译遇到没变的源码会先被 source hash 跳过，轮不到闸门；所以「手改了资产但没动它的源码」这件事什么都不会报，
直到源码变了或者一次强制编译碰到它 —— 在那之前没有任何东西处于危险中。

<a id="通知"></a>

### 通知

（自 1.9.0 起）

拒绝还会在编辑器右下角弹一条通知，按钮就是答案本身。它写出资产和源文件，并且**不会自动消失** —— 这是一个问题，
自己淡出的问题等于没回答过。

| 按钮 | 做什么 |
| :-- | :-- |
| **Revert to Source** | 丢掉手改并重建 |
| **Adopt Into Source** | 用资产反写源码 |
| **Detach** | 不再管理这个资产 |
| **Show Ephemeral Materials** | 在 Content Browser 里显出这个资产 —— 只在它是隐藏的 [Ephemeral](ephemeral.md) 实例时出现 |
| **Dismiss** | 关掉通知，什么都不改 |

前三个跑的就是右键菜单的同一份代码，所以照样先弹确认框、照样关掉再打开开着的资产编辑器、照样在之后重编。除 *Show Ephemeral
Materials* 外都会关掉通知 —— 那个按钮的用途是让你去**看**一个原本看不见的资产，这时候把三个答案收走就没意义了。第四个按钮正是
这条通知存在的原因：编辑器默认的 Ephemeral 状态下，生成的 ThinCustom 实例在 Content Browser 里根本没有图块，"右键这个资产"
指的东西不在屏幕上。

| 规则 | 效果 |
| :-- | :-- |
| 每个资产每**轮**一条 | 一个源文件报同一个资产两次，或两个源文件碰到它，仍然只有一条 |
| 第一条还在屏幕上时不弹第二条 | 反复保存一个坏源文件不会叠出一摞 |
| 一轮超过 **5** 个资产 | 其余的合成一条汇总，带 **Open Material Browser** |
| 新的一轮 | 仍然分歧的资产会再报一次 |

一*轮*就是你要求的一批工作：一次保存（连同防抖窗口里扫到的一切）、*Recompile DSM* / *Clean Generated Shaders* 背后的一次全工程扫描，
或者从 Material Content Browser 或 bridge 单独要求的一次编译。所以一个有五十个手改资产的工程做一次全量重编，最多弹六条通知。
无头运行从不弹通知 —— commandlet、cook 和任何 `-unattended` 运行只有日志和诊断。

<a id="三个出口"></a>

### 三个出口

三个都在资产右键菜单 **DreamShader** 下的 **Generated Asset** 一节里，材质、材质函数、layer、layer blend 和生成的 ThinCustom 实例
一视同仁 —— 自 `1.9.0` 起也在拒绝弹出的[通知](#通知)上。

| 动作 | 做什么 |
| :-- | :-- |
| **Revert to Source** | 丢掉手改、按源码重建。源文件不动。这是「源码才是对的」那个答案。已保存的资产重建后保存，纯内存的资产在内存里重建 —— 反过来做会把手改留在磁盘上、还报告成功，下个会话再把同一份分歧读回来。 |
| **Adopt Into Source** | 用资产当前的内容反写 `.dsm` / `.dsf`，让手改成为真相来源，然后重新编译让两边一致。这是「资产才是对的」那个答案。 |
| **Detach From DreamShader** | 丢掉全部 `DreamShader.*` 标记。资产保留内容，变成一个 DreamShader 永远不会再重建的普通手写资产。之后它就是「外来」的，所以编译原本拥有它的源码会撞上归属守卫，直到你给两者之一改名或挪位置。要保存资产才能留住这次改动 —— detach 只改内存。 |

**Adopt Into Source** 的细节：写任何东西之前，旧源码先被复制成 `<source>.bak`；文件自身的形态 —— 手写注释、`import` 指令、
排版 —— 会被替换成反编译器的输出。两种情况下它**拒绝执行**，理由相同：adopt 用一个资产重写整个文件，文件里说了而资产里没有的东西就没了。

| 拒绝条件 | 原因 |
| :-- | :-- |
| 源文件声明了不止一个资产 | [反编译器](../tools/decompiler.md)发射的是一个块而不是一个翻译单元，从这种文件里 adopt 一个资产会静默删掉其余的 |
| 源文件里有[预处理指令](../language/lexical.md#预处理器) —— `DSH8149` （自 1.9.0 起） | 生成资产只持有被选中的那个分支，用它重写源码会抹掉文件里每一个 `#if`，把碰巧最后构建的那套配置冻结下来。任何指令都算，只有 `#define` 的文件也算；`#Region` 不是预处理指令，不受影响 |

两种情况都请用 **Export .dss**（或 **Export Legacy .dsm**）导出、再手工合并到正确的分支。`.dss` / `.dsi` / `.dsp` 的 Adopt
是逐值拼接进原文件、保留注释和顺序的 （自 2.0.0 起），见 [DreamShaderLang 2.0](../language-v2/index.md)。

（自 2.0.0 起） 一个 `Tweaked` 的 ThinCustom 实例还多两个动作：**Adopt Tweaks as Source Defaults** 把它的覆盖值写进 `.dss`
里对应 `uniform` 的默认值（只对可写源码根下的 `.dss` 可用；`bool` / `int` / `uint` 装不下的标量值是 `DSH9109`），
**Extract Tweaks to .dsi** 把覆盖值写进一个以这个材质为父的新 `.dsi`。两者都随后清掉实例上的覆盖。见
[编辑器工具](../tools/editor-integration.md#generated-asset-一节)。

<a id="资产开着编辑器时拒绝重建"></a>

## 资产开着编辑器时拒绝重建

（自 1.8.0 起）

资产开在某个资产编辑器里时，重建被直接拒绝。编译报 `DSH8206`，消息里带着资产层 `DSH8101` 的原文：

```text
Asset '{ObjectPath}' is open in an asset editor, so it was NOT rebuilt. An open editor works on its
own copy of the asset and writes that copy back when you press Apply or Save, which would silently
undo this rebuild. Close the editor and compile again.
```

原因是**资产编辑器并不编辑资产本身**。`FMaterialEditor` 把资产复制成一个临时的 `UPreviewMaterial`，
在 Apply 或 Save 时用 `UpdateOriginalMaterial` 把副本盖回原件；材质实例编辑器则通过
`UMaterialEditorInstanceConstant` 包装写回。一个跨越了重建仍然开着的编辑器，手里握的是**重建之前**的副本，
下一次 Apply 会把重建做的一切还原 —— 然后以一次[手改检测](#手改检测)的形式出现在再下一次编译里，离病因已经很远。

**提示：**

拒绝是这里唯一安全的答案。那个副本里有没有未 Apply 的改动，是 `FMaterialEditor::bMaterialDirty`，
它对 MaterialEditor 模块是私有的 —— 所以「干净就关掉、脏就拒绝」不是这个插件问得出来的问题。
盲目关掉更糟：引擎的保存提示会在一次「保存即编译」的中途弹出来，还会把无头构建挂死。

**两个来历动作是例外。** *Revert to Source* 和 *Adopt Into Source* 自己关掉编辑器、动手、再打开 ——
因为你刚刚点了它们，很可能就是从那个编辑器自己的工具栏上点的，而一个在最有用的地方永远是灰的菜单项不是保护，
是 bug。关闭过程中可能出现引擎的保存提示；对 Adopt 来说它是承重的而不是噪音，因为未 Apply 的编辑器改动
在这个提示被回答之前，不算「这个资产当前的内容」。取消它就取消整个动作。

<a id="归属守卫"></a>

## 归属守卫

DreamShader 拒绝覆盖不是自己生成的资产。

| 项目 | 说明 |
| :-- | :-- |
| 触发条件 | 目标 package 存在于 **磁盘上**，且已有对象 **没有** `DreamShader.SourceFile` 元数据 |
| 适用于 | 每一种 backend、每一种块（ThinCustom 实例路径自 `1.8.0` 起） |
| 结果 | 生成失败；已有资产原封不动 |

| 码 | 针对 |
| :-- | :-- |
| `DSH8103`，包在 `DSH8201` 里 | `Graph` backend 的材质 |
| `DSH8107`，包在 `DSH8203` 里 | ThinCustom 实例 |
| `DSH8112`，包在 `DSH8202` 里 | 材质函数、layer 或 blend |
| `DSH8242`，包在 `DSH8240` 里 | `.dsi` 材质实例 |

对不在磁盘上的 package，这个守卫是不生效的：纯内存资产没有已保存的 package 需要保护，所以检查不会执行。
这个缺口由另一侧补上 —— 手改检测在内存里同样生效，因为源码路径在那边也会被打上。

目标路径上放着类不对的资产，是在问归属之前就被一次类检查挡掉的：`Graph` 材质是 `DSH8102`，ThinCustom 实例是 `DSH8106`
（通常是切换了源文件的 backend 之后），函数类是 `DSH8110` / `DSH8111`。见[资产路径](asset-paths.md#诊断)。

**提示：**

`1.8.0` 之前，**ThinCustom 实例路径只检查类、不检查来历** —— 于是往一个放着手写
`UDreamShaderMaterialInstance` 的路径上生成，会直接接管并重建它，顺手清空它的参数覆盖。ThinCustom 是默认
backend，这是三条创建路径里覆盖面最广、却唯一没有这个检查的一条。现在它跑的守卫和另外两条一样。

<a id="重置属性"></a>

## 重置属性

在图被重建之前，材质的渲染状态会按下表顺序恢复成这些值。`Settings` 在之后才应用，所以你声明的键会赢；
你 **没有** 声明的项则一律回到下表的值，不管材质编辑器里之前是什么。

| 属性 | 重置为 |
| :-- | :-- |
| `BlendMode` | `BLEND_Opaque` |
| `MaterialDomain` | `MD_Surface` |
| shading model | `MSM_DefaultLit` |
| `TwoSided` | `false` |
| `bUseMaterialAttributes` （自 1.9.0 起） | `false` |
| `OpacityMaskClipValue` | `0.3333` |
| `Wireframe` | `false` |
| `DitheredLODTransition` | `false` |
| `DitherOpacityMask` | `false` |
| `bAllowNegativeEmissiveColor` | `false` |
| `bCastDynamicShadowAsMasked` | `false` |
| `bCastRayTracedShadows` | `true` |
| `bEnableResponsiveAA` | `false` |
| `bScreenSpaceReflections` | `false` |
| `bContactShadows` | `false` |
| `bDisableDepthTest` | `false` |
| `bOutputTranslucentVelocity` | `false` |
| `bWriteOnlyAlpha` | `false` |
| `BlendableOutputAlpha` | `false` |
| `TranslucencyLightingMode` | `TLM_VolumetricNonDirectional` |
| `bTangentSpaceNormal` | `true` |
| `bAlwaysEvaluateWorldPositionOffset` | `false` |
| `bFullyRough` | `false` |
| `bIsSky` | `false` |
| `bIsThinSurface` | `false` |
| `MaterialDecalResponse` | `MDR_ColorNormalRoughness` |
| `bHasPixelAnimation` （自 UE 5.4 起） | `false` |
| `NumCustomizedUVs` | `0` |
| `bUsedWithVolumetricCloud` （自 1.8.0 起） | `Domain="Volume"` 时为 `true`，否则 `false` |

`bUseMaterialAttributes` 由 `Base.MaterialAttributes` 绑定设上，`1.9.0` 之前重置不清它 —— 于是从 `Base.MaterialAttributes` 改成
`Base.FrontMaterial` 加各个根 pin 的源文件（正是 `#if DS_SUBSTRATE` 分支的形状），重建之后标志仍然开着，引擎就从没连线的
MaterialAttributes 输入编每一个根 pin，各个单独的绑定全被忽略。

`bUsedWithVolumetricCloud` 是上表里唯一**不是常量**的一项：由 domain 决定。`Volume` domain 的材质就是体积
材质，而体积云渲染器会拒绝一个没打这个标志的材质 —— 所以你永远不需要手写它。显式写出来可以覆盖 domain 的选择：
一个只喂体积雾的 `Volume` 材质可以用 `bUsedWithVolumetricCloud = "false";` 把标志关掉，省下云的 shader 排列。

材质函数没有渲染状态；重新应用的是它们的资产级字段：

| 源码设置 | 字段 | 未声明时 |
| :-- | :-- | :-- |
| `Description` | `Description` | 清空 |
| `UserExposedCaption` | 不再应用 （自 2.0.0 起）：这个设置是 `DSH3264`，字段保留原值 | — |
| `ExposeToLibrary` | `bExposeToLibrary` —— 只在同时给了 `LibraryCategories` 时才打开 （自 2.0.0 起） | 设为 `false` |
| `LibraryCategories` | `LibraryCategoriesText` —— 逗号分隔，各项去空白，空项丢弃 | 清空 |

每次重新生成还会按块类型重新打上 material function usage。

<a id="构建键"></a>

## 构建键

重新生成的开销不小，所以未改动的源文件会被完全跳过。

| 项目 | 值 |
| :-- | :-- |
| 被哈希的内容 | 文件以及它 import 的每个 `.dsh` 的**预处理后**文本，加上编译它的上下文 （自 1.8.0 起） |
| 算法 | `FCrc::StrCrc32`，按 `%08x` 格式化 —— 八位小写十六进制 |
| 存储位置 | 生成资产的 package 元数据，以资产对象为键 |
| 绕过方式 | 生成入口上的 `bForce` 标记 |

```text
digest text  := for each imported header, in the order the include resolver first read it:
                  "// Begin DreamShader source: <project-relative path>\n" <preprocessed header>
                  "\n// End DreamShader source: <project-relative path>\n\n"
                then the same block for the file itself
build key    := "DSK7|Plugin=<version>|Engine=<major>.<minor>|" <settings> "Defines=<read defines>|"
                "\n--\n" <digest text>
build key    ->  CRC32  ->  "%08x"  ->  DreamShader.SourceHash   e.g. "9f2c41ab"
source path  ->  project-relative, forward slashes  ->  DreamShader.SourceFile
```

<a id="哈希覆盖什么"></a>

### 哈希覆盖什么

哈希覆盖的是编译真正读到的文本，**预处理之后**的，而不是磁盘上文件的字节。（自 2.0.0 起） 解析时不再内联任何东西：
每个被 import 的 `.dsh` 被单独预处理和解析，构建键把每个头文件的文本放进它自己的
`// Begin DreamShader source: <path>` / `// End DreamShader source: <path>` 块里哈希，然后是本文件（含它的 `import` 行）。
被两条 import 都碰到的头文件只哈希一次。

| 改动 | 会改变哪些文件的哈希 |
| :-- | :-- |
| 编辑 `M_Foo.dsm` | `M_Foo.dsm` |
| 编辑被 `M_Foo.dsm` 和 `M_Bar.dsm` 导入的 `Common.dsh` | `M_Foo.dsm` 和 `M_Bar.dsm` 两个都变 |
| 把工程挪到别的目录，或在别处检出 | 什么都不变 —— 这些块用相对工程目录的路径给每个文件命名，和 `DreamShader.SourceFile` 一样 （自 2.1.1 起）（2.0.0 – 2.1.0 用的是绝对路径，换个位置检出就全部重建）。工程目录之外的源文件仍用绝对路径 |
| 重命名源文件 | 存下来的路径对不上了，于是什么都不会被跳过 |
| 重排空白或改注释 | 哈希会变 —— 文本是逐字节比较的，不是语义比较 |
| 改 **Default Compiler Backend** | **每一个**源文件 （自 1.8.0 起） |
| 改 shading model / blend mode / domain **映射表** | **每一个**源文件 （自 1.8.0 起） |
| 升级插件，或升级引擎 | **每一个**源文件 （自 1.8.0 起） |
| 改一个[预处理器 define](../language/lexical.md#预处理器) | **读过**这个 define 的每个源文件 （自 1.9.0 起） —— 包括读它时它还没定义的那些 |

<a id="它是一把构建键不只是源码哈希"></a>

### 它是一把构建键，不只是源码哈希

（自 1.8.0 起）

这个 stamp 给源码在**编译它的上下文里**打指纹。任何会改变某份源码产出的东西都被折进来，
因为把它漏掉，就意味着跳过检查会对一个其实已经不是最新的资产回答「还是最新的」：

| 输入 | 为什么在键里 |
| :-- | :-- |
| 文件和它 import 的每个头文件的预处理后文本 | 编译真正的输入 —— 这也是改过的 `.dsh` 不需要额外条目的原因 |
| [Default Compiler Backend](../settings/project.md) | 决定一个 `Shader` 块变成 `UMaterial` 还是一个薄实例 |
| 三张映射表 | 决定 `Settings` 的键解析成什么 |
| 插件版本，加一个手动 bump 的格式标签 —— `DSK7` （自 2.1.2 起）（2.1.1 是 `DSK4`，1.9.0 – 2.1.0 是 `DSK3`） | 升级生成器会让旧生成器写出的东西作废。2.1.2 修正了链式写入、类型转换、数学、嵌套 return、参数默认值和条件表达式写入的降级，所以升级后每个生成资产都重建一次，源码不用动 |
| 引擎版本 | 能生成什么会随它变化（Substrate 就是一例） |
| 这个源文件**读过**的预处理器 define （自 1.9.0 起） | 它们决定源码的哪些分支被编译了 |

define 是被**精确**折进来的：只有文件真正读过的名字、各自当时的值，以及一个显式的 `<undef>` 标记表示读它时它还没定义。
折进整张表会让任何工程碰任何开关都重建全世界；省掉 `<undef>` 标记更糟 —— 那样*新增*一个 define 不会挪动哈希，
源码悄悄走了另一个分支、资产却从不重建。这份折叠按名字区分大小写排序（`TMap` 的遍历顺序不稳定），并且是单射的
（值里可以含 `=` 和 `;`，`-Define=A=1;B=2` 是合法的）。

**提示：**

在此之前键只哈希源码文本，于是改 backend 会让每个已生成的资产都看起来仍是最新的 —— 这正是那一个设置当年
外挂了一次「强制全量重扫」的原因。现在那次强制没有了：受影响的资产各自过不了跳过检查，而**不**受影响的
资产仍然被跳过，不做无谓重建。

改变键的组成会让所有既有 stamp 作废 —— 每个资产重建一次，一次而已，这正是预期效果。

**提示：**

编辑一个 `.dsh` 会让每一个 import 它的源文件失效，但头文件自己永远不生成任何东西 —— 直接交给编译器是 `DSH8296`。
开着 **Auto Compile On Save** 时，保存头文件会把每个直接或间接 import 它的源文件排进队列，那些编译会发现自己的键变了。
否则依赖方在它们自己被编译时重建：保存它们自己、启动扫描或改了构建键覆盖的设置之后的扫描、Material Content Browser 的 Compile、
[commandlet](../tools/commandlet.md)，或者 cook。批次的依赖顺序见[生成流程](index.md#什么会触发编译)。

`.dsp` 的键还覆盖它的编译在文件外读到的东西 —— pass 层名、它引用的材质的构建键和相关事实、它的 `.usf` 及其 include 的文本
（自 2.1.0 起），见 [Custom Pass（.dsp）](../language-v2/passes.md)。

<a id="元数据存在哪"></a>

### 元数据存在哪

有两个键写进生成资产的 **package 元数据**，以资产对象为键。

| 键 | 值 |
| :-- | :-- |
| `DreamShader.SourceFile` | 相对于工程目录的源码路径，使用正斜杠。工程之外的源码保留绝对路径。 |
| `DreamShader.SourceHash` | 八位十六进制的 CRC32。只在非空时写入。 |

存 *工程相对* 路径是刻意的：换台机器检出、或者把工程目录挪个地方，它仍然认得出自己生成的资产 —— [归属守卫](#归属守卫)和
*Make Ephemeral* 读的就是这个键。哈希也跟着它走：构建键用同样的工程相对路径给每个文件命名，所以别处的检出会跳过已是最新的东西。

哪些资产会被打标记：

| 资产 | 打标记 |
| :-- | :-- |
| `UDreamShaderMaterialInstance`（ThinCustom） | **总是** —— Ephemeral 和 Materialized 都打 |
| 隐藏的 `MB_DreamThinBase_*` base | 仅 Materialized |
| `UMaterial`（Graph backend） | **总是**（`1.8.0` 之后的版本起；之前只在持久化模式下） |
| `UMaterialFunction` / layer / layer blend | **总是**（同上） |
| `.dsi` 的 `UMaterialInstanceConstant` | **总是** |

**提示：**

到 1.8.0 为止，纯内存的 Graph 材质或函数只打源码**路径**，好让跳过永远不会在它身上触发。代价是没法说它是不是最新的 ——
Material Content Browser 永远把它们报成 stale。现在每个生成资产在两种状态下都带哈希，而那些*本来就是*"不管怎样都重建"的
入口改为强制越过跳过（见下）。

ThinCustom 实例还额外把源码路径和哈希作为只读 `UPROPERTY` 携带 —— `SourceFilePath` 和 `SourceHash`，分类 `DreamShader` ——
所以不用翻 package 元数据就能在详情面板里看到。`SourceFilePath` 存的是 **完整规范化** 的源码路径，而不是元数据里那种工程相对形式。
除了这两个键，构建还会写 `DreamShader.OutputDigest` 与 `DreamShader.OutputDigestClasses`（[手改检测](#手改检测)），以及有图的资产上的
`DreamShader.SourceSpans` 和 `DreamShader.DecompileHints`；它们都不参与跳过。没有任何地方写生成时间戳。

<a id="什么时候会跳过重新生成"></a>

### 什么时候会跳过重新生成

短路只在 **全部** 条件成立时才触发：

| # | 条件 |
| :-- | :-- |
| 1 | 这次生成调用没有设置 `bForce` |
| 2 | 资产存在，且新算出的哈希非空 |
| 3 | 存下来的 `DreamShader.SourceFile` 存在且非空 |
| 4 | 存下来的源码路径等于正在编译的源码的工程相对路径，**忽略大小写** |
| 5 | 存下来的 `DreamShader.SourceHash` 等于新哈希，**区分大小写** |

跳过报为 info `DSH8237`，产物的结果行是 `Skipped {ObjectPath} from {File}; source hash is unchanged (build key {BuildKey}).`。
比较哈希之前，一次会写盘的构建若遇到另一个编辑器持有本工程生成资产的写入权，会先被跳过（`DSH8209`）。

**保存失败不会被跳过掩盖** （自 2.1.2 起）：package 保存失败会清掉内存里的 source hash，所以再编一次同一个源文件，不加 `-Force`
也会重试生成和保存。源码归属和输出指纹保持不变：保存失败之后做的手改仍然会挡住重建。一批失败会让参与的每个资产都失效，
包括可能已经落盘的那些。

| 资产 | 跳过点 | 附加条件 | 结果行 |
| :-- | :-- | :-- | :-- |
| ThinCustom 材质 | 实例创建或复用之后，隐藏 base 创建 **之前** | — | `Skipped …` |
| `Graph` backend 材质 | 材质创建或复用之后 | — | `Skipped …` |
| 材质函数 | 函数资产创建或复用之后 | 资产的 material function usage 必须已经和块要求的一致 | `Skipped …` （自 2.0.0 起）（1.x 跳过函数时什么都不说） |

把 ThinCustom 的检查放在 base 创建之前，正是跳过之所以便宜的原因：不建 base、不做归属检查、不拆图。
而 usage 不匹配的材质函数 —— 比如一个 `ShaderLayer` 块对应的资产还标着 `Default` —— 即使哈希相同也会重新生成，并顺手纠正 usage。

<a id="强制重建"></a>

### 强制重建

| 路径 | 是否强制 |
| :-- | :-- |
| 保存时自动编译 | **否** —— 哈希短路生效 |
| 编辑器启动扫描 | **否** —— 见[内存材质](ephemeral.md#磁盘上已经存在的资产) |
| 改了 backend、映射表或预处理器 define 之后的同一次扫描 | **否** —— 键覆盖了它们，只有受影响的会重建 |
| *Tools ▸ DreamShader ▸ Recompile DSM*、*Clean Generated Shaders* | 是 —— 经 bridge 以强制方式排队 |
| Bridge 的 `recompile` 请求（`scope: "file"` / `"all"`） | 是 —— 显式请求就是"重建" |
| Material Content Browser 的 Compile / 缩略图刷新 | 是 |
| 实时预览渲染 | 经 bridge 的 `previewMaterial` 是；经预览 WebSocket 只在请求带 `force` 时 |
| *Materialize* 与子实例创建 | 是 |
| Cook | 是 |
| Commandlet `-run=DreamShader` | 仅在带 `-Force` 时；否则报 `Skipped {ObjectPath} from {SourceFile}; source hash is unchanged (build key {BuildKey}).` |

源语言里没有清除已存哈希的办法。要在没有强制入口的情况下强制重建，要么改动源码文本（任何改动，包括空白），要么删掉生成的资产。

<a id="custom-pass-管线"></a>

## Custom Pass 管线

（自 2.1.0 起） `UDreamPassPipeline` 没有图可拆。它的重建把 `.dsp` 拥有的数据写到同一个对象上，然后是导出 buffer 的
render target 和来源标记，再通知运行时 —— 新管线从下一帧开始生效。闸门和上面一样、顺序也一样：另一个编辑器持有写入权（跳过）、
构建键（跳过）、开着资产编辑器（`DSH8206`）、手改检测（`DSH8207`）；一切都在第一次写入之前暂存好，所以失败或取消会让资产和 HLSL
slot 原样不动。管线本来就是要在详情面板里手调的，调过之后读作 `Diverged`，**Adopt Into Source** 会把调整值逐个拼回 `.dsp`。
细节见 [Custom Pass（.dsp）](../language-v2/passes.md)。

<a id="说明"></a>

## 说明

- **重新生成不可撤销**，但它是**原子的** （自 1.8.0 起）：失败会把资产放回重建之前的样子。
  生成的材质实例刻意不是 `RF_Transactional`，因为撤销/重做会让 shader map 失步 —— 所以回滚不能靠事务，只能靠快照；
  这也是手改检测必须是一道*事前*检查的原因。
- 最稳妥的心智模型：把 `.dsm` / `.dsf` 当成资产本身。任何你想保留的东西都应该写在源码里 —— 或者，如果你改的是资产，用
  **Adopt Into Source** 让源码说出资产说的话。
- 被 source hash 跳过的那次重新生成，上面这些一件都不做 —— 资产完全没有被碰。
- 删掉生成的资产再重新编译，效果永远等价于一次强制重新生成，唯一的差别是材质函数的 pin GUID 会丢失，它的调用点会断。
- 这个哈希是 CRC32，不是密码学摘要。它用来检测改动，不是完整性机制。
- 2.0 起没有要同步的 `.ush` include：`Function` 的 HLSL 写在资产里每次调用的 Custom 节点中，没变的源码连同它一起被跳过。
- [反编译导出](../tools/decompiler.md)是把手改捞回来的正道：把改过的材质导出回源文件，然后以那份源码为准。

<a id="诊断"></a>

## 诊断

资产层的码经由发射器的码到达编译结果，后者引用前者。

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH8207: '{0}' no longer holds what DreamShader generated into it, so it was not rebuilt. {1}` | 输出指纹对不上 —— 资产被手改过；{1} 是 DSH8115 的原文。-Force 不能绕过。 | 用通知或右键资产 ▸ DreamShader ▸ Generated Asset，在 Revert to Source / Adopt Into Source / Detach From DreamShader 里选一个。只是调了 ThinCustom 实例的参数不算手改。 |
| `DSH8206: '{0}' is open in an asset editor, so it was not rebuilt. {1}` | 目标资产开在某个资产编辑器里；{1} 是 DSH8101 的原文。 | 关掉那个编辑器再编译。Revert / Adopt 会自己关它。 |
| `DSH8103: Asset '%s' already exists and was not generated by DreamShader. Rename your shader or move/delete the existing asset before regenerating.` | 材质上的归属守卫（ThinCustom 实例是 DSH8107，.dsi 实例是 DSH8242）。 | 给 Shader 换个名字，或把手写资产移走 / 删掉。 |
| `DSH8112: Asset '%s' already exists and was not generated by DreamShader. Rename your function or move/delete the existing asset before regenerating.` | 材质函数、layer 或 blend 上的归属守卫。 |  |
| `DSH8229: '{0}' was built but could not be saved. {1}` | 重建成功后 package 保存失败（文件只读、在版本控制里、路径不存在）；{1} 是 DSH8116，ThinCustom 实例与 base 一起保存时是 DSH8117。 | checkout 资产或让文件可写，然后再编一次 —— 不用 -Force，失败的保存已经让跳过失效。 |
| `DSH8208: '{0}' could not be snapshotted before rebuilding it, so a failed rebuild will not be rolled back.` | 资产打不出快照（警告）；重建照样进行，但失败时不会回滚。 |  |
| `DSH8155: rebuilding '%s' from '%s' dropped %d parameter override(s) the rebuilt material no longer declares: %s.` | ThinCustom 重建时，源码里已经不再声明的参数的覆盖值被丢掉（警告）。 | 名字是唯一能活过重建的东西；改名的参数要重新调。 |
| `DSH8149: '{0}' uses conditional compilation, and '{1}' holds only the branch that was taken -- adopting it would write that one branch back over the file and delete the rest. Move the change into the matching branch of the source by hand, or use DreamShader > Detach first if this asset should stop being generated from it.` | 对一个含预处理指令的源文件执行 Adopt Into Source。 |  |
| `DSH8237: '{0}' was left alone: its source hash is unchanged since it was last built.` | 不是错误 —— 短路生效了（info）。 | 加 -Force，或改动源码文本，即可强制重建。 |
| `DSH8209: '{0}' was left alone: another editor owns writing this project's generated assets to disk.` | 同一工程开了第二个编辑器，它不持有写入锁（info）。 | 从持有锁的那个编辑器编译，或关掉另一个。 [详解](../tools/editor-integration.md#一个工程只有一个编辑器持有-bridge) |
| `DSH8296: '{0}' is not a source the compiler builds on its own; it builds '.dss', '.dsi', '.dsp', '.dsm' and '.dsf' files, and a '.dsh' header only through the source that includes it.` | 直接编译了一个 .dsh。 | 编译 import 它的源文件。 |

落在一个背后有文件的资产上的编译走 Materialized 路径，并记
`'{ObjectPath}' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory. …` ——
见[内存材质](ephemeral.md#磁盘上已经存在的资产)。

<a id="完整示例"></a>

## 完整示例

```c
Shader(Name="Docs/M_Regen")
{
    Properties {
        ScalarParameter Intensity = 2.0 [Group="Look"; SortPriority=10];
        VectorParameter Tint      = float4(1.0, 0.4, 0.1, 1.0) [Group="Look"];
    }
    Settings { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs  { vec3 Color; Base.EmissiveColor = Color; }
    Graph    { Color = Tint.rgb * Intensity; }
    Layout   { Node(Var="Color", X=-400, Y=0); }
}
```

手改生成出来的资产，然后再保存一次 `.dsm` —— 这会**先被手改检测拦下来**，下面这张表是你选了 **Revert to Source** 之后发生的事：

```text
before regeneration                              after regeneration
-----------------------------------------------  --------------------------------------------
comment "DreamShader: …" (a generated box)       recreated
comment "Reviewed 2026-07-30"                    KEPT — no DreamShader: prefix
extra Multiply node wired in by hand             deleted
Two Sided ticked in the material editor          reset to false (not declared in Settings)
Intensity override = 5.0 on the instance         KEPT — restored by name after the rebuild
Color node dragged to (900, 400)                 back to (-400, 0), pinned by Layout
```

生成实例上的元数据：

```text
DreamShader.SourceFile   DShader/Docs/M_Regen.dsm
DreamShader.SourceHash   9f2c41ab
```

<a id="继续阅读"></a>

## 继续阅读

- [内存材质](ephemeral.md)
  - [资产路径](asset-paths.md)
  - [Layout 与 #Region](../language/layout.md)
  - [反编译导出](../tools/decompiler.md)
  - [材质 Settings](../settings/material.md)
