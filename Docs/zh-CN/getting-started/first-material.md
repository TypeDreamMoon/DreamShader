---
title: 第一个材质
description: 从零写一个 UI Unlit 材质 —— .dss 写法和 1.x 写法各一份 —— 生成它，并找到编辑器故意藏起来的那个结果。
---

本页写一个 UI 域、Unlit、带两个参数的材质，然后说明生成的材质到底在哪里 —— 这是每个人第一次都会被
绊到的地方。新源文件请写成 `.dss`；已经有 `.dsm` 的项目可以继续写 1.x 写法，两者由同一个编译器构建。

<a id="创建文件"></a>

## 创建文件

```text
<Project>/DShader/Materials/M_DreamPulse.dss
```

```hlsl
#pragma material(Domain = UI, ShadingModel = Unlit)

/// @group Color   @desc Main tint
uniform float3 Tint = float3(0.2, 0.6, 1.0);
/// @group Color   @slider 0 4
uniform float Strength = 1.0;

export void M_DreamPulse(inout material m)
{
    float t = UE.Time();
    float pulse = UE.Sine(Input = t);
    m.EmissiveColor = Tint * (pulse * 0.5 + 0.5) * Strength;
}
```

保存。在 *Auto Compile On Save* 开启的情况下，DreamShader 会在短暂防抖后编译文件并生成：

```text
/Game/Materials/M_DreamPulse
```

也可以不手写：*Material Content Browser ▸ New ▾ ▸ Material (.dss)* 从模板写出同样形状的文件。

<a id="每一部分做了什么"></a>

## 每一部分做了什么

| 部分 | 作用 | 参考 |
| --- | --- | --- |
| `#pragma material(…)` | 设置生成材质上的属性 —— 域、着色模型、混合模式，以及任何 `UMaterial` 属性 | [材质 Settings](../settings/material.md)、[枚举取值](../settings/material-enums.md) |
| `///` 行和 `uniform` | 材质参数及其元数据：分组、说明、滑条范围 | [DreamShaderLang 2.0](../language-v2/index.md#参数) |
| `export void M_DreamPulse(inout material m)` | 入口：让这个文件成为材质的那个函数。资产名就是函数名 | [DreamShaderLang 2.0](../language-v2/index.md#一个材质) |
| `m.EmissiveColor = …` | 一个材质输出，按引擎的属性表检查（拼错是 `DSH5200`） | [输出绑定](../generation/outputs.md) |
| `UE.Sine(Input = t)` | 一个材质节点，按引擎类名调用；`UE.Expression(Class = "Sine", …)` 是同一条路径 | [UE.Expression](../builtins/ue-expression.md) |

资产落在 `/Game` 下、文件相对源码根所在的文件夹里，名字取自 `export` 函数。入口上方的 `/// @name` 可以另起名字 ——
见[资产路径](../generation/asset-paths.md)。

<a id="同一个材质的-1x-写法"></a>

## 同一个材质的 1.x 写法

```c
// <Project>/DShader/Materials/M_DreamPulse.dsm
Shader(Name="Materials/M_DreamPulse")
{
    Properties = {
        vec3 Tint = vec3(0.2, 0.6, 1.0) [Group="Color"; Description="Main tint"];
        float Strength = 1.0 [Group="Color"; Slider(0, 4)];
    }

    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float t = UE.Time();
        float pulse = UE.Expression(Class="Sine", Input=t);

        Color = Tint * (pulse * 0.5 + 0.5) * Strength;
    }
}
```

| 区域 | 作用 |
| --- | --- |
| `Shader(Name="…")` | 声明要生成的资产。`Name` 是相对根的资产路径；`Root` 可选，默认为 `Game`。 |
| `Properties` | 生成材质参数节点 —— 这里是一个向量和一个标量，`[ … ]` 是它们的元数据。 |
| `Settings` | 设置生成材质上的属性。 |
| `Outputs` | 声明图输出变量，并把其中一个绑定到 `Base.EmissiveColor`。 |
| `Graph` | 创建材质节点，并把最终值赋给 `Color`。 |

两个文件构建同一个 `/Game/Materials/M_DreamPulse` —— 只留一个。[`dsc migrate`](../tools/migrate.md) 能把 1.x 文件改写成
`.dss`，并在写盘之前证明两者构建出同一张图。

`UE.Expression(Class="Sine", …)` 是通用反射调用：按名字创建运行中引擎的任意材质表达式类。`Class` 默认取函数名，所以更短的
`UE.Sine(Input=t)` 是完全相同的调用。（自 2.0.0 起） 起 `OutputType` 不再是必需的 —— 节点目录给节点定类型 ——
`Sine`、`MaterialExpressionSine`、`UMaterialExpressionSine` 和 `/Script/Engine.MaterialExpressionSine` 都能解析。类名区分大小写；
解析不到是 `DSH5212`。

<a id="生成的材质在哪里"></a>

## 生成的材质在哪里

去 Content Browser 里找是找不到的。这不是失败。

在默认的 `ThinCustom` backend 下，一次编译产生的是活着的 `UObject`，**磁盘上没有 `.uasset`** —— 这个状态叫
**Ephemeral**：

```text
UDreamShaderMaterialInstance          "M_DreamPulse"                              <- 你引用的那个资产
  └─ 父材质：UMaterial（隐藏，transient） "MB_DreamThinBase__Game_Materials_M_DreamPulse"
       └─ 生成的节点图
```

节点图 —— 每个参数、每条语句、每个输出 —— 都构建在那个隐藏的 base 上。实例只是一层薄包装，负责携带参数值、来源元数据和
编译好的 shader map。只要材质还是 Ephemeral，它的 `IsAsset()` 就返回 `false`，因此会从 Content Browser、资产选择器和保存列表里
隐藏；它的 package 在生成结束时被标记为非 dirty，所以 *Save All* 也不会顺手把它写到磁盘。

设计意图很明确：源文件才是你维护的东西。`Backend = Graph` 的材质则是普通的 `UMaterial`，每次构建都存盘，没有 Ephemeral
状态。

有三种方式可以拿到 Ephemeral 材质。

<a id="1-material-content-browser"></a>

### 1. Material Content Browser

*Tools ▸ DreamShader ▸ Material Content Browser*。Sources 模式列出每个源码根下的每个源文件，带编译状态、诊断和生成的资产：

| 状态 | 含义 |
| --- | --- |
| `●` **up to date** | 资产存在，且它记录的构建键与当前源一致 |
| `●` **stale** | 资产存在，但记录的构建键不同 |
| `○` **not compiled** | 解析出的对象路径上什么都没有 |
| `▲` **compile error** | 编译失败，或 bridge 的诊断里有错误 |
| `◆` **function / header** | 该条目是 `.dsf` 或 `.dsh` |
| `▲` **unresolved** | 算不出源文件构建什么：它有错误，或者什么都没声明 |

选中一项后可以使用 **Compile**、**Create instance**、**Open**、**Materialize** 和 **Open source**；检查器显示它的每条诊断、
imports 和依赖方、继承链和实时预览。这是日常操作生成材质的正常入口。见[编辑器工具](../tools/editor-integration.md#material-content-browser)。

<a id="2-让它们可见"></a>

### 2. 让它们可见

*Tools ▸ DreamShader ▸ Show Ephemeral Materials*，或者 *Project Settings ▸ DreamPlugin ▸ Dream Shader ▸ Compiler ▸
Show Ephemeral Materials* 里的同一个设置。切换时会立即为每个还活着的 Ephemeral 实例广播资产创建 / 删除事件，所以图块会
立刻出现或消失，不需要重新扫描。

当你要把生成的材质选作材质实例的 Parent，或者在细节面板里引用它时，就需要打开这个开关。

**警告：**

开关打开时，Ephemeral 材质看起来就是一个普通图块 —— 而对它执行显式 **Save** 会真的写出 `.uasset`，把它 Materialize。
  磁盘上有 package 的产物之后就留在磁盘上：编译会重建它并存盘，日志里写 `'…' exists as a saved asset, so it is rebuilt and
  saved on disk rather than in memory.`。真想要文件请用 *Materialize*；想回到 Ephemeral，运行
  *Tools ▸ DreamShader ▸ Make Ephemeral*，它只删除带有 DreamShader 来源元数据的 ThinCustom 产物。

<a id="3-materialize"></a>

### 3. Materialize

**Materialize** 把材质和它隐藏的 base 作为一个 `.uasset` 写到磁盘：以 Materialized 模式加强制重建重新跑一次该源文件的生成，
然后在解析出的路径上重新加载对象。Material Content Browser、细节面板和 Content Browser 右键菜单里都有这个按钮。

```text
磁盘上   <Project>/Content/Materials/M_DreamPulse.uasset
  export  M_DreamPulse                        UDreamShaderMaterialInstance
  export  MB_DreamThinBase_M_DreamPulse       UMaterial（隐藏，同一 package 的子对象）
```

为一个 Ephemeral 材质创建子实例、或编译一个以它为父的 `.dsi` 时，会先把父级 Materialize，因为 transient 的 base 不能作为
parent import。Cook 和 commandlet 会自动落盘。完整细节见[内存材质](../generation/ephemeral.md)。

<a id="控制参数面板"></a>

## 控制参数面板

`.dss` 里，`uniform` 上方的 `///` 标签写 Unreal 参数面板里的分组、排序、说明和范围：

```hlsl
/// @group Color   @sort 10   @desc Main tint
uniform float4 Tint = float4(0.2, 0.6, 1.0, 1.0);
/// @group Color   @sort 20   @slider 0 4
uniform float Strength = 1.0;
```

1.x 里改成显式的参数节点并加上元数据块：

```c
Properties = {
    VectorParameter Tint = float4(0.2, 0.6, 1.0, 1.0) [
        Group="Color";
        SortPriority=10;
        Description="Main tint";
    ];
    ScalarParameter Strength = 1.0 [
        Group="Color";
        Slider(0, 4);
    ];
}
```

`Group`、`SortPriority` 和 `Description`（写入节点的 `Desc`）是被识别的键；其他任何键都会直接写到生成节点所属类的反射
UPROPERTY 上。`Slider(min, max)` （自 1.5.0 起） 展开成 `SliderMin` / `SliderMax`；写在向量或开关参数上是 warning
`DSH7233`，范围被丢掉。见[元数据与分组](../parameters/metadata.md)。

<a id="排查"></a>

## 排查

| 现象 | 检查 |
| --- | --- |
| Content Browser 里什么都没有 | 正常。材质是 Ephemeral 的 —— 用 Material Content Browser、*Show Ephemeral Materials*，或 *Materialize*。 |
| 完全没有生成材质 | 确认文件在 *Source Directory* 下、扩展名是 `.dss` / `.dsm`、*Auto Compile On Save* 已开启；`.dss` 要有 `export` 入口，`.dsm` 要有顶层 `Shader`。 |
| 保存 `.dsh` 什么都没生成 | 这是对的 —— header 从不直接生成资产；保存它会把每个 import 它的源文件排进编译队列。 |
| 输出是黑的 | `.dss` 要写到 `m` 的成员上；1.x 的 `Outputs` 需要有 `Base.* = …` 绑定（没有 `Outputs` 是 warning `DSH2256`）。 |
| 名字找不到（`DSH4200`） | 2.0 的名字区分大小写；1.x 源文件里只差大小写的名字仍能解析，带 warning `DSH5275`。 |
| 节点类解析不到（`DSH5210` / `DSH5212`） | 类名区分大小写，必须是运行中引擎有的类。 |
| 材质函数的调用方断线了 | 重命名输入或输出是破坏性改动 —— pin 身份是按名字恢复的。 |
| 其他情况 | 看 Output Log（`LogDreamShader`），或 Material Content Browser 里那一行的错误。每条消息都带 `DSHnnnn` 码 —— 到[错误速查](../diagnostics/index.md)里查。 |

<a id="下一步"></a>

## 下一步

- [项目结构](project-layout.md) —— 项目变大之后文件放哪里
- [日常工作流](workflow.md) —— 什么会触发重建，什么不会
- [DreamShaderLang 2.0](../language-v2/index.md) —— `.dss` 能写什么
- [常用写法](../examples/patterns.md) —— 每种构件的 `.dss` 与 1.x 写法
- [Graph 语言](../graph/index.md) —— 1.x `Graph = { … }` 里能写什么
