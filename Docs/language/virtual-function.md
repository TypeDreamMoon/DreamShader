# VirtualFunction

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **VirtualFunction**

A top-level block that declares an **existing** `UMaterialFunction` asset — its path and its pin
signature — so that a `Graph` can call it. It generates nothing.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsh` and `.dsf` — all three file kinds accept it |
| Kind | top-level block |
| Generates | nothing — the asset must already exist |
| Multiplicity | any number per parse unit |
| Since | `1.2.0` |

## Synopsis

```c
VirtualFunction(Name = "<call-name>" [, Asset = "<asset-reference>"] [,])
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
| **`Name`** | yes | string | The name the block is called by in a `Graph`. Trimmed; must be non-empty. Not an asset path — nothing is created. |
| `Asset` | no | string | The asset reference. Takes precedence over `Options.Asset`; the `Options` value is consulted only when this is absent or blank after trimming. |

Attribute keys are matched case-insensitively. There is **no `Root` attribute** — the package root is
part of the asset reference itself. Unrecognized attribute keys are parsed and silently ignored, and
a duplicate key silently overwrites the earlier one.

> [!WARNING]
> An unquoted attribute value ends at the first `,` or `)`. `Asset=Path(Game, "F/X")` in the header
> therefore truncates to `Path(Game` and the parse fails with `Expected identifier near index {Index}.`
> Put `Path(...)` forms in `Options` — `Options = { Asset = Path(Game, "F/X"); }` — or quote the whole
> literal and escape the inner quotes.

## Sections

| Section | Accepted | Repeat behaviour | Reference |
| :-- | :-- | :-- | :-- |
| `Inputs` | yes | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Properties` | yes — **alias for `Inputs`**, no warning | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Outputs` | yes | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Results` | yes — alias for `Outputs`, no warning | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Options` | yes | merges; last key wins | [Options](options.md) |
| `Settings` | yes — alias for `Options`, no warning | merges; last key wins | [Options](options.md) |
| `Graph` | **no** — hard error | — | — |
| `Code` | **no** — hard error | — | — |
| `Layout` | no — unknown section | — | — |

Both `Graph` and `Code` share one diagnostic:
`VirtualFunction declares an existing MaterialFunction asset and does not support Graph or Code sections.`
Any other section name reports `Unknown VirtualFunction section '{Section}'.`

> [!NOTE]
> **Inside a `VirtualFunction`, and only here, `Properties` means `Inputs`.** The statements in it are
> parsed with the typed-parameter grammar (`[opt] <type> <name> [= <default>] [[ … ]] ;`) and appended
> to the block's input list. In a [`Shader`](shader.md), a [`ShaderFunction`](shader-function.md) or a
> [`ShaderLayer`](shader-layer.md), the same keyword declares parameter and `const` **nodes** with a
> different grammar. A declaration such as `const float X = 1;` that is legal in those blocks is not a
> valid `VirtualFunction` `Properties` statement — `const float` becomes the type token and generation
> fails with `VirtualFunction '{Name}' input 'X' uses unsupported type 'const float'.`

## The asset reference

The value is stored verbatim at parse time; nothing checks it until a `Graph` actually calls the
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

Rules that apply to every form:

- Root names and the two `Path(...)` arguments are matched case-insensitively, and each argument may
  be quoted or bare.
- A path that already starts with `/` is used verbatim; the root argument, if any, is ignored.
- A path that does **not** start with `/` requires a root — otherwise
  `Relative asset Path(...) references require a root such as Game, Engine, or Plugin.PluginName.`
- When the resolved path carries no `.ObjectName` suffix, the leaf name is appended automatically, so
  `/Game/F/X` becomes `/Game/F/X.X`.
- A plugin root must name a project plugin that is enabled, can contain content, has an existing
  `Content` directory and — on UE 5.6 and newer — is mounted. Each failure has its own message; see
  [Path](../parameters/path.md).

`Path(...)` here is the same resolver used by `UE.CollectionParam` and by reflected asset-valued
properties. It is **not** the resolver used by texture-property defaults, which has its own message
set; both are catalogued in [Path](../parameters/path.md).

## `Options` keys

`Options` (and its alias `Settings`) uses the ordinary settings grammar: `<Key> = <Value> ;`, keys
trimmed and lower-cased, values unquoted, later duplicates silently overwriting earlier ones.

| Key | Read by the compiler | Effect |
| :-- | :-- | :-- |
| `Asset` | yes | the asset reference, used when the header `Asset=` attribute is absent or blank |
| any other key | **no** | parsed, stored on the definition, never read |

`Description` is the key the editor's VirtualFunction tooling writes into generated declarations. It
is documentation for the reader only — the compiler ignores it, as it ignores every key other than
`Asset`. See [Options](options.md).

## Inputs and outputs

The declared parameters describe the **existing asset's** interface; they do not create anything.
They serve three purposes: they let the call site be type-checked, they name the pins, and they fix
the order of a statement call's arguments.

The accepted type tokens are the same set a [`ShaderFunction`](shader-function.md#parameter-types)
accepts, and the same `opt` rule applies: `opt` marks an input the caller may omit or pass `default`
for. Defaults and `[ … ]` metadata parse here as well, but there is no node to write them to — a
`VirtualFunction` never touches the asset it points at.

At call time each declared input is matched to a pin on the loaded `UMaterialFunction` by name
(case-insensitive); if no pin has that name, the pin at the same index is used. A declaration that
matches neither reports
`VirtualFunction '{Name}' input '{Input}' does not exist on MaterialFunction asset '{ObjectPath}'.`

## Calling a VirtualFunction

The callee name is matched case-insensitively against the full `Name` and against its last
`/`-separated segment. Only the leaf form is spellable in an expression, because `/` lexes as the
division operator.

```c
// VirtualFunction(Name="BufferWriter") declared in an imported .dsh:
vec3 Written = BufferWriter(Color, Alpha);        // single-output call as a value  (since 1.5.0)
BufferWriter(Color, Alpha, OutResult);            // statement call: inputs, then one target per output  (since 1.3.5)
```

Expression calls accept positional or named arguments (`BufferWriter(Color = Tint)`) but not a mix.
An expression call to a multi-output block selects which output it evaluates to with a named
`Output=` / `OutputName=` / `OutputIndex=` argument; `Output`/`OutputName` and `OutputIndex` cannot
be combined, which reports
`VirtualFunction '{Name}' cannot use OutputName/Output together with OutputIndex.`

Statement calls are positional only, and the trailing arguments — one per declared output — must be
plain variable names; each writes its output into that name, replacing any earlier value bound to it.
Full argument rules, including the `default` sentinel for `opt` inputs, are in
[Calls](../graph/calls.md).

> [!NOTE]
> Three `VirtualFunction` names are intercepted before the asset is loaded:
> `BreakOutFloat2Components`, `BreakOutFloat3Components` and `BreakOutFloat4Components`. An
> **expression** call to one of them that names an output — by channel name (`Output="G"`), by index
> (`OutputIndex=1`), or by an output whose declared name is a single `R`/`G`/`B`/`A` or `X`/`Y`/`Z`/`W`
> letter — compiles to a swizzle on the input instead of a `MaterialFunctionCall` node, and the asset
> is never loaded. Anything the interception does not recognize falls through to the normal call path.
> Statement calls are not intercepted.

## Notes

- **Nothing is generated, and nothing is validated at parse time.** A `VirtualFunction` whose asset
  does not exist parses cleanly and only fails when a `Graph` calls it. A file that declares only
  `VirtualFunction` blocks compiles successfully with the message
  `DreamShader file '{File}' contains VirtualFunction declarations only; no assets were generated.`
- A `VirtualFunction` is normally kept in a `.dsh` and imported where it is needed, which is what the
  editor's *Create VirtualFunction* action does — it writes a declaration under
  `DShader/VirtualFunctions`. See [VirtualFunction tools](../tools/virtual-function-tools.md).
- `Name` is a call name, not an asset path. It may contain `/`, but only its last segment is usable
  in a `Graph` expression.
- The header `Asset=` attribute wins over `Options.Asset`; the `Options` entry is only consulted when
  the attribute is missing or blank after trimming.
- Because `import` inlines every file into one text before parsing, a `VirtualFunction` declared in
  an imported `.dsh` is visible to the importing file's `Graph` blocks. See [import](import.md).
- Two `VirtualFunction` blocks with the same leaf name in one parse unit are both kept; the first
  match in declaration order wins at every call site.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this section.

### Parse time

| Message | Cause |
| :-- | :-- |
| `VirtualFunction(Name="...") is required.` | the header has no `Name` attribute |
| `VirtualFunction name cannot be empty.` | `Name` is empty after trimming |
| `VirtualFunction '{Name}' must provide Options = { Asset = Path(...); }.` | no asset reference in the header and none under `Options.Asset` |
| `VirtualFunction '{Name}' must declare at least one output.` | no `Outputs` / `Results` entry |
| `VirtualFunction declares an existing MaterialFunction asset and does not support Graph or Code sections.` | a `Graph` or `Code` section was used |
| `Unknown VirtualFunction section '{Section}'.` | a section other than `Inputs`, `Properties`, `Outputs`, `Results`, `Options`, `Settings`, `Graph`, `Code` |
| `Invalid typed declaration '{Statement}'.` | a parameter statement with no space between type and name, an empty left side, or a name that is not an identifier |
| `Invalid setting declaration '{Statement}'.` | an `Options` statement with no top-level `=` |
| `Invalid empty setting key in '{Statement}'.` | an `Options` statement whose key is empty |
| `Expected identifier near index {Index}.` | a malformed header attribute — including an unquoted `Path(Root, "…")` value |
| `Expected '{' near index {Index}.` | the body block is missing |
| `Unterminated block.` | the body `{` is never closed |

### Call time — asset resolution

These fire when a `Graph` calls the block, not when it is declared. The resolver's own message is
wrapped as `VirtualFunction '{Name}' asset reference is invalid: {Detail}`.

| Message ( `{Detail}` ) | Cause |
| :-- | :-- |
| `Asset reference cannot be empty.` | the stored reference is blank |
| `Asset Path(...) reference is missing a closing ')'.` | text begins with `Path(` but does not end with `)` |
| `Asset Path(...) contains an unterminated string literal.` | an unbalanced `"` inside the argument list |
| `Asset Path(...) expects either 1 argument (/Game/... path) or 2 arguments (Game\|Engine\|Plugin.PluginName, asset path).` | zero, or three or more, arguments |
| `Asset reference requires a non-empty path.` | the path argument is empty |
| `Relative asset Path(...) references require a root such as Game, Engine, or Plugin.PluginName.` | a relative path with no root argument |
| `Unsupported asset Path root '{Root}'. Use Game, Engine, or Plugin.PluginName.` | an unrecognized root segment |
| `Asset Path root '{Root}' has an invalid plugin name.` | the plugin name is empty or does not survive sanitization |
| `Asset Path root '{Root}' references plugin '{Plugin}', but no enabled plugin with that name was found.` | unknown plugin |
| `Asset Path root '{Root}' references plugin '{Plugin}', but the plugin is not enabled.` | disabled plugin |
| `Asset Path root '{Root}' references plugin '{Plugin}', but the plugin cannot contain content.` | code-only plugin |
| `Asset Path root '{Root}' references plugin '{Plugin}', but its Content directory does not exist: '{Dir}'.` | missing content directory |
| `Asset Path root '{Root}' references plugin '{Plugin}', but the plugin content is not mounted.` *(since UE 5.6)* | unmounted plugin content |
| `Invalid asset path '{Path}'.` | the resolved path has no `/` or ends with one |
| *(the engine's own `IsValidObjectPath` text)* | the resolved path is not a valid Unreal object path |

### Call time — interface

| Message | Cause |
| :-- | :-- |
| `VirtualFunction '{Name}' could not load MaterialFunction asset '{ObjectPath}'.` | the path resolved but no `UMaterialFunction` is there |
| `VirtualFunction '{Name}' must declare at least one output.` | reached with an empty output list |
| `VirtualFunction '{Name}' input '{Input}' does not exist on MaterialFunction asset '{ObjectPath}'.` | no input pin matches by name and no pin exists at that index |
| `VirtualFunction '{Name}' output '{Output}' does not exist on MaterialFunction asset '{ObjectPath}'.` | no output pin matches by name and no pin exists at that index |
| `VirtualFunction '{Name}' cannot use OutputName/Output together with OutputIndex.` | both output-selector arguments on one expression call |
| `VirtualFunction '{Name}' input '{Input}' uses unsupported type '{Type}'.` | type token outside the parameter-type table |
| `VirtualFunction '{Name}' input '{Input}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` parameter on UE 5.3 |
| `VirtualFunction '{Name}' output '{Output}' uses unsupported type '{Type}'.` | type token outside the parameter-type table |
| `VirtualFunction '{Name}' output '{Output}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` parameter on UE 5.3 |
| `Failed to create a MaterialFunctionCall node for '{Name}'.` | the call node could not be created |
| `Failed to assign material function '{Name}' to the generated call node.` | the loaded asset was rejected by the call node |

The diagnostics for argument counts, mixed positional/named arguments, missing required inputs and
output-target names are shared with every other call form and are listed in
[Calls](../graph/calls.md). The complete cross-stage list lives in the
[diagnostics index](../diagnostics/index.md).

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
                         Exposure is left unconnected (declared opt)
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
- [Path](../parameters/path.md) — every accepted `Path(...)` spelling and both resolvers
- [VirtualFunction tools](../tools/virtual-function-tools.md) — creating, opening and refreshing declarations from the editor
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
