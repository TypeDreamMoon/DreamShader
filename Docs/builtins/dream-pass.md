# Custom Pass material nodes

> [DreamShader](../index.md) » [Builtins](index.md) » **Custom Pass material nodes**

Two material expressions of the DreamShaderPass module, reached from a `.dss` like any other expression,
through `UE.<Name>`: one writes the data a [mesh pass](../language-v2/passes.md#mesh) draws, the other lets an
ordinary material read an [exported buffer](../runtime/index.md#exported-buffers).

| | |
| :-- | :-- |
| Classes | `UMaterialExpressionDreamPassOutput`, `UMaterialExpressionDreamPassBuffer` |
| Engine | 5.8 and later; on an older engine a call is a clear error rather than an unknown node |
| Translator | the default (legacy) material translator only: a material using either cannot set `bEnableNewHLSLGenerator` |

## `UE.DreamPassOutput`

```hlsl
// M_HighlightMask.dss -- the override material of a mesh pass
#pragma material(ShadingModel = Unlit, bUsedWithSkeletalMesh = true, bUsedWithInstancedStaticMeshes = true)

export void M_HighlightMask(inout material m)
{
    m.EmissiveColor = float3(0, 0, 0);
    UE.DreamPassOutput(Output0 = float4(1, 0, 0, 0));
}
```

A statement, not a value: the material's custom output with four pins, `Output0`…`Output3`, which a mesh pass
writes as `write Output0 = <buffer>;`. Each takes a `float` to `float4` and writes a `float4` — a `float` is
replicated, a `float2` or `float3` padded with zeros. An unconnected pin writes 0.

- It is also what makes a material **able** to be drawn by a mesh pass: the node gives the material a shader
  tag, and the mesh pass's shaders are compiled only for tagged materials — so a project's other materials
  pay nothing for Custom Pass.
- **An object's own material can carry it too.** A mesh pass with `Mode = Own` draws each selected object
  with its own material, and reads that material's `UE.DreamPassOutput` — per-object data (wetness, an id, a
  mask) without an override.
- **Top level only.** The engine compiles custom outputs from the material's own graph, not from a material
  function's. Inside a function the node does nothing — no tag, no outputs — so DreamShader warns, and the
  material editor does not offer it there.
- **One per material.**
- It works the same with Substrate on: its inputs are plain values, not a BSDF.

## `UE.DreamPassBuffer`

```hlsl
// M_Ground.dss -- a glow under the highlighted objects
export void M_Ground(inout material m)
{
    float Glow = UE.DreamPassBuffer(Pipeline = "CP_Highlight", Buffer = "Blurred").r;
    m.EmissiveColor = Glow * float3(1.0, 0.6, 0.0);
}
```

Samples an exported buffer's render target. `float4`.

| Argument | |
| :-- | :-- |
| `Pipeline` | the pipeline: a bare name (the product of a `.dsp` under the same source root) or an object path |
| `Buffer` | a buffer of it with `Export = true` |
| `Coordinates` | optional; the viewport UV when left out, which is what a screen-space buffer wants. A buffer laid out some other way (the wind field of the examples, tiled over world XY) passes its own UV |
| `SamplerSource` | optional; `FromTextureAsset` by default — the render target's own sampler, which the compiler sets to Clamp for a buffer sized from the view and to Wrap for a fixed-size one. `Wrap_WorldGroupSettings` or `Clamp_WorldGroupSettings` use a shared sampler instead |

- In a vertex shader (World Position Offset) it samples mip 0.
- A missing pipeline, a buffer the pipeline does not have or does not export, a built-in buffer, and an
  integer or depth buffer are errors — in DreamShader when the `.dss` compiles, and in the material editor.
- What it reads depends on when the buffer's last writer runs — this frame's picture or last frame's; see the
  table in [Exported buffers](../runtime/index.md#exported-buffers).
- The material depends on the `.dsp`: changing the buffer's export recompiles it.
- The material editor's preview shows the render target's last contents.

## Reading a pass's buffers in a pass material

A material run by a `fullscreen` pass reads the pass's buffers with the engine's own
`UE.UserSceneTexture(UserSceneTexture = "<name>", Coordinates = UV)` node, whose name the pass binds with
`read <name> = <buffer>;` — no Custom Pass node is involved, and the material previews as any post-process
material does.

## See also

- [Custom Pass pipelines — `.dsp`](../language-v2/passes.md)
- [Custom Pass runtime](../runtime/index.md)
- [UE.Expression](ue-expression.md) — how reflected expressions are called
