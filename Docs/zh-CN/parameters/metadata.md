---
title: 元数据与分组
description: 结尾的 [ … ] 块 —— 可识别的键与别名、Slider(min,max)、SamplerType、Group 作用域与 SortPriority 计数器，以及反射属性透传。
---

声明后面可以跟一个 `[ … ]` 块：一串 `Key = Value` 条目，用来设置生成节点的组织字段；对于语言不认识的键，则写入该
节点类的**反射 UPROPERTY**。

最后这一点才是这个块真正好用的地方：你生成的那个节点上引擎暴露的属性，大多都能从源码里设置。

<a id="写一个块"></a>

## 写一个块

```c
<declaration> [ <entry> ; <entry> ; … ] ;
<declaration> [ <entry> , <entry> , … ] ;

<entry> := <key> = <value>
         | Slider( <min> , <max> )
```

上面的每个 `[`、`]`、`;`、`,`、`=` 和 `( )` 都是**字面标点**。同一个块里 `;` 和 `,` 可以混用，`]` 之前多写一个分隔符
也会被接受。`Slider(…)` 是唯一不带 `=` 的条目形式。

```c
ScalarParameter Roughness = 0.5 [Group="Surface"; Slider(0, 1); Desc="Micro-surface roughness"];
```

| 规则 | 行为 |
| :-- | :-- |
| 块写在名字和默认值之后、`;` 之前 | 深度 0 处的 `[` 结束默认值；`( … )` 内和字符串字面量内的方括号属于默认值 |
| 只有一个 `[ … ]` 块的语句 | `DSH3250` |
| 条目用 `;` **或** `,` 分隔 | 同一个块内混用两者是允许的 |
| `]` 之前多余的 `;` 或 `,` | 接受 |
| 键比较时忽略大小写 | `[ Group = "X" ]`、`[group="X"]`、`[GROUP="X"]` 是同一个条目 |
| 值去引号 | 带引号的值去掉引号并处理转义；不带引号的值就是写下的文本 |
| 键不是名字、键后面没有 `=`、`=` 后面没有值、缺少 `]` | `DSH3255` |
| 重复键（忽略大小写） | `DSH3256` |

分号分隔形式自 （自 1.2.4 起） 起可用，元数据块本身自 （自 1.2.3 起） 起可用。

一个键做什么，取决于 property 的形式 （自 2.0.0 起）：在**声明**上（所有紧凑 token、`ScalarParameter`、
`StaticBoolParameter`、`VectorParameter`、`TextureObjectParameter`），条目变成 `uniform` 的 2.0 `///` 指令；在**于
使用处展开**的参数节点上（`StaticSwitchParameter`、两个 mask、纹理采样 token），条目变成反射调用的参数。见
[Properties 类型](../language/properties.md#参数节点-token)。

<a id="可识别的键"></a>

## 可识别的键

| 键 | 别名 | 值 | 效果 |
| :-- | :-- | :-- | :-- |
| `Group` | `Category` | 字符串 | 节点的参数分组（`Group` UPROPERTY，`FName`） |
| `Description` | `Desc`、`Tooltip` | 字符串 | 写入节点的 **`Desc`** UPROPERTY |
| `SortPriority` | `Sort` | 整数 | 节点的 `SortPriority`；非整数报 `DSH3258` |
| `ParameterName` | —— | 字符串 | 覆盖材质参数名；不写时使用声明的标识符 |
| `SamplerType` | —— | 枚举字面量 | 纹理节点的采样类型 —— 见 [SamplerType](#samplertype) |
| `Slider(min, max)` | —— | 两个数字，无 `=` | `SliderMin` 和 `SliderMax` |
| *其他任意键* | —— | 见下 | 写入生成节点类上同名的反射 UPROPERTY |

所有键比较都不区分大小写。（自 2.0.0 起） 可识别的键只做自己的事；1.x 里每个条目还会另外推给反射属性写入器。

无人设置时 `SortPriority` 为 **32**，与 Unreal 自身的节点默认值一致；不在任何 `Group(…)` 作用域里的声明会把这个 32 写出来。

<a id="slidermin-max"></a>

### `Slider(min, max)`

（自 1.5.0 起）

```c
ScalarParameter Roughness = 0.5 [Slider(0, 1)];
```

大小写不敏感匹配，括号里必须**恰好**是两个数字。它与 `[SliderMin = 0; SliderMax = 1]` 完全等价 —— 两种写法混用算重复：

| 写法 | 结果 |
| :-- | :-- |
| `[Slider(0, 1)]` | `SliderMin = 0`，`SliderMax = 1` |
| `[SliderMin = 0; SliderMax = 1]` | 完全相同 |
| `[Slider(0, 1); SliderMin = 0]` | `DSH3257` |
| `[Slider(0)]` / `[Slider(0, 1, 2)]` / `[Slider(a, b)]` | `DSH3257` |
| `[Slider(1, 0)]` / `[Slider(1, 1)]` | `DSH7220` —— 最小值必须小于最大值 （自 2.0.0 起） |

滑条属于标量参数。在其他声明上 —— 向量、纹理、静态布尔 —— 范围被丢弃，并在**范围所写的位置**给出**一条**警告
`DSH7233` （自 2.1.0 起）（此前发射器在声明处警告两次，说 `MaterialExpressionVectorParameter` 没有 `SliderMin`、
没有 `SliderMax` 字段）。单独的 `SliderMin` 或 `SliderMax` 像其他键一样走反射写入。在展开 token 上 —— 它们的类都没有
滑条 —— 边界在构建材质时被拒绝（`DSH8212`）。

<a id="parametername"></a>

### `ParameterName`

```c
ScalarParameter Rough [ParameterName = "Surface Roughness"];
```

声明的标识符仍然是 `Graph` 使用的名字；`ParameterName` 只改变材质暴露给实例和蓝图的名字。在声明上，空值报 `DSH7227`
（自 2.0.0 起）。它只通过反射写一次 （自 2.0.0 起）；能构建的参数节点 token 的类都有这个属性（1.x 在这里拒绝的
`DynamicParameter` 现在是 `DSH3253`）。

<a id="别名重写与自动注入"></a>

## 别名重写与自动注入

写下的键决定写入什么：

| 写下的键 | 在声明上 | 在展开 token 上 |
| :-- | :-- | :-- |
| `Group`、`Category` | `@group` → `Group` | 参数 `Group` |
| `Description`、`Desc`、`Tooltip` | `@desc` → `Desc` | 参数 `Desc` |
| `SortPriority`、`Sort` | `@sort` → `SortPriority` | 参数 `SortPriority` |
| `ParameterName` | `@name` | 参数 `ParameterName` |
| `SamplerType` | `@sampler`，去掉 `SAMPLERTYPE_` 前缀 | 参数 `SamplerType` |
| `Slider(a, b)`，或同时写 `SliderMin` 和 `SliderMax` | `@slider a b` | 参数 `SliderMin`、`SliderMax` |
| 其他任意键 | 小写拼写，走反射写入 | 同名参数 |

在声明上，外层 [`Group("Name") { … }` 作用域](#group-作用域与-sortpriority-计数器)会注入 `Group`（除非写了 `Group`
或 `Category`）和自动 `SortPriority`（除非写了 `SortPriority` 或 `Sort`）。展开 token 两者都不注入。

<a id="反射属性透传"></a>

## 反射属性透传

在**声明**上，不认识的键在构建材质时解析成生成节点类上的一个 `FProperty`：

1. 键与每个 UPROPERTY 名忽略大小写比较；
2. 若失败，第二轮**去掉每个 `FBoolProperty` 名开头的 `b`** 再比较一次。

于是 `[FractionalPart = true]` 绑定到 `bFractionalPart`，`[UseCustomPrimitiveData = true]` 绑定到
`bUseCustomPrimitiveData`。显式写出 `b` 同样有效。没有属性匹配的键是**警告** `DSH8210`，不写入 （自 2.0.0 起）
（1.x 报错）。

在**展开 token** 上，键先和类的输入引脚比对，再和引擎目录列出的属性比对（布尔属性不带 `b` 的拼写是别名）；只差大小写
的匹配给出 `DSH5276`。两者都不是的键报 `DSH5213` —— 或者，值是数字时，被当作以节点属性命名的引脚（`DSH5291`，提示），
节点没有这个引脚时在构建材质时被拒绝（`DSH8212`）。

<a id="按属性类型的取值语法"></a>

### 按属性类型的取值语法

| 属性类型 | 接受的文本 | 失败时的码 |
| :-- | :-- | :-- |
| `bool` | `true` / `false`，不区分大小写 | `DSH7132` |
| `int32` | 整数 | `DSH7133` |
| `uint32` | `[0, 4294967295]` 内的整数 | `DSH7134` |
| `float` | 数字 —— 这里 `true` / `false` 不算数字 | `DSH7135` |
| `double` | 同 `float` | `DSH7136` |
| `FString` | 任意文本，trim 后原样保留 | —— |
| `FName` | 任意文本，转成 `FName` | —— |
| 对象引用 | `Path(…)`、以 `/` 开头的绝对路径，或 `Class'…'` 引用 —— 见 [Path 资产引用](path.md) | 见[对象属性](#对象属性) |
| `enum` | 枚举字面量，四种拼写 —— 见[下文](#枚举字面量) | `DSH7140` |
| 以枚举为底的 `uint8` | 枚举字面量 | `DSH7141` |
| 普通 `uint8` | `[0, 255]` 内的整数 | `DSH7142` |
| 其他（结构体、数组等） | Unreal 自己的 import 文本，如 `(R=1,G=0,B=0,A=1)` | `DSH7143` |

数字按 Unreal 的 `LexTryParseString` 读，遇到第一个用不了的字符就停：`"0.5abc"` 是 `0.5`。这些码都包在 `DSH8213` 里
到达你，`DSH8213` 会指出节点和属性并引用这个码。在展开 token 上，绑定器先检查值的种类：枚举值按目录检查（见下），名字
或资产路径要是带引号的字符串，其余要是编译期常量（`DSH5224`）。

<a id="枚举字面量"></a>

### 枚举字面量

枚举值先 trim、小写，再删除所有空格、`_`、`-`、`:`、`.` 和 `/`。每个**未**标记 `UMETA(Hidden)` 的枚举项按四种拼写逐一尝试：

| # | 拼写 | 以 `SAMPLERTYPE_LinearColor` 为例 |
| :-- | :-- | :-- |
| 1 | 短名 | `SAMPLERTYPE_LinearColor` |
| 2 | 全限定名 | `EMaterialSamplerType::SAMPLERTYPE_LinearColor` |
| 3 | `DisplayName` 元数据 | `Linear Color` |
| 4 | 短名去掉第一个 `_` 及之前的部分 | `LinearColor` |

由于这套规范化，`"LinearColor"`、`"linear color"`、`"linear-color"`、`"SAMPLERTYPE_LinearColor"` 和 `"linear.color"`
都选中同一个值。这是声明上的键走的匹配器。

在**展开 token** 上，绑定器把值和引擎目录列出的枚举项比较 （自 2.0.0 起）：精确的名字通过；上面某种拼写规范化后
等于枚举项名的，接受并给出警告 `DSH5278`；其他 —— 包括不同时也是枚举项名的 `DisplayName`，如 `Shared: Wrap` —— 报
`DSH5215`。

**提示：**

引擎枚举的 `_MAX` 哨兵值通常不带 `Hidden` 元数据，所以在声明上 `"SAMPLERTYPE_MAX"` 这类值能解析成功而不报错 —— 写进去
的却不是一个真实值。不要使用它们。展开 token 所对照的目录里没有它们（`DSH5215`）。

<a id="对象属性"></a>

### 对象属性

对象类型的 UPROPERTY 接受资产引用，由 [Path 资产引用](path.md)里的解析器处理。

| 情况 | 码 |
| :-- | :-- |
| 值以 `Path(` 或 `/` 开头、或以 `'` 结尾，但解析不了 | 解析器自己的码 —— `DSH8118`–`DSH8132`，或 `Class'…'` 写了不相关的类时 `DSH1045` |
| 值以上都不是 | `DSH7137` |
| 资产能加载但类不对 | `DSH7139` |
| 资产加载失败 | `DSH7138` |

**警告：**

**加载失败的纹理会被静默接受。** 当属性是 `UTexture` 子类**且**名字恰好为 `Texture` 或 `TextureObject` 时，加载失败会
写入 `nullptr`，并把这次写入报告为成功。于是 `[Texture = Path(Game, "Typo")]` 不产生任何诊断就生成完毕，留下一个未绑定的
采样器。（自 2.0.0 起） 纹理采样 token 和 `const` 纹理的默认值也写到这个属性上，同样如此；纹理采样节点随后取引擎
默认纹理。采样器出来不对时，先检查资产路径。

<a id="不走反射的组织字段"></a>

## 不走反射的组织字段

`Group`、`SortPriority` 和 `Desc` 存在于 `UMaterialExpressionParameter` 子类上。缺少其中之一的类得到**警告** `DSH8210`，
值不写入。能构建的 15 个 token 生成的类都有这三个字段。

（自 2.0.0 起） 声明的类缺少的*其他*键也是同一个 `DSH8210` 警告，而 1.x 会让编译失败。在展开 token 上它是 `DSH5213`。

<a id="group-作用域与-sortpriority-计数器"></a>

## `Group(…)` 作用域与 SortPriority 计数器

（自 1.5.0 起） `Properties` 段里的 [`Group("Name") { … }` 作用域](../language/sections.md#properties)会给
成员打标记。它与显式条目的交互如下：

| 情况 | 结果 |
| :-- | :-- |
| 成员既没写 `Group` 也没写 `Category` | 注入外层分组 |
| 成员写了 `Group` 或 `Category` | 写的值优先；该成员忽略作用域 |
| 嵌套作用域 | 用 `\|` 组合 —— `Group("Outer") { Group("Inner") { … } }` 得到 `Outer\|Inner` |
| 字面写成 `Group("Manual\|Literal")` | 原样透传 |
| 成员既没写 `SortPriority` 也没写 `Sort` | 从自动计数器取下一个值 |
| 成员写了 `SortPriority` 或 `Sort` | 写的值优先，**且不消耗计数器槽位** |
| 成员是展开 token（静态开关、mask、纹理采样） | 不打标记：无分组、无自动排序值、不占槽位 （自 2.0.0 起） |
| 成员是 `const` 声明 | 打标记并占一个槽位，但两个标记都是 `DSH7224` 警告：常量没有分组和排序 |
| 作用域之外的声明 | 无分组，`SortPriority` 为 32 |
| `Group("")` | `DSH3260` |

计数器从 **0** 开始，步长 **10**，并且在整个块内的**所有作用域间共享**，不会按分组重置：

```c
Properties {
    Group("Surface") {
        ScalarParameter A = 0.5;                   // SortPriority = 0
        VectorParameter B = float4(1, 1, 1, 1);    // SortPriority = 10
    }
    Group("Detail") {
        ScalarParameter C = 1.0 [SortPriority=99]; // 99 —— 不消耗槽位
        ScalarParameter D = 2.0;                   // SortPriority = 20
    }
    ScalarParameter Loose = 3.0;                   // 无分组，SortPriority = 32
}
```

<a id="samplertype"></a>

## SamplerType

`SamplerType` 设置纹理节点如何解释它的纹理：生成的 `UMaterialExpressionTextureBase` 子类上的 `EMaterialSamplerType`。
它适用于紧凑纹理 token（含 `const`）、`TextureObjectParameter` 和纹理采样 token 15–20；RVT、SVT 的类没有 `SamplerType`
（`DSH5213`）。

（自 2.0.0 起） 在声明（紧凑纹理 token、`TextureObjectParameter`）上，它变成 2.0 的 `/// @sampler` 指令，去掉
`SAMPLERTYPE_` 前缀，由[枚举字面量](#枚举字面量)匹配器解析（都没匹配上报 `DSH8235`，空值报 `DSH7222`）。在纹理采样
token 上它是反射节点的参数，绑定时按引擎目录检查。

17 个枚举项。同一行的四种拼写在声明上都能选中它：

| 值 | 枚举常量 | `DisplayName` 拼写 |
| :-- | :-- | :-- |
| `Color` | `SAMPLERTYPE_Color` | `Color` |
| `Grayscale` | `SAMPLERTYPE_Grayscale` | `Grayscale` |
| `Alpha` | `SAMPLERTYPE_Alpha` | `Alpha` |
| `Normal` | `SAMPLERTYPE_Normal` | `Normal` |
| `Masks` | `SAMPLERTYPE_Masks` | `Masks` |
| `DistanceFieldFont` | `SAMPLERTYPE_DistanceFieldFont` | `Distance Field Font` |
| `LinearColor` | `SAMPLERTYPE_LinearColor` | `Linear Color` |
| `LinearGrayscale` | `SAMPLERTYPE_LinearGrayscale` | `Linear Grayscale` |
| `Data` | `SAMPLERTYPE_Data` | `Data` |
| `External` | `SAMPLERTYPE_External` | `External` |
| `VirtualColor` | `SAMPLERTYPE_VirtualColor` | `Virtual Color` |
| `VirtualGrayscale` | `SAMPLERTYPE_VirtualGrayscale` | `Virtual Grayscale` |
| `VirtualAlpha` | `SAMPLERTYPE_VirtualAlpha` | `Virtual Alpha` |
| `VirtualNormal` | `SAMPLERTYPE_VirtualNormal` | `Virtual Normal` |
| `VirtualMasks` | `SAMPLERTYPE_VirtualMasks` | **`Virtual Mask`** |
| `VirtualLinearColor` | `SAMPLERTYPE_VirtualLinearColor` | `Virtual Linear Color` |
| `VirtualLinearGrayscale` | `SAMPLERTYPE_VirtualLinearGrayscale` | `Virtual Linear Grayscale` |

`DistanceFieldFont` 和 `External` 没有 virtual 版本。在纹理采样 token 上，精确的枚举项名通过，规范化后等于枚举项名的
写法（`SAMPLERTYPE_Normal`、`linear color`）给出 `DSH5278`，不同时也是枚举项名的 `DisplayName` —— `Virtual Mask` ——
报 `DSH5215`。

<a id="应用顺序"></a>

### 应用顺序

| 步骤 | 发生了什么 |
| :-- | :-- |
| 1 | 创建节点并指定纹理资产 —— 来自 `= Path(…)`、来自声明维度对应的引擎回退资产，或由 `SetDefaultTexture()` 提供 |
| 2 | 源码没给 `SamplerType` 时，执行 `AutoSetSampleType()`，根据资产的压缩设置和 sRGB 标记推导 |
| 3 | 给了就写入那个值 |

显式 `SamplerType` **永远胜出**：有它的节点上根本不做推导。不写这个键则值跟随资产 —— 资产以后换了压缩设置，值也会变。
因此[反编译器](../tools/decompiler.md)每次导出都显式写出 `SamplerType` 和纹理采样相关键，让往返保持稳定。

<a id="samplersource-与其他纹理采样键"></a>

### `SamplerSource` 与其他纹理采样键

`SamplerSource` 只用于纹理采样 token，按目录检查：

| 值 | 枚举常量 | 含义 |
| :-- | :-- | :-- |
| `FromTextureAsset` | `SSM_FromTextureAsset` | 采样器取自纹理；占用着色器有限的采样器槽位之一 |
| `Wrap_WorldGroupSettings` | `SSM_Wrap_WorldGroupSettings` | 共享采样器，wrap 寻址，过滤来自 world 纹理组；不占槽位 |
| `Clamp_WorldGroupSettings` | `SSM_Clamp_WorldGroupSettings` | 共享采样器，clamp 寻址；不占槽位 |

`SSM_TerrainWeightmapGroupSettings` 带 `UMETA(Hidden)`，**不可选**。写枚举项名 —— `[SamplerSource="Wrap_WorldGroupSettings"]`；
`SSM_Wrap_WorldGroupSettings` 接受并给出 `DSH5278`。显示名 `Shared: Wrap` / `Shared: Clamp` 报 `DSH5215`
（自 2.0.0 起）（1.x 接受）。

反编译器在每个纹理采样参数上写出的键，以及它视为默认值的值：

| 键 | 类型 | 默认值 |
| :-- | :-- | :-- |
| `SamplerType` | `EMaterialSamplerType` | 由资产推导 |
| `SamplerSource` | `ESamplerSourceMode` | `FromTextureAsset` |
| `MipValueMode` | `ETextureMipValueMode` | `None` |
| `GatherMode` （自 UE 5.6 起） | 枚举 | `None` |
| `AutomaticViewMipBias` | bool | `true` |
| `ConstCoordinate` | `uint8` | `0` |
| `ConstMipValue` | `int32` | `-1` |
| `IsDefaultMeshpaintTexture` | bool | `false` |

`MipValueMode` 是唯一会改变节点**引脚**名的键，因而也决定了[引脚调用形式](../language/properties.md#各参数类型的引脚名)
在实际节点上能匹配哪些名字。

<a id="这个块还能用在哪"></a>

## 这个块还能用在哪

- 材质函数 `Inputs` / `Outputs` / `Results` 的类型化参数上，但那里只有 `Description` / `Desc` / `Tooltip` 和
  `SortPriority` / `Sort` 有效。`Group` 会被解析并保留但从不应用 —— Unreal 的函数输入/输出节点没有分组字段 —— 其他键则
  被忽略而不是走反射。输入的 `SortPriority` 默认取声明序号。
- **不能**用在 `Shader` 的 `Outputs` 语句上，也不能用在 `Settings`、`Options` 或 `Layout` 条目上。
- `const` 声明只保留常量节点有的东西：见 [Properties 类型](../language/properties.md#const-声明)。

<a id="诊断"></a>

## 诊断

<a id="解析期"></a>

### 解析期

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH3250: Expected a property type and a name, found {0}.` | 语句里只有一个 \[ … \] 块。 |  |
| `DSH3255: Expected ']' to close the metadata block, found {0}.` | 键不是名字、键后面没有 =、= 后面没有值，或块没有 \]。 |  |
| `DSH3256: Expected the metadata key '{0}' once, found it again.` | 键重复（忽略大小写比较）。 |  |
| `DSH3257: Expected 'Slider(min, max)' with two numbers, found '{0}'.` | Slider(…) 不是恰好两个数字，或与 SliderMin / SliderMax 同时出现。 |  |
| `DSH3258: Expected 'SortPriority' to be a whole number, found '{0}'.` | SortPriority / Sort 不是整数。 |  |
| `DSH3260: Expected a name inside 'Group("...")', found an empty string.` | Group("") { … }。 |  |

<a id="绑定期"></a>

### 绑定期

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH7220: '@slider' takes two numbers, a minimum and a maximum; '{0}' is not that.` | 滑条的最小值不小于最大值。 |  |
| `DSH7233: A slider range ('@slider', or 'Slider(min, max)' in a 1.x property) is for a scalar parameter; this one is '{0}', which has no slider, so the range is ignored.` | 警告：不是标量的声明上写了滑条，范围被丢弃。 | 删掉 Slider，或把 property 改成标量。 |
| `DSH7227: '@{0}' needs a value after it.` | 声明上的 ParameterName 为空。 |  |
| `DSH7224: '@{0}' means nothing here; it belongs on {1}.` | 警告：const 上的 Group 或 SortPriority。 |  |
| `DSH7222: '@sampler' needs a sampler type after it, such as 'Color', 'Normal' or 'LinearColor'.` | 声明上的 SamplerType 为空。 |  |
| `DSH5213: '{0}.{1}' has no pin or property called '{2}'.` | 展开 token 的键既不是类的引脚也不是属性。 |  |
| `DSH5215: '{0}' is not a value of '{1}' on '{2}'.` | 展开 token 的枚举值不是枚举项。 |  |
| `DSH5278: '{0}' is not spelled like a value of '{1}', and 1.x matched enumerators loosely, so this is '{2}'; a '.dss' writes '{2}'.` | 警告：展开 token 的枚举值是某个枚举项的 1.x 拼写。 |  |
| `DSH5224: '{0}' takes a name or an asset path; write it as a quoted string.` | 展开 token 的值不是其属性接受的种类。 |  |
| `DSH5276: '{0}' matches the engine name '{1}' only in case; 1.x matched engine names ignoring case, so this is '{1}', and a '.dss' needs the exact spelling.` | 警告：展开 token 的键只在忽略大小写时匹配引脚或属性。 |  |

<a id="构建期"></a>

### 构建期

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH8210: '{0}' has no property named '{1}', so that value was not written.` | 警告：声明的键，或 Group / SortPriority / Desc / ParameterName 之一，不是类的属性；不写入。 | 到该 token 生成的 Unreal 类上核对属性名。 |
| `DSH8213: '{0}' could not take '{1}' for its '{2}' property. {3}` | 属性接不住它的值；消息引用 DSH7132–DSH7143 或解析器的码。 |  [详解](path.md) |
| `DSH8212: '{0}' has no input pin named '{1}'.` | 展开 token 上的滑条或数值键不是节点的任何引脚。 |  |
| `DSH8235: '{0}' is not a sampler type; write one of the EMaterialSamplerType names, such as Color, Normal or LinearColor.` | 声明的 SamplerType 不是采样类型。 |  |

完整清单见[错误速查](../diagnostics/index.md)。

<a id="示例"></a>

## 示例

```c
Shader(Name="Docs/M_Metadata")
{
    Properties = {
        Group("11 - Specular") {
            TextureSampleParameter2D MetallicMap = Path(Game, "Textures/T_White_Linear") [
                SamplerType          = "LinearColor";
                SamplerSource        = "FromTextureAsset";
                MipValueMode         = "None";
                AutomaticViewMipBias = true;
                ConstCoordinate      = 0;
                ConstMipValue        = -1;
                Description          = "Packed metallic / roughness";
            ];

            ScalarParameter Metallic = 0.0 [Slider(0, 1); SortPriority = 51];
        }

        VectorParameter Tint = float4(1, 1, 1, 1) [
            Category               = "Look",
            Tooltip                = "Multiplied over base colour",
            UseCustomPrimitiveData = false,
            ParameterName          = "Base Tint"
        ];
    }

    Settings = { Domain = "Surface"; ShadingModel = "DefaultLit"; BlendMode = "Opaque"; }
    Outputs  = { vec3 Color; float M; Base.BaseColor = Color; Base.Metallic = M; }

    Graph = {
        vec4 S = MetallicMap(Coordinates = UE.TexCoord(Index = 0));
        Color = S.rgb * Tint.rgb;
        M     = S.b * Metallic;
    }
}
```

实际应用到节点上的属性：

```text
MetallicMap  Desc="Packed metallic / roughness"
             SamplerType=SAMPLERTYPE_LinearColor     SamplerSource=SSM_FromTextureAsset
             MipValueMode=TMVM_None  AutomaticViewMipBias=true
             ConstCoordinate=0       ConstMipValue=-1
Metallic     Group="11 - Specular"  SortPriority=51  SliderMin=0  SliderMax=1
Tint         Group="Look"           Desc="Multiplied over base colour"
             bUseCustomPrimitiveData=false           ParameterName="Base Tint"
```

`Metallic` 用了自己条目里的 `SortPriority = 51`。`MetallicMap` 是展开 token，所以 `Group("11 - Specular")` 作用域不会给
它打标记 （自 2.0.0 起）：要把它放进这个分组，就在它自己的块里写 `Group = "11 - Specular"`。

<a id="下一步"></a>

## 下一步

- [Properties 类型](../language/properties.md) —— 元数据块能跟在哪些 token 后面，以及每个 token 暴露的专属键
- [Path 资产引用](path.md) —— 对象类型条目接受的语法
- [材质 Settings](../settings/material.md) —— 同一套反射思路，作用在 `UMaterial` 上
- [反编译导出](../tools/decompiler.md) —— 往返时一定会写出哪些键
