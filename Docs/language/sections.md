---
title: Sections
description: Properties, Inputs, Outputs, Results, Settings, Options, Graph and Layout — which block accepts which, how repeats behave, and Group scopes.
---

The body of an attribute-taking block is a list of sections. A section is a name, an optional `=`,
and a braced list of `;`-separated statements.

```c
<section> := <section-name> [=] { <statement>… } [;]
```

| Notation | Meaning | Example |
| --- | --- | --- |
| `<x>` | Placeholder — substitute a real value; the angle brackets are not typed. | `Name = <string>` |
| `[ x ]` | Optional — the whole group may be left out. | `[, Root = <string>]` |
| `{ a \| b }` | Choice — take exactly one of the alternatives separated by \|. | `{ Node( … ) \| Comment( … ) }` |
| `…` | Repetition — the preceding item may appear any number of times. | `<property-declaration> …` |

Section names are matched **case-insensitively**, may appear in any order, and may be repeated. The
`=` is optional sugar (since 1.5.0), and so is the `;` after the closing `}`. All four of
these are the same section:

```c
Properties = { float A = 1.0; }
Properties   { float A = 1.0; }
properties = { float A = 1.0; };
PROPERTIES   { float A = 1.0 }
```

A section body is read token by token (since 2.0.0): any whitespace separates words, a tab as
well as a space; the reader tracks `()`, `[]` and `{}`, and a string is one token, so a `Group` body
and a bracketed metadata block never split a statement in the wrong place.

## Support matrix

| Section | `Shader` | `ShaderFunction` | `ShaderLayer` / `Blend` | `VirtualFunction` |
| :-- | :-- | :-- | :-- | :-- |
| `Properties` | parameter nodes | parameter nodes | parameter nodes | **alias of `Inputs`** |
| `Inputs` | unknown section | pin declarations | pin declarations, layer rules apply | pin declarations |
| `Outputs` | declarations **+ bindings** | pin declarations | pin declarations, layer rules apply | pin declarations |
| `Results` | unknown section | alias of `Outputs` | alias of `Outputs` | alias of `Outputs` |
| `Settings` | material settings | three function keys | three function keys | alias of `Options` |
| `Options` | unknown section | unknown section | unknown section | the `Asset` reference |
| `Graph` | required¹ | required | required | `DSH2247` |
| `Layout` | yes | yes | yes | unknown section |
| `Code` | `DSH2246` | `DSH2246` | `DSH2246` | `DSH2247` |

1. Unless the `Outputs` statements compute their values themselves — an output declaration with an
   initializer (since 1.3.4), or a binding whose right side is an expression (since 2.0.0).

"Unknown section" is `DSH2245`. `Function` and `GraphFunction` have no sections at all — their `{ … }`
is raw HLSL.

## Repeat behaviour

A repeated section does not always append.

| Section | On repeat |
| :-- | :-- |
| `Properties`, `Inputs`, `Outputs`, `Results` | **appends** to the earlier list |
| `Settings`, `Options` | **merges**; a key written twice keeps the later value, with the warning `DSH3262` (since 2.0.0) |
| `Graph` | **replaces** the earlier body, with the warning `DSH2258` (since 2.0.0) |
| `Layout` | **resets** — a second `Layout` discards the first entirely, with the warning `DSH2258` |

1.x did each of these overrides silently; they still take effect, and now they say so.

## `Properties`

Declares the parameter nodes, constant nodes and `UE.*` builtin nodes a block generates **into its own
graph**. In 2.0 a property is a `uniform` (or a `static const`) with its metadata in `///` lines;
`dsc migrate` rewrites a section that way.

```c
property-declaration := [ const ] <type-token> <name> [ = <default> ] [ [ <metadata> ] ] ;

group-scope          := Group( "<group-name>" ) { { <property-declaration> | <group-scope> }… } [ ; ]
```

The innermost `[ … ]` pair around `<metadata>` is **literal** DreamShaderLang punctuation; the outer
pair is the meta-syntax for "optional". A declaration with metadata therefore reads
`float Roughness = 0.5 [Group="Surface"];`.

```c
Properties = {
    const float DebugScale = 1.0;                 // UMaterialExpressionConstant
    float Strength         = 1.0;                 // UMaterialExpressionScalarParameter
    vec3  Tint             = vec3(1.0, 1.0, 1.0); // UMaterialExpressionVectorParameter
    UE.TexCoord(Index = 0) UV;                    // a builtin node, not a parameter
}
```

A declaration is read token by token, left to right (since 2.0.0):

| Step | Reads | When it is missing or wrong |
| :-- | :-- | :-- |
| 1 | an optional `const` (any case) | — |
| 2 | the type: one word, or `UE.<Function>` with its `( … )` argument list | `DSH3250`; after `const`, `DSH3251` |
| 3 | the name: one identifier | `DSH3250` |
| 4 | after `=`, the default: every token up to a `[` or `;` outside brackets | `DSH3254` when nothing follows the `=` |
| 5 | an optional `[ … ]` metadata block | `DSH3255` |
| 6 | `;`, or the `}` that closes the section or group | `DSH3250` |

So `UE.TexCoord(Index = 0) UV;` reads as type `UE.TexCoord(Index = 0)` and name `UV`, and a tab
between type and name is fine.

**Warning:**

The name must be an identifier (since 2.0.0). 1.x checked only that it was non-empty, so
`Properties { float 1Bad = 0; }` parsed and was unreachable; now `1Bad` is a malformed number
(`DSH2105`) and the declaration is `DSH3250`. A reserved word such as `in` or `out` cannot be a name
either. A name declared twice in one file is `DSH4210`, compared exactly (1.x ignored case).

A property node is built where the `Graph` **first reads it** and shared by every later read
(since 1.3.2); a property the `Graph` never reads produces no node. The `Graph` is read after the
block's other sections, so a property declared below it is still visible: declaration order does not
affect name resolution — only the auto-sort counter below.

| Type family | Generated node |
| :-- | :-- |
| compact scalar / vector tokens (`float`, `vec3`, …) | `ScalarParameter` / `VectorParameter` |
| compact texture tokens (`Texture2D`, …) | `TextureObjectParameter` |
| the 22 explicit parameter-node tokens | the named `UMaterialExpression` subclass |
| `UE.<Function>( … )` | that builtin's node |

Anything else is `DSH3252`. Seven of the explicit tokens have no 2.0 form and are `DSH3253`
(since 2.0.0): `DoubleVectorParameter`, `TextureCollectionParameter`, `CurveAtlasRowParameter`,
`DynamicParameter`, `FontSampleParameter`, `SpriteTextureSampler`, `SparseVolumeTextureObjectParameter`.
`StaticBool` is read as `bool` here, so it declares a **dynamic** bool parameter (since 2.0.0); a
static switch parameter is `StaticBoolParameter`. A `= <default>` after a `UE.*` property is
`DSH3259` — its arguments go inside the parentheses.

Besides `Path(Game, "…")`, `Path("/Game/…")` and a quoted absolute path, a texture default takes the
Content Browser's **Copy Reference** text, `Texture2D'/Game/Textures/T_X.T_X'` (since 1.9.0) —
bare, quoted, or as the path argument of `Path(…)`. A class in front of the quotes that does not match
the declared texture type is `DSH1044`; one that is no texture at all is `DSH1043`.

Type tokens, `const`, defaults and metadata in full: [Properties Types](properties.md)
and [Metadata and Groups](../parameters/metadata.md).

### `Group("Name") { … }` scopes

(since 1.5.0)

A `Group` scope stamps its name onto every declaration inside it, so a shared group need not be
repeated in each declaration's metadata.

```c
Properties {
    Group("Surface") {
        ScalarParameter Roughness = 0.5 [Slider(0, 1)];
        VectorParameter BaseColor = float4(1, 1, 1, 1);
    }
}
```

| Rule | Detail |
| :-- | :-- |
| Keyword | `Group`, case-insensitive |
| Argument | `(` right after the keyword, holding exactly one double-quoted string, then `)` |
| Name | that string, trimmed; it must be non-empty — `DSH3260` otherwise |
| Body | `{ … }` |
| Terminator | a single `;` after the closing `}` is optional |

`Group(…)` is the **only** construct that may open a `{` inside `Properties`. Any other `{` is
`DSH3250`, and a group whose `}` is missing runs on to the end of the file (`DSH2150`).

Scopes nest to any depth. A nested scope's effective name is the outer name, a `|`, and the inner
name — Unreal's own sub-category syntax. A `|` typed inside a name passes through unchanged:

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

An explicit key on the declaration always wins. The inherited group applies only when the member's
metadata names neither `Group` nor `Category`:

```c
Group("Surface") {
    float A = 0;                        // Group = "Surface"    (inherited)
    float B = 0 [Group="Override"];     // Group = "Override"   (explicit wins)
    float C = 0 [Category="Other"];     // Group = "Other"      (Category is the alias)
}
```

### Automatic `SortPriority`

Members of a group scope are auto-numbered in declaration order.

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

**Note:**

The counter is seeded once per `Properties` section. A block that declares `Properties` twice gets a
fresh counter starting at `0` in the second section, so two groups in two sections can end up with
overlapping sort priorities.

When neither a group scope nor explicit metadata supplies a value, a parameter gets `32` — the engine's
own default for parameter nodes, which is what 1.x left it at.

### Metadata

In the trailing `[ … ]` block, entries are separated by `;` or `,`, keys are matched ignoring case, and
a key written twice is `DSH3256`.

| Key | Aliases | Value |
| :-- | :-- | :-- |
| `Group` | `Category` | string |
| `Description` | `Desc`, `Tooltip` | string |
| `SortPriority` | `Sort` | whole number — `DSH3258` otherwise |
| `Slider(min, max)` | — | shorthand with no `=`; malformed or given twice is `DSH3257` |
| `ParameterName` | — | overrides the material parameter name |
| `SamplerType` | — | a sampler type; a `SAMPLERTYPE_` prefix is dropped |
| *any other key* | — | written to the node's same-named property by reflection |

Every key the table does not name is written to the node by reflection (since 2.0.0): a key the
parameter node class does not have is the warning `DSH8210` and the value is not written (1.x refused
it), and a value the property cannot take is `DSH8213`. A slider range belongs to a scalar parameter;
on a vector, texture or bool parameter it is one warning, `DSH7233`, and the range is dropped
(since 2.1.0). Full grammar: [Metadata and Groups](../parameters/metadata.md).

## `Inputs`, `Outputs` and `Results`

The typed-parameter sections. They declare the pins of a generated material function, or the
interface a `VirtualFunction` describes.

```c
parameter-declaration := [ opt ] <type> <name> [ = <default-expression> ] [ [ <metadata> ] ] ;
```

`Results` is a pure synonym of `Outputs` — it appends into the same list, with no warning.

| Member | Required | Notes |
| :-- | :-- | :-- |
| `opt` (since 1.2.3) | no | Marks the input optional in Unreal (`bUsePreviewValueAsDefault`); any case. |
| `<type>` | yes | One word; see [Types and Values](types.md). |
| `<name>` | yes | One identifier; becomes the pin name. |
| `= <default>` | no | The input's default, which also makes it optional (since 2.0.0). Meaningless on outputs. |
| `[ <metadata> ]` | no | Only `Description` / `Desc` / `Tooltip` and `SortPriority` / `Sort` have an effect. |

A statement that is not `[opt] <type> <name> [= <default>] [[ … ]] ;` is `DSH3271`, and so is an empty
default after `=`. Any whitespace may separate `opt`, the type and the name (since 2.0.0); a type
is one word, so `Material Attributes X;` or `float 4 Colour;` is `DSH3271`. A word that names no type
is `DSH4201`; two parameters of one name in a section are `DSH4214`.

**Note:**

`in` and `out` are **not** qualifiers in these sections. They exist only on the
[`Function` / `GraphFunction`](functions.md) signature form, which is a different
grammar. `Inputs = { in float X; }` is `DSH3271`: `in` is a keyword, not a type.

### Input defaults

A default on an input is the value the pin takes when the caller leaves it unconnected, and it makes
the input optional — 1.x gave it a preview value and left the pin required (since 2.0.0).

| Case | Behaviour |
| :-- | :-- |
| the default folds to a constant | written into `PreviewValue` |
| anything else | built as a **graph expression** and connected to the input's `Preview` pin |

The graph-expression path is what lets a default reference a node the block itself generates
(since 1.2.6):

```c
ShaderFunction(Name="Functions/F_Sample")
{
    Properties = {
        const Texture2D PreviewTex = Path(Engine, "EngineResources/DefaultTexture");
    }
    Inputs = {
        opt Texture2D Tex = PreviewTex;      // preview graph, not a literal
        opt float     Mix = 0.5;             // constant → PreviewValue
    }
    Outputs = { vec4 OutColor; }
    Graph   = { OutColor = Tex(Coordinates = UE.TexCoord(Index = 0)) * Mix; }
}
```

A default is converted to the input's type as an assignment would be: a value that does not fit is
`DSH4226`, and a wider vector is cut to its leading components with the info `DSH5289`. A
`StaticBool` input's default becomes a `StaticBool` node on the `Preview` pin, because the engine
ignores `PreviewValue` there.

### Outputs

- `= <expression>` on a material function's `Outputs` / `Results` entry is **dropped** with the warning
  `DSH3273`; `opt` on an output is the warning `DSH3272`. A function output is only ever driven by the
  `Graph`. This does not apply to a `Shader`'s `Outputs`, where an initializer is meaningful.
- A first output named exactly `Result` is the function's return value and starts at zero; every other
  output must be assigned in the `Graph` (`DSH6211`). A function must declare at least one output
  (`DSH4315`), and so must a `VirtualFunction` (`DSH6313`).
- A `MaterialAttributes` output starts as an empty material, so member writes such as
  `Attrs.BaseColor = …;` have a target; a layer's output starts as the material that came in.

### Metadata and order

Only `Description` / `Desc` / `Tooltip` and `SortPriority` / `Sort` have an effect; every other key —
`Group` included — is accepted and **silently ignored**: function input and output nodes have no group
field and get no reflected writes. A section's pins are stably sorted by `SortPriority` (one without
counts as its declaration index), and the asset gets dense priorities `0, 1, 2, …` (since 2.0.0).
Pin GUIDs are cached by name across regeneration, so regenerating a `ShaderFunction` does not break
existing call sites.

### `Properties` versus `Inputs`

Both put something into the generated function, but they are different grammars producing different
nodes.

| | `Properties` | `Inputs` |
| :-- | :-- | :-- |
| Grammar | `[const] <type> <name> …` | `[opt] <type> <name> …` |
| Produces | parameter, constant or `UE.*` nodes *inside* the function graph | `FunctionInput` pins *on* the function's interface |
| Visible to callers | as a material parameter on any material that uses the function | as a wired input pin |

## `Outputs` in a `Shader`

A `Shader`'s `Outputs` fills **two** lists from one body; each statement is classified by its first
tokens:

| Statement shape | Classified as |
| :-- | :-- |
| `<type> <name> ;` | output-variable declaration; starts at zero, as in 1.x |
| `<type> <name> = <expr> ;` | **initialized** output declaration (since 1.3.4) |
| `Base.<target> = <source> ;` | material-property binding |
| `Expression( … ).Pin[<i>] = <source> ;` | expression-input binding |
| `Expression( … ) { Pin[<i>] = <source> ; … }` | block form (since 1.9.0) — several pins on one terminal node |
| anything else | `DSH3266` (a malformed target that began as `Expression( … )` is `DSH3267`) |

```c
Outputs = {
    vec3  Color;
    float Alpha;

    Base.EmissiveColor = Color;
    Base.Opacity       = Alpha;
}
```

- Declarations and bindings may interleave freely; a binding may reference a variable declared later
  in the same section.
- `<source>` is an expression (since 2.0.0): an output variable's name, as 1.x wrote it, or
  anything a `Graph` expression may be, evaluated after the `Graph` has run. An `=` with nothing after
  it is `DSH3270`.
- `return` is a keyword (since 2.0.0): it cannot name an output declaration (`DSH3266`) and is no
  binding source.
- **No `[ … ]` metadata block** is accepted on a `Shader` `Outputs` statement (`DSH3266`). A name
  declared twice is `DSH4220`.

The **block form** writes an `Expression( … )` specification once. The specification *is* the
node-reuse key, so in the statement form one argument forgotten on one pin silently splits the node in
two; a block's *N* pins always land on **one** node:

```c
Outputs = {
    float _PhaseG_Out;
    float _PhaseG2_Out;

    Expression(Class="VolumetricAdvancedMaterialOutput", PerSamplePhaseEvaluation="false")
    {
        Pin[0] = _PhaseG_Out;
        Pin[1] = _PhaseG2_Out;
    }
}
```

A block holds only `Pin[<i>] = <source>;` statements (`DSH3267` otherwise); an empty block is
`DSH3269`; one pin of one node bound twice is `DSH3268`, across the block and statement forms alike. A
block and a statement whose specifications agree address the same node and may be mixed.

The full `Base.*` catalogue and the `Expression( … ).Pin[i]` rules: [Output Bindings](../generation/outputs.md).

## `Settings` and `Options`

Both share one statement grammar: `<Key> = <Value> ;`. Keys are matched ignoring case and kept as
written; a value runs to the `;` outside brackets, a single quoted string being its text with escapes
resolved and anything else the text of its tokens. A key that is not a word, a missing `=` or a missing
value is `DSH3261`; a key written twice keeps the later value with the warning `DSH3262`
(since 2.0.0).

| Block | What `Settings` means |
| :-- | :-- |
| `Shader` | material settings — special keys plus reflected `UMaterial` properties. See [Material Settings](../settings/material.md). |
| `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` | three keys have an effect: `Description`, `ExposeToLibrary`, `LibraryCategories`. |
| `VirtualFunction` | alias of `Options`: `Asset` is the asset reference, `Description` is kept as the declaration's description, other keys are read and ignored. |

**Note:**

The three function keys are applied on every build and **reset when absent**, so deleting a key from
the source deletes it from the asset. `LibraryCategories` is split on commas, each entry trimmed, empty
entries dropped. `ExposeToLibrary` is set only when it is `true` **and** `LibraryCategories` is not
empty (since 2.0.0); a value other than `true` / `false` is the warning `DSH3265` and counts as
`false`. `UserExposedCaption` is no longer applied — the warning `DSH3264`, the value kept for
`dsc migrate` (since 2.0.0). Any other key is the warning `DSH3263` and is ignored.

## `Graph`

The node-graph body. It is read in the same parse as the block's other sections, after them, by the
2.0 statement and expression parser (since 2.0.0); what a 1.x `Graph` did not have is refused by
name (`DSH2200`–`DSH2222`). Comments are recognized as anywhere else, `#Region` directives are valid
only here, and a `#if` preprocessor directive works here too (a `Graph` is DreamShaderLang, not HLSL).

```c
Graph = {
    vec2 UV  = UE.TexCoord(Index = 0);
    vec4 Tex = BaseTex(Coordinates = UV);
    Color = Tex.rgb * Tint;
}
```

The statement and expression language: [Graph Evaluation](../graph/index.md).

## `Layout`

Pins generated node positions and declares comment boxes. See
[Layout and #Region](layout.md).

## Diagnostics

| Message | Cause | Fix |
| --- | --- | --- |
| `DSH2245: Expected a Shader section (Properties, Settings, Outputs, Graph or Layout), found '{0}'.` | A section name the block does not know — the message lists the sections the block's kind takes. | Check the spelling, and that the previous section is closed. [Details](blocks.md) |
| `DSH3250: Expected a property type and a name, found {0}.` | A Properties line does not start with a type and a name: the section is not opened with {, a ; is missing, the name is not an identifier, or a { that is not a Group scope. | Check the line above for a missing ;, and that the type is one word. |
| `DSH3251: Expected a property type after 'const', found {0}.` | const with no type after it. | Write const float K = 1.0;. |
| `DSH3252: Expected a property type such as 'float', 'float4', 'Texture2D' or 'ScalarParameter', found '{0}'.` | The word in type position is no type a 1.x property can have — MaterialAttributes, Substrate, SamplerState and matrices among them. | Use a value type, a texture type or a parameter-node class. [Details](types.md) |
| `DSH3253: Expected a property type with a 2.0 spelling, found '{0}{1}', which has none; move this material to a .dss file and write the node with UE.Expression.` | One of the seven parameter-node tokens with no 2.0 form, or const before a parameter-node token that cannot be a constant. | Declare a plain value or parameter; for any other node class migrate the material to .dss and write UE.&lt;Class&gt;(...) where it is used. |
| `DSH3254: Expected a default value after '{0} =', found {1}.` | Nothing after =, or a default its type cannot read. | Write a number, a constructor, true / false or an asset reference, or remove the =. [Details](lexical.md) |
| `DSH3255: Expected ']' to close the metadata block, found {0}.` | A metadata block that is not closed, or an entry that is not Key = value. | Close it with ] and end each entry with ;. [Details](../parameters/metadata.md) |
| `DSH3256: Expected the metadata key '{0}' once, found it again.` | One key written twice in one [ … ] block. | Keep one of them. |
| `DSH3257: Expected 'Slider(min, max)' with two numbers, found '{0}'.` | Slider is not two numbers, or the range is given twice. | Write the range once, for example Slider(0.0, 4.0). |
| `DSH3258: Expected 'SortPriority' to be a whole number, found '{0}'.` | SortPriority (or Sort) is not a whole number. | Write an integer: SortPriority = 10;. |
| `DSH3259: Expected ')' to close the arguments of 'UE.{0}', found {1}.` | A UE.* property's argument list is not closed, or its arguments are written as a default after the name. | Close the ), and write the arguments inside it: float2 UV = UE.TexCoord(Index = 1);. |
| `DSH3260: Expected a name inside 'Group("...")', found an empty string.` | A Group with an empty name. | Name the group, or drop the block. |
| `DSH3261: Expected a setting name such as 'BlendMode', found {0}.` | A Settings or Options line that does not start with a setting name: a key, = or value is missing. | Check the line above for a missing ;. |
| `DSH3262: The setting '{0}' is written twice; the later value wins, as it did in 1.x.` | One setting written twice (warning). | Remove one of them. |
| `DSH3263: '{0}' is not a setting of '{1}'; 1.x ignored it and so does this front end.` | Not a setting of this kind of block — a material setting in a ShaderFunction, say (warning). | Check the spelling against the docs, or remove the line. [Details](../settings/material.md) |
| `DSH3264: 'UserExposedCaption' has no 2.0 spelling and is not applied; its value is kept for migration.` | UserExposedCaption set on a function (warning). A new asset has no caption; one an earlier build captioned keeps it. | Nothing for the build; set the caption on the asset by hand after migrating if it matters. |
| `DSH3265: Expected 'true' or 'false' for 'ExposeToLibrary', found '{0}'; 1.x ignored the setting and so does this front end.` | ExposeToLibrary is neither true nor false (warning). | Write ExposeToLibrary = true;. |
| `DSH3271: Expected a parameter type and name such as 'float Amount', found {0}.` | An Inputs / Outputs / Results line that is not [opt] &lt;Type&gt; &lt;Name&gt; [= default] [ [metadata] ];, or an empty default — a two-word type and in / out land here. | Check the type spelling and the ; of the line above. |
| `DSH3272: 'opt' on the output '{0}' means nothing; 1.x ignored it and so does this front end.` | opt on an output (warning). | Remove it. |
| `DSH3273: A default on the output '{0}' means nothing; 1.x ignored it, so it is dropped.` | A material function output with a default (warning). | Remove the default, or assign the value in the Graph. |
| `DSH3266: Expected ';' after the output declaration '{0}', found {1}.` | A Shader Outputs line that is neither an output declaration nor a Base.&lt;Attribute&gt; = …; or Expression(...) binding; or a declaration with no ;, a metadata block, or return as its name. | End each line with ; and use one of the three forms. [Details](../generation/outputs.md) |
| `DSH3267: Expected 'Pin[<index>] = <source>' for an Expression(...) output target, found {0}.` | A malformed Expression(...) target: its head, its pin selector, a missing Class, a repeated key, or a statement other than Pin[i] = …; in a block. | Name the pin by its index: Pin[0] = Color;. |
| `DSH3268: Expected each pin of Expression(Class = "{0}") to be bound once, found Pin[{1}] bound again.` | One pin of one node bound twice, across the block and statement forms. | Keep one binding per pin. |
| `DSH3269: Expected at least one 'Pin[<index>] = <source>;' in the Expression(...) block, found none.` | An Expression(...) block that binds no pin. | Add at least one Pin[&lt;index&gt;] = &lt;source&gt;;. |
| `DSH3270: Expected a source after 'Pin[{0}] =', found none.` | An = in Outputs with nothing after it. | Name a Graph variable or a constant. |
| `DSH4201: '{0}' is not a type; did you mean '{1}'? Type names are case-sensitive.` | A word in type position outside Properties that is no type. | Check the spelling. [Details](types.md) |
| `DSH4214: '{0}' is declared twice in the parameter list of '{1}'.` | Two inputs or outputs of one name. | Rename one of them. |
| `DSH4220: '{0}' is already declared in this block.` | A name declared twice in a Shader's Outputs. | Rename one of them. |
| `DSH6211: '{0}' is an 'out' parameter of '{1}' but the body never assigns it, so a caller would read a value nothing produced. Assign it before the function returns, or remove the parameter.` | The Graph never assigns an output. | Assign it in the Graph, or remove the output. |
| `DSH4315: Product {0} is a {1} and produces nothing; a material function needs at least one FunctionOutput.` | A material function, layer or blend with no output. | Declare at least one output. |
| `DSH4210: '{0}' is already declared in this file; one name declares one thing.` | Two properties (or a property and another declaration) of one name in a file, compared exactly. | Rename one of them. |
| `DSH7233: A slider range ('@slider', or 'Slider(min, max)' in a 1.x property) is for a scalar parameter; this one is '{0}', which has no slider, so the range is ignored.` | A slider range on a vector, texture or bool parameter (warning). | Remove the range, or put it on a scalar parameter. [Details](../parameters/metadata.md) |

## Where to next

  - [Types and Values](types.md) — Which type token is valid in which section.
  - [Properties Types](properties.md) — Compact types, const, and the 22 explicit parameter nodes.
  - [Metadata and Groups](../parameters/metadata.md) — The [ … ] block, Slider(…) and reflected passthrough.
  - [Output Bindings](../generation/outputs.md) — The Base.* catalogue and Expression( … ).Pin[i].
