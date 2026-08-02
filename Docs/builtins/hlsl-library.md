# DreamShaderBuiltins.ush

> [DreamShader](../index.md) » [Builtins](index.md) » **`DreamShaderBuiltins.ush`**

An HLSL header shipped with the plugin, defining `DS_*` macros and functions that mirror the HLSL the
material translator emits for the corresponding `UE.*` builtin nodes.

| | |
| :-- | :-- |
| Kind | shipped HLSL header |
| On disk | `<Plugin>/Shaders/DreamShaderBuiltins.ush` |
| Virtual shader path | `/Plugin/DreamShader/DreamShaderBuiltins.ush` |
| Include guard | `DREAMSHADER_BUILTINS_USH` |
| Defines | 25 `DS_*` symbols — 22 macros and 3 functions |
| Emitted by the plugin | **no** — see [Reachability](#reachability) |

## Synopsis

```hlsl
#include "/Plugin/DreamShader/DreamShaderBuiltins.ush"
```

## The virtual path

At module startup DreamShader registers a shader source directory mapping from the virtual directory
`/Plugin/DreamShader` to the plugin's `Shaders` folder, unless a mapping for that virtual directory
already exists. The mapping is unconditional — it does not depend on any project setting, backend
choice or engine version — so `/Plugin/DreamShader/DreamShaderBuiltins.ush` always resolves for the
engine's shader preprocessor.

This is a separate mapping from the one used for generated code. Per-source generated helper
includes live under the *generated shader virtual directory* and are named
`<SanitizedBaseName>_<hash>.ush`; see [Generated HLSL](../generation/generated-hlsl.md).

<a id="reachability"></a>

## Reachability

Nothing in the plugin emits an include for this header, and nothing in the plugin references any
`DS_*` symbol. The only occurrence of the name outside the file itself is a source comment.

| Path | Includes this header? |
| :-- | :-- |
| The generated per-source helper `.ush` added to a `Custom` node's include list | no — it contains only the `DreamShaderFn_*` definitions produced from `Function` blocks |
| The `Graph` backend | no — it emits material nodes, not HLSL |
| The `ThinCustom` backend | no |
| A hand-written `#include` in a [`Function`](../language/function.md) HLSL body | yes — the virtual path resolves |

The header's own comment describes it as being included by generated `DSI_*.ush` files inside the
material translation unit. That was the arrangement used by the **Instance backend**, which has been
retired: the resolved backend set is now `Graph` and `ThinCustom`, and `Backend = "Instance"` is an
alias for `ThinCustom` *(deprecated in 1.5.0)*. No `DSI_*.ush` file is produced any more, so the
comment describes a path that no longer runs. See [Backend](../settings/backend.md).

Every state read the header exposes now has a material-node equivalent in the
[`UE.*` catalogue](ue.md), and `SampleTexture2D(tex, uv)` in a `Graph` block desugars directly to a
`TextureSample` node. Reaching for this header is therefore only useful inside hand-written HLSL.

## Symbols

`Parameters` is the material parameter struct in scope at the include site; `Tex` is a texture
parameter identifier, from which the sampler name is derived by token pasting (`Tex##Sampler`).

| Symbol | Kind | Expansion | Node equivalent |
| :-- | :-- | :-- | :-- |
| `DS_TIME` | macro | `(View.GameTime)` | [`UE.Time()`](ue.md) |
| `DS_REAL_TIME` | macro | `(View.RealTime)` | — |
| `DS_DELTA_TIME` | macro | `(View.DeltaTime)` | — |
| `DS_PERIODIC_TIME(Period)` | macro | `(fmod(View.GameTime, (Period)))` | [`UE.Time(Period=…)`](ue.md) |
| `DS_TexCoord(Parameters, CoordinateIndex)` | `float2` function | `Parameters.TexCoords[CoordinateIndex].xy`, or `float2(0, 0)` when `NUM_TEX_COORD_INTERPOLATORS` is 0 | [`UE.TexCoord`](ue.md) |
| `DS_VertexColor(Parameters)` | `float4` function | `Parameters.VertexColor` | [`UE.VertexColor`](ue.md) |
| `DS_CameraVector(Parameters)` | macro | `((Parameters).CameraVector)` | [`UE.CameraVector`](ue.md) |
| `DS_ReflectionVector(Parameters)` | macro | `((Parameters).ReflectionVector)` | [`UE.ReflectionVector`](ue.md) |
| `DS_PixelNormalWS(Parameters)` | macro | `((Parameters).WorldNormal)` | [`UE.PixelNormalWS`](ue.md) |
| `DS_VertexNormalWS(Parameters)` | macro | `((Parameters).TangentToWorld[2])` | [`UE.VertexNormalWS`](ue.md) |
| `DS_TwoSidedSign(Parameters)` | macro | `((Parameters).TwoSidedSign)` | [`UE.TwoSidedSign`](ue.md) |
| `DS_ViewportUV(Parameters)` | macro | `(GetViewportUV(Parameters))` | [`UE.ViewportUV`](ue.md) |
| `DS_PixelDepth(Parameters)` | macro | `(GetPixelDepth(Parameters))` | [`UE.PixelDepth`](ue.md) |
| `DS_WorldPosition(Parameters)` | macro | `(WSDemote(GetWorldPosition(Parameters)))` | [`UE.WorldPosition`](ue.md) |
| `DS_TranslatedWorldPosition(Parameters)` | macro | `(GetTranslatedWorldPosition(Parameters))` | [`UE.TranslatedWorldPosition`](ue.md) |
| `DS_ObjectPosition(Parameters)` | macro | `(WSDemote(GetObjectWorldPosition(Parameters)))` | [`UE.ObjectPosition`](ue.md) |
| `DS_ObjectRadius(Parameters)` | macro | `(GetPrimitiveData(Parameters).ObjectRadius)` | [`UE.ObjectRadius`](ue.md) |
| `DS_ObjectBounds(Parameters)` | macro | `(float3(GetPrimitiveData(Parameters).ObjectBoundsX, …ObjectBoundsY, …ObjectBoundsZ))` | [`UE.ObjectBounds`](ue.md) |
| `DS_CameraPosition(Parameters)` | macro | `(WSDemote(GetWorldCameraOrigin(Parameters)))` | [`UE.CameraPosition`](ue.md) |
| `DS_PerInstanceRandom(Parameters)` | macro | `(GetPerInstanceRandom(Parameters))` | [`UE.PerInstanceRandom`](ue.md) |
| `DS_PerInstanceFadeAmount(Parameters)` | macro | `(GetPerInstanceFadeAmount(Parameters))` | [`UE.PerInstanceFadeAmount`](ue.md) |
| `DS_Panner(UV, Time, Speed)` | `float2` function | `UV + float2(frac(Time * Speed.x), frac(Time * Speed.y))` | [`UE.Panner`](ue.md) |
| `DS_SampleTexture2D(Tex, UV)` | macro | `Texture2DSample(Tex, Tex##Sampler, UV)` | `SampleTexture2D(tex, uv)` in a `Graph` block |
| `DS_SampleTexture2DLod(Tex, UV, Lod)` | macro | `Texture2DSampleLevel(Tex, Tex##Sampler, UV, Lod)` | — |
| `DS_SampleTexture2DBias(Tex, UV, Bias)` | macro | `Texture2DSampleBias(Tex, Tex##Sampler, UV, Bias)` | — |

The `WSDemote` calls lower the engine's large-world-coordinate vectors to `float3`. The translated
(camera-relative) position form is already `float3` and keeps precision near the camera.

The header's comment notes that the DSL used to write the unprefixed spellings
`SampleTexture2D` / `SampleTexture2DLod` / `SampleTexture2DBias` and have the generator lower them to
these macros. No such rewrite is performed today. `SampleTexture2D` is still a reserved name in a
`Graph` block, where it desugars to a `TextureSample` node — see [`UE.*` catalogue](ue.md) — but the
`Lod` and `Bias` spellings have no `Graph` equivalent.

## Notes

> [!WARNING]
> **An `#include` in a `Function` body lands inside a function.** A `Function` block's HLSL is
> emitted verbatim between the braces of the generated `DreamShaderFn_*` definition, so the
> `#include` — and therefore the header's contents — is inserted at that point. The 22 macros are
> unaffected: a `#define` is legal anywhere. The three function definitions (`DS_TexCoord`,
> `DS_VertexColor`, `DS_Panner`) are not legal inside another function body. Restrict a hand-written
> include to bodies where only the macro half is used, or copy the wanted function's body inline.

> [!WARNING]
> **`DS_TexCoord` and `DS_VertexColor` depend on translator side effects that nothing arranges any
> more.** `DS_TexCoord` reads `Parameters.TexCoords[…]`, which only exists when at least one
> interpolator slot was allocated during material translation
> (`NUM_TEX_COORD_INTERPOLATORS` > 0); with no slot allocated the function compiles and returns
> `float2(0, 0)`. `DS_VertexColor` reads `Parameters.VertexColor`, which is only fed when the
> material was translated with vertex-colour usage set. The retired Instance backend arranged both by
> compiling matching dummy input chunks. Under the `Graph` and `ThinCustom` backends nothing does, so
> use [`UE.TexCoord`](ue.md) and [`UE.VertexColor`](ue.md) — or a wired `Function` input — instead of
> these two.

- The remaining reads are side-effect free. Every field and `Get*(Parameters)` helper they touch is
  populated unconditionally at pixel entry, so they compile in any opaque Surface pixel evaluation
  with no usage flag or interpolator request.
- `DS_PerInstanceRandom` and `DS_PerInstanceFadeAmount` are meaningful only for instanced or
  GPU-culled draws. On a plain mesh they read back safe constants (`0` and `1` respectively) with no
  compile error.
- The header is guarded by `DREAMSHADER_BUILTINS_USH`, so including it more than once in the same
  translation unit is harmless.
- Inside a `Function` body the GLSL-alias pass rewrites whole identifiers before the body is emitted
  — `vec3` → `float3`, `mix` → `lerp`, `fract` → `frac`, `mod` → `fmod`, and so on. It skips string
  literals and comments, so the include path in `#include "…"` is never rewritten.

## Example

A top-level `Function` whose HLSL body uses only the macro half of the header:

```c
Function vec2 Wobble(in vec2 uv, in float speed)
{
    #include "/Plugin/DreamShader/DreamShaderBuiltins.ush"
    float t = DS_TIME * speed;
    return uv + float2(sin(t), cos(t)) * 0.02;
}
```

The include resolves through the registered virtual directory, and `DS_TIME` expands to
`(View.GameTime)`.

The node-based equivalent, which needs no include and no HLSL helper at all:

```c
GraphFunction vec2 Wobble(in vec2 uv, in float speed)
{
    float t = UE.Time() * speed;
    return uv + vec2(sin(t), cos(t)) * 0.02;
}
```

## See also

- [Builtins](index.md) — the call surfaces available inside `Graph`
- [`UE.*` catalogue](ue.md) — the node equivalent of every symbol on this page
- [`UE.Expression`](ue-expression.md) — reaching any other `UMaterialExpression`
- [Math builtins](math.md) — the unprefixed numeric call surface in `Graph`
- [Transform builtins](transform.md) — `UE.TransformVector` / `UE.TransformPosition`
- [`Function`](../language/function.md) — HLSL bodies and how they are emitted
- [`GraphFunction`](../language/graph-function.md) — the node-based alternative to an HLSL body
- [Generated HLSL](../generation/generated-hlsl.md) — the per-source `.ush` the plugin does emit
- [Backend](../settings/backend.md) — `Graph`, `ThinCustom`, and the retired `Instance` backend
- [Contributing](../contributing/index.md) — the plugin's source and content layout
