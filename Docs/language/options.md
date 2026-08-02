# Options

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Options**

The section of a `VirtualFunction` block that names the existing `UMaterialFunction` asset the
declaration stands for.

| | |
| :-- | :-- |
| Declared in | `.dsh`, `.dsf`, `.dsm` — inside a `VirtualFunction` block only |
| Kind | section |
| Generates | nothing — a `VirtualFunction` declares an asset, it does not create one |
| Since | `1.2.0` |

## Synopsis

```c
{ Options | Settings } [=]
{
    Asset = <asset-reference> ;
    [ <key> = <value> ; ]…
}
```

```c
asset-reference := Path( <root> , "<relative-path>" )
                 | Path( "<absolute-object-path>" )
                 | "<absolute-object-path>"
                 | <absolute-object-path>
```

`Options` and `Settings` are accepted interchangeably as the section keyword; both parse into the
same map. Section names are matched case-insensitively, the `=` before `{ … }` is optional
*(since 1.5.0)*, and a repeated section merges into the same map.

## Keys

| Key | Required | Value | Effect |
| :-- | :-- | :-- | :-- |
| **`Asset`** | yes, unless `VirtualFunction(Asset="…")` was given | asset reference | The `UMaterialFunction` a `Graph` call to this name resolves to. |
| *any other key* | — | any | **Parsed, stored, and never read.** |

`Asset` is the only key the compiler consumes. There is no diagnostic for an unrecognized key.

> [!NOTE]
> The [VirtualFunction sync service](../tools/virtual-function-tools.md) writes a `Description` key
> alongside `Asset` when it generates or refreshes a declaration. That key is round-tripped by the
> tooling but has no effect on compilation.

### Precedence

The block attribute wins over the section:

```c
VirtualFunction(Name="BufferWriter", Asset=Path(Game, "MaterialFunctions/F_BufferWriter"))
{
    Options = { Asset = Path(Game, "Ignored"); }   // not consulted — the attribute was present
    Outputs = { float3 Result; }
}
```

`Options.Asset` is read **only** when the `Asset` attribute is absent or empty after trimming.

## Statement grammar

Identical to the [`Settings`](../settings/index.md) grammar.

| Rule | Detail |
| :-- | :-- |
| Statement form | `<Key> = <Value> ;` |
| Split point | the first `=` that is outside `()`, `[]` and `"…"`, so `Asset = Path(Game, "A/B")` splits on the outer `=` |
| Key normalization | trimmed, then lower-cased. Nothing else — spaces, `_` and `-` are **not** removed |
| Value handling | one surrounding `"…"` pair is stripped and unescaped |
| Duplicate key | the later statement silently **overwrites** the earlier one |
| Comments | stripped from the section body before statements are split |

> [!WARNING]
> A `Settings`/`Options` value is trimmed on both sides of the `=`, but the text **inside** a quoted
> value survives the quote stripping untouched — unlike a `Layout` argument, which is trimmed again
> after unquoting. `Asset = "  /Game/MF/F_X  ";` stores the surrounding spaces, while
> `Asset =   /Game/MF/F_X  ;` stores `/Game/MF/F_X`. The asset-reference resolver trims the stored
> value again before resolving it, so `Asset` itself tolerates both.

String escapes recognized in a quoted value are `\n`, `\r`, `\t`, `\"` and `\\`; any other `\X`
yields the literal `X`.

## Asset reference resolution

The parser stores the raw text. The reference is resolved **when the `VirtualFunction` is called from
a `Graph`**, using the general asset-reference resolver — not the texture-default resolver. Accepted
root spellings:

| Root | Resolves to |
| :-- | :-- |
| `Game` | `/Game` |
| `Engine` | `/Engine` |
| `Plugin.<Name>` | the plugin's mounted asset path |
| `Plugins.<Name>` | the plugin's mounted asset path |

A `Path( … )` call takes either 1 argument (an absolute `/…` object path) or 2 arguments (root plus a
relative path). A bare or quoted value that is not a `Path( … )` call is used directly and must be an
absolute object path. The full grammar, both resolvers and every root diagnostic are on
[`Path(...)`](../parameters/path.md).

> [!WARNING]
> An unresolvable `Asset` is **not** diagnosed at parse time or at generation time — only at the
> moment a `Graph` statement calls the function, as
> `VirtualFunction '{Name}' asset reference is invalid: {Detail}`. A `VirtualFunction` that nothing
> calls compiles with a broken `Asset` and produces no message.

## Post-parse rules

The two `Name` rules are checked before the block body is parsed; the other two immediately after.

| Rule | Diagnostic |
| :-- | :-- |
| a `Name` attribute is present | `VirtualFunction(Name="...") is required.` |
| `Name` is non-empty after trimming | `VirtualFunction name cannot be empty.` |
| an asset is available from the attribute or from `Options.Asset` | `VirtualFunction '{Name}' must provide Options = { Asset = Path(...); }.` |
| at least one output is declared | `VirtualFunction '{Name}' must declare at least one output.` |

## Sections accepted alongside `Options`

| Section | Inside a `VirtualFunction` |
| :-- | :-- |
| `Options` | this page |
| `Settings` | alias for `Options` |
| `Inputs` | typed parameters — [Inputs / Outputs / Results](inputs-outputs.md) |
| `Properties` | **alias for `Inputs`** |
| `Outputs` | typed parameters |
| `Results` | alias for `Outputs` |
| `Graph` | **hard error** |
| `Code` | **hard error** |
| `Layout` | unknown section |

`Graph` and `Code` both report
`VirtualFunction declares an existing MaterialFunction asset and does not support Graph or Code sections.`
Any other name reports `Unknown VirtualFunction section '{Section}'.`

## Notes

- A `VirtualFunction`'s `Inputs` and `Outputs` describe the *existing* asset's interface. They are
  what a `Graph` call is checked against; they do not create pins. Getting them out of step with the
  asset produces call-site errors, not declaration errors.
- `.dsh` files may contain `VirtualFunction` blocks and nothing else that generates assets. A file
  holding only `VirtualFunction` declarations compiles successfully with
  `DreamShader file '{File}' contains VirtualFunction declarations only; no assets were generated.`
- The `Options` map is preserved verbatim, so tooling can round-trip additional keys through a
  declaration without the compiler rejecting them.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this section.

| Message | Stage | Cause |
| :-- | :-- | :-- |
| `Invalid setting declaration '{Statement}'.` | parse | a statement with no top-level `=` |
| `Invalid empty setting key in '{Statement}'.` | parse | the key side of the split is empty |
| `VirtualFunction(Name="...") is required.` | parse | no `Name` attribute on the block |
| `VirtualFunction name cannot be empty.` | parse | `Name` is empty after trimming |
| `VirtualFunction '{Name}' must provide Options = { Asset = Path(...); }.` | parse | no asset from the attribute or from `Options` |
| `VirtualFunction '{Name}' must declare at least one output.` | parse | empty `Outputs` / `Results` |
| `VirtualFunction declares an existing MaterialFunction asset and does not support Graph or Code sections.` | parse | a `Graph` or `Code` section in the block |
| `Unknown VirtualFunction section '{Section}'.` | parse | an unrecognized section name |
| `VirtualFunction '{Name}' asset reference is invalid: {Detail}` | graph build | the `Asset` value did not resolve at a call site |

The complete cross-stage list is in the [diagnostics index](../diagnostics/index.md).

## Example

```c
VirtualFunction(Name="BufferWriter")
{
    Options = {
        Asset = Path(Game, "MaterialFunctions/F_BufferWriter");
        Description = "Existing material function declared for Graph calls.";
    }

    Inputs = {
        float3 Color;
        float  Alpha;
    }

    Outputs = {
        float3 Result;
    }
}
```

Calling it from a `Shader` in the same parse unit:

```c
Graph = {
    float3 Written = BufferWriter(Color = Tint, Alpha = A);
}
```

Resolved reference:

```text
Asset  Path(Game, "MaterialFunctions/F_BufferWriter")
    -> /Game/MaterialFunctions/F_BufferWriter.F_BufferWriter
```

## See also

- [VirtualFunction](virtual-function.md) — the block `Options` belongs to
- [Inputs / Outputs / Results](inputs-outputs.md) — the interface sections of a `VirtualFunction`
- [Settings](../settings/index.md) — the identical statement grammar and key normalization
- [`Path(...)`](../parameters/path.md) — every root spelling and both resolvers
- [Calls](../graph/calls.md) — calling a `VirtualFunction` from `Graph`
- [VirtualFunction tools](../tools/virtual-function-tools.md) — the editor actions and the sync service
- [Source files](source-files.md) — which block kinds `.dsh` / `.dsf` / `.dsm` may hold
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
