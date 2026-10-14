---
title: 顶层块
description: 七种顶层块 —— Shader、ShaderFunction、ShaderLayer / ShaderLayerBlend、VirtualFunction、Function、GraphFunction、Namespace —— 及其各自的产出。
---

一个源文件就是一串平铺的顶层块。它们可以按任意顺序出现，彼此之间没有分隔符，除 `Shader` 之外每种都
可以在一个文件里出现任意多次。编译一个文件一次，就会构建它所有块描述的全部资产。

块关键字和小写保留字是**大小写敏感**的。`shader(…)` 和 `SHADER(…)` 什么都匹配不上，报 `DSH2240`。见
[词法与大小写](lexical.md)。

<a id="语法概览"></a>

## 语法概览

```c
<top-level-block> := { Shader | ShaderFunction | ShaderLayer | ShaderLayerBlend
                     | MaterialLayer | MaterialLayerBlend | VirtualFunction | Namespace }
                     ( <attribute> = <value> [, …] ) { <section>… }
                   | { Function [ SelfContained | Inline ] | GraphFunction }
                     [<return-type>] <name> ( [<parameter>, …] ) { <HLSL> }

<section>          := <section-name> [=] { <statement>… } [;]
```

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

也就是两种头部形态：资产型块用 `( Key = Value )` 属性列表加一组 [section](sections.md)；两种
函数型块用 C 风格签名加一段原始 HLSL。legacy 前端把每种块读成对应的 2.0 声明 （自 2.0.0 起），
[`dsc migrate`](../tools/migrate.md) 也按同样的对应关系写出 `.dss`。

<a id="各个块产出什么"></a>

## 各个块产出什么

| 块 | 头部 | 产出 | 起始版本 |
| :-- | :-- | :-- | :-- |
| `Shader` | `Shader(Name = "…"[, Root = "…"])` | `UMaterial`；在 ThinCustom 后端下是覆盖在隐藏基材质上的 `UDreamShaderMaterialInstance` | — |
| `ShaderFunction` | `ShaderFunction(Name = "…"[, Root = "…"])` | `UMaterialFunction`（usage `Default`） | — |
| `ShaderLayer` | `ShaderLayer(Name = "…"[, Root = "…"])` | `UMaterialFunctionMaterialLayer` | （自 1.3.0 起） |
| `ShaderLayerBlend` | `ShaderLayerBlend(Name = "…"[, Root = "…"])` | `UMaterialFunctionMaterialLayerBlend` | （自 1.3.0 起） |
| `VirtualFunction` | `VirtualFunction(Name = "…"[, Asset = "…"])` | **什么都不产出** —— 声明一个已存在的资产 | （自 1.2.0 起） |
| `Function` | `Function [SelfContained \| Inline] [<ret>] <Name>( … )` | 不产出资产也不产出文件；每个调用点一个 `Custom` 节点，代码里带着函数体和它调用的其他 `Function` | — |
| `GraphFunction` | `GraphFunction [<ret>] <Name>( … )` | 自身不产出；每个调用点一个 `Custom` 节点外加被提升出来的材质节点 | （自 1.3.1 起） |
| `Namespace` | `Namespace(Name = "…")` | 什么都不产出 —— 给成员名加上 `Ns::` 前缀 | — |

<a id="头部属性"></a>

### 头部属性

每个带属性的块都必须写 `Name`，缺失或为空是 `DSH2242`；块关键字后面没有 `(` 是 `DSH2241`。`Root` 默认是
`/Game` —— 插件源码根下的文件默认是该插件的挂载点；完整的路径语法见 [资产路径](../generation/asset-paths.md)。

- 属性键大小写不敏感（`name=` 也行）。值可以带引号也可以不带；不带引号的值一直延续到括号**之外**的下一个
  `,` 或 `)`，所以头部里写 `Asset = Path(Game, "F/X")` 也没问题 （自 2.0.0 起）。
- 同一个键写两次：后写的胜出，并给出警告 `DSH2244` （自 2.0.0 起）。
- `)` 之前的尾随逗号可以接受，和 1.x 一样 （自 2.1.1 起）（2.0.0 – 2.1.0 报 `DSH2243`）。两个连续逗号、缺键、
  缺 `=` 或缺值仍然是 `DSH2243`。
- 头部之后没有 `{`、或 section 缺名字缺 `{`，是 `DSH2257`；文件在块中间结束是 `DSH2150`。

<a id="shader"></a>

## `Shader`

声明一个材质：它的参数、渲染状态、输出绑定，以及供给它们的图。`.dss` 把同一个材质写成
`#pragma material(...)`、若干 `uniform` 和一个 `export void <Name>(inout material m)`。

```c
Shader(Name = "<asset-path>" [, Root = "<root>"])
{
    [Properties [=] { <property-declaration> ; … }]
    [Settings   [=] { <key> = <value> ; … }]
    [Outputs    [=] { { <output-declaration> | <output-binding> } ; … }]
    [Graph      [=] { <graph-statement> … }]
    [Layout     [=] { { Node( … ) | Comment( … ) } ; … }]
}
```

```c
Shader(Name="Materials/M_Emissive", Root="Game")
{
    Properties = {
        Group("Look") {
            vec3  Tint      = vec3(1.0, 0.4, 0.1) [Description="Emissive tint"];
            float Intensity = 2.0                 [Slider(0, 10)];
        }
        TextureSampleParameter2D BaseTex = Path(Game, "Textures/T_Noise");
    }

    Settings = {
        ShadingModel = "Unlit";
        BlendMode    = "Translucent";
        TwoSided     = true;
    }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph = {
        vec2 UV  = UE.TexCoord(Index = 0);
        vec4 Tex = BaseTex(Coordinates = UV);
        Color = Tex.rgb * Tint * Intensity;
        Alpha = Tex.a;
    }
}
```

只对这个块成立的规则：

- **只能写在 `.dsm` 里。** `.dsh` 里的 `Shader` 是 `DSH2249`，`.dsf` 里的是 `DSH2259` （自 2.1.1 起）。
- **每个文件最多一个**（`DSH2250`）。header 里不能有 `Shader`，所以 import 永远不会带进第二个。
- **可以和函数块、层块放在同一个 `.dsm` 里。** 一次编译把它们全部构建出来，各自成为一个资产；材质按名字调用
  这些函数，函数资产先构建 （自 2.1.1 起）（2.0.0 – 2.1.0 以 `DSH6201` 拒绝这种组合）。
- 没有 `Graph` 时，`Outputs` 里的语句得自己算出值 —— 带初始化式的输出声明 （自 1.3.4 起），或者右侧是表达式
  的绑定，例如 `Base.FrontMaterial = Substrate.Layer(Coat, Body);` （自 2.0.0 起）。既没有 `Graph`、
  `Outputs` 又是空的，是 `DSH2255`。空的 `Graph = { }` 合法。
- **真正让材质成立的是绑定。** 没有 `Outputs` section 的 `Shader` 是警告 `DSH2256`：材质照样构建，只是什么都
  没连上 （自 2.0.0 起）（1.9.x 及以前随后会在生成阶段失败）。
- 绑定 `Base.MaterialAttributes` 会自动打开 *Use Material Attributes*。`Base.FrontMaterial`（需要
  （自 UE 5.4 起））现在是普通属性 （自 2.0.0 起）：不再强制把着色模型设为 Substrate，也不再拒绝和
  `Base.MaterialAttributes` 并用；需要时在 `Settings` 里自己写 `ShadingModel = "Substrate";`。见
  [输出绑定](../generation/outputs.md)。
- `Inputs`、`Results`、`Options` 不是 `Shader` 认识的 section（`DSH2245`）；`Code` 是 `DSH2246`。`Graph` 或
  `Layout` 写两次时后一个胜出，并给出警告 `DSH2258`。

**提示：**

目标路径上最终落地的是哪个 UClass，取决于解析出的后端。默认的 `ThinCustom` 写出一个
`UDreamShaderMaterialInstance`，其父级是名为 `MB_DreamThinBase_<leaf>` 的隐藏 `UMaterial` 子对象；`Graph`
写出普通的 `UMaterial`。`Instance` 是 `ThinCustom` 的已弃用别名（警告 `DSH7204`）。`Graph` 后端的材质每次
构建成功都会保存 （自 2.0.0 起）；`ThinCustom` 产物在 cook、显式 *Materialize* 或有子实例之前一直是
**Ephemeral** 的（只在内存里）。见 [Backend](../settings/backend.md) 与 [内存材质](../generation/ephemeral.md)。

<a id="shaderfunction"></a>

## `ShaderFunction`

声明一个可复用的 `UMaterialFunction`：类型化的输入输出引脚、函数内部的参数节点，以及把它们连起来的图。
legacy 前端把它读成 2.0 的 `export` 函数：输入是参数，名字恰好叫 `Result` 的第一个输出是返回值，其余输出是
`out` 参数。

```c
ShaderFunction(Name = "<asset-path>" [, Root = "<root>"])
{
    [Properties            [=] { <property-declaration> ; … }]
    [Inputs                [=] { <parameter-declaration> ; … }]
    { Outputs | Results }  [=] { <parameter-declaration> ; … }
    Graph                  [=] { <graph-statement> … }
    [Settings              [=] { <key> = <value> ; … }]
    [Layout                [=] { { Node( … ) | Comment( … ) } ; … }]
}
```

```c
ShaderFunction(Name="Functions/F_Tint", Root="Game")
{
    Properties = {
        Group("Tint") {
            float Boost = 1.0 [Slider(0, 4); Description="Extra gain applied after tinting"];
        }
        const float Epsilon = 0.001;
    }

    Inputs = {
        vec3  InColor;
        vec3  InTint             [Description="Multiplied with InColor"];
        opt float Strength = 1.0 [Description="Blend amount"; SortPriority=10];
    }

    Outputs = {
        vec3  OutColor;
        float OutLuma;
    }

    Graph = {
        vec3 Tinted = InColor * InTint * Boost;
        OutColor    = lerp(InColor, Tinted, Strength);
        OutLuma     = dot(OutColor, vec3(0.2126, 0.7152, 0.0722)) + Epsilon;
    }
}
```

函数至少要有一个输出（`DSH4315`），`Graph` 必须给第一个 `Result` 之外的每个输出赋值（`DSH6211`）。这里的
`Properties` 声明的是函数*内部*的节点，`Inputs` 声明的是函数*上面*的引脚 —— 这个区别是本块最常见的困惑来源，
详见 [Section](sections.md)。

在同一个文件另一个块的 `Graph` 里，被调用名是资产的叶子名：`Name="Functions/F_Tint"` 就是 `F_Tint`。资产块
不能被导入（`DSH2249`），所以其他文件通过 [`VirtualFunction`](#virtualfunction) 使用生成的资产。如果文件
能调用的另一个函数（比如导入 header 里的 `Function`）已经占了这个名字，这个块的函数会改名为 `<Leaf>_Asset`
并给出 info `DSH5290`，资产本身保持原名。

**提示：**

重新生成时，每个输入输出引脚的 `Id` GUID 会按名字缓存并还原 （自 1.3.2 起），所以已有的手工
`MaterialFunctionCall` 节点能保住连线 —— **前提是引脚名没变**。改名等同于删掉再新建：调用点会丢连接。

<a id="shaderlayer-与-shaderlayerblend"></a>

## `ShaderLayer` 与 `ShaderLayerBlend`

（自 1.3.0 起）

`ShaderFunction` 的材质层变体。它们共用 `ShaderFunction` 的函数体解析器和 section 表，区别只在接口规则。
legacy 前端把 `ShaderLayer` 读成 2.0 的 `@layer` 函数，把 `ShaderLayerBlend` 读成 `@layerblend` 函数，
规则在绑定时检查 （自 2.0.0 起）。

```c
ShaderLayer(Name = "<asset-path>" [, Root = "<root>"])
{
    [Inputs                [=] { MaterialAttributes <name> ; }]
    { Outputs | Results }  [=] { MaterialAttributes <name> ; }
    Graph                  [=] { <graph-statement> … }
}

ShaderLayerBlend(Name = "<asset-path>" [, Root = "<root>"])
{
    Inputs                 [=] { MaterialAttributes <name> ; MaterialAttributes <name> ; }
    { Outputs | Results }  [=] { MaterialAttributes <name> ; }
    Graph                  [=] { <graph-statement> … }
}
```

| 类型 | 规则 | 诊断 |
| :-- | :-- | :-- |
| 两者 | 声明一个 `MaterialAttributes` 输出 | `DSH3278` |
| `ShaderLayer` | 只有材质：任何非 `MaterialAttributes` 输入、或材质之外的任何输出，都会破坏签名 | `DSH6204` |
| `ShaderLayerBlend` | 至少一个 `MaterialAttributes` 输入，材质之外没有别的输出 | `DSH6205` |

层的材质是**穿过**它的：层永远恰好有一个 `MaterialAttributes` 输入，以输出的名字命名 —— 1.x 块一个输入都没
声明时也一样。1.x 里名字不同的 `MaterialAttributes` 输入会变成这个输入，引脚随之改名并给出警告 `DSH3277`；
函数体里照样可以用 1.x 的名字读它。只有 1.x 输入写了 `opt` 时这个输入才是可选的。混合块的输出从空材质开始，
每个输入各是一个引脚；引擎的层堆栈混合的正好是两个。

标量、向量和纹理不能作为层或混合的输入，改用 `Properties` 暴露：它们会变成生成函数内部的参数节点，并出现在
材质层堆栈的参数面板上。层和混合也不能从 `Graph` 调用（`DSH6208`），它们的使用者是材质上的层堆栈。

```c
ShaderLayer(Name="Layers/L_Rust", Root="Game")
{
    Properties = {
        Group("Rust") {
            vec3  RustColor = vec3(0.35, 0.13, 0.05);
            float RustRough = 0.85 [Slider(0, 1)];
        }
    }

    Outputs = { MaterialAttributes Attrs; }

    Graph = {
        Attrs.BaseColor = RustColor;
        Attrs.Roughness = RustRough;
        Attrs.Metallic  = 0.0;
    }
}
```

**提示：**

在 （自 UE 5.7 起） 上，混合块的每个 `MaterialAttributes` 输入会根据名字得到一个
`BlendInputRelevance` —— 名字去掉空格、`_`、`-` 后忽略大小写比较：`Top` / `TopLayer` 映射为 `Top`，`Bottom` /
`BottomLayer` / `Base` / `BaseLayer` 映射为 `Bottom`。其他名字按位置回退：第一个输入是 `Bottom`，之后的是
`Top`。UE 5.3–5.6 上这个属性不存在，不会写入任何东西。

目标路径上已有资产时，只要它**是**预期类的子类就会被复用；`ShaderFunction` 要求类完全一致。所以在
`ShaderFunction` 和 `ShaderLayer` 之间切换而不挪走旧资产，会失败于 `DSH8110`。

<a id="已弃用的写法"></a>

### 已弃用的写法

**自 1.3.0 起弃用。**请改用 `ShaderLayer`。

`MaterialLayer(...)` 和 `MaterialLayerBlend(...)` 仍然能解析，也仍然生成完全相同的资产，各自给出警告
`DSH2251`。缺少 `Name` 时（`DSH2242`）消息里报的是你实际写的那个拼法。

<a id="virtualfunction"></a>

## `VirtualFunction`

（自 1.2.0 起）

声明一个**已经存在**的 `UMaterialFunction` —— 它的路径和引脚签名 —— 好让 `Graph` 能调用它。它什么都不
生成，在调用点需要它之前也什么都不校验。legacy 前端把它读成带 `/// @asset` 的 2.0 `extern` 原型。

```c
VirtualFunction(Name = "<call-name>" [, Asset = "<asset-reference>"])
{
    [{ Options | Settings }   [=] { Asset = <asset-reference> ; … }]
    [{ Inputs | Properties }  [=] { <parameter-declaration> ; … }]
    { Outputs | Results }     [=] { <parameter-declaration> ; … }
}
```

```c
VirtualFunction(Name="BufferWriter")
{
    Options = {
        Asset       = Path(Game, "MaterialFunctions/F_BufferWriter");
        Description = "Existing material function declared for Graph calls.";
    }

    Inputs = {
        vec3  Color;
        float Alpha;
        opt float Exposure = 1.0;
    }

    Outputs = {
        vec3  Result;
        float Coverage;
    }
}
```

- `Name` 是**调用名**，不是资产路径，必须是一个标识符（`DSH6311`）。1.x 接受带 `/` 的名字、只让最后一段可调用；
  现在那也是 `DSH6311` （自 2.0.0 起）。这里没有 `Root` 属性 —— 包根是资产引用自身的一部分。
- 头部的 `Asset=` 属性优先于 `Options.Asset`；只有属性缺失或去空白后为空时才读 section 里的值。两处都没有是
  `DSH6312`；没有任何输出是 `DSH6313`。
- `Options` 里的 `Description` 会作为声明的描述保留（`dsc migrate` 把它写在 `extern` 上方），不影响构建；其他
  键读取后忽略。
- `Graph` 和 `Code` 是 `DSH2247`：这个块声明资产，不构建资产。`Layout` 和其他名字是 `DSH2245`。
- 资产引用在构建调用时才解析：解析不了是 `DSH8270`，加载不到是 `DSH8219`；声明的输入输出和资产引脚对不上是
  `DSH8220` / `DSH8221`。没人调用的 `VirtualFunction` 带着坏掉的 `Asset` 也能干净地编译。
- 两个同名的 `VirtualFunction` 是 `DSH4210` （自 2.0.0 起）（1.x 保留两者并调用第一个）。只含
  `VirtualFunction` 块的文件编译成功、不产出资产。

**警告：**

**在 `VirtualFunction` 内部，而且只在这里，`Properties` 是 `Inputs` 的别名。** 它走的是类型化参数
语法，不是参数节点语法。像 `const float X = 1;` 这种在 `Shader` 里合法的声明，在这里是 `DSH3271` ——
`const` 是关键字，不是类型。

<a id="function"></a>

## `Function`

一个 HLSL helper。函数体是 HLSL 文本、原样保留，每个调用点变成一个 `UMaterialExpressionCustom` 节点，节点代码
就是这段函数体，外加它调用的其他 `Function` （自 2.0.0 起）（不再写 `.ush` include）。

```c
Function [ { Inline | SelfContained } ] [ <return-type> ] <name> ( [ <parameter-list> ] )
{
    <hlsl>
}
```

```c
Function float Luma(in vec3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

Function SelfContained Remap01(in float value, out float result)
{
    result = saturate(value * 0.5 + 0.5);
}
```

它没有属性头部、没有 `Settings`、也没有任何 section —— 参数列表之后的 `{ … }` 就是原始 HLSL。`SelfContained`
和它的旧拼法 `Inline` 现在读作普通函数、什么都不改变（`Inline` 给出警告 `DSH6306`）。完整的参数、返回类型和
产出规则见 [函数](functions.md)。

<a id="graphfunction"></a>

## `GraphFunction`

（自 1.3.1 起）

写法和 `Function` 完全一样，但函数体里的 `UE.*` 调用会被从文本中提取出来，构建成真正的材质节点，并作为
自动命名的输入引脚接到生成的 `Custom` 节点上。

```c
GraphFunction [ <return-type> ] <name> ( [ <parameter-list> ] )
{
    <hlsl-with-UE-calls>
}
```

```c
GraphFunction WindPulse(in float2 uv, out float pulse)
{
    float t = UE.Time();
    pulse = sin(uv.x * 8.0 + t);
}
```

`GraphFunction` 不接受修饰符：`GraphFunction SelfContained Foo(…)` 和 `GraphFunction Inline Foo(…)` 是
`DSH6307` （自 2.0.0 起）（1.x 把这个词当成返回类型吃掉）。`GraphFunction` 没有自包含模式。

<a id="namespace"></a>

## `Namespace`

给它包含的 `Function` 和 `GraphFunction` 声明的名字加上 `<Name>::` 前缀。

```c
Namespace(Name = "<identifier>")
{
    { <function-declaration> | <graph-function-declaration> } …
}
```

```c
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

`Namespace` 不是一个实体：不会为它存储任何对象，它也**不创建作用域**。成员的名字被压平成标识符
`Common_ApplyTint` （自 2.0.0 起），`Common::ApplyTint` 和 `Common_ApplyTint` 是同一个名字。`Name` 必须是
标识符（`DSH6309`），所以没有 `A::B` 这种多段声明。它**不能嵌套**，而且只能包含那两种块 —— 其他任何东西都是
`DSH6310`。空的 `Namespace(Name="X") { }` 可以接受，什么都不声明。见 [函数](functions.md#namespace)。

<a id="对照"></a>

## 对照

| | `Shader` | `ShaderFunction` | `ShaderLayer(Blend)` | `VirtualFunction` | `Function` | `GraphFunction` | `Namespace` |
| :-- | :-: | :-: | :-: | :-: | :-: | :-: | :-: |
| 写出 `.uasset` | 是 | 是 | 是 | 否 | 否 | 否 | 否 |
| 允许在 `.dsh` 中 | 否 | 否 | 否 | 是 | 是 | 是 | 是 |
| 每文件数量 | **1** | 任意 | 任意 | 任意 | 任意 | 任意 | 任意 |
| 函数体 | section | section | section | section | HLSL | HLSL | 块 |
| `Properties` | 参数节点 | 参数节点 | 参数节点 | `Inputs` 的别名 | — | — | — |
| 必须有 `Graph` | 除非 `Outputs` 自己算出值 | 是 | 是 | 拒绝 | — | — | — |
| 可从 `Graph` 调用 | 否 | 是 | 否 | 是 | 是 | 是 | 通过成员 |
| 可当值调用 | — | 是 | — | 是 | 是 | 是 | — |

多输出的函数也可以当值调用，值是第一个输出 （自 2.0.0 起）；见 [调用](../graph/calls.md)。

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH2240: Expected a top-level Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend, Function, GraphFunction, Namespace, VirtualFunction or import, found {0}.` | 顶层出现了块关键字之外的文本 —— 包括大小写写错的正确关键字。 | 检查大小写，块关键字是大小写敏感的；也检查上方有没有没闭合的块。 [详解](lexical.md) |
| `DSH2241: Expected '(' with the block's attributes, such as '(Name = "M_Example")', found {0}.` | 块关键字后面没有属性列表。每个 1.x 块至少要在这里写名字。 | 写成 Shader(Name = "M\_Example")，然后用 { 打开块体。 |
| `DSH2242: Expected a 'Name = "..."' attribute on '{0}', found none.` | 属性列表里没有 Name，或者是空的。{0} 是你实际写的那个拼法。 | 加上 Name = "..."。 |
| `DSH2243: Expected an attribute name such as 'Name', found {0}.` | 属性列表不是逗号分隔的 Key = value：缺键、缺 =、值不是字符串 / 单词 / 数字，或连着两个逗号。) 前的单个逗号没问题。 | 每个属性写成 Key = "value"。 |
| `DSH2244: The attribute '{0}' is written twice; the later value wins, as it did in 1.x.` | 同一个属性写了两次（警告）。 | 删掉其中一个。 |
| `DSH2257: Expected '{' to open the '{0}' block, found {1}.` | 该打开块的地方没有 {：属性列表之后、section 名之后，或者块里缺了 section 名。 | 用 { 打开块体，每个 section 以名字开头。 |
| `DSH2245: Expected a Shader section (Properties, Settings, Outputs, Graph or Layout), found '{0}'.` | 块里出现了不是 section 的单词 —— Shader 里的 Inputs、Results、Options 都在其中；消息按块的种类列出它接受的 section。 | 检查拼写（section 名忽略大小写，所以是字母错了），以及前一个 section 是否已经闭合。 [详解](sections.md) |
| `DSH2246: Expected 'Graph' as the body section of '{0}', found 'Code', which 1.x accepted only inside a Function.` | Shader 或 ShaderFunction 里写了 Code section。 | 改名为 Graph，或者把 HLSL 挪进 Function 再调用它。 |
| `DSH2247: Expected no body in the VirtualFunction '{0}', which declares an existing asset, found the section '{1}'.` | VirtualFunction 里写了 Graph 或 Code section。 | 删掉函数体；如果源码本来就要构建这个资产，把块改成 ShaderFunction。 |
| `DSH2258: The section '{0}' is written twice; the later one wins, as it did in 1.x.` | 一个块里同一个 section（Graph 或 Layout）写了两次（警告）。 | 把两个 section 合并成一个。 |
| `DSH2250: Expected one Shader block in a file, found a second one.` | 一个文件里有两个 Shader 块。 | 每个材质各用一个文件。 [详解](source-files.md) |
| `DSH2255: Expected a Graph section in the Shader '{0}', found none.` | Shader 没有 Graph，Outputs 也是空的，没有东西可构建。 | 加上 Graph = { ... }，或让 Outputs 里的语句自己算出值。 |
| `DSH2256: The Shader '{0}' has no Outputs section, so nothing its Graph computes reaches the material.` | Shader 有 Graph 但没有 Outputs section（警告）。材质照样构建，只是什么都没连上。 | 加上至少一条 Base.&lt;Attribute&gt; = &lt;variable&gt;; 绑定。 [详解](../generation/outputs.md) |
| `DSH3278: Expected a MaterialAttributes output on '{0}', found none.` | ShaderLayer 或 ShaderLayerBlend 没有 MaterialAttributes 类型的输出。 | 在 Outputs 里声明 MaterialAttributes Result;。 |
| `DSH6204: A '@layer' function is written 'export void {0}(inout material m)'.` | ShaderLayer 有 MaterialAttributes 之外的输入，或材质之外的输出。 | 层的控制项挪到 Properties。 |
| `DSH6205: A '@layerblend' function is written 'export void {0}(material Base, material Top, ..., inout material Result)': at least one 'material' input and a final 'inout material'.` | ShaderLayerBlend 没有 MaterialAttributes 输入，或者有材质之外的输出。 | 混合的控制项挪到 Properties。 |
| `DSH3277: The layer input '{0}' becomes the 'inout material' parameter named after the output '{1}', so the input pin changes its name.` | 1.x 层的 MaterialAttributes 输入和输出名字不同；2.0 层只有一个 inout material 参数，取输出的名字（警告）。 | 源码不用改。层堆栈按位置连线；按引脚名连过这个函数调用的图需要重连一次。 |
| `DSH6208: '{0}' is a {1} asset, not a function this file may call.` | Graph 调用了材质入口、层或层混合。层由使用它的材质的层堆栈来应用。 | 把共享代码抽成 helper，两边都调用它。 |
| `DSH2251: '{0}' is the old spelling of '{1}'; it still reads the same.` | 使用了 MaterialLayer / MaterialLayerBlend（警告）。 | 不必改；dsc migrate 会写出当前形式。 |
| `DSH6311: Expected a 'Name = "..."' attribute with a name on 'VirtualFunction', found none.` | VirtualFunction 没有 Name，或者名字不是标识符（例如含 /）。 | 加上一个标识符名字。 |
| `DSH6312: Expected an 'Asset = Path(...)' option on the VirtualFunction '{0}', found none.` | 头部和 Options 里都没有资产引用。 | 加上 Options = { Asset = Path(Game, "Functions/MF\_Example"); }。 |
| `DSH6313: Expected at least one output on the VirtualFunction '{0}', found none.` | VirtualFunction 没有声明任何输出。 | 按资产的顺序在 Outputs 里声明它的输出。 |
| `DSH6307: Expected a return type or a name after 'GraphFunction', found the modifier '{0}', which only a Function takes.` | GraphFunction 后面跟了 SelfContained 或 Inline。 | 删掉修饰符。 [详解](functions.md) |
| `DSH6309: Expected a 'Name = "..."' attribute with a name on 'Namespace', found none.` | Namespace 没有 Name，或名字不是标识符 —— ::、.、-、空格都不行。 | 写成 Namespace(Name = "BL") { ... }。 |
| `DSH6310: Expected only Function and GraphFunction blocks inside the namespace '{0}', found {1}.` | Namespace 体内出现了其他任何东西，包括嵌套的 Namespace。 | 把那个块挪出命名空间。 |
| `DSH4210: '{0}' is already declared in this file; one name declares one thing.` | 同一个名字声明了两次 —— 本文件里，或本文件和它导入的 header 之间。没有重载。 | 给其中一个改名。 [详解](import.md) |

<a id="继续阅读"></a>

## 继续阅读

- [Section](sections.md) — Properties、Inputs、Outputs、Settings、Graph、Layout，以及哪个块收哪个。
  - [函数](functions.md) — Function、GraphFunction 与 Namespace 的完整规则。
  - [材质 Settings](../settings/material.md) — Shader 的 Settings 接受的每一个键。
  - [资产路径](../generation/asset-paths.md) — Name 和 Root 如何变成包路径。
