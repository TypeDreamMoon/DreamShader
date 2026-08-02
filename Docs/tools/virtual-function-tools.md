# VirtualFunction tools

> [DreamShader](../index.md) » [Tools](index.md) » **VirtualFunction tools**

The editor actions that generate, open and refresh a
[`VirtualFunction`](../language/virtual-function.md) declaration for an existing `UMaterialFunction`
asset, plus the startup service that keeps those declarations in sync with the assets they name.

| | |
| :-- | :-- |
| Attaches to | `UMaterialFunction`, `UMaterialFunctionMaterialLayer`, `UMaterialFunctionMaterialLayerBlend` |
| Menu location | Content Browser context menu ▸ *DreamShader* ▸ **VirtualFunction**, and the Material Editor **DreamShader** toolbar combo |
| Writes | `<SourceDirectory>/VirtualFunctions/<Name>.dsh`, UTF-8 without BOM |
| Since | `1.2.0` (the declaration) · `1.2.1` (the menu and the `.dsh` writer) · `1.2.2` (reuse, *OpenVirtualFunction*, startup sync) |

## Context menu

The **VirtualFunction** section is built fresh on every right-click. Before building it, the editor
searches every project source file for a `VirtualFunction` declaration whose resolved asset matches
the selected `UMaterialFunction`. **The entries you get depend on the answer.**

### When a declaration already exists

| Entry name | Label | Tooltip | Icon | Effect |
| :-- | :-- | :-- | :-- | :-- |
| `DreamShader.OpenVirtualFunction` | **OpenVirtualFunction** | "Open the existing DreamShader VirtualFunction definition in VSCode." | `Icons.OpenInExternalEditor` | Opens the file that holds the declaration, positioned at the declaration's line and column |
| `DreamShader.CopyVirtualFunctionReference` | **Copy Virtual Function Reference** | "Copy a DreamShader Graph call that references this existing VirtualFunction." | `GenericCommands.Copy` | Copies a call built from the **declared** name, not from the asset name |

No other entry is offered — in particular there is no *Create*, because one already exists.

### When no declaration exists

| Entry name | Label | Tooltip | Icon | Effect |
| :-- | :-- | :-- | :-- | :-- |
| `DreamShader.CopyVirtualFunction` | **CopyVirtualFunction** | "Copy a complete DreamShader VirtualFunction declaration for this Material Function." | `GenericCommands.Copy` | Copies the whole declaration to the clipboard |
| `DreamShader.CreateVirtualFunction` | **CreateVirtualFunction** | "Create a .dsh file containing the VirtualFunction declaration." | `Icons.Save` | Writes the declaration to a new `.dsh` and opens it |
| `DreamShader.CopyVirtualFunctionCall` | **CopyVirtualFunctionCall** | "Copy a DreamShader Graph call example for this VirtualFunction." | `GenericCommands.Copy` | Copies a call example to the clipboard |

The match is on the declaration's resolved asset object path, compared case-insensitively; the first
match wins. Because the lookup reads the files, a declaration written by hand a moment ago is found
immediately — nothing is cached from startup.

> [!WARNING]
> The lookup **re-enumerates and re-lexes every `.dsm`, `.dsh` and `.dsf`** outside
> `DShader/Packages`, from disk, on every right-click of a Material Function asset. The cost scales
> with the number of project source files and is paid before the context menu appears. There is no
> cache and no way to disable it short of `-NoDreamShaderEditorBridge`, which removes the menu
> entirely.

## Actions

Runtime substitutions are shown as `{Placeholder}` throughout this page.

| Action | Success toast | Failure toasts |
| :-- | :-- | :-- |
| **CopyVirtualFunction** | `Copied VirtualFunction definition for {Asset}.` — the full text is also logged at Display | `DreamShader could not find the selected Material Function.` · `DreamShader failed to build VirtualFunction: {Error}` |
| **CreateVirtualFunction** | `Created VirtualFunction file: {File}` | asset gone · build failure · `DreamShader failed to create directory: {Directory}` · `DreamShader failed to write VirtualFunction file: {File}` · `Created VirtualFunction file but could not open it: {File}` |
| **OpenVirtualFunction** | `Opened VirtualFunction definition: {File}` | asset gone · `DreamShader could not find a VirtualFunction definition for {Asset}.` · `DreamShader could not open VirtualFunction file: {File}` |
| **Copy Virtual Function Reference** | `Copied VirtualFunction reference for {Name}.` — the **declared** name, not the asset name | asset gone · not found · `DreamShader failed to build VirtualFunction reference: {Error}` |
| **CopyVirtualFunctionCall** | `Copied VirtualFunction call for {Asset}.` | asset gone · `DreamShader failed to build VirtualFunction call: {Error}` |

> [!NOTE]
> **CreateVirtualFunction on an asset that already has a declaration silently redirects to
> OpenVirtualFunction.** It never writes a duplicate file. The toast you get is
> `Opened VirtualFunction definition: {File}`, not a "created" message.

## Where the file is written

| Aspect | Value |
| :-- | :-- |
| Directory | `<SourceDirectory>/VirtualFunctions/` — `DShader/VirtualFunctions/` by default |
| Preferred file name | `<sanitized MaterialFunction name>.dsh` |
| On collision | `<sanitized name>_<crc32 of the asset's object path>.dsh`, the CRC printed as eight lowercase hex digits |
| Encoding | UTF-8 without BOM |
| After writing | opened through the preferred-editor chain — VSCode, then the OS default editor, then Notepad |

A `.dsh` written here is picked up by the source-directory watcher like any other header, so the
materials that import it recompile automatically.

## The generated declaration

```c
VirtualFunction(Name="<SanitizedName>")
{
    Options = {
        Asset = <asset-literal>;
        Description = "Generated from <full object path>";
    }

    Inputs = {
        [opt ]<type> <Name>[ = <default>][ [ Description="…"; SortPriority=<n>; ] ];
        …
    }

    Outputs = {
        <type> <Name>[ [ Description="…"; SortPriority=<n>; ] ];
        …
    }
}
```

The body is tab-indented. A declaration is produced only when the asset exposes at least one output.

### Asset literal

| Object path begins with | Emitted as |
| :-- | :-- |
| `/Game/` | `Path(Game, "<rest>")` |
| `/Engine/` | `Path(Engine, "<rest>")` |
| a content plugin's mount point (longest match over the enabled content plugins) | `Path(Plugins.<PluginName>, "<rest>")` |
| anything else | the raw object path, unwrapped |

See [Path](../parameters/path.md) for the accepted root spellings.

### Input types

The mapping from Unreal's function-input types to DreamShaderLang tokens is exhaustive:

| `EFunctionInputType` | Token |
| :-- | :-- |
| `FunctionInput_Scalar` | `float` |
| `FunctionInput_Vector2` | `float2` |
| `FunctionInput_Vector3` | `float3` |
| `FunctionInput_Vector4` | `float4` |
| `FunctionInput_Texture2D` | `Texture2D` |
| `FunctionInput_TextureCube` | `TextureCube` |
| `FunctionInput_Texture2DArray` | `Texture2DArray` |
| `FunctionInput_VolumeTexture` | `VolumeTexture` |
| `FunctionInput_MaterialAttributes` | `MaterialAttributes` |
| `FunctionInput_Substrate` | `Substrate` |
| `FunctionInput_StaticBool` | `StaticBool` — kept distinct so callers can pass a static bool |
| `FunctionInput_Bool` | `bool` |
| *(any other value)* | `float4` |

### Optional inputs and defaults

An input is prefixed `opt` when the asset marks it as using its preview value as the default. The
default literal is rendered from that preview value:

| Input type | Default literal |
| :-- | :-- |
| Scalar | the float value |
| StaticBool, Bool | `true` / `false` |
| Vector2 | `float2(x, y)` |
| Vector3 | `float3(x, y, z)` |
| Vector4 | `float4(x, y, z, w)` |
| anything else | *(empty — the ` = ` suffix is omitted entirely)* |

> [!WARNING]
> A texture or material-attributes input marked optional gets the `opt` prefix but **no default
> value**, because there is no literal syntax for one. Supply the argument at the call site, or edit
> the declaration.

### Metadata suffix

| Key | Emitted when |
| :-- | :-- |
| `Description` | the asset's description for that parameter is non-empty |
| `SortPriority` | the asset's sort priority differs from the parameter's own index |

### Identifier sanitization

| Rule | Detail |
| :-- | :-- |
| First character | must be a letter or `_`; anything else is replaced with `_` |
| Later characters | must be alphanumeric or `_`; anything else is replaced with `_` |
| Runs of `_` | consecutive underscores collapse to one |
| All-underscore result | becomes `DreamShaderSymbol` |
| Empty input | becomes `DreamShaderSymbol` |
| A name that degraded to `DreamShaderSymbol` | falls back to `Input<N>`, `Output<N>` or `VirtualFunction<N>`, `<N>` being the 1-based index |
| Duplicate names in one declaration | uniquified with `_2`, `_3`, … |

Non-ASCII letters are preserved — the sanitizer is Unicode-aware, not ASCII-only.

Inside emitted string literals, `\`, `"`, carriage return, line feed and tab are escaped.

## The call example

**CopyVirtualFunctionCall** and **Copy Virtual Function Reference** both produce:

```c
<FunctionName>(<arg>, <arg>, …, OutputIndex=0)
```

| Rule | Detail |
| :-- | :-- |
| Argument per input | the input's sanitized, uniquified name |
| Optional inputs | the literal token `default` |
| Trailing argument | `OutputIndex=0`, always appended, even for a single-output function |

| Error | Cause |
| :-- | :-- |
| `VirtualFunction name cannot be empty.` | the declared name was empty |
| `VirtualFunction '{Name}' does not expose any outputs.` | the function exposes no outputs |
| `No MaterialFunction asset was provided.` | a null asset reached the builder |
| `MaterialFunction '{Name}' does not have a valid package path.` | the asset has no package to build a literal from |
| `MaterialFunction '{Name}' does not expose any outputs.` | the declaration builder found no outputs |

The copied text is a template. Replace the placeholder argument names with real `Graph` values, and
change `OutputIndex` when you want a different output. See [Calls](../graph/calls.md).

## Startup sync service

Once per editor session, at bridge startup, DreamShader re-reads every `VirtualFunction` declaration
in the project, rebuilds it from the live asset, and writes the file back when the text changed.

> [!WARNING]
> **There is no watcher for this.** Editing a `UMaterialFunction`'s inputs or outputs does not
> refresh the declaration; nor does saving the `.dsh`. The declarations are re-synchronized only on
> the next editor start.

| Aspect | Behaviour |
| :-- | :-- |
| Scanned files | every `.dsm`, `.dsh` and `.dsf` under the source directory, excluding `DShader/Packages` |
| Keyword matching | the bare word `VirtualFunction`, matched **case-sensitively**, with identifier-boundary checks |
| Skipped regions | quoted strings with `\` escapes, `//` line comments and `/* */` block comments |
| Block extraction | a balanced `( … )` followed by a balanced `{ … }`; an optional trailing `;` is absorbed into the block's range |
| Validation | each extracted block is re-parsed and must yield **exactly one** `VirtualFunction`, whose `Options.Asset` must resolve |
| Comparison | on normalized text — CRLF and CR become LF, then the whole text is trimmed — so line-ending and surrounding-whitespace differences never trigger a rewrite |
| Rewrite order | replacements are applied back to front, by descending start offset, so earlier offsets stay valid |
| Write-back | the whole file, UTF-8 without BOM |

Because the scanner is a hand-rolled lexer rather than the full parser, it finds declarations
anywhere a file may legally hold one — including inside a `.dsm` alongside a `Shader` block.

### Sync diagnostics

Every message below is recorded with stage `virtualFunctionSync`, code `virtual-function-sync`,
source `DreamShader VirtualFunction`, and severity `error` — the only severity the plugin produces.
They reach `diagnostics.json`, the sharded `diagnostics/` directory and `bridge.db`.

| Message | Cause |
| :-- | :-- |
| `DreamShader could not read VirtualFunction source file '{File}'.` | the file could not be read; reported at line 1, column 1 |
| `VirtualFunction attributes are missing a closing ')'.` | the attribute list is unbalanced |
| `VirtualFunction body is missing a closing '}'.` | the body braces are unbalanced |
| `VirtualFunction declaration is invalid: {ParserError}` | the extracted block did not parse, or did not contain exactly one `VirtualFunction` |
| `VirtualFunction '{Name}' asset reference is invalid: {Error}` | `Options.Asset` could not be resolved; the raw literal is recorded as the asset path |
| `VirtualFunction '{Name}' references missing MaterialFunction '{Path}'.` | the asset path resolved but the object failed to load |
| `VirtualFunction '{Name}' could not be refreshed from MaterialFunction '{Path}': {Error}` | the declaration builder failed for the live asset |
| `DreamShader failed to update VirtualFunction source file '{File}'.` | the rewritten file could not be written; reported at line 1, column 1 |

`Expected exactly one VirtualFunction block.` is the parser error text carried by the fourth row when
a block contains zero or several declarations.

### Reporting

| Log | When |
| :-- | :-- |
| `DreamShader refreshed {Count} VirtualFunction definition(s) in '{File}'.` | a file was rewritten |
| `DreamShader scanned {Scanned} VirtualFunction definition(s), refreshed {Refreshed}, reported {Issues} issue(s).` | at the end, only when something happened |

Files that hold at least one declaration and produced no diagnostics have their diagnostics cleared;
files that produced diagnostics have them set.

## Example

Right-click `/Game/Functions/MF_Weathering` (a `UMaterialFunction` with one scalar and one texture
input, and one output) ▸ *DreamShader* ▸ **CreateVirtualFunction**. The editor writes
`<Project>/DShader/VirtualFunctions/MF_Weathering.dsh` and opens it:

```c
VirtualFunction(Name="MF_Weathering")
{
	Options = {
		Asset = Path(Game, "Functions/MF_Weathering");
		Description = "Generated from /Game/Functions/MF_Weathering.MF_Weathering";
	}

	Inputs = {
		opt float Amount = 0.5 [
			Description="Weathering strength";
		];
		Texture2D Mask;
	}

	Outputs = {
		float3 Result;
	}
}
```

**CopyVirtualFunctionCall** on the same asset copies:

```c
MF_Weathering(default, Mask, OutputIndex=0)
```

Using it from a material:

```c
import "VirtualFunctions/MF_Weathering.dsh";

Shader(Name="Materials/M_Rock")
{
    Properties = {
        Texture2D MaskTex = Path(Game, "Textures/T_Mask");
    }

    Outputs = {
        float3 Color;
        Base.BaseColor = Color;
    }

    Graph = {
        Color = MF_Weathering(0.8, MaskTex, OutputIndex=0);
    }
}
```

## See also

- [VirtualFunction](../language/virtual-function.md) — the declaration's grammar and its `Options` keys
- [Options](../language/options.md) — every key the `Options` section accepts
- [Editor integration](editor-integration.md) — where the menu attaches and what else lives there
- [Calls](../graph/calls.md) — calling a `VirtualFunction` from a `Graph`
- [Inputs / Outputs / Results](../language/inputs-outputs.md) — `opt`, defaults and metadata in a declaration
- [Path](../parameters/path.md) — the `Path(Root, "…")` literal the `Asset` key uses
- [Import](../language/import.md) — how a material picks up the generated `.dsh`
- [Decompiler](decompiler.md) — the same declaration builder, used for every called function
- [Bridge](bridge.md) — where the sync diagnostics are published
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
