# Decompiler

> [DreamShader](../index.md) » [Tools](index.md) » **Decompiler**

The exporter that walks an existing `UMaterial` or `UMaterialFunction` node graph and writes an
equivalent DreamShaderLang source file.

| | |
| :-- | :-- |
| Accepts | `UMaterial` · `UMaterialFunction` · `UMaterialFunctionMaterialLayer` · `UMaterialFunctionMaterialLayerBlend` |
| Produces | one `.dsm` or `.dsf` file, UTF-8 without BOM |
| Writes to | `<SourceDirectory>/Decompiled/…` unless an explicit output path is given |
| Since | `1.3.5` (Content Browser actions) |

> [!NOTE]
> Treat the decompiler as a **migration starting point**, not a round-trip guarantee. It reproduces
> the graph's structure and the parts of the node state it can express, then leaves a `// Warning:`
> comment for everything it could not. The
> [known round-trip gaps](#known-round-trip-gaps) below are the ones worth checking by hand before
> deleting the original asset.

## Invoking it

| Route | Where | Produces |
| :-- | :-- | :-- |
| Content Browser | right-click a `UMaterial` ▸ *DreamShader* ▸ **Export DSM** | `.dsm` |
| Content Browser | right-click a `UMaterialFunction`, `UMaterialFunctionMaterialLayer` or `UMaterialFunctionMaterialLayerBlend` ▸ *DreamShader* ▸ **Export DSF** | `.dsf` |
| Material Editor | the **DreamShader** toolbar combo button ▸ **Export DSM** / **Export DSF** | as above |
| Commandlet | `-run=DreamShader decompile -Asset=<object path> [-Out=<file>]` | as above |

Both editor routes require **exactly one** selected asset, write the file, and then open it in your
preferred editor. See [Editor integration](editor-integration.md#content-browser-context-menus) and
[Commandlet](commandlet.md).

A progress dialog appears after a 0.25 s delay, titled `Decompiling Material '{Asset}'...` or
`Decompiling Material Function '{Asset}'...`, with one progress frame per visited node. The dialog is
suppressed in commandlet runs.

## Output paths

| Asset class | Directory | Extension |
| :-- | :-- | :-- |
| `UMaterial` | `<SourceDirectory>/Decompiled/Materials/` | `.dsm` |
| `UMaterialFunction` | `<SourceDirectory>/Decompiled/Functions/` | `.dsf` |
| `UMaterialFunctionMaterialLayer` | `<SourceDirectory>/Decompiled/Layers/` | `.dsf` |
| `UMaterialFunctionMaterialLayerBlend` | `<SourceDirectory>/Decompiled/LayerBlends/` | `.dsf` |

`<SourceDirectory>` is the *Source Directory* project setting, `DShader` by default. The layer and
layer-blend directories are separate from `Functions`, and the class test is ordered blend-first, so
a layer blend never lands in `Layers`.

Inside the category directory the asset's **package path** becomes the relative file path, one
directory per package segment:

```text
/Game/Materials/Metal/M_Steel
  →  <Project>/DShader/Decompiled/Materials/Game/Materials/Metal/M_Steel.dsm
```

| Rule | Detail |
| :-- | :-- |
| Leading and trailing `/` | stripped from the package name |
| Illegal characters | control characters and `< > : " / \ | ? *` become `_` |
| Empty segment | becomes `Folder<N>` for a folder, `Asset<N>` for the last segment, `<N>` being the 1-based index |
| No package | the asset's own name is used as the only segment |

Passing `-Out=` to the commandlet overrides the whole computation and writes exactly where told
(after path normalization). The editor routes never take an override.

### The generated asset name

The `Name=` written **inside** the file is not the file path. It is
`Decompiled/<Category>/<package segments>` with `\ / . :` replaced by `_` in each segment:

```text
/Game/Materials/Metal/M_Steel
  →  Shader(Name="Decompiled/Materials/Game/Materials/Metal/M_Steel")
```

Recompiling the exported file therefore creates a **new** asset under
`/Game/Decompiled/Materials/…`, leaving the original untouched. Edit `Name=` and `Root=` when you are
ready to take over the original path. See [Asset paths](../generation/asset-paths.md).

## File layout

```c
// Decompiled from <full object path>
[// Warning: <text>]…

[<VirtualFunction declaration>]…

{ Shader | ShaderFunction | ShaderLayer | ShaderLayerBlend }(Name="<generated name>")
{
    [Properties = { … }]
    [Inputs     = { … }]   // function kinds only
    [Settings   = { … }]   // Shader only
    [Outputs    = { … }]
    [Graph      = { … }]
    [Layout     = { … }]
}
```

| Block kind emitted | Source asset |
| :-- | :-- |
| `Shader` | `UMaterial` |
| `ShaderFunction` | `UMaterialFunction` |
| `ShaderLayer` | `UMaterialFunctionMaterialLayer` |
| `ShaderLayerBlend` | `UMaterialFunctionMaterialLayerBlend` |

The `Settings` block of a decompiled `Shader` always begins with `Domain`, `ShadingModel` and
`BlendMode`, emitted unconditionally. Every other setting is emitted only when it differs from the
`UMaterial` class default; the round-trip set is listed on
[Shader settings](../settings/material.md). When a `Base.FrontMaterial` binding was decompiled the
shading model is forced to `Substrate`, because a Substrate material's own shading-model enum does
not describe its surface.

`Outputs` declares one variable per connected material property and binds it, in the fixed order
`EmissiveColor`, `BaseColor`, `Metallic`, `Specular`, `Roughness`, `Anisotropy`, `Opacity`,
`OpacityMask`, `Normal`, `Tangent`, `WorldPositionOffset`, `SubsurfaceColor`, `CustomData0`,
`CustomData1`, `AmbientOcclusion`, `Refraction`, `PixelDepthOffset`, `MaterialAttributes`, and — on
UE 5.4 and newer — `FrontMaterial`. Unconnected properties are skipped entirely.

## What is exported faithfully

The node walker has a curated case for each class below. Everything else falls through to the
[generic fallback](#the-ueexpression-fallback).

### Emitted as DreamShaderLang syntax

| `UMaterialExpression` class | Emitted as |
| :-- | :-- |
| `Constant` | a float literal |
| `Constant2Vector` | `float2(x, y)` |
| `Constant3Vector` | `float3(r, g, b)` |
| `Constant4Vector` | `float4(r, g, b, a)` |
| `Add` | `a + b` |
| `Subtract` | `a - b` |
| `Multiply` | `a * b` |
| `Divide` | `a / b` |
| `OneMinus` | `1.0 - x` |
| `LinearInterpolate` | `lerp(a, b, alpha)` |
| `Clamp` | `clamp(x, min, max)` — only when the clamp mode is the default two-sided clamp |
| `Power` | `pow(base, exponent)` |
| `DotProduct` | `dot(a, b)` |
| `Normalize` | `normalize(v)` |
| `Min` | `min(a, b)` |
| `Max` | `max(a, b)` |
| `Abs` | `abs(x)` |
| `Saturate` | `saturate(x)` |
| `Floor` | `floor(x)` |
| `Ceil` | `ceil(x)` |
| `Frac` | `frac(x)` |
| `SquareRoot` | `sqrt(x)` |
| `Sine` | `sin(x)` — only when `Period` is 1 |
| `Cosine` | `cos(x)` — only when `Period` is 1 |
| `ComponentMask` | a swizzle on the input, built from the R/G/B/A flags in that order |
| `AppendVector` | `floatN(a, b)`, or a single merged swizzle when both operands swizzle the same base value |
| `Time` | `UE.Time()` — only when it neither ignores pause nor overrides the period |
| `Reroute` | nothing — plain reroutes are traced through, with a cycle guard |
| `NamedRerouteDeclaration`, `NamedRerouteUsage` | a named `Graph` temporary, reused by every usage |
| `FunctionInput` | the name declared in the `Inputs` section |
| `MaterialFunctionCall` | a generated [`VirtualFunction`](../language/virtual-function.md) declaration placed above the block, plus a call to it |

Unconnected operand pins fall back to the node's own constant property when it has one — `Min`,
`Max`, `LinearInterpolate`, `Power` and `Clamp` all read their `Const*` values — otherwise to a
literal `0.0`.

### Emitted as a property declaration

These become entries in the `Properties` section and are referenced by name in the `Graph`. Names are
uniquified, and a `ParameterName=` metadata entry is added whenever the DreamShaderLang identifier
had to differ from the asset's parameter name.

| `UMaterialExpression` class | Declaration |
| :-- | :-- |
| `ScalarParameter` | `ScalarParameter <Name> = <default>;` |
| `VectorParameter` | `VectorParameter <Name> = float4(r, g, b, a);` |
| `TextureObjectParameter` | `TextureObjectParameter <Name>[ = <asset path>];` |
| `TextureSampleParameter2D` | `TextureSampleParameter2D <Name>[ = <asset path>];` — **only when no input pin is connected** |

A `TextureSampleParameter2D` with any connected input pin is emitted as a curated `UE.Expression`
instead, carrying its parameter arguments and its sampler arguments together *(since 1.3.7)*. Its
`RGBA` output is emitted once into a named temporary; every other pin becomes a swizzle of it.

### Emitted as a curated `UE.Expression`

These keep a hand-written argument list rather than a reflection dump, so only the properties that
actually differ from the node default appear.

| `UMaterialExpression` class | Arguments emitted |
| :-- | :-- |
| `CurveAtlasRowParameter` | `ParameterName`, `Group`, `SortPriority` and `Desc` when non-default, then `DefaultValue`, `Curve`, `Atlas`, `UseCustomPrimitiveData` with `PrimitiveDataIndex`, and `CurveTime` when the time pin is connected |
| `StaticComponentMaskParameter` | `Input`, `DefaultR`, `DefaultG`, `DefaultB`, `DefaultA`, and `ParameterName` when set. `OutputType` follows the number of enabled channels |
| `StaticSwitchParameter` | `True`, `False`, and `ParameterName`, `DefaultValue`, `DynamicBranch` when non-default |
| `TextureCoordinate` | `CoordinateIndex`, `UTiling`, `VTiling` — each only when non-default |
| `Time` | `bIgnorePause`, `bOverride_Period`, `Period` — used when the node is not at its defaults |
| `Sine` / `Cosine` | `Input`, `Period` — used when `Period` is not 1 |
| `Clamp` | `Input`, `Min`, `Max`, `ClampMode` — used when the clamp mode is min-only or max-only |
| `Panner` | `Coordinate` or `ConstCoordinate`, `Time`, and either `Speed` or the non-zero `SpeedX` / `SpeedY`, plus `bFractionalPart` |
| `Rotator` | `Coordinate` or `ConstCoordinate`, `Time`, and `CenterX`, `CenterY`, `Speed` when non-default |
| `WorldPosition` | `WorldPositionShaderOffset` when it is not the default |
| `CameraVectorWS` | *(none)* |
| `ObjectPositionWS` | *(none)* |
| `ScreenPosition` | *(none)* |
| `VertexColor` | *(none)* — always typed `float4`, with the pin's mask emitted as a swizzle |
| `TextureSample` | the texture, sampler and mip arguments; the `RGBA` output is emitted once and every other pin becomes a swizzle of it |
| `Custom` | `Code`, `Description`, `Output` for a secondary output, the full `AdditionalOutputs` list, and one argument per connected input. `OutputType` is the node's own declared return type, never the selected output's type |

## The `UE.Expression` fallback

Any class without a case above is exported as a generic
[`UE.Expression`](../builtins/ue-expression.md) call, and the decompiler records a warning.

The argument list is built in two passes:

1. Every **connected** input pin, named after the pin, in pin order.
2. Every **reflected literal property** whose value differs from the class default *(since 1.3.7)*.

A property is exported in pass 2 only when all of these hold:

| Requirement | Detail |
| :-- | :-- |
| Not deprecated, transient or duplicate-transient | |
| Not a material-expression input | those are pass 1 |
| Marked editable | `CPF_Edit` |
| Not a control name | `Class`, `OutputType`, `ResultType`, `Output`, `OutputName`, `OutputIndex` |
| Not an editor-only name | `MaterialExpressionEditorX`, `MaterialExpressionEditorY`, `Desc`, `bCommentBubbleVisible`, `bShowOutputNameOnPin`, `bHidePreviewWindow`, `bCollapsed`, `bShaderInputData`, `SortPriority` |
| A supported property type | bool, numeric, enum, byte, name, string, text, or object reference |
| Different from the class default | identical values are omitted |
| Not already an argument | the first writer of a name wins |

Names are compared after normalization, so `bTwoSided` and `Two Sided` collide.

> [!WARNING]
> **Struct, array, map, set and delegate properties are dropped silently.** They are not one of the
> supported property types, so a node whose state lives in a struct exports with that state at its
> class default and no warning names the specific property. Re-set those by hand after the first
> compile, or keep the node as `UE.Expression` and add the missing arguments yourself.

`OutputType` is always emitted, resolved from the real output index. When a named output selector
(`Output=` / `OutputName=`) is present, `OutputIndex` is suppressed, because the generator rejects a
call that carries both. Calls with more than three arguments, or longer than 120 characters, are
emitted across multiple lines.

## Layout export

Controlled by the *Export Decompiled Layout* project setting, default **on**. When on, the file gets
a `Layout` section:

| Emitted line | From |
| :-- | :-- |
| `Comment(Name="<text>", X=<x>, Y=<y>, W=<w>, H=<h>, Color=float4(r, g, b, a));` | every editor comment box |
| `Node(Var="<name>", X=<x>, Y=<y>);` | every named expression, sorted by X, then Y, then name |

Comment boxes whose text begins with `DreamShader: ` are skipped — those are generated markers, not
authored comments.

Independently of the setting, each expression is also assigned to the **smallest** comment box that
encloses it, and those assignments become `#Region` / `#EndRegion` directives around the
corresponding `Graph` statements. Turning layout export off removes the `Layout` block but not the
regions.

See [Layout](../language/layout.md).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}`.

### Warnings written into the file

Each is emitted once, as a `// Warning: …` comment under the `// Decompiled from …` header line. They
do not fail the export.

| Message | Cause |
| :-- | :-- |
| `Exported '{Class}' as UE.Expression; review reflected literal properties if the node has editor-only state.` | a node with no curated case |
| `MaterialFunctionCall '{Path}' is not a plain MaterialFunction; it was exported through UE.Expression.` | the call targets a layer or layer blend rather than a plain material function |
| `A MaterialFunctionCall had no function asset and was exported as a zero literal.` | the call node has no function assigned |
| `Failed to emit VirtualFunction for '{Path}': {Error}` | the called function's declaration could not be built |
| `Named reroute usage '{Node}' has no valid declaration; emitted a default literal.` | a dangling named-reroute usage reached as a node |
| `Named reroute usage '{Node}' has no valid declaration; emitted its default value.` | the same, reached through an input pin |
| `Detected a recursive graph dependency while decompiling node '{Node}'; emitted a default literal to avoid stack overflow.` | a cycle in the expression graph |
| `Detected a recursive reroute dependency while decompiling node '{Node}'; emitted a default literal to avoid stack overflow.` | a cycle through plain reroutes |
| `Detected a recursive named reroute dependency for '{Node}'; emitted a default literal to avoid stack overflow.` | a cycle through named reroutes |
| `Append node '{Node}' resolved to {A} + {B} components, which cannot fit a float4; masked its inputs down to {A2} + {B2}. Review the emitted swizzle.` | an append whose operands exceed four components |

### Export failures

| Message | Cause |
| :-- | :-- |
| `No Material asset was provided.` | a null material reached the decompiler |
| `No MaterialFunction asset was provided.` | a null function reached the decompiler |
| `No asset was provided.` | a null asset reached the service |
| `MaterialFunction '{Name}' does not expose any outputs.` | the function declares no outputs |
| `DreamShader decompile supports Material and MaterialFunction assets only: {Path}` | any other asset class |
| `Decompile did not produce source text.` | the decompile reported failure with no message |
| `DreamShader failed to resolve an output file path.` | the computed output path was empty |
| `DreamShader failed to create output directory '{Directory}'.` | the directory could not be created |
| `DreamShader failed to write decompiled source '{File}'.` | the file could not be written |

### Editor toasts

| Toast | Cause |
| :-- | :-- |
| `DreamShader could not find the selected Material.` / `…Material Function.` | the asset was unloaded between right-click and click |
| `DreamShader failed to export DSM: {Error}` / `DreamShader failed to export DSF: {Error}` | the decompile failed |
| *(the raw write error)* | the file could not be saved |
| `Exported DSM but could not open it: {File}` | written, but the editor could not be launched |
| `Exported DSM: {File}` / `Exported DSF: {File}` | success |

Logs: `Exported Material '{Asset}' to DSM '{File}'.` at Display, and
`Failed to export Material '{Asset}' to DSM: {Error}` at Warning.

## Known round-trip gaps

Verified behaviour of 1.5.0. Each row is something the exported file will not reproduce.

| Gap | Effect | Work-around |
| :-- | :-- | :-- |
| Material-function settings are never emitted | `Description`, `ExposeToLibrary`, `LibraryCategories` and `UserExposedCaption` are lost when exporting a `UMaterialFunction` | add a `Settings` block by hand — see [Function settings](../settings/function.md) |
| Only the blessed `UMaterial` property set is emitted | properties outside it — for example `OpacityMaskClipValue`, `NumCustomizedUVs`, translucency lighting mode, displacement scaling and Nanite override — keep the class default | add the keys to `Settings`; they resolve by reflection — see [Shader settings](../settings/material.md) |
| Struct-, array-, map- and set-valued node properties are not reflected | a fallback `UE.Expression` node loses that state, with no per-property warning | set the property in the material after generation, or extend the emitted call |
| Node comment text (`Desc`) and node `SortPriority` are dropped | comment bubbles and pin ordering are not reproduced | re-apply by hand |
| Node positions depend on a setting | with *Export Decompiled Layout* off, the regenerated graph is auto-laid-out instead | leave the setting on, or write `Layout` by hand |
| Comment boxes prefixed `DreamShader: ` are dropped | generated markers are not re-emitted, by design | none needed |
| A `MaterialFunctionCall` on a layer or layer blend falls back to `UE.Expression` | the call is not expressed as a `VirtualFunction` | export the layer separately and call it |
| A `MaterialFunctionCall` with no assigned function becomes `0.0` | the branch is silently constant-folded | re-assign the function in the original asset and re-export |
| Cycles emit a default literal | the cyclic branch evaluates to a constant | break the cycle in the original graph |
| An append wider than four components is masked down | components are dropped | check the emitted swizzle |
| Material **instances** are not supported | `UMaterialInstanceConstant` is rejected outright | export the parent `UMaterial`, then re-create the instance |
| Texture-sample `GatherMode` round-trips only on UE 5.6 and newer | on older engines the property is omitted | none |
| `bHasPixelAnimation` is in the emitted flag set only on UE 5.4 and newer | on older engines the flag is omitted | none |
| `Base.FrontMaterial` and the `Substrate` shading-model spelling exist only on UE 5.4 and newer | a Substrate material cannot be exported meaningfully below 5.4 | none |
| The generated `Name=` points into `Decompiled/…` | recompiling creates a second asset rather than replacing the original | edit `Name=` / `Root=` once the source is trusted |
| Large graphs skip automatic layout at generation time | a big regenerated graph can come back visually unordered when no `Layout` block is present | keep layout export on — see [Graph layout](../generation/graph-layout.md) |

## Example

Export `/Game/Materials/M_Steel` headlessly, then inspect the result:

```powershell
& "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "I:\Project\Project.uproject" `
    -run=DreamShader decompile -Asset="/Game/Materials/M_Steel" `
    -unattended -nopause -nosplash -stdout -log
```

```text
DreamShader decompiled '/Game/Materials/M_Steel.M_Steel' to
'I:/Project/DShader/Decompiled/Materials/Game/Materials/M_Steel.dsm'.
```

The written file:

```c
// Decompiled from /Game/Materials/M_Steel.M_Steel
Shader(Name="Decompiled/Materials/Game/Materials/M_Steel")
{
    Properties = {
        ScalarParameter Roughness_0 = 0.35 [ParameterName="Roughness"];
        VectorParameter Tint = float4(0.8, 0.8, 0.82, 1.0);
    }
    Settings = {
        Domain = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode = "Opaque";
    }

    Outputs = {
        float3 BaseColor;
        float Metallic;
        float Roughness;

        Base.BaseColor = BaseColor;
        Base.Metallic  = Metallic;
        Base.Roughness = Roughness;
    }

    Graph = {
        BaseColor = Tint.rgb;
        Metallic  = 1.0;
        Roughness = saturate(Roughness_0);
    }

    Layout = {
        Node(Var="Tint", X=-640, Y=-208);
        Node(Var="Roughness_0", X=-640, Y=48);
    }
}
```

## See also

- [Editor integration](editor-integration.md) — the *Export DSM* / *Export DSF* menu entries
- [Commandlet](commandlet.md) — `-run=DreamShader decompile`, `-Asset` and `-Out`
- [UE.Expression](../builtins/ue-expression.md) — the generic call the fallback emits
- [Layout](../language/layout.md) — `Node` and `Comment` directives, and `#Region`
- [Shader settings](../settings/material.md) — the settings the exporter can and cannot emit
- [Function settings](../settings/function.md) — the keys a decompiled `.dsf` will be missing
- [VirtualFunction](../language/virtual-function.md) — the declaration emitted for each called function
- [VirtualFunction tools](virtual-function-tools.md) — the same declaration builder, driven from the asset menu
- [Asset paths](../generation/asset-paths.md) — what the emitted `Name=` resolves to
- [Graph layout](../generation/graph-layout.md) — what happens when no `Layout` block is present
- [Project settings](../settings/project.md) — *Export Decompiled Layout* and *Source Directory*
- [Substrate builtins](../builtins/substrate.md) — the UE 5.4 gate behind `Base.FrontMaterial`
