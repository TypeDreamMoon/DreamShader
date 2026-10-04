# Properties

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Properties**

A section that declares the parameter nodes, constant nodes and `UE.*` builtin nodes a block
generates into its material graph.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` |
| Kind | section |
| Generates | the nodes of the declarations that the graph actually reads |
| Aliased | inside a `VirtualFunction`, `Properties` is a **synonym for `Inputs`** — see [Notes](#notes) |

In 2.0 a property is a `uniform` (or a `static const`) with its metadata in `///` lines — see
[DreamShaderLang 2.0](../language-v2/index.md); [`dsc migrate`](../tools/migrate.md) rewrites a section
that way.

## Synopsis

```c
Properties [=]
{
    <property-declaration>…
    <group-scope>…
}
```

```c
property-declaration := [ const ] <type-token> <name> [ = <default> ] [ [ <metadata> ] ] ;

group-scope          := Group( "<group-name>" ) { { <property-declaration> | <group-scope> }… } [ ; ]
```

In `property-declaration` the innermost `[ … ]` pair around `<metadata>` is **literal
DreamShaderLang punctuation**; the outer pair is the meta-syntax for "optional". A declaration with a
metadata block therefore looks like `float Roughness = 0.5 [Group="Surface"];`.

The `=` between the section name and its `{ … }` block is optional sugar *(since 1.5.0)*. The section
name is matched case-insensitively. A `;` after the section's closing `}` is optional, and so is the
`;` of the last declaration before it.

## Declaration members

| Member | Required | Form | Description |
| :-- | :-- | :-- | :-- |
| `const` | no | keyword | Generates a constant node instead of a parameter node. Any case. *(since 1.2.6)* |
| **`<type-token>`** | yes | token | Selects the generated node class and the default-value grammar. See [Type tokens](#type-tokens). |
| **`<name>`** | yes | identifier | The identifier the `Graph` reads, and — unless `[ParameterName=…]` overrides it — the material parameter name. |
| `= <default>` | no | literal | Initial value. The accepted literal grammar is chosen by the type token, not by the shape of the literal. |
| `[ <metadata> ]` | no | block | Group, sort priority, description, slider bounds, and arbitrary reflected `UMaterialExpression` properties. See [Metadata](../parameters/metadata.md). |

### Parse order

*(since 2.0.0)* A declaration is read token by token, left to right:

| Step | Reads | When it is missing or wrong |
| :-- | :-- | :-- |
| 1 | an optional `const` | — |
| 2 | the type: one word, or `UE.<Function>` with its `( … )` argument list | [`DSH3250`](../diagnostics/DSH3xxx.md#dsh3250); after `const`, [`DSH3251`](../diagnostics/DSH3xxx.md#dsh3251) |
| 3 | the name: one identifier | `DSH3250` |
| 4 | after `=`, the default: every token up to a `[` or `;` outside brackets | [`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254) when nothing follows the `=` |
| 5 | an optional `[ … ]` metadata block | [`DSH3255`](../diagnostics/DSH3xxx.md#dsh3255) |
| 6 | `;`, or the `}` that closes the section or group | `DSH3250` |

Whitespace of any kind separates the words, so a tab between type and name is fine, and a `UE.*`
builtin type with a spaced argument list reads as one type:

```c
UE.TexCoord(Index = 0) UV;      // type = UE.TexCoord(Index = 0), name = UV
```

> [!NOTE]
> *(since 2.0.0)* The name must be an identifier. 1.x checked only that it was non-empty, so
> `Properties { float 1Bad = 0; }` parsed and was unreachable; now `1Bad` is a malformed number
> ([`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105)) and the declaration is `DSH3250`. A keyword such as
> `in` or `out` cannot be a name either.

## Type tokens

Four token families are recognized, all matched case-insensitively. The complete catalogues live on
their own pages:

| Family | Tokens | Generated node | Reference |
| :-- | :-- | :-- | :-- |
| Compact scalar | `float` `half` `double` `int` `uint` `bool`, and each with a `1` suffix (`float1` …) | `UMaterialExpressionScalarParameter` | [Compact types](../parameters/compact-types.md) |
| Compact vector | `float2..4` `half2..4` `double2..4` `int2..4` `uint2..4` `bool2..4` `vec2..4` `ivec2..4` `uvec2..4` `bvec2..4` | `UMaterialExpressionVectorParameter` | [Compact types](../parameters/compact-types.md) |
| Compact texture | `Texture2D` `TextureCube` `Texture2DArray` `Texture3D` `VolumeTexture` — 5 tokens | `UMaterialExpressionTextureObjectParameter` | [Compact types](../parameters/compact-types.md) |
| Explicit parameter node | 22 `*Parameter` / sampler tokens | the named `UMaterialExpression` subclass | [Parameter nodes](../parameters/parameter-nodes.md) |
| `UE.<Function>[( … )]` | any builtin name, with an argument list | the builtin's node, or a reflected node when `OutputType=` is given | [UE builtins](../builtins/ue.md), [`UE.Expression`](../builtins/ue-expression.md) |

Anything else is [`DSH3252`](../diagnostics/DSH3xxx.md#dsh3252). Of the 22 explicit tokens, seven have
no 2.0 form and are [`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253) *(since 2.0.0)*:
`DoubleVectorParameter`, `TextureCollectionParameter`, `CurveAtlasRowParameter`, `DynamicParameter`,
`FontSampleParameter`, `SpriteTextureSampler`, `SparseVolumeTextureObjectParameter`. Of the others,
`ScalarParameter`, `VectorParameter`, `StaticBoolParameter` and `TextureObjectParameter` declare a
parameter like the compact tokens do; the remaining eleven become a node at every read of the
property, as in 1.x.

> [!NOTE]
> The `Properties` token set is **not** the same as the `Inputs` / `Outputs` token set.
> `MaterialAttributes`, `Substrate` and `SamplerState` are valid parameter types but are **not** valid
> `Properties` types; neither are the matrix spellings. `StaticBool` is read as `bool` here
> *(since 2.0.0)*, so it declares a dynamic bool parameter; a static switch parameter is
> `StaticBoolParameter`. The per-context validity matrix is on [Types](types.md).

An inline `= <default>` on a `UE.*` builtin property is
[`DSH3259`](../diagnostics/DSH3xxx.md#dsh3259) — its arguments go inside the parentheses instead.

### Texture defaults

A texture token's `= <default>` is an asset reference. Four spellings are accepted, and all four
resolve to the same object path:

```c
Texture2D A = Path(Game, "Textures/T_X");                      // root + relative path
Texture2D B = Path("/Game/Textures/T_X");                      // absolute path
Texture2D C = "/Game/Textures/T_X";                            // bare quoted absolute path
Texture2D D = Texture2D'/Game/Textures/T_X.T_X';               // Content Browser "Copy Reference" (since 1.9.0)
```

The last form is what **Copy Reference** puts on the clipboard, in either the
`/Script/Engine.Texture2D'…'` or the older `Texture2D'…'` spelling, and it may be pasted bare, in
quotes, or as the path argument of `Path(…)`. The class in front of the quotes is checked against the
declared texture type: pasting a `Texture2D'…'` into a `TextureSampleParameterVolume` is refused by
name ([`DSH1044`](../diagnostics/DSH1xxx.md#dsh1044); a class that is no texture at all is
[`DSH1043`](../diagnostics/DSH1xxx.md#dsh1043)) rather than left to fail at load. A reference that does
not resolve is [`DSH8271`](../diagnostics/DSH8xxx.md#dsh8271), and one that resolves to nothing
loadable [`DSH8218`](../diagnostics/DSH8xxx.md#dsh8218). See [`Path(…)`](../parameters/path.md).

## `const` properties

*(since 1.2.6)*

`const` replaces the parameter node with a constant node, so the value is baked into the material and
does not appear in the parameter list of an instance.

| Declared type | Generated node |
| :-- | :-- |
| scalar | `UMaterialExpressionConstant` |
| vector, 2 components | `UMaterialExpressionConstant2Vector` |
| vector, 3 components | `UMaterialExpressionConstant3Vector` |
| vector, 4 components | `UMaterialExpressionConstant4Vector` |
| texture | `UMaterialExpressionTextureObject` |

Rules:

- `const` is legal with the compact scalar, vector and texture tokens, and *(since 2.0.0)* with
  `ScalarParameter`, `VectorParameter` and `TextureObjectParameter`, which read as those. With any
  other parameter-node token it is `DSH3253`.
- A `const` without a default is zero. A `const` texture without a default is a `TextureObject` node
  with no texture of its own written *(since 2.0.0)*: 1.x fell back to the engine's default asset for
  the dimension, and refused `Texture2DArray`.
- A constant's value is the compiler's: an expression that combines it with other constants may fold
  into one constant node.
- **A `const` vector reads as its whole node output (index 0)** in `Graph`, whereas a non-`const`
  vector parameter reads through the named output matching its component count (`R`, `RG`, `RGB`,
  `RGBA`). See [Reading parameters in Graph](../parameters/graph-usage.md).

```c
Properties = {
    const float DebugScale = 1.0;                // UMaterialExpressionConstant
    float Strength         = 1.0;                // UMaterialExpressionScalarParameter
    vec3  Tint             = vec3(1.0, 1.0, 1.0);// UMaterialExpressionVectorParameter
}
```

## `Group("Name") { … }` scope blocks

*(since 1.5.0)*

A `Group` scope stamps its name onto every declaration it contains, so a shared group does not have
to be repeated in each declaration's metadata.

```c
Properties {
    Group("Surface") {
        ScalarParameter Roughness = 0.5 [Slider(0, 1)];
        VectorParameter BaseColor = float4(1, 1, 1, 1);
    }
}
```

### Head grammar

| Rule | Detail |
| :-- | :-- |
| Keyword | `Group`, matched case-insensitively |
| Argument list | `(` immediately after the keyword, holding exactly one double-quoted string, then `)` |
| Name | the string, trimmed; it must be non-empty — [`DSH3260`](../diagnostics/DSH3xxx.md#dsh3260) |
| Body | `{ … }` |
| Terminator | a single `;` after the closing `}` is optional |

`Group(…)` is the **only** construct that may open a `{` inside `Properties`. Any other `{` is a
diagnostic (`DSH3250`), and a group whose `}` is missing runs on to the end of the file
([`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150)).

### Nesting and `|` composition

Scopes nest to any depth. A nested scope's effective group name is the enclosing name, a `|`, and the
inner name — which is Unreal's own sub-category syntax for the `Group` property:

```c
Properties {
    Group("Surface") {
        float A = 0;                    // Group = "Surface"
        Group("Detail") {
            float B = 0;                // Group = "Surface|Detail"
            Group("Micro") {
                float C = 0;            // Group = "Surface|Detail|Micro"
            }
        }
    }
}
```

A literal `|` typed inside a group name passes through unchanged, so
`Group("Manual|Literal") { … }` produces `Manual|Literal` exactly.

### Inheritance precedence

The inherited group is applied to a member **only if** that member's metadata block typed neither
`Group` nor `Category`. An explicit key on the declaration always wins:

```c
Group("Surface") {
    float A = 0;                        // Group = "Surface"     (inherited)
    float B = 0 [Group="Override"];     // Group = "Override"    (explicit wins)
    float C = 0 [Category="Other"];     // Group = "Other"       (Category is the alias)
}
```

### Automatic `SortPriority`

Members of a group scope are auto-numbered by declaration order.

| Rule | Value |
| :-- | :-- |
| Counter start | `0` |
| Counter step | `10` |
| Counter scope | **one counter shared by every group in the same `Properties` section**, not one per group |
| Explicit `SortPriority` / `Sort` | wins, and **does not consume a slot** |
| Ungrouped (top-level) declarations | never auto-numbered, and never given a group |

```c
Properties {
    Group("Surface") {
        ScalarParameter A = 0.5;                    // SortPriority = 0
        VectorParameter B = float4(1, 1, 1, 1);     // SortPriority = 10
    }
    Group("Detail") {
        ScalarParameter C = 1.0 [SortPriority=99];  // SortPriority = 99, no slot consumed
        ScalarParameter D = 2.0;                    // SortPriority = 20  (counter continued)
    }
    ScalarParameter Loose = 3.0;                    // no group, no auto sort
}
```

> [!NOTE]
> The counter is seeded once per `Properties` section. A block that declares `Properties` twice gets
> a fresh counter starting at `0` in the second section, so two groups in two sections can end up
> with overlapping sort priorities.

When no group scope and no explicit metadata supply a value, a parameter gets `32`, the engine's own
default for parameter nodes — what 1.x left it at.

## Metadata

The trailing `[ … ]` block is shared with the typed-parameter sections. Its recognized keys are:

| Key | Aliases | Value |
| :-- | :-- | :-- |
| `Group` | `Category` | string |
| `Description` | `Desc`, `Tooltip` | string |
| `SortPriority` | `Sort` | whole number — [`DSH3258`](../diagnostics/DSH3xxx.md#dsh3258) otherwise |
| `Slider(min, max)` | — | shorthand with no `=`; the same range as `SliderMin` + `SliderMax` — [`DSH3257`](../diagnostics/DSH3xxx.md#dsh3257) when malformed or given twice |
| `ParameterName` | — | string; overrides the material parameter name |
| `SamplerType` | — | a sampler type; a `SAMPLERTYPE_` prefix is dropped |
| *anything else* | — | written to the same-named reflected `UMaterialExpression` property |

Entries are separated by `;` or `,`, and keys are matched ignoring case: a key written twice is
[`DSH3256`](../diagnostics/DSH3xxx.md#dsh3256). A slider range on anything but a scalar parameter is
ignored with the warning [`DSH7233`](../diagnostics/DSH7xxx.md#dsh7233). Full grammar, the reflection
rules and the enum-value spellings are on [Metadata](../parameters/metadata.md).

## Ordering and repetition

- Statements are `;`-terminated. The reader tracks `()`, `[]` and `{}`, and a string is one token, so
  a `Group` body and a bracketed metadata block never split a statement in the wrong place.
- Comments may stand anywhere between tokens.
- Sections may appear **in any order** inside a block, and a repeated `Properties` section
  **appends** to the previous one. There is no "declared twice" diagnostic at the section level.
- **Declaration order does not affect name resolution in `Graph`**: the `Graph` is read after every
  other section of the block, so a property declared below it is still visible. A parameter is built
  where the graph first reads it and shared by every later read, and a property nothing reads
  produces no node. The eleven parameter-node tokens without a parameter form are the exception: each
  read is its own node, as in 1.x.
- Declaration order *does* determine the auto-`SortPriority` counter.
- A `UE.*` property becomes a value at the head of the `Graph` body that reads it, built only when the
  body reads it, in declaration order.
- A name declared twice in one file is [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210). The check
  compares names exactly *(since 2.0.0)*; 1.x compared them ignoring case.

## Notes

- **`Properties` means something different inside a `VirtualFunction`.** There it is an alias for
  [`Inputs`](inputs-outputs.md) and gets the typed-parameter grammar (`opt`, no `const`, no node
  tokens) instead of the declaration grammar on this page. See [VirtualFunction](virtual-function.md).
- Where the property nodes land is the [graph layout](../generation/graph-layout.md)'s business; a
  [`Layout`](layout.md) section can pin them.
- *(since 2.0.0)* Every metadata key the table above does not name is written to the node by
  reflection. On a parameter a key the node class does not have is the warning
  [`DSH8210`](../diagnostics/DSH8xxx.md#dsh8210) and the value is not written — 1.x refused it, except
  for `Group`, `SortPriority` and `Desc`; a value the property cannot take is
  [`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213). On one of the eleven parameter-node tokens without a
  parameter form the keys are arguments of the node, and an unknown one is
  [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213) — or, when its value is a number, the info
  [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291) followed by [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212)
  once the built node has no such pin.
- Regeneration destroys hand edits to the generated nodes. See
  [Regeneration](../generation/regeneration.md).

## Diagnostics

Every diagnostic carries the line and column of the declaration it is about.

| Code | Raised when |
| :-- | :-- |
| `DSH3250` | the section is not opened with `{`; a statement has no type and name; the declaration does not end with `;`; a `{` that is not a `Group` scope |
| `DSH3251` | `const` with no type after it |
| `DSH3252` | the type is neither a compact token, a parameter-node token nor `UE.*` |
| `DSH3253` | a parameter-node token with no 2.0 form, or `const` before a parameter-node token that cannot be a constant |
| `DSH3254` | nothing after `=`, or a default the type cannot read: a scalar that is not a number or `true` / `false`, a vector that is not a `name(a, b, c, d)` literal, a static switch that is not `true` / `false` |
| `DSH3255` | a metadata block that is not closed, or an entry that is not `Key = Value` |
| `DSH3256` | a metadata key written twice |
| `DSH3257` | a malformed `Slider(…)`, or a slider range given twice |
| `DSH3258` | a `SortPriority` that is not a whole number |
| `DSH3259` | an unclosed `UE.*` argument list, or a default written after a `UE.*` property |
| `DSH3260` | `Group("")` |
| `DSH2150` | the file ends inside the section or a group |
| `DSH1043`, `DSH1044` | a texture default whose `Class'…'` shell is not a texture, or not the declared texture type |
| `DSH4210` | a name declared twice |
| `DSH7233` | warning: a slider range on a parameter that has no slider |
| `DSH8210` | warning: a metadata key the parameter node does not have |
| `DSH8213` | a metadata value the node property cannot take |
| `DSH8218`, `DSH8271` | a texture default that does not load, or does not resolve |

The complete list is in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Materials/M_Surface", Root="Game")
{
    Properties {
        Group("Surface") {
            VectorParameter BaseColor = float4(0.8, 0.8, 0.8, 1.0);   // SortPriority 0
            ScalarParameter Roughness = 0.55 [Slider(0, 1)];          // SortPriority 10
            Group("Advanced") {
                ScalarParameter Metallic = 0.0;                       // Group "Surface|Advanced", 20
            }
        }

        Group("Textures") {
            TextureSampleParameter2D AlbedoMap = Path(Game, "Textures/T_White_Linear") [
                SamplerType="LinearColor";
                MipValueMode="None";
                AutomaticViewMipBias=true;
            ];                                                        // SortPriority 30
        }

        const float DebugScale = 1.0;                                 // ungrouped, not auto-sorted
        UE.TexCoord(Index = 0) UV;                                    // ungrouped
    }

    Outputs {
        float3 Color;
        float  Rough;
        float  Metal;

        Base.BaseColor  = Color;
        Base.Roughness  = Rough;
        Base.Metallic   = Metal;
    }

    Graph {
        float4 Albedo = AlbedoMap(Coordinates = UV);
        Color = Albedo.rgb * BaseColor.rgb * DebugScale;
        Rough = Roughness;
        Metal = Metallic;
    }
}
```

Resulting parameter organization:

```text
Surface           SortPriority  0   BaseColor   VectorParameter
Surface           SortPriority 10   Roughness   ScalarParameter, slider 0..1
Surface|Advanced  SortPriority 20   Metallic    ScalarParameter
Textures          SortPriority 30   AlbedoMap   TextureSampleParameter2D

DebugScale  -> UMaterialExpressionConstant           not a parameter; ungrouped, not auto-sorted
UV          -> UMaterialExpressionTextureCoordinate  ungrouped, not auto-sorted
```

## See also

- [Shader](shader.md) — the block whose `Properties` become material parameters
- [ShaderFunction](shader-function.md) — function-local `Properties` *(since 1.2.6)*
- [VirtualFunction](virtual-function.md) — where `Properties` instead aliases `Inputs`
- [Inputs / Outputs / Results](inputs-outputs.md) — the typed-parameter section grammar
- [Types](types.md) — the full type-token catalogue and per-context validity matrix
- [Compact types](../parameters/compact-types.md) — every compact token and the node it generates
- [Parameter nodes](../parameters/parameter-nodes.md) — all 22 explicit `*Parameter` tokens
- [Metadata](../parameters/metadata.md) — the `[ … ]` block, `Slider(…)`, reflected passthrough
- [Sampler type](../parameters/sampler-type.md) — `SamplerType` values and spellings
- [`Path(...)`](../parameters/path.md) — asset-reference grammar for texture defaults
- [Reading parameters in Graph](../parameters/graph-usage.md) — bare reads and the pin call form
- [UE builtins](../builtins/ue.md) — every `UE.*` name accepted as a property type
- [Output bindings](output-bindings.md) — connecting graph values to material properties
- [Layout](layout.md) — pinning the generated node positions
- [DreamShaderLang 2.0](../language-v2/index.md) — `uniform`s and `///` metadata, the `.dss` spelling
- [Diagnostics index](../diagnostics/index.md) — every code
