---
title: "Swizzle"
description: "Swizzle"
---

<a id="swizzle"></a>

# Swizzle

> [DreamShader](../index.md) » [Graph](index.md) » **Swizzle**

对数值使用后缀成员访问，用于选择、重排或重复其通道。

| | |
| :-- | :-- |
| 声明于 | `.dsm`, `.dsf` — 位于 `Graph { … }` 体内、`Outputs` 绑定表达式内，或 `Outputs` 声明默认值中 |
| 类型 | 表达式，后缀 |
| 生成 | 对于升序掩码生成一个 `UMaterialExpressionComponentMask`；当源节点恰好为这些通道提供输出引脚时生成**无**；对于重排或重复掩码，每个通道生成一个掩码加 `UMaterialExpressionAppendVector` × (N−1) |

<a id="synopsis"></a>

## 概要

```c
<swizzle> := <expression> . <channels>
<channels> := <channel> [ <channel> ] [ <channel> ] [ <channel> ]
<channel>  := { x | y | z | w | r | g | b | a }
```

一到四个通道字符。`.` 和通道字母是字面量；`[ … ]` 和
`{ a | b }` 是元记号。

<a id="channel-sets"></a>

## 通道集合

| 字符 | 通道索引 |
| :-- | :-- |
| `x`, `X`, `r`, `R` | 0 |
| `y`, `Y`, `g`, `G` | 1 |
| `z`, `Z`, `b`, `B` | 2 |
| `w`, `W`, `a`, `A` | 3 |

- 恰好存在**两**个集合：`xyzw` 和 `rgba`。没有 `stpq` 集合，也没有 `uv` 集合 — `u`
  和 `v` 均无法解析，因此 `.uv` 会失败并产生 [`DSH4230`](../diagnostics/DSH4xxx.md#dsh4230)。
- 通道字符**不区分大小写**进行匹配：`.RGB`, `.Rgb`, `.XyZ` 均有效。
- **在一个 swizzle 中混用两个集合是 `DSH4230`** *（自 2.0.0 起；1.x 将 `.xg` 视为 `.xy`）*。

<a id="length-and-bounds"></a>

## 长度与边界

| 规则 | 违规 |
| :-- | :-- |
| 一到四个通道字符 | 5 个或更多：`DSH4230` |
| 每个字符必须是通道字母 | 未知字母：`DSH4230` |
| 每个通道索引必须**小于**基础值的分量数 | 超出范围：`DSH4230` |
| 每个 swizzle 一个集合 | `.xg`: `DSH4230` |

swizzle 只能缩小或重排；它永远无法读取超出基础宽度的内容。

| 基础 | Swizzle | 结果 |
| :-- | :-- | :-- |
| `float4` | `.rgb` | `float3` |
| `float3` | `.a` | 错误 — 通道 3 ≥ 3 个分量 |
| `float2` | `.xyz` | 错误 — 通道 2 ≥ 2 个分量 |
| `float` | `.x`, `.r` | 基础值 |
| `float` | `.xx`, `.rrr` | 散布为 `float2` / `float3` |
| `float` | `.y`, `.z`, `.w`, `.g`, `.b`, `.a` | **错误** |

> [!WARNING]
> **标量不会通过超出范围的通道进行散布。** 对标量参数使用 `Roughness.yz` 是一个硬错误，而不是两分量广播：`DSH4230`。要广播标量，请重复通道 0
> （`Roughness.xx`）、使用构造函数（`float2(Roughness)`），或依赖
> 赋值时的[标量拓宽](conversions.md#widening)。

<a id="lowering"></a>

## 降级

*（自 2.0.0 起）* swizzle 是一个节点：生成器从不在连接上放置通道掩码。以下四种
情况按此顺序尝试。

| # | 适用条件 | 创建的节点 | 结果 |
| :-- | :-- | :-- | :-- |
| 1 | 源节点恰好将所选通道作为输出引脚发布 — 纹理采样的 `.rgb` 或 `.a`、向量参数的 `.rgb` 或 `.r` | **无** | 读取该引脚 |
| 2 | 通道索引**严格递增且无重复** | 一个 `ComponentMask` | 所选通道 |
| 3 | 基础有 2–4 个分量且掩码被重排或重复 | 每个不同通道一个单通道掩码（或按情况 1 使用其引脚），然后 N−1 个 `AppendVector` | 各部分串联 |
| 4 | 基础有 1 个分量且每个通道都在范围内 | N−1 个 `AppendVector`（对于 1 字符 swizzle 则无） | 标量复制 N 次 |

恒等掩码 — 值的每个通道按顺序：`.x` 作用于 `float`、`.rgb` 作用于 `float3` — 在 `.dss` 中不是
节点。在 1.x 源中它保留为 `ComponentMask`，因为 1.x 无论值声明为哪种宽度
都会对连接进行掩码。

<a id="sequential-versus-non-sequential-masks"></a>

### 顺序与非顺序掩码

| Swizzle | 通道 | 情况 | 节点成本 |
| :-- | :-- | :-- | :-- |
| `.r` / `.x` | 0 | 2 | 1 个 `ComponentMask` |
| `.a` | 3 | 2 | 1 个 `ComponentMask` |
| `.rg` / `.xy` | 0,1 | 2 | 1 个 `ComponentMask` |
| `.rgb` / `.xyz` | 0,1,2 | 2 | 1 个 `ComponentMask` |
| `.ga` | 1,3 | 2 — 递增，允许有间隔 | 1 个 `ComponentMask` |
| `.rgba` 作用于 `float4` | 0,1,2,3 | 恒等 | 在 `.dss` 中无；在 1.x 源中为 1 个 `ComponentMask` |
| `.gr` | 1,0 | 3 | 2 个 `ComponentMask`, 1 个 `AppendVector` |
| `.bgr` | 2,1,0 | 3 *（自 1.3.3 起）* | 3 个 `ComponentMask`, 2 个 `AppendVector` |
| `.xxx` | 0,0,0 | 3 | 1 个 `ComponentMask`, 2 个 `AppendVector` |
| `.rrgg` | 0,0,1,1 | 3 | 2 个 `ComponentMask`, 3 个 `AppendVector` |
| `.rr` 作用于 `float` | 0,0 | 4 | 1 个 `AppendVector` |

凡源节点有引脚之处，情况 1 会将此表中的 `ComponentMask` 替换为引脚。无论 swizzle 被书写多少次，每个
节点都只计数一次：相等的节点会被合并（参见
[节点复用](node-reuse.md)）。

<a id="composed-swizzles"></a>

### 组合 swizzle

对已 swizzle 的值再次 swizzle 会通过现有掩码重新映射，因此通道编号始终
相对于被 swizzle 的值，而不是相对于原始节点。

| 书写 | 含义 |
| :-- | :-- |
| `v.rgb.b` | `v` 的通道 2 — `.rgb` 选择 `{0,1,2}`，然后 `.b` 选取该列表的条目 2 |
| `v.ga.r` | `v` 的通道 1 — `.ga` 选择 `{1,3}`，然后 `.r` 选取条目 0 |
| `v.ga.g` | `v` 的通道 3 |
| `v.bgr.r` | `v` 的通道 2 |

当结果保持升序时，掩码的掩码会折叠为一个掩码，因此 `v.rgb.rg` 在 `v` 上是一个
`ComponentMask`（或一个引脚）。选择外部列表中不存在的条目是针对 *已 swizzle* 宽度的
普通边界失败 — `v.ga.b` 是 `DSH4230`，因为 `.ga` 有 2
个分量，无论 `v` 的宽度如何。

<a id="swizzling-a-call-result"></a>

### 对调用结果进行 swizzle

`.` 是普通后缀运算符，因此任何调用结果都可以直接 swizzle — 无需临时
变量。

```c
float u  = UE.TexCoord().x;
vec3  c  = SampleTexture2D(Albedo, uv).rgb;
vec2  yx = vec4(1.0, 2.0, 3.0, 4.0).yx;
float m  = MyFunction(A, B).r;
```

Swizzle 应用于调用的值。对于 `Function`、`GraphFunction`、`ShaderFunction`
或 `VirtualFunction` 的调用，若成员名是函数某个输出的名称，则改选该输出；
当函数不返回任何内容时，**任意**成员也如此（旧规则 L3b）：在仅有 `out` 参数的函数上的 `.r`
会被读取为输出名称，且
[`DSH5280`](../diagnostics/DSH5xxx.md#dsh5280)，除非有个输出名为 `r`。参见
[调用](calls.md#output-selection)。对于具有多个输出的 `UE.*` 节点，成员名指定一个输出；
当输出是同一个值的通道视图时（例如纹理采样），允许使用 swizzle。

<a id="non-swizzlable-bases"></a>

## 不可 swizzle 的基础值

对非数值值的成员访问并不是 swizzle。

| 基础值 | 行为 |
| :-- | :-- |
| `MaterialAttributes` | 不是 swizzle。成员名会作为材质属性解析并读取。参见 [MaterialAttributes](material-attributes.md) |
| 纹理对象 | [`DSH4206`](../diagnostics/DSH4xxx.md#dsh4206)：纹理的成员是采样方法，必须被调用 |
| `Substrate` | [`DSH4207`](../diagnostics/DSH4xxx.md#dsh4207)：该值没有这样的成员 |

<a id="notes"></a>

## 说明

- 无论写多少次，一个 swizzle 只消耗一个节点：`Src.rgb` 使用十次就是一个
  `ComponentMask` —— 或者当 `Src` 具有 `RGB` 引脚时，不消耗节点。
- 在 1.x 源中的窄化会以相同的 swizzle 取前导通道（`r`、`rg`、`rgb`），因此
  隐式裁剪的开销与 swizzle 相同。参见 [转换](conversions.md#narrowing)。
- 常量的 swizzle 会被折叠：`vec4(1.0, 2.0, 3.0, 4.0).yx` 就是一个 `Constant2Vector(2, 1)`。
- 重复和重排序的 swizzle 会像任何其他表达式一样被合并，因此 `Src.bgr` 写两次
  只会产生一条 `AppendVector` 链。参见 [节点复用](node-reuse.md)。
- `.` 后跟任何非标识符的内容，在任何 swizzle 逻辑运行之前就是解析错误：
  [`DSH2161`](../diagnostics/DSH2xxx.md#dsh2161)。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH4230` | 出现了非通道字母的字符、超过四个字符、通道达到或超出基础值宽度，或两组通道混用。 |
| `DSH4206` | 纹理对象的成员。 |
| `DSH4207` | 没有成员的值的成员，例如 `Substrate` 值。 |
| [`DSH5201`](../diagnostics/DSH5xxx.md#dsh5201) | 多输出 `UE.*` 节点的成员未指定任何输出，或 swizzle 跨越了位于不同输出上的通道。 |
| `DSH5280` | 仅有 `out` 参数的函数调用的成员未指定任何输出。 |
| `DSH2161` | `.` 后未跟标识符。 |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214)、[`DSH8226`](../diagnostics/DSH8xxx.md#dsh8226) | 发射器无法创建 `ComponentMask` 或 `AppendVector` 节点，或收到的掩码不是一到四个升序通道（内部防护）。 |

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_Swizzle")
{
    Properties { vec4 Src = vec4(0.1, 0.2, 0.3, 0.4); }
    Settings   { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }

    Graph {
        vec3  Forward   = Src.rgb;   // leading channels -> the parameter's RGB pin
        vec3  Reordered = Src.bgr;   // reordered        -> a mask per channel, 2 AppendVector
        float One       = Src.a;     // ascending        -> 1 ComponentMask
        vec2  Repeated  = One.rr;    // scalar base      -> 1 AppendVector
        Color = Forward + Reordered * One + vec3(Repeated, 0.0);
    }
}
```

生成的节点：

```text
VectorParameter                       -> Src (property node)
(no node)                             -> Src.rgb        [the RGB pin of Src]
ComponentMask B, ComponentMask G,
  AppendVector, AppendVector          -> Src.bgr        [r is the R pin of Src]
ComponentMask A                       -> Src.a
AppendVector                          -> One.rr
Multiply, Add, Constant(0.0),
  AppendVector, Add                   -> Base.EmissiveColor
```

<a id="see-also"></a>

## 另请参见

- [表达式](expressions.md) —— 后缀优先级以及 `.` 的绑定位置
- [转换](conversions.md) —— 隐式窄化与拓宽
- [构造函数](constructors.md) —— 显式拓宽或重新打包的方式
- [MaterialAttributes](material-attributes.md) —— `.` 在属性值上的含义
- [调用](calls.md) —— 对函数或内置调用结果进行 swizzle
- [节点复用](node-reuse.md) —— 为什么写十次的 swizzle 是一个节点
- [不支持的构造](unsupported.md) —— 为什么 `v[0]` 会被拒绝
- [图表布局](../generation/graph-layout.md) —— 生成的节点放置在何处
