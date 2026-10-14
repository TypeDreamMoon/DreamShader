---
title: "Substrate 语法糖"
description: "Substrate 语法糖"
---

<a id="substrate-sugar"></a>

# Substrate 语法糖

> [DreamShader](../index.md) » [DreamShaderLang 2.0](index.md) » **Substrate 语法糖**

给 Substrate 图准备的更短写法。每一条都只是一种拼写，仅此而已：它绑定到一组固定的 [`Substrate.*`](../builtins/substrate.md) 节点模式上，生成的图就是长写法所生成的那一个。
反编译器写长写法，只有在被要求给出可读形式时才写短写法。

| | |
| :-- | :-- |
| 适用于 | `.dss`，以及 `.dsm` / `.dsf` / `.dsh` —— 两门前端都归到同一个绑定器 |
| 需要 | 引擎自带的 Substrate 节点（UE 5.4+）；`Substrate.Select` 需要 UE 5.6 |
| 会不会改变图 | 不会 —— `A + B` **就是** `Substrate.Add(A = A, B = B)` |
| 起始版本 | `2.0.0` |

```hlsl
#pragma material(Substrate = Native)

/// @group Surface
uniform Texture2D Albedo;
uniform float Rust = 0.3;
uniform float Coat = 1.0;

export void M_CoatedMetal(inout material m)
{
    float4 Tex = Albedo.Sample(UE.TexCoord(Index = 0));

    Substrate Metal = Substrate.Slab(BaseColor = Tex.rgb, Metallic = 1.0, Roughness = 0.35);   // S3
    Substrate RustS = Substrate.Slab(BaseColor = Tex.rgb * float3(0.6, 0.3, 0.1), Roughness = 0.9);
    Substrate Body  = lerp(Metal, RustS, saturate(Tex.a * Rust));                              // S1
    Substrate Clear = Substrate.Slab(IOR = 1.5, Roughness = 0.1);                              // S3

    m.FrontMaterial = Substrate.Layer(Clear * Coat, Body, 0.01);                               // S1
}
```

<a id="operators-and-lerp"></a>

## 运算符与 `lerp`

`Substrate` 值不是数。它有两个运算符和一个内建函数，每一个都是一个节点：

| 你写的 | 它是 | 引脚 |
| :-- | :-- | :-- |
| `A + B` | `Substrate.Add` | `A`、`B` |
| `A * w`、`w * A` | `Substrate.Weight` | `A`、`Weight` |
| `lerp(A, B, t)` | `Substrate.HorizontalMix` | `Background`、`Foreground`、`Mix` |

优先级就是语言本身的：`A + B * w` 先给 `B` 加权，再相加。Substrate 值上的其他所有运算符 —— `-`、`/`、`A * B`、一元负号、诸如 `A += B` 这样的复合赋值 ——
都是 [`DSH5293`](../diagnostics/DSH5xxx.md)，把一个 Substrate 值和一个数混在一起的 `lerp` 也是。

五个组合节点还**按位置**接受运算对象，其中两个有短名字：

| 调用 | 位置顺序 |
| :-- | :-- |
| `Substrate.Add(A, B)` | `A`、`B` |
| `Substrate.Weight(A, w)` | `A`、`Weight` |
| `Substrate.Mix(Bg, Fg, t)` —— 还有 `HorizontalMix`、`HorizontalMixing` | `Background`、`Foreground`、`Mix` |
| `Substrate.Layer(Top, Base, Thickness)` —— 还有 `VerticalLayer`、`VerticalLayering` | `Top`、`Base`、`Thickness` |
| `Substrate.Select(A, B, t)` | `A`、`B`、`SelectValue` |

BSDF 有十八个引脚，所以 BSDF 只接受按名字传参。

> [!NOTE]
> 这些运算符取引擎对参数混合（parameter blending）的默认值 —— 关。需要它的源码写具名调用 `Substrate.Mix(A, B, t, UseParameterBlending = true)`，
> 而反编译器会把设置了任何属性的节点保留为它本来所是的那个调用。

<a id="if-and--over-substrate-values"></a>

## Substrate 值上的 `if` 与 `?:`

两边各留下一个不同 Substrate 值的分支也是一个节点：

| 条件是 | 分支变成 |
| :-- | :-- |
| 一个 `/// @static` uniform，或一个 `/// @static` 函数形参 | 一个类型为 `Substrate` 的 `StaticSwitch` —— 其中一边在编译期被裁掉 |
| 任何运行时才定的东西 | `Substrate.Select(A = <else side>, B = <then side>, SelectValue = <condition>)` |

```hlsl
Substrate Front = Dry;
if (Wetness > 0.5)          // run time: a Substrate.Select
{
    Front = Wet;
}
m.FrontMaterial = Front;
```

`Substrate.Select` 从 UE 5.6 起才有；更老的引擎上运行时形式是
[`DSH4378`](../diagnostics/DSH4xxx.md)，它会说明这一点并给出两条出路 —— 把条件改成静态的，
或者对两个值做 `lerp`。Select 会对输入做参数混合，而引擎会拒绝混合某些不同类的 BSDF 组合；
当两边是不同节点类时，编译器会给出警告 [`DSH4380`](../diagnostics/DSH4xxx.md) 并照样生成节点，因为哪些组合可行是引擎自己的事，由它自己上报。

<a id="legacy-parameters-on-a-slab"></a>

## Slab 上的旧参数

`Substrate.Slab` 的参数化是 `DiffuseAlbedo` 和 `F0`，而这两样谁也没有对应的贴图。
下面这些实参**不是**节点的引脚；每一族都是某个转换节点的输入侧，编译器会把这个转换节点接在它所代表的引脚前面：

| 实参 | 转换节点 | 接到 |
| :-- | :-- | :-- |
| `BaseColor`、`Metallic`、`Specular` | `Substrate.MetalnessToDiffuseAlbedoF0` | `DiffuseAlbedo`、`F0` |
| `Haziness` —— 相对于本次调用自己的 `Roughness` | `Substrate.HazinessToSecondaryRoughness` | `SecondRoughness`、`SecondRoughnessWeight` |
| `Transmittance`、`Thickness` | `Substrate.TransmittanceToMFP` | `SSSMFP` |
| `IOR` | 没有：`F0 = ((n − 1) / (n + 1))²` | `F0` |

一个节点只要具备某一族所接的全部引脚，就接受那一族：`Substrate.Slab` 四族全收，
`Substrate.SimpleClearCoat` 收以 `F0` 结尾的那两族。名字本身**就是**节点引脚的，就按引脚处理 ——
`Substrate.ShadingModels(BaseColor = …)` 不转换任何东西。

- 省略的实参在转换节点上保留引擎的默认值（`BaseColor` 0.18、
  `Specular` 0.5、`Metallic` 0）。
- 为数字的 `IOR` 折成常量 `F0`（`1.5` → `0.04`）；算出来的则是四个
  算术节点。
- 转换节点是普通节点，所以两个 slab 转换的相同 `BaseColor` 和 `Metallic`
  共用一个 —— 与任何两个相等的调用得到的合并一样。

| 错误 | 代码 |
| :-- | :-- |
| 给同一组引脚起了两个名字 —— `BaseColor` 与 `DiffuseAlbedo` 或 `F0`、`IOR` 与 `Metallic` / `Specular` / `BaseColor` / `F0`、`Haziness` 与 `SecondRoughness`、`Transmittance` 与 `SSSMFP` | [`DSH5295`](../diagnostics/DSH5xxx.md) |
| 有 `Haziness` 没 `Roughness`、有 `Thickness` 没 `Transmittance` | [`DSH5296`](../diagnostics/DSH5xxx.md) |
| 同一个实参写两次 | `DSH4215` |
| 本引擎缺少该转换节点 | [`DSH4381`](../diagnostics/DSH4xxx.md) |

<a id="building-a-value-member-by-member"></a>

## 按成员逐步构建值

一个声明为**不带实参**的节点调用的 `Substrate` 局部变量，是一个「还在构建中」的值：

```hlsl
Substrate S = Substrate.Slab();      // no arguments: a builder. With arguments it is a finished node.
S.BaseColor = Albedo;                // a pin, or an argument of the table above -- in any order
S.Metallic  = 1.0;
#if DS_HAS_FUZZ
S.FuzzAmount = Fuzz;                 // the preprocessor cuts lines, which is what this form is for
#endif
m.FrontMaterial = S;                 // the first use of S makes the node
```

| 规则 | |
| :-- | :-- |
| 写成员会连接一个引脚 | `S.Pin = x`；`S.Pin *= x` 和 `++S.Pin` 从成员当前持有的值出发 |
| 最后一次写入生效 | 第二次 `S.Roughness = …` 会替换第一次 |
| 读成员不生成节点 | `S.Roughness` 就是当初写进去的值；读一个从没写过的成员是 [`DSH5299`](../diagnostics/DSH5xxx.md) |
| 第一次使用即封印该值 | 赋值给 `S`、传参、用作运算对象。之后再写成员是 [`DSH5297`](../diagnostics/DSH5xxx.md) |
| 声明在某个 `if` 之外时，不能在该分支内写成员 | 那会变成同一个节点的两个版本 —— [`DSH5298`](../diagnostics/DSH5xxx.md)。构建两个值并在它们之间选择。**在分支内部**声明的 builder 属于那个分支自己。`#if` 不是 `if`。 |
| 整体赋值的 builder 之后就没有成员了 | `S = T;` 然后 `S.Roughness` 是 [`DSH5297`](../diagnostics/DSH5xxx.md) |
| 引脚是整体连接的 | `S.DiffuseAlbedo.r = 1` 就是 `DSH4229` |
| 调用在调用处被求值，builder 则在第一次使用时被求值 | 在那里不带 `Roughness` 的 `Haziness` 是 [`DSH5296`](../diagnostics/DSH5xxx.md) |

节点在值第一次被使用的地方生成，而不是在声明处，而且走的是调用所走的同一条路径 ——
所以 `Substrate.Slab(Roughness = r, BaseColor = c)` 和构建它的那四行不可能得出不同的图，反编译器也总是写调用形式。

> [!NOTE]
> 绑定器从上到下把主体读一遍。它看不到的是展开循环绕回来：在文本中位于使用点*上方*、却在第二轮*
> 之后*才运行的写入，会转而由 IR 构建器拒绝，如 [`DSH4383`](../diagnostics/DSH4xxx.md)。请在循环内声明该值。

<a id="frontmaterial-of-a-material-that-arrived-through-a-pin"></a>

## 经由引脚到来的材质 `FrontMaterial`

`m.FrontMaterial` 在存在 `material` 的地方就是 Substrate 值：像任何属性一样书写，读取时 ——
从图层的输入、混合的 `Bottom` 和 `Top`、`UE.BlendMaterialAttributes` 的结果 —— 通过
`GetMaterialAttributes` 节点，因为引擎的 Break 节点没有这样的输出。这正是图层混合能够携带 Substrate 的原因：

```hlsl
/// @layerblend
export void MLB_Wet(material Bottom, material Top, float Alpha, inout material R)
{
    R.FrontMaterial = lerp(Bottom.FrontMaterial, Top.FrontMaterial, Alpha);
}
```

<a id="one-source-two-kinds-of-project--substrate-"></a>

## 一份源码，两种工程 —— `Substrate =`

`#pragma material(Substrate = …)` 说明：一份按旧属性写的材质，在开了 Substrate 的工程里该怎么读。

| 模式 | 工程**没开** Substrate | 工程**开了** Substrate |
| :-- | :-- | :-- |
| `Legacy` *（默认）* | 照原样 | 照原样 —— 引擎在加载资产时自己转换旧属性 |
| `Bridge` | 照原样 | **Surface** 材质的着色属性被收进接在 `FrontMaterial` 上的一个 `Substrate.ShadingModels` 节点，逐个引脚与引擎自己的转换一致 —— 见下表 |
| `Native` | 驱动 `FrontMaterial` 是 [`DSH4382`](../diagnostics/DSH4xxx.md)：引擎无法编译该资产 | 照原样 |

`Bridge` 收哪些属性，与引擎转换旧 Surface 材质时收的完全一样，所以
资产读起来就是引擎加载时会转出来的那一个 —— 区别只在于：这个节点在你构建、diff、反编译的那张图里：

| 属性 | `Substrate.ShadingModels` 的引脚 | 之后还留在材质上吗 |
| :-- | :-- | :-- |
| `BaseColor`、`Metallic`、`Specular`、`Roughness`、`Anisotropy`、`EmissiveColor`、`Tangent` | 同名引脚 | 否 |
| `SubsurfaceColor` | `SubSurfaceColor` | 否 |
| `ClearCoat`、`ClearCoatRoughness`（`CustomData0` / `CustomData1`） | `ClearCoat`、`ClearCoatRoughness` | 否 |
| `Normal`、`Opacity` | 同名引脚 | **是** —— 引擎对这两个是「复制」，`Bridge` 也一样 |
| `ShadingModel`（设置为 `FromMaterialExpression` 的材质） | `ShadingModel` | **是** |
| 其余 —— `OpacityMask`、`WorldPositionOffset`、`PixelDepthOffset`、`Displacement`、`Refraction`、`AmbientOcclusion`、`SurfaceThickness`、Customized UV | — | 留在原处 |

节点的 `ShadingModelOverride` 取材质的 `ShadingModel` 设置；自己计算
着色模型的材质不动 override，改为驱动那个引脚。一个材质如果没有写任何「会被移走」的属性，就没有可桥接的东西，保持原样。

「开了 Substrate」就是内建宏 [`DS_SUBSTRATE`](../language/preprocessor.md)，凡是写了 `Bridge` 或 `Native` 的材质，它都会进入
[build key](../generation/caching.md) —— 切换工程设置会重建这些材质。驱动 `FrontMaterial` 自身的 `Bridge` 材质
保持原样；其他域（domain）的材质也一样，引擎会用它自己的节点来转换。未知模式是
[`DSH7232`](../diagnostics/DSH7xxx.md)。

<a id="names-and-engines-that-lack-a-node"></a>

## 名字，以及缺少某个节点的引擎

引擎的每个 Substrate 节点类 —— 以及已加载插件的那些 —— 都可调用，不管
DreamShader 是否听说过它：一个类用它自己的类名，去掉前面的 `MaterialExpressionSubstrate`、
去掉后面的 `BSDF`，所以 UE 5.8 的 `UMaterialExpressionSubstrateToonBSDF` 就是 `Substrate.Toon`，
插件无需改动。[`Substrate.*`](../builtins/substrate.md) 里的名字就是这些名字再加上
几个别名（`HorizontalMix` / `Mix`、`VerticalLayer` / `Layer`）。

当某个语法糖需要本引擎没有的节点时，消息会点出拥有它的引擎 ——
`Substrate.Select` 自 5.6 起、`Substrate.Toon` 自 5.8 起 —— 而不是报告一个未知名字
（[`DSH5294`](../diagnostics/DSH5xxx.md)）。

<a id="decompiling"></a>

## 反编译

反编译器把每个 Substrate 节点都写成它本来的调用，名字用源码里的写法 ——
`Substrate.Slab`，而不是反射出来的 `SubstrateSlabBSDF`。Coverage Weight 写成 `Substrate.Weight(A = …, Weight = …)`，
slab 前面的 Transmittance-To-MFP 写成 `SSSMFP = Substrate.TransmittanceToMFP(…).MFP`，所以在文本里搜图里看得到的节点，搜得到。

语法糖只在可读形式里写回（命令行的 `-Readable`、bridge 的
`readable`），而且只在与图完全一致的地方：

| 图里是 | 可读文本写成 |
| :-- | :-- |
| `SubstrateAdd` / `SubstrateWeight` / `SubstrateHorizontalMixing`，每个引脚都接了、没设任何属性 | `A + B`、`A * w`、`lerp(A, B, t)`，放在以节点命名的局部变量里（`SubstrateWeight`） |
| 输出只接一个 BSDF、自身什么都没设的转换节点 | 那个 BSDF 上的 `BaseColor = …`、`Metallic = …`、`Haziness = …`、`Transmittance = …` |

两种形式都会写成：

| 图里是 | 文本说 |
| :-- | :-- |
| 产物写着 `Bridge` 或 `Native` 的材质 | `#pragma material(Substrate = …)` |

`IOR` 是那个不会被读回的：常量 `F0` 说明不了它曾是折射率。折过的
`Bridge` 材质反编译成它变成的那个 `Substrate.ShadingModels` 调用。

<a id="1x-sources"></a>

## 1.x 源码

语法糖是绑定器的，所以 `.dsm` 用自己的一套拼写拥有全部这些 —— `float3` 写成 `vec3`，
以及 2.0 写 `m.X =` 的地方写成 `Outputs` 段：

```c
Shader(Name = "Materials/M_CoatedMetal")
{
    Outputs {
        Base.FrontMaterial = Substrate.Layer(Clear * Coat, Body, 0.01);   // an expression, not a variable name
    }
    Graph {
        Substrate Metal = Substrate.Slab(BaseColor = Tint.rgb, Metallic = 1.0, Roughness = 0.35);
        Substrate Body  = lerp(Metal, RustS, saturate(Rust));
        Substrate Clear = Substrate.Slab(IOR = 1.5, Roughness = 0.1);
    }
}
```

`Outputs` 绑定的右侧本身就是一个表达式；它在 `Graph` 跑完之后
求值。`Outputs` 把一切都算完的 `Shader` 根本不需要 `Graph` 段。

<a id="diagnostics"></a>

## 诊断

| 代码 | 严重性 | 说明 |
| :-- | :-- | :-- |
| `DSH5293` | error | Substrate 值没有的运算符；对一个 Substrate 值和一个数做 `lerp` |
| `DSH5294` | error | 语法糖需要本引擎没有的节点，以及哪个引擎有它 |
| `DSH5295` | error | 同一组引脚有两个名字 |
| `DSH5296` | error | 有 `Haziness` 没 `Roughness`；有 `Thickness` 没 `Transmittance` |
| `DSH5297` | error | 在值被使用之后写成员；被整体赋值的 builder 的成员 |
| `DSH5298` | error | 声明在某个 `if` 外面的成员写入 |
| `DSH5299` | error | 没有这个引脚；读取一个从没写过的成员 |
| `DSH4378` | error | 在不支持 `Substrate.Select` 的引擎上，对 Substrate 值做运行时分 |
| `DSH4380` | warning | 不同 BSDF 之间的 `Substrate.Select` |
| `DSH4381` | error | 本引擎没有的转换节点 |
| `DSH4382` | error | Substrate 关闭时用 `Substrate = Native` 驱动 `FrontMaterial` |
| `DSH4383` | error | 展开循环绕回到值被使用或赋值之后的 builder 成员 |
| `DSH7232` | error | 未知的 `Substrate =` 模式 |

<a id="see-also"></a>

## 另见

- [`Substrate.*`](../builtins/substrate.md) —— 这些节点本身，逐个引脚
- [材质设置](../settings/material.md) —— `#pragma material(...)`
- [预处理器](../language/preprocessor.md) —— `DS_SUBSTRATE`，以及 `#if` 围绕 builder 行的用法
- [反编译器](../tools/decompiler.md)
