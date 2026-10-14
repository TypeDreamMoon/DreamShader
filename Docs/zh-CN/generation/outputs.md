---
title: 输出绑定
description: Outputs section —— 声明输出变量、Base 目标清单，以及 Expression(...).Pin[i] 与 Expression(...) { … } 两种写法。
---

`Shader` 的 `Outputs` section 同时干两件事：

1. **声明** `Graph` 将要写入的变量；
2. 把值 **绑定** 到 Unreal 材质属性输入，或绑定到显式创建的 `UMaterialExpression` 节点的 pin 上。

真正把值送进材质的是绑定，不是声明。有 `Graph`、没有 `Outputs` section 的 `Shader` 会带着警告 `DSH2256` 照样构建，
只是什么都没接到材质上；既没有 `Graph`、`Outputs` 里也什么都没算的 `Shader` 是 `DSH2255` （自 2.0.0 起）。

绑定在 2.0 里写成对 material 参数的赋值，`m.EmissiveColor = Color;` —— 见 [DreamShaderLang 2.0](../language-v2/index.md)；
[`dsc migrate`](../tools/migrate.md) 会把 `.dsm` 改写成那样。

<a id="语法概览"></a>

## 语法概览

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

```c
Outputs [=]
{
    <output-declaration>…
    <output-binding>…
}

output-declaration := <type> <name> [ = <expression> ] ;

output-binding     := Base. <target> = <source> ;
                    | Expression( <key> = <value> [, <key> = <value> ]… ) . Pin[ <index> ] = <source> ;
                    | Expression( <key> = <value> [, <key> = <value> ]… ) { <pin-binding>… } [;]

pin-binding        := Pin[ <index> ] = <source> ;
```

`<source>` 是一个**表达式** （自 2.0.0 起）：可以是 1.x 写法里的输出变量名，也可以是 `Graph` 表达式能写的任何东西 ——
`Base.FrontMaterial = Substrate.Layer(Coat, Body, 0.01);`。它在 `Graph` 跑完之后求值，受 1.x `Graph` 表达式规则约束
（`DSH2200`–`DSH2222`）。`<index>` 两侧的 `[` 和 `]` 是 **字面量**。

**提示：**

1.x 里绑定右侧只能是一个**名字**，`Base.EmissiveColor = Tint * Strength;` 不合法。2.0 起它就是一个表达式；
不过把值先在 `Graph` 里算好、再绑定变量，仍然是最好读的写法。

section 一条语句一条语句地读，按语句开头的 token 归类：

| 语句开头 | 读作 |
| :-- | :-- |
| 两个词 `<type> <name>` | 输出声明，带或不带初始化 （自 1.3.4 起） |
| `Base` `.` `<name>` `=` | 材质属性绑定 |
| `Expression` `(` … `)` `.` | 表达式绑定的语句形式 |
| `Expression` `(` … `)` `{` | [块形式](#块形式) （自 1.9.0 起） |
| 其他，包括跟在 `Expression( … )` 之外任何东西后面的 `{` | `DSH3266`；以 `Expression( … )` 目标开头的是 `DSH3267` |

`Expression( … ) { … }` 是 `Outputs` 里**唯一**能开花括号的构造，正如 `Group("Name") { … }` 是 `Properties` 里唯一的一个。

声明变成材质入口里、`Graph` 之前的一个局部变量；没有初始化的从零开始，和 1.x 一样（纹理或 `Substrate` 输出没有零值，
必须由 `Graph` 赋值）。绑定变成 `Graph` 之后对材质的一次赋值。

<a id="绑定目标种类"></a>

## 绑定目标种类

| 目标文本开头 | 种类 | 含义 |
| :-- | :-- | :-- |
| `Base.`（不区分大小写） | 材质属性 | 连到 `UMaterial` 的某个属性输入 |
| `Expression(`（不区分大小写） | 表达式输入 | 创建一个 `UMaterialExpression` 并连到它的某个输入 pin |
| 其它 | — | `DSH3266` |

<a id="basetarget-清单"></a>

## `Base.<target>` 清单

（自 2.0.0 起） 这些名字就是引擎自己的：引擎材质属性表里每一个有 `EMaterialProperty` 的属性，用引擎给它的名字，
再加上下面的 1.x 别名。名字**精确**匹配；在 1.x 文件里只有忽略大小写才对得上的名字仍然能解析，但给出警告 `DSH5276`
（legacy 规则 L19）。什么都对不上的名字是 `DSH5200`。

下表是 1.x 文档列过的写法，以及各自对应的属性和值类型；引擎的表里可能还有更多名字，它们同样能解析。

| `Base.` 写法 | `EMaterialProperty` | 值类型 |
| :-- | :-- | :-- |
| `BaseColor` | `MP_BaseColor` | Float3 |
| `MaterialAttributes`、`Attributes` | `MP_MaterialAttributes` | MaterialAttributes |
| `FrontMaterial` （自 UE 5.4 起） | `MP_FrontMaterial` | Substrate |
| `EmissiveColor`、`Emissive` | `MP_EmissiveColor` | Float3 |
| `Opacity` | `MP_Opacity` | Float1 |
| `OpacityMask` | `MP_OpacityMask` | Float1 |
| `Metallic` | `MP_Metallic` | Float1 |
| `Specular` | `MP_Specular` | Float1 |
| `Roughness` | `MP_Roughness` | Float1 |
| `Normal` | `MP_Normal` | Float3 |
| `AmbientOcclusion`、`AO` | `MP_AmbientOcclusion` | Float1 |
| `Refraction` | `MP_Refraction` | Float3 |
| `WorldPositionOffset`、`WPO` | `MP_WorldPositionOffset` | Float3 |
| `PixelDepthOffset`、`PDO` | `MP_PixelDepthOffset` | Float1 |
| `SubsurfaceColor` | `MP_SubsurfaceColor` | Float3 |
| `ClearCoat` | `MP_CustomData0` | Float1 |
| `ClearCoatRoughness` | `MP_CustomData1` | Float1 |
| `CustomData0` | `MP_CustomData0` | Float1 |
| `CustomData1` | `MP_CustomData1` | Float1 |
| `DiffuseColor` | `MP_DiffuseColor` | Float3 |
| `SpecularColor` | `MP_SpecularColor` | Float3 |
| `SurfaceThickness` | `MP_SurfaceThickness` | Float1 |
| `Displacement` | `MP_Displacement` | Float1 |
| `CustomizedUV0` … `CustomizedUV7` | `MP_CustomizedUVs0` … `MP_CustomizedUVs7` | Float2 |
| `Anisotropy` | `MP_Anisotropy` | Float1 |
| `Tangent` | `MP_Tangent` | Float3 |

**警告：**

1.x 还接受 `CustomizedUVs0` … `CustomizedUVs7`。引擎把这些属性叫 `CustomizedUV0` … `CustomizedUV7`，所以带 `s` 的写法
（自 2.0.0 起） 是 `DSH5200`；请写 `CustomizedUV<n>`。

**提示：**

`ClearCoat` 和 `CustomData0` 是 **同一个** 材质属性，`ClearCoatRoughness` 和 `CustomData1` 也是。在一个 `Shader` 里
同时绑定一对里的两种写法，等于往同一个输入写了两次；后面那条语句赢，且没有任何诊断。

<a id="仅-fork-可用的目标"></a>

### 仅 fork 可用的目标

这些属性只存在于 Moon 引擎分支；在原版 Unreal 上它们都是 `DSH5200`。`Mooa…` 是分支改名前的旧写法，仍作为别名接受。

| `Base.` 写法 | `EMaterialProperty` | 值类型 |
| :-- | :-- | :-- |
| `MoonEncodedAttribute0`、`MooaEncodedAttribute0` | `MP_MoonEncodedAttribute0` | Float4 |
| `MoonEncodedAttribute1`、`MooaEncodedAttribute1` | `MP_MoonEncodedAttribute1` | Float4 |
| `MoonEncodedAttribute2`、`MooaEncodedAttribute2` | `MP_MoonEncodedAttribute2` | Float4 |
| `MoonEncodedAttribute3`、`MooaEncodedAttribute3` | `MP_MoonEncodedAttribute3` | Float4 |
| `MoonEncodedAttribute4`、`MooaEncodedAttribute4` | `MP_MoonEncodedAttribute4` | Float4 |

<a id="带副作用的目标"></a>

## 带副作用的目标

<a id="basematerialattributes"></a>

### `Base.MaterialAttributes`

（自 1.2.5 起）

绑定 `Base.MaterialAttributes` 会把 `UMaterial::bUseMaterialAttributes` 设为 `true` —— 也就是
**Use Material Attributes** 那个勾选框 —— 于是材质暴露的是单一的 attributes 输入，而不是各个独立属性输入。这个勾不需要你自己打，
也无法在材质编辑器里取消：每次重新生成都会重置并重新应用它。被绑定的值必须是 `MaterialAttributes`，否则是 `DSH4228`。

<a id="basefrontmaterial"></a>

### `Base.FrontMaterial`

（自 UE 5.4 起）

被绑定的值必须是 Substrate 值，由 [`Substrate.*` 内置节点](../builtins/substrate.md)或
[Substrate 语法糖](../language-v2/substrate.md)产生；否则是 `DSH4228`。

（自 2.0.0 起） `FrontMaterial` 是一个**普通属性**：1.x 生成器给它加的额外规则 —— 强制把着色模型设成 Substrate、
拒绝 `"Substrate"` 之外的显式 `ShadingModel`、与 `Base.MaterialAttributes` 互斥、整表面 `Custom` 节点路径 —— 都不再适用。
需要的话请在 `Settings` 里写 `ShadingModel = "Substrate";`（在 UE 5.3 上这个设置是 `DSH7128`）。在开了 Substrate 的工程里，
按旧属性写的材质怎么读，由 `Substrate =` 设置决定 —— 见 [Substrate 语法糖](../language-v2/substrate.md)。

```c
Outputs = {
    Substrate Surface;
    Base.FrontMaterial = Surface;
}
Graph = {
    Surface = Substrate.Unlit(EmissiveColor = Color);
}
```

<a id="expression--pini-目标"></a>

## `Expression( … ).Pin[i]` 目标

有些 Unreal 输出根本不是材质属性输入，而是散落在图里的 *custom output* 节点。非 `Base` 的绑定形式就是用来创建这类节点、
并把源值连到它的某个输入 pin 上的。

```c
Expression( Class = "<ExpressionClass>" [, <key> = <value> ]… ) . Pin[ <index> ] = <source> ;
```

（自 2.0.0 起） 两种写法的目标都变成材质入口里的一次 `UE.Expression(Class = "…", …)` 节点调用，所以绑定规则就是
`Graph` 里 [`UE.Expression`](../builtins/ue-expression.md) 调用的规则。

<a id="检查顺序与要求"></a>

### 检查顺序与要求

| 要求 | 违反时 |
| :-- | :-- |
| 头部是 `Expression(` `<key> = <value>` [`,` …] `)`，每个值非空 | `DSH3267` |
| 没有重复的键（忽略大小写比较） | `DSH3267` |
| 存在 `Class` 参数 | `DSH3267` |
| `)` 之后是 `.Pin[<index>] = <source>` 或一个 `{ … }` 块，索引是整数字面量 | `DSH3267` |
| `=` 后面有东西 | `DSH3270` |
| 引擎里有这个类 | `DSH5212` |
| 索引指向该类的一个输入 pin | `DSH5285` |
| 其他每个参数都对应该类的一个 pin 或属性 | `DSH5213` |

`Class` 忽略大小写匹配。下面的复用键会把其余每个键转小写、给每个值去引号；节点调用按原样携带每个参数。

<a id="类解析"></a>

### 类解析

（自 2.0.0 起） 类在引擎的表达式目录里按精确名字查找，依次尝试：短名（`ThinTranslucentMaterialOutput`）、类名
（`MaterialExpressionThinTranslucentMaterialOutput`）、类路径（`/Script/Engine.MaterialExpressionThinTranslucentMaterialOutput`）、
前面补上 `MaterialExpression` 的短名，以及带 `U` 的 C++ 名（`UMaterialExpressionThinTranslucentMaterialOutput`，1.x 不认）。
在 1.x 文件里只有忽略大小写才对得上某个类的名字也能解析，并给出警告 `DSH5276`。其余都是 `DSH5212`。同样的规则见
[通用 UE.Expression](../builtins/ue-expression.md)。

<a id="其它参数"></a>

### 其它参数

除 `Class` 之外的每个参数都按类去匹配：先找输入 pin，再找反射属性。值按数字、`true` / `false`、带引号的字符串或裸词读取；
裸词指图里的某个值，或者对枚举属性来说是某个枚举项。

（自 2.0.0 起） 指向**输入** pin 的参数会连接那个 pin，效果和 `Pin[<index>]` 一样；1.x 会拒绝它。用两种方式连同一个 pin 是
`DSH4215`。既不是 pin 也不是属性的名字是 `DSH5213` —— 例外是 1.x 文件里的图值，节点建好之后再按名字连接（`DSH5291`，info；
建好的节点没有这个 pin 时是 `DSH8212`）。属性接受不了的值是 `DSH8213`。

<a id="pin-索引与节点复用"></a>

### Pin 索引与节点复用

- `Pin[<index>]` 是节点输入列表的零基索引，顺序按引擎声明的顺序。
  `Expression(Class="ThinTranslucentMaterialOutput").Pin[0]` 是 `TransmittanceColor`，`.Pin[1]` 是 `SurfaceCoverage`。
  索引越界是 `DSH5285`。
- 一个节点的每个 pin 只能绑定 **一次**，语句形式和[块形式](#块形式)合起来算：`DSH3268`。
- 节点在同一个 `Outputs` section 内按 **类加上排序后的参数列表** 去重。两条 `Expression( … )` 规格完全相同的绑定共用一个节点、
  绑不同的 pin；任何一个参数不同都会创建第二个节点，同样的头写在第二个 `Outputs` section 里也会。
- 源必须是图里的值：没声明过的名字是 `DSH4200`，pin 接受不了的值是 `DSH5214`。必需的 pin 没连上是警告 `DSH5279`。
- 节点摆在哪里由图布局决定。

这种写法最常用的几个 custom output 的 pin 顺序：

| `Class` | `Pin[i]`，按顺序 |
| :-- | :-- |
| `ThinTranslucentMaterialOutput` | `0` TransmittanceColor、`1` SurfaceCoverage |
| `ClearCoatNormalCustomOutput` | `0` Input |
| `RuntimeVirtualTextureOutput` | `0` BaseColor、`1` Specular、`2` Roughness、`3` Normal、`4` WorldHeight、`5` Opacity、`6` Mask、`7` Displacement、`8` Mask4 |

其他任何 `UMaterialExpressionCustomOutput` 子类都一样；pin 顺序就是引擎声明输入的顺序，也就是材质编辑器里节点上从上到下的顺序。

```c
Outputs = {
    float3 Transmittance;
    float  Coverage;
    float3 CoatNormal;

    // Both statements reuse one ThinTranslucentMaterialOutput node.
    Expression(Class="ThinTranslucentMaterialOutput").Pin[0] = Transmittance;
    Expression(Class="ThinTranslucentMaterialOutput").Pin[1] = Coverage;

    // A different class -> a second node.
    Expression(Class="ClearCoatNormalCustomOutput").Pin[0] = CoatNormal;
}
```

<a id="块形式"></a>

## 块形式

（自 1.9.0 起）

一个 pin 很多的终端节点，用语句形式要为每个 pin 把整个 `Expression( … )` 规格重写一遍；而规格**就是**去重键，所以
**漏写一个参数就会悄无声息地把节点劈成两个**。块形式只写一次规格：

```c
Expression( <key> = <value> [, <key> = <value> ]… )
{
    Pin[ <index> ] = <source> ;
    …
}
```

```c
Outputs = {
    float _PhaseG_Out;
    float _PhaseG2_Out;
    float _PhaseBlend_Out;

    Expression(Class="VolumetricAdvancedMaterialOutput",
        PerSamplePhaseEvaluation="false",
        MultiScatteringApproximationOctaveCount=0,
        bGroundContribution="false")
    {
        Pin[0] = _PhaseG_Out;
        Pin[1] = _PhaseG2_Out;
        Pin[2] = _PhaseBlend_Out;
    }
}
```

块里的每个 pin 共享头部的类和参数列表 —— 正好就是复用键 —— 所以一个有 *N* 个 pin 的块**永远**只产出一个节点。
除此之外两种形式在下游完全一样：同样的类解析、同样的参数检查、同样的 pin 索引检查、同样的诊断。

| 规则 | 细节 |
| :-- | :-- |
| 什么能开花括号 | 只有 `Expression( … )`。变量或 `Base.` 目标后面的 `{` 是 `DSH3266`；`Expression( … ).Pin[i]` 后面的是 `DSH3267` |
| 头部形状 | `Expression(` … `)`，右括号后面什么都没有 —— pin 选择写在块内每条语句上 |
| 块内语句 | 只能是 `Pin[ <index> ] = <source> ;` —— 否则 `DSH3267` |
| 注释和空行 | 块内任何位置都可以 |
| 空块 | `DSH3269` —— 一个 pin 都不绑的块什么都不会建，所以被拒绝而不是被忽略 |
| 没闭合的块 | 解析一直跑到文件末尾：`DSH2150` |
| 结尾的 `;` | 右花括号后可选，和 `Group("Name") { … }` 一样 |
| 重复 | 同一个 `Outputs` section 里，同一个节点的块可以写不止一次，也可以和它的语句形式绑定随意混用 |

类和参数列表一致的块与散写语句指向**同一个**节点，所以它们填的是它的不同 pin：

```c
Outputs = {
    float3 Transmittance;
    float  Coverage;

    Expression(Class="ThinTranslucentMaterialOutput")
    {
        Pin[0] = Transmittance;
    }

    // Same specification -> same node -> this fills its other pin.
    Expression(Class="ThinTranslucentMaterialOutput").Pin[1] = Coverage;
}
```

**提示：**

参数列表哪怕只差一个参数的两个块，就是两个不同的节点，各有各的 `Pin[0]`。这不算重复绑定，也不会被诊断 ——
正是同一条规则让语句形式的重复写法变得危险。1.x 解析器为这种写法用过的 `DSH3133`–`DSH3137` （自 2.0.0 起）
不再报出，换成了上表的 `DSH3266`–`DSH3269` 和 `DSH2150`。

<a id="保留名-return"></a>

## 保留名 `return`

（自 2.0.0 起） `return` 在每个文件里都是关键字。它不能作输出声明的名字（`DSH3266`），也不能作绑定源：
`Base.EmissiveColor = return;` 是 `DSH2151`。1.x 为整表面 `Custom` 节点路径保留这个名字，而编译器已经没有那条路径了。

<a id="校验规则"></a>

## 校验规则

| 情况 | 码 |
| :-- | :-- |
| 声明的类型不是类型 | `DSH4201` |
| 同一个名字在 `Outputs` 里声明两次 | `DSH4220` |
| 源名字什么都没声明 | `DSH4200` |
| 材质没有的 `Base.` 名字 | `DSH5200` |
| 材质 pin 接受不了的值；更宽的向量不会被收窄进材质 pin，1.x 文件里也不会 | `DSH4228` |
| `Expression( … )` pin 接受不了的值 | `DSH5214` |

`Graph` 里对一个 `Outputs` 声明过的名字的声明，读作对那个输出的赋值 —— 1.x 就是这么处理的。

<a id="说明"></a>

## 说明

- 绑定和声明可以自由穿插；绑定可以引用同一 section 中稍后才声明的变量。
- 重复出现的 `Outputs` section 会分别追加到两份列表。
- `Shader` 的任何 `Outputs` 语句都不接受 `[ … ]` 元数据块。
- 每条绑定都会经过一对生成的 `NamedReroute`，按写下的 `Base.` 名字命名为 `DS_<pin>_<n>`，好让大图里的材质根节点保持可读。
- 绑定到当前着色模型或混合模式用不到的属性 **不会** 被诊断出来 —— Unreal 只是不去读那个输入。请看材质编辑器里变灰的输入。
  材质根本没有对应输入的属性是 `DSH8217`。

<a id="常见错误"></a>

## 常见错误

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH5200: A material has no '{0}' pin; did you mean '{1}'? Attribute names are case-sensitive.` | Base. 后面的名字不是任何材质属性 —— 包括 1.x 的 CustomizedUVs&lt;n&gt; 写法。 | 照上面的清单改拼写；CustomizedUVs&lt;n&gt; 改成 CustomizedUV&lt;n&gt;。 |
| `DSH5276: '{0}' matches the engine name '{1}' only in case; 1.x matched engine names ignoring case, so this is '{1}', and a '.dss' needs the exact spelling.` | Base. 名字、类或参数只有忽略大小写才对得上（警告）。 | 照引擎的大小写改。 |
| `DSH3266: Expected ';' after the output declaration '{0}', found {1}.` | section 没用 { 打开、声明后面没有 ;，或者一行既不是声明也不是绑定（包括把 return 当声明名）。 |  |
| `DSH3267: Expected 'Pin[<index>] = <source>' for an Expression(...) output target, found {0}.` | Expression(...) 目标写错：头部、pin 选择、缺 Class、重复的键，或块内不是 Pin\[i\] = &lt;source&gt;; 的语句。 |  |
| `DSH3268: Expected each pin of Expression(Class = "{0}") to be bound once, found Pin[{1}] bound again.` | 同一个节点的同一个 pin 绑了两次（语句形式和块形式合起来算）。 |  |
| `DSH3269: Expected at least one 'Pin[<index>] = <source>;' in the Expression(...) block, found none.` | 块形式里一个 pin 都没绑。 |  |
| `DSH3270: Expected a source after 'Pin[{0}] =', found none.` | = 后面什么都没有。 |  |
| `DSH2151: Expected an expression, found {0}.` | 源不是表达式 —— 比如 Base.EmissiveColor = return;。 |  |
| `DSH4228: {0} expects {1}, and this is {2}.` | 材质 pin 接受不了这个值：把非 attributes 值绑到 Base.MaterialAttributes、非 Substrate 值绑到 Base.FrontMaterial，或者宽度不对。 |  [详解](../builtins/substrate.md) |
| `DSH5212: '{0}' is not a material expression class this engine has.` | Class= 指向的类不存在。 |  [详解](../builtins/ue-expression.md) |
| `DSH5285: '{0}.{1}' has {2} input pin(s), counted from 0, and this argument connects pin {3}.` | pin 索引超出了节点输入列表。 |  |
| `DSH5213: '{0}.{1}' has no pin or property called '{2}'.` | 参数既不是该类的 pin，也不是它的属性。 |  |
| `DSH5214: {0} expects {1}, and this is {2}.` | Expression(...) 的 pin 接受不了这个值。 |  |
| `DSH5279: '{0}.{1}' leaves its required '{2}' pin unconnected, which 1.x allowed and the engine reports when the material compiles.` | 节点的必需 pin 没连（警告）。 |  |
| `DSH7128: ShadingModel="Substrate" requires Unreal Engine 5.4 or newer.` | 在 UE 5.3 上写了 ShadingModel = Substrate。 |  |
| `DSH8217: This material has no input for the attribute '{0}'; check the material domain and shading model the file asks for.` | 属性存在，但这个材质（按它的 domain、混合模式和着色模型）没有对应的输入。 |  |
| `DSH2256: The Shader '{0}' has no Outputs section, so nothing its Graph computes reaches the material.` | 有 Graph 没有 Outputs（警告）。材质照样构建，什么都没接上。 | 至少加一条 Base.&lt;Attribute&gt; = &lt;variable&gt;; 绑定。 |

跨阶段的完整列表在[错误速查](../diagnostics/index.md)。

<a id="完整示例"></a>

## 完整示例

```c
Shader(Name="DreamShaderTests/Corpus/M_Outputs")
{
    Properties = {
        vec3  Tint = vec3(0.4, 0.8, 1.0);
        float A    = 0.75;
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph = {
        Color = Tint;
        Alpha = A;
    }
}
```

生成的连接：

```text
UMaterial /Game/DreamShaderTests/Corpus/M_Outputs
  MP_EmissiveColor  <- NamedReroute DS_EmissiveColor_<n>  <- VectorParameter "Tint"  RGB output
  MP_Opacity        <- NamedReroute DS_Opacity_<n>        <- ScalarParameter "A"     R output
  bUseMaterialAttributes = false
```

<a id="继续阅读"></a>

## 继续阅读

- [生成流程](index.md)
  - [Substrate 节点](../builtins/substrate.md)
  - [通用 UE.Expression](../builtins/ue-expression.md)
  - [材质 Settings](../settings/material.md)
  - [Section](../language/sections.md)
