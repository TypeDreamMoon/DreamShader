---
title: "DreamShaderLang 2.0 —— 今天已经存在的东西"
description: "DreamShaderLang 2.0 —— 今天已经存在的东西"
---

<a id="dreamshaderlang-20--what-exists-today"></a>

# DreamShaderLang 2.0 —— 今天已经存在的东西

> [DreamShader](../index.md) » **DreamShaderLang 2.0**

> [!IMPORTANT]
> **一个编译器构建一切。** 只有一条流水线 —— 预处理 → 解析 →
> 绑定 → 降低为图 IR → 运行各个 pass → 校验 → 发射 —— 而它前面有两个前端：
> 一个 `.dss` 由 2.0 解析器读取，而一个 `.dsm` / `.dsf` 由 **legacy 前端**读取，
> 它把 [1.x 语言](../language/index.md)读进同一棵树。1.x 生成器已经没了；它构建出来的东西
> 被逐节点复现（68 个真实源文件中的 68 个，对照冻结的 1.x 图转储测量）。
>
> 随之而来的新东西：**材质实例作为源**（[`.dsi`](instances.md)）、一个**写出
> `.dss` 的反编译器**，以及 **[`dsc migrate`](../tools/migrate.md)**，它把 1.x 文件改写成 2.0，并在写入任何东西之前
> 先证明这次改写。

DreamShaderLang 2.0 去掉了 1.x 的节块（`Shader`、`Properties`、`Outputs`、`Graph`……），
把材质写成它骨子里本来的样子：**带声明的 HLSL**。过去需要节块的元数据，
现在寄居在 `///` 文档注释和 `#pragma` 行上，所以一个 `.dss` 文件就是一份
源，没有旁文件，也没有剥离步骤。

| | |
| :-- | :-- |
| 扩展名 | `.dss` —— 一个 2.0 编译单元 · `.dsi` —— 一个[材质实例](instances.md) · `.dsp` —— 一条 [Custom Pass 管线](passes.md)（UE 5.8） · `.dsh` —— 一个共享头文件，可以同时装两种语言 |
| 1.x 扩展名 | `.dsm` / `.dsf` —— 由 legacy 前端读取，由同一个编译器构建 |
| 模块 | [`DreamShaderLang`](../api/lang-module.md)（仅 `Core`）—— 两个前端、绑定器、IR、反编译与迁移 · `DreamShaderCompiler`（编辑器）—— 流水线、发射器、资产 · [`DreamShaderPass`](../api/pass-module.md)（运行时）—— 管线资产及其渲染 pass |
| 测试 | `DreamShader.Lang2.*`、`DreamShader.Compiler2.*`、`DreamShader.Pass.*` |
| 状态 | 在 `2.0.0` 中发布：两个前端、绑定器、IR、各 pass、校验器、发射器、节点 ↔ 源码导航、`.dsi`、2.0 反编译器、`dsc migrate`、Substrate 语法糖、IR 图布局、`dsc fmt` |

<a id="two-short-examples"></a>

## 两个简短示例

一个材质 —— 一个 `export void <Name>(inout material m)` 就是入口，而签名是唯一
声明它的东西：

```hlsl
// M_TeleportGlow.dss
#pragma material(ShadingModel = Unlit, BlendMode = Additive)

/// @group Glow|Look   @desc Multiplied on top of the particle colour
uniform float4 Tint = float4(1, 1, 1, 1);

/// @group Glow|Look   @desc Overall emissive gain
uniform float Intensity = 0.7;

float GlowMask(float2 UV)
{
    float2 P = UV * 2.0 - 1.0;
    return pow(saturate(1.0 - dot(P, P)), 2.4);
}

export void M_TeleportGlow(inout material m)
{
    float2 UV = UE.TexCoord(Index = 0);
    m.EmissiveColor = Tint.rgb * GlowMask(UV) * Intensity;
}
```

一个材质函数 —— 额外输出是 HLSL 的 `out` 参数，可选输入是默认
参数，而 `extern` 把一个原型绑定到一个已经存在的资产：

```hlsl
// MF_ToonUV.dss
/// @asset /MoonToon/MaterialFunctions/Utils/MF_UVChannelSwitch
extern float2 MF_UVChannelSwitch(float UVChannelIndex);

/// @library MoonToon|Shared
/// @param UVChannel    Which UV channel to read.
export float2 MF_ToonUV(float UVChannel = 0.0, float4 ScaleOffset = float4(1, 1, 0, 0))
{
    float2 Selected = MF_UVChannelSwitch(UVChannel);
    return Selected * ScaleOffset.xy + ScaleOffset.zw;
}
```

<a id="what-the-parser-accepts"></a>

## 解析器接受什么

<a id="declarations"></a>

### 声明

| 形式 | 说明 |
| :-- | :-- |
| `uniform <Type> Name = <init>;` | 一个材质参数。`uniform bool` 默认是动态标量；`/// @static` 则要求一个静态开关。 |
| `static const <Type> Name = <init>;` | 一个编译期常量。`static`、`const` 和一个裸全局也接受。 |
| `<Type> Name(params) { … }` | 一个文件局部的辅助函数；没有链接关键字。 |
| `export <Type> Name(params) { … }` | 产出一个资产。`void (inout material)` 表示材质；其他任何东西表示材质函数。 |
| `extern <Type> Name(params);` | 一个通过 `/// @asset` 绑定到已存在资产的原型。没有函数体。 |
| `struct Name { … };` | 字段可以自带 `///` 块和数组维度。没有方法，没有嵌套。 |
| `#include "path"` · `import "path";` | 同一回事；拼写会被记录下来，好让打印器复现所写的内容。路径必须是**双引号**的：`#include <Engine/Private/Common.ush>` 是 HLSL 的拼写，不是这里的，而且是 `DSH3203`。解析器不打开文件；绑定器读取并绑定它。 |
| `Ns::Fn(…)` | 对被包含头文件中某个 1.x `Namespace` 成员的调用，它被声明为 `Ns_Fn`；带限定符的拼写由打印器保留 *（2.1.2 之后）*。见 [Namespace](../language/namespace.md#calling-a-namespaced-function)。 |
| `#pragma material(Key = Value, …)` | 文件级材质设置。 |
| `#pragma layout(Node\|Comment, Key = Value, …)` | 反编译器写下的节点坐标。 |
| `#pragma region Name` · `#pragma endregion` | 1.x `#Region` 注释框的 2.0 拼写。 |
| `#pragma generate(Functions = "MF_{Name}", Library = …, Desc = …)` | *（2.1.2 之后）* 在包的头部：它之后的每个辅助函数都生成一个材质函数。见 [打包 ▸ 资产头文件](../../tools/packages.md#asset-headers)。 |
| `#pragma nogeneration` | *（2.1.2 之后）* 只有点名该文件的编译才会构建它的资产。见 [NoGeneration](../generation/no-generation.md)。 |
| `#pragma disable_warning(DSH…, …)` · `#pragma disable_error(DSH…, …)` | *（2.1.2 之后）* 该文件不报告的诊断码。见 [禁用诊断](disable-diagnostics.md)。 |

类型：`float half double int uint bool`、它们的向量（`float2`..`float4`）和矩阵
（`float2x2`..`float4x4`）、`Texture2D TextureCube Texture2DArray Texture3D VolumeTexture`、
`SamplerState`、`material`、`Substrate`、`void`，以及任何其他名字 —— 一个用户 `struct`，或一个
稍后由语义 pass 判定的拼写。

<a id="-doc-blocks"></a>

### `///` 文档块

声明上方的 `///` 行就是它的文档块。它们会被收集，直到第一个不是 `///` 行的 token，所以它们之间的空行不会结束这个块。逐行来看：
`@key value @key value …` —— 一个键是 `@` + 标识符，它的值一直延伸到下一个 ` @` 或
行尾，所以 `@desc Contact: name@example.com` 会把地址保留在值里。第一个 `@` 之前的文本
是自由文本。键会被转为小写，未知的键会原样保留给
语义 pass。一个 `@` 后面没有跟标识符是一个警告（`DSH3220`），并且无论它落在哪里都会作为普通
文本保留 —— 它不会开始一条指令。一个下面什么都没有的块什么都
不记录：它会被丢弃并给出警告（`DSH3221`），永远不会到达语法树。

今天语法树携带的键包括 `@group`、`@desc`、`@name`、`@sort`、`@slider`、`@default`、
`@sampler`、`@static`、`@asset`、`@library`、`@layer`、`@layerblend`、`@param`、`@custom`、
`@generate` 和 `@nogeneration` —— 解析器把它们全部记录下来；只有 `@custom` 会改变解析，办法是让函数体变成不透明的。
`@nogeneration` *（2.1.2 之后）* 用在 `export` 上是 `#pragma nogeneration` 的单个函数形式；见
[NoGeneration](../generation/no-generation.md)。
`@generate <name>` *（2.1.2 之后）* 要求一个包从这个函数生成一个材质函数
（`@generate none` 让它从 `#pragma generate` 中退出）；它只在包的头部有意义，
在其他任何地方都是 `DSH7234` —— 见 [打包 ▸ 资产头文件](../../tools/packages.md#asset-headers)。

<a id="statements-and-expressions"></a>

### 语句与表达式

每一种语句：变量声明、表达式、块、`if` / `else`、`for`、`while`、
`do … while`、`return`、`break`、`continue`、`discard`，以及空的 `;`。一个 `for` 初始化可以是一个
声明或一个表达式，而三个部分中的任何一个都可以为空。

完整的 C 表达式优先级、右结合赋值与 `?:`、前缀与后缀 `++`/`--`、
强制转换 `(float3)x`、构造器 `float3(…)`、成员/swizzle/下标链，以及 `{ … }` 初始化
列表（只能作为初始化器）。命名实参 —— `UE.TexCoord(Index = 0)` —— 按命名实参解析；
位置实参不能跟在命名实参之后。

在图函数体里，`condition ? a : b` 的条件只求一次，只有被选中那一侧的
赋值与 `out` / `inout` 写入生效。两侧都从条件之后的状态出发。
``@custom`` 函数体则遵循引擎的 HLSL 规则。

字面量原样保留其词素，所以 `1.0f`、`0x10`、`2u` 和 `.5` 打印回来都
原封不动。

<a id="-custom-bodies"></a>

### `/// @custom` 函数体

一个函数的文档块带着 `@custom` 时，它的函数体被**逐字**捕获，而不是解析：
字符串、注释和 `#` 指令里的花括号不会结束捕获，文本会原样交给
引擎的 shader 编译器。2.0 文件就是这样保住 1.x Custom 节点的行为。

**函数体不是 DreamShaderLang，这个前端对它一无所知。** 只读花括号 ——
它们仍然必须配平，内部的字符串、注释或 `#` 行不能让它失衡。
其余内容都是从源码里逐字切出来的，所以一个在别处会成为词法错误的字符 —— 一个 `$` 寄存器名、
一个 `@` 内建、一个 `'`、一行中间的 `#` ——
在这里不会被任何东西报告。`dsc check --shaders` 才是会抱怨那段文本的地方。
函数体外面一行的同一个 `#` 仍然是 `DSH2106`；这种沉默只限于函数体内。

只有一个例外，而这正是它的意义所在：**函数体内的 `UE.` 节点调用会被提升进
图。** 调用的文本留在原处；编译器构建那个节点，把它接到 Custom 节点的一个额外输入
（`_ds_<Function>_UE0`、`_UE1`、……）上，并把输入的名字放到调用的位置。

```hlsl
/// @custom
float Pulse(float2 UV, float Gain)
{
    float T = UE.Time();                                              // a Time node, wired in
    float Mixed = UE.LinearInterpolate(A = 0.0, B = Gain, Alpha = 0.5); // built once per call site
    return sin(UV.x * 8.0 + T) * Mixed;
}
```

被提升的调用在图里求值，在 HLSL 之外：它可以读函数的形参（它们
被每次调用的实参替换）和文件的全局变量，读不到函数体声明的任何东西
（`DSH6326`）。调用之后可以选出输出（`UE.SceneTexture(...).InvSize`、`[k]`）。
带提升调用的函数不能被另一个 HLSL 函数体调用（`DSH6327`），因为这些输入
属于它自己的节点。

**节点长什么样。** 返回值的函数是一个带该输出的 Custom 节点。一个
`void` 函数的**第一个 `out` 形参是节点的主输出**，其余 `out` 是它的
额外输出，这正是 1.x `Function` 一直生成的节点；一个不带任何 `out` 的
`void` 函数返回一个未使用的 float。``/// @name`` 用在 `@custom` 函数上时设置节点的标题（它的
Description），否则就是函数名。

<a id="what-the-pipeline-does-today"></a>

## 今天这条流水线做什么

一个解析后的文件会发生什么：

- **资产。** 一个 `.dss` 通过 1.x 资产工厂、摘要、来源、原子重建与图布局，编译成
  `UMaterial`、`UMaterialFunction`、材质层或层混合 —— 所以
  分歧检测、*Revert* / *Adopt* / *Detach* 和 Content Browser 的行为
  都和一个 `.dsm` 一样。
- **语义。** 解析名字、给表达式定型，调用按签名或
  引擎的表达式目录匹配，`///` 指令值得到校验，`#include` /
  `import` 路径被读取并绑定（跨文件重复定义的名字是 `DSH4210`，成环是
  `DSH4211`）。
- **两者之间的图 IR**，带常量折叠、结构去重和死节点
  裁剪，以及一个在任何东西触碰资产之前运行的校验器。`dsc dump-ir` 打印它。
- **符号索引**（`dsc index`）与**节点 ↔ 源码导航**：每个发射出的表达式都在资产的
  `DreamShader.SourceSpans` 元数据里带着它的源码行，Material Editor 的节点
  上下文菜单里有 *DreamShader ▸ Open Source Line*，编辑器桥接在
  反方向上回答 `reveal-node`。
- **shader 检查。** `dsc check -Shaders` 构建产物，并把 HLSL 编译错误
  报告到它们来自的源码行。
- **1.x 语言**，经由 legacy 前端：每一条有文档的 1.x 宽松规则都是一条编号规则，
  只适用于 1.x 文本，并在适用处写明（`DSH5275`–`DSH5292`），所以一份 1.x 源码
  构建它一直构建的东西，而一份 `.dss` 保持严格。从未被记录的
  1.x 静默行为会报错，错误信息说明 1.x 当时做了什么（`DSH2200`–`DSH2222`）。
- **材质实例**作为 [`.dsi`](instances.md) 源，会对照其父级的参数检查。
- **Custom Pass 管线**作为 [`.dsp`](passes.md) 源（UE 5.8）：缓冲区与填充
  它们的 pass —— 全屏、mesh、compute —— 在它们指定的注入点，会对照它们使用的材质
  与 HLSL 文件检查，构建成一个 `UDreamPassPipeline` 资产。
- **回来的路。** `dsc decompile` 把材质、函数、层、混合、实例或管线读进
  IR 并打印出 2.0 文本（[反编译器](../tools/decompiler.md)）；[`dsc migrate`](../tools/migrate.md)
  把 1.x 源码改写为 `.dss`。两者都用编译它们写出的文本并比较 IR 来证明。
- **注释在改写后存活。** 解析器把 `//` 和 `/* */` 注释作为 trivia 保留在
  它们所附着的声明或语句上，打印器再把它们写回来。

<a id="pins-defaults-layers-and-blends"></a>

### 引脚、默认值、层与混合

| 功能 | 写法 |
| :-- | :-- |
| 一个 StaticBool 函数引脚 | `/// @static <Parameter>` 用在函数上，对应 `bool` 形参（否则是 `DSH7231`）。其上再写 `if` 就是 StaticSwitch。 |
| 一个图表达式默认值 | `float Row = UE.TexCoord(CoordinateIndex = 1).r` —— 连到输入的 Preview 引脚。常量默认值仍是预览值。 |
| 一个谁都不能覆盖的纹理 | `/// @default /Game/T_Noise` + `static const Texture2D Noise;` —— 一个 TextureObject 节点，不是参数。`@sampler` 适用。 |
| 一个层混合 | `/// @layerblend export void B(material Bottom, material Top, inout material Result)` —— 两个材质输入和一个**从空开始**的结果；`Bottom` / `Top`（还有 `Base`）设置引脚的混合相关性。 |
| 一个材质层 | `/// @layer export void L(inout material m)` —— 输入是可选的，就像引擎自己的层模板那样。 |
| 一个必需引脚留空 | 是警告（`DSH5219`），不是错误：只有材质编译才知道节点是否会为它读默认值。 |
| 一个按节点命名的引脚 | 对于节点按属性命名引脚的类（`Substrate.MoonToonModifier`、TextureSample 的导数引脚、`UE.LandscapeLayerBlend(Layer_Grass = ...)` 按其 `Layers`），目录里没有列出的名字会在构建出的节点上查找（`DSH5291`，在那里被拒绝并给出 `DSH8212`）。引脚自己的属性名总能解析。 |
| 未传入的可选输入 | 在调用节点上保持未连接；被调用方的默认值生效。 |

辅助函数和 `@custom` 默认值使用被调用方的参数。例如，
以 `F(7)` 形式调用的 `float F(float x, float y = x)` 会收到 `y = 7`，包括调用位于
另一个函数内部时。显式实参先在调用方求值，然后才是默认值；
互相依赖的默认值使用声明的参数类型，并按需解析其他默认值。
没有显式值的依赖环是 [DSH6224](../diagnostics/DSH6xxx.md#dsh6224)。
默认值可以初始化辅助函数的输出，但 Custom 的输出必须在其函数体内初始化；
把任何必需的初值通过一个单独的输入引脚传进去。
对 `export` 或 `extern` 材质函数的调用会让省略的引脚留空，并使用资产
自己的默认值；它不会在调用方求原型上的默认值。

<a id="substrate-layout-and-tooling"></a>

### Substrate、布局与工具

| 特性 | 写法 |
| :-- | :-- |
| [Substrate 语法糖](substrate.md) | 对 Substrate 值使用 `A + B`、`A * w`、`lerp(A, B, t)`；`Substrate.Slab(BaseColor = ..., Metallic = ...)`；对 Substrate 值的运行时 `if`；`Substrate S = Substrate.Slab(); S.Roughness = r;`；`#pragma material(Substrate = Legacy \| Bridge \| Native)` |
| 一种读取源码的布局 | *Project Settings ▸ DreamShader ▸ Graph Layout Style* —— [Blocks（默认）、Source Bands 或 Layered](../generation/graph-layout.md#layout-styles)，或 1.x 的 `Classic`；`dsc dump-layout` 不构建任何东西就能画出全部三种 |
| 一个格式化器 | [`dsc fmt`](../../tools/commandlet.md#fmt) —— 打印器，带有检查，拒绝写入它无法担保的文件 |
| 生成的资产，列表 | [`dsc list-generated`](../tools/commandlet.md#list-generated)，用于[源代码管理](../generation/source-control.md) |

<a id="what-the-pipeline-does-not-do-yet"></a>

## 管线*还*没做的事

- **没有材质层*堆栈*。** `@layer` 与 `@layerblend` 产出两种函数，但
  材质层级的层堆栈是未来的一个 `#pragma material` 键。
- **没有 `let` / `auto`，而且节点不是你可以存储的值。** 在读取其输出的
  地方写调用；两个相同的调用会变成一个节点，所以重写它没有任何代价。
- **图里没有矩阵。** 不能被折叠掉的矩阵无论出现在哪里都是 `DSH4361` ——
  包括 `@custom` 引脚，引擎没有对应的矩阵类型。改在 custom
  函数体内部计算它。
- **没有无界循环。** 当循环次数是可证明的常量时（`MaxUnrolledIterations`，默认 64），`for` / `while` / `do` 会被展开；
  其他情况是 `DSH4360`，并指向
  `/// @custom`。分支内部的 `discard` 是 `DSH4362` —— 图里没有它的形式。
- **不能把 `material` 接入 Custom 节点的输入。** 引擎的 custom-expression 翻译器没有
  材质属性输入引脚，所以带 `material` 参数的 `/// @custom` 函数
  是 `DSH6252`。输出可以是属性。
- **没有双曲函数。** `sinh` / `cosh` / `tanh` 没有材质节点；把它们写在 `@custom` 函数体里。
- **解析器内部没有预处理。** 解析器期望文本的 `#if` 已经被
  解析；到达它的杂散 `#if` 在文件作用域是 `DSH3201`，在函数
  体内是 `DSH2160`，修复方法是将该函数标记为 `/// @custom`。
- **没有 `switch`**（`DSH2160`），也没有逗号运算符。
- **`.dsi` 里没有层参数。** 实例文件设置全局参数；只存在于材质层或
  混合上的参数是 `DSH7268`。

<a id="diagnostics"></a>

## 诊断

每个 2.0 前端消息都带有一个稳定的 `DSHnnnn` 代码和一条可本地化的文本。代码是
契约；措辞不是。

| 范围 | 由谁抛出 |
| :-- | :-- |
| `DSH2101`–`DSH2119` | 词法分析器。目前已分配：`2101` 未知字符 · `2102` 未终止的块注释 · `2103` 未终止的字符串 · `2104` 未知转义 · `2105` 格式错误的数字 · `2106` 一个不是其所在行第一个东西的 `#` |
| `DSH2150`–`DSH2189` | 表达式与语句。目前已分配：`2150`–`2155`，`2157`–`2165` |
| `DSH2200`–`DSH2269` | 遗留前端：1.x Graph 语句（`2200`–`2222`）和顶层块（`2240`–`2260`） |
| `DSH2300`–`DSH2349` | `.dsp` 语法：`buffer` 与 `pass` 声明、pass 函数体、绑定 |
| `DSH3200`–`DSH3249` | 声明、类型、`#` 指令、`///` 块。目前已分配：`3200`–`3208`，`3210`，`3211`，`3213`–`3222` |
| `DSH3250`–`DSH3299` | 遗留前端的各个区段：Properties、Settings、Outputs、Inputs、Layout |
| `DSH3300`–`DSH3349` | `.dsp` 声明：`#pragma pipeline`、buffer 与 pass 键、其他地方仅限 `.dsp` 的声明 |
| `DSH4200`–`DSH4299` | 绑定器 —— 名称、类型、表达式、语句、循环、region |
| `DSH4300`–`DSH4349` | IR 校验器 |
| `DSH4350`–`DSH4399` | 降低阶段的拒绝 —— 矩阵、无法证明的循环、分支中的 `discard`、在写入之前读取属性 |
| `DSH4400`–`DSH4449` | `.dsp` 名称与引用：buffer、材质、HLSL 文件、入口、层 |
| `DSH5200`–`DSH5249` | 反射的 `UE.*` 调用、表达式目录、材质属性 |
| `DSH5250`–`DSH5292` | 1.x 调用写法（`5250`–`5265`）和编号的遗留规则（`5275`–`5292`） |
| `DSH5300`–`DSH5329` | `.dss` 中的 Custom Pass 材质节点：引擎闸门及其允许所在位置（`5300`–`5314`）、`UE.DreamPassBuffer` 命名的管线与缓冲区（`5315`–`5329`） |
| `DSH6200`–`DSH6219` | 函数种类、入口、`export` / `extern` |
| `DSH6220`–`DSH6249` | 辅助函数内联 |
| `DSH6250`–`DSH6299` | `/// @custom` HLSL |
| `DSH6300`–`DSH6330` | 1.x `Function` / `GraphFunction` / `Namespace` / `VirtualFunction`，以及从逐字函数体中提升出来的调用 |
| `DSH7200`–`DSH7249` | uniform、`///` 指令、`#pragma material` |
| `DSH7250`–`DSH7270` | `.dsi`：`#pragma instance` 与覆盖 |
| `DSH7300`–`DSH7379` | `.dsp` 检查：pass 种类与注入点、按帧顺序的缓冲区读写、针对材质或 HLSL 文件的绑定、限制 |
| `DSH8200`–`DSH8299` | 发射器、资产创建和管线驱动器（`8240`–`8265`：材质实例） |
| `DSH8300`–`DSH8339` | Custom Pass 发射：管线资产、导出的渲染目标、HLSL 插槽、插槽注册表、快照、预检查 |
| `DSH9020`–`DSH9059` | 工具（`check`、`dump-ir`、`index`、`export-catalog`）以及节点 ↔ 源码导航 |
| `DSH9060`–`DSH9089` | 反编译器：图导入（`9060`–`9074`）、IR 到源码（`9075`–`9084`）、服务（`9085`–`9089`） |
| `DSH9090`–`DSH9099` | `dsc migrate` |
| `DSH9100`–`DSH9109` | `.dsi` 回读与实例源码重写器 |
| `DSH9200`–`DSH9209` | `dsc pass-registry` |
| `DSH9210`–`DSH9229` | 管线反编译、其重新解析检查，以及针对管线的 Adopt Into Source |

每个诊断代码的原因与修复方法，见[诊断索引](../diagnostics/index.md)。

<a id="running-the-tests"></a>

## 运行测试

`dsc.ps1` 以无头方式驱动管线（`compile`、`check`、`dump-ir`、`index`、`export-catalog` ——
见 [Commandlet](../tools/commandlet.md)）；自动化测试套件正是各层被固定下来的方式。

在编辑器中：*Tools ▸ Test Automation*，筛选 `DreamShader.Lang2` 或 `DreamShader.Compiler2`。

无头运行：

```
"F:\UnrealEngine\UE_Moon\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "I:\UnrealProject_Moon\DEV_58\MoonEngineSample\MoonEngineSample.uproject" ^
  -ExecCmds="Automation RunTests DreamShader.Lang2; Quit" ^
  -nullrhi -unattended -nopause -nosplash -log
```

| 测试 | 覆盖内容 |
| :-- | :-- |
| `DreamShader.Lang2.Corpus.*` | `Tests/Corpus/Lang/` 下的测试用例语料库 —— 每个文件一个子测试，断言解析结果、诊断代码、结构计数以及打印/解析/打印的往返 |
| `DreamShader.Lang2.Lexer.*` | token、字面量、注释、`#` 行 |
| `DreamShader.Lang2.Declarations.*` | 存储、链接、`struct`、`#pragma`、`///` 块 |
| `DreamShader.Lang2.Expressions.*` | 优先级、调用、强制转换、构造器、链 |
| `DreamShader.Lang2.Statements.*` | 每一种语句种类、`for` 形状、恢复 |
| `DreamShader.Lang2.Printer.*` | 往返与格式化 |
| `DreamShader.Lang2.Binder.*` | 名称解析、类型、调用匹配、函数种类、指令 |
| `DreamShader.Lang2.IR.*` | 降级、各趟 pass、校验器 |
| `DreamShader.Lang2.CorpusIR.*` | `Tests/Corpus/IR/` —— 输入 `.dss`，输出 `DumpDreamShaderIRText` 基准 |
| `DreamShader.Compiler2.{Smoke,Corpus,Parity}.*` | 端到端到真实资产，以及与同一材质 1.x 孪生版本的 `dump-graph` 差异对比 |

要覆盖新的构造，往语料库树里放一个 `.dss`（外加一个可选的 `.expected.json`）——
不需要 C++，不需要重新编译。基准格式记录在
[`Tests/Corpus/README.md`](../../../Tests/Corpus/README.md) 中。

<a id="see-also"></a>

## 另见

- [Substrate 语法糖](substrate.md) —— 运算符、slab 上的 legacy 参数、逐成员构建的值、`Substrate =`
- [材质实例](instances.md) —— `.dsi`
- [Custom Pass 管线](passes.md) —— `.dsp`，以及运行它们的[运行时](../runtime/index.md)
- [禁用诊断](disable-diagnostics.md) —— `disable_warning`、`disable_error`
- [NoGeneration](../generation/no-generation.md) —— 只由具名 compile 构建的源文件
- [`DreamShaderLang` C++ API](../api/lang-module.md) —— 该模块、其头文件和入口点
- [语言参考（1.x）](../language/index.md) —— 当前随附的语法
- [预处理器](../language/preprocessor.md) —— `#if` 与 define 表，两种语法共用
- [诊断索引](../diagnostics/index.md) —— 每一个 `DSHnnnn`
