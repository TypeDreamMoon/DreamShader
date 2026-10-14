---
title: DreamShaderLang 2.0
description: DreamShader 2.0 的 HLSL 形状语言 —— .dss 源文件、uniform 与文档指令、每个 export 一个资产，以及它和 1.x 语言怎么并存。
---

DreamShader 2.0 在 1.x 语言旁边加了第二门语言。材质被写成它骨子里本来的样子 —— 带声明的 HLSL：
`uniform` 就是参数，`///` 文档注释带着参数的元数据，`#pragma material(...)` 放材质设置，一个导出函数把材质填满。

| 项目 | 值 |
| :-- | :-- |
| 文件 | `.dss` —— 编译单元 · `.dsi` —— [材质实例](instances.md) · `.dsp` —— [Custom Pass 管线](passes.md)（UE 5.8） · `.dsh` —— 共享头文件，可以同时装两种语言 |
| 产物 | 每个文件至多一个材质；其余每个 `export` 各出一个材质函数、层或层混合；一个 `.dsp` 出一条 `UDreamPassPipeline` |
| 编译器 | 和编 `.dsm` / `.dsf` 的是同一个：两个前端、一份图 IR、一个发射器 |
| 起始版本 | （自 2.0.0 起） |

**提示：**

**没有任何东西逼你迁移。** `.dsm` 由 legacy 前端读成和 `.dss` 一样的语法树，再由同一份代码去编 ——
拿四个真实源码根的 1.x 冻结图做过对拍，每个源文件都逐节点重建出同一张图。想要只有 2.0 才有的东西时再搬：
真正的表达式与控制流、`#include`、和材质放在一起的材质实例、经得起工具折腾的注释。
改写交给 [`dsc migrate`](../tools/migrate.md)，它先证明、后落盘。

<a id="一个材质"></a>

## 一个材质

```hlsl
// M_TeleportGlow.dss
#pragma material(ShadingModel = Unlit, BlendMode = Additive)

/// @group Glow|Look   @desc 乘在粒子颜色上
uniform float4 Tint = float4(1, 1, 1, 1);

/// @group Glow|Look   @desc 整体自发光增益
/// @slider 0 4
uniform float Intensity = 0.7;

float GlowMask(float2 UV)                       // helper：内联进调用它的图
{
    float2 P = UV * 2.0 - 1.0;
    return pow(saturate(1.0 - dot(P, P)), 2.4);
}

export void M_TeleportGlow(inout material m)    // 入口：正是这个签名让它成为材质
{
    float2 UV = UE.TexCoord(Index = 0);
    m.EmissiveColor = Tint.rgb * GlowMask(UV) * Intensity;
}
```

资产以函数名命名，落在文件所在的位置 —— `DShader/FX/M_TeleportGlow.dss` 生成 `/Game/FX/M_TeleportGlow`。
`/// @name /Game/Somewhere/Else` 可以改写这个去处。

<a id="一个材质函数"></a>

## 一个材质函数

```hlsl
// MF_ToonUV.dss
/// @asset /MoonToon/MaterialFunctions/Utils/MF_UVChannelSwitch
extern float2 MF_UVChannelSwitch(float UVChannelIndex);      // 一个已经存在的资产

/// @library MoonToon|Shared
/// @param UVChannel    读哪个 UV 通道。
export float2 MF_ToonUV(float UVChannel = 0.0, float4 ScaleOffset = float4(1, 1, 0, 0))
{
    float2 Selected = MF_UVChannelSwitch(UVChannel);
    return Selected * ScaleOffset.xy + ScaleOffset.zw;
}
```

形参就是输入，带默认值的形参是可选输入，返回值是第一个输出，`out` 形参是其余输出。

默认值在**被调用方**求值 （自 2.1.2 起）：`float F(float x, float y = x)` 写成 `F(7)` 时 `y = 7`，
调用写在另一个函数里也一样。显式实参先在调用方求值；互相引用的默认值按需解析，成环且没有显式值是 `DSH6224`。
helper 的 `out` 可以由默认值初始化，`@custom` 函数的输出不行（同样是 `DSH6224`）—— 初值要从另一个输入引脚传进来。
调用 `export` 或 `extern` 材质函数时，省略的引脚留空，由资产自己的默认值生效，不会在调用方求原型上的默认值。

<a id="函数是什么"></a>

## 函数是什么

| 声明 | 变成 |
| :-- | :-- |
| `export void M(inout material m)` | 本文件的材质 —— 至多一个 |
| `export T F(...)` | 一个材质函数资产 |
| `/// @layer` + `export void L(inout material m)` | 材质层 |
| `/// @layerblend` + `export void B(material Bottom, material Top, inout material Result)` | 材质层混合 |
| `T Helper(...)` —— 不带链接关键字 | 自己不产出任何东西：内联进每个调用者的图 |
| `/// @custom` + 函数 | 一个 Custom 节点；函数体是 HLSL，原样交给 shader 编译器 |
| `/// @asset <path>` + `extern T F(...);` | 对一个已存在的材质函数资产的调用 |

`#include "x.dss"`（或 `.dsh`）把另一个文件的声明带进来。保存一个被包含的 `.dss` 时，包含它的源文件会一起重建 （自 2.1.2 起）。

<a id="参数"></a>

## 参数

`uniform` 是材质参数，它上面的 `///` 块就是参数的元数据。

| 指令 | 用在 | 含义 |
| :-- | :-- | :-- |
| `@group A\|B` | uniform | 实例编辑器里的分组；`\|` 表示嵌套 |
| `@desc 文本` | 任意 | tooltip / 描述 |
| `@sort 10` · `@slider 0 1` | uniform | 排序优先级 · 滑条范围 |
| `@static` | `uniform bool` | 静态开关：对它的 `if` 是编译期裁掉，不是运行时选择 |
| `@default /Game/T_X` · `@sampler Normal` | 纹理 uniform | 默认资产 · 采样器类型 |
| `@name` | uniform · 产物 | 显示名 · 资产路径 |
| `@library A\|B` · `@param 名字 文本` | export | 函数库分类 · 某个输入引脚的描述 |

`static const T Name = ...;` 是编译期常量，不产生参数。

<a id="函数体"></a>

## 函数体

真正的 HLSL 表达式与语句：完整的运算符优先级、`?:`、强制转换、构造器、swizzle、`if` / `else`，
以及编译器能证明循环次数的 `for` / `while`（会被展开）。引擎节点是对两个命名空间的调用，实参按名字给：

```hlsl
float3 N = UE.VertexNormalWS();
float4 Tex = Albedo.Sample(UE.TexCoord(Index = 0));
m.Opacity = UE.Fresnel(ExponentIn = 4.0, BaseReflectFractionIn = 0.04);
m.FrontMaterial = Substrate.Slab(BaseColor = Tex.rgb, Metallic = 1.0, Roughness = 0.35);
```

引擎和已加载插件里的每一个 `UMaterialExpression` 类都能写成 `UE.<Name>`；Substrate 节点在 `Substrate.` 之下，
并且有[自己的语法糖](substrate.md)。两次相同的调用是同一个节点，所以在用到值的地方直接写调用没有任何代价。
引脚按节点各自命名的类，用节点上显示的名字传参 —— 例如 `UE.LandscapeLayerBlend(Layer_Grass = ...)`，名字取自它的 `Layers` （自 2.0.2 起）。

条件是 `/// @static` uniform 的 `if` 是 StaticSwitch；其他 `if` 按变量逐个走运行时选择。
`material` 是一个带字段的值 —— `m.BaseColor = ...` —— 没人赋值的字段就是没人连的引脚。

<a id="几条求值规则"></a>

### 几条求值规则

| 写法 | 行为 |
| :-- | :-- |
| `c ? a : b` | 条件只求一次；只有被选中那一侧的赋值、`++` 与 `out` / `inout` 写入生效，两侧都从条件之后的状态出发，和 `if` 一样 （自 2.1.2 起）。`@custom` 函数体照引擎的 HLSL 规则 |
| `(int)x`、`(bool)x` | 图里的强制转换不改值，常量也一样：`(int)0.5` 与 `(bool)0.5` 都是 0.5 （自 2.1.2 起）。要截断写 `trunc(x)`，要真假写 `x != 0` |
| `v.zyx.x = 1` | 链式 swizzle / 下标写入，写的是它最终指到的分量（这里是 `v.z`）（自 2.1.2 起） |
| 嵌套 `if` 的每个分支都 `return` | 外层之后仍然可达的 `return` 会保留 （自 2.1.2 起） |
| `round`、`sin`、`cos` | `round` 半数取偶（`round(2.5)` 是 2），`sin` / `cos` 是 `Period = 1` 的节点、按周期而不是弧度，常量折叠也一样（`sin(0.25)` 是 1）（自 2.1.2 起）；`@custom` 函数体是 HLSL，仍按弧度。见[数学内置](../builtins/math.md#round) |

<a id="region-与布局"></a>

## Region 与布局

```hlsl
#pragma region "Rim light"
float Rim = pow(1.0 - saturate(dot(N, V)), RimPower);
m.EmissiveColor = RimColor * Rim;
#pragma endregion
```

region 是围住其中语句所生成节点的注释框。`#pragma layout(...)` 是反编译器写下来、用来保住手摆节点位置的行。
其余节点怎么排是一个工程设置 —— **Graph Layout Style**：`Blocks`（默认），
按 region 和语句分成一个个框、框之间用具名 reroute 相连；1.x 的布局 `Classic`；`SourceBands`，按源码语句一条一带；`Layered`，整图分层。
`dsc dump-layout` 可以不构建任何东西就把三种读源码的布局画出来。

<a id="它没有的"></a>

## 它没有的

| 没有 | 替代 |
| :-- | :-- |
| 运行时才知道次数的循环、分支里的 `discard` | 写进 `/// @custom` 函数 |
| 图里的矩阵 | 在 `/// @custom` 函数体里算 |
| `switch`、逗号运算符、`let` / `auto` | `if` 链；把类型写出来 |
| 材质层**堆栈** | `@layer` 与 `@layerblend` 产出两种函数；堆栈在编辑器里组装 |

<a id="custom-pass-管线"></a>

## Custom Pass 管线

（自 2.1.0 起） `.dsp` 是第三种源文件：一个文件就是一条 Custom Pass 管线 —— 具名缓冲区，加上一串在渲染器注入点运行的 pass
（全屏材质或 HLSL、compute、mesh、clear、copy），编译成 `UDreamPassPipeline` 资产，并能反编译回 `.dsp`。
它只在 **UE 5.8** 及以上运行；更老的引擎上照样能解析、格式化和绑定，编译会被拒绝。
写法见 [Custom Pass 管线](passes.md)，激活与运行见 [Custom Pass 运行时](../runtime/index.md)。

<a id="工具"></a>

## 工具

| 工具 | 2.0 |
| :-- | :-- |
| [`dsc check`](../tools/commandlet.md) | 编到 IR 校验为止、不写任何资产 —— CI 的闸门；`-Shaders` 还会编 shader，并把 HLSL 错误映射回源码行 |
| [`dsc fmt`](../tools/commandlet.md) | 语言自己的打印器当格式化器，担保不了的文件一律不写 |
| [`dsc decompile`](../tools/decompiler.md) | 把编译器倒着跑一遍，写出 `.dss`（实例是 `.dsi`，管线是 `.dsp`） |
| [VS Code](../tools/vscode.md) | 高亮与代码片段；跳转、悬停、重命名与节点补全由编译器自己的索引回答 |
| 编辑器 | 节点 ↔ 源码行双向跳转，手改过的资产可以 *Adopt Into Source* |
