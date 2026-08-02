# Source files

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Source files**

The three DreamShaderLang file kinds — `.dsm`, `.dsf` and `.dsh` — and the rule that decides which
top-level blocks each may contain.

| | |
| :-- | :-- |
| Declared in | — |
| Kind | translation unit |
| Extensions | `.dsm` material · `.dsf` function *(since 1.3.5)* · `.dsh` header |
| Discovered under | `<Project>/DShader` by default; configurable |
| Enforced by | the editor source loader, before parsing |

## Synopsis

```c
// <name>.dsm — Dream Shader Material
[ import "<specifier>" ; ]…
[ Shader( Name = "…" [, Root = "…"] ) { … } ]          // at most one per translation unit
[ <any function, layer, virtual-function or namespace block> ]…
```

```c
// <name>.dsf — Dream Shader Function
[ import "<specifier>" ; ]…
[ { ShaderFunction | ShaderLayer | ShaderLayerBlend }( Name = "…" [, Root = "…"] ) { … } ]…
[ { VirtualFunction | Namespace | Function | GraphFunction } … ]…
```

```c
// <name>.dsh — Dream Shader Header
[ import "<specifier>" ; ]…
[ { VirtualFunction | Namespace | Function | GraphFunction } … ]…
```

## What each kind may contain

| Top-level block | `.dsm` | `.dsf` | `.dsh` | Reference |
| :-- | :-- | :-- | :-- | :-- |
| `Shader` | yes | no | no | [Shader](shader.md) |
| `ShaderFunction` | yes | yes | no | [ShaderFunction](shader-function.md) |
| `ShaderLayer` | yes | yes | no | [ShaderLayer](shader-layer.md) |
| `ShaderLayerBlend` | yes | yes | no | [ShaderLayerBlend](shader-layer.md) |
| `MaterialLayer` *(deprecated in 1.3.0)* | yes | yes | no | [ShaderLayer](shader-layer.md) |
| `MaterialLayerBlend` *(deprecated in 1.3.0)* | yes | yes | no | [ShaderLayer](shader-layer.md) |
| `VirtualFunction` | yes | yes | yes | [VirtualFunction](virtual-function.md) |
| `Namespace` | yes | yes | yes | [Namespace](namespace.md) |
| `Function` | yes | yes | yes | [Function](function.md) |
| `GraphFunction` | yes | yes | yes | [GraphFunction](graph-function.md) |
| `import` | yes | yes | yes | [`import`](import.md) |

`.dsm` has no content restriction at all: the table's `.dsm` column is what the grammar accepts, not
a separate check.

## How the restriction is enforced

The restriction is **not** a parse. After a file's `import` lines have been removed and before the
declaration parser runs, the loader scans that file's remaining text for literal substrings, all
compared case-insensitively.

| File kind | Rejected when the text contains | Message |
| :-- | :-- | :-- |
| `.dsh` | `Shader(` **or** `ShaderFunction(` **or** `ShaderLayer(` **or** `ShaderLayerBlend(` **or** `MaterialLayer(` **or** `MaterialLayerBlend(` | `DreamShader header '{Path}' may only declare Function/Namespace/GraphFunction/VirtualFunction blocks and imports.` |
| `.dsf` | `Shader(` | `DreamShader function file '{Path}' may only declare imports, Function/Namespace/GraphFunction/VirtualFunction blocks, and ShaderFunction/ShaderLayer/ShaderLayerBlend blocks.` |
| `.dsm` | — | — |

`ShaderFunction(` does not contain the substring `Shader(`, which is exactly why a single needle is
enough for `.dsf` while `.dsh` needs six. Likewise `MaterialLayerBlend(` does not contain
`MaterialLayer(`, so both spellings are listed.

> [!WARNING]
> Because this is a substring scan, the forbidden text is rejected wherever it appears — inside a
> line comment, inside a block comment, or inside a string literal. A `.dsh` containing the comment
> `// see Shader(Name="…")` or the literal `"Shader("` is rejected with the message above.
>
> The converse also holds. `Shader (` with a space before the parenthesis contains no forbidden
> substring and passes the scan; the declaration parser then accepts it, because it consumes the
> keyword and the `(` as separate tokens. A user-chosen block name that ends in the same characters,
> such as `MyShader(`, contains `Shader(` and trips the `.dsf` rule.

> [!NOTE]
> The scan applies to each file's **own** text. When file A imports file B, B is checked against B's
> extension rule and A against A's. A `.dsh` that imports a `.dsf` full of `ShaderFunction(` blocks
> therefore passes, and the imported blocks are compiled as part of the translation unit. The kind
> rules constrain what you write in a file, not what its import closure ends up containing.

The check lives in the editor source loader. The runtime parser entry point is extension-agnostic:
calling it directly on the text of a `.dsh` will happily parse a `Shader(…)` block out of it. See
[`DreamShaderParser.h`](../api/parser.md).

## File discovery

Files are found by recursive scans of the source directory. Directories are not returned, and every
result is normalized to a full path.

| Scan | Extensions | Excluded | Used for |
| :-- | :-- | :-- | :-- |
| All source files | `.dsm`, `.dsf`, `.dsh` | everything under the packages directory | dependency graph, workspace generation, editor file lists (result sorted) |
| Generatable files | `.dsm`, `.dsf` | `.dsm` files under the packages directory | batch compile / generate-all |

> [!NOTE]
> The two exclusions are not symmetric. The all-sources scan drops **every** file under the packages
> directory; the generatable scan drops only package **`.dsm`** files. A `.dsf` shipped inside
> `DShader/Packages` is therefore still picked up as a generatable file and will produce function
> assets on a generate-all.

| Directory | Default | Project setting |
| :-- | :-- | :-- |
| Source | `<Project>/DShader` | *Source Directory* |
| Packages | `<Source>/Packages` | derived from *Source Directory*; not separately configurable |
| Generated shaders | `<Project>/Intermediate/DreamShader/GeneratedShaders` | *Generated Shader Directory* |

A configured path that is relative is resolved against the project directory. All three directories
are created at module startup. See [Project settings](../settings/project.md) and
[Packages](../tools/packages.md).

## What each kind generates

| Kind | Compiled as | Produces |
| :-- | :-- | :-- |
| `.dsm` | material entry point | the `UMaterial` (or thin instance) of its `Shader` block, plus every function asset it declares |
| `.dsf` | asset entry point | the function assets it declares |
| `.dsh` | not an entry point | nothing; a header is consumed through [`import`](import.md) |

A `.dsm` with no `Shader` block is still compilable — it produces whatever function assets it does
declare.

## Notes

- **Extensions are compared case-insensitively.** `M_Water.DSM` is a material file.
- **At most one `Shader` per translation unit.** Imports are inlined into a single text before
  parsing, so the limit spans the whole transitive import closure, not the individual file. A second
  `Shader` fails with `Only one top-level Shader block is currently supported.`
- A `.dsh` is the natural home for `VirtualFunction` declarations; the editor's *Create Virtual
  Function* action writes one under `DShader/VirtualFunctions`. See
  [VirtualFunction tools](../tools/virtual-function-tools.md).
- Nothing prevents a `.dsf` or `.dsh` from being imported by any other kind; extension only decides
  the default extension of an unsuffixed import specifier and the content rule applied to the file
  itself.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

| Message | Cause |
| :-- | :-- |
| `DreamShader header '{Path}' may only declare Function/Namespace/GraphFunction/VirtualFunction blocks and imports.` | a `.dsh` whose text contains one of the six forbidden substrings |
| `DreamShader function file '{Path}' may only declare imports, Function/Namespace/GraphFunction/VirtualFunction blocks, and ShaderFunction/ShaderLayer/ShaderLayerBlend blocks.` | a `.dsf` whose text contains `Shader(` |
| `DreamShader could not read '{Path}'.` | the file exists in the dependency graph but could not be loaded |
| `DreamShader import '{Specifier}' referenced from '{Path}' could not be resolved.` | see [`import`](import.md) |
| `DreamShader import cycle detected at '{Path}'.` | see [`import`](import.md) |
| `A top-level Shader, Function, GraphFunction, Namespace, ShaderFunction, ShaderLayer, ShaderLayerBlend, or VirtualFunction block was not found.` | the translation unit declares no top-level block |
| `Only one top-level Shader block is currently supported.` | a second `Shader` block anywhere in the import closure |

Generation-stage messages about the wrong file kind reaching a generator are listed on
[Shader](shader.md#diagnostics) and in the [diagnostics index](../diagnostics/index.md).

## Example

```text
<Project>/DShader/
├── Materials/
│   └── M_Water.dsm
├── Functions/
│   └── F_Tint.dsf
├── Shared/
│   └── Common.dsh
└── Packages/
    └── @typedreammoon/
        └── dream-noise/
            └── Library/
                └── Noise.dsh
```

```c
// DShader/Shared/Common.dsh — header: helpers only
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

```c
// DShader/Functions/F_Tint.dsf — function file: may declare ShaderFunction
import "Shared/Common.dsh";

ShaderFunction(Name="Functions/F_Tint")
{
    Inputs = {
        vec3 InColor;
        opt float Strength = 1.0;
    }

    Outputs = {
        vec3 OutColor;
    }

    Graph = {
        OutColor = InColor * Strength;
    }
}
```

```c
// DShader/Materials/M_Water.dsm — material file: may declare Shader
import "Functions/F_Tint.dsf";

Shader(Name="Materials/M_Water")
{
    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = vec3(0.1, 0.3, 0.6);
    }
}
```

Compiling `M_Water.dsm` produces both assets:

```text
/Game/Materials/M_Water        UMaterial
/Game/Functions/F_Tint         UMaterialFunction
```

## See also

- [`import`](import.md) — how the files above are assembled into one translation unit
- [Shader](shader.md) — the `.dsm`-only top-level block
- [ShaderFunction](shader-function.md) — the block a `.dsf` normally holds
- [VirtualFunction](virtual-function.md) — declaring an existing asset from a `.dsh`
- [Lexical elements](lexical.md) — comments and strings, which the kind scan does not understand
- [Packages](../tools/packages.md) — `DShader/Packages` and the `@scope/name` layout
- [Project settings](../settings/project.md) — *Source Directory* and *Generated Shader Directory*
- [Commandlet](../tools/commandlet.md) — compiling files headlessly
- [Testing](../contributing/testing.md) — the `Tests/Corpus` fixture format
