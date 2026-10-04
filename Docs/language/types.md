# Type tokens

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Type tokens**

The closed set of identifiers that name a value's shape in a declaration, and the per-context rules
that decide which of them a given declaration position accepts.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` — wherever a declaration is legal |
| Kind | lexical category |
| Matching | **case-insensitive** in every context |

*(since 2.0.0)* Every type token goes through one table: a 1.x spelling is read as its 2.0 type
(`vec3` and `Float3` as `float3`, `MaterialAttributes` as `material`, `StaticBool` as `bool`), with no
diagnostic, and a word that is no type is kept as written for the binder to refuse
([`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201)) — in `Properties` the front end refuses it first
([`DSH3252`](../diagnostics/DSH3xxx.md#dsh3252)). The 2.0 type names are on
[DreamShaderLang 2.0](../language-v2/index.md#declarations).

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
> *(since 2.0.0)* `Prop` is checked by the front end while the section is read. The other columns are
> checked by the binder for the whole file, and by the lowering to the graph where a value of the type
> would become a pin — a type no pin carries is refused there, at the declaration or call that needs
> the pin.

## Validity matrix

`✔` accepted · `✘` rejected. A cell's code is the one a rejection raises.

| Tokens | `Prop` | `I/O` | `Fn in` | `Fn out` | `Out decl` | `Graph` |
| :-- | :-: | :-: | :-: | :-: | :-: | :-: |
| scalars and vectors: `float`, `half`, `int`, `uint`, `bool` families, their GLSL spellings | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `Texture2D` `TextureCube` `Texture2DArray` `Texture3D` `VolumeTexture` | ✔ | ✔ ⁽¹⁾ | ✔ ⁽²⁾ | ✘ `DSH6254` | ✔ ⁽³⁾ | ✔ ⁽⁴⁾ |
| `MaterialAttributes` | ✘ `DSH3252` | ✔ | ✘ `DSH6210` | ✔ | ✔ | ✔ |
| `Substrate` | ✘ `DSH3252` | ✔ ⁽⁵⁾ | ✘ `DSH6253` | ✘ `DSH6253` | ✔ ⁽³⁾ | ✔ ⁽⁴⁾ |
| `StaticBool` | ✔ ⁽⁶⁾ | ✔ ⁽⁷⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ |
| `StaticBoolParameter` | ✔ ⁽⁹⁾ | ✔ ⁽⁷⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ |
| `mat2` `mat3` `mat4`, `float2x2` … `float4x4` | ✘ `DSH3252` | ✘ | ✘ | ✘ | ✘ | ✘ — see [Matrices](#matrices) |

1. A `Texture3D` input is a `VolumeTexture` pin.
2. A texture-typed `in` parameter of a `Function` also takes a companion `SamplerState <Name>Sampler`
   parameter in the generated HLSL, the one the engine declares beside every texture input of a
   `Custom` node. See [Function](function.md).
3. A variable only: no `Base.` pin takes a texture, and a `Substrate` one goes only to
   `Base.FrontMaterial`. It has no zero value, so the `Graph` has to assign it before it is read.
4. Texture, `SamplerState` and `Substrate` declarations in a `Graph` block **must** carry an
   initializer — there is no default value for them:
   [`DSH2215`](../diagnostics/DSH2xxx.md#dsh2215).
5. A `Substrate` input is a Substrate pin of the function; Substrate values come from `Substrate.*`
   nodes, which the engine has from UE 5.4 on.
6. *(since 2.0.0)* Read as `bool`, so `StaticBool X = true;` in `Properties` declares a **dynamic**
   bool parameter, not a static one; 1.x refused the token there. Write `StaticBoolParameter` for a
   static switch parameter.
7. An input of either token is a `StaticBool` pin of the function.
8. Read as `bool`.
9. In `Properties`, `StaticBoolParameter` is a **parameter-node token**, not a type token: it
   generates a `UMaterialExpressionStaticBoolParameter` and accepts only `true` / `false` as its
   default ([`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254)). See
   [Parameter nodes](../parameters/parameter-nodes.md).

*(since 2.0.0)* `SamplerState` is a type of its own; 1.x read it as `Texture2D` wherever it accepted
it. It is refused in `Properties` (`DSH3252`), as a material-function input
([`DSH4364`](../diagnostics/DSH4xxx.md#dsh4364)) and as a `Function` result (`DSH6254`), and a `Graph`
declaration of one needs an initializer (`DSH2215`).

### Counts at a glance

| Family | Tokens | Count |
| :-- | :-- | --: |
| Scalar (1 component) | `float` `half` `double` `int` `uint` `bool`, and each with a `1` suffix (`float1` …) | 12 |
| Vector (2 / 3 / 4 components) | `float2..4` `half2..4` `double2..4` `int2..4` `uint2..4` `bool2..4` `vec2..4` `ivec2..4` `uvec2..4` `bvec2..4` | 30 |
| Texture | `Texture2D` `TextureCube` `Texture2DArray` `Texture3D` `VolumeTexture` | 5 |
| Opaque / other | `SamplerState` `MaterialAttributes` `Substrate` `StaticBool` `StaticBoolParameter` | 5 |

> [!NOTE]
> There is no `vec1`, `ivec1`, `uvec1` or `bvec1`, and no `half` GLSL vector spelling. The
> single-component GLSL-style forms simply do not exist; use `float` or `float1`.

## GLSL aliases

`vec` / `ivec` / `uvec` / `bvec` spellings are first-class tokens everywhere the corresponding
`float` / `int` / `uint` / `bool` spelling is accepted — in `Properties`, `Inputs`, `Outputs`, `Graph`
declarations and `Graph` constructors — and are read as that spelling.

| GLSL spelling | Equivalent |
| :-- | :-- |
| `vec2` `vec3` `vec4` | `float2` `float3` `float4` |
| `ivec2` `ivec3` `ivec4` | `int2` `int3` `int4` |
| `uvec2` `uvec3` `uvec4` | `uint2` `uint3` `uint4` |
| `bvec2` `bvec3` `bvec4` | `bool2` `bool3` `bool4` |

### GLSL identifier rewrites in `Function` bodies

A `Function` / `GraphFunction` **signature** reads every type token through the table above, and the
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
body are **not** rewritten. In a `Graph` block `mix`, `fract` and `mod` call `lerp`, `frac` and `fmod`,
with the warning [`DSH5277`](../diagnostics/DSH5xxx.md#dsh5277) *(since 2.0.0)* (legacy rule L2). See
[Math builtins](../builtins/math.md).

> [!WARNING]
> The body rewrite matches the whole identifier, ignoring case. A helper, local variable or struct
> member named `Mix`, `Mod`, `Fract`, `Vec3`, `Mat4` … inside a `Function` or `GraphFunction` body is
> silently renamed to `lerp`, `fmod`, `frac`, `float3`, `float4x4`. There is no diagnostic; the
> failure surfaces as an HLSL compile error, or as silently different math. Rename the identifier.

## Matrices

There are **no matrix values in a graph**. `mat2` / `mat3` / `mat4` and `float2x2` / `float3x3` /
`float4x4` are refused in `Properties` (`DSH3252`), and anywhere else a value of a matrix type that
would become a node or a pin is [`DSH4361`](../diagnostics/DSH4xxx.md#dsh4361).

`mat2` / `mat3` / `mat4` are nevertheless accepted *lexically* in a `Function` signature, which reads
them as `float2x2` … `float4x4`. The declaration therefore parses, and the call fails:

```c
Function float3 Rotate(in mat3 basis, in vec3 v) { return mul(basis, v); }
// parses; a Graph call that passes a matrix to 'basis' is DSH4361
```

Matrix-shaped work is reachable through the transform builtins — see
[UE.TransformVector / UE.TransformPosition](../builtins/transform.md) — or inside an HLSL body.

## Whitespace inside a token

*(since 2.0.0)* A type token is one word. 1.x removed the spaces inside a token in some contexts, so
`float 3` read as `float3` and `Material Attributes` as `MaterialAttributes` there; now they are two
words, and the declaration does not parse.

## Generated HLSL spelling

When a `Function` is lowered to a generated `.ush` helper, each declared type is written into the HLSL
signature in its 2.0 spelling:

| Declared token | HLSL emitted |
| :-- | :-- |
| `VolumeTexture` | `Texture3D` |
| `MaterialAttributes` | `FMaterialAttributes` — as a result only; as an input it is `DSH6210` |
| `StaticBool`, `StaticBoolParameter` | `bool` *(since 2.0.0)* |
| a GLSL spelling or another case (`vec3`, `Float3`) | the 2.0 spelling (`float3`) |
| everything else | the token, as written |

*(since 2.0.0)* 1.x wrote `MaterialAttributes`, `StaticBool` and `StaticBoolParameter` verbatim, which
produced a helper Unreal could not compile.

## Component-count derivation

A token's width is its digit suffix: `…2` to `…4` is a vector of that many components, `…1` or no
digit a scalar — `float1` is HLSL's spelling of a scalar — and `…NxM` a matrix.

## Removed

| Token | Status | Replacement |
| :-- | :-- | :-- |
| `Scalar` | **removed** | `float` |
| `Color` | **removed** | `float4` / `vec4` |
| `Vector` | **removed** | `float2` … `float4` / `vec2` … `vec4` |

These names are not special-cased anywhere; they are refused like any word that is no type —
`DSH3252` in a `Properties` block, `DSH4201` everywhere else.

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH3252` | the token is not accepted in `Properties` |
| [`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253) | a `Properties` parameter-node token with no 2.0 form |
| `DSH4201` | a word in type position outside `Properties` is no type |
| `DSH2215` | a texture, `SamplerState` or `Substrate` declaration in a `Graph` block has no initializer |
| `DSH4361` | a matrix value would become a node or a pin |
| `DSH4364` | a material-function input of a type no function pin carries, such as `SamplerState` |
| `DSH6210` | a `Function` / `GraphFunction` takes `MaterialAttributes` as an input |
| [`DSH6253`](../diagnostics/DSH6xxx.md#dsh6253) | a `Function` / `GraphFunction` takes or returns `Substrate` |
| `DSH6254` | a `Function` / `GraphFunction` returns a texture or a sampler |
| [`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228) | a value of one kind is assigned where another is declared — a number to a `MaterialAttributes` or texture variable, a texture to a number |
| `DSH5277` | warning: `mix`, `fract` or `mod` in a `Graph` block |

The complete list is in the [diagnostics index](../diagnostics/index.md).

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
        opt StaticBool         UseTint;       // a StaticBool pin
        opt MaterialAttributes InAttrs;       // a MaterialAttributes pin
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
// Function signature tokens are read as their 2.0 spelling, then checked like any declaration.
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
- [Constructors](../graph/constructors.md) — the constructor names
- [MaterialAttributes](../graph/material-attributes.md) — reading and writing attribute members
- [Compact parameter types](../parameters/compact-types.md) — which node each compact token generates
- [Parameter nodes](../parameters/parameter-nodes.md) — the 22 explicit `*Parameter` tokens
- [OutputType / ResultType](../builtins/output-type.md) — the token set accepted by `UE.*` calls
- [Substrate builtins](../builtins/substrate.md) — the UE 5.4+ `Substrate.*` surface
- [DreamShaderLang 2.0](../language-v2/index.md) — the type names a `.dss` writes
- [Diagnostics index](../diagnostics/index.md) — every code
