# Metadata block

> [DreamShader](../index.md) » [Parameters](index.md) » **Metadata block**

The trailing `[ … ]` block of a declaration: a list of `Key = Value` entries that set the generated
node's organization fields and, for any key the language does not recognize, write directly to a
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
| The block comes after the name and the default, right before `;` | a `[` at depth zero ends the default; brackets inside `( … )` and inside string literals belong to the default |
| A statement that is only a `[ … ]` block | [`DSH3250`](../diagnostics/DSH3xxx.md#dsh3250) |
| Entries are separated by `;` **or** `,` | mixing both in one block is accepted |
| A trailing `;` or `,` before `]` | accepted |
| Keys are compared ignoring case | `[ Group = "X" ]`, `[group="X"]` and `[GROUP="X"]` are the same entry |
| Values are unquoted | a quoted value loses its quotes and its escapes are read; an unquoted value is the text as written |
| A key that is not a name, a key with no `=`, an `=` with no value, a missing `]` | [`DSH3255`](../diagnostics/DSH3xxx.md#dsh3255) |
| A duplicate key (ignoring case) | [`DSH3256`](../diagnostics/DSH3xxx.md#dsh3256) |

What a key does depends on the form the property takes *(since 2.0.0)*: on a **declaration** (every
compact token, `ScalarParameter`, `StaticBoolParameter`, `VectorParameter`, `TextureObjectParameter`)
the entries become the 2.0 `///` directives of a `uniform`; on a parameter node **expanded at its uses**
(`StaticSwitchParameter`, the two masks, the texture-sample tokens) they become arguments of the
reflected call. See [Parameter node tokens](parameter-nodes.md#the-22-tokens).

## Recognized keys

| Key | Aliases | Value | Effect |
| :-- | :-- | :-- | :-- |
| `Group` | `Category` | string | The node's parameter group (`Group` UPROPERTY, an `FName`) |
| `Description` | `Desc`, `Tooltip` | string | Written to the node's **`Desc`** UPROPERTY |
| `SortPriority` | `Sort` | integer | The node's `SortPriority`; a non-integer is [`DSH3258`](../diagnostics/DSH3xxx.md#dsh3258) |
| `ParameterName` | — | string | Overrides the material parameter name; the declared identifier is used when absent |
| `SamplerType` | — | enum literal | The texture node's sampler type — see [SamplerType](sampler-type.md) |
| `Slider(min, max)` | — | two numbers, no `=` | `SliderMin` and `SliderMax` |
| *anything else* | — | see below | Written to the same-named reflected UPROPERTY of the generated node's class |

All key comparisons are case-insensitive. *(since 2.0.0)* A recognized key does only its own job; in
1.x every entry was also pushed through the reflected-property writer.

`SortPriority` is **32** when nothing sets it, matching Unreal's own node default. A declaration
outside any `Group(…)` scope gets that 32 written out.

### `Slider(min, max)`

```c
ScalarParameter Roughness = 0.5 [Slider(0, 1)];
```

Matched case-insensitively; the parentheses must hold **exactly two** numbers. It is exactly
equivalent to `[SliderMin = 0; SliderMax = 1]` — and combining the two forms in one block is a
duplicate:

| Written | Result |
| :-- | :-- |
| `[Slider(0, 1)]` | `SliderMin = 0`, `SliderMax = 1` |
| `[SliderMin = 0; SliderMax = 1]` | identical |
| `[Slider(0, 1); SliderMin = 0]` | [`DSH3257`](../diagnostics/DSH3xxx.md#dsh3257) |
| `[Slider(0)]` / `[Slider(0, 1, 2)]` / `[Slider(a, b)]` | `DSH3257` |
| `[Slider(1, 0)]` / `[Slider(1, 1)]` | [`DSH7220`](../diagnostics/DSH7xxx.md#dsh7220) — the minimum must be below the maximum *(since 2.0.0)* |

A slider belongs to a scalar parameter. On any other declaration — a vector, a texture, a static bool —
the range is dropped with a [`DSH7233`](../diagnostics/DSH7xxx.md#dsh7233) warning *(since 2.0.0)*. A
lone `SliderMin` or `SliderMax` is written by reflection like any other key. On an expanded token,
whose classes have no slider, the bounds are refused when the material is built
([`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212)).

### `ParameterName`

```c
ScalarParameter Rough [ParameterName = "Surface Roughness"];
```

The declared identifier stays the name the `Graph` uses; `ParameterName` changes only the name the
material exposes to instances and Blueprints. On a declaration an empty value is
[`DSH7227`](../diagnostics/DSH7xxx.md#dsh7227) *(since 2.0.0)*.

> [!NOTE]
> *(since 2.0.0)* `ParameterName` is written once, by reflection. A class with no `ParameterName`
> UPROPERTY would get a [`DSH8210`](../diagnostics/DSH8xxx.md#dsh8210) warning; none of the parameter
> node tokens that build is such a class (`DynamicParameter`, the one 1.x refused here, is
> [`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253)).

## Alias rewriting and auto-injection

The typed key decides what is written:

| Typed key | On a declaration | On an expanded token |
| :-- | :-- | :-- |
| `Group`, `Category` | `@group` → `Group` | argument `Group` |
| `Description`, `Desc`, `Tooltip` | `@desc` → `Desc` | argument `Desc` |
| `SortPriority`, `Sort` | `@sort` → `SortPriority` | argument `SortPriority` |
| `ParameterName` | `@name` | argument `ParameterName` |
| `SamplerType` | `@sampler`, a `SAMPLERTYPE_` prefix removed | argument `SamplerType` |
| `Slider(a, b)`, or `SliderMin` and `SliderMax` together | `@slider a b` | arguments `SliderMin`, `SliderMax` |
| anything else | its lower-case spelling, written by reflection | an argument of that name |

On a declaration, the enclosing [`Group("Name") { … }` scope](#group-scopes-and-the-sortpriority-counter)
injects `Group` unless `Group` or `Category` was typed, and the automatic `SortPriority` unless
`SortPriority` or `Sort` was typed. An expanded token gets neither.

## Reflected property passthrough

On a **declaration**, a key that is not recognized is resolved to an `FProperty` on the generated
node's class when the material is built:

1. the key is compared against each UPROPERTY name, ignoring case;
2. if that fails, a second pass strips a leading `b` from every `FBoolProperty` name and compares
   again.

So `[FractionalPart = true]` binds `bFractionalPart`, and `[UseCustomPrimitiveData = true]` binds
`bUseCustomPrimitiveData`. Writing the `b` explicitly also works. A key no property matches is a
[`DSH8210`](../diagnostics/DSH8xxx.md#dsh8210) **warning** and is not written *(since 2.0.0; 1.x
refused it)*.

On an **expanded token**, a key is matched first against the class's input pins, then against its
properties, as the engine catalog lists them (with the `b`-less spelling of a boolean as an alias); a
match that differs only in case is [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276). A key that is
neither is [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213) — or, when its value is a number, it is taken
for a pin named after the node's properties ([`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291), info) and
refused when the material is built if the node has no such pin (`DSH8212`).

### Value grammar by property type

| Property type | Accepted text | Code on failure |
| :-- | :-- | :-- |
| `bool` | `true` / `false`, case-insensitive | [`DSH7132`](../diagnostics/DSH7xxx.md#dsh7132) |
| `int32` | an integer | [`DSH7133`](../diagnostics/DSH7xxx.md#dsh7133) |
| `uint32` | an integer in `[0, 4294967295]` | [`DSH7134`](../diagnostics/DSH7xxx.md#dsh7134) |
| `float` | a number — `true` / `false` are not numbers here | [`DSH7135`](../diagnostics/DSH7xxx.md#dsh7135) |
| `double` | as `float` | [`DSH7136`](../diagnostics/DSH7xxx.md#dsh7136) |
| `FString` | anything, verbatim after trimming | — |
| `FName` | anything, converted to an `FName` | — |
| object reference | `Path(…)`, an absolute `/…` path, or a `Class'…'` reference — see [Path(…)](path.md#two-resolvers) | see [below](#object-properties) |
| `enum` | an enum literal, four spellings — see [below](#enum-literals) | [`DSH7140`](../diagnostics/DSH7xxx.md#dsh7140) |
| `uint8` backed by an enum | an enum literal | [`DSH7141`](../diagnostics/DSH7xxx.md#dsh7141) |
| plain `uint8` | an integer in `[0, 255]` | [`DSH7142`](../diagnostics/DSH7xxx.md#dsh7142) |
| anything else (structs, arrays, …) | Unreal's own import text, e.g. `(R=1,G=0,B=0,A=1)` | [`DSH7143`](../diagnostics/DSH7xxx.md#dsh7143) |

A number is read the way Unreal's `LexTryParseString` reads it, which stops at the first character it
cannot use: `"0.5abc"` is `0.5`. Each of these codes reaches you inside
[`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213), which names the node and the property and quotes the
code. On an expanded token the binder checks the value's kind first: an enum value against the
catalog (below), a name or an asset path as a quoted string, anything else as a compile-time
constant ([`DSH5224`](../diagnostics/DSH5xxx.md#dsh5224)).

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

That is the matcher a declaration's keys go through. On an **expanded token** the binder checks the
value against the enumerators the engine catalog lists *(since 2.0.0)*: an exact name passes; one of
the spellings above that normalizes to an enumerator name is accepted with a
[`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278) warning; anything else — including a `DisplayName` that
is not also an enumerator name, such as `Shared: Wrap` — is
[`DSH5215`](../diagnostics/DSH5xxx.md#dsh5215).

### Object properties

An object-valued UPROPERTY takes an asset reference resolved by the shared resolver documented in
[Path(…)](path.md#two-resolvers).

| Situation | Code |
| :-- | :-- |
| Value starts with `Path(` or `/`, or ends with `'`, and does not resolve | the resolver's own code — [`DSH8118`](../diagnostics/DSH8xxx.md#dsh8118)–[`DSH8132`](../diagnostics/DSH8xxx.md#dsh8132), or [`DSH1045`](../diagnostics/DSH1xxx.md#dsh1045) for a `Class'…'` of an unrelated class |
| Value is none of those | [`DSH7137`](../diagnostics/DSH7xxx.md#dsh7137) |
| Asset loads but is the wrong class | [`DSH7139`](../diagnostics/DSH7xxx.md#dsh7139) |
| Asset fails to load | [`DSH7138`](../diagnostics/DSH7xxx.md#dsh7138) |

> [!WARNING]
> **A texture that fails to load is silently accepted.** When the property is a `UTexture` subclass
> *and* is named exactly `Texture` or `TextureObject`, a failed load writes `nullptr` and the write is
> reported as successful. `[Texture = Path(Game, "Typo")]` therefore generates without any diagnostic
> and leaves an unbound sampler. *(since 2.0.0)* The same goes for the default of a texture-sample
> token and of a `const` texture, which are written to that property too; a texture-sample node then
> takes the engine's default texture. Check the asset path when a sampler comes out wrong.

## Organization fields that are not reflected

`Group`, `SortPriority` and `Desc` exist on `UMaterialExpressionParameter` subclasses. A class that
lacks one of them gets a [`DSH8210`](../diagnostics/DSH8xxx.md#dsh8210) **warning** and the value is
not written. Every class the 15 building tokens make has all three.

*(since 2.0.0)* Any *other* key a declaration's class lacks is the same `DSH8210` warning, where 1.x
stopped the compile. On an expanded token it is `DSH5213`.

> [!NOTE]
> `DynamicParameter`, the 1.x case that lacked all three, is `DSH3253` and builds nothing.

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
| Member that is an expanded token (static switch, mask, texture sample) | not stamped: no group, no auto sort priority, no counter slot *(since 2.0.0)* |
| Member declared `const` | stamped and given a slot, and both stamps are [`DSH7224`](../diagnostics/DSH7xxx.md#dsh7224) warnings: a constant has no group or sort priority |
| Declaration outside any scope | no group, and `SortPriority` 32 |

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
    ScalarParameter Loose = 3.0;                   // no group, SortPriority = 32
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
- A `const` declaration keeps only what a constant node has: see
  [Compact type tokens](compact-types.md#notes).
- An explicit `[SamplerType=…]` always overrides the type inferred from the asset.
- `_MAX` sentinels of engine enums usually carry no `Hidden` metadata, so on a declaration a value
  such as `"SAMPLERTYPE_MAX"` resolves rather than erroring. Do not use them. The catalog an expanded
  token is checked against does not list them (`DSH5215`).

## Diagnostics

### Parse time

| Code | Raised when |
| :-- | :-- |
| `DSH3250` | the statement is only a `[ … ]` block |
| `DSH3255` | an entry whose key is not a name, a key with no `=`, an `=` with no value, or a block with no `]` |
| `DSH3256` | a duplicate key, compared ignoring case |
| `DSH3257` | `Slider(…)` without exactly two numbers, or combined with `SliderMin` / `SliderMax` |
| `DSH3258` | a non-integer `SortPriority` / `Sort` |

### Binding

| Code | Raised when |
| :-- | :-- |
| `DSH7220` | a slider whose minimum is not below its maximum |
| `DSH7233` | a slider on a declaration that is not a scalar (warning; dropped) |
| `DSH7227` | an empty `ParameterName` on a declaration |
| `DSH7224` | `Group` or `SortPriority` on a `const` (warning) |
| [`DSH7222`](../diagnostics/DSH7xxx.md#dsh7222) | an empty `SamplerType` on a declaration |
| `DSH5213` | an expanded token's key is neither a pin nor a property of its class |
| `DSH5215` / `DSH5278` | an expanded token's enum value is no enumerator / is a 1.x spelling of one (warning) |
| `DSH5224` | an expanded token's value is not of the kind its property takes |
| `DSH5276` | an expanded token's key matches a pin or property only ignoring case (warning) |

### Build time

| Code | Raised when |
| :-- | :-- |
| `DSH8210` | a declaration's key, or one of `Group` / `SortPriority` / `Desc` / `ParameterName`, is no property of the class (warning; not written) |
| `DSH8213` | a property could not take its value; the message quotes `DSH7132`–`DSH7143` or the resolver's code |
| `DSH8212` | a slider or a number-valued key on an expanded token names no pin of the node |
| [`DSH8235`](../diagnostics/DSH8xxx.md#dsh8235) | a declaration's `SamplerType` names no sampler type |

The complete list lives in the [diagnostics index](../diagnostics/index.md).

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
MetallicMap  Desc="Packed metallic / roughness"
             SamplerType=SAMPLERTYPE_LinearColor     SamplerSource=SSM_FromTextureAsset
             MipValueMode=TMVM_None  AutomaticViewMipBias=true
             ConstCoordinate=0       ConstMipValue=-1
Metallic     Group="11 - Specular"  SortPriority=51  SliderMin=0  SliderMax=1
Tint         Group="Look"           Desc="Multiplied over base colour"
             bUseCustomPrimitiveData=false           ParameterName="Base Tint"
```

`Metallic` takes `SortPriority = 51` from its own entry. `MetallicMap` is an expanded token, so the
`Group("11 - Specular")` scope does not stamp it *(since 2.0.0)*: write `Group = "11 - Specular"` in its
own block to put it there.

## See also

- [Parameters](index.md) — the hub and the decision table
- [Compact type tokens](compact-types.md) — the tokens a metadata block can follow
- [Parameter node tokens](parameter-nodes.md) — the class-specific keys each token exposes
- [SamplerType](sampler-type.md) — the fully expanded value table for one reflected enum
- [Path(…)](path.md) — the asset-reference grammar object properties accept
- [Properties (section)](../language/properties.md) — `Group("Name") { … }` scopes and ordering
- [Inputs / Outputs / Results](../language/inputs-outputs.md) — where the same block has a reduced effect
- [Decompiler](../tools/decompiler.md) — which metadata keys a round trip always emits
- [Diagnostics index](../diagnostics/index.md) — every code
