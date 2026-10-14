---
title: "你的第一个材质"
description: "从零开始编写一个 UI 域 Unlit 材质——分别以 .dss 和 1.x 语法各写一次——生成它，并找到编辑器刻意隐藏的结果。"
---

本页编写一个 UI 域、Unlit、带两个参数的材质，然后说明生成的材质实际上
在哪里——这是每个人第一次都会踩的坑。新源文件请写为 `.dss`；已经含有 `.dsm`
文件的项目可以继续使用 1.x 语法，同一个编译器可以构建两者。

<a id="create-the-file"></a>

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

保存它。开启 *Auto Compile On Save* 后，DreamShader 会在短暂防抖后编译该文件并生成：

```text
/Game/Materials/M_DreamPulse
```

或者跳过手动输入：*Material Content Browser ▸ New ▾ ▸ Material (.dss)* 会从模板写出相同的结构。

<a id="what-each-part-does"></a>

## 各部分的作用

| 部分 | 用途 | 参考 |
| --- | --- | --- |
| `#pragma material(…)` | 设置生成材质上的属性——域、着色模型、混合模式，以及任何 `UMaterial` 属性 | [材质设置](../settings/material.md)、[枚举值](../settings/material-enums.md) |
| `///` 行和 `uniform` | 材质参数及其元数据：分组、说明、滑条范围 | [DreamShaderLang 2.0](../language-v2/index.md#declarations) |
| `export void M_DreamPulse(inout material m)` | 入口：使该文件成为材质的那个函数。资产以它命名 | [DreamShaderLang 2.0](../language-v2/index.md#two-short-examples) |
| `m.EmissiveColor = …` | 一个材质输出，会按引擎的属性列表检查（拼错就是 `DSH5200`） | [输出绑定](../generation/outputs.md) |
| `UE.Sine(Input = t)` | 一个材质节点，按引擎类名调用；`UE.Expression(Class = "Sine", …)` 是同一路径 | [UE.Expression](../builtins/ue-expression.md) |

资产落在 `/Game` 下、文件相对于源根目录所在的文件夹中，以 `export` 函数命名。入口上方的
`/// @name` 可以另外指定名称——见[资产路径](../generation/asset-paths.md)。

<a id="the-same-material-in-1x-syntax"></a>

## 同一材质的 1.x 语法

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

| 区块 | 用途 |
| --- | --- |
| `Shader(Name="…")` | 声明要生成的资产。`Name` 是相对于根目录的资产路径；`Root` 可选，默认是 `Game`。 |
| `Properties` | 创建材质参数节点——这里是一个向量和一个标量，其元数据在 `[ … ]` 中。 |
| `Settings` | 设置生成材质上的属性。 |
| `Outputs` | 声明图输出变量，并将其中一个绑定到 `Base.EmissiveColor`。 |
| `Graph` | 创建材质节点并将最终值赋给 `Color`。 |

两个文件构建的是同一个 `/Game/Materials/M_DreamPulse`——只保留一个。[`dsc migrate`](../tools/migrate.md)
会将 1.x 文件改写为 `.dss`，并在写入前证明两者构建的是同一张图。

`UE.Expression(Class="Sine", …)` 是通用的反射调用：它按名称创建运行中引擎的任意材质表达式类。`Class` 默认是
函数名，因此更短的 `UE.Sine(Input=t)` 是完全相同的调用。（自 2.0.0 起）`OutputType` 不再是必需的——节点目录会为节点设定类型——而且 `Sine`、
`MaterialExpressionSine`、`UMaterialExpressionSine` 和 `/Script/Engine.MaterialExpressionSine` 都能解析。类名
区分大小写；无法解析的就是 `DSH5212`。

<a id="finding-the-generated-material"></a>

## 找到生成的材质

在 Content Browser 里查找是找不到的。这不是失败。

在默认的 `ThinCustom` 后端下，一次编译会生成活动的 `UObject`，并且**磁盘上没有 `.uasset`**——该状态
称为 **Ephemeral**：

```text
UDreamShaderMaterialInstance          "M_DreamPulse"                              <- the asset you reference
  └─ parent: UMaterial (hidden, transient) "MB_DreamThinBase__Game_Materials_M_DreamPulse"
       └─ the generated node graph
```

节点图——每个参数、每条语句、每个输出——都构建在那个隐藏的基础上。实例只是一个薄包装，
携带参数值、来源元数据和编译后的 shader map。当材质处于
Ephemeral 状态时，它的 `IsAsset()` 返回 `false`，因此会从 Content Browser、资产选择器和保存列表中隐藏；它的
package 在生成结束时被标记为非 dirty，所以 *Save All* 也不会把它扫到磁盘上。

这一意图是刻意的：源文件才是你要维护的东西。`Backend = Graph` 材质则是普通的
`UMaterial`，每次构建都会保存，没有 Ephemeral 状态。

有三种方式可以访问 Ephemeral 材质。

<a id="1-the-material-content-browser"></a>

### 1. Material Content Browser

*Tools ▸ DreamShader ▸ Material Content Browser*。Sources 模式列出每个源根目录下的每个源文件，附带
其编译状态、诊断信息和它构建的资产：

| 状态 | 含义 |
| --- | --- |
| `●` **up to date** | 资产存在，且其记录的构建键与当前源匹配 |
| `●` **stale** | 资产存在，但记录的构建键不同 |
| `○` **not compiled** | 解析出的对象路径上没有任何内容 |
| `▲` **compile error** | 编译失败，或 bridge 的诊断信息中含有错误 |
| `◆` **function / header** | 该条目是一个 `.dsf` 或 `.dsh` |
| `▲` **unresolved** | 无法确定源文件构建的是什么：它有错误，或者什么都没声明 |

选中一个条目后，你可以使用 **Compile**、**Create instance**、**Open**、**Materialize** 和 **Open source**；
检查器会显示每条诊断信息、导入项和依赖项、继承链以及实时预览。这是
处理生成材质的常规方式。见[编辑器工具](../../tools/editor-integration.md#material-submenu)。

<a id="2-make-them-visible"></a>

### 2. 让它们可见

*Tools ▸ DreamShader ▸ Show Ephemeral Materials*，或者 *Project Settings ▸ DreamPlugin ▸ Dream Shader ▸
Compiler ▸ Show Ephemeral Materials* 中的同一设置。切换它会立即为每个活动的
Ephemeral 实例广播资产创建 / 资产移除事件，因此图块会出现或消失，无需重新扫描。

当你把生成的材质选为材质实例的 Parent，或者从细节面板中引用它时，就需要打开此开关。

**警告：**

开启该开关后，Ephemeral 材质看起来和任何其他图块一样——对它显式执行 **Save** 确实会写入
`.uasset`，从而将其具体化。磁盘上有 package 的产物会留在磁盘上：编译会重建它并保存它，
记录 `'…' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory.`。当你想要文件时，
请使用 *Materialize*；要回到 Ephemeral，请运行 *Tools ▸ DreamShader ▸ Make Ephemeral*，它只删除带有 DreamShader 来源元数据的 ThinCustom
产物。

<a id="3-materialize-it"></a>

### 3. 具体化它

**Materialize** 会把材质及其隐藏的基础写为一个磁盘上的 `.uasset`：它会以
Materialized 和 forced 重新运行该源的生成，然后重新加载解析出的路径上的对象。该按钮位于 Material Content Browser、
细节面板和 Content Browser 右键菜单中。

```text
on disk  <Project>/Content/Materials/M_DreamPulse.uasset
  export  M_DreamPulse                        UDreamShaderMaterialInstance
  export  MB_DreamThinBase_M_DreamPulse       UMaterial (hidden, a subobject in the same package)
```

为一个 Ephemeral 材质创建子实例，或编译一个以其为父级的 `.dsi`，会先具体化父级，
因为 transient 基础无法作为父级导入。Cook 和 commandlet 会自动具体化。
完整说明见[内存中材质](../generation/ephemeral.md)。

<a id="controlling-the-parameter-panel"></a>

## 控制参数面板

在 `.dss` 中，`///` 标签写在 `uniform` 上方，用于在 Unreal 的参数
面板上写入分组、排序顺序、说明和范围：

```hlsl
/// @group Color   @sort 10   @desc Main tint
uniform float4 Tint = float4(0.2, 0.6, 1.0, 1.0);
/// @group Color   @sort 20   @slider 0 4
uniform float Strength = 1.0;
```

在 1.x 中，改用显式参数节点并添加元数据块：

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
UPROPERTY 上。`Slider(min, max)`（自 1.5.0 起）展开成
`SliderMin` / `SliderMax`；写在向量或开关参数上是 warning `DSH7233`，范围被丢掉。见
[元数据](../parameters/metadata.md)。

<a id="troubleshooting"></a>

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
| 其他情况 | 看 Output Log（`LogDreamShader`），或 Material Content Browser 里那一行的错误。每条消息都带 `DSHnnnn` 码 —— 到[诊断](../diagnostics/index.md)里查。 |

<a id="next"></a>

## 下一步

- [项目结构](project-layout.md) —— 项目变大之后文件放哪里
- [日常工作流](workflow.md) —— 什么会触发重建，什么不会
- [DreamShaderLang 2.0](../language-v2/index.md) —— `.dss` 能写什么
- [常用写法](../examples/patterns.md) —— 每种构件的 `.dss` 与 1.x 写法
- [Graph 语言](../graph/index.md) —— 1.x `Graph = { … }` 里能写什么
