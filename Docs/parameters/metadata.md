# Metadata block

> [DreamShader](../index.md) » [Parameters](index.md) » **Metadata block**

The trailing `[ … ]` block of a declaration: a list of `Key = Value` entries that set the generated
node's organization fields and, for any key the parser does not recognize, write directly to a
reflected UPROPERTY of that node's class.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — after a `Properties` declaration, or after an `Inputs` / `Outputs` / `Results` typed parameter |
| Kind | declaration suffix |
| Generates | property writes on the node the declaration generated |
| Since | `1.2.3`; semicolon-separated form since `1.2.4`; `Slider(min, max)` since `1.5.0` |

## Synopsis

```c
<declaration> [ <entry> ; <entry> ; … ] ;
<declaration> [ <entry> , <entry> , … ] ;
```

```text
<entry> := <key> = <value>
         | Slider( <min> , <max> )
```

Every `[`, `]`, `;`, `,`, `=` and `( )` above is **literal DreamShaderLang punctuation**, not
meta-notation. `;` and `,` may be mixed within one block, and a trailing separator before `]` is
accepted. `Slider(…)` is the only entry form without an `=`.

Placement rules:

| Rule | Behaviour |
| :-- | :-- |
| The block must be the **last** thing in the statement | a statement that does not end with `]` has no metadata at all — the `[ … ]` is left in the declaration text and fails elsewhere |
| The opening `[` is found at paren-depth 0 and bracket-depth 0 | brackets inside `( … )` and inside string literals are safe |
| Text before the block must be non-empty | otherwise `Metadata must follow a declaration.` |
| Entries are split on `;` **or** `,` at top level | mixing both in one block is accepted |
| A trailing `;` or `,` before `]` | accepted; the empty entry is dropped |
| Keys are trimmed and lower-cased before comparison | `[ Group = "X" ]`, `[group="X"]` and `[GROUP="X"]` are the same entry |
| Values are unquoted, then trimmed | `"  X  "` becomes `X`; an unquoted value is trimmed as written |
| A duplicate key (after lower-casing) | `Metadata key '{Key}' is declared more than once.` |

## Recognized keys

| Key | Aliases | Value | Effect |
| :-- | :-- | :-- | :-- |
| `Group` | `Category` | string | The node's parameter group; also the `Group` UPROPERTY (an `FName`) |
| `Description` | `Desc`, `Tooltip` | string | Written to the node's **`Desc`** UPROPERTY |
| `SortPriority` | `Sort` | integer | The node's `SortPriority`; a non-integer is an error |
| `ParameterName` | — | string | Overrides the material parameter name; the declared identifier is used when absent |
| `Slider(min, max)` | — | two numbers, no `=` | Expands to the reflected properties `SliderMin` and `SliderMax` |
| *anything else* | — | see below | Written to the same-named reflected UPROPERTY of the generated node's class |

All key comparisons are case-insensitive. Every entry — including the recognized ones — is *also*
stored under its lower-cased key and pushed through the reflected-property writer, so the recognized
keys are exactly the ones with an extra typed effect, not a separate namespace.

`SortPriority` defaults to **32** when nothing sets it, matching Unreal's own node default.

### `Slider(min, max)`

```c
ScalarParameter Roughness = 0.5 [Slider(0, 1)];
```

Matched case-insensitively, must end with `)`, and the inner text must split on a top-level `,` into
**exactly two** numeric values. Both are written as reflected `SliderMin` / `SliderMax` properties, so
it is exactly equivalent to `[SliderMin = 0; SliderMax = 1]` — and combining the two forms in one
block is a duplicate:

| Written | Result |
| :-- | :-- |
| `[Slider(0, 1)]` | `SliderMin = 0`, `SliderMax = 1` |
| `[SliderMin = 0; SliderMax = 1]` | identical |
| `[Slider(0, 1); SliderMin = 0]` | `Metadata SliderMin/SliderMax is declared more than once (entry '{Entry}').` |
| `[Slider(0)]` / `[Slider(0, 1, 2)]` / `[Slider(a, b)]` | `Metadata 'Slider(min, max)' requires exactly two numeric bounds: '{Entry}'.` |

`SliderMin` / `SliderMax` exist on `UMaterialExpressionScalarParameter`. On a class without them the
entry fails as an unreflected property.

### `ParameterName`

```c
ScalarParameter Rough [ParameterName = "Surface Roughness"];
```

The declared identifier stays the name the `Graph` uses; `ParameterName` changes only the name the
material exposes to instances and Blueprints. An empty value falls back to the declared name.

> [!NOTE]
> `ParameterName` is *also* left in the reflected-property list, so it is written twice — once as the
> node's parameter name and once through reflection. That is harmless on a
> `UMaterialExpressionParameter` subclass. On a class with no `ParameterName` UPROPERTY —
> `DynamicParameter` is the one in the token set — `[ParameterName="…"]` is a **hard error**, not a
> warning.

## Alias rewriting and auto-injection

Before anything is written, the entry list is completed and its keys rewritten:

| Step | Behaviour |
| :-- | :-- |
| 1 | `Group` is injected as key `group` unless `Group` **or** `Category` was typed literally |
| 2 | `SortPriority` is injected unless `SortPriority` **or** `Sort` was typed literally |
| 3 | `Description` is injected as key `desc` unless `Description`, `Desc` **or** `Tooltip` was typed literally |
| 4 | keys are rewritten to their real UPROPERTY names, per the table below |

| Typed key | Reflected UPROPERTY |
| :-- | :-- |
| `Description` | `Desc` |
| `Tooltip` | `Desc` |
| `Category` | `Group` |
| `Sort` | `SortPriority` |
| anything else | passes through as the lower-cased key |

Injection in steps 1–3 is what applies a value that came from somewhere other than this block — most
often the enclosing [`Group("Name") { … }` scope](#group-scopes-and-the-sortpriority-counter).

## Reflected property passthrough

Any key that is not `Slider(…)` is resolved to an `FProperty` on the generated node's class:

1. the key is trimmed, lower-cased and compared against each UPROPERTY name, case-insensitively;
2. if that fails, a second pass strips a leading `b` from every `FBoolProperty` name and compares
   again.

So `[FractionalPart = true]` binds `bFractionalPart`, and `[UseCustomPrimitiveData = true]` binds
`bUseCustomPrimitiveData`. Writing the `b` explicitly also works.

### Value grammar by property type

| Property type | Accepted text | Error on failure |
| :-- | :-- | :-- |
| `bool` | `true` / `false`, case-insensitive | `'{Value}' is not a valid boolean value for '{Property}'.` |
| `int32` | a signed 32-bit integer | `'{Value}' is not a valid integer value for '{Property}'.` |
| `uint32` | an integer in `[0, 4294967295]` | `'{Value}' is not a valid unsigned integer value for '{Property}'.` |
| `float` | a number, or `true` / `false` (`1.0` / `0.0`) | `'{Value}' is not a valid numeric value for '{Property}'.` |
| `double` | as `float` | `'{Value}' is not a valid numeric value for '{Property}'.` |
| `FString` | anything, verbatim after trimming | — |
| `FName` | anything, converted to an `FName` | — |
| object reference | `Path(…)`, an absolute `/…` path, or a bare path — see [Path(…)](path.md#two-resolvers) | see [below](#object-properties) |
| `enum` | an enum literal, four spellings — see [below](#enum-literals) | `'{Value}' is not a valid enum value for '{Property}'.` |
| `uint8` backed by an enum | an enum literal | `'{Value}' is not a valid enum value for '{Property}'.` |
| plain `uint8` | an integer in `[0, 255]` | `'{Value}' is not a valid byte value for '{Property}'.` |
| anything else (structs, arrays, …) | Unreal's own import text, e.g. `(R=1,G=0,B=0,A=1)` | `Property '{Property}' on '{Class}' is not a supported literal type yet.` |

### Enum literals

An enum value is normalized by trimming, lower-casing and then removing every space, `_`, `-`, `:`,
`.` and `/`. Each enumerator that is **not** tagged `UMETA(Hidden)` is tried against four spellings:

| # | Spelling | Example for `SAMPLERTYPE_LinearColor` |
| :-- | :-- | :-- |
| 1 | short name | `SAMPLERTYPE_LinearColor` |
| 2 | fully qualified name | `EMaterialSamplerType::SAMPLERTYPE_LinearColor` |
| 3 | `DisplayName` metadata | `Linear Color` |
| 4 | short name with everything up to the first `_` removed | `LinearColor` |

Because of the normalization, `"LinearColor"`, `"linear color"`, `"linear-color"`,
`"SAMPLERTYPE_LinearColor"` and `"linear.color"` are all the same value. See
[SamplerType](sampler-type.md) for the fully expanded table of one such enum.

### Object properties

An object-valued UPROPERTY takes an asset reference resolved by the metadata resolver documented in
[Path(…)](path.md#two-resolvers).

| Situation | Result |
| :-- | :-- |
| Value starts with `Path(` or `/` and does not resolve | the resolver's own message is reported |
| Value is neither and does not resolve | `Object property '{Property}' expects Path(...) or an absolute Unreal object path.` |
| Asset loads but is the wrong class | `Asset '{Path}' is not compatible with '{Property}'. Expected '{Class}'.` |
| Asset fails to load | `Failed to load asset '{Path}' for '{Property}'.` |

> [!WARNING]
> **A texture that fails to load is silently accepted.** When the property is a `UTexture` subclass
> *and* is named exactly `Texture` or `TextureObject`, a failed load writes `nullptr` and the write is
> reported as successful. `[Texture = Path(Game, "Typo")]` therefore generates without any diagnostic
> and leaves an unbound sampler, which then fails at shader compile. Check the asset path when a
> sampler comes out empty.

## Organization fields that are not reflected

`Group`, `SortPriority` and `Desc` exist on `UMaterialExpressionParameter` subclasses. Not every
parameter node is one — `UMaterialExpressionDynamicParameter` is not. When one of those three fields
is missing from the class it is **skipped with a warning**, not an error:

```text
'{Class}' does not expose the '{Field}' organization field; ignoring it for this parameter.
```

Any *other* unresolved key is a hard error:

```text
Metadata property '{Key}' is not a reflected property on '{Class}'.
```

> [!NOTE]
> The practical consequence: `DynamicParameter Dyn = float4(0,0,0,0) [Group="S"; SortPriority=10];`
> compiles, logs two warnings, and the node ends up in no group. Its default value *is* applied, and
> its parameter name is written to `ParamNames[0]`.

## `Group(…)` scopes and the SortPriority counter

A [`Group("Name") { … }` scope](../language/properties.md) in a `Properties` section stamps its
members. The interaction with an explicit metadata entry is:

| Situation | Result |
| :-- | :-- |
| Member typed neither `Group` nor `Category` | the enclosing group is injected |
| Member typed `Group` or `Category` | the typed value wins; the scope is ignored for that member |
| Nested scopes | composed with `\|` — `Group("Outer") { Group("Inner") { … } }` yields `Outer\|Inner` |
| A literal `Group("Manual\|Literal")` | passes through unchanged |
| Member typed neither `SortPriority` nor `Sort` | it receives the next value from an auto counter |
| Member typed `SortPriority` or `Sort` | the typed value wins **and does not consume a counter slot** |
| Declaration outside any scope | untouched — no group, and no auto sort priority |

The counter starts at **0** and steps by **10**. It is **shared across every scope in the block**, not
reset per group:

```c
Properties {
    Group("Surface") {
        ScalarParameter A = 0.5;                   // SortPriority = 0
        VectorParameter B = float4(1, 1, 1, 1);    // SortPriority = 10
    }
    Group("Detail") {
        ScalarParameter C = 1.0 [SortPriority=99]; // 99 — does not consume a slot
        ScalarParameter D = 2.0;                   // SortPriority = 20
    }
    ScalarParameter Loose = 3.0;                   // no group, no auto sort
}
```

## Notes

- The same `[ … ]` block is accepted on `Inputs` / `Outputs` / `Results` typed parameters, but there
  only `Description` / `Desc` / `Tooltip` and `SortPriority` / `Sort` have any effect. `Group` is
  parsed and retained but never applied — Unreal's function input/output nodes have no group field —
  and every other key is ignored rather than reflected. An input's `SortPriority` defaults to its
  declaration index. See [Inputs / Outputs / Results](../language/inputs-outputs.md).
- **No metadata block is accepted on a `Shader`'s `Outputs` statements**, nor on `Settings`, `Options`
  or `Layout` entries.
- A `const` declaration still accepts metadata: `[Desc="…"]` on a `Constant` node works.
- Metadata is applied **after** the node's default value and after `AutoSetSampleType()`, so an
  explicit `[SamplerType=…]` overrides the inferred one.
- `_MAX` sentinels of engine enums usually carry no `Hidden` metadata, so a value such as
  `"SAMPLERTYPE_MAX"` resolves rather than erroring. Do not use them.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

### Parse time

| Message | Cause |
| :-- | :-- |
| `Metadata must follow a declaration.` | the statement is only a `[ … ]` block |
| `Metadata entry '{Entry}' must use Key=Value syntax.` | an entry with no top-level `=` that is not `Slider(…)` |
| `Invalid metadata entry '{Entry}'.` | the key is empty after normalization |
| `Metadata key '{Key}' is declared more than once.` | a duplicate key; the message quotes the spelling as typed |
| `Metadata 'Slider(min, max)' requires exactly two numeric bounds: '{Entry}'.` | wrong arity or a non-numeric bound |
| `Metadata SliderMin/SliderMax is declared more than once (entry '{Entry}').` | `Slider(…)` combined with an explicit `SliderMin` / `SliderMax` |
| `Metadata SortPriority value '{Value}' is not an integer.` | a non-integer `SortPriority` / `Sort` |

### Generation time

| Message | Cause |
| :-- | :-- |
| `Metadata property '{Key}' is not a reflected property on '{Class}'.` | no UPROPERTY matched, and the key is not one of the three organization fields |
| `Metadata property '{Key}' on '{Class}': {Inner}` | the property was found but the value could not be converted |
| `'{Class}' does not expose the '{Field}' organization field; ignoring it for this parameter.` | **warning** — `Group`, `SortPriority` or `Desc` is missing from the class |
| `'{Class}' does not expose a ParameterName property.` | `ParameterName` written on a class without that UPROPERTY |
| `'{Value}' is not a valid boolean value for '{Property}'.` | boolean UPROPERTY, non-boolean text |
| `'{Value}' is not a valid integer value for '{Property}'.` | `int32` UPROPERTY |
| `'{Value}' is not a valid unsigned integer value for '{Property}'.` | `uint32` UPROPERTY, or out of range |
| `'{Value}' is not a valid numeric value for '{Property}'.` | `float` or `double` UPROPERTY |
| `'{Value}' is not a valid byte value for '{Property}'.` | plain `uint8` UPROPERTY, or out of `[0, 255]` |
| `'{Value}' is not a valid enum value for '{Property}'.` | no enumerator matched under any of the four spellings |
| `Object property '{Property}' expects Path(...) or an absolute Unreal object path.` | an object UPROPERTY given text that is neither |
| `Asset '{Path}' is not compatible with '{Property}'. Expected '{Class}'.` | the asset loaded but is the wrong class |
| `Failed to load asset '{Path}' for '{Property}'.` | the asset could not be loaded (except the silent-null texture case above) |
| `Property '{Property}' on '{Class}' is not a supported literal type yet.` | a struct or container UPROPERTY whose import text was rejected |
| `property '{Name}': {Inner}` | wrapper applied to every metadata failure, naming the declaration |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Docs/M_Metadata")
{
    Properties = {
        Group("11 - Specular") {
            TextureSampleParameter2D MetallicMap = Path(Game, "Textures/T_White_Linear") [
                SamplerType          = "LinearColor";
                SamplerSource        = "FromTextureAsset";
                MipValueMode         = "None";
                AutomaticViewMipBias = true;
                ConstCoordinate      = 0;
                ConstMipValue        = -1;
                Description          = "Packed metallic / roughness";
            ];

            ScalarParameter Metallic = 0.0 [Slider(0, 1); SortPriority = 51];
        }

        VectorParameter Tint = float4(1, 1, 1, 1) [
            Category               = "Look",
            Tooltip                = "Multiplied over base colour",
            UseCustomPrimitiveData = false,
            ParameterName          = "Base Tint"
        ];
    }

    Settings = { Domain = "Surface"; ShadingModel = "DefaultLit"; BlendMode = "Opaque"; }
    Outputs  = { vec3 Color; float M; Base.BaseColor = Color; Base.Metallic = M; }

    Graph = {
        vec4 S = MetallicMap(Coordinates = UE.TexCoord(Index = 0));
        Color = S.rgb * Tint.rgb;
        M     = S.b * Metallic;
    }
}
```

Applied properties:

```text
MetallicMap  Group="11 - Specular"  SortPriority=0   Desc="Packed metallic / roughness"
             SamplerType=SAMPLERTYPE_LinearColor     SamplerSource=SSM_FromTextureAsset
             MipValueMode=TMVM_None  bAutomaticViewMipBias=true
             ConstCoordinate=0       ConstMipValue=-1
Metallic     Group="11 - Specular"  SortPriority=51  SliderMin=0  SliderMax=1
Tint         Group="Look"           Desc="Multiplied over base colour"
             bUseCustomPrimitiveData=false           ParameterName="Base Tint"
```

`Metallic` takes `SortPriority = 51` from its own entry, so it does not consume a counter slot;
`MetallicMap` took slot 0.

## See also

- [Parameters](index.md) — the hub and the decision table
- [Compact type tokens](compact-types.md) — the tokens a metadata block can follow
- [Parameter node tokens](parameter-nodes.md) — the class-specific keys each token exposes
- [SamplerType](sampler-type.md) — the fully expanded value table for one reflected enum
- [Path(…)](path.md) — the asset-reference grammar object properties accept
- [Properties (section)](../language/properties.md) — `Group("Name") { … }` scopes and ordering
- [Inputs / Outputs / Results](../language/inputs-outputs.md) — where the same block has a reduced effect
- [Decompiler](../tools/decompiler.md) — which metadata keys a round trip always emits
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
