---
title: "NoGeneration"
description: "NoGeneration"
---

<a id="nogeneration"></a>

# NoGeneration

> [DreamShader](../index.md) » [Generation](index.md) » **NoGeneration**

*（2.1.2 之后）* 一个源，或它的某个导出，只有**命名**它的编译才会构建它。其他所有编译都会让其资产保持磁盘上的原样，任何使用该资产的东西也按原样使用它。

| | |
| :-- | :-- |
| 写法 | `#pragma nogeneration`（整个文件）· 在 `export` 上写 `/// @nogeneration`（该资产）· 在 1.x 块的头部写 `NoGeneration = true`（该块的资产）|
| 适用于 | 源构建的每个资产：材质、实例（`.dsi`）、材质函数、图层与混合、Custom Pass 管线（`.dsp`）|
| 由谁构建 | [材质内容浏览器](../../tools/material-browser.md)中的*编译*、*还原为源*、创建材质的实例、针对该文件的桥接请求、[`compile -Source`](../tools/commandlet.md)、`check -Source -Shaders` |
| 不受影响 | 编辑器启动时的扫描、保存、它所依赖的源的重新构建（一个头部、某个 `.dsi` 的父级、某个 `.dsp` 的材质）、*全部重新编译*、`compile -All`、`check -All -Shaders`、cook |

用于重建代价高昂的资产，或由某人在编辑器中手动调整、不应在共享头部每次更改时被重建的资产。

<a id="writing-it"></a>

## 编写它

```hlsl
// M_Ocean.dss -- none of its assets is rebuilt unless the file is compiled by name.
#pragma nogeneration

export void M_Ocean(inout material m) { … }
```

```hlsl
// Only MF_Heavy is left alone; MF_Light is built as usual.
/// @nogeneration
export float3 MF_Heavy(float3 p) { … }

export float3 MF_Light(float3 p) { return MF_Heavy(p) * 0.5; }
```

```c
// 1.x: on any block that builds an asset -- Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend.
ShaderFunction(Name = "Noise/MF_Heavy", NoGeneration = true)
{
    …
}
```

1.x 文件也可以在其顶层为文件的每个块写入 `#pragma nogeneration`。[`dsc migrate`](../tools/migrate.md) 会把 `NoGeneration = true` 写为 `/// @nogeneration`。

| 错误 | 代码 |
| :-- | :-- |
| `NoGeneration =` 写成了 `true`、`false`、`1` 或 `0` 之外的值 | [`DSH2260`](../diagnostics/DSH2xxx.md#dsh2260) — 该块会像 `false` 那样被构建 |
| `#pragma nogeneration(…)` | [`DSH3219`](../diagnostics/DSH3xxx.md#dsh3219) — 参数会被忽略，文件**仍会**保持标记 |
| 在头部中写 `#pragma nogeneration` | [`DSH7235`](../diagnostics/DSH7xxx.md#dsh7235) — 被忽略；包含它的文件不会被标记 |
| 在辅助项、某个 `@custom` 函数或某个 `extern` 上写 `/// @nogeneration` | [`DSH7224`](../diagnostics/DSH7xxx.md#dsh7224) — 它标记的是资产，而只有 `export` 才有资产 |

<a id="what-a-compile-that-does-not-name-it-does"></a>

## 未命名它的编译会做什么

源仍会被读取、绑定和检查，因此保存仍会报告其错误。然后，对于每个 NoGeneration 资产：

| 该资产 | 会发生什么 | 报告为 |
| :-- | :-- | :-- |
| 已存在 | 保持原样，即使源自其构建以来已经更改 | info [`DSH8277`](../diagnostics/DSH8xxx.md#dsh8277)；结果行 `Skipped <path> from <source>; the source marks it NoGeneration, …` |
| 从未被构建过 | 不写入任何内容 | warning [`DSH8278`](../diagnostics/DSH8xxx.md#dsh8278)；`Skipped <path> from <source>; … it has never been built.` |

文件的其他资产会照常构建。调用同一文件的 NoGeneration 导出的资产会调用磁盘上的资产；当没有时，会以
[`DSH8279`](../diagnostics/DSH8xxx.md#dsh8279) 失败。

<a id="what-uses-it"></a>

## 使用它的东西

没有任何东西会替它先编译一个 NoGeneration 源：

| 引用 | 磁盘上存在该资产时 | 不存在时 |
| :-- | :-- | :-- |
| 某个 `.dsi`，其 `Parent` 由它构建 | 该实例会基于该资产构建 — 其参数从该资产读取，而非从源读取 | [`DSH8279`](../diagnostics/DSH8xxx.md#dsh8279) |
| 某个 `.dsp` pass，其 `Material` 由它构建 | 基于该资产进行检查 | `DSH8279` |
| 某个 `.dss`，它读取由它构建的某个 `.dsp` 的缓冲区 | 基于该管线进行检查 | `DSH8279` |
| 某个 `extern` 函数，或某个命名该资产的 1.x `MaterialFunctionCall` | 该资产，与任何资产一样 | 发射器的 [`DSH8219`](../diagnostics/DSH8xxx.md#dsh8219) |

`DSH8279` 要求你按名称编译该源一次；从那时起，其使用者会基于那次编译所写的内容进行构建。

<a id="hand-edits"></a>

## 手动编辑

NoGeneration 资产绝不会在你背后被重建，因此对它的手动编辑会在每次不命名该源的编译中保留下来。而命名它的编译是一次普通重建，[分歧](divergence.md)检查仍会阻止它在未经询问的情况下覆盖手动编辑。

<a id="see-also"></a>

## 另见

- [缓存](caching.md) — 编译让资产保持原样的另一个原因：其构建键是当前的
- [重新生成](regeneration.md) — 重建会对资产做什么
- [禁用诊断](../language-v2/disable-diagnostics.md) — 对于一个从不打算自动构建的文件，可以禁用 `DSH8278`
