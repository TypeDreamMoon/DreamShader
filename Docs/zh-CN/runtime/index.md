---
title: Custom Pass 运行时
description: 编译好的 .dsp 管线在运行时发生什么 —— 什么让它作用于一个视图、pass 何时运行、缓冲区里装着什么、导出的缓冲区怎么给材质 / 蓝图 / UMG / Niagara 读，以及怎么看它做了什么。需要 UE 5.8。
---

一条编译好的 [`.dsp` 管线](../language-v2/passes.md)在运行时的样子：什么让它作用于一个视图、它的 pass 何时运行、缓冲区里装着什么，以及怎么看它做了什么。

| 项目 | 值 |
| :-- | :-- |
| 模块 | `DreamShaderPass`（Runtime，`PostConfigInit`） |
| 引擎 | **UE 5.8** 及以上，延迟渲染器，桌面 RHI（D3D12、Vulkan、Metal）。5.8 以下模块只带资产类型编译（`DREAMSHADER_WITH_CUSTOM_PASS` 为 `0`），什么都不运行；不支持移动端渲染器 |
| 资产 | `UDreamPassPipeline` |
| 设置 | 工程设置 ▸ DreamPlugin ▸ **DreamShader Custom Pass**（`UDreamPassSettings`），见[工程设置](../settings/project.md#dreamshader-custom-pass) |
| 控制台 | `r.DreamPass.Enable`、`r.DreamPass.DisablePipelines`、`r.DreamPass.Visualize`、`DreamPass.Dump`、`stat DreamPass` |
| 起始版本 | （自 2.1.0 起） |

<a id="什么让管线生效"></a>

## 什么让管线生效

| 来源 | 在哪 | 作用于 |
| :-- | :-- | :-- |
| 工程设置 | `UDreamPassSettings::GlobalPipelines` | 每个世界的每个视图 |
| 体积 | `ADreamPassVolume` | 体积内的视图 —— `bUnbound` 时是所有视图 —— 在体积外 `BlendRadius` 范围内渐入 |
| 组件 | 任意 actor 上的 `UDreamPassComponent` | `Scope = World`：它所在世界的每个视图；`Scope = ViewTarget`：视图目标是它的 owner 的那些视图 |
| API | `UDreamPassSubsystem::AddPipeline` / `RemovePipeline`、`SetParameter`、`SetWeight`；蓝图里同名（分类 `DreamShader \| Custom Pass`） | 世界的每个视图，或某一个本地玩家的视图 |

每个来源给出一条管线、一个优先级、一个权重（0–1）和参数覆盖。对每个视图，一条管线不管被多少来源要求，都只运行**一次**：

- 参数从默认值出发，每个激活的覆盖按它的权重、按**优先级**升序混进来 —— 数值做 lerp；bool、int、纹理在权重过半时切换 ——
  所以优先级最高的胜出，和后处理体积一样；
- 权重取各激活中最大的；权重为 0 时不运行。每个 pass 都拿到它，名为 `DreamPassWeight`（同名的材质标量参数；HLSL 里是 `DP_Weight`），
  体积的渐入渐出就是这样传到效果上的；
- 在同一注入点有 pass 的管线按 **`Order`** 升序运行，再按资产路径；同一条管线的 pass 按声明顺序。优先级和 Order 互不相干。

视图的种类不在管线的 `Views` 里、缺了 `Requires` 要的东西，或者 `Enabled` 参数为 false 时，这条管线不在这个视图里运行。

<a id="蓝图与-c"></a>

## 蓝图与 C++

```cpp
#include "DreamPassSubsystem.h"

UDreamPassSubsystem* Passes = UDreamPassSubsystem::Get(GetWorld());
FDreamPassHandle Handle = Passes->AddPipeline(HighlightPipeline, /*Priority*/ 0.0f, /*Overrides*/ {});
Passes->SetParameter(Handle, TEXT("Thickness"), FDreamPassParameterValue::MakeFloat(3.0f));
Passes->SetWeight(Handle, 0.5f);
// ...
Passes->RemovePipeline(Handle);
```

| 入口 | 用途 |
| :-- | :-- |
| `UDreamPassSubsystem` | 世界子系统（游戏、PIE、编辑器世界），只在游戏线程用：`AddPipeline(Pipeline, Priority, Overrides, PlayerIndex = -1)`、`RemovePipeline`、`IsActive`、`SetParameter`、`SetWeight`；`AddToList` / `RemoveFromList` / `ClearList` 填 mesh pass 的 `List(Name)` |
| `UDreamPassBlueprintLibrary` | 上面这些的蓝图版（带 world context），外加 `SetFloatParameter`、`SetColorParameter`、`SetBoolParameter`、`SetIntParameter`、`SetTextureParameter`、`Make…Value`，以及 `GetExportTarget` |
| `ADreamPassVolume` | `Pipeline`、`bEnabled`、`bUnbound`、`Priority`、`BlendRadius`（cm，默认 100）、`BlendWeight`、`Overrides`；`SetEnabled`、`SetBlendWeight`、`SetParameter` 可在蓝图调用 |
| `UDreamPassComponent` | `Pipeline`、`bEnabled`、`Priority`、`Weight`、`Scope`（`World` / `ViewTarget`）、`PlayerIndex`、`Overrides` |
| `UDreamPassLayerComponent` | 给 actor 的图元分配 pass layer，供 mesh pass 的 `Filter = Layer(...)` 选择；layer 名在工程设置里定义（至多 32 个） |
| `IDreamPassActivationSource` | 自定义激活来源：实现 `GatherDreamPassActivations`，用 `UDreamPassSubsystem::RegisterSource` 注册 |

<a id="pass-何时运行"></a>

## pass 何时运行

每个 pass 在自己的[注入点](../language-v2/passes.md#注入点)运行。底层是引擎的 scene view extension 回调加上它的 post-opaque 渲染委托。
同一个 view family 的所有注入点共用一个 render graph，所以 `BeginView` 建的缓冲区到 `EndOfView` 还在 —— 而另一个 view family
（例如同一帧里渲染的场景捕获）永远看不到它。

视图没有的注入点走不到，它的 pass 记为跳过：

| 注入点 | 走不到的时候 |
| :-- | :-- |
| `AfterBasePass` | 路径追踪 |
| `BeforePostProcess` | 视图不 resolve 场景（大多数场景捕获来源）、调试视图模式 |
| `PostProcess.*` | 视图关了后处理 |
| `PostProcess.TranslucencyAfterDOF` | 没有分开的半透明，或者 DOF 已经把它合成了 |

<a id="内置缓冲区"></a>

## 内置缓冲区

| 名字 | 可读于 | 可写于 |
| :-- | :-- | :-- |
| `SceneColor` | `AfterBasePass` 起 | `AfterBasePass`、`AfterOpaque`、`BeforePostProcess`、`PostProcess.*`（`TranslucencyAfterDOF` 除外）、`EndOfView`（view family 纹理） |
| `SceneDepth` | `BeforeBasePass` 起 | — |
| `CustomDepth` | `AfterOpaque` 起；`r.CustomDepth.Order` 把它排在前面时 `AfterBasePass` 起 | — |
| `CustomStencil` | 只能通过场景纹理读（`SceneTexture` 节点、`CalcSceneCustomStencil`） | — |
| `GBufferA`…`F`、`Velocity` | `AfterBasePass` 起 | `GBufferA`…`F` 在 `AfterBasePass` |
| `Translucency` | `PostProcess.*` | `PostProcess.TranslucencyAfterDOF` |

写场景颜色时写进一张临时纹理，再复制回视图的矩形 —— 在后处理链上则作为链的新颜色交下去 —— 所以 pass 可以一边写场景颜色一边读它。

颜色链被上采样之后，深度、custom depth、GBuffer 和 velocity 仍按渲染分辨率的视图矩形读取 （自 2.1.2 起）：
它们的输入尺寸和 UV 矩形描述的是原来的纹理，`SceneColor` 和 `Translucency` 则用链上各自的输入矩形。

<a id="跨帧的缓冲区"></a>

## 跨帧的缓冲区

缓冲区只有一级 mip。`Mips` 不为 `1` 的源会被拒绝（`DSH7308`），旧资产里这样的缓冲区会带一条警告被跳过 （自 2.1.2 起）。

`History = true` 按视图把缓冲区留到下一帧；`<Name>.Previous` 读上一帧的那份。视图尺寸变了，历史从头开始（黑）；
没有 view state 的视图 —— 既没开 `bCaptureEveryFrame` 也没开 `bAlwaysPersistRenderingState` 的场景捕获、缩略图 —— 根本没有历史：
那里 `.Previous` 读出来是黑，并警告一次。

<a id="导出的缓冲区"></a>

## 导出的缓冲区

`Export = true` 在本帧最后一个写它的 pass 之后，把缓冲区复制进编译器在管线旁边建的渲染目标资产（`<Pipeline>_<Buffer>`）。
普通材质用 [`UE.DreamPassBuffer`](../builtins/dream-pass.md#uedreampassbuffer) 读它；蓝图、UMG 和 Niagara 像用别的渲染目标一样用它
（`UDreamPassBlueprintLibrary::GetExportTarget`）。

计划中的最后一个写者被跳过时，之前一次成功的写在 `EndOfView` 导出；一个写者都没成功时，这一帧不替换导出的副本 （自 2.1.2 起）。

- **只有一个视图写它**：每帧第一个属于第一个本地玩家的游戏视图或编辑器视口。分屏、其他编辑器视口和场景捕获在自己的帧里照样用这个缓冲区，但从不覆盖导出的副本。
- **尺寸**跟随那个视图的输出尺寸乘以缓冲区的 `Scale`（或它的固定尺寸）；尺寸变化会重建渲染目标，所以变化后的第一帧读到的是清过的。
- **材质读到的**，是材质渲染时渲染目标里装的东西：

| 缓冲区最后一个写者运行于 | 不透明材质读到 | 半透明材质读到 | UMG 读到 |
| :-- | :-- | :-- | :-- |
| `BeginView`、`BeforeBasePass` | 本帧 | 本帧 | 本帧 |
| `AfterBasePass`、`AfterOpaque` | **上一帧** | 本帧 | 本帧 |
| `BeforePostProcess` 及之后 | **上一帧** | **上一帧** | 本帧 |

- **不再有人填它时**，它被清成缓冲区的 `Clear` 值（`Clear = None` 时为 0）：在那个视图第一次不带这条管线渲染的帧末 —— 被移除、禁用、淡出视图 ——
  以及世界里根本没有管线能运行、或世界结束时立即清掉。没有这样的视图的帧（窗口最小化、编辑器视口不是实时的）保持原样。
  所以读它的材质不会一直显示一个已经关掉的效果的最后一幅画面。
- **Alpha**：`PostProcess.AfterTonemap` 及之后的画面 alpha 为 0，`copy` 会照样保留。UMG 会把图片乘上纹理的 alpha，
  所以为 UMG 填导出缓冲区的 pass 要在那里写 1 —— [UI 毛玻璃](../examples/custom-pass.md#ui-毛玻璃)示例就是这么做的。

<a id="哪些视图运行管线"></a>

## 哪些视图运行管线

| 视图 | 对应 `Views` 的种类 |
| :-- | :-- |
| 游戏视口（含 PIE），每个本地玩家一个视图 | `Game` |
| 编辑器视口，透视或正交（从不包括它的 hit-proxy pass） | `Editor` |
| 场景捕获，2D 或 cube | `SceneCapture` |
| 平面反射 | `PlanarReflection` |
| 反射捕获 | `ReflectionCapture` |
| 缩略图或其他没有 view state 的渲染 | `Thumbnail` |

<a id="调试"></a>

## 调试

| 工具 | 作用 |
| :-- | :-- |
| `DreamPass.Dump` | 每个世界的来源、列表、layer，以及上一帧运行和跳过了什么、为什么 |
| `r.DreamPass.Visualize <Pipeline>.<Buffer>` | 在每个运行这条管线的视图左下角画出那个缓冲区 |
| `r.DreamPass.Enable 0` | 本次会话关掉所有管线 |
| `r.DreamPass.DisablePipelines CP_A,CP_B` | 关掉这几条 |
| `stat DreamPass` | 运行与失败的 pass 数、graph 搭建时间 |
| GPU 抓帧、Insights | 每个 pass 是 `DreamPass <注入点>` 下名为 `<Pipeline>.<Pass>` 的事件 |

<a id="限制"></a>

## 限制

- **mesh pass 画不了 Nanite 图元**；见 [Pass](../language-v2/passes.md#mesh) 里的 `Nanite =`。
- **只通过动态网格元素绘制的图元** —— CPU 蒙皮的骨骼网格、程序化网格 —— 会被 mesh pass 跳过，并警告一次。GPU 蒙皮的骨骼网格、静态网格、实例化静态网格和地形都能画。
- mesh pass **没有逐实例剔除**：实例化静态网格只要包围盒在视野里，所有实例都画。
- **HLSL pass 不能放进 DLC**：它们的 shader 在基础游戏的全局 shader 缓存里。
- **第一次使用可能卡顿**：pass 的管线状态没有预缓存。
- **带 `UE.DreamPassOutput` 或 `UE.DreamPassBuffer` 的材质不能用新的材质翻译器。**

<a id="接下来"></a>

## 接下来

- [Custom Pass 管线](../language-v2/passes.md) — .dsp 的写法：缓冲区、pass、注入点、参数。
  - [HLSL pass](../builtins/hlsl-library.md) — hlsl 块、.usf、绑定名、DreamPass.ush、槽位与预检。
  - [Custom Pass 材质节点](../builtins/dream-pass.md) — UE.DreamPassOutput 与 UE.DreamPassBuffer。
  - [Custom Pass 示例](../examples/custom-pass.md) — 七条完整管线，连同它们的 .dss。
