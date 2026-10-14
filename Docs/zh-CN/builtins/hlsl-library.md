---
title: HLSL pass
description: compute pass 和不带材质的 fullscreen pass 跑的 HLSL —— 写在 .dsp 的 hlsl 块里还是 .usf 里、绑定生成的名字、DreamPass.ush 的固定参数与辅助函数，以及代码怎样经过预检和快照进入引擎。需要 UE 5.8。
---

`compute` pass，以及不带 `Material` 的 `fullscreen` pass，运行的是 HLSL。代码写在 `.dsp` 本身里 —— pass 的 `hlsl { }` 块，或者文件 `hlsl { }` 块里的一个入口 ——
或者写在 pass 用 `Shader =` 指向的、工程里任意位置的 `.usf` 里。本页讲这段 HLSL 能用什么，以及它怎么进入引擎。

| 项目 | 值 |
| :-- | :-- |
| 头文件 | `/Plugin/DreamShader/Pass/DreamPass.ush` —— 自动 include，再 include 一次也无妨 |
| Shader 类型 | `FDreamPassCS`（compute）与 `FDreamPassPS`（像素），每个**槽位**一个排列 |
| 槽位 | compute 32 个、像素 16 个（`DREAMSHADER_PASS_COMPUTE_SLOTS`、`DREAMSHADER_PASS_PIXEL_SLOTS`，可在 Target.cs 用 `GlobalDefinitions` 覆盖） |
| 快照 | `<DShader>/.dreampass/` —— **和源码一起提交** |
| 起始版本 | （自 2.1.0 起） |

<a id="代码写在哪"></a>

## 代码写在哪

每个 HLSL pass 的代码恰好在一处：

| 位置 | pass 怎么写 | `Entry` |
| :-- | :-- | :-- |
| 自己的块 | pass 里的 `hlsl { ... }` | 整个函数时：要运行的函数，不写就是 `Main`；只有入口语句时：不写 |
| 文件的块 | 只写 `Entry = <函数>;` | 必须：文件 `hlsl { }` 块里的一个函数 |
| shader 文件 | `Shader = "<文件>.usf";` | 必须：文件里的一个函数 |

一个块要么装**整个函数** —— 和 `.usf` 装的一样：函数、`groupshared`、`struct`、`#define`、`#include` —— 要么只装**入口的语句**，函数由编译器来写。
是哪一种看块的顶层：出现函数定义、`groupshared` / `struct` / `cbuffer` 声明或 `[numthreads(...)]` 就是整个函数；
其他情况 —— 语句、`if`、`for`、`[unroll]` —— 就是入口的语句。把一个 `.usf` 搬进 `.dsp`，就是把它的文本贴进一个块。

<a id="整个函数"></a>

### 整个函数

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

compute pass 的入口前面带 `[numthreads(x, y, z)]`，dispatch 就按这个组大小计数；只有组大小用宏写时才需要 `Threads = uint3(...)`。

<a id="入口的语句"></a>

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

编译器把语句包进一个函数，函数里有这些名字：

| Pass | 函数 | 语句能用的名字 |
| :-- | :-- | :-- |
| `compute` | `[numthreads(...)]` 取自 `Threads`（不写时 `uint3(8, 8, 1)`）；超出 `DP_DispatchSize` 的线程在你的语句之前就返回 | `Id`（`SV_DispatchThreadID`）、`GroupId`（`SV_GroupID`）、`LocalId`（`SV_GroupThreadID`）、`LocalIndex`（`SV_GroupIndex`） |
| `fullscreen` | 每个 `write` 一个以它命名、初始清零的 `out float4`，按 `write` 的顺序是 `SV_Target0`..`3` | `SvPosition`（`SV_POSITION`）、`Pixel`（从视图左上角数起的像素中心）、`UV`（在视图里的位置，0..1），以及那些 write |

`return;` 提前结束语句（像素 pass 的输出已经清零）。绑定不能用这些名字；语句里不能有函数、`groupshared` 或 `#include` ——
它们放进文件的块，或者改让这个块装整个函数。

<a id="文件的-hlsl-块"></a>

### 文件的 hlsl 块

```hlsl
hlsl
{
    // 共享代码：编译进每一个代码写在 .dsp 里的 pass。
    float GaussianWeight(int Offset, float Spread)
    {
        return exp(-float(Offset * Offset) / (2.0 * Spread * Spread));
    }

    // 入口：只为点名它的 pass 编译。
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

一个 `.dsp` 只有一个。被 pass 用 `Entry` 点名的是**入口**；块里其余的都是**共享代码**，编译在每个代码写在 `.dsp` 里的 pass 的代码之前。
由 pass 名字的生成方式（见下）得出两条规则：

- **共享代码不用任何 pass 的名字** —— 不用 `read`、`write`、`param` 名，不用它们的 `Size`、`UVRect` 名，连作形参或局部变量都不行：
  这些名字是它被编进的那个槽位的 `#define`。入口可以用：它只为点名它的 pass 编译，而这些 pass 都绑定了它用到的名字。
- **不调用入口。** 在别的 pass 的槽位里它根本不存在。两个入口共用的东西，挪进一个单独的函数。

两条都在编译管线时检查。pass 自己的块也看得见共享代码，可以随意调用其中的函数。

<a id="pass-定义的名字"></a>

## pass 定义的名字

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

`Source`、`Result`、`Radius` 之所以存在，是因为 pass 绑定了它们：在代码之前，每个绑定按书写顺序变成指向槽位固定参数的 `#define`。

| 绑定 | 定义 | 含义 |
| :-- | :-- | :-- |
| 第 i 个 `read X = …` | `X`、`XSize`、`XUVRect` | `DP_Input<i>`（`Texture2D<float4>`）、它的尺寸（xy，zw 是倒数）、它的画面在纹理里的 UV 矩形 |
| 第 j 个 `write Y = …` | `Y`、`YSize` | compute：`DP_Output<j>`（`RWTexture2D<float4>`）；像素：只有尺寸 —— 写 `SV_Target<j>` |
| `param P = …` | `P` | `DP_Params[]` 的一部分：`float` 占一个分量，`float2` 对齐占半个，`float3` 占一个新向量的 xyz，`float4` 占整个；`int`、`bool` 用 `asint` / `asuint` 读回 |

因为它们是宏，绑定名要挑不会和引擎标识符撞车的（`InMask`，而不是 `Texture`）。`Entry` 也同样被改名成槽位固定的入口点，所以它前面的 `[numthreads]` 就是 dispatch 用的组大小。

<a id="固定参数"></a>

## 固定参数

| 名字 | 说明 |
| :-- | :-- |
| `DP_Input0`…`DP_Input7` | `read` 的纹理；没绑定的是黑的 |
| `DP_InputSize[8]`、`DP_InputUVRect[8]` | 尺寸（xy）与尺寸倒数（zw）；画面的 UV 矩形（min xy、max zw）—— 管线自己的缓冲区是 (0, 0, 1, 1)，分屏时的场景颜色是一个子矩形 |
| `DreamPassInputUV(i, uv)` | 第 i 个输入画面上的 `uv`，换算成它纹理里的 UV |
| `DreamPassSample(Name, UV)` | 在视图上的 UV 处、在画面之内，对 `read` 的 Name 线性采样 |
| `DreamPassLoad(Name, Pixel)` | `read` 的 Name 在某像素（从视图左上角数起）处的纹素 |
| `DreamPassSceneUV(UV)` | 视图上的 UV 换成场景纹理的 UV，供 `CalcSceneDepth` 等使用；`BeginView` 不可用 |
| `DP_PointClamp`、`DP_LinearClamp` | 采样器 |
| `DP_Output0`…`DP_Output3` | 仅 compute：`write` 的 UAV |
| `DP_OutputSize[4]` | 输出的尺寸 |
| `DP_Params[16]` | 打包好的 `param` 值 |
| `DP_Weight` | 管线在这个视图里的权重 |
| `DP_ViewRect` | 输出 0 在这个视图里的矩形（min xy、max zw）。管线自己的缓冲区从 0 开始，分屏时某个视图的场景颜色不是 —— 写 `Out[DP_ViewRect.xy + Id.xy]` |
| `DP_DispatchSize` | compute：请求的线程数；dispatch 会向上取整到整组，所以超出它要提前返回 |
| `DP_Time` | 世界时间、世界帧间隔、真实时间（秒）、帧号 |
| `View`、`SceneTexturesStruct` | 引擎的 view uniform buffer 与场景纹理 —— `CalcSceneDepth`、`CalcSceneCustomStencil` 以及 `SceneTexturesCommon.ush` 里的其他函数都能用 |

`DreamPassSample`、`DreamPassLoad`、`DreamPassSceneUV` 是宏 —— `DreamPassSample(InColor, UV)` 读的是绑定生成的 `InColorUVRect` —— 在 `.usf` 和块里都能用。

`BeginView` 时 view uniform buffer 还不存在：在那里的 pass 里用 `View` 是编译错误，计时值用 `DP_Time` 代替；那里的场景纹理只是占位。
文件块里的共享代码可以用 `View` —— 别的 pass 可能在帧里更晚运行 —— 但调用这种函数的 `BeginView` pass 在槽位预检时会被拒绝（`DSH8318`）。

<a id="hlsl-怎么进入引擎"></a>

## HLSL 怎么进入引擎

pass 不能有自己的全局 shader 类型：shader 类型在插件加载时注册。所以每个 HLSL pass 在它的管线编译时分到一个**槽位** —— `FDreamPassCS` 或 `FDreamPassPS` 的一个排列：

**预检。** 用你的代码、按工程的 shader 格式编译这个槽位。错误是带文件和行号的 DreamShader 诊断 —— 代码写在 `.dsp` 里时就是 `.dsp` 的行 —— 管线不会编译成功。
文件块共享代码里的错误只报一次，并列出它让哪些 pass 失败。没有入口用到的代码根本不会被编译 —— 引擎在 shader 编译器读槽位之前就丢掉了 —— 所以还没人调用的辅助函数不检查，和 `.usf` 一样。
**快照。** 代码的快照 —— `.usf` 以及它用相对路径 include 的每个文件；或者代码在 `.dsp` 里时，文件的块（别的 pass 的入口抹掉）、pass 自己的代码，以及它们相对 `.dsp` include 的每个文件 ——
写进 `<DShader>/.dreampass/Slots/<C|P><NN>/`，注册表（`RegistryCompute.ush`、`RegistryPixel.ush`）把槽位映射过去。`Registry.json` 记录哪个 pass 占着哪个槽位。
**两个 shader 类型**在编辑器里重新编译，pass 在下一帧运行。

全局 shader 从快照构建，从不直接用你正在编辑的文件。这很重要，因为全局 shader 的编译错误是致命的：直接编你写了一半的代码，编辑器或 cook 会停下。
有了快照，拉到你改动的队友拿到的是通过了你预检的那份，cook 也一样。在 `.dsp` 里块的上方加一行注释不会挪动快照的任何一行 —— 快照里不含 `.dsp` 的行号 —— 所以不会重编任何全局 shader。

- 槽位是稳定的：pass 跨编译保持自己的槽位；新 pass 拿最小的空闲槽位；删掉的 pass 在它的管线再次编译时释放槽位；`dsc pass-registry -Gc` 释放已不存在的管线的槽位。槽位用完是编译错误，并告诉你要调大哪个数。
- 没用到的槽位编译成空入口，所以每次构建的排列列表都一样 —— cook 出的构建要求这一点。
- 全局 shader 被 cook 进基础游戏的全局 shader 缓存：**HLSL pass 不能放进 DLC。**
- 块在与它的 `{` 配对的 `}` 处结束；注释、字符串和 `#` 行里的花括号不算。块里不支持含花括号的多行宏（行尾 `\`）。

<a id="接下来"></a>

## 接下来

- [Custom Pass 管线](../language-v2/passes.md) — .dsp 的写法：缓冲区、pass、注入点、参数。
  - [Custom Pass 运行时](../runtime/index.md) — 激活、注入点、内置与导出的缓冲区、视图、调试、限制。
  - [Custom Pass 示例](../examples/custom-pass.md) — 三种写 HLSL 的方式都有完整示例。
