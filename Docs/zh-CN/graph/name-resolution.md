---
title: "名称解析"
description: "名称解析"
---

<a id="name-resolution"></a>

# 名称解析

> [DreamShader](../index.md) » [Graph](index.md) » **名称解析**

在 `Graph` 块内，裸标识符和调用目标是如何被查找的，按照编译器尝试它们的固定顺序，以及每个层面会遮蔽什么。

| | |
| :-- | :-- |
| 声明位置 | `.dsm`、`.dsf` —— 适用于 `Graph { … }` 体内的每个表达式、`Outputs` 绑定表达式以及 `Outputs` 声明默认值 |
| 类型 | 解析规则 |

名称的查找分为两个阶段 *(自 2.0.0 起)*。旧版前端在解析时会读取若干 1.x 写法——布尔字面量、类型名称、`N::F`、1.x 调用简写以及会变成节点的属性——而 2.0 绑定器会以与处理 `.dss` 相同的方式解析其余所有内容。绑定器以**区分大小写**的方式比较名称；当名称仅在大小写上不同但仍匹配时，1.x 源码仍会通过旧版规则 L19 构建成功，并会就此发出警告。

<a id="synopsis"></a>

## 概要

存在两个不同的阶梯。运行哪一个完全取决于名称后面是否跟着 `(`。

```c
<identifier>                 // bare-identifier ladder
<identifier> ( … )           // call-name ladder
<identifier> :: <identifier> ( … )   // call-name ladder, qualified callee
```

`Common::ApplyTint` 被读取为单个标识符 `Common_ApplyTint`，即命名空间函数声明时所用的名称。`UE.TexCoord` 和 `Substrate.Slab` 是节点调用。不属于以上任何一种的被调方——比如对括号表达式调用——是 [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208)。

<a id="bare-identifiers"></a>

## 裸标识符

| 顺序 | 阶段 | 层面 | 大小写 | 结果 |
| :-- | :-- | :-- | :-- | :-- |
| 1 | 解析 | 布尔字面量 `true` 和 `false` | 不敏感 | 具有一个分量的 bool 值 |
| 2 | 解析 | 类型名称——`float3`、`vec3`、`half`、`Texture2D`…… | 不敏感 | 一个类型，不是值：[`DSH4204`](../diagnostics/DSH4xxx.md#dsh4204) |
| 3 | 解析 | 类型为参数节点的属性（`TextureSampleParameter2D`、`ChannelMaskParameter`……），`StaticSwitchParameter` 除外 | 不敏感 | 该属性的节点，在此读取处写出 |
| 4 | 绑定 | Graph 变量——本体的声明、`Shader` 的 `Outputs` 声明，或本体读取的 `UE.*` 属性——最内层块优先 | 敏感 | 该变量 |
| 5 | 绑定 | 函数的参数：`ShaderFunction` 的输入或输出，或 `Base`，即 `Shader` 所构建的材质 | 敏感 | 该参数 |
| 6 | 绑定 | 文件作用域声明：标量、向量、纹理或 static-bool 属性 | 敏感 | 材质参数 |
| 7 | 绑定 | 4–6 中恰好一个在忽略大小写时匹配 | 不敏感 | 那一个，并带有警告 [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275)（旧版规则 L19） |
| 8 | — | *（无匹配）* | — | [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200)；单独 `UE` 或 `Substrate` 为 [`DSH4203`](../diagnostics/DSH4xxx.md#dsh4203) |

步骤 1–3 在任何变量已知之前运行，因此它们会胜过同名的变量。在步骤 6 读取的属性每次都是参数节点；在[去重过程](node-reuse.md)之后，相等的节点为一个节点。步骤 7 需要唯一匹配：当两个声明在忽略大小写时匹配，则二者都不会被选中，该名称为 `DSH4200`。

> [!NOTE]
> `StaticSwitchParameter` 属性故意**不会**作为裸标识符解析——它需要它的两个分支。按名称读取它会失败并给出 `DSH4200`。请使用调用形式 `Switch(True = …, False = …)`；参见[调用](calls.md#staticswitchparameter)。

<a id="call-names"></a>

## 调用名称

严格按照此顺序探测。第一个认领该名称的层面胜出；后续层面永远不会被查询。

| 顺序 | 阶段 | 层面 | 大小写 | 备注 |
| :-- | :-- | :-- | :-- | :-- |
| 1 | 解析 | `Path(…)` | 不敏感 | 资源引用，不是调用 |
| 2 | 解析 | 类型名称 | 不敏感 | 构造函数：[构造函数](constructors.md#constructor-names) |
| 3 | 解析 | `N::F` | — | 读取为标识符 `N_F`，然后从步骤 9 开始查找 |
| 4 | 解析 | 类型为参数节点的属性 | 不敏感 | `StaticSwitchParameter`：[静态开关调用](calls.md#staticswitchparameter)；其他：[引脚调用](calls.md#input-pin-wiring) |
| 5 | 解析 | `SampleTexture2D` | **敏感** | `UE.Expression(Class="TextureSample", …)`；除两个位置参数之外的任何内容都是 [`DSH5256`](../diagnostics/DSH5xxx.md#dsh5256) |
| 6 | 解析 | `BreakOutFloat2Components` … `BreakOutFloat4Components` 带输出选择器 | 不敏感 | 第一个参数的 swizzle；参见[调用](calls.md#breakoutfloatncomponents) |
| 7 | 解析 | 1.x `UE.*` 简写——`UE.TexCoord`、`UE.Time`、`UE.SceneTexture`、`UE.StaticSwitchParameter`…… | 不敏感 | 它们的参数按 1.x 方式读取，简写从未读取的参数会被丢弃并带有警告 [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254)；`UE.SceneTexture` 和 `UE.StaticSwitchParameter` 会变成 `UE.Expression` 调用。参见 [UE 内置函数](../builtins/ue.md) |
| 8 | 绑定 | `UE.<Node>` / `Substrate.<Node>` | 节点名称：精确，然后不敏感并给出 [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276) | [`UE.*` 目录](../builtins/ue.md)和 [`Substrate.*` 目录](../builtins/substrate.md)，UE 5.4+ |
| 9 | 绑定 | 数学内置函数 | 敏感 | [数学内置函数](../builtins/math.md)中的 HLSL 写法 |
| 10 | 绑定 | `mix`、`fract`、`mod`、`inversesqrt` | 敏感 | 内置函数的 GLSL 写法：该内置函数，并带有警告 [`DSH5277`](../diagnostics/DSH5xxx.md#dsh5277) |
| 11 | 绑定 | `Function`、`GraphFunction`、`ShaderFunction`、`VirtualFunction`——本文件或导入头文件的 | 敏感 | 见下文 |
| 12 | 绑定 | 恰好一个内置函数，否则恰好一个函数，在忽略大小写时匹配 | 不敏感 | 那一个，并带有 `DSH5275`（旧版规则 L19） |
| 13 | — | *（无匹配）* | — | `DSH4208` |

绑定器精确比较节点前的 `UE` *(自 2.0.0 起)*：`ue.TexCoord()` 会把 `ue` 读取为未声明名称 `DSH4200`。

内置函数直接认领其名称。格式错误的 `clamp(x)` 是该内置函数的参数数量错误 [`DSH4224`](../diagnostics/DSH4xxx.md#dsh4224)；不存在同名的用户函数可供回退，因为声明一个用户函数是 [`DSH6206`](../diagnostics/DSH6xxx.md#dsh6206)。

在步骤 11 没有需要解决的歧义：一个名称声明一个事物，而某个名称的第二次声明在声明处就是 [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210) *(自 2.0.0 起)*。

<a id="how-each-declaration-kind-is-matched"></a>

### 每种声明类型如何被匹配

| 类型 | 声明所用名称 |
| :-- | :-- |
| `Function`、`GraphFunction` | 其名称。在 `Namespace(Name="N")` 内，名称为 `N_F`，即写法 `N::F` 被读取为的名称。 |
| `ShaderFunction` | 其 `Name` 中最后一个以 `/` 分隔的段，其中标识符不能包含的每个字符都被替换为 `_`：声明为 `Name="Functions/F_Tint"` 的块可作为 `F_Tint` 调用。当在它之前声明的函数已具有该名称时，`<Name>_Asset`（信息 [`DSH5290`](../diagnostics/DSH5xxx.md#dsh5290)）。 |
| `ShaderLayer`、`ShaderLayerBlend` | 同 `ShaderFunction`，但调用为 [`DSH6208`](../diagnostics/DSH6xxx.md#dsh6208) *(自 2.0.0 起)*。 |
| `VirtualFunction` | 其 `Name`，必须是标识符（[`DSH6311`](../diagnostics/DSH6xxx.md#dsh6311)）。 |

生成的 HLSL 符号 `DreamShaderFn_<name>` 不再是调用可使用的名称 *(自 2.0.0 起)*。

这里是**没有重载解析**：名称不会按元数或参数类型划分作用域。导入的标头被声明进文件中，而不是作为文本粘贴进去，且它声明的名称不可以被再次声明：`DSH4210` 同时命名两个文件。此处名称同样区分大小写比较，因此 `Foo` 和 `foo` 是两个函数*（自 2.0.0 起；1.x 曾将它们报告为声明了两次）*，而 `N::F` 与一个名为 `N_F` 的函数是同一个名称，`DSH4210`。

带命名空间的函数只能通过 `N::F` 或其读取形式 `N_F` 访问。这里没有 `using` 风格的导入，也没有无限定的回退。

<a id="shadowing"></a>

## 遮蔽

因为层级是严格有序的，"遮蔽"总是单向进行：靠前的表层隐藏靠后的表层，绝不会反过来。

| 情况 | 结果 |
| :-- | :-- |
| 一个名为 `lerp`、`dot`、`pow`、`saturate`、`frac`、`fmod`、… 或 `mix`、`fract`、`mod` 的 `Function` | [`DSH6206`](../diagnostics/DSH6xxx.md#dsh6206)*（自 2.0.0 起）*。该函数未被声明，每次调用都是该内置函数。 |
| 一个名为 `float`、`float2`、`vec3`、`int4`、`bool2` 或任何其他类型名称的 `Function` | **永远不会被调用。** 在解析调用时该名称被读取为类型，因此每次调用都会构建一个构造函数。 |
| 一个名为 `SampleTexture2D` 的 `Function` | **永远不会被调用。** 每个 `SampleTexture2D(…)` 在解析时都会被改写。名为 `sampletexture2d` 的函数**会**被该写法调用：改写会进行精确比较。 |
| 一个名称以 `UE.` 或 `Substrate.` 开头的 `Function` | 不是名称：`UE.X(…)` 是一个节点调用。 |
| 与标量、向量、纹理或静态布尔属性同名的 Graph 变量 | 该变量从其声明处起生效，声明会以警告 [`DSH4251`](../diagnostics/DSH4xxx.md#dsh4251) 说明这一点。 |
| 与参数节点属性同名的 Graph 变量 | 属性生效，无论何种情况：上述第 3 步和第 4 步会在该行被解析时运行。 |
| 与 `Function` 同名的属性 | [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210)：属性是文件作用域声明，与函数共享同一个命名空间*（自 2.0.0 起）*。参数节点属性不是声明；它在其被使用处写出，因此以其名称进行的调用是该属性的引脚调用。 |
| 同一文件两个块中相同的属性名 | `DSH4210`：每个块的标量、向量、纹理和静态布尔属性都在文件作用域声明*（自 2.0.0 起）*。参数节点属性属于它们各自的块。 |
| 一个名为 `True` 或 `False`（任意大小写）的变量或属性 | 不可读：`true` 和 `false` 在任何名称之前被读取为字面量*（自 2.0.0 起；1.x 曾让该名称遮蔽字面量）*。 |
| 一个名为类型名称的变量，例如 `Half` 或 `Float3` | 不可读：该名称被读取为类型，`DSH4204`*（自 2.0.0 起）*。 |

> [!WARNING]
> 遮蔽构造函数是静默的。`Function vec3(in float x, out float3 r)` 会被解析、被声明，并且
> 在 `Graph` 块中根本不会被调用 —— 每次 `vec3(…)` 调用都会构建一个构造函数。请重命名该
> 函数。

<a id="identifier-case"></a>

## 标识符大小写

| 元素 | 大小写敏感性 |
| :-- | :-- |
| `if`、`else` | **敏感** |
| 顶层块关键字（`Shader`、`Function`、`GraphFunction`、`Namespace`、`VirtualFunction`、`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend`） | **敏感** |
| `SampleTexture2D` | **敏感** |
| 节点前的 `UE` | **敏感***（自 2.0.0 起）* |
| 区段名称（`Properties`、`Settings`、`Outputs`、`Graph`、`Layout`） | 不敏感 |
| 类型记号（`float3`、`vec3`、`MaterialAttributes`、`Texture2D`、`Substrate`） | 不敏感 |
| 构造函数名称 | 不敏感 |
| `true` / `false` | 不敏感 |
| `in` / `out` 参数限定符 | 不敏感 |
| 分量重组通道（`.RGB`、`.xyZ`） | 不敏感 |
| 输出选择器与静态开关分支名称（`Output`、`OutputName`、`OutputIndex`、`True`、`False`、`A`、`B`） | 不敏感 |
| 数学内置函数名称 | **敏感***（自 2.0.0 起）*；其他大小写以 `DSH5275` 解析 |
| Graph 变量、属性和参数名称 | **敏感***（自 2.0.0 起）*；当恰好有一个声明匹配时，其他大小写以 `DSH5275` 解析 |
| 参数节点属性名称 | 不敏感 —— 在该行被解析时进行匹配 |
| `Function` / `GraphFunction` / 材质函数名称 | **敏感***（自 2.0.0 起）*；其他大小写以 `DSH5275` 解析 |
| 命名调用参数名称 | 函数的参数：作为名称，`DSH5275`；节点的引脚和属性：`DSH5276`；内置函数的参数（`A`、`B`、`Alpha`）：**敏感**，[`DSH4216`](../diagnostics/DSH4xxx.md#dsh4216) |
| `UE.*` / `Substrate.*` 节点、引脚、属性和输出名称 | 引擎的拼写；其他大小写以 `DSH5276` 解析 |
| `MaterialAttributes` 成员名称 | 引擎的拼写；其他大小写以 `DSH5276` 解析 |
| `default` 调用参数 | **敏感***（自 2.0.0 起）* |

> [!NOTE]
> 以与声明不同的大小写写出的名称**就是**该声明，并带有 `DSH5275`。
> `float3 Color = …;` 后跟 `color = …;` 写出 `Color`*（自 2.0.0 起；1.x 曾创建名为
> `color` 的第二个条目）*。只有在任何大小写下都不匹配任何内容的名称才会由赋值声明（旧
> 规则 L26，信息 [`DSH5292`](../diagnostics/DSH5xxx.md#dsh5292)）。`dsc migrate` 会按照每个此类
> 名称被声明的方式重新拼写它。

<a id="named-argument-matching"></a>

## 命名参数匹配

参数名称会精确比较。函数的参数名称会以 `DSH5275` 回退到唯一的不区分大小写匹配，节点的引脚和
属性名称会以 `DSH5276` 回退；内置函数的参数名称没有回退。上述 1.x 伪参数在匹配时忽略大小写。
位置参数在前面并按顺序填充参数；位于命名参数之后的位置参数是 [`DSH2158`](../diagnostics/DSH2xxx.md#dsh2158)*（自 2.0.0 起）*。

哪些被调用者接受命名参数已记录于 [调用](calls.md#positional-and-named)。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH4200` | 一个裸名称，既不是 Graph 变量、参数或文件作用域声明，或者忽略大小写时匹配到多个此类名称。此外，一个 `StaticSwitchParameter` 属性在未调用时被读取。 |
| `DSH4203` | `UE` 或 `Substrate` 被作为值读取。 |
| `DSH4204` | 一个类型名称被作为值读取。 |
| `DSH4208` | 一个调用名称到达了阶梯的末端，或者被调用者不是名称。 |
| `DSH5275` | *（警告）* 一个名称仅在忽略大小写时才与其声明匹配。 |
| `DSH5276` | *（警告）* 一个引擎名称——节点、引脚、属性、输出、材质特性——仅在忽略大小写时才匹配。 |
| `DSH5277` | *（警告）* 内置项的 GLSL 拼写。 |
| `DSH4210` | 一个名称在文件中或被导入的头文件中被声明了两次——包括 `N::F` 与函数 `N_F` 的重名。 |
| `DSH6206` | 一个与内置项同名的函数。 |
| `DSH6208` | 对 `Shader`、`ShaderLayer` 或 `ShaderLayerBlend` 的调用。 |
| `DSH4251` | *（警告）* 一个 Graph 变量隐藏了同名的属性。 |
| [`DSH5260`](../diagnostics/DSH5xxx.md#dsh5260) | `::` 后面没有跟名称。 |
| [`DSH2161`](../diagnostics/DSH2xxx.md#dsh2161) | `.` 后面没有跟名称。 |

<a id="example"></a>

## 示例

```c
import "Helpers.dsh";

Shader(Name="Docs/M_Resolution")
{
    Properties {
        vec3                  Tint    = vec3(1.0, 0.5, 0.2);
        StaticSwitchParameter UseTint = true;
    }

    Settings { Domain = "UI"; ShadingModel = "Unlit"; }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        // 'Tint' is the file-scope property: a VectorParameter node.
        vec3 Base1 = Tint;

        // 'TINT' matches 'Tint' only ignoring case: the same parameter, warning DSH5275.
        vec3 Base2 = TINT;

        // 'lerp' is the builtin; a function of that name in Helpers.dsh would be DSH6206.
        vec3 Mixed = lerp(Base1, Base2, 0.5);

        // A namespaced Function needs its full name.
        vec3 Lit = Common::ApplyTint(Mixed, Tint);

        // 'UseTint' is a StaticSwitchParameter: callable, not readable.
        Color = UseTint(True = Lit, False = Mixed);
    }
}
```

解析轨迹：

```text
Tint                  -> bare 6   file-scope property               -> VectorParameter node
TINT                  -> bare 7   'Tint', ignoring case (DSH5275)   -> the same node
lerp(...)             -> call 9   math builtin                      -> LinearInterpolate node
Common::ApplyTint(..) -> call 3   read as Common_ApplyTint
                         call 11  Function (namespaced)             -> Custom node
UseTint(True=,False=) -> call 4   StaticSwitchParameter             -> StaticSwitchParameter node
UseTint               -> (bare read) ERROR: DSH4200
```

<a id="see-also"></a>

## 另请参阅

- [调用](calls.md) — 每个表面接受的调用形式，以及所有调用诊断
- [表达式](expressions.md) — 标识符和被调用者在语法中出现的位置
- [声明](declarations.md) — Graph 变量如何声明
- [构造函数](constructors.md) — 保留的构造函数拼写
- [数学内置项](../builtins/math.md) — 保留的内置项拼写
- [UE 内置项](../builtins/ue.md) — 按前缀声明的 `UE.*` 表面
- [Substrate 内置项](../builtins/substrate.md) — `Substrate.*` 表面
- [Graph 中的参数](../../parameters/graph-usage.md) — 读取参数和引脚调用形式
- [`Namespace`](../language/namespace.md) — 声明 `Namespace::Name` 以及禁止嵌套规则
- [词法元素](../language/lexical.md) — 声明语法的完整大小写敏感性矩阵
- [节点复用](node-reuse.md) — 相等节点如何合并
- [`dsc migrate`](../tools/migrate.md) — 本页每条旧规则的 `.dss` 拼写
