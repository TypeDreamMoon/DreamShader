---
title: "`dsc migrate` —— 从 1.x 源文件迁移到 `.dss`"
description: "`dsc migrate` —— 从 1.x 源文件迁移到 `.dss`"
---

<a id="dsc-migrate--1x-sources-to-dss"></a>

# `dsc migrate` —— 从 1.x 源文件迁移到 `.dss`

> [DreamShader](../index.md) » [工具](index.md) » **迁移**

`dsc migrate` 把 `.dsm`、`.dsf` 与 `.dsh` 文件改写成 DreamShaderLang 2.0。没有任何东西逼你
迁移：legacy 前端会继续用同一个编译器构建 1.x 源码。当你想要只有 `.dss` 才有的东西时再迁移 —— 真正的表达式和控制流、
`#include`、与材质并排的材质实例、能在工具处理之后存活的注释。

```powershell
./dsc.ps1 migrate DShader/Materials/M_Foo.dsm -Check    # prove the rewrite, write nothing
./dsc.ps1 migrate DShader/Materials/M_Foo.dsm           # write M_Foo.dss, move M_Foo.dsm away
./dsc.ps1 migrate -All -Check                           # every 1.x source of the writable roots
./dsc.ps1 migrate -Root MoonToon -Check                 # one source root, by its or its plugin's name
```

| 开关 | 含义 |
| :-- | :-- |
| `<file>` | 单个 1.x 源文件。它 import 的头文件只有在没有别的文件 include 时才会一起迁移。 |
| `-All` | **可写**源码根（即工程自己的）下的全部 1.x 源文件。插件的源码根不在其内。 |
| `-Root <name>` | 某一个源码根下的全部 1.x 源文件，按根的显示名或其插件名指定 |
| `-Check` | 除了写盘什么都做。退出码说明是否每个文件都能迁移 |
| `-DryRun` | 报告将会写出什么；不跳过任何证明 |
| `-Out <dir>` | 把 `.dss` 文件写到指定目录下（镜像源码树），1.x 文件原地不动 |
| `-NoBackup` | 直接删除 1.x 文件，而不是把它挪到 `Saved/DreamShader/Migrated/` |

一次迁移多个文件时，头文件先迁；只有当包含某个头文件的所有文件都在同一次运行里迁移时，这个头文件才会被迁：
`.dsh` 可以同时装两种语言，所以迁到一半的工程照样能构建。

<a id="what-is-proved-before-anything-is-written"></a>

## 落盘之前先证明什么

1. **注释一条不丢。** 拿源文件的注释逐条去对结果里的注释（`DSH9092`）。
2. **文本能按 2.0 编过。** 它由 2.0 前端解析、绑定并降级，其 `@custom` 体里的每个调用都会与
   被调用者变成的 HLSL 比对 —— 这就是着色器编译器会做的事，也是别的什么东西都不会为头文件做的事，因为头文件单独降级为什么都不是（`DSH9097`）。
   被拒绝的文本会留在 `Saved/DreamShader/Migrated/Rejected/` 下，诊断指向它。
3. **它编出同一张图。** 1.x 文件的 IR 和新文本的 IR 从根开始逐节点比较（`DSH9098`，警告，指出第一个不同的节点）。
   同一个资产写成两种样子不算差异：引脚上的字面量与其 `Const*` 孪生属性、恒等 swizzle、Custom 节点代码里的空白及其源代码标记的位置、
   IR 给 Custom 节点第一个输出起的名字、描述里的 `\r\n` 与 `\n`、按 float 精度相等的数。
4. **资产留在原处。** 如果新文件所在位置会推出另一个资产路径，导出函数会带上保住原路径的 `/// @name`
   （保不住时 `DSH9098` 会说）。

带 `#if` 行的源文件会被拒绝（`DSH9090`）：否则只有今天被选中的那个分支能活下来。

在开发工程的四个源码根上实测（71 个文件）：71 个全部能按 2.0 编过，68 个比较相等，
另外三条警告是真实差异 —— 两处 1.x 反编译器伪影，把一个节点声明得比实际更窄；一处显式的 `.x` 写进 float 引脚。

<a id="what-the-rewrite-does"></a>

## 改写做了什么

legacy 前端本来就会把每个 1.x 构造读进 2.0 树里。剩下要写出来的，是只有 **legacy 规则** 才能让它成立的东西，
因为 `.dss` 没有这些规则就绑不上。每条规则同时也是 1.x 源文件上的一条诊断，所以对 `.dsm` 跑 `dsc check`
就能看到迁移会改什么。

| 1.x | `.dss` | 规则 |
| :-- | :-- | :-- |
| `import "x.dsh";` | `#include "x.dsh"` | |
| `Shader(Name = "...") { Properties / Settings / Outputs / Graph }` | `#pragma material(...)`、带 `///` 指令的 `uniform`、一个 `export void M(inout material Base)` | |
| `ShaderFunction` 输入与输出 | 形参；名为 `Result` 的第一个输出是返回值，其余是 `out` | |
| `Function` / `GraphFunction` | `/// @custom`，函数体原样保留；GraphFunction 体里的 `UE.` 调用就地改写 | L8 |
| `Namespace(Name = "N") { Function F }` | `N_F`，并加 `/// @name N::F` 让 Custom 节点保住标题；`N::F(` 调用跟着 | |
| `Function F(in a, out r, out o)`，它什么都不返回 | `T F(a, out o)`：函数体以 `T r = (T)0;` 开头、以 `return r;` 结尾 | L9 |
| `F(a, r, o)` 在函数体或图里把这个第一个 `out` 也传出去 | `r = F(a, o)` | L9 |
| `Namespace` 块的 `Function` | 保留它的 `out` 参数 | L9 |
| 同一文件函数体里的 `x = N::F(a, o)` | `x = N_F_r(a, o)`，通过一个在 `N_F` 旁边加上的 `@custom` 函数传出它的第一个 `out` | L9 |
| `VirtualFunction` | `/// @asset` + `extern` 原型 | |
| `mix`、`fract`、`mod`、`vec3` | `lerp`、`frac`、`fmod`、`float3` | L2（`DSH5277`） |
| 函数上的 `F(a).Out`、`F(a, Output = "Out")`、`[k]` | 一条调用语句，每个输出一个局部变量，相同的调用共用 | L3b |
| `x = UE.Node()`，或把 `UE.Node()` 直接接进引脚，带多个输出 | `UE.Node().FirstOutput` | L3c（`DSH5287`） |
| 用 `R` 作为返回值的 `F(a, b, R, O)` | `R = F(a, b, O)`；未声明的接收者会被声明 | L5（`DSH5283`） |
| `opt float S;` | `float S = 0.0` | L7 |
| `SAMPLERTYPE_Normal`、`PPI_SceneColor` | `Normal`、`SceneColor` | L12（`DSH5278`） |
| 大小写不同的名字 | 声明的拼写 | L19（`DSH5275`、`DSH5276`） |
| 把 `float4` 写进 `float3` 的位置 | `.rgb` | L22（`DSH5289`） |
| 名字与它能调用的某个函数相同的块 | 该块对应的函数被 `X_Asset`，`/// @name` 保住资产 | L23（`DSH5290`） |
| `F(a, default, c)` | `F(a, C = c)` | L25 |
| `x = value;`，其中 `x` 没有任何地方声明 | `T x = value;` | L26（`DSH5292`） |
| `7 / 2`，整数之间 1.x 按浮点除的 `/` | `float(7) / 2` | L27 |
| 不带 `Destination` 的 `UE.TransformPosition(P)` | `UE.TransformPosition(P, Destination = World)`，即 1.x 写下的目标 | |
| `Description = "a\r\nb"` | doc 块的自由文本，一行一条 `///` | |
| 没有 `SortPriority` 的属性 | `/// @sort 32`，即 1.x 留给它的优先级 | |
| `Backend = "Instance"` / `""` | `ThinCustom` / `Graph` | |

注释留在它原先挨着的语句或声明旁边；1.x 块上方的注释会移到该块变成的第一个声明上方。

<a id="functions-that-return-nothing-l9"></a>

### 什么都不返回的函数（L9）

1.x 把每个什么都不返回的 `Function` 和 `GraphFunction` 的 HLSL 都写成返回它的
第一个 `out`，并先归零。正因如此，1.x 函数体才能把它当值调用 —— `fac = Checker(co);` ——
或者把这个 `out` 也传出去，`Voronoi(co, d, color);`，而 1.x 把它读作 `d = Voronoi(co, color);`。
`.dss` 函数会完全按声明原样发出，所以迁移后的函数就按那样声明：

```hlsl
/// @custom
float BLChecker_fac(float3 p)
{
    float result = (float)0;
    ...
    return result;
}
```

而每个传了这个第一个 `out` 的调用都改成给它赋值，函数体和图里都是如此。图为它生成的节点就是 1.x 生成的那个节点。

`Namespace` 块中的函数是一个库的公开入口：它的 `out` 参数就是它的 Custom 节点的引脚，也是由它生成的材质函数的输出，因此会保留这些。1.x 无法在函数体里调用它（那里不会重写 `N::F`）；当同一文件的函数体仍然要读取它的值时，调用会被转到 migrate 在旁边添加的适配器 `N_F_<first out>`。

函数的形态由声明它的文件决定，所以**要先把头文件就地迁移，再迁移包含它的文件** —— `-All` 和集合就是如此。对一个仍然是 1.x 的头文件里的 1.x 函数的调用，按该函数当前的样子写出，而 `DSH9094` 会指出哪些调用在头文件迁移后需要新形式：图中对不返回任何值的函数的调用，或函数体读取 `Namespace` 函数的值。

<a id="what-can-stop-a-file"></a>

## 什么会挡住一个文件

| | |
| :-- | :-- |
| `DSH9090` | 源文件用了 `#if` |
| `DSH9091` | `import` 在路径前面写了源码根名 |
| `DSH9097` | 迁移出的文本编不过：迁移器不处理的写法，或没有 2.0 写法的 1.x 宽容规则。改 1.x 源文件，再迁一次。 |
| `DSH9099` | `.dss` 已经存在，或文件写不进 / 挪不走。没写出 `.dss` 就绝不删 1.x 文件。 |

<a id="see-also"></a>

## 另见

- [2.0 语言](../language-v2/index.md) · [Commandlet](commandlet.md) · [反编译器](decompiler.md)
- [诊断 `DSH9090`–`DSH9099`](../diagnostics/DSH9xxx.md)
