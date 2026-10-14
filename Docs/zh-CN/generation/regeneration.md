---
title: "重新生成"
description: "重新生成"
---

<a id="regeneration"></a>

# 重新生成

> [DreamShader](../index.md) » [生成](index.md) » **重新生成**

当一个已经生成的资产的源文件被再次编译时，会发生什么。

| | |
| :-- | :-- |
| 适用于 | 每一个生成的 `UMaterial`、`UDreamShaderMaterialInstance`、`UMaterialFunction`、`UMaterialFunctionMaterialLayer`、`UMaterialFunctionMaterialLayerBlend`。`UDreamPassPipeline` 及其 render target 没有图，重建方式不同 *（自 2.1.0 起）* —— 见 [Custom Pass 管线](#custom-pass-pipelines) |
| 触发于 | 任何没有被 source hash 跳过的编译 —— 见[缓存](caching.md) |
| 效果 | 生成的图被拆掉并从源码重建 |

<a id="summary"></a>

## 概述

**生成出来的资产是源码的产物，不是文档。** 重新生成会从 `.dsm` 或 `.dsf` 重建它，而下表标为无法留存的每一处改动，都会被这次重建毁掉。

> [!IMPORTANT]
> 一次*本会*毁掉手改的重建不会发生。自 `1.8.0` 起，每次成功生成的末尾都会给资产打上指纹，
> 而发现指纹对不上的重建会停下来、让你选择 —— 见[分歧](divergence.md)。因此下表描述的是重建对
> 一个**仍然没被动过**的资产做了什么，以及 **Revert to Source** 动作对一个被动过的资产做了什么。

| 改动 | 能否在重新生成后留存 |
| :-- | :-- |
| 文本 **不以** `DreamShader: ` 开头的注释框 | **能** |
| 文本以 `DreamShader: ` 开头的注释框 | 不能 —— 被删除 |
| 你手动添加的节点 | 不能 —— 被删除 |
| 对生成节点做的属性微调 | 不能 —— 节点被删掉并重建 |
| 节点位置 | 不能，除非被 [`Layout`](../language/layout.md) section 钉住 |
| 在编辑器里改过的材质设置 | 不能 —— [重置属性](#reset-properties)里的每一项都会恢复默认值，然后重新应用 `Settings` |
| 生成的 ThinCustom 实例上的参数覆盖 | **能**，按名字 *（自 1.9.0 起）* —— 先清空再放回，见下面的说明 |
| 材质函数上的 `FunctionInput` / `FunctionOutput` pin 身份 | **能** *（自 1.3.2 起）* |
| 命名 reroute 的变量 GUID | 能 —— 只有在无效时才重新生成 |

<a id="sequence"></a>

## 执行顺序

0. **另一个编辑器持有本工程生成资产的写入权时跳过**
   （[`DSH8209`](../diagnostics/DSH8xxx.md#dsh8209)），或 **source hash 未变时跳过**
   （[`DSH8237`](../diagnostics/DSH8xxx.md#dsh8237)）；然后**资产开着资产编辑器时拒绝**，
   再然后**指纹对不上时拒绝** —— 全都在下面任何一步之前，也在任何一步变得不可逆之前。见
   [资产开着资产编辑器](#open-in-an-asset-editor)和 [分歧](divergence.md)。
1. 对目标对象调用 `Modify()`。
2. **清除生成的注释** —— 每一个文本以字面量 `DreamShader: ` 开头的
   `UMaterialExpressionComment`。随后读下材质函数的 pin GUID。
3. **摘下整张图**：给资产打快照，清空每一个材质属性输入，清空表达式集合 —— 旧节点保持存活，不被销毁。材质还会清空它的编辑器
   参数缓存。快照打不出来时重建照样进行，但不受保护，并给出警告 [`DSH8208`](../diagnostics/DSH8xxx.md#dsh8208)。
4. **把材质重置为默认值** —— 见[重置属性](#reset-properties)。材质函数改为重新应用它的 usage、描述和库字段。
5. 应用 `Settings`（材质）。
6. 发射图：参数、`Graph` 语句、每次 `Function` 调用一个 Custom 节点、
   `Outputs` 绑定。
7. 按工程的 [Graph Layout Style](graph-layout.md#layout-styles) 布局 —— 每次构建都运行
   *（自 2.0.0 起）*。
8. **提交**：被摘下的旧节点到这时才销毁。
9. 重新编译。

*（自 2.0.0 起）* 拆除只有一条路径。1.x 在 1200 个表达式以下逐个删节点、以上整体清扫；把整张图作为一个单位摘下来，
使得逐节点删除 —— 它的连线扫描是平方复杂度 —— 变得没有意义。

**重建是原子的。** 第 1-8 步要么全部生效、要么一条都不生效：旧图是被摘下而不是被销毁的，任何一步失败都会把它放回去 ——
节点、连线、渲染状态、材质函数 usage 和 pin GUID 一并回来 —— 然后编译才返回。一次失败的编译留下的资产，就是它开始时的那个资产。

这件事很重要，因为不是所有失败都能提前拦住。*（自 2.0.0 起）* 解析、绑定、降级和 IR 校验都在资产被碰之前跑完，
但发射在拆除之后仍有失败的可能：材质拒绝的设置
（[`DSH8215`](../diagnostics/DSH8xxx.md#dsh8215)）、引擎没有的节点、pin 或属性
（`DSH8210`–`DSH8228`）、一次取消（[`DSH8298`](../diagnostics/DSH8xxx.md#dsh8298)）。在 `1.8.0` 之前，这类
失败会留下一个**被清空的**资产，这对材质来说已经很糟，对材质函数来说更糟 —— 它的调用点是从活着的资产上读 pin 的：
一个坏掉的 `.dsf` 会把每一个调用它的材质一起带走，而且没有 undo（生成的资产刻意不是
`RF_Transactional` 的）。

> [!NOTE]
> 回滚唯一恢复不了的是 `DreamShader: ` 注释框，它们在第 2 步被删除 —— 必然删在快照之前，因为先取快照
> 就会捕获到一批已经消失的注释的引用。它们是生成出来的装饰，下一次成功编译会重新造出来。你自己写的注释
> 从来不带这个前缀，一开始就不会被删。

<a id="what-survives"></a>

## 什么能留下来

图上唯一能留下来的手改，是文本不带 DreamShader 前缀的注释框。

| | |
| :-- | :-- |
| 前缀 | `DreamShader: ` —— 单词、冒号，加一个尾随空格 |
| 比较 | **区分大小写** |
| 效果 | 文本以该前缀开头的注释在重建前被删除；其它注释一概不动 |

`dreamshader: Notes`、`DREAMSHADER: Notes` 和 `DreamShader:Notes`（没有空格）都通不过前缀检测，
因此都会**留下来**。这是官方支持的、手工标注生成材质的方式。

> [!NOTE]
> 推论：把生成的注释框从 `DreamShader: Sampling` 改名成 `Sampling`，它就变成永久的了，而下一次重新生成会在它上面
> 再创建*第二个*叫 `DreamShader: Sampling` 的框。想让 DreamShader 自己的框保持同步，就别动它们的文本，改源码
> [`Layout`](../language/layout.md) section 里的 `Comment(Name=…)` 条目。

有两种身份被刻意保留下来，好让已有的调用点不被打断：

- **材质函数 pin。** 在图被清空之前，每个 `UMaterialExpressionFunctionInput` 和 `UMaterialExpressionFunctionOutput` 的 `Id` GUID
  会按名字缓存起来，然后恢复到同名的新建 pin 上。项目里别处的 `MaterialFunctionCall` 节点因此能在该函数重新生成后保住连线
  *（自 1.3.2 起）*。
- **命名 reroute。** 声明的变量 GUID 只有在现有的那个无效时才重新生成。

因此在源码里重命名一个输入或输出，对它的调用点来说是破坏性变更：旧名字的 GUID 没有可以恢复的目标了。

<a id="parameter-overrides-on-a-generated-instance"></a>

## 生成实例上的参数覆盖

在 **ThinCustom** 后端下，重新生成仍然会对生成的 `UDreamShaderMaterialInstance` 调用 `ClearParameterValuesEditorOnly()` —— 实例必须与其参数集可能已变化的 base 保持一致。自 `1.9.0` 起，它不会因此丢失任何东西：覆盖值会在重建之前从实例上读出，并在重建之后按名称和种类写回，同时静态排列也会更新，因此被恢复的静态 switch 会生成匹配的 shader map。该实例读作 [`Tweaked`](divergence.md#parameter-overrides-on-a-generated-thincustom-instance)，而不是已分歧，重建也不会被拒绝。

> [!WARNING]
> 在生成器拆掉并重建的图中，只有名称能存活下来。**在源码中重命名或删除一个参数，它的覆盖值就没了** —— 会以 `DSH8155` 日志行的形式报告一次，列出每个被丢弃的参数。[Adopt Into Source](divergence.md#adopt-into-source) 也仍然会拒绝一个携带覆盖值的实例，因为反编译器写出的是 base 的图，没有地方放置它们。

对于你绝不希望重建触碰的值，旧习惯仍然更稳妥。要么

- 将其作为 `Properties` 默认值移入源码，这样生成的实例就会携带它，要么
- 创建一个以生成的实例为父级的**子** `UMaterialInstanceConstant`，并在那里覆盖。这个子级是一个普通资产，重新生成永远不会触碰它，而且由于生成的实例拥有静态排列，子级会共享它的 shader map，不产生额外编译开销。

Material Content Browser 的实例创建操作产生的正是这样一个子级，位于 `<parent directory>/<Instance Subfolder>` —— 见
[Ephemeral materials](ephemeral.md#materializing-to-disk)。

<a id="open-in-an-asset-editor"></a>

## 在资产编辑器中打开

当资产在某个资产编辑器中打开时，重建会被直接拒绝。编译报告
[`DSH8206`](../diagnostics/DSH8xxx.md#dsh8206)，其消息携带资产层的
[`DSH8101`](../diagnostics/DSH8xxx.md#dsh8101) 文本：

```text
Asset '{ObjectPath}' is open in an asset editor, so it was NOT rebuilt. An open editor works on its
own copy of the asset and writes that copy back when you press Apply or Save, which would silently
undo this rebuild. Close the editor and compile again.
```

| | |
| :-- | :-- |
| 触发条件 | `UAssetEditorSubsystem::FindEditorForAsset` 报告目标上打开了编辑器 |
| 适用于 | 材质、材质函数、layer、layer blend 以及生成的 ThinCustom 实例 |
| 自 | `1.8.0` |

原因是**资产编辑器并不编辑资产本身。** `FMaterialEditor` 将它复制成一个临时的 `UPreviewMaterial`，并在 Apply 或 Save 时把该副本盖回原件（`UpdateOriginalMaterial`）；材质实例编辑器则通过一个
`UMaterialEditorInstanceConstant` 包装写回。因此，一个跨越重建仍然开着的编辑器手里握的是资产的**重建之前**副本，下一次 Apply 会把重建做的一切还原 —— 然后以再下一次编译时的[分歧](divergence.md)形式出现，离病因已经很远。

> [!NOTE]
> 拒绝是这里唯一安全的答案。那个副本里有没有未应用的改动，是
> `FMaterialEditor::bMaterialDirty`，它对 MaterialEditor 模块是私有的 —— 所以「干净就关掉、脏就拒绝」不是这个插件问得出来的问题。盲目关掉更糟：引擎的保存提示会在一次「保存即编译」的中途弹出来，还会把无头构建挂死。

**两个来历动作是例外。** *Revert to Source* 和 *Adopt Into Source* 自己关掉编辑器、动手、再打开 —— 因为你刚刚点了它们，很可能就是从那个编辑器自己的工具栏上点的，而一个在最有用的地方永远是灰的菜单项不是保护，是 bug。关闭过程中可能出现引擎的保存提示；对 Adopt 来说它是承重的而不是噪音，因为未应用的编辑器改动在这个提示被回答之前，不算「这个资产当前的内容」。取消它就取消整个动作。

<a id="ownership-guard"></a>

## 归属守卫

DreamShader 拒绝覆盖不是自己生成的资产。

| | |
| :-- | :-- |
| 触发条件 | 目标 package 存在于 **磁盘上**，且已有对象 **没有** `DreamShader.SourceFile` 元数据 |
| 适用于 | 每一种后端、每一种块类型 *（ThinCustom 实例路径自 `1.8.0` 起）* |
| 结果 | 生成失败；已有资产原封不动 |

| 码 | 针对 |
| :-- | :-- |
| [`DSH8103`](../diagnostics/DSH8xxx.md#dsh8103)，包在 `DSH8201` 里 | `Graph` 后端的材质 |
| [`DSH8107`](../diagnostics/DSH8xxx.md#dsh8107)，包在 `DSH8203` 里 | ThinCustom 实例 |
| [`DSH8112`](../diagnostics/DSH8xxx.md#dsh8112)，包在 `DSH8202` 里 | 材质函数、layer 或 blend |
| [`DSH8242`](../diagnostics/DSH8xxx.md#dsh8242)，包在 `DSH8240` 里 | `.dsi` 材质实例 |

<a id="where-the-guard-does-not-apply"></a>

### 守卫不适用的情况

对不在磁盘上的 package，这个守卫是不生效的：纯内存资产没有已保存的 package 需要保护，所以检查不会执行。这个缺口由另一侧补上 —— [分歧](divergence.md)在内存里同样生效，因为源码路径在那边也会被打上。

目标路径上放着类不对的资产，会在问归属之前就被一次类检查挡掉：`Graph` 材质是 [`DSH8102`](../diagnostics/DSH8xxx.md#dsh8102)，ThinCustom 实例是 [`DSH8106`](../diagnostics/DSH8xxx.md#dsh8106) —— 通常是切换了源文件的后端之后的情况 —— 函数类则是 `DSH8110` / `DSH8111`。见
[资产路径](asset-paths.md#diagnostics)。

> [!NOTE]
> 在 `1.8.0` 之前，ThinCustom 实例路径只检查类、不检查来历 —— 于是往一个放着手写 `UDreamShaderMaterialInstance` 的路径上生成，会直接接管并重建它，顺手清空它的参数覆盖。由于默认后端是 ThinCustom，那是三条路径里覆盖面最广、却唯一没有这个检查的一条。现在它跑的守卫和另外两条一样。

<a id="reset-properties"></a>

## 重置属性

在图被重建之前，材质的渲染状态会按下表顺序恢复成这些值。`Settings` 在之后才应用，所以你声明的键会赢；你 **没有** 声明的项则一律回到下表的值，不管材质编辑器里之前是什么。

| 属性 | 重置为 |
| :-- | :-- |
| `BlendMode` | `BLEND_Opaque` |
| `MaterialDomain` | `MD_Surface` |
| shading model | `MSM_DefaultLit` |
| `TwoSided` | `false` |
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
| `bHasPixelAnimation` *(UE 5.4+)* | `false` |
| `NumCustomizedUVs` | `0` |

材质函数没有渲染状态；重新应用的是它们的资产级字段：

| 源码设置 | 字段 | 未声明时 |
| :-- | :-- | :-- |
| `Description` | `Description` | 清空 |
| `UserExposedCaption` | 不再应用 *(自 2.0.0 起)*：该设置是 [`DSH3264`](../diagnostics/DSH3xxx.md#dsh3264)，字段保留原值 | — |
| `ExposeToLibrary` | `bExposeToLibrary` — 仅在同时给出 `LibraryCategories` 时打开 *(自 2.0.0 起)* | 设为 `false` |
| `LibraryCategories` | `LibraryCategoriesText` — 逗号分隔，各项去空白，空项丢弃 | 清空 |

每次重新生成时，材质函数的 usage 也会按块类型重新打上。

<a id="custom-pass-pipelines"></a>

## Custom Pass 管线

*(自 2.1.0 起)* 管线没有图可拆。它的重建会把其 [`.dsp`](../language-v2/passes.md) 拥有的数据写到同一个对象上——顺序、默认注入点、视图、需求、启用参数、参数、缓冲区和 pass——然后是它的导出渲染目标和来源标记，再通知运行时，运行时从下一帧开始运行新管线。

| | |
| :-- | :-- |
| 闸门 | 同上，顺序相同：另一个编辑器持有写入权（跳过）、[构建键](caching.md#custom-pass-pipelines)（跳过）、开在资产编辑器中（`DSH8206`）、[手改检测](divergence.md#a-custom-pass-pipeline)（`DSH8207`） |
| 归属 | 路径上放着另一个类会被拒绝（`DSH8302`），或是一个并非 DreamShader 生成的已保存管线（`DSH8303`） |
| 原子性 | 一切都在首次写入前暂存好：管线数据写到一份临时副本上，HLSL 槽位在内存中规划并[预先检查](../../runtime/hlsl.md#how-the-hlsl-gets-into-the-engine)，每个渲染目标在内存中查找或创建。一次失败的预先检查，或在写入前的取消（`DSH8298`），会让资产、槽位注册表和快照保持原样，上一个版本继续运行 |
| 写入顺序 | 快照和注册表文件；渲染目标的设置；管线数据；编辑器中同步重新编译槽位着色器；标记；对管线及其触及的每个渲染目标各保存一次 |
| 保存失败 | `DSH8311`。槽位已在注册表中；下次编译会保存资产 |

<a id="render-targets"></a>

### 渲染目标

每个带有 `Export = true` 的缓冲区，都在管线文件夹中有一个 `UTextureRenderTarget2D`，[`<Pipeline>_<Buffer>`](asset-paths.md#custom-pass-pipelines)。

| | |
| :-- | :-- |
| 复用 | 原地复用：一个读取该缓冲区的材质会把那个对象本身列为其纹理之一，因此它永远不会被替换。仅当其某项设置发生变化时才会重写它的设置 |
| 设置 | 缓冲区格式对应的渲染目标格式；视图尺寸的缓冲区使用 `Clamp` 寻址，固定尺寸的缓冲区使用 `Wrap`；缓冲区的 `Clear` 作为清除颜色（`Clear = None` 为透明黑）；线性 gamma；不自动生成 mip。新建的为 64 × 64，直到运行时按视图调整尺寸；固定尺寸缓冲区则设为其 `Size` |
| 归属 | 路径上放着另一个类，或是一个并非 DreamShader 创建的已保存渲染目标，会在任何写入之前被拒绝（`DSH8313`） |
| 标记 | `DreamShader.SourceFile` — 仅路径 — 以及 `DreamShader.PassPipeline`，即管线的对象路径 |
| 不再导出 | 已被移除、重命名或不再 `Export = true` 的缓冲区，在管线不带它被保存后失去其渲染目标（`DSH8312`，info）。若还有别的东西引用它——例如一个在旧缓冲区上带有 `UE.DreamPassBuffer` 的材质——则保留，并由 `DSH8310` 指出引用者；在没有任何东西读取它之后手动删除 |

<a id="notes"></a>

## 说明

- **重新生成不可撤销。** 生成的材质实例刻意不是
  `RF_Transactional`，因为撤销/重做会让着色器映射失步。这也是
  [手改检测](divergence.md)闸门是一道*事前*检查的原因：一旦图被清空，就没有东西
  可以回到。
- 最稳妥的心智模型：把 `.dsm` / `.dsf` 当成资产本身。任何你想保留的东西
  都应该写在源码里——或者，如果你改的是资产，用 **Adopt Into Source** 让
  源码说出资产说的话。
- 被源码哈希跳过的重新生成不会做上面任何一件事——资产完全没有被碰。见[缓存](caching.md#when-regeneration-is-skipped)。
- 删掉生成的资产再重新编译，永远等价于一次强制重新生成，除非
  材质函数的 pin GUID 会丢失，其调用点会断。
- [反编译器](../tools/decompiler.md)是把手改捞回来的正道：把改过的
  材质导出回 `.dsm` / `.dsf`，然后以那份为准。

<a id="diagnostics"></a>

## 诊断

资产层的码经由发射器的码到达编译结果，后者引用前者。

| 码 | 触发条件 |
| :-- | :-- |
| `DSH8103`、`DSH8107`、`DSH8112`、`DSH8242` | [归属守卫](#ownership-guard)拒绝了一个并非 DreamShader 生成的已保存资产 |
| `DSH8102`、`DSH8106`、`DSH8110`、`DSH8111` | 路径上放着错误的类——见[资产路径](asset-paths.md#diagnostics) |
| [`DSH8229`](../diagnostics/DSH8xxx.md#dsh8229) | 重建成功后包保存失败；它携带 [`DSH8116`](../diagnostics/DSH8xxx.md#dsh8116)，或对于与 base 一起保存的 ThinCustom 实例为 [`DSH8117`](../diagnostics/DSH8xxx.md#dsh8117) |
| `DSH8207` | [手改检测](divergence.md)闸门，它还会发出一个[通知](divergence.md#what-you-see-since-190) *(自 1.9.0 起)* |
| `DSH8206` | [开编辑器闸门](#open-in-an-asset-editor) |
| `DSH8208` | *(警告)* 资产无法打快照，因此失败的重建不会被回滚 |
| `DSH8155` | *(警告)* 一次 ThinCustom 重建丢弃了源码不再声明的参数覆盖 |

落在一个背后有文件的资产上的编译走 Materialized 路径并记录
`'{ObjectPath}' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory. …`——
见[临时材质](ephemeral.md#when-the-asset-already-exists-on-disk)。

<a id="example"></a>

## 示例

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

手改生成的资产，然后选择 **Revert to Source**——一次普通重新编译会被
[手改检测](divergence.md)闸门拒绝：

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

<a id="see-also"></a>

## 另请参阅

- [缓存](caching.md) — 何时跳过重新生成，以及守卫所用的来源元数据
- [临时材质](ephemeral.md) — 子实例、物化，以及存储如何决定模式
- [图布局](graph-layout.md) — 节点位置为何会移动，以及 `Layout` 如何固定它们
- [布局](../language/layout.md) — 为生成的框命名的 `Comment(Name=…)` 指令
- [着色器设置](../settings/material.md) — 在属性重置后幸存的每一个键
- [函数设置](../../settings/function.md) — 每次重建都会重新应用的材质函数字段
- [ShaderFunction](../language/shader-function.md) — pin 标识，以及重命名输入为何会破坏调用方
- [手改检测](divergence.md) — 当资产不再与所生成的内容匹配时会发生什么
- [反编译器](../tools/decompiler.md) — 把手改捕回源码
- [资产路径](asset-paths.md) — 守卫的同级错误背后的类匹配规则
- [诊断索引](../diagnostics/index.md) — 按阶段列出的每一条消息
