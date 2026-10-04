# Path(…) asset references

> [DreamShader](../index.md) » [Parameters](index.md) » **Path(…)**

The asset-reference form: a package root plus a relative path, resolved to a full Unreal object path
at generation time.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` — the `= <default>` of a texture property, an object-valued metadata entry, a `UE.CollectionParam` argument, an object-valued `Settings` value, or a `VirtualFunction`'s `Options.Asset` |
| Kind | value form |
| Generates | an `FSoftObjectPath`-style object path such as `/Game/Textures/T_X.T_X` |
| Since | `1.2.0` (`Plugin.` / `Plugins.` roots); bare quoted paths since `1.5.0`; `Class'…'` references since `1.9.0` |

## Synopsis

```c
Path( <root> , "<relative-path>" )
Path( "<absolute-path>" )
"<absolute-path>"
<Class>'<absolute-path>'
```

`<root>` and the path may each be written quoted or bare.

```c
Texture2D A = Path(Game, "Textures/T_X");
Texture2D B = Path("Plugin.MyPlugin", "Textures/T_X");
Texture2D C = Path("/Game/Textures/T_X");
Texture2D D = "/Game/Textures/T_X";
Texture2D E = Texture2D'/Game/Textures/T_X.T_X';
```

The front end keeps a reference as the text it was written as; it is resolved when the asset is built,
because only the editor knows which plugins are mounted.

## Content Browser references

*(since 1.9.0)*

Right-clicking an asset and choosing **Copy Reference** puts Unreal's *export* form on the clipboard:

```text
/Script/Engine.Texture2D'/Game/Cloud/Texture/T_VolumeCloud_03.T_VolumeCloud_03'
```

Paste it in as it stands. The resolver strips the `<Class>'…'` shell before anything else and uses
the object path inside the quotes; the class in front of the quotes is used only for the check
below. Three spellings of the shell are recognized:

| Written | Class read as |
| :-- | :-- |
| `/Script/Engine.Texture2D'/Game/…'` | `Texture2D` |
| `Texture2D'/Game/…'` — the older engine spelling | `Texture2D` |
| `'/Game/…'` — quotes with no class | *(none)* |

The shell may be pasted bare, wrapped in quotes, or handed to `Path(…)` — all four of these name the
same asset:

```c
Texture2D A = Texture2D'/Game/Textures/T_X.T_X';
Texture2D B = "Texture2D'/Game/Textures/T_X.T_X'";
Texture2D C = Path("Texture2D'/Game/Textures/T_X.T_X'");
Texture2D D = Path(Game, "Texture2D'/Game/Textures/T_X.T_X'");   // the root is ignored
```

The path inside the quotes is **absolute**, so a root written beside it is ignored, exactly as it is
for any other absolute path. See [Roots and absolute paths](#roots-and-absolute-paths).

A shell is only recognized when it is well formed: the text must end in `'`, there must be an
earlier `'`, and what lies between them must be non-empty and contain no further `'`. Anything else
is left alone and reaches the resolver's ordinary grammar — and its ordinary diagnostics — unchanged.

### Class checking

The class named by the shell is checked against the slot it is being assigned to, before the asset
is loaded:

| Slot | Written class | Outcome |
| :-- | :-- | :-- |
| the default of a texture property declaring its dimension (`Texture2D`, `TextureCube`, `Texture2DArray`, `Texture3D`, `VolumeTexture`, and `TextureSampleParameter2D` / `2DArray` / `Cube` / `Volume` / `SubUV`) | a texture class of another dimension | [`DSH1044`](../diagnostics/DSH1xxx.md#dsh1044), naming both |
| the default of any texture property | a class DreamShader knows is not a texture (`MaterialFunction`, `CurveLinearColor`, `Font`, …) | [`DSH1043`](../diagnostics/DSH1xxx.md#dsh1043) |
| any object property — a [metadata](metadata.md#object-properties) entry, a texture-sample default, a `UE.CollectionParam` collection, an object setting, a `VirtualFunction`'s `Options.Asset` | a class unrelated to the one the slot takes | [`DSH1045`](../diagnostics/DSH1xxx.md#dsh1045), naming both |
| anything | a class this build does not have, or one whose dimension DreamShader does not model (`SparseVolumeTexture`, `TextureCubeArray`, `TextureCollection`, `RuntimeVirtualTexture`) | accepted — stripped and forgotten |

`DSH1043` and `DSH1044` are raised right after the file is parsed; `DSH1045` when the asset is built.

> [!NOTE]
> An unrecognized class is never an error. Unreal itself ignores the class when it resolves an export
> path, and a project is free to name classes DreamShader has never heard of, so a prefix that cannot
> be placed is dropped rather than refused.

`TextureObjectParameter`, which declares no dimension of its own, and
`TextureSampleParameterCubeArray` are only checked against the "is this a texture at all" half of
the table; `TextureObjectParameter` takes its real dimension from the asset that loads.

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

All spellings are matched case-insensitively. Any other first segment is
[`DSH8126`](../diagnostics/DSH8xxx.md#dsh8126).

**Segments after the root are appended as folders.** `Path("Game/Textures", "T_X")` resolves to
`/Game/Textures/T_X`, and `Path("Plugin/MyPlugin/Materials", "MF_X")` to
`/MyPlugin/Materials/MF_X`.

A plugin root resolves through the plugin's own mounted asset path, normalized the same way (backslashes
folded, trailing `/` stripped, a leading `/` added). If that path is empty or just `/`, `/<PluginName>`
is used instead. The plugin has to be known, enabled, able to contain content, have a `Content`
folder on disk and — on UE 5.6 and newer — be mounted (`DSH8118` … `DSH8122`).

### Roots and absolute paths

*(changed in 1.9.0)*

A root is only consulted when the asset path is **relative**. Written beside an absolute path — one
that begins with `/`, including the path inside a `Class'…'` shell — the root is **ignored**, and it
is not validated either: `Path(Plugin.NotInstalled, "/Game/Textures/T_X")` resolves to
`/Game/Textures/T_X.T_X` without complaining about the plugin.

Before 1.9.0 the texture-default resolver prepended the root anyway, so
`Texture2D T = Path(Game, "/Game/Textures/T_X");` resolved to `/Game/Game/Textures/T_X.T_X` and then
failed to load. Since 1.9.0 the root is dropped.

> [!NOTE]
> If a source relied on the old prepending — a relative path deliberately written with a leading `/`,
> as in `Path(Game, "/Textures/T_X")` — it now resolves to `/Textures/T_X.T_X` and fails to load.
> Drop the leading `/`: `Path(Game, "Textures/T_X")`.

## Object-path completion

After the root and the relative path are joined, the result is completed to a full object path: if the
text after the last `/` contains no `.`, the asset name is appended after a `.`.

| Written | Resolved |
| :-- | :-- |
| `Path(Game, "Textures/T_X")` | `/Game/Textures/T_X.T_X` |
| `Path(Game, "Textures/T_X.T_X")` | `/Game/Textures/T_X.T_X` |
| `"/Game/Textures/T_X"` | `/Game/Textures/T_X.T_X` |
| `Texture2D'/Game/Textures/T_X.T_X'` | `/Game/Textures/T_X.T_X` |
| `Path(Game, "Texture2D'/Game/Textures/T_X.T_X'")` | `/Game/Textures/T_X.T_X` — the root is ignored |

The completed path is finally validated by Unreal's own object-path validator, whose message is
reported, without a code of its own, inside the diagnostic of the slot.

<a id="two-resolvers"></a>

## The resolver

*(since 2.0.0)* There is **one** resolver. 1.x had two — one for texture defaults, one for metadata and
the rest — with different accepted forms and different messages; every reference site now goes
through the shared one, and the diagnostic of the slot quotes its code.

| Where the reference appears | Reported as |
| :-- | :-- |
| The `= <default>` of a compact texture token or `TextureObjectParameter` | [`DSH8271`](../diagnostics/DSH8xxx.md#dsh8271) when it does not resolve, [`DSH8218`](../diagnostics/DSH8xxx.md#dsh8218) when it does not load |
| The `= <default>` of a `const` texture, or of a `TextureSampleParameter*` token | the node's `Texture` property: [`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213) when it does not resolve; a failed load is silent (below) |
| An object-valued [metadata](metadata.md#object-properties) entry — `[Texture=…]`, `[VirtualTexture=…]`, … | `DSH8213` |
| `UE.CollectionParam(Collection = …)` / `UE.CollectionParameter(Asset = …)` | `DSH8213`; the named parameter must exist in the collection ([`DSH8254`](../diagnostics/DSH8xxx.md#dsh8254)) |
| An object-valued `Settings` value — `PhysicalMaterial = Path(…)` | [`DSH8215`](../diagnostics/DSH8xxx.md#dsh8215), quoting `DSH7125` |
| A `VirtualFunction`'s `Options = { Asset = …; }` | [`DSH8270`](../diagnostics/DSH8xxx.md#dsh8270) when it does not resolve, [`DSH8219`](../diagnostics/DSH8xxx.md#dsh8219) when it does not load |

### What the resolver accepts

| Form | |
| :-- | :-- |
| `Path(root, "path")` | accepted |
| `Path("/absolute/path")` | accepted |
| `"/absolute/path"` (bare **quoted**) | accepted |
| `/absolute/path` (bare **unquoted**) | accepted. As the default of a texture declaration it is loaded as written *(since 2.0.0; the 1.x texture-default resolver refused it)*; quote it all the same, because unquoted it is read as tokens, and a segment such as `1Folder` is a malformed number ([`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105)) |
| `Class'/absolute/path'`, quoted or bare | accepted *(since 1.9.0)* |
| `Path(…)` with 3+ arguments | [`DSH8130`](../diagnostics/DSH8xxx.md#dsh8130) *(since 2.0.0 for a texture default, which used to stop reading after the second)* |
| Root **and** an absolute asset path | the root is **ignored**; the absolute path wins |
| Plugin name | must survive Unreal's object-name sanitizer unchanged |
| Plugin `Content` directory must exist | checked |
| Plugin content must be mounted | checked *(since UE 5.6)* |

A texture declaration's default that is no reference at all — `Texture2D T = Foo;` — is not refused
as a spelling any more; it is simply not found when the material is built (`DSH8218`).

## String escapes

Quoted paths are string literals, read by the lexer *(since 2.0.0)*: `\\`, `\"`, `\n`, `\r`, `\t` and
`\0` are recognized, and any other `\X` is [`DSH2104`](../diagnostics/DSH2xxx.md#dsh2104) — 1.x read it
as the literal `X`. A backslash in a path is therefore written `\\`, and the resolver then folds it to
`/`. See [Lexical elements](../language/lexical.md).

## Load behaviour

Resolving a path produces text. Whether the asset then loads is a separate step, and the outcome
depends on the slot:

| Slot | Asset fails to load |
| :-- | :-- |
| A compact texture token's or `TextureObjectParameter`'s default | `DSH8218` |
| A `const` texture token's default | **silently written as `nullptr`** *(since 2.0.0)* |
| A texture-sample token's default | **silently written as `nullptr`**; the node then takes the engine's default texture *(since 2.0.0)* |
| A metadata object property named `Texture` or `TextureObject` whose type is a `UTexture` subclass | **silently written as `nullptr`, reported as success** |
| Any other object property, including a `UE.CollectionParam` collection | `DSH8213`, quoting [`DSH7138`](../diagnostics/DSH7xxx.md#dsh7138) |
| An object setting | `DSH8215`, quoting `DSH7138` |
| A `VirtualFunction`'s asset | `DSH8219` |

> [!WARNING]
> The silent-null case is the one to watch: `[Texture = Path(Game, "Typo")]` — and since 2.0.0
> `TextureSampleParameter2D T = Path(Game, "Typo");` — generates without any diagnostic and leaves
> the sampler on a texture nobody chose. A path that resolves is not a path that exists.

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| [`DSH8127`](../diagnostics/DSH8xxx.md#dsh8127) | the reference is empty after trimming |
| [`DSH8128`](../diagnostics/DSH8xxx.md#dsh8128) | a value beginning with `Path(` that does not end with `)` |
| [`DSH8129`](../diagnostics/DSH8xxx.md#dsh8129) | an unclosed `"` inside the argument list |
| `DSH8130` | three or more arguments — an empty `Path()` counts as one empty argument and is `DSH8131` instead |
| [`DSH8131`](../diagnostics/DSH8xxx.md#dsh8131) | the asset-path argument is empty |
| [`DSH8123`](../diagnostics/DSH8xxx.md#dsh8123) | a relative path with no root, including a bare quoted relative path |
| `DSH8126` | the first root segment is none of the six accepted spellings |
| [`DSH8124`](../diagnostics/DSH8xxx.md#dsh8124) | a `Plugin.<Name>` / `Plugins.<Name>` root whose name is empty or changed by Unreal's object-name sanitizer |
| [`DSH8125`](../diagnostics/DSH8xxx.md#dsh8125) | the same, for a `Plugin/<Name>` / `Plugins/<Name>` root |
| [`DSH8118`](../diagnostics/DSH8xxx.md#dsh8118) | the plugin manager does not know the plugin |
| [`DSH8119`](../diagnostics/DSH8xxx.md#dsh8119) | the plugin exists but is disabled |
| [`DSH8120`](../diagnostics/DSH8xxx.md#dsh8120) | the plugin cannot contain content |
| [`DSH8121`](../diagnostics/DSH8xxx.md#dsh8121) | the plugin's `Content` folder is missing on disk |
| [`DSH8122`](../diagnostics/DSH8xxx.md#dsh8122) | the plugin's content is not mounted *(since UE 5.6)* |
| [`DSH8132`](../diagnostics/DSH8xxx.md#dsh8132) | the joined path has no `/`, or ends with one |
| *(no code)* | Unreal's object-path validator refused the completed path; its message is quoted by the slot's diagnostic |
| `DSH1043` / `DSH1044` | a texture default's `Class'…'` names a non-texture / a texture of another dimension |
| `DSH1045` | a `Class'…'` names a class unrelated to the object property |
| `DSH2104` | an unknown escape in a quoted path |

Each of these reaches you inside the diagnostic of its slot — see [The resolver](#two-resolvers). The
complete list lives in the [diagnostics index](../diagnostics/index.md).

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

        // Pasted straight from the Content Browser's "Copy Reference".
        Texture2D H = /Script/Engine.Texture2D'/Game/Textures/T_White.T_White';
        Texture2D I = "Texture2D'/Game/Textures/T_White.T_White'";

        // An object-valued metadata entry.
        TextureSampleParameter2D Mask [Texture = Path(Game, "Textures/T_Mask")];

        // A UE builtin argument.
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
H  /Game/Textures/T_White.T_White
I  /Game/Textures/T_White.T_White
```

A reference is resolved when the property it belongs to is built. This `Graph` reads only `Wind`, so
the textures are dropped as unread ([`DSH4390`](../diagnostics/DSH4xxx.md#dsh4390), info) and only the
collection is resolved; read them and each resolves as listed.

## See also

- [Parameters](index.md) — the hub and the decision table
- [Compact type tokens](compact-types.md) — the texture tokens whose defaults use this form
- [Parameter node tokens](parameter-nodes.md) — which tokens accept `= Path(…)` and which do not
- [Metadata block](metadata.md) — object-valued metadata entries and the silent-null texture case
- [UE builtins](../builtins/ue.md) — `UE.CollectionParam` and its `Collection` argument
- [VirtualFunction](../language/virtual-function.md) — `Options = { Asset = Path(…); }`
- [Lexical elements](../language/lexical.md) — string literals and escape sequences
- [Asset paths](../generation/asset-paths.md) — the *output* side: `Name=` + `Root=` → package path
- [Diagnostics index](../diagnostics/index.md) — every code
