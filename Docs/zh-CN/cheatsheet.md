<a id="dreamshaderlang-cheat-sheet"></a>

# DreamShaderLang 速查表

2.0 语言的一页：文件类型、每种文件的形态，以及最耗时的那些错误。本页上每个完整的源码都由测试套件编译。关于某一行背后的规则，请阅读[2.0 语言](language-v2/index.md)；关于某个错误码，请阅读[诊断](diagnostics/README.md)，或以 JSON 形式查看同样的数据，[`diagnostics/codes.json`](../diagnostics/codes.json)。

`.dss` 是**带声明的 HLSL**：参数是 `uniform`，元数据依附于 `///` 行和 `#pragma` 行，一个 `export` 函数就是该资产。它不是 HLSL：不要从 HLSL、GLSL 或材质编辑器中猜测名称。

<a id="files"></a>

## 文件

| 文件 | 包含 | 构建为 |
| :-- | :-- | :-- |
| `.dss` | `uniform`、辅助函数、一个 `export` 入口或多个 `export` 函数、`#pragma material` | 一个材质，或材质函数 / 材质层 / 混合 |
| `.dsi` | `#pragma instance(Parent = …)` 和 `uniform` 覆盖 | 一个材质实例 |
| `.dsp` | `#pragma pipeline`、`buffer`、`pass` | 一个 Custom Pass 管线（UE 5.8+） |
| `.dsh` | 共享辅助函数，通过 `#include "Path/File.dsh"` 引入 | 其自身不构建任何内容 |
| `.dsm` / `.dsf` | 1.x 语言；仍可构建，新源码请写为 `.dss` | 与 1.x 相同 |

`DShader/Props/M_Crate.dss` 配合 `export void M_Crate(…)` 构建出 `/Game/Props/M_Crate`：`DShader/` 下的文件夹，加上 `export` 的名称。`/// @name Other` 可重命名该资产。

<a id="a-material"></a>

## 一个材质

```hlsl
// DShader/CheatSheet/M_CheatSheetSurface.dss -- a lit, textured surface with an optional pulsing edge glow
#pragma material(Domain = Surface, BlendMode = Opaque, ShadingModel = DefaultLit)

/// @group Surface   @sort 10   @default /Engine/EngineResources/WhiteSquareTexture
uniform Texture2D Albedo;

/// @group Surface   @sort 20   @desc Multiplies the texture
uniform float4 Tint = float4(0.8, 0.75, 0.7, 1.0);

/// @group Surface   @sort 30   @slider 0 1
uniform float SurfaceRoughness = 0.6;

/// @static
/// @group Glow   @sort 40
uniform bool UseGlow = true;

/// @group Glow   @sort 50   @slider 0 10
uniform float GlowStrength = 2.0;

float EdgeMask(float2 UV, float Width)              // a helper: inlined at each call
{
    float2 D = min(UV, 1.0 - UV);
    return 1.0 - saturate(min(D.x, D.y) / Width);
}

export void M_CheatSheetSurface(inout material m)   // the entry: this signature makes a material
{
    float2 UV = UE.TexCoord(Index = 0);
    float4 Texel = Albedo.Sample(UV);
    m.BaseColor = Texel.rgb * Tint.rgb;
    m.Roughness = SurfaceRoughness;
    m.Metallic = 0.0;

    float3 Glow = float3(0, 0, 0);
    if (UseGlow)                                     // a static bool: a StaticSwitch, not a branch
    {
        float Pulse = 0.5 + 0.5 * sin(UE.Time() * 0.5);   // sin takes cycles: one pulse every 2 s
        Glow = Tint.rgb * EdgeMask(UV, 0.08) * Pulse * GlowStrength;
    }
    m.EmissiveColor = Glow;
}
```

- 每个文件一个入口，`export void <Name>(inout material m)`。输出是 `m` 的成员：`BaseColor`、`Metallic`、`Roughness`、`Specular`、`EmissiveColor`、`Opacity`、`OpacityMask`、`Normal`、`WorldPositionOffset`、`AmbientOcclusion`、……——即引擎自身的引脚名称，区分大小写。
- 给 uniform 起一个不同于输出的名称（`SurfaceRoughness`，而不是 `Roughness`）；这样更易读，也更易搜索。

<a id="a-ui-material"></a>

## 一个 UI 材质

```hlsl
// DShader/CheatSheet/M_CheatSheetPanel.dss -- a translucent UI panel: a soft vertical gradient and a scrolling band
#pragma material(Domain = UI, BlendMode = Translucent, ShadingModel = Unlit)

/// @group Panel   @sort 10   @desc Base colour; alpha scales the opacity
uniform float4 Tint = float4(0.12, 0.16, 0.2, 0.85);

/// @group Panel   @sort 20   @slider 0 2   @desc Band travel, panels per second
uniform float ScrollSpeed = 0.25;

export void M_CheatSheetPanel(inout material m)
{
    float2 UV = UE.TexCoord(Index = 0);
    float Band = frac(UV.y - UE.Time() * ScrollSpeed);
    float Glow = pow(saturate(1.0 - abs(Band - 0.5) * 4.0), 3.0);
    m.EmissiveColor = Tint.rgb * lerp(1.15, 0.85, UV.y) + Glow * 0.08;
    m.Opacity = Tint.a;
}
```

UI 材质只写入 `EmissiveColor` 和 `Opacity`（或 `OpacityMask` 搭配 `BlendMode = Masked`）；其他内容都不会到达屏幕。

<a id="parameters"></a>

## 参数

```hlsl
uniform <Type> <Name> = <value>;          // a material parameter
static const <Type> <Name> = <value>;     // a constant, folded where it is used
```

| uniform 上方的 `///` 标签 | 作用 |
| :-- | :-- |
| `@group A\|B` · `@sort N` · `@desc text` | 分组、排序优先级、工具提示 |
| `@slider min max` | 滑块范围，仅标量 |
| `@static` | `uniform bool` 会变为静态开关 |
| `@default /Game/T_X` · `@sampler Color` | 纹理的默认资产和采样器类型 |
| `@name Real Name` | 参数的引擎名称（重命名标识符时保留覆盖值） |

多个标签可共处一行。纹理不接受初始化器：改用 `@default`。不支持 uniform 数组。

<a id="pragma-material"></a>

## `#pragma material`

```hlsl
#pragma material(Domain = PostProcess, BlendMode = Translucent, ShadingModel = Unlit, TwoSided = true)
```

键为 `UMaterial` 属性名；枚举值省略其前缀（`Translucent`，而不是 `BLEND_Translucent`）。

| 键 | 常用值 |
| :-- | :-- |
| `Domain` | `Surface`, `DeferredDecal`, `LightFunction`, `Volume`, `PostProcess`, `UI` |
| `BlendMode` | `Opaque`, `Masked`, `Translucent`, `Additive`, `Modulate`, `AlphaComposite` |
| `ShadingModel` | `Unlit`, `DefaultLit`, `Subsurface`, `ClearCoat`, `TwoSidedFoliage`, `Cloth`, `Substrate` |
| `Backend` | `ThinCustom`（项目默认）、`Graph`（一个美术可直接打开的普通 `UMaterial`） |

<a id="expressions"></a>

## 表达式

- 类型：`float` `half` `int` `uint` `bool`、`float2`..`float4`、`Texture2D` `TextureCube` `Texture2DArray` `Texture3D`、`SamplerState`、`material`、`Substrate`。
- 语句就是 HLSL：声明、`if`/`else`、带常量循环次数的 `for`/`while`/`do`（最多 64 次）、`return`、`?:`、swizzle、类型转换、构造函数。不支持矩阵，不支持 `struct`，不支持 `switch`。
- 数学函数，直接调用（`saturate(x)`）：

`abs` `acos` `asin` `atan` `atan2` `ceil` `clamp` `cos` `cross` `ddx` `ddy` `distance` `dot` `exp` `exp2` `floor` `fmod` `frac` `fwidth` `length` `lerp` `log` `log2` `log10` `max` `min` `normalize` `pow` `rcp` `reflect` `refract` `round` `rsqrt` `saturate` `sign` `sin` `smoothstep` `sqrt` `step` `tan` `trunc`

- **`sin`、`cos`、`tan` 接受的是周期数，而不是弧度**——`sin(0.25)` 等于 1。要使用弧度请写 `sin(x / 6.2831853)`。
- 引擎节点：`UE.<Node>(Pin = value, …)`，具名参数：`UE.TexCoord(Index = 0)`、`UE.Time()`、`UE.Panner(Coordinate = UV, Speed = float2(0.1, 0))`。有多个输出的节点需指定其中一个：`UE.ScreenPosition().ViewportUV`。DreamMCP 的 `lookup` 可列出任意节点的引脚。
- 纹理：`Albedo.Sample(UV)`、`Albedo.Sample(S, UV)`、`Albedo.SampleLevel(UV, Mip)`。
- 标量可拓宽为任意向量；向量绝不会静默收窄——请显式写出 swizzle（`c.rgb`）。
- 两个完全相同的调用是同一个节点：无需为了共享节点而把它保存在变量中。

<a id="material-functions"></a>

## 材质函数

```hlsl
// DShader/CheatSheet/MF_CheatSheetTiling.dss -- a material function: tiled UV plus the cell index
/// @library CheatSheet|UV
/// @param UV       The coordinates to tile.
/// @param Scale    Tiles per UV unit.
export float2 MF_CheatSheetTiling(float2 UV, float Scale = 4.0, out float2 Cell)
{
    float2 T = UV * Scale;
    Cell = floor(T);
    return frac(T);
}
```

| 写法 | 构建为 |
| :-- | :-- |
| `float3 F(float2 UV) { … }` | 不构建任何内容——每次调用时内联到图中 |
| `export float3 F(float2 UV, float S = 1.0, out float A) { … }` | 一个材质函数；`out` = 额外输出，默认值 = 可选输入 |
| `/// @layer` + `export void L(inout material m)` | 一个材质层 |
| `/// @layerblend` + `export void B(material Bottom, material Top, inout material Result)` | 一个层混合 |
| `/// @asset /Game/MF_X` + `extern float3 MF_X(float2 UV);` | 无——调用已有资产 |
| `/// @custom` + `float F(float2 UV) { … }` | 一个 Custom HLSL 节点；函数体是原样的 HLSL（矩阵、真正的循环） |

入口和其他 `export` 不能共用一个文件：请把材质和它的函数放在不同的文件中。

<a id="instances--dsi"></a>

## 实例 — `.dsi`

```hlsl
// DShader/CheatSheet/MI_CheatSheetSurface_Red.dsi
#pragma instance(Parent = "M_CheatSheetSurface")

uniform float4 Tint = float4(0.9, 0.2, 0.15, 1.0);
uniform bool UseGlow = false;
```

`Parent` 是同一源根下的裸产品名，或一个对象路径。只覆盖父级声明过的项，并使用父级的类型；元数据归属于父级。

<a id="custom-pass-pipelines--dsp-ue-58"></a>

## Custom Pass 管线 — `.dsp`（UE 5.8+）

```hlsl
// DShader/Passes/CP_Highlight.dsp
#pragma pipeline(Order = 100)

/// @group Look
uniform float4 OutlineColor = float4(1.0, 0.6, 0.0, 1.0);
/// @group Look   @slider 1 8
uniform float  OutlineWidth = 3.0;

buffer Mask    : R8(Clear = 0);
buffer Blurred : R8(Scale = 0.5, Export = true);

pass DrawMask : mesh                       // the objects on the Highlight layer, into Mask
{
    Injection = AfterOpaque;
    Filter    = Layer(Highlight);
    Material  = "M_HighlightMask";         // a .dss with UE.DreamPassOutput
    Depth     = None;
    write Output0 = Mask;
}

pass Blur : compute                        // HLSL, written in the pass
{
    Injection = AfterOpaque;
    Dispatch  = Blurred;
    read  Source = Mask;
    write Result = Blurred;
    param Radius = OutlineWidth;

    hlsl
    {
        const float2 UV = (Id.xy + 0.5) * ResultSize.zw;
        const int R = (int)Radius;
        float Sum = 0;
        for (int y = -R; y <= R; ++y)
            for (int x = -R; x <= R; ++x)
                Sum += Source.SampleLevel(DP_LinearClamp, UV + float2(x, y) * SourceSize.zw, 0).r;
        Result[DP_ViewRect.xy + Id.xy] = Sum / ((2 * R + 1) * (2 * R + 1));
    }
}

pass Composite : fullscreen                // a Post Process material
{
    Injection = BeforePostProcess;
    Material  = "PP_OutlineComposite";
    read  Mask;
    read  Blurred;
    write SceneColor;
    param Color = OutlineColor;
}
```

Pass 使用的材质就是普通的 `.dss` 文件。详情见 [Passes](language-v2/passes.md)。

<a id="mistakes-that-cost-the-most-time"></a>

## 最耗时的错误

| 代码 | 严重性 | 消息 |
| :-- | :-- | :-- |
| [DSH4200](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4200) | error | '{0}' 未声明；你是不是想写 '{1}'？名称区分大小写。 |
| [DSH4250](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4250) | error | '{0}' 是 GLSL 的写法；本语言是 HLSL，所以请写 '{1}'。 |
| [DSH4226](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4226) | error | {0} 期望 {1}，而这里是 {2}。 |
| [DSH4243](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4243) | error | 这个 '/' 的两边都是整数，而材质图没有整数除法；要小数请写 'float(a) / b'，要整数部分请写 'floor(float(a) / b)'。 |
| [DSH4361](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4361) | error | 此表达式是 {0}，而材质图没有矩阵；请改在 custom 函数体内计算。 |
| [DSH4246](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4246) | error | 材质图没有双曲函数节点，因此 '{0}' 无法降级；请把它写在 '/// @custom' 函数体中，那里的着色器编译器支持它。 |
| [DSH4362](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4362) | error | 'discard' 没有材质图形式；请改为把材质的 OpacityMask 设为零，或把该分支移入 '/// @custom' 函数。 |
| [DSH2160](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh2xxx/#dsh2160) | error | 不支持的语句：预处理行 '#{0}' 不能出现在函数体内；请把该函数标记为 /// @custom，把它的函数体交给着色器编译器。 |
| [DSH4365](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4365) | error | 结构体没有图形表示形式；请传入它的某个字段，或把整个内容移入 '/// @custom' 函数。 |
| [DSH6200](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh6xxx/#dsh6200) | error | '{0}' 是第二个材质入口；它上方的 '{1}' 已经是入口，而一个文件只生成一个材质。 |
| [DSH6201](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh6xxx/#dsh6201) | error | '{0}' 是从入口为 '{1}' 的文件中导出的；一个文件要么生成材质，要么生成函数，不能两者兼有。请把它移到单独的文件中，或去掉 'export' 让它成为辅助函数。 |
| [DSH6206](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh6xxx/#dsh6206) | error | '{0}' 是内建操作，不能重新声明；请重命名该函数。 |
| [DSH5200](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh5xxx/#dsh5200) | error | 材质没有 '{0}' 引脚；你是不是想用 '{1}'？属性名区分大小写。 |
| [DSH5220](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh5xxx/#dsh5220) | error | '{0}.{1}' 接受具名参数：请写 'Pin = value'。 |
| [DSH5201](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh5xxx/#dsh5201) | error | 此 Custom 节点没有声明名为 '{0}' 的输出；它的输出是 '{1}'。输出由 'AdditionalOutputs' 声明。 |
| [DSH7213](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh7xxx/#dsh7213) | error | 纹理 uniform 没有 HLSL 初始化器；请把它的默认资产写为 '/// @default /Game/...'。 |
| [DSH7211](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh7xxx/#dsh7211) | error | '{0}' 是没有存储类的文件作用域变量；材质参数请写 'uniform'，编译期常量请写 'static const'。 |
| [DSH7233](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh7xxx/#dsh7233) | warning | 滑块范围（'@slider'，或 1.x 属性中的 'Slider(min, max)'）用于标量参数；而这个是 '{0}'，它没有滑块，因此该范围被忽略。 |

- 大小写不正确的名称无法被找到：`tint` 而非 `Tint`。错误消息会提示正确的名称。
- GLSL 的写法——`mix`、`fract`、`mod`、`vec3`——应为 `lerp`、`frac`、`fmod`、`float3`。
- 矩阵运算、`sinh`/`cosh`/`tanh` 和真正的循环应放在 `/// @custom` 函数中。
- 每次修改后运行 `check`；它不写入任何内容。`compile` 构建资产；`preview` 渲染资产。

<!-- Generated by .skill/gen-knowledge.ps1 from .skill/cheatsheet/cheatsheet.src.md. Edit that file, not this one. -->
