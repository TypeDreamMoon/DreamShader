---
title: Backend
description: Graph 与 ThinCustom 的区别 —— 谁承载图、谁是可寻址的资产，以及按文件的设置如何与项目默认值一起解析。
---

`Backend` 决定一个源文件如何被**物化**：变成可见的 `UMaterial` 节点图，还是变成隐藏基础材质之上的一个轻量材质实例。

```c
Shader(Name = "<asset-path>")
{
    Settings [=] {
        Backend = { "Graph" | "ThinCustom" | "Instance" };
    }
}
```

值在 trim 并去引号后大小写不敏感匹配；引号可选。`Backend` 是[特殊键](material.md#特殊键)：绑定源码时读取，
永远到不了材质或反射解析器。2.0 写法是 `#pragma material(Backend = ThinCustom)`。它当前的形态自 （自 1.5.0 起） 起确立。

<a id="可接受的取值"></a>

## 可接受的取值

| 值 | 解析为 | 产出 |
| :-- | :-- | :-- |
| `Graph` | `Graph` | 在解析出的资产路径上生成一个 `UMaterial`，节点图直接建在它上面 |
| `ThinCustom` | `ThinCustom` | 在解析出的资产路径上生成一个 `UDreamShaderMaterialInstance`，其父级是名为 `MB_DreamThinBase_…` 的隐藏 `UMaterial` |
| `Instance` | `ThinCustom` | 与 `ThinCustom` 完全相同，给出警告 `DSH7204` （自 2.0.0 起） |
| *（空字符串）* | —— | 错误 `DSH7201` （自 2.0.0 起）（1.x 读作 `Graph`） |
| *（不写该键）* | 项目的 *Default Compiler Backend* | 见[优先级](#优先级) |
| 其他任意值 | —— | 错误 `DSH7202` |

**自 1.5.0 起弃用。**请改用 `ThinCustom`。

`Backend = "Instance"` 是 `ThinCustom` 的别名；旧的无图实例后端已经退役，运行时不再存在 `Instance` 后端。这个拼写仍能
编译，（自 2.0.0 起） 起会以警告 `DSH7204` 说明。请改写成 `Backend = "ThinCustom";`，或者删掉这个键、让项目默认值生效。

**提示：**

`Backend = "";` 是错误 （自 2.0.0 起）。1.x 把它读作 **`Graph`**，等于用一个没人写的值悄悄覆盖了项目默认值。只有*不写*
这个键才会回落到 *Default Compiler Backend*；想要 `Graph` 就写 `Graph`。

<a id="优先级"></a>

## 优先级

| `Settings = { Backend }` | 项目 *Default Compiler Backend* | 解析出的后端 |
| :-- | :-- | :-- |
| 不写 | `ThinCustom`（出厂默认） | `ThinCustom` |
| 不写 | `Instance` | `ThinCustom` |
| 不写 | `Graph` | `Graph` |
| `"Graph"` | 任意 | `Graph` |
| `"ThinCustom"` | 任意 | `ThinCustom` |
| `"Instance"` | 任意 | `ThinCustom`（`DSH7204`） |
| `""` | 任意 | ——（`DSH7201`） |

显式写的 `Backend` 永远压过项目设置；项目设置见[项目设置](project.md)。

`Backend` 在绑定源码时解析，早于任何材质对象存在，也早于 `Settings` 其余部分被应用。无法识别的值是绑定错误，编译在那里
停下，该文件不会再报构建期的设置诊断。

<a id="每种后端产出什么"></a>

## 每种后端产出什么

<a id="graph"></a>

### `Graph`

| 方面 | 行为 |
| :-- | :-- |
| 写出的资产 | 由 `Name` 和 `Root` 推导出的包路径上的一个 `UMaterial` |
| 图 | 直接建在材质上，随后布局并重新编译 |
| 存储 | `Graph` 材质没有 Ephemeral 状态：负责写出本工程生成资产的那个编辑器每次构建都会标脏、写入源信息并保存包。其他编辑器只在内存里构建它并清掉脏标记，所以 Save All 不会把它持久化 |
| 复用冲突 | `DSH8201`，引用 `DSH8102`（路径上是别的类）或 `DSH8103`（DreamShader 没生成过的已保存资产） |

<a id="thincustom--默认"></a>

### `ThinCustom` —— 默认

（自 1.5.0 起）

| 方面 | 行为 |
| :-- | :-- |
| 写出的资产 | 解析出的资产路径上的一个 `UDreamShaderMaterialInstance`（`UMaterialInstanceConstant` 的子类）。**这个实例才是可寻址的资产。** |
| 隐藏基础材质名（Ephemeral 状态） | `MB_DreamThinBase_<净化后的包路径>` （自 2.0.0 起） —— 实例的整个包路径，`[A-Za-z0-9_]` 之外的字符换成 `_` 并合并连续的 `_`。包 `/Game/Docs/M_Tint` 得到 `MB_DreamThinBase__Game_Docs_M_Tint`，所以不同文件夹里同名的两个实例永远不会共用基础材质 |
| 隐藏基础材质名（持久化模式） | `MB_DreamThinBase_<实例叶子名>` —— 实例自身的对象名，不含路径。`Name="Docs/M_Tint"` 得到 `MB_DreamThinBase_M_Tint` |
| 基础材质归属（Ephemeral 状态） | 归 transient 包所有，标记为 public、standalone、transient |
| 基础材质归属（持久化模式） | 作为实例的**子对象**，两者共享一个包、一个 `.uasset` |
| 图与设置 | 建在**基础材质**上；所有 `Settings` 键都落在那里，而不是实例上 |
| 实例接线 | 父级指向基础材质，清空参数覆盖、按名字恢复实例上设置过的覆盖，写入源路径与哈希，更新静态排列 |
| 复用冲突 | `DSH8203`，引用 `DSH8106`（路径上不是 DreamShader 实例材质）或 `DSH8107`（DreamShader 没生成过的已保存资产） |

`UDreamShaderMaterialInstance` 覆盖了两个引擎行为：

| 覆盖 | 规则 |
| :-- | :-- |
| `HasOverridenBaseProperties()` | 当父级是 `UMaterial` 时强制为 `true` —— 也就是隐藏基础材质之上的根实例。其他父级走原生实现，因此以 DreamShader 实例为父的子实例会**共享**根实例已编译的 shader map，而不是自己再编一份。 |
| `IsAsset()` | 当包是新建的**且** *Show Ephemeral Materials* 关闭时返回 `false`。纯内存材质因此从内容浏览器、资产注册表枚举和保存选择器中消失。该设置每次调用都实时读取。 |

构建时发现本该是 Ephemeral 的资产已经保存在磁盘上，会记一条警告：
`'{Asset}' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory. Run Tools > DreamShader > Make Ephemeral to make it Ephemeral again.`
参见[内存材质](../generation/ephemeral.md)。

<a id="修改项目默认值"></a>

## 修改项目默认值

在编辑器运行时修改 *Default Compiler Backend* 会重建工程的源文件，并记录
`DreamShader setting '{Setting}' changed; regenerating all source files.` 构建键不受这次修改影响的源文件会被跳过。如果磁盘上
还有持久化的生成资产，会弹出提示指向清理操作：
`{Count} previously generated asset(s) are still saved on disk and shadow the Ephemeral materials. Run Tools > DreamShader > Make Ephemeral to remove them.`

**警告：**

单独给某个材质切换后端会把之前的资产留在原地，下一次编译就会撞上上面的复用冲突错误。先删掉旧资产，再重新生成。

<a id="备注"></a>

## 备注

- 后端不改变语言层面的能力。两者都构建同一张节点图，接受完全相同的特性集合；区别只在于谁承载图、谁是可寻址的对象。
- 在 `ThinCustom` 下，从生成的实例上读 `BlendMode` 或 shading model 看到的是从隐藏基础材质**继承**来的值 —— 设置是写在那里的。
- `ThinCustom` 产物在日常编辑器工作中是 Ephemeral 的：它在 cook 时、通过[命令行](../tools/commandlet.md)、或通过显式的
  *Materialize* 操作落盘。`Graph` 材质在构建时就被保存。见[内存材质](../generation/ephemeral.md)。
- `Backend` 只在 `Shader` 块里生效。在 `ShaderFunction` 的 `Settings` 块里它是警告 `DSH3263`，被忽略 ——
  见[材质函数的 Settings](material.md#材质函数的-settings)。

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH7201: 'Backend' has no value; write 'Backend = Graph' or 'Backend = ThinCustom'. An empty value meant Graph in 1.x and means nothing now.` | Backend 的值为空。 | 写 Graph 或 ThinCustom，或删掉该键以使用项目默认值。 |
| `DSH7202: 'Backend = {0}' is not a backend; the backends are 'Graph' and 'ThinCustom'.` | 值不是 Graph、ThinCustom 或 Instance。 | 改正拼写，或删掉该键以使用项目默认值。 |
| `DSH7204: 'Backend = Instance' is the old spelling of 'Backend = ThinCustom'; write the new one.` | 警告：值是 Instance，按 ThinCustom 构建。 |  |
| `DSH8201: The material for '{0}' could not be created or reused. {1}` | Graph 后端：材质建不出来也复用不了；消息引用 DSH8102 或 DSH8103。 | 删掉旧资产再重新生成。 |
| `DSH8203: The ThinCustom instance for '{0}' could not be created or reused. {1}` | ThinCustom 后端：实例建不出来也复用不了；消息引用 DSH8106 或 DSH8107。 | 删掉旧资产再重新生成。 |
| `DSH8230: Cannot create a ThinCustom base material without an instance.` | 隐藏基础材质创建失败。 |  |
| `DSH8229: '{0}' was built but could not be saved. {1}` | 资产已构建但保存失败。 |  |

日志里值得认识的输出：

| 信息 | 含义 |
| :-- | :-- |
| `Generated {Kind} {Asset} from {File}.` | 产物已构建；`Shader` 不论哪种后端 `{Kind}` 都是 `Material` |
| `Skipped {Asset} from {File}; source hash is unchanged (build key {BuildKey}).` | [源哈希缓存](../generation/regeneration.md)跳过了这次重建（`DSH8237`，提示） |
| `'{Asset}' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory. …` | 产物已落盘（Materialized），由存储状态决定 |

<a id="示例"></a>

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

持久化模式下生成的资产：

```text
package     /Game/Docs/M_ThinCustom
asset       /Game/Docs/M_ThinCustom.M_ThinCustom      UDreamShaderMaterialInstance
subobject   MB_DreamThinBase_M_ThinCustom             UMaterial  (hidden base, same package)

Settings applied to:  MB_DreamThinBase_M_ThinCustom
  BlendMode    = BLEND_Opaque
  ShadingModel = MSM_DefaultLit
  TwoSided     = true
```

在编辑器日常的 Ephemeral 状态下，实例还是同一个对象，但基础材质是 transient 包里一个单独的对象，名为
`MB_DreamThinBase__Game_Docs_M_ThinCustom` —— 用的是净化后的包路径，不是叶子名。同一个文件把 `Backend = "Graph";` 换上，则在
`/Game/Docs/M_ThinCustom` 生成单个 `UMaterial`，自己承载图和设置。

<a id="下一步"></a>

## 下一步

- [项目设置](project.md) —— *Default Compiler Backend* 与 Ephemeral 材质可见性开关
- [内存材质](../generation/ephemeral.md) —— 纯内存生成、隐藏基础材质，以及如何落盘
- [生成流程](../generation/index.md) —— 后端解析在流程中的位置
- [资产路径](../generation/asset-paths.md) —— `Name` 和 `Root` 如何变成包路径
