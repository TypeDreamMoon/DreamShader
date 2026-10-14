---
title: "Backend"
description: "Backend"
---

<a id="backend"></a>

# Backend

> [DreamShader](../index.md) » [Settings](index.md) » **Backend**

`Shader` 设置，用于选择源文件如何被物化：一个可见的 `UMaterial` 节点图，
还是隐藏基础材质之上的一个轻量材质实例。

| | |
| :-- | :-- |
| 声明于 | `.dsm` —— 位于 `Shader` `Settings` 块内 |
| 类型 | 特殊设置键 |
| 生成 | `UMaterial`（`Graph`）或 `UDreamShaderMaterialInstance` + 一个隐藏的 `UMaterial` 基础材质（`ThinCustom`） |
| 起始版本 | `1.5.0`，自其当前形态起 |

<a id="synopsis"></a>

## 概要

```c
Shader(Name = "<asset-path>")
{
    Settings [=] {
        Backend = { "Graph" | "ThinCustom" | "Instance" };
    }
}
```

该值在去除首尾空白并去除引号后以大小写不敏感方式匹配；引号可选。
`Backend` 是一个特殊键：它在源文件绑定时被读取，永远不会到达材质或
反射解析器。2.0 的写法是 `#pragma material(Backend = ThinCustom)`。

<a id="accepted-values"></a>

## 可接受的取值

| 值 | 解析为 | 产出 |
| :-- | :-- | :-- |
| `Graph` | `Graph` | 在解析出的资产路径上生成一个 `UMaterial`，节点图直接建在它上面 |
| `ThinCustom` | `ThinCustom` | 在解析出的资产路径上生成一个 `UDreamShaderMaterialInstance`，其父级是名为 `MB_DreamThinBase_…` 的隐藏 `UMaterial` —— 该后缀在 Ephemeral 与 Materialized 状态下不同，见 [`ThinCustom`](#thincustom-since-150) |
| `Instance` | `ThinCustom` | 与 `ThinCustom` 完全相同，并给出一个 [`DSH7204`](../diagnostics/DSH7xxx.md#dsh7204) 警告 *（自 2.0.0 起）* |
| *（空字符串）* | — | [`DSH7201`](../diagnostics/DSH7xxx.md#dsh7201)，一个错误 *（自 2.0.0 起；1.x 将其读作 `Graph`）* |
| *（不写该键）* | 项目的 *Default Compiler Backend* | 见 [优先级](#precedence) |
| 其他任意值 | — | [`DSH7202`](../diagnostics/DSH7xxx.md#dsh7202)，一个错误 |

> [!WARNING]
> `Backend = "Instance"` **自 1.5.0 起已弃用**。它是 `ThinCustom` 的别名；旧的无图实例后端已退役，
> 运行时不再存在 `Instance` 后端。该拼写仍然能编译，并且自 2.0.0 起会以 `DSH7204` 警告说明这一点。
> 请将其替换为 `Backend = "ThinCustom";` —— 或删除该键，让项目默认值生效。

> [!NOTE]
> `Backend = "";` 是一个错误 *（自 2.0.0 起）*。1.x 将其读作 **`Graph`**，这等于用一个没人写的值
> 悄悄覆盖了项目默认值。只有*不写*该键才会回落到 *Default Compiler
> Backend*；如果你想要的就是它，请写 `Graph`。

<a id="precedence"></a>

## 优先级

| `Settings = { Backend }` | 项目 *Default Compiler Backend* | 解析出的后端 |
| :-- | :-- | :-- |
| 不写 | `ThinCustom`（出厂默认） | `ThinCustom` |
| 不写 | `Instance` | `ThinCustom` |
| 不写 | `Graph` | `Graph` |
| `"Graph"` | 任意 | `Graph` |
| `"ThinCustom"` | 任意 | `ThinCustom` |
| `"Instance"` | 任意 | `ThinCustom`（`DSH7204`） |
| `""` | 任意 | —（`DSH7201`） |

显式写的 `Backend` 永远压过项目设置。项目设置见
[项目设置](project.md#settings)。

`Backend` 在源文件绑定时解析，早于任何材质对象存在，也早于 `Settings` 的其余部分被应用。
无法识别的值是一个绑定错误，因此编译会在那里停止，该文件不会再报告构建期的设置诊断。

<a id="what-each-backend-produces"></a>

## 每种后端产出什么

<a id="graph"></a>

### `Graph`

| 方面 | 行为 |
| :-- | :-- |
| 写出的资产 | 由 `Name` 和 `Root` 推导出的包路径上的一个 `UMaterial` |
| 图 | 直接建在材质上，随后布局并重新编译 |
| 存储 | `Graph` 材质没有 Ephemeral 状态：负责写出本工程生成资产的那个编辑器每次构建都会标脏包、写入源信息并保存它。其他编辑器只在内存里构建它并清除其脏标记，因此 Save All 无法将其持久化 |
| 复用冲突 | [`DSH8201`](../diagnostics/DSH8xxx.md#dsh8201)，引用 [`DSH8102`](../diagnostics/DSH8xxx.md#dsh8102)（路径上是别的类）或 [`DSH8103`](../diagnostics/DSH8xxx.md#dsh8103)（DreamShader 没生成过的已保存资产） |

<a id="thincustom-since-150"></a>

### `ThinCustom` *（自 1.5.0 起）*

| 方面 | 行为 |
| :-- | :-- |
| 写出的资产 | 解析出的资产路径上的一个 `UDreamShaderMaterialInstance` —— `UMaterialInstanceConstant` 的子类。**这个实例才是可寻址的资产。** |
| 隐藏基础材质名，Ephemeral 状态 | `MB_DreamThinBase_<sanitized package path>` *（自 2.0.0 起）* —— 实例的整个包路径，`[A-Za-z0-9_]` 之外的字符换成 `_`，连续的 `_` 合并。包 `/Game/Docs/M_Tint` 得到 `MB_DreamThinBase__Game_Docs_M_Tint`，因此不同文件夹里同叶子名的两个实例永远不会共用基础材质 |
| 隐藏基础材质名，持久化模式 | `MB_DreamThinBase_<instance leaf name>` —— 实例自身的对象名，不含路径。`Name="Docs/M_Tint"` 得到 `MB_DreamThinBase_M_Tint` |
| 基础材质归属，Ephemeral 状态 | 归 transient 包所有，标记为 public、standalone 和 transient |
| 基础材质归属，持久化模式 | 作为实例的**子对象**，两者共享一个包、一个 `.uasset` |
| 图与设置 | 建在**基础材质**上；所有 `Settings` 键都落在那里，而不是实例上 |
| 实例接线 | 父级设为基础材质，清空参数覆盖并按名字恢复实例上设置过的覆盖，写入源路径与哈希，更新静态排列 |
| 复用冲突 | [`DSH8203`](../diagnostics/DSH8xxx.md#dsh8203)，引用 [`DSH8106`](../diagnostics/DSH8xxx.md#dsh8106)（路径上不是 DreamShader 实例材质）或 [`DSH8107`](../diagnostics/DSH8xxx.md#dsh8107)（DreamShader 没生成过的已保存资产） |

`UDreamShaderMaterialInstance` 覆盖了两个 `UObject`/`UMaterialInterface` 行为：

| 覆盖 | 规则 |
| :-- | :-- |
| `HasOverridenBaseProperties()` | 当父级是 `UMaterial` 时强制为 `true` —— 也就是隐藏基础材质之上的根实例；其他父级走原生实现。因此以 DreamShader 实例为父的子实例会**共享**根实例已编译的 shader map，而不是自己再编一份。 |
| `IsAsset()` | 当包是新建的**且** *Show Ephemeral Materials* 关闭时为 `false`。纯内存材质因此从内容浏览器、资产注册表枚举和保存选择器中消失。该设置每次调用都实时读取。 |

构建时发现本该是 Ephemeral 的资产却已保存，会记一条警告：
`'{Asset}' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory. Run
Tools > DreamShader > Make Ephemeral to make it Ephemeral again.` 见
[内存材质](../generation/ephemeral.md)。

<a id="changing-the-project-default"></a>

## 修改项目默认值

在编辑器运行时修改 *Default Compiler Backend* 会重建工程的源文件，并记录
`DreamShader setting '{Setting}' changed; regenerating all source files.`。构建键不受这次修改影响的源文件会被跳过。如果磁盘上还有持久化的生成资产，会弹出提示
指向清理操作：`{Count} 个先前生成的资产仍保存在磁盘上并遮蔽 Ephemeral 材质。运行 Tools > DreamShader > Make Ephemeral 将其删除。`

单独给某个材质切换后端会把之前的资产留在原地。接下来你就会撞上上面的复用冲突错误；删掉旧资产再重新生成。

<a id="notes"></a>

## 备注

- 后端不改变语言层面的能力。两种后端构建相同的节点图，接受完全相同的功能集；它们的区别仅在于由哪个对象承载图、哪个对象可被寻址。
- 在 `ThinCustom` 下，从生成的实例上读取 `BlendMode` 或着色模型，显示的是从隐藏基础材质**继承**来的值——设置正是写在那里的。
- `ThinCustom` 产物在日常编辑器工作中是 Ephemeral 的：它在 cook 时通过[命令行](../tools/commandlet.md)，或通过显式的 *物化* 操作写入磁盘。而 `Graph` 材质在构建时即被保存。参见[临时材质](../generation/ephemeral.md)。
- `Backend` 仅在 `Shader` 块中生效。在 `ShaderFunction` 的 `Settings` 块中，它是一条 [`DSH3263`](../diagnostics/DSH3xxx.md#dsh3263) 警告并会被忽略——参见[函数设置](function.md)。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH7201` | `Backend` 的值为空 |
| `DSH7202` | 该值不是 `Graph`、`ThinCustom` 或 `Instance` |
| `DSH7204` | 该值为 `Instance`（警告；按 `ThinCustom` 构建） |
| `DSH8201` | `Graph` 后端：材质无法创建或复用；消息引用了 `DSH8102` 或 `DSH8103` |
| `DSH8203` | `ThinCustom` 后端：实例无法创建或复用；消息引用了 `DSH8106` 或 `DSH8107` |
| [`DSH8230`](../diagnostics/DSH8xxx.md#dsh8230) | 隐藏基础材质无法创建 |
| [`DSH8229`](../diagnostics/DSH8xxx.md#dsh8229) | 资产已构建但无法保存 |

信息性输出：

| 输出 | 含义 |
| :-- | :-- |
| `Generated {Kind} {Asset} from {File}.` | 已构建一个产物；在任一后端下 `{Kind}` 对于 `Shader` 均为 `Material` |
| `Skipped {Asset} from {File}; source hash is unchanged (build key {BuildKey}).` | [源哈希缓存](../generation/caching.md)抑制了重建（[`DSH8237`](../diagnostics/DSH8xxx.md#dsh8237)，信息） |
| `'{Asset}' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory. Run Tools > DreamShader > Make Ephemeral to make it Ephemeral again.` | 该产物已物化，因此由存储决定 |

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_ThinCustom", Root="Game")
{
    Properties {
        VectorParameter Tint = float4(0.2, 0.6, 1.0, 1.0) [Group="Look"];
    }

    Settings {
        Backend      = "ThinCustom";
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
        TwoSided     = true;
    }

    Outputs {
        vec3 Color;
        Base.BaseColor = Color;
    }

    Graph {
        Color = Tint.rgb;
    }
}
```

生成的资产：

```text
package     /Game/Docs/M_ThinCustom
asset       /Game/Docs/M_ThinCustom.M_ThinCustom      UDreamShaderMaterialInstance
subobject   MB_DreamThinBase_M_ThinCustom             UMaterial  (hidden base, same package)

Settings applied to:  MB_DreamThinBase_M_ThinCustom
  BlendMode    = BLEND_Opaque
  ShadingModel = MSM_DefaultLit
  TwoSided     = true
```

该列表对应的是持久化模式。在编辑器日常的纯内存模式下，实例是同一个对象，但基础材质是 transient 包中一个单独的对象，名为 `MB_DreamThinBase__Game_Docs_M_ThinCustom`——使用的是净化后的包路径，而非叶子名。

同一个文件改用 `Backend = "Graph";` 后，会在 `/Game/Docs/M_ThinCustom` 处生成单个 `UMaterial`，由它自身承载图和设置。

<a id="see-also"></a>

## 另请参阅

- [设置](index.md)——每种块类型共用的块语法
- [着色器设置](material.md)——其他特殊键与反射解析器
- [项目设置](project.md)——*默认编译器后端*与临时材质可见性开关
- [临时材质](../generation/ephemeral.md)——纯内存生成、隐藏基础材质、物化到磁盘
- [生成管线](../generation/index.md)——后端解析在管线中的位置
- [资产路径](../generation/asset-paths.md)——`Name` 与 `Root` 如何变成包路径
- [缓存](../generation/caching.md)——源哈希跳过
- [材质实例 API](../api/material-instance.md)——C++ 中的 `UDreamShaderMaterialInstance`
- [着色器](../language/shader.md)——外层块
- [材质内容浏览器](../tools/material-browser.md)——物化并检查生成的资产
- [诊断索引](../diagnostics/index.md)——所有代码
