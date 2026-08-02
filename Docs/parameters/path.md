# Path(…) asset references

> [DreamShader](../index.md) » [Parameters](index.md) » **Path(…)**

The asset-reference form: a package root plus a relative path, resolved to a full Unreal object path
at generation time.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` — the `= <default>` of a texture property, an object-valued metadata entry, a `UE.CollectionParam` argument, or a `VirtualFunction`'s `Options.Asset` |
| Kind | value form |
| Generates | an `FSoftObjectPath`-style object path such as `/Game/Textures/T_X.T_X` |
| Since | `1.2.0` (`Plugin.` / `Plugins.` roots); bare quoted paths since `1.5.0` |

## Synopsis

```c
Path( <root> , "<relative-path>" )
Path( "<absolute-path>" )
"<absolute-path>"
```

`<root>` may be written quoted or bare. The relative path may be written quoted or bare in the
metadata resolver; a texture default's path is unquoted before use either way.

```c
Texture2D A = Path(Game, "Textures/T_X");
Texture2D B = Path("Plugin.MyPlugin", "Textures/T_X");
Texture2D C = Path("/Game/Textures/T_X");
Texture2D D = "/Game/Textures/T_X";
```

## Roots

The root argument is unquoted, `\` is folded to `/`, and leading and trailing `/` are stripped; the
result is then split on `/`. The **first** segment names the root.

| Root spelling | Resolves to |
| :-- | :-- |
| `Game` | `/Game` |
| `Engine` | `/Engine` |
| `Plugin.<Name>` | the plugin's mounted asset path |
| `Plugins.<Name>` | the plugin's mounted asset path |
| `Plugin/<Name>` | the plugin's mounted asset path — consumes two segments |
| `Plugins/<Name>` | the plugin's mounted asset path — consumes two segments |

All spellings are matched case-insensitively. Any other first segment fails.

**Segments after the root are appended as folders.** `Path("Game/Textures", "T_X")` resolves to
`/Game/Textures/T_X`, and `Path("Plugin/MyPlugin/Materials", "MF_X")` to
`/MyPlugin/Materials/MF_X`.

A plugin root resolves through the plugin's own mounted asset path, normalized the same way (backslashes
folded, trailing `/` stripped, a leading `/` added). If that path is empty or just `/`, `/<PluginName>`
is used instead.

## Object-path completion

After the root and the relative path are joined, the result is completed to a full object path: if the
text after the last `/` contains no `.`, the asset name is appended after a `.`.

| Written | Resolved |
| :-- | :-- |
| `Path(Game, "Textures/T_X")` | `/Game/Textures/T_X.T_X` |
| `Path(Game, "Textures/T_X.T_X")` | `/Game/Textures/T_X.T_X` |
| `"/Game/Textures/T_X"` | `/Game/Textures/T_X.T_X` |

The completed path is finally validated by Unreal's own object-path validator, whose message is
reported verbatim when it fails.

## Two resolvers

There are **two independent implementations** with different accepted forms and different error text.
Which one runs depends on where the reference appears.

| Where the reference appears | Resolver | Messages begin |
| :-- | :-- | :-- |
| The `= <default>` of a compact texture token, a `TextureObjectParameter`-family token, or a `TextureSampleParameter*`-family token | texture-default resolver | `Texture …` |
| An object-valued [metadata](metadata.md#object-properties) entry — `[Texture=…]`, `[Curve=…]`, `[Font=…]`, `[VirtualTexture=…]`, … | asset-reference resolver | `Asset …` |
| `UE.CollectionParam(Collection = …)` / `UE.CollectionParameter(Asset = …)` | asset-reference resolver | `Asset …` |
| A `VirtualFunction`'s `Options = { Asset = …; }` | asset-reference resolver | `Asset …` |

### Differences

| Behaviour | Texture-default resolver | Asset-reference resolver |
| :-- | :-- | :-- |
| `Path(root, "path")` | accepted | accepted |
| `Path("/absolute/path")` | accepted | accepted |
| `"/absolute/path"` (bare **quoted**) | accepted | accepted |
| `/absolute/path` (bare **unquoted**) | rejected — the value must start with `Path` or `"` | accepted |
| `Path(…)` with 3+ arguments | the third argument is never read; the missing `)` is reported | rejected with an explicit arity message |
| Root **and** an absolute asset path | the root is still prepended, producing `/Game/Game/…` | the root is **ignored**; the absolute path wins |
| Plugin-name validation | characters must be alphanumeric or `_` | the name must survive Unreal's object-name sanitizer unchanged |
| Plugin `Content` directory must exist | not checked | checked |
| Plugin content must be mounted | not checked | checked *(since UE 5.6)* |

> [!WARNING]
> **Do not combine a root with an absolute path in a texture default.**
> `Texture2D T = Path(Game, "/Game/Textures/T_X");` resolves to `/Game/Game/Textures/T_X.T_X` and then
> fails to load. Write `Path(Game, "Textures/T_X")` or `Path("/Game/Textures/T_X")`. The same
> expression is accepted in a metadata entry, because that resolver drops the root when the path is
> absolute — the two forms are not interchangeable.

## String escapes

Quoted paths are string literals. Both resolvers recognize `\n`, `\r`, `\t`, `\"` and `\\`; any other
`\X` yields the literal `X`. See [Lexical elements](../language/lexical.md).

## Load behaviour

Resolving a path produces text. Whether the asset then loads is a separate step, and the outcome
depends on the slot:

| Slot | Asset fails to load |
| :-- | :-- |
| A compact texture token's default | `Texture property '{Name}' could not load asset '{Path}'.` |
| A `const` texture token's default | `Const texture property '{Name}' could not load asset '{Path}'.` |
| The engine fallback asset for a dimension | `Texture property '{Name}' could not load default {Type} asset '{Path}'.` |
| A metadata object property named `Texture` or `TextureObject` whose type is a `UTexture` subclass | **silently written as `nullptr`, reported as success** |
| Any other metadata object property | `Failed to load asset '{Path}' for '{Property}'.` |
| `UE.CollectionParam`'s `Collection` | `Could not load MaterialParameterCollection '{Path}'.` |

> [!WARNING]
> The silent-null case is the one to watch: `[Texture = Path(Game, "Typo")]` generates without any
> diagnostic and leaves the sampler unbound, which surfaces later as an Unreal shader-compile error.
> A path that resolves is not a path that exists.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this page.

### Texture-default resolver

| Message | Cause |
| :-- | :-- |
| `Texture defaults must use Path(Game\|Engine\|Plugin.PluginName, "/Folder/Asset"), Path("/Game/Folder/Asset"), or a bare "/Game/Folder/Asset".` | the value is neither a quoted string nor a call to `Path` |
| `Unexpected trailing tokens after texture Path(...) reference.` | text after the closing `)` |
| `Texture Path(...) requires a non-empty asset path.` | the asset-path argument is empty |
| `Relative texture Path(...) references require a root such as Game, Engine, or Plugin.PluginName.` | a relative path with no root, including a bare quoted relative path |
| `Unsupported texture Path root '{Root}'. Use Game, Engine, or Plugin.PluginName.` | the first root segment is none of the six accepted spellings |
| `Texture Path root '{Root}' has an invalid plugin name.` | the plugin name contains a character other than a letter, digit or `_` |
| `Texture Path root '{Root}' references plugin '{Plugin}', but no enabled plugin with that name was found.` | the plugin manager does not know the plugin |
| `Texture Path root '{Root}' references plugin '{Plugin}', but the plugin is not enabled.` | the plugin exists but is disabled |
| `Texture Path root '{Root}' references plugin '{Plugin}', but the plugin cannot contain content.` | the plugin declares no content |
| `Invalid texture asset path '{Path}'.` | the joined path has no `/`, or ends with one |
| *(Unreal's object-path validation message)* | the completed path is not a valid object path |

The caller wraps every one of these:

| Wrapper | Applied to |
| :-- | :-- |
| `Invalid texture default value '{Text}' for property '{Name}'. {Inner}` | compact texture tokens and the `TextureObjectParameter` family |
| `Invalid texture sample default value '{Text}' for property '{Name}'. {Inner}` | the eight texture-sample tokens |

### Asset-reference resolver

| Message | Cause |
| :-- | :-- |
| `Asset reference cannot be empty.` | the value is empty after trimming |
| `Asset Path(...) reference is missing a closing ')'.` | a value beginning with `Path(` that does not end with `)` |
| `Asset Path(...) contains an unterminated string literal.` | an unclosed `"` inside the argument list |
| `Asset Path(...) expects either 1 argument (/Game/... path) or 2 arguments (Game\|Engine\|Plugin.PluginName, asset path).` | three or more arguments — an empty `Path()` counts as one empty argument and fails with the next message instead |
| `Asset reference requires a non-empty path.` | the asset-path argument is empty |
| `Relative asset Path(...) references require a root such as Game, Engine, or Plugin.PluginName.` | a relative path with no root |
| `Unsupported asset Path root '{Root}'. Use Game, Engine, or Plugin.PluginName.` | the first root segment is none of the six accepted spellings |
| `Asset Path root '{Root}' has an invalid plugin name.` | the plugin name is empty, or is changed by Unreal's object-name sanitizer |
| `Asset Path root '{Root}' references plugin '{Plugin}', but no enabled plugin with that name was found.` | the plugin manager does not know the plugin |
| `Asset Path root '{Root}' references plugin '{Plugin}', but the plugin is not enabled.` | the plugin exists but is disabled |
| `Asset Path root '{Root}' references plugin '{Plugin}', but the plugin cannot contain content.` | the plugin declares no content |
| `Asset Path root '{Root}' references plugin '{Plugin}', but its Content directory does not exist: '{Dir}'.` | the plugin's `Content` folder is missing on disk |
| `Asset Path root '{Root}' references plugin '{Plugin}', but the plugin content is not mounted.` | the plugin's content is not mounted *(since UE 5.6)* |
| `Invalid asset path '{Path}'.` | the joined path has no `/`, or ends with one |
| *(Unreal's object-path validation message)* | the completed path is not a valid object path |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Docs/M_Paths")
{
    Properties = {
        // Root + relative path — the canonical form.
        Texture2D A = Path(Game, "Textures/T_White");

        // Quoted root, and extra folder segments carried by the root.
        Texture2D B = Path("Game/Textures", "T_Noise");

        // Engine content.
        TextureCube C = Path(Engine, "EngineResources/DefaultTextureCube");

        // Plugin content: both spellings resolve identically.
        Texture2D D = Path(Plugin.DreamShader, "Textures/T_Probe");
        Texture2D E = Path("Plugins/DreamShader", "Textures/T_Probe");

        // Single-argument and bare quoted forms — both must be absolute.
        Texture2D F = Path("/Game/Textures/T_White");
        Texture2D G = "/Game/Textures/T_White";

        // The asset-reference resolver, reached through metadata.
        CurveAtlasRowParameter Row = float3(0.5, 0.5, 0.5) [
            Curve = Path(Game, "Curves/CV_Ramp");
            Atlas = Path(Game, "Curves/CA_Ramps")
        ];

        // The asset-reference resolver, reached through a UE builtin argument.
        UE.CollectionParam(Collection = Path(Game, "Collections/MPC_World"),
                           Parameter  = "WindStrength") Wind;
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }
    Outputs  = { vec3 Color; Base.EmissiveColor = Color; }

    Graph = {
        Color = vec3(Wind, Wind, Wind);
    }
}
```

Resolved object paths:

```text
A  /Game/Textures/T_White.T_White
B  /Game/Textures/T_Noise.T_Noise
C  /Engine/EngineResources/DefaultTextureCube.DefaultTextureCube
D  /DreamShader/Textures/T_Probe.T_Probe          (plugin mounted asset path)
E  /DreamShader/Textures/T_Probe.T_Probe
F  /Game/Textures/T_White.T_White
G  /Game/Textures/T_White.T_White
```

## See also

- [Parameters](index.md) — the hub and the decision table
- [Compact type tokens](compact-types.md) — the texture tokens whose defaults use this form
- [Parameter node tokens](parameter-nodes.md) — which tokens accept `= Path(…)` and which do not
- [Metadata block](metadata.md) — object-valued metadata entries and the silent-null texture case
- [UE builtins](../builtins/ue.md) — `UE.CollectionParam` and its `Collection` argument
- [VirtualFunction](../language/virtual-function.md) — `Options = { Asset = Path(…); }`
- [Lexical elements](../language/lexical.md) — string literals and escape sequences
- [Asset paths](../generation/asset-paths.md) — the *output* side: `Name=` + `Root=` → package path
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
