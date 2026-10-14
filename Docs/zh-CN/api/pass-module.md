---
title: "DreamShaderPass"
description: "DreamShaderPass"
---

<a id="dreamshaderpass"></a>

# DreamShaderPass

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderPass**

自定义 Pass 的运行时：[`.dsp`](../language-v2/passes.md) 编译成的资产，决定哪些管线应用到某个视图的依据，以及在其 Pass 于渲染器内运行时使用的场景视图扩展。本页介绍该模块的 C++ 与蓝图接口；这些 Pass 在运行时的行为请参阅[自定义 Pass 运行时](../runtime/index.md)。

| | |
| :-- | :-- |
| 模块 | `DreamShaderPass`（**Runtime**，`PostConfigInit` 加载阶段） |
| 引擎 | 插件支持的每个引擎；渲染部分仅支持 5.8 及更高版本 — 见 [`DREAMSHADER_WITH_CUSTOM_PASS`](#dreamshader_with_custom_pass) |
| 导出宏 | `DREAMSHADERPASS_API` |
| 日志类别 | `LogDreamPass` |
| 公共依赖 | `Core`、`CoreUObject`、`DeveloperSettings`、`DreamShader`、`Engine`、`RenderCore`、`RHI` |
| 私有依赖 | `Projects`、`Renderer` |
| 蓝图类别 | `DreamShader \| Custom Pass` |

<a id="public-headers"></a>

## 公共头文件

| 头文件 | 内容 |
| :-- | :-- |
| `DreamShaderPassModule.h` | `LogDreamPass`；槽位数量；HLSL 槽位注册表与快照所在之处 |
| `DreamPassTypes.h` | 构成管线的枚举与描述符 — 注入点、Pass 种类、缓冲区格式、参数及其值、绑定、网格过滤器 — 以及这些枚举的 `.dsp` 拼写 |
| `DreamPassPipeline.h` | `UDreamPassPipeline`，该资产 |
| `DreamPassSettings.h` | `UDreamPassSettings` — [项目设置](../settings/project.md#dreamshader-custom-pass) |
| `DreamPassSubsystem.h` | `UDreamPassSubsystem`, `IDreamPassActivationSource`, `FDreamPassHandle` |
| `DreamPassBlueprintLibrary.h` | `UDreamPassBlueprintLibrary` |
| `DreamPassVolume.h` | `ADreamPassVolume` |
| `DreamPassComponent.h` | `UDreamPassComponent`, `EDreamPassComponentScope` |
| `DreamPassLayerComponent.h` | `UDreamPassLayerComponent`（"Dream Pass Layers"）— 为其 Actor 的图元提供网格 Pass 通过 `Filter = Layer(...)` 选择的 Pass 层 |
| `Materials/MaterialExpressionDreamPassOutput.h` | `UMaterialExpressionDreamPassOutput` — [`UE.DreamPassOutput`](../builtins/dream-pass.md#uedreampassoutput) |
| `Materials/MaterialExpressionDreamPassBuffer.h` | `UMaterialExpressionDreamPassBuffer` — [`UE.DreamPassBuffer`](../builtins/dream-pass.md#uedreampassbuffer)；其静态 `ResolveExportTarget(Pipeline, Buffer, OutProblem)` 是该节点和 DreamShader 共用的检查 |

<a id="dreamshader_with_custom_pass"></a>

## `DREAMSHADER_WITH_CUSTOM_PASS`

该模块的公共定义，由 `DreamShaderPass.Build.cs` 根据目标引擎设置：5.8 及更高版本为 `1`，更低版本为 `0` — 或者在任何使用 `-ProjectDefine:DREAMSHADER_FORCE_NO_CUSTOM_PASS` 构建的引擎上（用于测试）。所有依赖此模块的模块读取到的都是同一个结果。

- **反射类型在每个引擎上都存在** — 资产、设置、子系统、体积和组件在任何地方都能编译，因此项目在较旧引擎上打开时仍能加载并保存它们。
- **所有与渲染器相关的内容在 `0` 时被编译排除**：场景视图扩展、各 Pass、全局着色器和网格 Pass 着色器。它们不会运行，模块会在启动时于日志中说明一次。

<a id="activating-a-pipeline"></a>

## 激活管线

从 C++ 中，通过世界的子系统：

```cpp
#include "DreamPassSubsystem.h"

UDreamPassSubsystem* Passes = UDreamPassSubsystem::Get(GetWorld());
FDreamPassHandle Handle = Passes->AddPipeline(HighlightPipeline, /*Priority*/ 0.0f, /*Overrides*/ {});
Passes->SetParameter(Handle, TEXT("Thickness"), FDreamPassParameterValue::MakeFloat(3.0f));
Passes->SetWeight(Handle, 0.5f);
// ...
Passes->RemovePipeline(Handle);
```

从蓝图中，相同的调用位于 `UDreamPassBlueprintLibrary` 上，带有世界上下文，另外还有 `SetFloatParameter`、`SetColorParameter`、`SetBoolParameter`、`SetIntParameter`、`SetTextureParameter` 以及 `Make…Value` 辅助函数。

或者在关卡中放置一个来源 — `ADreamPassVolume`、`UDreamPassComponent` — 或者在项目设置中列出该管线。这四种方式如何组合，请参阅[什么使管线生效](../../runtime/index.md#what-makes-a-pipeline-apply)。

<a id="udreampasssubsystem"></a>

## `UDreamPassSubsystem`

游戏、PIE 和编辑器世界的 `UWorldSubsystem`；编辑器预览世界没有。仅限游戏线程。

| 函数 | |
| :-- | :-- |
| `static Get(const UWorld*)` | 世界的子系统，或为空 |
| `AddPipeline(Pipeline, Priority, Overrides, PlayerIndex = -1)` | 在世界中的每个视图 — 或某个本地玩家的视图 — 中运行该管线，直到 `RemovePipeline`；返回其句柄 |
| `RemovePipeline(Handle)` · `IsActive(Handle)` | |
| `SetParameter(Handle, Name, Value)` | 设置或添加一个重写；对于未知句柄或参数，或类型错误的值返回 false |
| `SetWeight(Handle, Weight)` | 0–1 |
| `AddToList(List, Primitive)` · `RemoveFromList` · `ClearList` | 网格 Pass 通过 `Filter = List(Name)` 选择的命名列表 |
| `RegisterSource(Object, Source)` · `UnregisterSource(Object)` | 你自己的[激活来源](#writing-an-activation-source) |
| `ResolveView(Query, OutPipelines)` | 适用于某个视图的每条管线，按执行顺序混合 — 渲染器每帧收到的内容 |
| `DumpState(Ar)` | `DreamPass.Dump` 为此世界打印的内容 |

`FDreamPassHandle` 是一个 `BlueprintType` 结构体；默认的它无效（`IsValid()`）。

<a id="adreampassvolume"></a>

## `ADreamPassVolume`

一个 `AVolume`，在其内部视图中运行其管线，并使用后处理体积的算术。

| 属性 | 默认值 | |
| :-- | :-- | :-- |
| `Pipeline` | 无 | 无：体积不做任何事 |
| `bEnabled` | `true` | |
| `bUnbound` | `false` | 适用于该世界的每个视图，无论其位置在哪；未绑定的体积不参与空间加载 |
| `Priority` | `0` | 其重写在同一条管线的所有激活中的位置：较高者胜出 |
| `BlendRadius` | `100` | 体积外其权重从 `BlendWeight` 淡出到 0 的厘米距离；低于 1 时为硬边缘 |
| `BlendWeight` | `1` | 0–1，在体积内部 |
| `Overrides` | 空 | 参数值；省略的值保留较低优先级赋予它的值 |

`SetEnabled`、`SetBlendWeight` 和 `SetParameter(Name, Value)` 可由蓝图调用；`GetWeightAt(Location)` 是那里的视图将获得的权重。在游戏世界之外，体积遵循其编辑器可见性：在大纲中隐藏它就会将其从编辑器视口中移除。运行时生成的体积没有画刷，并且仅在未绑定时应用。

<a id="udreampasscomponent"></a>

## `UDreamPassComponent`

一个 Actor 组件，从任意 Actor 运行其管线。

| 属性 | 默认值 | |
| :-- | :-- | :-- |
| `Pipeline` | 无 | |
| `bEnabled` | `true` | |
| `Priority` | `0` | |
| `Weight` | `1` | 0–1 |
| `Scope` | `World` | `World`：其世界的每个视图 · `ViewTarget`：视图目标为拥有者的视图 — 透过它观看的玩家以及任何旁观它的玩家 |
| `PlayerIndex` | `-1` | 仅该本地玩家的视图（控制器 id）；`-1`：任意视图，包括编辑器视口 |
| `Overrides` | 空 | |

`SetEnabled`、`SetWeight`、`SetParameter`、`SetFloatParameter` 和 `SetColorParameter` 可由蓝图调用；`SetColorParameter` 设置 `float3` 或 `float4` 参数，取管线声明的那一个。

<a id="writing-an-activation-source"></a>

## 编写激活来源

任何东西都可以通过实现 `IDreamPassActivationSource` 并在应生效时向子系统注册来激活管线：

```cpp
class FMyGameModeSource : public IDreamPassActivationSource
{
public:
    virtual void GatherDreamPassActivations(const FDreamPassViewQuery& View, TArray<FDreamPassActivation>& Out) const override
    {
        if (View.PlayerIndex == 0)   // the first player only
        {
            FDreamPassActivation& Activation = Out.AddDefaulted_GetRef();
            Activation.Pipeline = Pipeline;
            Activation.Weight = Fade;
        }
    }

    UDreamPassPipeline* Pipeline = nullptr;   // keep it referenced elsewhere: the subsystem does not
    float Fade = 1.0f;
};

Subsystem->RegisterSource(Owner, &Source);   // Owner: the UObject whose lifetime bounds the registration
Subsystem->UnregisterSource(Owner);
```

`GatherDreamPassActivations` 每帧都会在游戏线程上针对每个视图调用，因此它应当开销低廉。`FDreamPassViewQuery` 给出视图的位置、本地玩家、视图目标、种类与能力；激活的 `Overrides` 是来源所拥有存储的一个视图，仅在调用期间有效。

<a id="udreampasspipeline"></a>

## `UDreamPassPipeline`

编译后的管线。其字段与 `.dsp`——`Order`、`DefaultInjection`、`Views`、`Requires`、
`EnabledParameter`、`Parameters`、`Buffers`、`Passes`（导出的渲染目标）——相对应，而 `SourceFilePath` 和
`SourceHash` 则说明是哪个源构建了它。要编辑 `.dsp`，而不是资产：编译会覆盖它，而在
细节面板中进行的编辑是一种[分歧](../generation/divergence.md)，需要采纳或丢弃。在
`.dsp` 中编写的通道 HLSL 仅保存在编辑器的数据中——通道上的 `HlslSource` 说明其代码位于何处，`InlineHlsl` 和
`InlineHlslLine` 保存其自身的 `hlsl` 块，而管线的 `SharedHlsl` 则保存该文件的——这是回到文本的方式：
实际运行的是槽位的快照，而 cook 不携带任何 HLSL 文本。

| 成员 | |
| :-- | :-- |
| `GetExportTarget(Buffer)` | 导出的缓冲区被复制到的渲染目标；Blueprint 纯函数 |
| `FindBuffer(Name)` · `FindParameter(Name)` · `FindPassIndex(Name)` | 查找 |
| `Validate(OutProblems)` | 运行时依赖的检查，针对整个资产 |
| `NotifyChanged()` · `OnPipelineChanged` | 编辑之后——编译器在写入资产后调用它，`PostLoad` 和细节面板也会调用：重新检查每个通道，跳过失败的通道并给出指明问题的警告，并广播静态的 `OnPipelineChanged`。渲染器每帧都会重新读取资产 |

<a id="the-hlsl-slot-registry"></a>

## HLSL 槽位注册表

| 函数（`UE::DreamPass`） | 返回值 |
| :-- | :-- |
| `GetUserShaderDirectory()` | `<project source root>/.dreampass`——提交它 |
| `GetUserShaderVirtualDirectory()` | `/DreamPassUser` |
| `GetRegistryFilePath(bCompute)` | `RegistryCompute.ush` 或其中的 `RegistryPixel.ush` |
| `GetSlotDirectory(bCompute, Slot)` · `GetSlotVirtualDirectory` | `Slots/C07`、`Slots/P07`：某个槽位的快照 |
| `GetComputeSlotCount()` · `GetPixelSlotCount()` | `DREAMSHADER_PASS_COMPUTE_SLOTS`（32）、`DREAMSHADER_PASS_PIXEL_SLOTS`（16） |

这些计数可以从 `Target.cs`（`GlobalDefinitions.Add("DREAMSHADER_PASS_COMPUTE_SLOTS=48")`）覆盖；
提高其中一个会重新编译两个全局着色器类型，仅此而已。注册表的编写方式参见
[HLSL 通道](../../runtime/hlsl.md#how-the-hlsl-gets-into-the-engine)。

<a id="see-also"></a>

## 另请参阅

- [Custom Pass 运行时](../runtime/index.md)
- [Custom Pass 管线——`.dsp`](../language-v2/passes.md)
- [HLSL 通道](../../runtime/hlsl.md)
- [Custom Pass 材质节点](../builtins/dream-pass.md)
- [项目设置](../settings/project.md#dreamshader-custom-pass)
