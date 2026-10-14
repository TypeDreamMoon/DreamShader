---
title: Substrate 语法糖
description: Substrate 值上的运算符与 lerp、Slab 上的旧参数化实参、运行时分支、按成员逐步构建的值，以及一份源码同时适配开与不开 Substrate 的工程。
---

给 Substrate 图准备的更短的写法。每一条都**只是拼写**：它绑定到一组固定的 `Substrate.*` 节点模式上，
生成的图和长写法完全一样。反编译器默认写长写法，只在要可读形式时才把短写法写回来（见[反编译](#反编译)）。两门语言都有 —— `.dsm` 和 `.dss` 一视同仁。

| 项目 | 值 |
| :-- | :-- |
| 需要 | 引擎自带的 Substrate 节点（UE 5.4+）；`Substrate.Select` 需要 UE 5.6 |
| 会不会改变图 | 不会 —— `A + B` **就是** `Substrate.Add(A = A, B = B)` |
| 起始版本 | （自 2.0.0 起） |

```hlsl
#pragma material(Substrate = Native)

uniform Texture2D Albedo;
uniform float Rust = 0.3;
uniform float Coat = 1.0;

export void M_CoatedMetal(inout material m)
{
    float4 Tex = Albedo.Sample(UE.TexCoord(Index = 0));

    Substrate Metal = Substrate.Slab(BaseColor = Tex.rgb, Metallic = 1.0, Roughness = 0.35);
    Substrate RustS = Substrate.Slab(BaseColor = Tex.rgb * float3(0.6, 0.3, 0.1), Roughness = 0.9);
    Substrate Body  = lerp(Metal, RustS, saturate(Tex.a * Rust));
    Substrate Clear = Substrate.Slab(IOR = 1.5, Roughness = 0.1);

    m.FrontMaterial = Substrate.Layer(Clear * Coat, Body, 0.01);
}
```

<a id="运算符与-lerp"></a>

## 运算符与 lerp

Substrate 值不是数。它有两个运算符和一个内建函数，每一个都是一个节点。

| 你写的 | 它是 | 引脚 |
| :-- | :-- | :-- |
| `A + B` | `Substrate.Add` | `A`、`B` |
| `A * w`、`w * A` | `Substrate.Weight` | `A`、`Weight` |
| `lerp(A, B, t)` | `Substrate.HorizontalMix` | `Background`、`Foreground`、`Mix` |

优先级就是语言本身的：`A + B * w` 先给 `B` 加权，再相加。Substrate 值上的其他运算符 —— `-`、`/`、`A * B`、`A += B` ——
是 `DSH5293`；把一个 Substrate 值和一个数混在一起的 `lerp` 也是。

五个组合节点还可以**按位置**传实参，其中两个有短名字：
`Substrate.Add(A, B)`、`Substrate.Weight(A, w)`、`Substrate.Mix(Bg, Fg, t)`、
`Substrate.Layer(Top, Base, Thickness)`、`Substrate.Select(A, B, t)`。BSDF 有十八个引脚，所以 BSDF 只接受按名字传参。

**提示：**

运算符取引擎对参数混合（parameter blending）的默认值 —— 关。需要它的源码写具名调用：
`Substrate.Mix(A, B, t, UseParameterBlending = true)`。

<a id="substrate-值上的分支"></a>

## Substrate 值上的分支

| 条件是 | 分支变成 |
| :-- | :-- |
| `/// @static` 的 uniform 或函数形参 | 类型为 Substrate 的 `StaticSwitch` —— 其中一边在编译期被裁掉 |
| 任何运行时才定的东西 | `Substrate.Select`：`else` 一侧接 `A`，`then` 一侧接 `B`，条件接 `SelectValue` |

`Substrate.Select` 从 UE 5.6 起才有；更老的引擎上运行时形式是 `DSH4378`，消息里给出两条出路：把条件改成静态的，
或者对两个值做 `lerp`。Select 会对两个输入做参数混合，而引擎会拒绝某些不同类 BSDF 的组合，
所以两边是不同节点类时会得到一条警告 `DSH4380`，节点照样生成。

<a id="slab-上的旧参数"></a>

## Slab 上的旧参数

`Substrate.Slab` 的参数化是 `DiffuseAlbedo` 和 `F0`，而没人手里有这两样的贴图。下面这些实参**不是引脚**；
每一族都是某个转换节点的输入侧，编译器会把这个转换节点接在它所代表的引脚前面。

| 实参 | 转换节点 | 接到 |
| :-- | :-- | :-- |
| `BaseColor`、`Metallic`、`Specular` | `Substrate.MetalnessToDiffuseAlbedoF0` | `DiffuseAlbedo`、`F0` |
| `Haziness` —— 相对于本次调用自己的 `Roughness` | `Substrate.HazinessToSecondaryRoughness` | `SecondRoughness`、`SecondRoughnessWeight` |
| `Transmittance`、`Thickness` | `Substrate.TransmittanceToMFP` | `SSSMFP` |
| `IOR` | 没有节点：F0 = ((n − 1) / (n + 1))² | `F0` |

一个节点只要具备某一族所接的全部引脚，就接受那一族 —— `Slab` 四族全收，`SimpleClearCoat` 收落在 `F0` 上的那两族 ——
而名字本身**就是**该节点引脚的，就按引脚处理。`IOR` 是数字时折成常量 `F0`（`1.5` 得 `0.04`）；
两个 slab 转换的是同一组值时共用一个转换节点。给同一组引脚起了两个名字是 `DSH5295`；
有 `Haziness` 没 `Roughness`、有 `Thickness` 没 `Transmittance` 是 `DSH5296`。

<a id="按成员逐步构建"></a>

## 按成员逐步构建

用**不带实参**的节点调用声明的 Substrate 局部变量，是一个「还在构建中」的值。

```hlsl
Substrate S = Substrate.Slab();      // 没有实参：builder
S.BaseColor = Albedo;                // 引脚，或上表里的实参 —— 顺序随意
S.Metallic  = 1.0;
#if DS_HAS_FUZZ
S.FuzzAmount = Fuzz;                 // 预处理器按行裁剪，这种写法正是为它准备的
#endif
m.FrontMaterial = S;                 // 第一次使用 S 时才生成节点
```

| 规则 | 说明 |
| :-- | :-- |
| 最后一次写入生效 | `S.Pin *= x` 和 `++S.Pin` 从成员当前持有的值出发 |
| 读成员不产生节点 | 读到的就是当初写进去的值；读一个从没写过的成员是 `DSH5299` |
| 第一次使用即封印 | 之后再写成员是 `DSH5297` |
| 声明在 `if` 外面时，不能在 `if` 里面写成员 | 那会变成「同一个节点的两个版本」—— `DSH5298`。在分支内部声明的 builder 属于那个分支自己；`#if` 不是 `if`。 |

节点在值第一次被使用的地方生成，而且走的是和调用相同的路径，
所以 builder 与 `Substrate.Slab(Roughness = r, BaseColor = c)` 不可能生成出不同的图。

<a id="承载-substrate-的层混合"></a>

## 承载 Substrate 的层混合

凡是有 `material` 的地方，`m.FrontMaterial` 就是一个 Substrate 值 —— 材质经由引脚进来时也一样，
此时通过 GetMaterialAttributes 节点读取。

```hlsl
/// @layerblend
export void MLB_Wet(material Bottom, material Top, float Alpha, inout material R)
{
    R.FrontMaterial = lerp(Bottom.FrontMaterial, Top.FrontMaterial, Alpha);
}
```

<a id="一份源码两种工程"></a>

## 一份源码，两种工程

`#pragma material(Substrate = ...)` 说明：一个按旧属性写的材质，在开了 Substrate 的工程里该怎么读。

| 模式 | 工程没开 Substrate | 工程开了 Substrate |
| :-- | :-- | :-- |
| `Legacy`（默认） | 照原样 | 照原样 —— 引擎自己转换旧属性 |
| `Bridge` | 照原样 | Surface 材质的着色属性被收进接在 `FrontMaterial` 上的一个 `Substrate.ShadingModels` 节点，逐个引脚与引擎自己的转换一致 —— 见下表 |
| `Native` | 驱动 `FrontMaterial` 是 `DSH4382` | 照原样 |

`Bridge` 收哪些属性，与引擎转换旧 Surface 材质时收的完全一样，所以资产读起来就是引擎加载时会转出来的那一个 ——
区别只在于：这个节点在你构建、diff、反编译的那张图里。

| 属性 | `Substrate.ShadingModels` 的引脚 | 之后还留在材质上吗 |
| :-- | :-- | :-- |
| `BaseColor`、`Metallic`、`Specular`、`Roughness`、`Anisotropy`、`EmissiveColor`、`Tangent` | 同名引脚 | 否 |
| `SubsurfaceColor` | `SubSurfaceColor` | 否 |
| `ClearCoat`、`ClearCoatRoughness`（即 `CustomData0` / `CustomData1`） | `ClearCoat`、`ClearCoatRoughness` | 否 |
| `Normal`、`Opacity` | 同名引脚 | **是** —— 引擎对这两个是「复制」，`Bridge` 也一样 |
| `ShadingModel`（设置为 `FromMaterialExpression` 的材质） | `ShadingModel` | **是** |
| 其余 —— `OpacityMask`、`WorldPositionOffset`、`PixelDepthOffset`、`Displacement`、`Refraction`、`AmbientOcclusion`、`SurfaceThickness`、Customized UV | — | 留在原处 |

节点的 `ShadingModelOverride` 取材质的 `ShadingModel` 设置；自己计算着色模型的材质不动 override，改为驱动那个引脚。
一个材质如果没有写任何「会被移走」的属性，就没有可桥接的东西，保持原样。

`ShadingModel` 引脚 （自 2.0.1 起） 起才接得上：引擎把它声明成材质自身引脚的类型，之前的目录不认它，
`Bridge` 会把计算出来的着色模型（`m.ShadingModel = UE.ShadingModel(...)`）漏在节点外面。
这个引脚不是必需的 —— 没接时节点读自己的 `ShadingModelOverride`。

「开了 Substrate」就是内建宏 `DS_SUBSTRATE`；凡是写了 `Bridge` 或 `Native` 的材质，它都会进入 build key ——
切换工程设置时，恰好重建这些材质。

<a id="反编译"></a>

## 反编译

（自 2.0.2 起） 反编译器把每个 Substrate 节点都写成它本来的调用，名字用源码里的写法 —— `Substrate.Slab`，
而不是反射出来的 `SubstrateSlabBSDF`。Coverage Weight 写成 `Substrate.Weight(A = …, Weight = …)`，
slab 前面的 Transmittance-To-MFP 写成 `SSSMFP = Substrate.TransmittanceToMFP(…).MFP`：图里看得到的节点，在文本里搜得到。

语法糖只在**可读形式**里写回（命令行的 `-Readable`、bridge 的 `readable`），而且只在与图完全一致的地方：

| 图里是 | 可读文本写成 |
| :-- | :-- |
| 每个引脚都接了、没设任何属性的 `SubstrateAdd` / `SubstrateWeight` / `SubstrateHorizontalMixing` | `A + B`、`A * w`、`lerp(A, B, t)`，放在以节点命名的局部变量里（`SubstrateWeight`） |
| 输出只接一个 BSDF、自身什么都没设的转换节点 | 那个 BSDF 上的 `BaseColor = …`、`Metallic = …`、`Haziness = …`、`Transmittance = …` |

两种形式都会写出 `#pragma material(Substrate = …)`（产物记着 `Bridge` 或 `Native` 时）。`IOR` 不会被读回 ——
常量 `F0` 说明不了它曾是折射率；折过的 `Bridge` 材质反编译成它变成的那个 `Substrate.ShadingModels` 调用。
用语法糖写的源码照旧编译。见[反编译器](../tools/decompiler.md)。

<a id="在-1x-源码里"></a>

## 在 1.x 源码里

```c
Shader(Name = "Materials/M_CoatedMetal")
{
    Outputs {
        Base.FrontMaterial = Substrate.Layer(Clear * Coat, Body, 0.01);   // 是表达式，不是变量名
    }
    Graph {
        Substrate Metal = Substrate.Slab(BaseColor = Tint.rgb, Metallic = 1.0, Roughness = 0.35);
        Substrate Body  = lerp(Metal, RustS, saturate(Rust));
        Substrate Clear = Substrate.Slab(IOR = 1.5, Roughness = 0.1);
    }
}
```

`Outputs` 绑定的右侧本身就是一个表达式，在 `Graph` 跑完之后求值；`Outputs` 把一切都算完的 `Shader` 不需要 `Graph` 段。
