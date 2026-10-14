---
title: Custom Pass 管线
description: .dsp —— 把自定义渲染 pass 写成源码。具名缓冲区、在渲染器注入点运行的全屏 / compute / mesh / clear / copy pass、可被激活方覆盖的参数，编译成一个 UDreamPassPipeline。需要 UE 5.8。
---

一个 `.dsp` 文件就是一个 `UDreamPassPipeline`：几个具名缓冲区、一串各自挑选渲染器注入点运行的 pass，以及激活方可以覆盖的参数。
不改引擎就能给 Unreal 加一个渲染 pass —— 选中物体的遮罩、一次模糊、一个屏幕空间效果、一个写数据纹理的 compute shader。

| 项目 | 值 |
| :-- | :-- |
| 产物 | 一个 `UDreamPassPipeline`，以文件名命名，落在同目录 `.dss` 会放产物的地方；每个导出的缓冲区再加一个 `UTextureRenderTarget2D` |
| 运行于 | **DreamShaderPass** 运行时模块 —— **UE 5.8** 及以上。更老的引擎上 `.dsp` 照样能解析、格式化和绑定，编译会被拒绝 |
| Pass 种类 | `fullscreen`（Post Process 材质或 HLSL）、`compute`（HLSL）、`mesh`（把选中的图元再画一遍）、`clear`、`copy` |
| 激活方式 | 工程设置、`ADreamPassVolume`、`UDreamPassComponent`、蓝图 / C++ —— 见 [Custom Pass 运行时](../runtime/index.md) |
| 起始版本 | （自 2.1.0 起） |

```hlsl
// DShader/Passes/CP_Highlight.dsp —— 选中的物体描一圈橙色轮廓，隔着墙也看得见
#pragma pipeline(Order = 100)

/// @group Look
uniform float4 OutlineColor = float4(1.0, 0.6, 0.0, 1.0);
/// @group Look   @slider 1 8
uniform float  OutlineWidth = 3.0;

buffer Mask    : R8(Clear = 0);
/// @desc 模糊后的遮罩；地面材质也读它。
buffer Blurred : R8(Scale = 0.5, Export = true);

pass DrawMask : mesh
{
    Injection = AfterOpaque;
    Filter    = Layer(Highlight);
    Material  = "M_HighlightMask";
    Depth     = None;
    write Output0 = Mask;
}

pass Blur : compute
{
    Injection = AfterOpaque;
    Dispatch  = Blurred;
    read  Source = Mask;
    write Result = Blurred;
    param Radius = OutlineWidth;

    hlsl
    {
        // 入口的语句：Id 是本线程的纹素，超出 dispatch 的线程已经提前返回。
        const float2 UV = (Id.xy + 0.5) * ResultSize.zw;
        const int R = (int)Radius;
        float Sum = 0;
        for (int y = -R; y <= R; ++y)
        {
            for (int x = -R; x <= R; ++x)
            {
                Sum += Source.SampleLevel(DP_LinearClamp, UV + float2(x, y) * SourceSize.zw, 0).r;
            }
        }
        Result[DP_ViewRect.xy + Id.xy] = Sum / ((2 * R + 1) * (2 * R + 1));
    }
}

pass Composite : fullscreen
{
    Injection = BeforePostProcess;
    Material  = "PP_OutlineComposite";
    read  Mask;
    read  Blurred;
    write SceneColor;
    param Color = OutlineColor;
}
```

用到的材质就是普通的 `.dss`：`#pragma material(Domain = PostProcess, ...)` 的材质通过 `UE.UserSceneTexture` 读 pass 的缓冲区，
mesh pass 的材质通过 `UE.DreamPassOutput` 写输出 —— 见 [Custom Pass 材质节点](../builtins/dream-pass.md)。
HLSL pass 的代码像 `Blur` 这样直接写在 `.dsp` 里，或者放在 pass 用 `Shader =` 指向的 `.usf` 里 —— 见 [HLSL pass](../builtins/hlsl-library.md)。
带全部 `.dss` 的完整示例在 [Custom Pass 示例](../examples/custom-pass.md)。

<a id="文件"></a>

## 文件

| 声明 | 形式 | 说明 |
| :-- | :-- | :-- |
| `#pragma pipeline(...)` | 至多一条 | [管线键](#管线键) |
| `uniform` | `uniform <type> <Name> [= <value>];` | 激活方可以覆盖的参数：`float`–`float4`、`int`、`bool`、`Texture2D`（配 `/// @default <资产>`）。`///` 接受 `@group`、`@desc`、`@slider`、`@sort` |
| `static const` | `static const <type> <Name> = <value>;` | 常量，在用到的地方折叠 |
| `buffer` | `buffer <Name> : <Format>[(<Key> = <Value>, ...)];` | [缓冲区](#缓冲区) |
| `hlsl` | `hlsl { <HLSL> }`，至多一个 | 所有代码写在 `.dsp` 里的 pass 都带着它编译的 HLSL（共享函数），以及 pass 用 `Entry = <函数>;` 挑选的入口。见 [HLSL pass](../builtins/hlsl-library.md#文件的-hlsl-块) |
| `pass` | `pass <Name> : <kind> { ... }` | [Pass](#pass)；同一注入点内按声明顺序运行 |

`buffer`、`pass`、`hlsl` 只在 `.dsp` 里是关键字，`read`、`write`、`param`、`hlsl` 只在 pass 体内是关键字，所以 `.dss` 读起来和以前完全一样。
`.dsp` 没有 DreamShaderLang 函数，也没有自己的 `#include`：HLSL 函数和它们 include 的文件都放进 `hlsl` 块。
`hlsl` 块里是交给 shader 编译器的 HLSL，按写下的样子保留 —— 注释、空行、`#if`、`#define`、`#include` 都在；
DreamShader 自己的预处理器不碰它。注释经得起 `dsc fmt` 和 Adopt。

<a id="管线键"></a>

## 管线键

| 键 | 值 | 默认 |
| :-- | :-- | :-- |
| `Order` | 整数 —— 在同一注入点有 pass 的管线按 `Order` 升序运行，再按资产路径 | `0` |
| `Injection` | 没写注入点的 pass 用的[注入点](#注入点) | `BeforePostProcess` |
| `Views` | `Game`、`Editor`、`SceneCapture`、`PlanarReflection`、`ReflectionCapture`、`Thumbnail`，用 `\|` 连接 | `Game \| Editor` |
| `Requires` | `PostProcess`、`SceneResolve`、`CustomStencil` —— 视图缺了其中一样，整条管线关掉，而不是只跑一半 | 无 |
| `Enabled` | 一个 `bool` uniform，或常量 | `true` |

<a id="注入点"></a>

## 注入点

每个 pass 自己写注入点；管线只给默认值。

| 注入点 | 在帧里的位置 | 那里有的场景纹理 |
| :-- | :-- | :-- |
| `BeginView` | 可见性之前，什么都还没画 | 没有 —— 没有 SceneColor、SceneDepth，也没有 `View` uniform buffer（见 [HLSL pass](../builtins/hlsl-library.md#固定参数)） |
| `BeforeBasePass` | depth prepass 之后 | SceneDepth |
| `AfterBasePass` | GBuffer 已写、尚未光照 | GBuffer A–F、Velocity、SceneDepth；SceneColor 只有自发光；`r.CustomDepth.Order` 把它排在前面时（开了 DBuffer 贴花的默认）还有 CustomDepth |
| `AfterOpaque` | 光照、雾、天空已完成，半透明还没有 | 所有不透明的东西、CustomDepth / CustomStencil |
| `BeforePostProcess` | 除后处理链之外的一切 | 一切；DOF 之后渲染的半透明仍是分开的 |
| `PostProcess.BeforeDOF`、`PostProcess.AfterDOF` | 后处理链内，渲染分辨率 | 链上的颜色、所有场景纹理 |
| `PostProcess.TranslucencyAfterDOF` | DOF 之后的半透明合成之前 | 这里链上的是 `Translucency`，SceneColor 只读。没有分开的半透明时引擎跳过这一点 |
| `PostProcess.ReplaceTonemapper` | 替代 tonemapper —— 至多一条管线，整个进程里也只有一个扩展能拿到 | SceneColor、bloom；必须产出最终颜色 |
| `PostProcess.AfterMotionBlur`、`PostProcess.AfterTonemap`、`PostProcess.AfterFXAA` | 上采样之后，输出分辨率 | 链上的颜色 |
| `EndOfView` | 完成的视图，UI 之前 | view family 纹理（LDR） |

视图没有的注入点 —— 关了后处理、不 resolve 的场景捕获、路径追踪下的 `AfterBasePass` —— 在那里就是走不到；
它的 pass 在 `DreamPass.Dump` 里记为跳过，它本该写的缓冲区读出来是 `Clear` 值。

<a id="缓冲区"></a>

## 缓冲区

```hlsl
buffer <Name> : <Format>[(<Key> = <Value>, ...)];
```

格式：`R8 RG8 RGBA8 R16F RG16F RGBA16F R32F RG32F RGBA32F Depth32` —— `Depth32` 只能作 mesh pass 自己的深度。
`R32U`、`RG32U` 是保留的：还没有 pass 能读写整数缓冲区（`DSH7321`）；id 放进 `R32F` 在 16777216 以内是精确的。

| 键 | 值 | 默认 |
| :-- | :-- | :-- |
| `Scale` | 相对 `Resolution` | `1.0` |
| `Size` | `int2(w, h)`，用固定尺寸代替 `Scale` | — |
| `Resolution` | `Render`（上采样之前）或 `Output`（之后） | 第一个写它的 pass 所在的分辨率 |
| `Clear` | 标量、`float4(...)` 或 `None` —— `None` 时，任何写之前的读都是错误 | `0` |
| `Mips` | 只能是 `1`：pass 无法初始化或更新 mip 链，其他值是 `DSH7308` （自 2.1.2 起） | `1` |
| `History` | `true`：按视图跨帧保留；`<Name>.Previous` 读上一帧的 | `false` |
| `Export` | `true`：在最后一个写它的 pass 之后复制进一个渲染目标资产，普通材质、蓝图、UMG、Niagara 都能读 —— 见[导出的缓冲区](../runtime/index.md#导出的缓冲区) | `false` |

**警告：**

2.1.2 之前 `Mips` 大于 1 也能通过，但只有 mip 0 被写过、其余是未定义内容。现有管线请把 `Mips` 改成 `1`。

内置名字不用声明：`SceneColor`、`SceneDepth`、`CustomDepth`、`CustomStencil`、`GBufferA`…`GBufferF`、`Velocity`、`Translucency`。
各自在哪里能读、能写，见 [Custom Pass 运行时](../runtime/index.md#内置缓冲区)。

<a id="pass"></a>

## Pass

```hlsl
/// @desc 可选的描述
pass <Name> : <kind>
{
    <Key> = <Value>;
    read  [<pass 里的名字> =] <buffer>[.Previous];
    write [<pass 里的名字> =] <buffer>;
    param <pass 里的名字> = <expression>;
    hlsl { <HLSL> }
}
```

键以大写字母开头，绑定以 `read`、`write`、`param` 开头。`read Mask;` 就是 `read Mask = Mask;`。
`hlsl { ... }` 至多一个、后面不带 `;`，是写在 pass 里的 HLSL pass 代码，见 [HLSL pass](../builtins/hlsl-library.md)。

| 种类 | 键 | `read X = B` | `write Y = B` | `param P = E` |
| :-- | :-- | :-- | :-- | :-- |
| 带 `Material` 的 `fullscreen` | `Material` | 材质里名为 X 的 `UE.UserSceneTexture` | 恰好一个：`write B;` | 材质参数 P |
| HLSL 的 `fullscreen` | `Shader`、`Entry`，或 `hlsl` 块 | HLSL 名字 X | 至多四个，依次是 SV_Target0..3 | HLSL 名字 P |
| `compute` | `Shader`、`Entry`、`Threads`、`Dispatch`，或 `hlsl` 块 | HLSL 名字 X | 至多四个 UAV | HLSL 名字 P |
| `mesh` | `Filter`、`Material`、`Mode`、`Depth`、`Cull`、`Blend`、`Usage`、`Nanite`、`NaniteValue` | — | `Output0`…`Output3`：`UE.DreamPassOutput` 的输出 | 覆盖材质的参数 P |
| `clear` | `Value` | — | 一个 | — |
| `copy` | — | 一个，源 | 一个，目标 | — |

每种 pass 都还接受 `Injection` 和 `Enabled`（一个 `bool` uniform；为 false 的地方这个 pass 关掉）。

`param` 里的 `E` 是 uniform、`static const`、字面量、`DreamPassWeight`（管线在这个视图里的权重），
或它们乘、加常量 —— `param Radius = OutlineWidth * 2 + 1;`。别的都会被拒绝：这个值每个视图在 CPU 上算一次。

<a id="fullscreen"></a>

### fullscreen

`Material = "<名字或对象路径>"` 通过引擎自己的后处理 pass 运行一个 Post Process 材质，所以它看到的和后处理体积的材质看到的完全一样。
缓冲区以 `UE.UserSceneTexture` 输入的形式到达，按引擎的方式摆放：每个名字一个槽位，按材质里的顺序，跳过材质里 `SceneTexture` 节点读的槽位（`PostProcessInput0`…`4`）。
材质自己不读 `PostProcessInput0` 时能绑五个缓冲区，否则四个。写数据缓冲区而不是场景颜色的材质要在 `#pragma material` 里设
`bDisablePreExposureScale = true`，否则曝光会缩放它读写的值。

不写 `Material` 时，这个 pass 在像素 shader 槽位里运行 HLSL：自己的 `hlsl { ... }` 块、指向文件 `hlsl` 块里某个函数的 `Entry = <函数>`，
或 `Shader = "<文件>.usf"` 加 `Entry`。见 [HLSL pass](../builtins/hlsl-library.md)。

<a id="compute"></a>

### compute

代码是 pass 自己的 `hlsl { ... }` 块、文件 `hlsl` 块里的一个 `Entry`，或一个 `.usf`（`Shader` 和 `Entry`）。入口函数前面带 `[numthreads(x, y, z)]`；
只有线程组大小用宏写时才需要 `Threads = uint3(...)` —— 另外，只写入口语句的块用 `Threads` 作组大小（不写时是 `uint3(8, 8, 1)`）。
`Dispatch` 是 `<buffer>`（每个纹素一个线程）、`<buffer> * <scale>` / `<buffer> / <n>`，或 `uint3(x, y, z)`。

<a id="mesh"></a>

### mesh

把选中的图元再画一遍，画进至多四个缓冲区。

| 键 | 取值 | 说明 |
| :-- | :-- | :-- |
| `Filter` | `Stencil(v)`、`Stencil(v, mask)`、`Layer(A \| B)`、`List(Name)`，用 `\|` 和 `&` 组合（`&` 优先） | 选哪些图元。Layer 是工程的（工程设置 ▸ DreamShader Custom Pass），由 `UDreamPassLayerComponent` 发给图元；List 由 `UDreamPassSubsystem::AddToList` 填充 |
| `Material` | 带 `UE.DreamPassOutput` 的 Surface 材质 | 覆盖材质 |
| `Mode` | `Override`（有材质时默认）、`Own`（没有时默认）、`OwnOrOverride` | 谁来画：覆盖材质、图元自己的材质（需要带 `UE.DreamPassOutput`），或者能用自己的就用自己的 |
| `Depth` | `TestScene`（默认）、`None`、`Own(<Depth32 buffer>)` | 对场景深度测试、不测试，或只在这些图元之间测试 |
| `Cull` | `Auto`（默认）、`Back`、`Front`、`None` | `Auto` 跟随图元自己的材质和图元本身，覆盖材质画的时候也一样 |
| `Blend` | `Replace`（默认）、`Add`、`Max`、`Min`、`AlphaBlend` | |
| `Usage` | `StaticMesh`、`InstancedStaticMeshes`、`SkeletalMesh`、`Landscape`、`SplineMesh`、`GeometryCache` | 覆盖材质需要哪些 usage 标志；编译管线时检查。默认前三个 |
| `Nanite` | `Skip`（Layer / List 的默认）、`StencilMask`（Stencil 筛选的默认）、`AssignStencil(<值>)` | 见下 |
| `NaniteValue` | `float4(...)` | `StencilMask` 写的值 |

**警告：**

**mesh pass 画不了 Nanite 图元**：引擎不给 Nanite 几何体可供 pass 绘制的 mesh batch，它自己的 Nanite 光栅化器也不公开。
`Skip` 把这些图元排除在外（每个 pass 一条警告）。`StencilMask` 在 CustomStencil 与 pass 的 `Stencil(...)` 筛选匹配的地方写 `NaniteValue`
—— 引擎的 custom depth pass 在 `r.CustomDepth=3` 时确实会光栅化 Nanite —— 所以遮罩照样能盖住它们，只是没有它们材质的输出。
`AssignStencil(v)` 让 Layer 和 List 也能这样，办法是给其中的 Nanite 成员设 custom depth 和 stencil `v`；它会和工程自己用的 stencil 冲突，所以默认关闭。

`Mode = Override` 遇到自身材质是 masked 或会移动顶点的图元时：如果那个材质带 `UE.DreamPassOutput`，就用它自己的材质画；
否则覆盖材质按不透明来画，并警告一次。引擎的 custom depth pass 也是这条规则。

<a id="clear-与-copy"></a>

### clear 与 copy

`clear` 把它的缓冲区清成 `Value`（`Depth32` 则是深度值）。缓冲区第一次使用时本来就会清；`clear` 是为了在帧里稍后再清一次。
`copy` 在格式和尺寸一致时直接复制，否则缩放、转换着画过去 —— 把 `SceneColor` `copy` 进半尺寸缓冲区就是一次抓屏。

<a id="引用"></a>

## 引用

`Material` 和 `.dsi` 的 `Parent` 一样，要么是**裸名字** —— 同一源码根下某个 `.dss` 的材质或某个 `.dsi` 的实例，资产不存在或比源码旧时先被编译 ——
要么是指向任意材质的**对象路径**。`Shader` 是相对 `.dsp` 所在目录的路径（通常就放在 `.dsp` 旁边），或者 `/Project/Passes/Blur.usf` 这样的虚拟路径；
`hlsl` 块里的 `#include` 也一样。这个文件不需要映射着色器目录：引擎编译的是它的快照（见 [HLSL 怎么进入引擎](../builtins/hlsl-library.md#hlsl-怎么进入引擎)）。

<a id="编译器检查什么"></a>

## 编译器检查什么

能在帧之前知道的都在编译管线时检查：每个名字都声明过或是内置的；pass 种类能在它的注入点运行，内置缓冲区能在那里读或写；
按帧顺序没有缓冲区在所有写之前被读（`Clear = None`）；没有 pass 同时读写同一个缓冲区（`B.Previous` 和 `B` 算两个）；
全屏材质读的名字正是它的 `read` 绑定的、且不超过槽位数；mesh pass 的材质带 `UE.DreamPassOutput`、它写的输出都接上了、有所需的 usage 标志；
HLSL 文件存在、`Entry` 存在、绑定装得进槽位（8 个输入、4 个输出、64 个标量参数）；写在 `.dsp` 里的 HLSL 每个 pass 只在一处、形式与入口符合 pass 的种类，
文件的 `hlsl` 块不引用任何 pass 的槽位名、不调用任何入口；写场景颜色的 pass 在场景颜色的分辨率上运行；导出的缓冲区不是深度或整数缓冲区；`Mips` 是 1。
诊断码是 `DSH2300`–`DSH2349`、`DSH3300`–`DSH3349`、`DSH4400`–`DSH4449`、`DSH7300`–`DSH7379`、`DSH8300`–`DSH8339` —— 见[诊断](../diagnostics/index.md)。

<a id="工具"></a>

## 工具

- `dsc compile CP_X.dsp`、`dsc check`、`dsc check -Shaders`（还会按工程的 shader 格式预检每个 HLSL pass）、`dsc dump-ir`、`dsc index`、`dsc fmt` 都接受 `.dsp`。
- `dsc decompile /Game/.../CP_X` 写出 `Decompiled/Pipelines/Game/.../CP_X.dsp`（目录跟随资产的包路径），然后解析、绑定写出的文本并与资产比较。
- 在细节面板里改过的管线，**Adopt Into Source** 会逐个值地就地改写 `.dsp`，注释和顺序都保留。
- `dsc pass-registry` 列出 HLSL 槽位及其归属；`-Gc` 释放已不存在的 pass 的槽位。
- 材质浏览器列出 `.dsp` 文件，**New Source** 提供三个管线模板。
- VS Code 扩展 2.1.0 起认识 `.dsp`（语言 id `dreamshader-dsp`）。

<a id="接下来"></a>

## 接下来

- [Custom Pass 运行时](../runtime/index.md) — 激活、注入点、内置与导出的缓冲区、视图、调试、限制。
  - [HLSL pass](../builtins/hlsl-library.md) — hlsl 块、.usf、绑定名、DreamPass.ush、槽位与预检。
  - [Custom Pass 材质节点](../builtins/dream-pass.md) — UE.DreamPassOutput 与 UE.DreamPassBuffer。
  - [Custom Pass 示例](../examples/custom-pass.md) — 七条完整管线，连同它们的 .dss。
