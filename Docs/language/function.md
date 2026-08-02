# Function

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Function**

A top-level block that declares a reusable HLSL helper: the body is HLSL text, it is emitted verbatim
into a generated `.ush` include, and every call site becomes a `UMaterialExpressionCustom` node.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` — and inside a [`Namespace`](namespace.md) body |
| Kind | top-level block |
| Generates | one HLSL function `DreamShaderFn_<Name>` in the [generated include](../generation/generated-hlsl.md); no `.uasset` |
| Multiplicity | any number per parse unit; names must be unique ignoring case |

## Synopsis

```c
Function [ { Inline | SelfContained } ] [ <return-type> ] <name> ( [ <parameter-list> ] )
{
    <hlsl>
}
```

```c
<parameter-list> ::= <parameter> [ , <parameter> ] …
<parameter>      ::= [ { in | out } ] <type-token> <parameter-name>
```

The keyword `Function` is matched **case-sensitively** and requires a non-identifier character after
it. `function`, `FUNCTION` and `Functions` are not the keyword and fall through to
`Unexpected token near index {Index}.` See
[Lexical elements](lexical.md#case-sensitivity).

Everything else in the declaration — the `Inline` / `SelfContained` modifier, the `in` / `out`
qualifiers, and every type token — is matched **case-insensitively**.

There is no `(Key = Value)` header, no `Settings`, and no sections of any kind. The `{ … }` after the
parameter list is raw HLSL, extracted by balanced-brace scanning that is aware of `//`, `/* */` and
`"…"`.

## Declaration order

The parser reads identifiers left to right and disambiguates by lookahead:

| Form | Read as |
| :-- | :-- |
| `Function Name(…)` | name |
| `Function Type Name(…)` | return type, then name |
| `Function SelfContained Name(…)` | modifier, then name |
| `Function SelfContained Type Name(…)` | modifier, return type, then name |
| `Function Type SelfContained Name(…)` | return type `Type`, name `SelfContained`, then a stray `Name` — fails with `Function 'SelfContained' is missing a valid parameter list. Expected '(' near index {Index}.` |

The rule is mechanical: after reading one identifier the scanner skips whitespace and comments; if the
next character is not `(`, the identifier just read was the *return type* and the next identifier is
the name. The modifier is therefore fixed in first position.

## Modifiers

| Modifier | Accepted on | Effect |
| :-- | :-- | :-- |
| `SelfContained` | `Function` only | the function's HLSL is embedded into each caller's Custom node instead of being referenced through the include |
| `Inline` | `Function` only | **exact synonym** of `SelfContained` — same parser branch, same flag, no behavioural difference whatsoever |

Both spellings are case-insensitive. Neither is accepted on
[`GraphFunction`](graph-function.md): there the modifier check is skipped and the token is consumed
as a return type instead.

> [!WARNING]
> A function may not be **named** `Inline` or `SelfContained`. `Function SelfContained(in float a,
> out float b) { … }` takes the modifier branch, then finds `(` where the name should be, and fails
> with `Function declaration is missing a valid function name after SelfContained.`

## Parameters

Parameters are split on top-level `,` (depth tracking covers `()`, `[]` and `"…"`, but **not** `{}`),
then each parameter is split on whitespace. Exactly two or three whitespace-separated tokens are
accepted:

| Tokens | Form | Qualifier |
| :-- | :-- | :-- |
| 2 | `<type> <name>` | defaults to `in` |
| 3 | `<qualifier> <type> <name>` | as written |
| fewer than 2, or more than 3 | — | `Function '{Name}' has an invalid parameter declaration '{Text}'.` |

| Rule | Behaviour |
| :-- | :-- |
| Qualifiers | Only `in` and `out`. Lower-cased before comparison, so `In`, `OUT`, `iN` all work. |
| `inout` | **Does not exist.** `inout float x` is a three-token parameter whose qualifier is neither `in` nor `out`, and is rejected: `Function '{Name}' parameter '{Text}' uses unsupported qualifier 'inout'. Supported qualifiers are in and out.` |
| Whitespace | Any whitespace separates the tokens, tabs included. This is *unlike* the `Inputs` / `Outputs` / `Results` sections, which split on a literal space character only. |
| Empty parameters | Empty segments are silently skipped, so a trailing comma before `)` is tolerated. |
| Order | `in` parameters become the function's inputs in declaration order; `out` parameters become its results in declaration order. The two may be interleaved. |
| Reserved name | A parameter named `__return` (ignoring case) is rejected — the name is reserved for return-type lowering. |
| Not available | `opt`, default values, and `[ … ]` metadata blocks are **not** part of this grammar. They belong to the [`Inputs` / `Outputs` sections](inputs-outputs.md) of `ShaderFunction` and `VirtualFunction`. |

### At least one output

A `Function` must produce something. `Results` — the `out` parameter list — must be non-empty after
parsing, which means the declaration must carry **either** at least one `out` parameter **or** a
return type. Neither → `Function '{Name}' must declare at least one out parameter.`

## Return type

| Form | Legal | Result |
| :-- | :-- | :-- |
| `Function Name(in T a, out U r)` | yes | one result, `r` |
| `Function Name(in T a, out U r1, out V r2)` | yes | two results, `r1` then `r2` |
| `Function U Name(in T a) { return expr; }` | yes | one synthetic result named `__return` of type `U` |
| `Function U Name(in T a, out V r)` | **no** | `Function '{Name}' has a return type and cannot also declare out parameters. Use out parameters without a return type for multiple outputs.` |
| `Function Name(in T a)` | **no** | no output at all |
| `Function U Name(…) { return; }` | **no** | bare `return;` under a return type |
| `Function <empty-after-normalisation> Name(…)` | **no** | `Function '{Name}' has an invalid return type '{Token}'.` |

So: **a return type implies exactly one output, and forbids every explicit `out`.** Multiple outputs
require `out` parameters and no return type.

### `return` lowering

When a return type is declared, the body is rewritten before codegen: every `return` keyword at
**brace depth 0**, bounded by non-identifier characters, is replaced by the literal text
`__return =`. The trailing expression and its `;` are preserved verbatim. The scan skips `//`,
`/* */`, `"…"` and `'…'`, so `returnValue` and `my_return` are untouched.

A bare `return;` at depth 0 is a hard error:
`A function with a return type cannot use a bare 'return;'. Return a value, e.g. 'return expr;'.`

> [!NOTE]
> Only depth-0 `return`s are lowered. A `return expr;` nested inside `if { … }` stays a real HLSL
> `return` in the emitted `DreamShaderFn_*` body. That is legal HLSL and returns the same type, but it
> bypasses the `__return` variable. The equivalent construct inside a
> [`GraphFunction`](graph-function.md) behaves differently, because that body is inlined directly
> into the Custom node.

## Type tokens

The 15 GLSL aliases (`vec2`…`vec4`, `ivec*`, `uvec*`, `bvec*`, `mat2`…`mat4`) are normalized to their
HLSL spellings at parse time, on both the return type and every parameter type. A token that matches
no alias is passed through with its case preserved and is **not validated at parse time** — an
unknown type token only fails when the function is called. The full alias list is in
[Type tokens](types.md).

After normalisation, the resolver accepts:

| Token(s) | Resolves as | Valid as `in` | Valid as result / return |
| :-- | :-- | :-- | :-- |
| `float`, `float1`, `half`, `half1`, `int`, `uint`, `bool` | 1 component | yes | yes |
| `float2`, `half2`, `int2`, `uint2`, `bool2` | 2 components | yes | yes |
| `float3`, `half3`, `int3`, `uint3`, `bool3` | 3 components | yes | yes |
| `float4`, `half4`, `int4`, `uint4`, `bool4` | 4 components | yes | yes |
| `MaterialAttributes` | material attributes | yes | yes |
| `StaticBool`, `StaticBoolParameter` | 1 component | yes | **no** |
| `Texture2D`, `SamplerState` | Texture2D object | yes | **no** |
| `TextureCube` | TextureCube object | yes | **no** |
| `Texture2DArray` | Texture2DArray object | yes | **no** |
| `Texture3D`, `VolumeTexture` | volume texture object | yes | **no** |
| `Substrate` *(since UE 5.4)* | Substrate material | **no** — see below | **no** |

Anything not in this table produces
`DreamShader Function '{Name}' input '{Param}' uses unsupported type '{Token}'.` or
`DreamShader Function '{Name}' has unsupported result type '{Token}'.` at the call site. Matrices
(`float3x3`, `float4x4`, and therefore `mat2`/`mat3`/`mat4`) are accepted by the parser and rejected
there.

> [!WARNING]
> `Substrate` is never usable on a `Function`. On UE 5.4+ it fails with
> `… uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or
> ShaderFunction instead.`; on UE 5.3 the message is
> `… uses Substrate, which requires Unreal Engine 5.4 or newer.` See
> [Substrate builtins](../builtins/substrate.md).

> [!NOTE]
> Resolving is not the same as compiling. Only `VolumeTexture` is rewritten in the generated HLSL (to
> `Texture3D`); every other token is emitted exactly as written. A token the resolver accepts but
> HLSL does not know will surface later, as a shader-compiler error against the generated `.ush`.

### Texture parameters and samplers

Five tokens are treated as *texture function parameters*: `Texture2D`, `TextureCube`,
`Texture2DArray`, `Texture3D`, `VolumeTexture` (case-insensitive). For each one, the generated HLSL
signature gains an extra `SamplerState <ParamName>Sampler` parameter immediately after it, and every
generated call site passes the matching `<argument>Sampler` argument. Use that name inside the body
to sample the texture.

`SamplerState` is **not** in that list. A parameter declared `SamplerState S` resolves as an ordinary
Texture2D object at the call site and receives no automatic companion parameter.

## Body normalisation

Every `Function` and `GraphFunction` body — and nothing else in the language — is passed through an
identifier-level rewrite before it is stored. The scan is comment- and string-aware. Two things
happen:

1. **Alias substitution.** Whole identifiers are matched *case-insensitively* against 18 entries: the
   15 GLSL type aliases plus `mix` → `lerp`, `fract` → `frac`, `mod` → `fmod`.
2. **Qualified-name flattening.** A token of the form `A::B` (repeatable) is replaced by the
   identifier-sanitized spelling of the whole thing — `Common::Remap01` becomes `Common_Remap01`.

> [!WARNING]
> Because the alias match is case-insensitive and applies to whole identifiers anywhere in the body,
> a helper of your own called `Mix`, `Mod`, `Fract`, `Vec3`, `Mat4` — or a *variable* by one of those
> names — is silently renamed in the emitted HLSL. There is no diagnostic. Rename the identifier, or
> spell it so it does not collide (`MixColor`, `ModValue`).

> [!WARNING]
> Namespace-qualified calls do not work **inside** a `Function` or `GraphFunction` body. See
> [Namespace](namespace.md#calling-a-namespaced-function) for the observed symptom and the
> workaround. Namespace-qualified calls from a `Graph` block are unaffected.

## Generated HLSL

Each `Function` is emitted into the generated include as:

```hlsl
<ResultType0> DreamShaderFn_<SanitizedName>(<params>)
{
	<ResultType0> <Result0Name> = (<ResultType0>)0;
	<Result1Name> = (<ResultType1>)0;
	<body, indented>
	return <Result0Name>;
}
```

| Element | Rule |
| :-- | :-- |
| Symbol name | `DreamShaderFn_` + the function's full name with every non-`[A-Za-z0-9_]` character replaced by `_` and runs of underscores collapsed. `Common::ApplyTint` → `DreamShaderFn_Common_ApplyTint`. |
| Why the prefix | An unprefixed `Luminance(float3)` would redefine the engine intrinsic from `/Engine/Private/Common.ush` and fail shader compilation with `redefinition of 'Luminance'`. |
| Return value | `Results[0]` is the HLSL return value, never a parameter. |
| Parameters | every `in` parameter; a `SamplerState <Name>Sampler` after each texture-typed input; then every `Results[i]` with `i ≥ 1` as `out <Type> <Name>`. |
| Type rewriting | `VolumeTexture` → `Texture3D`; everything else verbatim. |

The include file, its name, its header guard and its virtual path are specified in
[Generated HLSL](../generation/generated-hlsl.md). All `Function` declarations in a parse unit go into
it — including `SelfContained` ones. `GraphFunction` declarations never do.

## `Inline` / `SelfContained` mode

Without the modifier, a call site emits a Custom node whose `Code` calls the include:

```hlsl
return DreamShaderFn_Remap01(value);
```

and whose `IncludeFilePaths` always contains the generated `.ush` virtual path.

With the modifier, the function and the transitive closure of the plain `Function`s it calls are
**embedded into the calling Custom node's own code**, wrapped in a struct:

```hlsl
struct generated_wrapper_<SanitizedHint>_<CRC32 %08X>
{
	<definition of each embedded function, dependencies first>
};
generated_wrapper_…_<CRC32 %08X> __ds_wrapper_<CRC32 %08X>;

return __ds_wrapper_<CRC32 %08X>.DreamShaderFn_Remap01(value);
```

| Aspect | Default | `Inline` / `SelfContained` |
| :-- | :-- | :-- |
| Where the body lives | the shared generated `.ush` | duplicated inside every calling Custom node |
| `IncludeFilePaths` | always the generated include | only when some *other* direct callee was not embedded |
| Struct wrapper | none | `generated_wrapper_*` type plus an `__ds_wrapper_*` instance variable |
| Members emitted | — | in dependency order (a callee precedes its caller) |
| Sibling calls inside the wrapper | — | plain `DreamShaderFn_*`, with no `this.` qualifier |
| Recursion | permitted by the include (HLSL will reject it) | detected and reported before emission |
| Still written to the include | yes | yes — the modifier adds embedding, it does not remove the include entry |

The hint used for both CRC values is the function's full DSL name, hashed **unsanitized**, so
`Common::Remap01` and `Common_Remap01` hash differently even though they sanitize to the same symbol.

> [!NOTE]
> Embedding is driven by the *call site*, not the declaration alone. Only functions reached from an
> embedded root are pulled into the wrapper; a plain `Function` that the embedded closure calls is
> embedded with it.

## Calling a Function

Both call forms live in a [`Graph`](../graph/index.md) block and are specified in
[Calling functions](../graph/calls.md). In summary:

| Form | Requirement |
| :-- | :-- |
| `x = Fn(a, b);` — value call *(since 1.3.1)* | exactly one output; argument count equals the input count |
| `Fn(a, b, OutX, OutY);` — statement call | arguments are all inputs, then one **plain variable name** per `out` result, in declaration order |

Named arguments are not supported on `Function` in either form. Out targets must be bare names, must
be non-empty, and must be distinct within one call (compared case-insensitively). Because lookup also
accepts the mangled spelling, `DreamShaderFn_Luma(c)` is a legal call in a `Graph` block.

## Notes

- **No overload resolution.** Names are not scoped by arity or parameter types; the first declaration
  whose name matches (ignoring case) wins. Two `Function`s with the same name in the same parse unit
  are caught later, when the include is written.
- **The parse unit is the whole import closure.** `import` directives are inlined into one text before
  parsing, so a name clash across imported headers is a clash. See [import](import.md).
- **Case-insensitive collisions are real collisions.** `Luma` and `luma` are "declared more than
  once"; so are `A::B` and `A_B`, which mangle to the same symbol.
- A `.dsm` or `.dsf` containing only `Function` blocks compiles successfully and produces no asset:
  `Generated DreamShader helper include '{Path}' from {File}.`
- On UE 5.3 the generated Custom nodes display their code in the material graph; from UE 5.4 onward
  `ShowCode` is set to `false` on every generated Custom node.
- The legacy section-style body `Function Name { Inputs = { … } Code = { … } }` is **not** a supported
  form. A `{` where the parameter list's `(` should be makes `Name` the return type, and the parse
  fails with `Function declaration is missing a function name after the return type 'Name'.`

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this section.

### Parse time

| Message | Cause |
| :-- | :-- |
| `Function declaration is missing a valid function name.` | the token after `Function` is not an identifier |
| `Function declaration is missing a valid function name after SelfContained.` | `Function SelfContained(` or `Function Inline(` |
| `Function declaration is missing a function name after the return type '{Token}'.` | a return type not followed by an identifier |
| `Function '{Name}' is missing a valid parameter list. {Inner}` | the `( … )` block could not be extracted |
| `Function '{Name}' is missing a valid body block. {Inner}` | the `{ … }` block could not be extracted |
| `Function '{Name}' has an invalid return type '{Token}'.` | the return-type token is empty after normalisation |
| `Function '{Name}' has an invalid parameter declaration '{Text}'.` | the parameter has fewer than 2 or more than 3 whitespace tokens, or an empty type or name |
| `Function '{Name}' parameter '{Text}' uses unsupported qualifier '{Qualifier}'. Supported qualifiers are in and out.` | a qualifier other than `in` / `out` — including `inout` |
| `Function '{Name}' parameter name '__return' is reserved for return-type lowering.` | a parameter named `__return` |
| `Function '{Name}' has a return type and cannot also declare out parameters. Use out parameters without a return type for multiple outputs.` | return type combined with any `out` |
| `Function '{Name}' must declare at least one out parameter.` | no `out` parameter and no return type |
| `A function with a return type cannot use a bare 'return;'. Return a value, e.g. 'return expr;'.` | a depth-0 `return;` in a return-typed body |
| `Expected '{Char}' near index {Index}.` | the `(` or `{` opener is missing |
| `Unterminated '{Char}' block.` | the `(` or `{` is never closed |

`{Name}` is the fully-qualified name — `Ns::Fn` inside a [`Namespace`](namespace.md).

### Include generation

| Message | Cause |
| :-- | :-- |
| `DreamShader Function '{Name}' is declared more than once.` | two declarations with names equal ignoring case |
| `DreamShader Function '{Name}' collides with another generated helper symbol '{Symbol}'. Rename the Function or Namespace.` | two names that sanitize to the same `DreamShaderFn_*` symbol |
| `SelfContained Function cycle detected: {A -> B -> A}. HLSL Custom nodes cannot compile recursive DreamShader functions.` | a cycle in the embedded closure |
| `Unknown SelfContained DreamShader Function '{Name}'.` | an embedding root that resolves to no declaration |
| `Failed to write generated helper include '{Path}'.` | the `.ush` could not be written |

### Call time

| Message | Cause |
| :-- | :-- |
| `Unknown Graph function '{Name}'.` | no `Function` with that name |
| `DreamShader Function '{Name}' has {N} outputs and must be called with explicit out variables, for example {Name}(..., ResultA, ResultB).` | value-call form used on a multi-output function |
| `DreamShader Function '{Name}' returns one value and expects {N} input argument(s) when used as a value expression, but got {M}.` | wrong argument count in a value call |
| `DreamShader Function '{Name}' must declare at least one out result.` | statement call on a function with no results |
| `DreamShader Function '{Name}' expects {N} arguments ({I} inputs, {O} out targets) but got {M}.` | wrong argument count in a statement call |
| `DreamShader Function '{Name}' currently uses positional arguments only.` | a `Key = Value` argument was passed |
| `DreamShader Function '{Name}' out argument {N} must be a plain variable name.` | an out target that is not a bare identifier (index is 1-based) |
| `DreamShader Function '{Name}' has an empty out target name.` | an empty out target |
| `DreamShader Function '{Name}' cannot write multiple out results into '{Target}' in the same call.` | the same target named twice (compared case-insensitively) |
| `DreamShader Function '{Name}' input '{Param}': {Inner}` | the argument expression failed to evaluate or coerce |
| `DreamShader Function '{Name}' input '{Param}' uses unsupported type '{Token}'.` | the declared parameter type does not resolve |
| `DreamShader Function '{Name}' input '{Param}' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead.` | `Substrate` parameter, UE 5.4+ |
| `DreamShader Function '{Name}' input '{Param}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` parameter, UE 5.3 |
| `DreamShader Function '{Name}' has unsupported result type '{Token}'.` | a result type outside the float1–4 / `MaterialAttributes` set |
| `DreamShader Function '{Name}' result '{Result}' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead.` | `Substrate` result, UE 5.4+ |
| `DreamShader Function '{Name}' result '{Result}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` result, UE 5.3 |
| `Failed to create a Custom node for DreamShader Function '{Name}'.` | node creation failed |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
// DShader/Lib/Color.dsh

Function float Luma(in vec3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

Function SelfContained Remap01(in float value, out float result)
{
    result = saturate(value * 0.5 + 0.5);
}

Function SampleTinted(in Texture2D tex, in vec2 uv, in vec3 tint, out vec3 rgb, out float alpha)
{
    float4 texel = Texture2DSample(tex, texSampler, uv);
    rgb   = texel.rgb * tint;
    alpha = texel.a;
}
```

Used from a `Shader`:

```c
import "Lib/Color.dsh"

Shader(Name="Materials/M_Tinted")
{
    Properties = {
        Texture2D BaseTex = Path(Game, "Textures/T_Base");
        vec3      Tint    = vec3(1.0, 0.6, 0.2);
    }
    Outputs = {
        vec3  Color;
        float Alpha;
        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }
    Graph = {
        vec2  UV = UE.TexCoord(Index = 0);
        vec3  Rgb;
        float A;
        SampleTinted(BaseTex, UV, Tint, Rgb, A);

        float Key;
        Remap01(Luma(Rgb), Key);

        Color = Rgb * Key;
        Alpha = A;
    }
}
```

Generated include (excerpt) — note the injected `SamplerState`, the `__return` lowering, and the
`Results[0]`-as-return-value shape:

```hlsl
// <Project>/Intermediate/DreamShader/GeneratedShaders/Color_<hash>.ush
// virtual path /DreamShaderGenerated/Color_<hash>.ush

float DreamShaderFn_Luma(float3 color)
{
	float __return = (float)0;
	__return = dot(color, float3(0.299, 0.587, 0.114));
	return __return;
}

float DreamShaderFn_Remap01(float value)
{
	float result = (float)0;
	result = saturate(value * 0.5 + 0.5);
	return result;
}

float3 DreamShaderFn_SampleTinted(Texture2D tex, SamplerState texSampler, float2 uv, float3 tint, out float alpha)
{
	float3 rgb = (float3)0;
	alpha = (float)0;
	float4 texel = Texture2DSample(tex, texSampler, uv);
	rgb   = texel.rgb * tint;
	alpha = texel.a;
	return rgb;
}
```

## See also

- [GraphFunction](graph-function.md) — the variant whose `UE.*` calls become real material nodes
- [Namespace](namespace.md) — `Ns::Fn` qualification and the `DreamShaderFn_Ns_Fn` symbol
- [Calling functions](../graph/calls.md) — value vs statement calls, argument rules, ambiguity
- [Generated HLSL](../generation/generated-hlsl.md) — the `.ush` include, its name, guard and location
- [Type tokens](types.md) — the complete token catalogue and the GLSL alias tables
- [Inputs / Outputs / Results](inputs-outputs.md) — the *section* parameter grammar, with `opt` and metadata
- [ShaderFunction](shader-function.md) — the block that generates a real `UMaterialFunction` asset
- [Source files](source-files.md) — which of `.dsm` / `.dsh` / `.dsf` may hold a `Function`
- [Lexical elements](lexical.md) — comments, identifiers, and the case-sensitivity matrix
- [HLSL library](../builtins/hlsl-library.md) — helpers available inside a `Function` body
- [Substrate builtins](../builtins/substrate.md) — why `Substrate` is a `GraphFunction`/`ShaderFunction` concern
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
