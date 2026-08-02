# Type tokens

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Type tokens**

The closed set of identifiers that name a value's shape in a declaration, and the per-context rules
that decide which of them a given declaration position accepts.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` — wherever a declaration is legal |
| Kind | lexical category |
| Matching | **case-insensitive** in every context |

## Synopsis

A type token is the first element of every declaration form in the language:

```c
// Properties section
[const] <type> <name> [ = <default> ] [ [ <metadata> ] ] ;

// Inputs / Outputs / Results of a material function
[opt] <type> <name> [ = <default> ] [ [ <metadata> ] ] ;

// Shader Outputs variable declaration
<type> <name> [ = <expression> ] ;

// Function / GraphFunction signature
Function [ <type> ] <name> ( [ { in | out } ] <type> <name> , … ) { … }

// Graph declaration
<type> <name> [ = { <expression> | { <brace-initializer> } } ] ;
```

## Contexts

The six declaration positions do **not** share one type set. Each row is a column of the
[validity matrix](#validity-matrix).

| Column | Position | Reference |
| :-- | :-- | :-- |
| `Prop` | `Properties` of `Shader` / `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` | [Properties](properties.md) |
| `I/O` | `Inputs` / `Outputs` / `Results` of `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` / `VirtualFunction` (and `Properties` inside a `VirtualFunction`, where it aliases `Inputs`) | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Fn in` | an `in` parameter of `Function` / `GraphFunction` | [Function](function.md), [GraphFunction](graph-function.md) |
| `Fn out` | an `out` parameter, or the declared return type, of `Function` / `GraphFunction` | [Function](function.md) |
| `Out decl` | a variable declaration inside a `Shader`'s `Outputs` section | [Output bindings](output-bindings.md) |
| `Graph` | a declaration statement inside a `Graph` block | [Graph declarations](../graph/declarations.md) |

> [!NOTE]
> `Prop` is validated at parse time; every other column is validated at **generation** time, when the
> declaration is first used. A `Function` whose parameter carries an unaccepted token parses cleanly
> and only fails when something calls it.

## Validity matrix

`✔` accepted · `✘` rejected. Component count is the value's width as the graph builder sees it;
`0` means an opaque value (texture object, `MaterialAttributes`, `Substrate`) that carries no channels.

| Token | Comp. | `Prop` | `I/O` | `Fn in` | `Fn out` | `Out decl` | `Graph` |
| :-- | --: | :-: | :-: | :-: | :-: | :-: | :-: |
| `float` | 1 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `float1` | 1 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `half` | 1 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `half1` | 1 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `int` | 1 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `uint` | 1 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `bool` | 1 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `float2` | 2 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `half2` | 2 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `vec2` | 2 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `int2` | 2 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `uint2` | 2 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `bool2` | 2 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `ivec2` | 2 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `uvec2` | 2 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `bvec2` | 2 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `float3` | 3 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `half3` | 3 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `vec3` | 3 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `int3` | 3 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `uint3` | 3 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `bool3` | 3 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `ivec3` | 3 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `uvec3` | 3 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `bvec3` | 3 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `float4` | 4 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `half4` | 4 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `vec4` | 4 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `int4` | 4 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `uint4` | 4 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `bool4` | 4 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `ivec4` | 4 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `uvec4` | 4 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `bvec4` | 4 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `Texture2D` | 0 | ✔ | ✔ | ✔ ⁽¹⁾ | ✘ | ✘ | ✔ ⁽²⁾ |
| `TextureCube` | 0 | ✔ | ✔ | ✔ ⁽¹⁾ | ✘ | ✘ | ✔ ⁽²⁾ |
| `Texture2DArray` | 0 | ✔ | ✔ | ✔ ⁽¹⁾ | ✘ | ✘ | ✔ ⁽²⁾ |
| `Texture3D` | 0 | ✔ | ✔ | ✔ ⁽¹⁾ | ✘ | ✘ | ✔ ⁽²⁾ |
| `VolumeTexture` | 0 | ✔ | ✔ | ✔ ⁽¹⁾ | ✘ | ✘ | ✔ ⁽²⁾ |
| `SamplerState` | 0 | ✘ | ✔ ⁽³⁾ | ✔ ⁽³⁾⁽⁴⁾ | ✘ | ✘ | ✔ ⁽²⁾⁽³⁾ |
| `MaterialAttributes` | 0 | ✘ | ✔ | ✔ ⁽⁵⁾ | ✔ | ✔ | ✔ |
| `Substrate` | 0 | ✘ | ✔ ⁽⁶⁾ | ✘ ⁽⁷⁾ | ✘ ⁽⁷⁾ | ✔ ⁽⁶⁾ | ✔ ⁽²⁾⁽⁶⁾ |
| `StaticBool` | 1 | ✘ | ✔ | ✔ ⁽⁸⁾ | ✘ | ✘ | ✔ ⁽⁸⁾ |
| `StaticBoolParameter` | 1 | ✔ ⁽⁹⁾ | ✔ | ✔ ⁽⁸⁾ | ✘ | ✘ | ✔ ⁽⁸⁾ |

1. A texture-typed `in` parameter of a `Function` also emits a companion `SamplerState <Name>Sampler`
   parameter into the generated HLSL signature, and every call site inserts `<argument>Sampler`
   after the argument. See [Function](function.md).
2. Texture, `SamplerState` and `Substrate` declarations in a `Graph` block **must** carry an
   initializer — there is no default value for them.
   Error: `Graph variable type '{Type}' requires an explicit initializer.`
3. `SamplerState` is an alias of `Texture2D` in every context that accepts it: it resolves to a
   `Texture2D` texture object, not to a distinct sampler value.
4. `SamplerState` is **not** in the set that triggers the companion-sampler expansion of note 1, so a
   `SamplerState` `in` parameter emits `SamplerState <Name>` into the generated HLSL and receives a
   texture object at the call site.
5. `MaterialAttributes` resolves as a `Function` input type, but the token is emitted verbatim into
   the generated HLSL signature — see the warning under [Generated HLSL spelling](#generated-hlsl-spelling).
6. `Substrate` requires **UE 5.4 or newer**. On an older engine the token does not resolve at all and
   the position-specific "requires Unreal Engine 5.4 or newer" diagnostic is emitted.
7. `Substrate` resolves as a `Function` / `GraphFunction` parameter or result type and is then
   rejected with a dedicated message. The two forms word it differently — see
   [Diagnostics](#diagnostics).
8. *(unreleased)* — `StaticBool` resolving as a one-component type at call sites, and the
   `true` / `false` literals that feed it, are post-1.5.0 behaviour.
9. In `Properties`, `StaticBoolParameter` is a **parameter-node token**, not a type token: it
   generates a `UMaterialExpressionStaticBoolParameter` and accepts only `true` / `false` as its
   default. See [Parameter nodes](../parameters/parameter-nodes.md).

### Counts at a glance

| Family | Tokens | Count |
| :-- | :-- | --: |
| Scalar (1 component) | `float` `float1` `half` `half1` `int` `uint` `bool` | 7 |
| Vector (2 / 3 / 4 components) | `float2..4` `half2..4` `vec2..4` `int2..4` `uint2..4` `bool2..4` `ivec2..4` `uvec2..4` `bvec2..4` | 27 |
| Texture | `Texture2D` `TextureCube` `Texture2DArray` `Texture3D` `VolumeTexture` | 5 |
| Opaque / other | `SamplerState` `MaterialAttributes` `Substrate` `StaticBool` `StaticBoolParameter` | 5 |

> [!NOTE]
> There is no `vec1`, `ivec1`, `uvec1` or `bvec1`, and no `half` GLSL vector spelling. The
> single-component GLSL-style forms simply do not exist; use `float` or `float1`.

## GLSL aliases

`vec` / `ivec` / `uvec` / `bvec` spellings are first-class tokens everywhere the corresponding
`float` / `int` / `uint` / `bool` spelling is accepted — they are not rewritten in `Properties`,
`Inputs`, `Outputs`, `Graph` declarations or `Graph` constructors.

| GLSL spelling | Equivalent |
| :-- | :-- |
| `vec2` `vec3` `vec4` | `float2` `float3` `float4` |
| `ivec2` `ivec3` `ivec4` | `int2` `int3` `int4` |
| `uvec2` `uvec3` `uvec4` | `uint2` `uint3` `uint4` |
| `bvec2` `bvec3` `bvec4` | `bool2` `bool3` `bool4` |

Because `int`, `uint`, `bool` and `half` all collapse to the same float component counts, the choice
between `int3`, `ivec3`, `bool3` and `float3` is documentation only. The single observable difference
is the integer marker set by an integer **constructor call** in a `Graph` block, which exists solely
to reject integer division — see [Constructors](../graph/constructors.md) and
[Conversions](../graph/conversions.md).

### GLSL identifier rewrites in `Function` bodies

A `Function` / `GraphFunction` **signature** runs every type token through a normalizer, and the
**body** runs every identifier through a superset of the same map. Both are matched on the
lower-cased whole identifier, so the rewrite is case-insensitive.

| Rewritten identifier | Becomes | In signature types | In body text |
| :-- | :-- | :-: | :-: |
| `vec2` `vec3` `vec4` | `float2` `float3` `float4` | ✔ | ✔ |
| `ivec2` `ivec3` `ivec4` | `int2` `int3` `int4` | ✔ | ✔ |
| `uvec2` `uvec3` `uvec4` | `uint2` `uint3` `uint4` | ✔ | ✔ |
| `bvec2` `bvec3` `bvec4` | `bool2` `bool3` `bool4` | ✔ | ✔ |
| `mat2` | `float2x2` | ✔ | ✔ |
| `mat3` | `float3x3` | ✔ | ✔ |
| `mat4` | `float4x4` | ✔ | ✔ |
| `mix` | `lerp` | ✘ | ✔ |
| `fract` | `frac` | ✘ | ✔ |
| `mod` | `fmod` | ✘ | ✔ |

15 signature aliases; 18 body aliases. The rewrite is comment- and string-aware. It is applied to
`Function` and `GraphFunction` bodies only — a `Graph` block, a `Shader` body and a `ShaderFunction`
body are **not** normalized. In a `Graph` block, `fract` and `mod` are instead served by real math
builtins *(since 1.5.0)*; `mix` is a real builtin alias of `lerp`. See
[Math builtins](../builtins/math.md).

> [!WARNING]
> The body rewrite matches the whole identifier, ignoring case. A helper, local variable or struct
> member named `Mix`, `Mod`, `Fract`, `Vec3`, `Mat4` … inside a `Function` or `GraphFunction` body is
> silently renamed to `lerp`, `fmod`, `frac`, `float3`, `float4x4`. There is no diagnostic; the
> failure surfaces as an HLSL compile error, or as silently different math. Rename the identifier.

## Matrices

There are **no matrix types**. `mat2` / `mat3` / `mat4` and `float2x2` / `float3x3` / `float4x4` are
rejected by every column of the [validity matrix](#validity-matrix).

`mat2` / `mat3` / `mat4` are nevertheless accepted *lexically* in a `Function` signature, because the
signature normalizer rewrites them before any validation runs. The declaration therefore parses, and
the call fails later:

```c
Function float3 Rotate(in mat3 basis, in vec3 v) { return mul(basis, v); }
// parses; a call fails with:
//   DreamShader Function 'Rotate' input 'basis' uses unsupported type 'float3x3'.
```

Matrix-shaped work is reachable only through the transform builtins — see
[UE.TransformVector / UE.TransformPosition](../builtins/transform.md).

## Whitespace inside a token

The scalar/vector resolver used by `Fn in`, `Fn out`, `Out decl` and `Graph` lower-cases the token and
then removes **every** space before comparing, and the `MaterialAttributes` / `Substrate` predicates
do the same. The texture, `SamplerState` and `StaticBool` tokens are compared verbatim (ignoring case
only), so `Texture 2D` does **not** resolve.

| Written | Resolves as |
| :-- | :-- |
| `float 3` | `float3` |
| `Material Attributes` | `MaterialAttributes` |
| `Sub strate` | `Substrate` |

The `Properties` dispatcher does **not** do this — it compares the token verbatim (ignoring case
only), so `float 3 X;` in a `Properties` block splits as type `float` / name `3 X` and fails
differently. See [Properties](properties.md).

## Generated HLSL spelling

When a `Function` is lowered to a generated `.ush` helper, each declared type token is written into
the HLSL signature. Exactly one rewrite is applied at that point:

| Declared token | HLSL emitted |
| :-- | :-- |
| `VolumeTexture` | `Texture3D` |
| everything else | the token, verbatim |

> [!WARNING]
> Because the token is emitted verbatim, a token that resolves for DreamShader but has no HLSL
> spelling — `MaterialAttributes`, `StaticBool`, `StaticBoolParameter` — produces a generated helper
> that Unreal cannot compile. The DreamShader-side validation passes and the failure appears as a
> shader-compile error naming the unknown type. Use a `ShaderFunction` or a `GraphFunction` for
> `MaterialAttributes` and static-bool plumbing.

## Component-count derivation

| Context | Rule |
| :-- | :-- |
| `Properties` vector tokens | the **last character** of the token: `…2` → 2, `…4` → 4, anything else → 3 |
| `Graph` constructors | the **last character** of the name: `…2` → 2, `…3` → 3, `…4` → 4, anything else → 1 |
| every other context | a fixed table keyed on the whole token |

This is why `float1` and `half1` are one component (they end in `1`, which is neither `2`, `3` nor
`4`) while still being listed explicitly in the scalar set.

The inverse mapping — used when a brace initializer has to name the target type of an existing
variable — always produces the **`float` spelling**: `0` → `MaterialAttributes`, `1` → `float`,
`2` → `float2`, `3` → `float3`, `4` → `float4`.

## Removed

| Token | Status | Replacement |
| :-- | :-- | :-- |
| `Scalar` | **removed** | `float` |
| `Color` | **removed** | `float4` / `vec4` |
| `Vector` | **removed** | `float2` … `float4` / `vec2` … `vec4` |

These names are not special-cased anywhere; they fall through to the generic "unsupported type"
diagnostic of whichever context they appear in — `Unsupported property type 'Scalar'.` in a
`Properties` block, `Unsupported Graph variable type 'Color' for '{Name}'.` in a `Graph` block.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table. `{Kind}` is one of
`ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`.

| Message | Cause |
| :-- | :-- |
| `Unsupported property type '{Type}'.` | the token is not accepted in `Properties` |
| `Unsupported Graph variable type '{Type}'.` | the token is not accepted in a `Graph` declaration with no initializer |
| `Unsupported Graph variable type '{Type}' for '{Name}'.` | the token is not accepted in a `Graph` declaration with an initializer |
| `Graph variable '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` declared in a `Graph` block on UE 5.3 |
| `Graph variable type '{Type}' requires an explicit initializer.` | a texture, `SamplerState` or `Substrate` declaration with no `=` |
| `Unsupported output type '{Type}' for '{Name}'.` | the token is not accepted in a `Shader` `Outputs` declaration |
| `Output '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` output declaration on UE 5.3 |
| `{Kind} '{Function}' input '{Name}' uses unsupported type '{Type}'.` | the token is not accepted in an `Inputs` section |
| `{Kind} '{Function}' output '{Name}' uses unsupported type '{Type}'.` | the token is not accepted in an `Outputs` / `Results` section |
| `{Kind} '{Function}' input '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` input on UE 5.3 |
| `{Kind} '{Function}' output '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` output on UE 5.3 |
| `DreamShader Function '{Name}' input '{Input}' uses unsupported type '{Type}'.` | the token is not accepted as a `Function` `in` parameter |
| `DreamShader Function '{Name}' has unsupported result type '{Type}'.` | the token is not accepted as a `Function` result |
| `DreamShader Function '{Name}' input '{Input}' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead.` | `Substrate` `in` parameter on a `Function` |
| `DreamShader Function '{Name}' result '{Result}' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead.` | `Substrate` result on a `Function` |
| `DreamShader GraphFunction '{Name}' input '{Input}' uses unsupported type '{Type}'.` | the token is not accepted as a `GraphFunction` `in` parameter |
| `DreamShader GraphFunction '{Name}' has unsupported result type '{Type}'.` | the token is not accepted as a `GraphFunction` result |
| `DreamShader GraphFunction '{Name}' input '{Input}' uses Substrate, which is only supported by GraphFunction Graph blocks.` | `Substrate` `in` parameter on a `GraphFunction` |
| `DreamShader GraphFunction '{Name}' result '{Result}' uses Substrate, which is only supported by GraphFunction Graph blocks.` | `Substrate` result on a `GraphFunction` |
| `Expected a MaterialAttributes value.` | a numeric value was assigned where `MaterialAttributes` was declared |
| `Expected a Substrate value.` | a non-`Substrate` value was assigned where `Substrate` was declared |
| `Expected a texture object value.` | a numeric value was assigned where a texture type was declared |
| `Expected a texture object value with a matching texture type.` | a texture of the wrong dimension was assigned |
| `MaterialAttributes values cannot be assigned to numeric outputs.` | a `MaterialAttributes` value was assigned to a numeric target |
| `Substrate values cannot be assigned to numeric outputs.` | a `Substrate` value was assigned to a numeric target |
| `Texture objects cannot be assigned to numeric outputs.` | a texture object was assigned to a numeric target |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

One token from each family, each in a context that accepts it:

```c
ShaderFunction(Name="Functions/F_Types")
{
    Properties = {
        float     Strength = 1.0;              // scalar token
        vec3      Tint     = vec3(1, 1, 1);    // GLSL vector spelling
        Texture2D Noise    = Path(Game, "Textures/T_Noise");
    }

    Inputs = {
        vec2                   UV;            // numeric
        opt StaticBool         UseTint;       // Inputs-only token
        opt MaterialAttributes InAttrs;       // Inputs-only token
    }

    Outputs = {
        vec3 OutColor;
    }

    Graph = {
        Texture2D Src = Noise;                 // texture declaration needs an initializer
        vec4 Sampled  = SampleTexture2D(Src, UV);
        float3 Base   = Sampled.rgb * Strength;
        OutColor      = Base * Tint;
    }
}
```

```c
// Function signature tokens are normalized, then validated at the call site.
Function float Luma(in vec3 color)          // vec3 -> float3 in the generated HLSL
{
    return dot(color, float3(0.299, 0.587, 0.114));
}
```

## See also

- [Properties](properties.md) — the `Properties` section grammar and which tokens it accepts
- [Inputs / Outputs / Results](inputs-outputs.md) — typed-parameter sections of material functions
- [Output bindings](output-bindings.md) — `Shader` `Outputs` declarations and `Base.*` targets
- [Function](function.md) — signature grammar, `in` / `out`, return types, generated HLSL
- [GraphFunction](graph-function.md) — the `UE.*`-hoisting function form
- [Keywords](keywords.md) — the complete keyword index and deprecated spellings
- [Graph declarations](../graph/declarations.md) — declaring values inside a `Graph` block
- [Conversions](../graph/conversions.md) — coercion, widening, silent narrowing
- [Constructors](../graph/constructors.md) — the 34 constructor names and the integer marker
- [MaterialAttributes](../graph/material-attributes.md) — reading and writing attribute members
- [Compact parameter types](../parameters/compact-types.md) — which node each compact token generates
- [Parameter nodes](../parameters/parameter-nodes.md) — the 22 explicit `*Parameter` tokens
- [OutputType / ResultType](../builtins/output-type.md) — the token set accepted by `UE.*` calls
- [Substrate builtins](../builtins/substrate.md) — the UE 5.4+ `Substrate.*` surface
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
