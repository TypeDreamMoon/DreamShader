---
title: 从 1.x 迁移
description: dsc migrate 把 .dsm、.dsf、.dsh 源文件改写成 .dss —— 并且在写任何东西之前先证明：注释一条不丢，生成的图一模一样。
---

`dsc migrate` 把 `.dsm`、`.dsf` 与 `.dsh` 文件改写成 DreamShaderLang 2.0。没有任何东西逼你迁移：
legacy 前端会继续用同一个编译器构建 1.x 源码，而且 `.dsh` 可以同时装两种语言，所以一个工程可以一次只搬一个文件。

```powershell
./dsc.ps1 migrate DShader/Materials/M_Foo.dsm -Check    # 只证明改写可行，什么都不写
./dsc.ps1 migrate DShader/Materials/M_Foo.dsm           # 写出 M_Foo.dss，把 M_Foo.dsm 挪走
./dsc.ps1 migrate -All -Check                           # 工程自己的源码根下全部 1.x 源文件
./dsc.ps1 migrate -Root MoonToon -Check                 # 指定一个源码根，按它的名字或所属插件名
```

| 开关 | 含义 |
| :-- | :-- |
| 一个文件 | 单个 1.x 源文件。它 import 的头文件只有在没有别的文件 include 时才会一起迁移。 |
| `-All` | **可写**源码根（即工程自己的）下的全部 1.x 源文件。插件的源码根不在其内。 |
| `-Root` | 某一个源码根下的全部 1.x 源文件，按根的显示名或其插件名指定 |
| `-Check` | 除了写盘什么都做；退出码说明是否每个文件都能迁移 |
| `-DryRun` | 报告将会写出什么 |
| `-Out` | 把 `.dss` 写到指定目录下（镜像源码树），1.x 文件原地不动 |
| `-NoBackup` | 直接删除 1.x 文件，而不是挪到 `Saved/DreamShader/Migrated/` |

开关也可以带值写 （自 2.1.1 起）：`-Check=true`、`-DryRun=1` 与 `-Check` 相同（`true` / `1` / `yes` / `on` 为开，
`false` / `0` / `no` / `off` 为关），别的值是 `DSH9110`，命令什么都不做并以 `1` 退出。见[命令行](commandlet.md)。

一次迁移多个文件时，头文件先迁；只有当包含某个头文件的所有文件都在同一次运行里迁移时，这个头文件才会被迁 ——
`.dsh` 可以同时装两种语言，所以迁到一半的工程照样能构建。

<a id="落盘之前先证明什么"></a>

## 落盘之前先证明什么

**注释一条不丢。** 拿源文件的注释逐条去对结果里的注释（`DSH9092`）。
**文本能按 2.0 编过。** 由 2.0 前端解析、绑定、降级（`DSH9097`）。被拒的文本会留在
`Saved/DreamShader/Migrated/Rejected/` 下，诊断指向它。
**编出同一张图。** 1.x 文件的 IR 和新文本的 IR 从根开始逐节点比较（`DSH9098`，警告，指出第一个不同的节点）。
同一个资产的两种写法不算差异：引脚上的字面量与它的 `Const*` 孪生属性、恒等 swizzle、Custom 节点代码里的空白、
描述里的 `
` 与 `
`、按 float 精度相等的数。
**资产留在原处。** 如果新文件所在位置会推导出另一个资产路径，导出函数会带上保住原路径的 `/// @name`（保不住时 `DSH9098` 会说）。

带 `#if` 行的源文件会被拒绝（`DSH9090`）：否则只有今天被选中的那个分支能活下来。

<a id="改写做了什么"></a>

## 改写做了什么

| 1.x | `.dss` |
| :-- | :-- |
| `import "x.dsh";` | `#include "x.dsh"` |
| 带 `Properties` / `Settings` / `Outputs` / `Graph` 的 `Shader(Name = "...")` | `#pragma material(...)`、带 `///` 指令的 `uniform`、一个 `export void M(inout material Base)` |
| `ShaderFunction` 的输入与输出 | 形参；名为 `Result` 的第一个输出是返回值，其余是 `out` |
| `Function` / `GraphFunction` | `/// @custom`，函数体原样保留；GraphFunction 体里的 `UE.` 调用就地改成 2.0 写法 |
| `Namespace(Name = "N")` 里的 `Function F` | `N_F`，并加 `/// @name N::F`，让 Custom 节点保住标题 |
| `VirtualFunction` | `/// @asset` + `extern` 原型 |
| `mix`、`fract`、`mod`、`vec3` | `lerp`、`frac`、`fmod`、`float3` |
| `F(a).Out`、`F(a, Output = "Out")`、函数调用上的 `[k]` | 一条调用语句，每个输出一个局部变量，相同的调用共用 |
| 多输出节点赋给变量，或直接接到引脚上：`x = UE.Node()` | `UE.Node().FirstOutput`，把输出名写出来（`UE.WorldPosition().XYZ`）（自 2.1.1 起） |
| `F(a, default, c)` | `F(a, C = c)` |
| `opt float S;` | `float S = 0.0` |
| `SAMPLERTYPE_Normal`、`PPI_SceneColor` | `Normal`、`SceneColor` |
| `float4` 放进 `float3` 的位置 | `.rgb` —— 1.x 悄悄截断，2.0 要求把 swizzle 写出来 |
| 没有任何地方声明过 `x` 的 `x = value;` | `T x = value;` |
| `7 / 2` —— 1.x 按浮点除的整数相除 | `float(7) / 2` （自 2.1.1 起） |
| 不带 `Destination` 的 `UE.TransformPosition(P)` | `UE.TransformPosition(P, Destination = World)` —— 1.x 写的就是世界空间 （自 2.1.1 起） |
| 没有 `SortPriority` 的属性 | `/// @sort 32`，1.x 留给它的优先级 |
| `Backend = "Instance"` / `""` | `ThinCustom` / `Graph` |

这些改写大多在 1.x 源文件上同时也是一条诊断，所以对 `.dsm` 跑一次 `dsc check` 就能看到迁移会改什么。
注释留在它原先挨着的语句或声明旁边。

**提示：**

在开发工程的四个源码根上实测 —— 71 个文件：71 个全部能按 2.0 编过，68 个比较相等，
另外三条警告是改写让它们显形的真实差异。

<a id="什么会挡住一个文件"></a>

## 什么会挡住一个文件

| 诊断码 | 原因 |
| :-- | :-- |
| `DSH9090` | 源文件用了 `#if` |
| `DSH9091` | `import` 在路径前面写了源码根名 |
| `DSH9097` | 迁移出的文本编不过：迁移器不处理的写法，或没有 2.0 写法的 1.x 宽容规则。改 1.x 源文件，再迁一次 |
| `DSH9099` | `.dss` 已经存在，或文件写不进 / 挪不走。没写出 `.dss` 就绝不删 1.x 文件 |
