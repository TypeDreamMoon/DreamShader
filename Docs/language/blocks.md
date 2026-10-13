---
title: Top-level Blocks
description: The seven top-level blocks — Shader, ShaderFunction, ShaderLayer / ShaderLayerBlend, VirtualFunction, Function, GraphFunction, Namespace — and what each generates.
---

A source file is a flat sequence of top-level blocks. They may appear in any order with no separator
between them, and apart from `Shader` each may appear any number of times in a file. Compiling a file
once builds every asset its blocks describe.

Block keywords and the lower-case reserved words are **case-sensitive**. `shader(…)` and `SHADER(…)`
match nothing and report `DSH2240`. See [Lexical Elements](lexical.md).

## Synopsis

```c
<top-level-block> := { Shader | ShaderFunction | ShaderLayer | ShaderLayerBlend
                     | MaterialLayer | MaterialLayerBlend | VirtualFunction | Namespace }
                     ( <attribute> = <value> [, …] ) { <section>… }
                   | { Function [ SelfContained | Inline ] | GraphFunction }
                     [<return-type>] <name> ( [<parameter>, …] ) { <HLSL> }

<section>          := <section-name> [=] { <statement>… } [;]
```

| Notation | Meaning | Example |
| --- | --- | --- |
| `<x>` | Placeholder — substitute a real value; the angle brackets are not typed. | `Name = <string>` |
| `[ x ]` | Optional — the whole group may be left out. | `[, Root = <string>]` |
| `{ a \| b }` | Choice — take exactly one of the alternatives separated by \|. | `{ Node( … ) \| Comment( … ) }` |
| `…` | Repetition — the preceding item may appear any number of times. | `<property-declaration> …` |

That is two header shapes: the asset blocks take a `( Key = Value )` attribute list and a set of
[sections](sections.md); the two function blocks take a C-style signature and a raw
HLSL body. The legacy front end reads each block as its 2.0 declaration (since 2.0.0), and
[`dsc migrate`](../tools/migrate.md) writes the `.dss` the same way.

## What each block generates

| Block | Header | Generates | Since |
| :-- | :-- | :-- | :-- |
| `Shader` | `Shader(Name = "…"[, Root = "…"])` | `UMaterial`; under the ThinCustom backend a `UDreamShaderMaterialInstance` over a hidden base | — |
| `ShaderFunction` | `ShaderFunction(Name = "…"[, Root = "…"])` | `UMaterialFunction` (usage `Default`) | — |
| `ShaderLayer` | `ShaderLayer(Name = "…"[, Root = "…"])` | `UMaterialFunctionMaterialLayer` | (since 1.3.0) |
| `ShaderLayerBlend` | `ShaderLayerBlend(Name = "…"[, Root = "…"])` | `UMaterialFunctionMaterialLayerBlend` | (since 1.3.0) |
| `VirtualFunction` | `VirtualFunction(Name = "…"[, Asset = "…"])` | **nothing** — declares an existing asset | (since 1.2.0) |
| `Function` | `Function [SelfContained \| Inline] [<ret>] <Name>( … )` | no asset and no file; one `Custom` node per call site, whose code holds the body and every other `Function` it calls | — |
| `GraphFunction` | `GraphFunction [<ret>] <Name>( … )` | nothing by itself; one `Custom` node per call site plus the lifted material nodes | (since 1.3.1) |
| `Namespace` | `Namespace(Name = "…")` | nothing — prefixes member names with `Ns::` | — |

### Header attributes

Every attributed block requires `Name`; a missing or empty one is `DSH2242`, and a block word with no
`(` after it is `DSH2241`. `Root` defaults to `/Game` — to the plugin's mount point for a file under a
plugin source root; the full path grammar is in [Asset Paths](../generation/asset-paths.md).

- Attribute keys are case-insensitive (`name=` works). Values may be quoted or bare; a bare value runs
  to the next `,` or `)` **outside parentheses**, so `Asset = Path(Game, "F/X")` works in a header
  too (since 2.0.0).
- A key written twice: the later value wins, with the warning `DSH2244` (since 2.0.0).
- A trailing comma before `)` is accepted, as in 1.x (since 2.1.1) (2.0.0 – 2.1.0 reported
  `DSH2243`). Two commas in a row, a missing key, `=` or value are still `DSH2243`.
- No `{` after the header, or a section missing its name or its `{`, is `DSH2257`; a file that ends
  inside a block is `DSH2150`.

## `Shader`

Declares one material: its parameters, its render state, its output bindings and the graph that feeds
them. A `.dss` writes the same material as `#pragma material(...)`, `uniform`s and one
`export void <Name>(inout material m)`.

```c
Shader(Name = "<asset-path>" [, Root = "<root>"])
{
    [Properties [=] { <property-declaration> ; … }]
    [Settings   [=] { <key> = <value> ; … }]
    [Outputs    [=] { { <output-declaration> | <output-binding> } ; … }]
    [Graph      [=] { <graph-statement> … }]
    [Layout     [=] { { Node( … ) | Comment( … ) } ; … }]
}
```

```c
Shader(Name="Materials/M_Emissive", Root="Game")
{
    Properties = {
        Group("Look") {
            vec3  Tint      = vec3(1.0, 0.4, 0.1) [Description="Emissive tint"];
            float Intensity = 2.0                 [Slider(0, 10)];
        }
        TextureSampleParameter2D BaseTex = Path(Game, "Textures/T_Noise");
    }

    Settings = {
        ShadingModel = "Unlit";
        BlendMode    = "Translucent";
        TwoSided     = true;
    }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph = {
        vec2 UV  = UE.TexCoord(Index = 0);
        vec4 Tex = BaseTex(Coordinates = UV);
        Color = Tex.rgb * Tint * Intensity;
        Alpha = Tex.a;
    }
}
```

Rules that only apply here:

- **`.dsm` only.** A `Shader` in a `.dsh` is `DSH2249`, in a `.dsf` `DSH2259` (since 2.1.1).
- **At most one per file** (`DSH2250`). A header cannot hold a `Shader`, so an import never adds a
  second one.
- **It may share a `.dsm` with function and layer blocks.** One compile builds them all, each into its
  own asset; the material calls such a function by name and the function's asset is built first
  (since 2.1.1) (2.0.0 – 2.1.0 refused the pair with `DSH6201`).
- Without a `Graph`, the `Outputs` statements have to compute the values themselves — an output
  declaration with an initializer (since 1.3.4), or a binding whose right side is an expression,
  such as `Base.FrontMaterial = Substrate.Layer(Coat, Body);` (since 2.0.0). No `Graph` and an
  empty `Outputs` is `DSH2255`. An empty `Graph = { }` is legal.
- **Bindings are what make a material.** A `Shader` with no `Outputs` section is the warning
  `DSH2256`: the material builds, with nothing wired to it (since 2.0.0) (through 1.9.x
  generation then failed).
- Binding `Base.MaterialAttributes` auto-enables *Use Material Attributes*. `Base.FrontMaterial`
  (needs (since UE 5.4)) is an ordinary attribute now (since 2.0.0): it no longer forces
  the shading model to Substrate or refuses `Base.MaterialAttributes` beside it; write
  `ShadingModel = "Substrate";` in `Settings` where the material needs it. See
  [Output Bindings](../generation/outputs.md).
- `Inputs`, `Results` and `Options` are not sections a `Shader` knows (`DSH2245`); `Code` is
  `DSH2246`. A `Graph` or `Layout` written twice keeps the later one, with the warning `DSH2258`.

**Note:**

Which UClass lands at the target path depends on the resolved backend. `ThinCustom` — the default —
writes a `UDreamShaderMaterialInstance` whose parent is a hidden `UMaterial` subobject named
`MB_DreamThinBase_<leaf>`; `Graph` writes a plain `UMaterial`. `Instance` is a deprecated alias of
`ThinCustom` (warning `DSH7204`). A `Graph`-backend material is saved on every successful build
(since 2.0.0); a `ThinCustom` product stays **Ephemeral** — in memory only — until a cook, an
explicit *Materialize*, or a child instance gives it a package. See [Backend](../settings/backend.md)
and [In-memory Materials](../generation/ephemeral.md).

## `ShaderFunction`

Declares one reusable `UMaterialFunction`: typed input and output pins, function-local parameter
nodes, and the graph that connects them. The legacy front end reads it as a 2.0 `export` function: the
inputs are parameters, a first output named exactly `Result` is the return value, and the other
outputs are `out` parameters.

```c
ShaderFunction(Name = "<asset-path>" [, Root = "<root>"])
{
    [Properties            [=] { <property-declaration> ; … }]
    [Inputs                [=] { <parameter-declaration> ; … }]
    { Outputs | Results }  [=] { <parameter-declaration> ; … }
    Graph                  [=] { <graph-statement> … }
    [Settings              [=] { <key> = <value> ; … }]
    [Layout                [=] { { Node( … ) | Comment( … ) } ; … }]
}
```

```c
ShaderFunction(Name="Functions/F_Tint", Root="Game")
{
    Properties = {
        Group("Tint") {
            float Boost = 1.0 [Slider(0, 4); Description="Extra gain applied after tinting"];
        }
        const float Epsilon = 0.001;
    }

    Inputs = {
        vec3  InColor;
        vec3  InTint             [Description="Multiplied with InColor"];
        opt float Strength = 1.0 [Description="Blend amount"; SortPriority=10];
    }

    Outputs = {
        vec3  OutColor;
        float OutLuma;
    }

    Graph = {
        vec3 Tinted = InColor * InTint * Boost;
        OutColor    = lerp(InColor, Tinted, Strength);
        OutLuma     = dot(OutColor, vec3(0.2126, 0.7152, 0.0722)) + Epsilon;
    }
}
```

A function needs at least one output (`DSH4315`), and its `Graph` must assign every output but a first
`Result` (`DSH6211`). `Properties` here declares nodes *inside* the function, while `Inputs` declares
pins *on* it — the most common source of confusion in this block; see
[Sections](sections.md).

Inside the `Graph` of another block of the same file, the callee name is the asset's leaf:
`F_Tint` for `Name="Functions/F_Tint"`. An asset block cannot be imported (`DSH2249`), so any other
file reaches the generated asset through a [`VirtualFunction`](#virtualfunction). When a function the
file can call already has that name — a `Function` from an imported header, say — the block's function
is renamed `<Leaf>_Asset` with the info `DSH5290`, and the asset keeps its name.

**Note:**

On regeneration the `Id` GUID of every input and output pin is cached by name and restored
(since 1.3.2), so existing hand-placed `MaterialFunctionCall` nodes keep their wiring — **as long
as the pin name is unchanged**. A rename is a delete plus an add: call sites lose that connection.

## `ShaderLayer` and `ShaderLayerBlend`

(since 1.3.0)

The material-layer variants of `ShaderFunction`. They share its body parser and section table; only
the interface rules differ. The legacy front end reads `ShaderLayer` as a 2.0 `@layer` function and
`ShaderLayerBlend` as a `@layerblend` function, and checks the rules when the file is bound
(since 2.0.0).

```c
ShaderLayer(Name = "<asset-path>" [, Root = "<root>"])
{
    [Inputs                [=] { MaterialAttributes <name> ; }]
    { Outputs | Results }  [=] { MaterialAttributes <name> ; }
    Graph                  [=] { <graph-statement> … }
}

ShaderLayerBlend(Name = "<asset-path>" [, Root = "<root>"])
{
    Inputs                 [=] { MaterialAttributes <name> ; MaterialAttributes <name> ; }
    { Outputs | Results }  [=] { MaterialAttributes <name> ; }
    Graph                  [=] { <graph-statement> … }
}
```

| Kind | Rule | Code |
| :-- | :-- | :-- |
| both | a `MaterialAttributes` output is declared | `DSH3278` |
| `ShaderLayer` | the material is all there is: any input that is not `MaterialAttributes`, and any output besides the material, breaks the signature | `DSH6204` |
| `ShaderLayerBlend` | at least one `MaterialAttributes` input, and no output besides the material | `DSH6205` |

A layer's material flows **through** it: the layer always has exactly one `MaterialAttributes` input,
named after its output — also when the 1.x block declared no input at all. A 1.x `MaterialAttributes`
input with another name becomes that input, and the pin changes its name with the warning `DSH3277`;
the body still reads it under its 1.x name. The input is optional only when the 1.x input was declared
`opt`. A blend's output starts empty and each input is a pin of its own; the engine's layer stack
blends exactly two.

Scalars, vectors and textures cannot be layer or blend inputs. Expose them through `Properties`
instead: they become parameter nodes inside the generated function and appear on the layer stack's
parameter panel. Neither block can be called from a `Graph` (`DSH6208`); their consumer is the layer
stack on a material.

```c
ShaderLayer(Name="Layers/L_Rust", Root="Game")
{
    Properties = {
        Group("Rust") {
            vec3  RustColor = vec3(0.35, 0.13, 0.05);
            float RustRough = 0.85 [Slider(0, 1)];
        }
    }

    Outputs = { MaterialAttributes Attrs; }

    Graph = {
        Attrs.BaseColor = RustColor;
        Attrs.Roughness = RustRough;
        Attrs.Metallic  = 0.0;
    }
}
```

**Note:**

On (since UE 5.7), each `MaterialAttributes` input of a blend gets a `BlendInputRelevance` from
its name — spaces, `_` and `-` removed, compared ignoring case: `Top` / `TopLayer` map to `Top`;
`Bottom` / `BottomLayer` / `Base` / `BaseLayer` map to `Bottom`. Any other name falls back by
position: the first input is `Bottom`, later ones `Top`. On UE 5.3–5.6 the property does not exist and
nothing is written.

An existing asset at the target path is reused when it **is a** subclass of the expected class;
`ShaderFunction` requires the exact class. Switching a block between `ShaderFunction` and
`ShaderLayer` without moving the old asset therefore fails with `DSH8110`.

### Deprecated spellings

**Deprecated since 1.3.0.** Use `ShaderLayer` instead.

`MaterialLayer(...)` and `MaterialLayerBlend(...)` still parse and still generate exactly the same
assets, each with the warning `DSH2251`. A missing `Name` (`DSH2242`) names the spelling you actually
typed.

## `VirtualFunction`

(since 1.2.0)

Declares an **existing** `UMaterialFunction` — its path and its pin signature — so a `Graph` can call
it. It generates nothing, and validates nothing until a call site needs it. The legacy front end reads
it as a 2.0 `extern` prototype carrying `/// @asset`.

```c
VirtualFunction(Name = "<call-name>" [, Asset = "<asset-reference>"])
{
    [{ Options | Settings }   [=] { Asset = <asset-reference> ; … }]
    [{ Inputs | Properties }  [=] { <parameter-declaration> ; … }]
    { Outputs | Results }     [=] { <parameter-declaration> ; … }
}
```

```c
VirtualFunction(Name="BufferWriter")
{
    Options = {
        Asset       = Path(Game, "MaterialFunctions/F_BufferWriter");
        Description = "Existing material function declared for Graph calls.";
    }

    Inputs = {
        vec3  Color;
        float Alpha;
        opt float Exposure = 1.0;
    }

    Outputs = {
        vec3  Result;
        float Coverage;
    }
}
```

- `Name` is the **call name**, not an asset path, and must be an identifier (`DSH6311`). 1.x accepted a
  `/` in it and made only the last segment callable; that is `DSH6311` now (since 2.0.0). There is
  no `Root` attribute — the package root is part of the asset reference itself.
- The header `Asset=` attribute wins over `Options.Asset`; the section value is read only when the
  attribute is absent or blank after trimming. Neither is `DSH6312`; no output at all is `DSH6313`.
- `Description` in `Options` is kept as the declaration's description (`dsc migrate` writes it above
  the `extern`) and changes nothing in a build; other keys are read and ignored.
- `Graph` and `Code` are `DSH2247`: the block declares an asset, it does not build one. `Layout` and
  any other name are `DSH2245`.
- The reference is resolved when a call is built: one that does not resolve is `DSH8270`, one that does
  not load `DSH8219`, and a declared input or output the asset does not have is `DSH8220` / `DSH8221`.
  A `VirtualFunction` nothing calls compiles cleanly with a broken `Asset`.
- Two `VirtualFunction`s of one name are `DSH4210` (since 2.0.0) (1.x kept both and called the
  first). A file of nothing but `VirtualFunction` blocks compiles and produces no asset.

**Warning:**

**Inside a `VirtualFunction`, and only there, `Properties` is an alias for `Inputs`.** It takes the
typed-parameter grammar, not the parameter-node grammar. A declaration such as `const float X = 1;`
that is legal in a `Shader` is `DSH3271` here — `const` is a keyword, not a type.

## `Function`

An HLSL helper. The body is HLSL text, kept verbatim, and every call site becomes one
`UMaterialExpressionCustom` node whose code is that body plus every other `Function` it calls
(since 2.0.0) (no `.ush` include is written any more).

```c
Function [ { Inline | SelfContained } ] [ <return-type> ] <name> ( [ <parameter-list> ] )
{
    <hlsl>
}
```

```c
Function float Luma(in vec3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

Function SelfContained Remap01(in float value, out float result)
{
    result = saturate(value * 0.5 + 0.5);
}
```

There is no attribute header, no `Settings` and no sections — the `{ … }` after the parameter list is
raw HLSL. `SelfContained` and its old spelling `Inline` now read as a plain function and change nothing
(`Inline` warns `DSH6306`). The full parameter, return-type and output rules are in
[Functions](functions.md).

## `GraphFunction`

(since 1.3.1)

Written exactly like `Function`, but the `UE.*` calls in its body are lifted out of the text, built as
real material nodes, and wired into the generated `Custom` node as auto-named input pins.

```c
GraphFunction [ <return-type> ] <name> ( [ <parameter-list> ] )
{
    <hlsl-with-UE-calls>
}
```

```c
GraphFunction WindPulse(in float2 uv, out float pulse)
{
    float t = UE.Time();
    pulse = sin(uv.x * 8.0 + t);
}
```

`GraphFunction` takes no modifier: `GraphFunction SelfContained Foo(…)` and
`GraphFunction Inline Foo(…)` are `DSH6307` (since 2.0.0) (1.x consumed the word as a return
type). There is no self-contained mode for `GraphFunction`.

## `Namespace`

Prefixes the names of the `Function` and `GraphFunction` declarations it contains with `<Name>::`.

```c
Namespace(Name = "<identifier>")
{
    { <function-declaration> | <graph-function-declaration> } …
}
```

```c
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

A `Namespace` is not an entity: no object is stored for it and it creates **no scope**. A member's
name is flattened to the identifier `Common_ApplyTint` (since 2.0.0), so `Common::ApplyTint` and
`Common_ApplyTint` are one name. `Name` must be an identifier (`DSH6309`), so there is no `A::B`
multi-segment form. It **cannot nest** and may contain only those two block kinds — anything else is
`DSH6310`. An empty `Namespace(Name="X") { }` is accepted and declares nothing. See
[Functions](functions.md#namespace).

## At a glance

| | `Shader` | `ShaderFunction` | `ShaderLayer(Blend)` | `VirtualFunction` | `Function` | `GraphFunction` | `Namespace` |
| :-- | :-: | :-: | :-: | :-: | :-: | :-: | :-: |
| Writes a `.uasset` | yes | yes | yes | no | no | no | no |
| Allowed in `.dsh` | no | no | no | yes | yes | yes | yes |
| Count per file | **1** | any | any | any | any | any | any |
| Body | sections | sections | sections | sections | HLSL | HLSL | blocks |
| `Properties` | parameter nodes | parameter nodes | parameter nodes | alias of `Inputs` | — | — | — |
| `Graph` required | unless `Outputs` computes its values | yes | yes | refused | — | — | — |
| Callable from `Graph` | no | yes | no | yes | yes | yes | via members |
| Callable as a value | — | yes | — | yes | yes | yes | — |

A function with several outputs is callable as a value too, and evaluates to its first output
(since 2.0.0); see [Calls](../graph/calls.md).

## Diagnostics

| Message | Cause | Fix |
| --- | --- | --- |
| `DSH2240: Expected a top-level Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend, Function, GraphFunction, Namespace, VirtualFunction or import, found {0}.` | Text at top level that is no block keyword — a correct keyword in the wrong case included. | Check the case; block keywords are case-sensitive. Also look for an unclosed block above. [Details](lexical.md) |
| `DSH2241: Expected '(' with the block's attributes, such as '(Name = "M_Example")', found {0}.` | A block word not followed by its attribute list. Every 1.x block names itself there. | Write Shader(Name = "M_Example") and open the body with { after it. |
| `DSH2242: Expected a 'Name = "..."' attribute on '{0}', found none.` | The attribute list has no Name, or an empty one. {0} is the spelling you typed. | Add Name = "...". |
| `DSH2243: Expected an attribute name such as 'Name', found {0}.` | The attribute list is not comma-separated Key = value pairs: a key, = or value is missing, or two commas stand in a row. One comma before ) is fine. | Write each attribute as Key = "value". |
| `DSH2244: The attribute '{0}' is written twice; the later value wins, as it did in 1.x.` | One attribute written twice (warning). | Remove one of them. |
| `DSH2257: Expected '{' to open the '{0}' block, found {1}.` | No { where a block opens: after the attribute list, after a section name, or a section name is missing inside the block. | Open the body with { and start each section with its name. |
| `DSH2245: Expected a Shader section (Properties, Settings, Outputs, Graph or Layout), found '{0}'.` | A word that is no section opens a line inside a block — Inputs, Results and Options in a Shader among them; the message lists the sections the block's kind takes. | Check the spelling (section names ignore case, so it is the letters) and that the previous section is closed. [Details](sections.md) |
| `DSH2246: Expected 'Graph' as the body section of '{0}', found 'Code', which 1.x accepted only inside a Function.` | A Shader or ShaderFunction has a Code section. | Rename it to Graph, or move the HLSL into a Function and call it. |
| `DSH2247: Expected no body in the VirtualFunction '{0}', which declares an existing asset, found the section '{1}'.` | A VirtualFunction has a Graph or Code section. | Remove the body, or make the block a ShaderFunction if the source is meant to build the asset. |
| `DSH2258: The section '{0}' is written twice; the later one wins, as it did in 1.x.` | One section (Graph or Layout) written twice in a block (warning). | Merge the two sections. |
| `DSH2250: Expected one Shader block in a file, found a second one.` | A file holds two Shader blocks. | Give each material its own file. [Details](source-files.md) |
| `DSH2255: Expected a Graph section in the Shader '{0}', found none.` | A Shader with no Graph and an empty Outputs: nothing to build. | Add Graph = { ... }, or let the Outputs statements compute their values. |
| `DSH2256: The Shader '{0}' has no Outputs section, so nothing its Graph computes reaches the material.` | A Shader with a Graph and no Outputs section (warning). The material builds, with nothing wired to it. | Add at least one Base.&lt;Attribute&gt; = &lt;variable&gt;; binding. [Details](../generation/outputs.md) |
| `DSH3278: Expected a MaterialAttributes output on '{0}', found none.` | A ShaderLayer or ShaderLayerBlend has no MaterialAttributes output. | Declare MaterialAttributes Result; in Outputs. |
| `DSH6204: A '@layer' function is written 'export void {0}(inout material m)'.` | A ShaderLayer with an input that is not MaterialAttributes, or an output besides the material. | Move layer controls to Properties. |
| `DSH6205: A '@layerblend' function is written 'export void {0}(material Base, material Top, ..., inout material Result)': at least one 'material' input and a final 'inout material'.` | A ShaderLayerBlend with no MaterialAttributes input, or with an output besides the material. | Move blend controls to Properties. |
| `DSH3277: The layer input '{0}' becomes the 'inout material' parameter named after the output '{1}', so the input pin changes its name.` | A 1.x layer names its MaterialAttributes input and output differently; a 2.0 layer has one inout material parameter, named after the output (warning). | Nothing in the source. Layer stacks wire by position; a graph that wired the function call by pin name needs reconnecting once. |
| `DSH6208: '{0}' is a {1} asset, not a function this file may call.` | A Graph calls the material entry, a layer or a layer blend. A layer is applied by the layer stack of the material that uses it. | Factor the shared code into a helper and call that from both. |
| `DSH2251: '{0}' is the old spelling of '{1}'; it still reads the same.` | MaterialLayer / MaterialLayerBlend is used (warning). | Nothing has to change; dsc migrate writes the current form. |
| `DSH6311: Expected a 'Name = "..."' attribute with a name on 'VirtualFunction', found none.` | A VirtualFunction with no Name, or one that is not an identifier (a / in it, say). | Give it an identifier name. |
| `DSH6312: Expected an 'Asset = Path(...)' option on the VirtualFunction '{0}', found none.` | No asset reference in the header or in Options. | Add Options = { Asset = Path(Game, "Functions/MF_Example"); }. |
| `DSH6313: Expected at least one output on the VirtualFunction '{0}', found none.` | A VirtualFunction declares no output. | Declare the asset's outputs under Outputs, in the asset's order. |
| `DSH6307: Expected a return type or a name after 'GraphFunction', found the modifier '{0}', which only a Function takes.` | GraphFunction followed by SelfContained or Inline. | Remove the modifier. [Details](functions.md) |
| `DSH6309: Expected a 'Name = "..."' attribute with a name on 'Namespace', found none.` | A Namespace with no Name, or one that is not an identifier — ::, ., -, a space. | Write Namespace(Name = "BL") { ... }. |
| `DSH6310: Expected only Function and GraphFunction blocks inside the namespace '{0}', found {1}.` | Anything else inside a Namespace body, a nested Namespace included. | Move the other block out of the namespace. |
| `DSH4210: '{0}' is already declared in this file; one name declares one thing.` | One name declared twice — in the file, or in the file and a header it imports. There is no overloading. | Rename one of them. [Details](import.md) |

## Where to next

  - [Sections](sections.md) — Properties, Inputs, Outputs, Settings, Graph, Layout, and which block takes which.
  - [Functions](functions.md) — The full rules of Function, GraphFunction and Namespace.
  - [Material Settings](../settings/material.md) — Every key a Shader's Settings accepts.
  - [Asset Paths](../generation/asset-paths.md) — How Name and Root become a package path.
