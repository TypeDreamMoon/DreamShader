---
title: "HLSL 通道"
description: "HLSL 通道"
---

<a id="hlsl-passes"></a>

# HLSL 通道

> [DreamShader](../index.md) » [自定义通道运行时](index.md) » **HLSL 通道**

`compute` 通道，或没有 `Material` 的 `fullscreen` 通道，会运行 HLSL。代码写在 `.dsp`
本身中——写在通道的 `hlsl { }` 块中，或作为文件 `hlsl { }` 块的一个条目——或写在通过 `Shader =` 为通道命名、
位于项目中任意位置的 `.usf` 中。本页说明该 HLSL 可以使用什么，以及它如何
进入引擎。

| | |
| :-- | :-- |
| 头文件 | `/Plugin/DreamShader/Pass/DreamPass.ush` — 已为你包含；再次包含也无害 |
| 着色器类型 | `FDreamPassCS`（计算）和 `FDreamPassPS`（像素），每个 **槽位** 一个排列 |
| 槽位 | 32 个计算，16 个像素（`DREAMSHADER_PASS_COMPUTE_SLOTS`、`DREAMSHADER_PASS_PIXEL_SLOTS`，可从 Target.cs 用 `GlobalDefinitions` 覆盖） |
| 快照 | `<DShader>/.dreampass/` — 与你的源文件一起 **提交它** |

<a id="where-the-code-is"></a>

## 代码在哪里

每个 HLSL 通道的代码都只存在于一个地方：

| 位置 | 通道如何声明 | `Entry` |
| :-- | :-- | :-- |
| 它自己的块 | 通道中的 `hlsl { ... }` | 完整函数：运行的那个函数，未写入时为 `Main`；入口的语句：无 |
| 文件的块 | 仅 `Entry = <function>;` | 必需：文件 `hlsl { }` 块中的一个函数 |
| 着色器文件 | `Shader = "<file>.usf";` | 必需：该文件中的一个函数 |

一个块要么存放 **完整函数**——即 `.usf` 所存放的内容：函数、`groupshared`、`struct`、`#define`、
`#include`——要么仅存放 **入口的语句**，其函数由编译器写出。究竟属于哪一种，
根据块的顶层来判定：函数定义、`groupshared`、`struct` 或 `cbuffer` 声明，或
`[numthreads(...)]` 会使其成为完整函数；其他任何内容——语句、`if`、`for`、`[unroll]`——会使其成为
入口的语句。将 `.usf` 移入 `.dsp` 就是把它的文本粘贴到一个块中。

<a id="whole-functions"></a>

### 完整函数

```hlsl
pass TileLuma : compute
{
    Injection = BeforePostProcess;
    Dispatch  = TileAverage * 16;
    read  InColor = SceneColor;
    write Result  = TileAverage;

    hlsl
    {
        groupshared float Partial[256];

        [numthreads(16, 16, 1)]
        void Main(uint3 Id : SV_DispatchThreadID, uint3 Group : SV_GroupID, uint Index : SV_GroupIndex)
        {
            const float2 UV = (Id.xy + 0.5) / (ResultSize.xy * 16.0);
            Partial[Index] = dot(DreamPassSample(InColor, UV).rgb, float3(0.2126, 0.7152, 0.0722));
            GroupMemoryBarrierWithGroupSync();
            for (uint Stride = 128; Stride > 0; Stride >>= 1)
            {
                if (Index < Stride)
                {
                    Partial[Index] += Partial[Index + Stride];
                }
                GroupMemoryBarrierWithGroupSync();
            }
            if (Index == 0)
            {
                Result[Group.xy] = Partial[0] / 256.0;
            }
        }
    }
}
```

计算通道的入口前面有 `[numthreads(x, y, z)]`，这就是派发所依据的组大小；
仅当用宏书写时才需要 `Threads = uint3(...)`。

<a id="the-statements-of-the-entry"></a>

### 入口的语句

```hlsl
pass Pulse : fullscreen
{
    Injection = BeforePostProcess;
    read  InColor = SceneColor;
    write Out     = SceneColor;
    param PulseTint = PulseColor;

    hlsl
    {
        const float3 Scene = DreamPassSample(InColor, UV).rgb;
        const float  Depth = CalcSceneDepth(DreamPassSceneUV(UV));
        Out = float4(Scene + PulseTint.rgb * saturate(1.0 - Depth / 4000.0) * DP_Weight, 1.0);
    }
}
```

编译器会在语句周围写出函数，其中包含以下名称：

| 通道 | 函数 | 语句可见的名称 |
| :-- | :-- | :-- |
| `compute` | 来自 `Threads` 的 `[numthreads(...)]`（未写入时为 `uint3(8, 8, 1)`）；超过 `DP_DispatchSize` 的线程会在你的语句之前返回 | `Id`（`SV_DispatchThreadID`）、`GroupId`（`SV_GroupID`）、`LocalId`（`SV_GroupThreadID`）、`LocalIndex`（`SV_GroupIndex`） |
| `fullscreen` | 每个 `write` 一个 `out float4`，以其命名并清零为 0，在 `write` 顺序中为 `SV_Target0`..`3` | `SvPosition`（`SV_POSITION`）、`Pixel`（从视图左上角计起的像素中心）、`UV`（它在视图中的位置，0..1），以及写入 |

`return;` 会提前结束语句（像素通道的输出已经清零）。绑定不能采用这些
名称之一，语句也不能包含函数、`groupshared` 或 `#include`——这些要放在
文件的块中，或者让该块存放完整函数。

<a id="the-files-hlsl-block"></a>

### 文件的 `hlsl` 块

```hlsl
hlsl
{
    // Shared code: compiled into every pass whose code is in the .dsp.
    float GaussianWeight(int Offset, float Spread)
    {
        return exp(-float(Offset * Offset) / (2.0 * Spread * Spread));
    }

    // An entry: compiled only for the passes that name it.
    [numthreads(8, 8, 1)]
    void BlurCS(uint3 Id : SV_DispatchThreadID)
    {
        if (any(Id.xy >= DP_DispatchSize.xy))
        {
            return;
        }
        float4 Sum = 0;
        float Total = 0;
        for (int i = -12; i <= 12; ++i)
        {
            const float W = GaussianWeight(i, SigmaTexels);
            Sum   += W * Source.Load(int3(clamp(int2(Id.xy) + int2(Axis) * i, int2(0, 0), int2(SourceSize.xy) - 1), 0));
            Total += W;
        }
        Result[Id.xy] = Sum / Total;
    }
}

pass BlurH : compute
{
    Entry = BlurCS;
    read  Source = Bright;
    write Result = BlurX;
    param SigmaTexels = Sigma;
    param Axis        = float2(1, 0);
}
```

一个 `.dsp` 拥有一个。通道用 `Entry` 命名的内容是 **入口**；块中的其他所有内容都是 **共享
代码**，会在代码位于 `.dsp` 中的每个通道的代码之前编译。根据通道名称的生成方式（见下文），
有两条规则：

- **共享代码不能使用任何通道的名称**——不能使用 `read`、`write` 或 `param` 名称，也不能使用它们的 `Size` 和
  `UVRect` 名称，甚至不能作为参数或局部变量：这些名称是它所编译到的槽位的 `#define`。
  入口可以使用它们：它只为命名它的那些通道编译，而其中每个通道
  都会绑定它所使用的内容。
- **没有任何东西调用入口。** 在任何其他通道的槽位中它都不存在。将两个入口共享的内容移到
  它自己的函数中。

两者都会在管线编译时检查。通道自己的块也能看到共享代码，并可自由调用其
函数。

<a id="names-a-pass-makes"></a>

## 通道生成的名称

```hlsl
pass Blur : compute
{
    Shader = "BoxBlur.usf";
    Entry  = BlurCS;
    read   Source = Mask;
    write  Result = Blurred;
    param  Radius = BlurRadius;
}
```

名称 `Source`、`Result` 和 `Radius` 之所以存在，是因为通道绑定了它们：在其代码之前，每个绑定
都会按绑定的书写顺序变成一个指向槽位固定参数的 `#define`。

| 绑定 | 定义 | 含义 |
| :-- | :-- | :-- |
| `read X = …`（第 i 个） | `X`、`XSize`、`XUVRect` | `DP_Input<i>`（一个 `Texture2D<float4>`）、其尺寸（xy，以及 zw 中的其倒数）、其图片在纹理内的 UV 矩形 |
| `write Y = …`（第 j 个） | `Y`、`YSize` | 计算：`DP_Output<j>`（一个 `RWTexture2D<float4>`）；像素：仅尺寸——写入 `SV_Target<j>` |
| `param P = …` | `P` | `DP_Params[]` 的一个分量：`float` 是一个，`float2` 是一个对齐的一半，`float3` 是一个新向量的 xyz，`float4` 是一个完整的；`int` 和 `bool` 用 `asint` / `asuint` 读回 |

因为它们是宏，请选择不会与引擎标识符冲突的绑定名称（用 `InMask`，而不是
`Texture`）。`Entry` 同样会被重命名为槽位的固定入口点，因此在其前面的 `[numthreads]`
就是派发所使用的内容。

<a id="the-fixed-parameters"></a>

## 固定参数

| 名称 | |
| :-- | :-- |
| `DP_Input0`…`DP_Input7` | `read` 纹理；未绑定的为黑色 |
| `DP_InputSize[8]`、`DP_InputUVRect[8]` | 尺寸（xy）和逆尺寸（zw）；图片的 UV 矩形（最小 xy，最大 zw）——管线缓冲区为 (0, 0, 1, 1)，分屏中场景颜色为子矩形 |
| `DreamPassInputUV(i, uv)` | 输入 i 的图片上的 `uv`，作为其纹理中的 UV |
| `DreamPassSample(Name, UV)` | 在视图上的 UV 处、其图片内部，对 `read` 名称采样（线性） |
| `DreamPassLoad(Name, Pixel)` | `read` 名称的一个纹素，位于从视图左上角计起的像素处 |
| `DreamPassSceneUV(UV)` | 视图上的 UV，作为场景纹理的 UV，用于 `CalcSceneDepth` 及其余部分；不用于 `BeginView` |
| `DP_PointClamp`、`DP_LinearClamp` | 采样器 |
| `DP_Output0`…`DP_Output3` | 仅计算：`write` UAV |
| `DP_OutputSize[4]` | 输出的尺寸 |
| `DP_Params[16]` | 打包的 `param` 值 |
| `DP_Weight` | 管线在视图中的权重 |
| `DP_ViewRect` | 此视图的输出 0 的矩形（最小 xy，最大 zw）。管线的缓冲区从 0 开始；分屏中一个视图的场景颜色则不是——写入 `Out[DP_ViewRect.xy + Id.xy]` |
| `DP_DispatchSize` | 计算：请求的线程数；一次派发会向上取整到整组，因此超过它要提前返回 |
| `DP_Time` | 世界时间、世界增量时间、真实时间（秒）、帧号 |
| `View`、`SceneTexturesStruct` | 引擎的视图 uniform 缓冲区和场景纹理——`CalcSceneDepth`、`CalcSceneCustomStencil` 以及 `SceneTexturesCommon.ush` 的其余部分可用 |

`DreamPassSample`、`DreamPassLoad` 和 `DreamPassSceneUV` 是宏——`DreamPassSample(InColor, UV)` 会读取
`InColorUVRect`，即该绑定所使用的名称——它们既可在 `.usf` 中工作，也可在块中工作。

在 `BeginView` 处，视图 uniform 缓冲区尚不存在；在那里 pass 中使用 `View` 会产生编译错误，
而 `DP_Time` 会代替计时值。那里的场景纹理是占位符。文件块的共享代码可以使用 `View`——其他 pass 可能会在帧的更晚时间运行——但一个 `BeginView` pass 若调用
这样的函数，会在其槽位被预检查时被拒绝（`DSH8318`）。

<a id="how-the-hlsl-gets-into-the-engine"></a>

## HLSL 如何进入引擎

pass 不能拥有自己的全局 shader 类型：shader 类型会在插件加载时注册。每个 HLSL
pass 在其管线编译时都会被分配一个**槽位**——即 `FDreamPassCS` 或 `FDreamPassPS` 的一种排列：

1. **预检查**会针对项目的 shader 格式，用你的代码编译该槽位。若出现错误，便是
   一个带有文件和行号的 DreamShader 诊断——对于 `.dsp` 中的代码，则是 `.dsp` 的行号——
   并且该管线不会编译。文件块共享代码中的错误只会报告一次，并附带其失败的 pass。
   没有任何入口可达的代码完全不会被编译——引擎会在 shader 编译器读取该槽位之前将其丢弃——
   因此尚无 pass 调用的辅助函数不会被检查，这与 `.usf` 中一样。
2. 代码的**快照**——即 `.usf` 及其通过相对路径包含的每个文件，或者，对于
   `.dsp` 中的代码，则是将该文件中其他 pass 的入口置空后的文件块、其后是该 pass 自身的代码，以及它们通过相对于
   `.dsp` 的路径包含的每个文件——会被写入 `<DShader>/.dreampass/Slots/<C|P><NN>/`，
   并且注册表（`RegistryCompute.ush`、`RegistryPixel.ush`）会把该槽位映射到它。`Registry.json` 会记录
   哪个 pass 拥有哪个槽位。
3. **这两种 shader 类型会在编辑器中重新编译**，该 pass 会在下一帧运行。

全局 shader 是基于快照构建的，永远不会基于你正在编辑的文件。这一点很重要，因为全局 shader 中的
编译错误是致命的：编辑器或 cook 如果直接编译你半成品的代码，就会停止。
有了快照，拉取你的改动的队友会得到通过你预检查的内容，cook 也是如此。
在块上方添加到 `.dsp` 的注释不会移动快照的任何一行：快照不保存
`.dsp` 的行号，因此不会重新编译任何全局 shader。

- 槽位是稳定的：一个 pass 在多次编译之间会保持其槽位；新 pass 会取最低的可用槽位；
  被移除的 pass 会在其管线再次编译时释放其槽位；`dsc pass-registry -Gc` 会释放
  不再存在的管线的槽位。槽位耗尽会是一个编译错误，并指出需要提高的数量。
- 未使用的槽位会编译为空入口，因此排列列表在每个构建中都是相同的——这是
  烘焙构建所要求的。
- 全局 shader 会被烘焙进基础游戏的全局 shader 缓存：**HLSL pass 无法随 DLC 发布。**
- 块结束于与其 `{` 匹配的 `}`；注释、字符串和 `#` 行中的大括号不计入。
  在块中不支持包含大括号的多行宏（行末为 `\`）。

<a id="see-also"></a>

## 另请参阅

- [Custom Pass 管线 —— `.dsp`](../language-v2/passes.md)
- [Custom Pass 运行时](index.md)
- [Custom Pass 示例](../examples/custom-pass.md)
