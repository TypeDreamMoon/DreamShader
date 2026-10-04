# Declarations

> [DreamShader](../index.md) » [Graph](index.md) » **Declarations**

A statement that introduces a new name into the `Graph` body and binds it to a typed value.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph` block |
| Kind | statement |
| Generates | nodes for the initializer; a declaration without one is given a zero of its type |

## Synopsis

```c
<type> <name> [ = { <expression> | <brace-initializer> } ]
     [ , <name> [ = { <expression> | <brace-initializer> } ] ] … ;

brace-initializer := { [ <expression> [ , <expression> ] … ] }
```

A statement is a declaration when it starts with two names in a row — a type token and the variable's
name *(since 2.0.0; 1.x split the text at its last top-level whitespace)*. Both are single tokens.

> [!NOTE]
> The name is one identifier. `float3 A.B = x;` is [`DSH2154`](../diagnostics/DSH2xxx.md#dsh2154) at
> the `.` *(since 2.0.0)*; 1.x declared a variable literally named `A.B`. A member write has no type in
> front of it: `Attrs.BaseColor = x;`.

## Accepted type tokens

| Token(s) | Components | Notes |
| :-- | --: | :-- |
| `float` `float1` `half` `half1` `int` `uint` `bool` | 1 | |
| `float2` `half2` `vec2` `int2` `uint2` `bool2` `ivec2` `uvec2` `bvec2` | 2 | |
| `float3` `half3` `vec3` `int3` `uint3` `bool3` `ivec3` `uvec3` `bvec3` | 3 | |
| `float4` `half4` `vec4` `int4` `uint4` `bool4` `ivec4` `uvec4` `bvec4` | 4 | |
| `MaterialAttributes` | 0 | see [MaterialAttributes](material-attributes.md) |
| `Substrate` | 0 | **UE 5.4+**; requires an initializer |
| `StaticBool` `StaticBoolParameter` | 1 | a `bool` variable *(since 1.6.0)* |
| `Texture2D` | 0 | requires an initializer |
| `TextureCube` | 0 | requires an initializer |
| `Texture2DArray` | 0 | requires an initializer |
| `Texture3D` `VolumeTexture` | 0 | requires an initializer |
| `SamplerState` | 0 | a sampler of its own, no longer a `Texture2D` *(since 2.0.0)*; requires an initializer |

Every row is matched case-insensitively (`Float3`, `VEC3`, `materialattributes`). A type token is one
token *(since 2.0.0)*: `float 3` and `Material Attributes` are no longer read as types.
`vec*`, `ivec*`, `uvec*` and `bvec*` read as `float*`, `int*`, `uint*` and `bool*`.

`int`, `uint`, `bool` and `half` are kinds of their own to the compiler — a `/` between two integer
constructors is [`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243), a condition is a `bool` — and every one of them is
a float in the generated graph. See [Type tokens](../language/types.md). The matrix spellings
(`mat3`, `float3x3`) resolve, but the graph has no matrix values:
[`DSH4361`](../diagnostics/DSH4xxx.md#dsh4361).

## Uninitialized declarations

`<type> <name>;` is given an initializer by the legacy front end, because 1.x read an unset variable
as zero:

| Declared type | Initializer |
| :-- | :-- |
| scalar (1 component) | `0` (`false` for a `bool`) |
| vector (2–4 components) | `floatN(0, …)` — one zero per component, in the declared kind |
| `MaterialAttributes` | an empty attribute set (`MakeMaterialAttributes`) |
| `Substrate` | none — [`DSH2215`](../diagnostics/DSH2xxx.md#dsh2215) |
| any texture type, `SamplerState` | none — `DSH2215` |
| a token that does not resolve | [`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201) |

```c
float3 c;              // float3(0, 0, 0)
MaterialAttributes A;  // an empty attribute set, ready for member writes
Texture2D T;           // DSH2215: requires an initializer
```

## Declarations with an initializer

The initializer is evaluated, then converted to the declared type:

| Order | Step |
| --: | :-- |
| 1 | the type token must resolve, else `DSH4201` |
| 2 | a scalar spreads to every component of a vector type |
| 3 | a wider vector gives its **leading** components — legacy rule L22, said by the info [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289) |
| 4 | anything else that does not fit — a narrower vector, a texture into a number — is [`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228) |

Step 4 means a 2-component value never widens to 3. See [Conversions](conversions.md).

## Authoritative widths override the declared width

*(since 2.0.0)* They no longer do: the declared type is the variable's type. 1.x let certain values
(`UE.TexCoord`, `UE.CameraVectorWS`, a constant-folded constructor, `dot`, …) keep their own width
whatever the declaration said, so `float2 dir = UE.CameraVectorWS();` made `dir` three components
without a message. Now `dir` is a `float2`: the leading two components of the value, said by
`DSH5289` when the value is typed wider.

```c
Graph = {
    float3 c = Tint;            // Tint is a float4 property: DSH5289, c = Tint.rgb
    float2 dir = UE.CameraVectorWS();   // dir is two components
}
```

A 1.x source that relied on the override builds a different graph than it did through 1.9.x, and
nothing reports it: [`dsc migrate`](../tools/migrate.md) compares what the 2.0 compiler builds from the
1.x file with what it builds from the `.dss`, and the two agree. Rebuild such a material and check it.

Binary operators never narrow an operand: two operands of different widths, neither of them a
scalar, are [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226). Assignment, declaration, function inputs
and pins take the leading components (`DSH5289`). See [Conversions](conversions.md).

## Comma declarators

```c
float a = 1, b, c = 3;
```

One variable per declarator is declared. Rules:

| Rule | Behaviour |
| :-- | :-- |
| Recognition | a declaration whose declarator is followed by `,` and another name |
| Shared type | every declarator has the type of the **first**; a type token after a `,` is read as the next declarator's name, and what follows it is `DSH2154` |
| Declarator names | must be identifiers — [`DSH2163`](../diagnostics/DSH2xxx.md#dsh2163) otherwise |
| Initializers | each declarator may carry its own `= <expression>` or `= { … }`, or none |
| Source location | each declarator reports its own line and column *(since 2.0.0)* |

`float a = 1, b, c = 3;` declares three 1-component values: `a` initialized to `1`, `b` to `0`,
`c` to `3`.

## Brace initializers

A brace list as the initializer of a declaration is rewritten to a constructor call of the declared
type: `T x = { a, b };` is `T x = T(a, b);`. Brace initializers are therefore **exactly** constructor
calls and inherit every rule of [Constructors](constructors.md): positional arguments only
([`DSH4225`](../diagnostics/DSH4xxx.md#dsh4225)), a single scalar spreads to every component, and
several arguments must add up to exactly the type's width
([`DSH4222`](../diagnostics/DSH4xxx.md#dsh4222)).

`{}` is special-cased: it is no initializer at all, so the variable gets the zero of its type.

The target type is always the declared type token. A brace list anywhere else — on the right of an
assignment or a member write, as an argument — is [`DSH2162`](../diagnostics/DSH2xxx.md#dsh2162)
*(since 2.0.0)*. A texture, `Substrate` or `MaterialAttributes` type has no constructor:
`Texture2D t = {x};` is [`DSH4223`](../diagnostics/DSH4xxx.md#dsh4223).

> [!WARNING]
> Nested braces do not work. `float4 m = {{1,2},{3,4}};` is
> [`DSH2214`](../diagnostics/DSH2xxx.md#dsh2214); write `float4(float2(1, 2), float2(3, 4))`.

```c
Graph = {
    float r = 1.0;
    float g = 0.5;
    float b = 0.2;
    vec3 rgb  = {r, g, b};      // -> vec3(r, g, b)
    vec4 rgba = {rgb, 1.0};     // -> vec4(rgb, 1.0), mixed-width packing
    Color = rgba.rgb;
}
```

## Redeclaration

A declaration whose name is already declared in the same block is
[`DSH4220`](../diagnostics/DSH4xxx.md#dsh4220). So is a declaration inside an `if` or `else` body of a
name the enclosing body already declares, and in a `Shader` a declaration named `Base`, which is the
material itself.

The check compares names **exactly** *(since 2.0.0)*: `float a = 1; float A = 2;` declares two
variables, where 1.x reported a redeclaration. A read of `a` or `A` finds its own variable; a read
in a case that matches neither exactly finds a unique case-insensitive match, with the warning
[`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275).

Assignment to an existing name is not a redeclaration — omit the type token to reassign. See
[Statements](statements.md#assignment).

## Scope

Each `if` and `else` body is a block with a scope of its own *(since 2.0.0)*; 1.x had one value map
for the whole body.

| Situation | Visibility after the statement |
| :-- | :-- |
| Declaration at body level | visible for the rest of the body. In a `Shader` that body also holds the `Outputs` declarations, before the `Graph` statements |
| Declaration inside an `if` or `else` body | visible to the end of that body only |
| A property read inside a branch | a property is a file-scope input, never a branch value |

| Case | Outcome |
| :-- | :-- |
| a variable declared **before** the `if` and assigned in a branch | its two values are merged into one conditional node, and the variable keeps the merged value after the `if` — see [if / else](if.md) |
| a variable declared **inside** a branch | dropped when the branch ends; nothing is merged, and a read after the `if` is [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200) |
| a branch declares a name the enclosing body already declares | `DSH4220` |

> [!WARNING]
> 1.x merged a name declared in both branches and let it be read after the `if`. That name is now
> branch-local. Declare the variable **before** the `if` and assign it in each branch:
>
> ```c
> float3 Blend = float3(0.0, 0.0, 0.0);
> if (Mask > 0.5) { Blend = float3(1.0, 0.0, 0.0); }
> else            { Blend = float3(0.25, 0.25, 0.25); }
> Color = Blend;
> ```

A branch-local name may be declared again after the `if`: its scope has ended.

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH4201` | the type token does not resolve |
| `DSH2215` | a texture, `SamplerState` or `Substrate` variable has no initializer |
| `DSH4220` | the name is declared already in this block, by the enclosing body, or is `Base` in a `Shader` |
| `DSH4228` | the initializer does not fit the declared type |
| `DSH5289` | *info:* a wider initializer was cut to its leading components |
| `DSH2154` | a declaration does not end with `;`, or its name is not one identifier |
| `DSH2163` | a declarator after `,` is not a name |
| `DSH2162` | a brace list outside a declaration's initializer |
| `DSH2214` | a brace list inside a brace initializer |
| `DSH4222` | a brace initializer's components do not add up to the type's width |
| `DSH4223` | a brace initializer on a type with no constructor |
| [`DSH2212`](../diagnostics/DSH2xxx.md#dsh2212) | `static` or `const` on a Graph variable |
| [`DSH2213`](../diagnostics/DSH2xxx.md#dsh2213) | an array declarator, `float w[4];` |
| [`DSH5294`](../diagnostics/DSH5xxx.md#dsh5294) | a `Substrate` node on an engine that does not have it |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="DreamShaderTests/Corpus/M_Declarations")
{
    Properties = {
        vec4  Src = vec4(0.1, 0.2, 0.3, 0.4);
        float K   = 2.0;
    }
    Settings = { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs  = { vec3 Color; Base.EmissiveColor = Color; }

    Graph = {
        float a = 1.0, b = 0.5, c;      // comma declarators; 'c' is 0
        vec3  rgb  = Src.rgb;           // swizzle of a property
        vec4  full = {rgb, a};          // brace initializer -> vec4(rgb, a)
        vec3  lit;                      // float3(0, 0, 0)

        if (K > 1.0) {
            lit = full.rgb * b;         // assignment, not declaration: 'lit' exists before the if,
        } else {                        // so both branches change the same variable and merge
            lit = full.rgb * c;
        }

        Color = lit;
    }
}
```

## See also

- [Statements](statements.md) — every statement form and the classification order
- [Type tokens](../language/types.md) — the full per-context validity matrix
- [Constructors](constructors.md) — the rules a brace initializer inherits
- [Conversions](conversions.md) — narrowing, splatting, and where each applies
- [Expressions](expressions.md) — what may appear on the right of `=`
- [if / else](if.md) — branch execution and the merge
- [Name resolution](name-resolution.md) — exact and case-insensitive lookup
- [MaterialAttributes](material-attributes.md) — declaring and writing attribute values
- [Swizzles](swizzle.md) — how a swizzle affects a declared value's width
- [Node reuse](node-reuse.md) — why two identical initializers share one node
- [Unsupported constructs](unsupported.md) — `return`, `for`, `+=` and their codes
- [Output bindings](../language/output-bindings.md) — `Outputs` declarations and their initializers
- [Diagnostics index](../diagnostics/index.md) — every code, by stage
