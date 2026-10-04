# VirtualFunction

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **VirtualFunction**

A top-level block that declares an **existing** `UMaterialFunction` asset — its path and its pin
signature — so that a `Graph` can call it. It generates nothing.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsh` and `.dsf` — all three file kinds accept it |
| Kind | top-level block |
| Generates | nothing — the asset must already exist |
| Multiplicity | any number per file; one name declares one thing |
| Since | `1.2.0` |

The legacy front end reads a `VirtualFunction` as a 2.0 `extern` prototype carrying `/// @asset`; see
[the 2.0 language](../language-v2/index.md) and [`dsc migrate`](../tools/migrate.md).

## Synopsis

```c
VirtualFunction(Name = "<call-name>" [, Asset = <asset-reference>])
{
    [{ Options | Settings }   [=] { Asset = <asset-reference> ; … }]
    [{ Inputs | Properties }  [=] { <parameter-declaration> ; … }]
    { Outputs | Results }     [=] { <parameter-declaration> ; … }
}
```

A `Name`, an asset reference and at least one output are required. The asset reference may come from
the header attribute or from `Options`; see [The asset reference](#the-asset-reference).

The `=` between a section name and its `{ … }` block is optional sugar *(since 1.5.0)*; a `;` after a
section's closing `}` is optional. Sections may appear in any order and may be repeated.

The keyword `VirtualFunction` is matched **case-sensitively**; section names are matched
case-insensitively.

## Header attributes

| Attribute | Required | Value | Effect |
| :-- | :-- | :-- | :-- |
| **`Name`** | yes | string | The name the block is called by in a `Graph`. Trimmed; must be an identifier ([`DSH6311`](../diagnostics/DSH6xxx.md#dsh6311)). Not an asset path — nothing is created. |
| `Asset` | no | asset reference | Takes precedence over `Options.Asset`; the `Options` value is consulted only when this is absent or blank after trimming. |

Attribute keys are matched case-insensitively. There is **no `Root` attribute** — the package root is
part of the asset reference itself. Unrecognized attribute keys are parsed and silently ignored; a key
written twice is a warning ([`DSH2244`](../diagnostics/DSH2xxx.md#dsh2244)) and the later value wins.
A trailing comma before `)` is accepted, as in 1.x *(2.0.0 – 2.1.0 refused it with [`DSH2243`](../diagnostics/DSH2xxx.md#dsh2243))*.

*(since 2.0.0)* An unquoted attribute value runs to the next `,` or `)` **outside parentheses**, so
`Asset = Path(Game, "F/X")` works in the header as well as in `Options`. (1.x cut the value at the
first `,` and failed the parse.)

*(since 2.0.0)* `Name` is an identifier. A `/` in it — which 1.x accepted, making only the last
segment callable — is `DSH6311`.

## Sections

| Section | Accepted | Repeat behaviour | Reference |
| :-- | :-- | :-- | :-- |
| `Inputs` | yes | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Properties` | yes — **alias for `Inputs`**, no warning | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Outputs` | yes | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Results` | yes — alias for `Outputs`, no warning | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Options` | yes | merges; a key written twice is a warning ([`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262)) and the later value wins | [Options](options.md) |
| `Settings` | yes — alias for `Options`, no warning | merges, as `Options` | [Options](options.md) |
| `Graph` | **no** — [`DSH2247`](../diagnostics/DSH2xxx.md#dsh2247) | — | — |
| `Code` | **no** — `DSH2247` | — | — |
| anything else, `Layout` included | **no** — [`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245) | — | — |

> [!NOTE]
> **Inside a `VirtualFunction`, and only here, `Properties` means `Inputs`.** The statements in it are
> read with the typed-parameter grammar (`[opt] <type> <name> [= <default>] [[ … ]] ;`) and appended
> to the block's input list. In a [`Shader`](shader.md), a [`ShaderFunction`](shader-function.md) or a
> [`ShaderLayer`](shader-layer.md), the same keyword declares parameter and `const` **nodes** with a
> different grammar. A declaration such as `const float X = 1;` that is legal in those blocks is not a
> valid `VirtualFunction` `Properties` statement: `const` is a keyword, not a type, so the statement is
> [`DSH3271`](../diagnostics/DSH3xxx.md#dsh3271).

## The asset reference

The value is stored as written at parse time; nothing checks it until the emitter builds a call to the
block. Leading and trailing whitespace is trimmed, one enclosing pair of `"` is stripped and
unescaped, and `\` is rewritten to `/` in the path.

| Form | Resolves to |
| :-- | :-- |
| `Path(Game, "Folder/Asset")` | `/Game/Folder/Asset.Asset` |
| `Path(Engine, "Folder/Asset")` | `/Engine/Folder/Asset.Asset` |
| `Path(Plugin.<PluginName>, "Folder/Asset")` | the plugin's mounted asset path + `/Folder/Asset` |
| `Path(Plugins.<PluginName>, "Folder/Asset")` | same |
| `Path(Plugin/<PluginName>, "Folder/Asset")` | same |
| `Path(Plugins/<PluginName>, "Folder/Asset")` | same |
| `Path(<Root>/<Folder>…, "Asset")` | the root, then the extra segments as folders — `Path(Game/Materials, "F_X")` → `/Game/Materials/F_X.F_X` |
| `Path("/Game/Folder/Asset")` | one argument: an absolute package path, used as-is |
| `"/Game/Folder/Asset"` | a bare quoted absolute path, no `Path(...)` wrapper |
| `/Game/Folder/Asset` | a bare unquoted absolute path |
| `/Game/Folder/Asset.Asset` | a full Unreal object path |
| `"MaterialFunction'/Game/Folder/Asset.Asset'"` | the engine's *Copy Reference* form, quoted; a class that is not a material function is [`DSH1045`](../diagnostics/DSH1xxx.md#dsh1045) |

Rules that apply to every form:

- Root names and the two `Path(...)` arguments are matched case-insensitively, and each argument may
  be quoted or bare.
- A path that already starts with `/` is used verbatim; the root argument, if any, is ignored.
- A path that does **not** start with `/` requires a root
  ([`DSH8123`](../diagnostics/DSH8xxx.md#dsh8123)).
- When the resolved path carries no `.ObjectName` suffix, the leaf name is appended, so `/Game/F/X`
  becomes `/Game/F/X.X`.
- A plugin root must name a project plugin that is enabled, can contain content, has an existing
  `Content` directory and — on UE 5.6 and newer — is mounted. Each failure has its own code
  (`DSH8118`–`DSH8122`, `DSH8124`); see [Path](../parameters/path.md).

A reference that does not resolve is reported as
[`DSH8270`](../diagnostics/DSH8xxx.md#dsh8270), whose message carries the resolver's own code
(`DSH8118`–`DSH8132`, `DSH1045`).

## `Options` keys

`Options` (and its alias `Settings`) uses the ordinary settings grammar: `<Key> = <Value> ;`, keys
compared ignoring case, a quoted value unquoted.

| Key | Read | Effect |
| :-- | :-- | :-- |
| `Asset` | yes | the asset reference, used when the header `Asset=` attribute is absent or blank |
| `Description` | yes | kept as the declaration's description *(since 2.0.0)*, which is what `dsc migrate` writes above the `extern`; it changes nothing in a build |
| any other key | **no** | parsed and ignored, without a diagnostic |

`Description` is the key the editor's VirtualFunction tooling writes into generated declarations. See
[Options](options.md).

## Inputs and outputs

The declared parameters describe the **existing asset's** interface; they do not create anything.
They serve three purposes: they let the call site be type-checked, they name the pins, and they fix
the order of a statement call's arguments.

The accepted type tokens and the parameter grammar are a [`ShaderFunction`'s](shader-function.md#parameter-types),
and the same `opt` rule applies: `opt` — or *(since 2.0.0)* a default value — marks an input the caller
may leave out. Defaults and `[ … ]` metadata parse here as well, but nothing is written anywhere: a
`VirtualFunction` never touches the asset it points at, and an input a call does not pass is left
unconnected on the call node, so the asset's own default applies. An output named exactly `Result`
that comes first is the call's return value.

At emit time each input a call passes is matched to a pin on the loaded `UMaterialFunction` by name —
exactly, then ignoring case — and failing both by the input's position among the declared inputs.
Outputs are matched the same way. An input or output that matches nothing is
[`DSH8220`](../diagnostics/DSH8xxx.md#dsh8220) / [`DSH8221`](../diagnostics/DSH8xxx.md#dsh8221).

## Calling a VirtualFunction

The callee name is the `Name` attribute. *(since 2.0.0)* It matches exactly; a call that matches it
only in case resolves when the match is unique, with
[`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275).

```c
// VirtualFunction(Name="BufferWriter") declared in an imported .dsh:
vec3 Written = BufferWriter(Color, Alpha);        // single-output call as a value  (since 1.5.0)
BufferWriter(Color, Alpha, OutResult);            // statement call: inputs, then one target per output  (since 1.3.5)
```

Expression calls accept positional or named arguments (`BufferWriter(Color = Tint)`). An expression
call to a multi-output block selects which output it evaluates to with a named `Output=` /
`OutputName=` / `OutputIndex=` argument; `Output`/`OutputName` and `OutputIndex` together are
[`DSH5252`](../diagnostics/DSH5xxx.md#dsh5252).

A statement call passes the inputs, then one receiver per declared output, each of which must be a
variable; each writes its output into that name, replacing any earlier value bound to it. Full
argument rules, including the `default` sentinel for `opt` inputs, are in
[Calls](../graph/calls.md).

> [!NOTE]
> Three names are intercepted before anything is looked up: `BreakOutFloat2Components`,
> `BreakOutFloat3Components` and `BreakOutFloat4Components` (any case). An **expression** call to one
> of them whose first argument is positional and that selects a channel — `Output="G"` (one of
> `R`/`G`/`B`/`A`/`X`/`Y`/`Z`/`W`), `Output="1"`, or `OutputIndex=1` — inside the vector's width
> compiles to a swizzle on that argument instead of a `MaterialFunctionCall` node, and no asset is
> loaded. Anything the interception does not recognize falls through to the normal call path.
> Statement calls are not intercepted.

## Notes

- **Nothing is generated, and nothing is checked against the asset before it is called.** A
  `VirtualFunction` whose asset does not exist builds cleanly until a `Graph` calls it; then the
  emitter reports [`DSH8219`](../diagnostics/DSH8xxx.md#dsh8219). A file that declares only
  `VirtualFunction` blocks is accepted and produces nothing.
- A `VirtualFunction` is normally kept in a `.dsh` and imported where it is needed, which is what the
  editor's *Create VirtualFunction* action does — it writes a declaration under
  `DShader/VirtualFunctions`. See [VirtualFunction tools](../tools/virtual-function-tools.md).
- The header `Asset=` attribute wins over `Options.Asset`; the `Options` entry is only consulted when
  the attribute is missing or blank after trimming.
- A header is parsed on its own and its declarations are declared into the importing file, so a
  `VirtualFunction` declared in an imported `.dsh` is visible to the importing file's `Graph` blocks.
  See [import](import.md).
- Two `VirtualFunction` blocks with the same name are [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210)
  *(since 2.0.0; 1.x kept both and called the first)*.

## Diagnostics

Each code carries the line and column of the construct; the code's page has the message.

### Parse time

| Code | Raised when |
| :-- | :-- |
| [`DSH2241`](../diagnostics/DSH2xxx.md#dsh2241) | no `(` after `VirtualFunction` |
| `DSH2243` | a malformed attribute list |
| `DSH2244` | an attribute written twice (warning) |
| `DSH6311` | no `Name`, or one that is not an identifier |
| [`DSH2257`](../diagnostics/DSH2xxx.md#dsh2257) | no `{` after the header, or a section without its name or its `{` |
| `DSH2247` | a `Graph` or `Code` section |
| `DSH2245` | any other unknown section |
| `DSH3271` | a parameter statement that is not `[opt] <type> <name> [= <default>] [[ … ]] ;` |
| [`DSH3272`](../diagnostics/DSH3xxx.md#dsh3272) / [`DSH3273`](../diagnostics/DSH3xxx.md#dsh3273) | `opt` / a default on an output (warnings; ignored) |
| [`DSH3255`](../diagnostics/DSH3xxx.md#dsh3255)–[`DSH3258`](../diagnostics/DSH3xxx.md#dsh3258) | a malformed `[ … ]` metadata block |
| [`DSH3261`](../diagnostics/DSH3xxx.md#dsh3261) | an `Options` statement without a key, an `=` or a value |
| `DSH3262` | an `Options` key written twice (warning) |
| [`DSH6312`](../diagnostics/DSH6xxx.md#dsh6312) | no asset reference in the header and none under `Options.Asset` |
| [`DSH6313`](../diagnostics/DSH6xxx.md#dsh6313) | no `Outputs` / `Results` entry |
| [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) | the body is never closed |

### Binding and call time

| Code | Raised when |
| :-- | :-- |
| [`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201) | a type token names no type |
| [`DSH4214`](../diagnostics/DSH4xxx.md#dsh4214) | two parameters share a name |
| `DSH4210` | the name is declared twice |
| `DSH5252` | `Output`/`OutputName` and `OutputIndex` on one call |
| [`DSH5250`](../diagnostics/DSH5xxx.md#dsh5250) / [`DSH5251`](../diagnostics/DSH5xxx.md#dsh5251) | an `OutputIndex` that is not a whole number / an `Output` that is not a name |

The diagnostics for argument counts, named arguments, missing inputs and receivers are shared with
every other call and are listed in [Calls](../graph/calls.md) and
[`Function` § Call time](function.md#call-time).

### Emit time — asset resolution and interface

These fire when the emitter builds a call, not when the block is declared.

| Code | Raised when |
| :-- | :-- |
| `DSH8270` | the asset reference does not resolve; the message carries the resolver's code (`DSH8118`–`DSH8132`, `DSH1045`) |
| `DSH8219` | no `UMaterialFunction` loads from the resolved path, or the call node refuses it |
| `DSH8220` | a passed input matches no pin |
| `DSH8221` | an output matches no pin |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | the `MaterialFunctionCall` node could not be created |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

`DShader/VirtualFunctions/VF_BufferWriter.dsh`:

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
        opt float Exposure = 1.0 [Description="Optional gain applied inside the function"];
    }

    Outputs = {
        vec3  Result;
        float Coverage;
    }
}
```

`DShader/M_Buffered.dsm`:

```c
import "VirtualFunctions/VF_BufferWriter.dsh";

Shader(Name="Materials/M_Buffered")
{
    Properties = {
        vec3  Tint  = vec3(1.0, 0.9, 0.8);
        float Fade  = 0.5 [Slider(0, 1)];
    }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph = {
        BufferWriter(Tint, Fade, Color, Alpha);
    }
}
```

Result:

```text
VF_BufferWriter.dsh  ->  no asset generated
M_Buffered.dsm       ->  /Game/Materials/M_Buffered
                         contains one MaterialFunctionCall node bound to
                         /Game/MaterialFunctions/F_BufferWriter.F_BufferWriter
                         Exposure is left unconnected (optional, not passed)
```

## See also

- [Options](options.md) — the `Options` section and every key it recognizes
- [Inputs / Outputs / Results](inputs-outputs.md) — the typed-parameter grammar, `opt`, defaults, metadata
- [ShaderFunction](shader-function.md) — generating a `UMaterialFunction` instead of declaring one
- [ShaderLayer / ShaderLayerBlend](shader-layer.md) — the material-layer function blocks
- [Shader](shader.md) — the `UMaterial`-producing top-level block
- [Source files](source-files.md) — which block kinds each of `.dsm` / `.dsh` / `.dsf` may contain
- [import](import.md) — how a `.dsh` declaration reaches the file that calls it
- [Calls](../graph/calls.md) — argument forms, `default`, statement vs expression calls
- [Path](../parameters/path.md) — every accepted `Path(...)` spelling
- [VirtualFunction tools](../tools/virtual-function-tools.md) — creating, opening and refreshing declarations from the editor
- [Diagnostics index](../diagnostics/index.md) — every code, by stage
