# DSH7xxx --- Properties, parameters and settings

> The block between the generated markers is written by `.skill/gen-diagnostics.ps1`.
> Everything below a marker is written by hand and survives a regeneration.

## DSH7111

<!-- generated:begin DSH7111 -->
**Severity** error

**Message**

```
Invalid boolean value '%s' for %s.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:120`
<!-- generated:end DSH7111 -->

**Cause.** A setting that is a bool was given something other than `true` / `false` (any case).

**Fix.** Write `true` or `false`.

## DSH7112

<!-- generated:begin DSH7112 -->
**Severity** error

**Message**

```
Setting path segment cannot be empty.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:187`
<!-- generated:end DSH7112 -->

**Cause.** A material setting key that is not one of the well-known ones (`BlendMode`,
`ShadingModel`, `Domain`, ...) is read as a reflection path into `UMaterial`: property names
separated by `.`, with `[index]` on a fixed-size array. This key has an empty segment: a leading or
trailing `.`, or two in a row.

**Fix.** Remove the stray `.`.

## DSH7113

<!-- generated:begin DSH7113 -->
**Severity** error

**Message**

```
Invalid array setting segment '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:200`
<!-- generated:end DSH7113 -->

**Cause.** A material setting key that is not one of the well-known ones (`BlendMode`,
`ShadingModel`, `Domain`, ...) is read as a reflection path into `UMaterial`: property names
separated by `.`, with `[index]` on a fixed-size array. A segment of this key has a `[` without a
property name in front of it, or without its `]`.

**Fix.** Write the segment as `Name[2]`.

## DSH7114

<!-- generated:begin DSH7114 -->
**Severity** error

**Message**

```
Invalid array index '%s' in setting segment '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:207`
<!-- generated:end DSH7114 -->

**Cause.** A material setting key that is not one of the well-known ones (`BlendMode`,
`ShadingModel`, `Domain`, ...) is read as a reflection path into `UMaterial`: property names
separated by `.`, with `[index]` on a fixed-size array. The text between `[` and `]` is not a whole
number of zero or more.

**Fix.** Use a literal index: `Name[2]`.

## DSH7115

<!-- generated:begin DSH7115 -->
**Severity** error

**Message**

```
Invalid array setting segment '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:215`
<!-- generated:end DSH7115 -->

**Cause.** A material setting key that is not one of the well-known ones (`BlendMode`,
`ShadingModel`, `Domain`, ...) is read as a reflection path into `UMaterial`: property names
separated by `.`, with `[index]` on a fixed-size array. Something follows the `]` of a segment other
than the next `.`.

**Fix.** Write one index per segment: `Name[2].Field`.

## DSH7116

<!-- generated:begin DSH7116 -->
**Severity** error

**Message**

```
Invalid material setting target.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:300`
<!-- generated:end DSH7116 -->

**Cause.** The settings were applied to no material. Internal error: the asset was not created
before its settings were written.

**Fix.** Report it with the source; the first error of the same build usually names the real cause.

## DSH7117

<!-- generated:begin DSH7117 -->
**Severity** error

**Message**

```
Invalid material setting path '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:306`
<!-- generated:end DSH7117 -->

**Cause.** A material setting key that is not one of the well-known ones (`BlendMode`,
`ShadingModel`, `Domain`, ...) is read as a reflection path into `UMaterial`: property names
separated by `.`, with `[index]` on a fixed-size array. This key could not be split into segments at
all (empty, or only dots).

**Fix.** Write the property name as the material's Details panel property is called in C++
(`TranslucencyLightingMode`, not its display name).

## DSH7118

<!-- generated:begin DSH7118 -->
**Severity** error

**Message**

```
Unsupported material setting '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:323`
<!-- generated:end DSH7118 -->

**Cause.** A material setting key that is not one of the well-known ones (`BlendMode`,
`ShadingModel`, `Domain`, ...) is read as a reflection path into `UMaterial`: property names
separated by `.`, with `[index]` on a fixed-size array. No property of that name exists on the
material (or on the struct the path has reached). Display names do not count; neither do editor-only
categories.

**Fix.** Look the property's C++ name up in `Material.h`. The keys with a meaning of their own are
listed in `Docs/settings`.

## DSH7119

<!-- generated:begin DSH7119 -->
**Severity** error

**Message**

```
Setting '%s' is not an indexed array property.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:330`
<!-- generated:end DSH7119 -->

**Cause.** A setting key puts `[index]` on a property that is not a fixed-size array.

**Fix.** Remove the index.

## DSH7120

<!-- generated:begin DSH7120 -->
**Severity** error

**Message**

```
Array index %d is out of range for setting '%s' (max %d).
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:335`
<!-- generated:end DSH7120 -->

**Cause.** A setting key indexes a fixed-size array past its end. The message gives the last valid
index.

**Fix.** Use an index inside the array.

## DSH7121

<!-- generated:begin DSH7121 -->
**Severity** error

**Message**

```
Setting '%s' requires an explicit [index].
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:340`
<!-- generated:end DSH7121 -->

**Cause.** A setting key names a fixed-size array without saying which element.

**Fix.** Add `[index]`.

## DSH7122

<!-- generated:begin DSH7122 -->
**Severity** error

**Message**

```
Setting path '%s' cannot continue through '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:356`
<!-- generated:end DSH7122 -->

**Cause.** A setting key continues with `.Field` after a property that is not a struct, so there is
nothing to look `Field` up in.

**Fix.** End the path at that property, or fix the property name in front of the `.`.

## DSH7123

<!-- generated:begin DSH7123 -->
**Severity** error

**Message**

```
Invalid material setting path '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:363`
<!-- generated:end DSH7123 -->

**Cause.** A setting path ran out of segments without reaching a property. Internal backstop of the
path walker.

**Fix.** Report it with the key that caused it.

## DSH7124

<!-- generated:begin DSH7124 -->
**Severity** error

**Message**

```
Failed to create a transient material for Settings validation.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:371`
<!-- generated:end DSH7124 -->

**Cause.** Settings are validated against a scratch material before anything is built, and that
scratch object could not be created. This points at an engine in a bad state (out of memory,
shutting down), not at the source.

**Fix.** Retry; if it persists, restart the editor.

## DSH7125

<!-- generated:begin DSH7125 -->
**Severity** error

**Message**

```
Invalid value '%s' for setting '%s'. %s
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:384`
<!-- generated:end DSH7125 -->

**Cause.** The value of a reflected setting does not fit its property's type; the sentence after it
says what the type expected (a bool, an integer, an enum value, an object path ...). Raised while
the settings are validated, before any asset is touched.

**Fix.** Write the value in the form that sentence asks for. Enum values go by their short name
(`TLM_Surface` or `Surface`).

## DSH7126

<!-- generated:begin DSH7126 -->
**Severity** error

**Message**

```
Invalid value '%s' for setting '%s'. %s
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:402`
<!-- generated:end DSH7126 -->

**Cause.** Same as DSH7125, raised while the setting is applied to the real material -- reached only
when validation passed and the asset then refused the value, which means the property differs
between the scratch material and the asset's class.

**Fix.** Check the value against the property's type; report it if DSH7125 did not fire first.

## DSH7127

<!-- generated:begin DSH7127 -->
**Severity** error

**Message**

```
Unsupported BlendMode/RenderType '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:420`
<!-- generated:end DSH7127 -->

**Cause.** `BlendMode` (or its 1.x alias `RenderType`) is not a value of the engine's `EBlendMode`.
Values are matched without the `BLEND_` prefix, ignoring case, spaces and underscores; hidden enum
entries do not count.

**Fix.** Use `Opaque`, `Masked`, `Translucent`, `Additive`, `Modulate`, `AlphaComposite`,
`AlphaHoldout`, or whatever else this engine's enum has.

## DSH7128

<!-- generated:begin DSH7128 -->
**Severity** error

**Message**

```
ShadingModel="Substrate" requires Unreal Engine 5.4 or newer.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:431`
<!-- generated:end DSH7128 -->

**Cause.** `ShadingModel = Substrate` is set on an engine older than 5.4, which has no Substrate
front material.

**Fix.** Use a classic shading model there, or build with 5.4 or newer.

## DSH7129

<!-- generated:begin DSH7129 -->
**Severity** error

**Message**

```
Unsupported ShadingModel '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:437`
<!-- generated:end DSH7129 -->

**Cause.** `ShadingModel` is not a value of the engine's `EMaterialShadingModel` (matched without
`MSM_`, ignoring case, spaces and underscores).

**Fix.** Use one of the engine's names without the `MSM_` prefix (`DefaultLit`, `Unlit`,
`Subsurface`, `ClearCoat`, `TwoSidedFoliage`, `Hair`, `Cloth`, `Eye`, `SingleLayerWater`,
`ThinTranslucent`, `Substrate`, ...); an engine fork may add its own.

## DSH7130

<!-- generated:begin DSH7130 -->
**Severity** error

**Message**

```
Unsupported MaterialDomain '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp:446`
<!-- generated:end DSH7130 -->

**Cause.** `Domain` is not a value of the engine's `EMaterialDomain` (matched without `MD_`,
ignoring case).

**Fix.** Use `Surface`, `DeferredDecal`, `LightFunction`, `Volume`, `PostProcess` or `UI`.

## DSH7131

<!-- generated:begin DSH7131 -->
**Severity** error

**Message**

```
Invalid reflected property target.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:75`
<!-- generated:end DSH7131 -->

**Cause.** A literal was to be written into a property that does not exist on the object. Internal
error: the catalog listed a property the live class does not have.

**Fix.** Report it; re-exporting the catalog (`dsc export-catalog`) rules out a stale manifest.

## DSH7132

<!-- generated:begin DSH7132 -->
**Severity** error

**Message**

```
'%s' is not a valid boolean value for '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:103`
<!-- generated:end DSH7132 -->

**Cause.** A bool property of a node (or a bool setting) was given something other than `true` /
`false`.

**Fix.** Write `true` or `false`.

## DSH7133

<!-- generated:begin DSH7133 -->
**Severity** error

**Message**

```
'%s' is not a valid integer value for '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:114`
<!-- generated:end DSH7133 -->

**Cause.** An integer property was given text that is not a whole number.

**Fix.** Write an integer, without a decimal point.

## DSH7134

<!-- generated:begin DSH7134 -->
**Severity** error

**Message**

```
'%s' is not a valid unsigned integer value for '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:125`
<!-- generated:end DSH7134 -->

**Cause.** An unsigned integer property was given a negative number or text that is no number.

**Fix.** Write a whole number of zero or more.

## DSH7135

<!-- generated:begin DSH7135 -->
**Severity** error

**Message**

```
'%s' is not a valid numeric value for '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:136`
<!-- generated:end DSH7135 -->

**Cause.** A float property was given text that is not a number.

**Fix.** Write a number. A vector property takes `(X=..,Y=..,Z=..)` in the engine's own text form,
not an HLSL constructor.

## DSH7136

<!-- generated:begin DSH7136 -->
**Severity** error

**Message**

```
'%s' is not a valid numeric value for '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:147`
<!-- generated:end DSH7136 -->

**Cause.** A double property was given text that is not a number.

**Fix.** Write a number.

## DSH7137

<!-- generated:begin DSH7137 -->
**Severity** error

**Message**

```
Object property '%s' expects Path(...) or an absolute Unreal object path.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:169`
<!-- generated:end DSH7137 -->

**Cause.** An object property (a texture, a function, a collection) was given text that is neither
`Path(Root, "...")` nor an absolute object path such as `/Game/Textures/T_Noise`.

**Fix.** Write the asset as `Path(Game, "Textures/T_Noise")` or as its object path.

## DSH7138

<!-- generated:begin DSH7138 -->
**Severity** error

**Message**

```
Failed to load asset '%s' for '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:184`
<!-- generated:end DSH7138 -->

**Cause.** The asset an object property names could not be loaded: the path is wrong, the asset was
moved, or its plugin is not mounted in this project.

**Fix.** Copy the reference from the Content Browser (*Copy Reference*) and paste it in; check that
the plugin owning the asset is enabled.

## DSH7139

<!-- generated:begin DSH7139 -->
**Severity** error

**Message**

```
Asset '%s' is not compatible with '%s'. Expected '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:189`
<!-- generated:end DSH7139 -->

**Cause.** The asset loaded, and it is not of the class the property takes: a Texture2D where a
TextureCube is expected, a material where a function is.

**Fix.** Point the property at an asset of the class the message names.

## DSH7140

<!-- generated:begin DSH7140 -->
**Severity** error

**Message**

```
'%s' is not a valid enum value for '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:203`
<!-- generated:end DSH7140 -->

**Cause.** An enum property (an `FEnumProperty`) was given a name that is not one of the enum's
values. Values are matched with and without the enum's prefix, ignoring case.

**Fix.** Use one of the values the builtin catalog lists for that property (`dsc export-catalog`).

## DSH7141

<!-- generated:begin DSH7141 -->
**Severity** error

**Message**

```
'%s' is not a valid enum value for '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:218`
<!-- generated:end DSH7141 -->

**Cause.** Same as DSH7140, for the older `TEnumAsByte` kind of enum property.

**Fix.** Use one of the enum's values.

## DSH7142

<!-- generated:begin DSH7142 -->
**Severity** error

**Message**

```
'%s' is not a valid byte value for '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:228`
<!-- generated:end DSH7142 -->

**Cause.** A byte property that is not an enum was given text that is not a number from 0 to 255.

**Fix.** Write a number from 0 to 255.

## DSH7143

<!-- generated:begin DSH7143 -->
**Severity** error

**Message**

```
Property '%s' on '%s' is not a supported literal type yet.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:242`
<!-- generated:end DSH7143 -->

**Cause.** The property exists and its type is not one a literal can be written into: a struct
without a text form the writer knows, an array, a map, a delegate.

**Fix.** Leave the property out of the call and set it on the asset by hand, or ask for the type to
be supported; the message names the property and its class.

## DSH7144

<!-- generated:begin DSH7144 -->
**Severity** error

**Message**

```
Invalid reflected property target.
```

**Raised by** `Source/DreamShaderCompiler/Private/Reflection/DreamShaderMaterialLiteralPropertyWriter.cpp:249`
<!-- generated:end DSH7144 -->

**Cause.** A literal was to be written into a null object or property. Internal error.

**Fix.** Report it with the source.

## DSH7200

<!-- generated:begin DSH7200 -->
**Severity** error

**Message**

```
'{0}' is set twice by '#pragma material'; it was already set on line {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:516`
<!-- generated:end DSH7200 -->

**Cause.** One `#pragma material` key is set twice, possibly on two different lines — the lines are
merged into one settings table. Keys are compared without regard to case, as the 1.x settings were.

**Fix.** Delete one of the two. The message names the line the first one was written on.

## DSH7201

<!-- generated:begin DSH7201 -->
**Severity** error

**Message**

```
'Backend' has no value; write 'Backend = Graph' or 'Backend = ThinCustom'. An empty value meant Graph in 1.x and means nothing now.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:564`
<!-- generated:end DSH7201 -->

**Cause.** `Backend` was given an empty value. 1.x read `Backend = ""` as `Graph`, which silently
overrode the project default with a value nobody wrote; 2.0 refuses it rather than carry the trap
forward.

**Fix.** Write `Backend = Graph` or `Backend = ThinCustom`, or delete the key to take the project
default.

## DSH7202

<!-- generated:begin DSH7202 -->
**Severity** error

**Message**

```
'Backend = {0}' is not a backend; the backends are 'Graph' and 'ThinCustom'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:593`
<!-- generated:end DSH7202 -->

**Cause.** `Backend` names something that is not a backend.

**Fix.** The backends are `Graph` (a visible material graph) and `ThinCustom` (a hidden base
material plus a lightweight instance). `Instance` is the old spelling of `ThinCustom` and still
works, with a warning.

## DSH7203

<!-- generated:begin DSH7203 -->
**Severity** warning

**Message**

```
'#pragma material' configures a material, and this file has no 'export void Name(inout material m)' entry to configure.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1328`
<!-- generated:end DSH7203 -->

**Cause.** The file has `#pragma material(...)` and no material entry to configure. A function
library does not become a `UMaterial`, so the settings have nothing to apply to.

**Fix.** Delete the pragma, or add the `export void Name(inout material m)` the file was meant to
have.

## DSH7204

<!-- generated:begin DSH7204 -->
**Severity** warning

**Message**

```
'Backend = Instance' is the old spelling of 'Backend = ThinCustom'; write the new one.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:585`
<!-- generated:end DSH7204 -->

**Cause.** `Backend = Instance` — the 1.x deprecation-window spelling of `ThinCustom`.

**Fix.** Write `Backend = ThinCustom`. Nothing changes about what is produced.

## DSH7205

<!-- generated:begin DSH7205 -->
**Severity** error

**Message**

```
'#pragma material' takes 'Key = Value' pairs; '{0}' has no key.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:500`
<!-- generated:end DSH7205 -->

**Cause.** `#pragma material` was given something without a key.

**Fix.** Write `Key = Value`. The keys are the `UMaterial` property names; unknown ones are passed
through to the material by reflection, so a misspelt key is reported by the emitter and not here.

## DSH7210

<!-- generated:begin DSH7210 -->
**Severity** error

**Message**

```
'{0}' is a compile-time constant, and this initializer is not one; a constant is built from literals and other constants.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:290`, `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:607`
<!-- generated:end DSH7210 -->

**Cause.** A `static const` (or `const`) has an initializer the binder cannot fold to a value. A
constant becomes a Constant node in the graph, so its value has to exist before anything runs.

**Fix.** Build it from literals and other constants. A value that depends on a `uniform`, a texture
sample or a node is not a constant — declare it as an ordinary local.

## DSH7211

<!-- generated:begin DSH7211 -->
**Severity** error

**Message**

```
'{0}' is a file-scope variable with no storage class; write 'uniform' for a material parameter or 'static const' for a compile-time constant.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:749`
<!-- generated:end DSH7211 -->

**Cause.** A file-scope variable has no storage class: `float Gain = 1.0;` or `static float …`.
Neither has a node. A file-scope value is either an input to the material (`uniform`) or a constant
folded into it (`static const`).

**Fix.** Write `uniform` or `static const`. `const` alone is accepted and means the same as
`static const`.

## DSH7212

<!-- generated:begin DSH7212 -->
**Severity** error

**Message**

```
A file-scope variable of type {0} has no node; a 'uniform' or 'static const' must be numeric, bool, a texture or a sampler.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:776`
<!-- generated:end DSH7212 -->

**Cause.** A file-scope variable has a type with no node: a `material`, a `Substrate` value, a user
`struct`, `void`.

**Fix.** A `material` is produced by the entry, not declared; a `struct` is a compile-time
aggregate and lives inside a function; a `Substrate` value comes from a `Substrate.` node.

## DSH7213

<!-- generated:begin DSH7213 -->
**Severity** error

**Message**

```
A texture uniform has no HLSL initializer; write its default asset as '/// @default /Game/...'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:788`
<!-- generated:end DSH7213 -->

**Cause.** A texture `uniform` has an HLSL initializer. A texture has no literal value.

**Fix.** Write its default asset as `/// @default /Game/Textures/T_Name` above the declaration, and
leave the declaration itself as plain legal HLSL.

## DSH7214

<!-- generated:begin DSH7214 -->
**Severity** error

**Message**

```
'{0}' is a compile-time constant and must be initialised where it is declared.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:799`, `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:569`
<!-- generated:end DSH7214 -->

**Cause.** A `static const` — at file scope or inside a function — has no initializer.

**Fix.** Initialise it where it is declared, or drop `const` to make it an ordinary variable.

## DSH7215

<!-- generated:begin DSH7215 -->
**Severity** error

**Message**

```
'@layer' and '@layerblend' make two different assets; a function is one or the other.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:468`
<!-- generated:end DSH7215 -->

**Cause.** One function carries both `@layer` and `@layerblend`. They select two different asset
classes.

**Fix.** Keep the one that matches the signature: `@layer` for `void (inout material)`,
`@layerblend` for `void (material …, inout material)`.

## DSH7216

<!-- generated:begin DSH7216 -->
**Severity** error

**Message**

```
A 'uniform' array has no parameter node; declare one uniform per element, or make it 'static const'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:821`
<!-- generated:end DSH7216 -->

**Cause.** A `uniform` is declared as an array. There is no array parameter node.

**Fix.** Declare one uniform per element, or make it `static const` if the values are fixed, or use
a texture for a table that has to be read at run time.

## DSH7220

<!-- generated:begin DSH7220 -->
**Severity** error

**Message**

```
'@slider' takes two numbers, a minimum and a maximum; '{0}' is not that.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:189`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:199`
<!-- generated:end DSH7220 -->

**Cause.** `@slider` is not two numbers, or its minimum is not below its maximum.

**Fix.** Write `/// @slider 0 4`.

## DSH7221

<!-- generated:begin DSH7221 -->
**Severity** error

**Message**

```
'@sort' takes one whole number; '{0}' is not that.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:222`
<!-- generated:end DSH7221 -->

**Cause.** `@sort` is not one whole number.

**Fix.** Write `/// @sort 3`. Without it, parameters sort in declaration order.

## DSH7222

<!-- generated:begin DSH7222 -->
**Severity** error

**Message**

```
'@sampler' needs a sampler type after it, such as 'Color', 'Normal' or 'LinearColor'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:258`
<!-- generated:end DSH7222 -->

**Cause.** `@sampler` has no value.

**Fix.** Name a sampler type: `Color`, `Normal`, `LinearColor`, `Grayscale`, `Masks`, … The list
comes from the engine, so an unknown spelling is reported by the emitter and not here.

## DSH7223

<!-- generated:begin DSH7223 -->
**Severity** error

**Message**

```
'@static' asks for a static switch and is only meaningful on a 'uniform bool'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:810`
<!-- generated:end DSH7223 -->

**Cause.** `@static` sits on something that is not a `uniform bool`. It asks for a static switch,
which only a boolean parameter can become.

**Fix.** Delete the directive, or change the declaration to `uniform bool`.

## DSH7224

<!-- generated:begin DSH7224 -->
**Severity** warning

**Message**

```
'@{0}' means nothing here; it belongs on {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:909`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:138`
<!-- generated:end DSH7224 -->

**Cause.** A `///` directive is written on a declaration where it cannot mean anything: `@slider` on
a function, `@library` on a uniform, `@sampler` on something that is not a texture, `@asset` on a function that is not an `extern`
prototype, `@library` or `@name` on a function that is not `export`. It is dropped.

**Fix.** Move it to the declaration it belongs to, or delete it. Unknown keys are **not** this
warning — they are passed through to the node by reflection.

## DSH7225

<!-- generated:begin DSH7225 -->
**Severity** warning

**Message**

```
'@static {0}' does not name a parameter of '{1}'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1015`, `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1037`, `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1063`
<!-- generated:end DSH7225 -->

**Cause.** `@param <name>` names something that is not a parameter of the function. Usually the
parameter was renamed and the comment was not.

**Fix.** Correct the name, or delete the line.

## DSH7226

<!-- generated:begin DSH7226 -->
**Severity** warning

**Message**

```
'@custom {0}' is not a modifier this language knows; the only one is 'selfcontained'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:432`
<!-- generated:end DSH7226 -->

**Cause.** `@custom` is followed by a word that is not a modifier it knows.

**Fix.** The only modifier is `selfcontained`. `Inline`, the 1.x spelling, is gone.

## DSH7227

<!-- generated:begin DSH7227 -->
**Severity** error

**Message**

```
'@{0}' needs a value after it.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:152`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:290`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:329`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:356`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:360`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:470`
<!-- generated:end DSH7227 -->

**Cause.** A directive that needs a value has none: `@name`, `@asset`, `@library`, `@default`, or
`@param` without a parameter name.

**Fix.** Write the value after the key. A directive's value runs to the next ` @` or to the end of
the line.

## DSH7228

<!-- generated:begin DSH7228 -->
**Severity** warning

**Message**

```
'@custom' on '{0}' did not make its body opaque; the directive has to sit in the '///' block directly above the declaration.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1281`
<!-- generated:end DSH7228 -->

**Cause.** `@custom` was recorded on a function whose body was parsed as DreamShaderLang rather
than captured verbatim. The parser only makes a body opaque when it sees `@custom` in the `///`
block **directly above** the declaration.

**Fix.** Move the `@custom` line so nothing separates it from the declaration.

## DSH7229

<!-- generated:begin DSH7229 -->
**Severity** warning

**Message**

```
'@{0}' is written twice in this block; the last one wins.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:122`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:376`
<!-- generated:end DSH7229 -->

**Cause.** One key appears twice in one `///` block. The last one wins.

**Fix.** Delete the earlier one. `@param` is the one key that may legitimately repeat and never
reports this.

## DSH7230

<!-- generated:begin DSH7230 -->
**Severity** warning

**Message**

```
'#pragma layout' expects a whole number for '{0}'; '{1}' was ignored.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:667`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:687`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:749`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:760`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:772`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:783`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:800`
<!-- generated:end DSH7230 -->

**Cause.** A `#pragma layout(...)` line could not be read: no `Node`/`Comment` selector, an unknown
key, or a coordinate that is not a whole number. The line is ignored and nothing else is affected.

**Fix.** Layout is written by the decompiler and is rarely edited by hand; the shape is
`#pragma layout(Node, Var = UV, X = -1100, Y = -120)` and
`#pragma layout(Comment, Text = "Sampling", X = …, Y = …, Width = …, Height = …)`. A node with no
layout entry is simply placed by the layout pass.

## DSH7231

<!-- generated:begin DSH7231 -->
**Severity** error

**Message**

```
'@static {0}' makes a parameter a static bool pin, which only a 'bool' input can be, and '{0}' is {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:990`
<!-- generated:end DSH7231 -->

**Cause.** `/// @static <Parameter>` on a function makes that parameter a StaticBool pin -- a pin
whose value picks a shader permutation -- and the parameter named is not a `bool`.

**Fix.** Name a `bool` parameter, or remove the directive.

## DSH7232

<!-- generated:begin DSH7232 -->
**Severity** error

**Message**

```
'Substrate = {0}' is not a Substrate mode; the modes are 'Legacy', 'Bridge' and 'Native'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:642`
<!-- generated:end DSH7232 -->

**Cause.** `#pragma material(Substrate = ...)` (or the `Substrate` setting of a 1.x `Shader`) names
something other than the three modes.

**Fix.** Write `Legacy` (the default: built as written, the engine converts), `Bridge` (the legacy
attributes become one `Substrate.ShadingModels` node in a Substrate project) or `Native` (the source
is Substrate source).

## DSH7250

<!-- generated:begin DSH7250 -->
**Severity** error

**Message**

```
A '.dsi' needs one '#pragma instance(Parent = "...")' naming the material it is an instance of, and this file has none.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:310`
<!-- generated:end DSH7250 -->

**Cause.** A `.dsi` file has no `#pragma instance(...)`. The pragma is what makes the file an
instance and says of what.

**Fix.** Add `#pragma instance(Parent = "M_Parent")` at the top: a bare name for a source beside
this file, or an object path for any material.

## DSH7251

<!-- generated:begin DSH7251 -->
**Severity** error

**Message**

```
'#pragma instance' is written a second time, and one '.dsi' is one material instance; the line {0} already declares it.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:341`
<!-- generated:end DSH7251 -->

**Cause.** A `.dsi` has two `#pragma instance` lines. One file is one instance.

**Fix.** Merge the keys into one pragma, or split the file.

## DSH7252

<!-- generated:begin DSH7252 -->
**Severity** error

**Message**

```
'Parent' in '#pragma instance' names the material this is an instance of, and it is empty.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:425`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:440`
<!-- generated:end DSH7252 -->

**Cause.** `#pragma instance` has no `Parent`, or `Parent` is there and empty. An instance is
nothing but overrides of one parent.

**Fix.** Name the parent: a DreamShader material by its name, or any material or instance by object
path -- `#pragma instance(Parent = "M_Skin")`.

## DSH7253

<!-- generated:begin DSH7253 -->
**Severity** error

**Message**

```
'{0}' is set twice by '#pragma instance'; it was already set on line {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:406`
<!-- generated:end DSH7253 -->

**Cause.** One key is written twice in `#pragma instance(...)`.

**Fix.** Keep one.

## DSH7254

<!-- generated:begin DSH7254 -->
**Severity** error

**Message**

```
A '.dsi' is configured by '#pragma instance', and '#pragma material' configures a material; write these keys in '#pragma instance(...)'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:213`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:253`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:267`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:280`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:293`
<!-- generated:end DSH7254 -->

**Cause.** A `.dsi` holds something an instance has no use for: `#pragma material(...)` (that pragma
configures a material being built), a declaration that is not a `uniform`, a function, a `struct`,
or an `#include`. An instance has no graph; it only assigns parameters of its parent.

**Fix.** Move pragma keys into `#pragma instance(...)` -- only properties an instance can override
are accepted there (`BlendMode`, `TwoSided`, `ShadingModel`, `OpacityMaskClipValue`, ...; see
`Docs/language-v2/instances.md`). Move functions and types to a `.dss` or a `.dsh`, and remove
includes.

## DSH7255

<!-- generated:begin DSH7255 -->
**Severity** error

**Message**

```
'#pragma instance' declares a material instance and belongs in a '.dsi' file of its own, and this line is in '{0}'; move it and its overrides into a '.dsi'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:324`
<!-- generated:end DSH7255 -->

**Cause.** `#pragma instance` stands in a `.dss` (or a header). A material and its instances are
separate assets and separate files.

**Fix.** Move the pragma and the overrides below it into a `.dsi` of their own.

## DSH7256

<!-- generated:begin DSH7256 -->
**Severity** error

**Message**

```
'{0}' overrides a {1} parameter and needs the value it is set to, as 'uniform {2} {0} = ...;', and it has no initializer.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:665`
<!-- generated:end DSH7256 -->

**Cause.** An override of a scalar, vector or switch parameter has no initializer, so there is no
value to set.

**Fix.** Write the value: `uniform float Gain = 2.0;`.

## DSH7257

<!-- generated:begin DSH7257 -->
**Severity** error

**Message**

```
'{0}' overrides a {1} parameter, which takes an asset rather than an HLSL value; write '/// @default /Game/...' (or '/// @default None') above it instead of an initializer.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:640`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:651`
<!-- generated:end DSH7257 -->

**Cause.** An override of a texture (or font, runtime virtual texture, collection ...) parameter is
given an initializer, or has no `/// @default`. An asset is not an HLSL value; it is named by the
directive, exactly as in the parent.

**Fix.** Write `/// @default /Game/Textures/T_Override` above `uniform Texture2D Name;`, or `///
@default None` to clear it.

## DSH7258

<!-- generated:begin DSH7258 -->
**Severity** error

**Message**

```
'{0}' is not a parameter of the parent '{1}'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:851`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:865`
<!-- generated:end DSH7258 -->

**Cause.** The `.dsi` overrides a name that is not a parameter of its parent. Either the name is
wrong, or the parent declares that uniform and never reads it, in which case the material has no
such parameter.

**Fix.** Use the parameter's name as the parent's Details panel shows it (`/// @name` on the
override when it is not an identifier). `dsc index` lists a parent's parameters.

## DSH7259

<!-- generated:begin DSH7259 -->
**Severity** error

**Message**

```
'{0}' is a {1} parameter of type '{2}' in the parent, and this override declares '{3}'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:924`
<!-- generated:end DSH7259 -->

**Cause.** The override declares another type than the parent's parameter has: `float3` for a scalar
parameter, `bool` without `@static` for a static switch, `Texture2D` for a cube texture.

**Fix.** Declare the override exactly as the parent declares the uniform.

## DSH7260

<!-- generated:begin DSH7260 -->
**Severity** error

**Message**

```
'/// @static' on an instance override means a static switch ('uniform bool') or a static component mask ('uniform bool4'), and '{0}' is declared '{1}'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:580`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:611`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:898`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:909`
<!-- generated:end DSH7260 -->

**Cause.** `/// @static` and the parent disagree. The directive stands on an override that is
neither `uniform bool` (a static switch) nor `uniform bool4` (a static component mask); or the
parent's parameter is static and the override lacks `/// @static`; or the parent sets the parameter
at run time and the override asks for a static one.

**Fix.** Declare the override the way the parent declares the parameter: add or remove `///
@static`, or fix the type.

## DSH7261

<!-- generated:begin DSH7261 -->
**Severity** error

**Message**

```
'{0}' is overridden a second time, and one instance sets a parameter once; the override on line {1} already sets it.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:881`
<!-- generated:end DSH7261 -->

**Cause.** One parameter is overridden twice in a `.dsi`.

**Fix.** Keep one of the two lines.

## DSH7262

<!-- generated:begin DSH7262 -->
**Severity** warning

**Message**

```
'@{0}' has no effect above '#pragma instance', where only '@name' is read; remove it.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:374`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:514`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:677`
<!-- generated:end DSH7262 -->

**Cause.** A directive that describes a parameter (`@group`, `@sort`, `@slider`, `@desc`, `@default`
on a value) stands on an instance override or above the pragma. That metadata belongs to the parent;
an instance only sets values.

**Fix.** Remove it. `@name`, `@static`, `@default` (on an asset override) and `@page` are the
directives a `.dsi` reads.

## DSH7263

<!-- generated:begin DSH7263 -->
**Severity** info

**Message**

```
The parameters of the parent are not available here, so the names and types of these overrides are checked only for their shape; compile the parent first, or check the file in the editor.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:786`
<!-- generated:end DSH7263 -->

**Cause.** The parent's parameters could not be looked up where this file was bound -- the parent is
an asset path and no editor is attached (a language-service check), or the parent source has not
been compiled yet -- so override names and types are checked for their shape only.

**Fix.** Nothing to fix. Compile the parent first, or check the file with the editor running, to
have every name verified.

## DSH7264

<!-- generated:begin DSH7264 -->
**Severity** error

**Message**

```
'{0}' matches the parent parameter '{1}' only in case, and DreamShader names are case-sensitive; write '{1}'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:840`
<!-- generated:end DSH7264 -->

**Cause.** An override matches a parent parameter only when case is ignored. Parameter names are
case-sensitive in the engine, so the override would silently set nothing.

**Fix.** Spell the name exactly as the parent does.

## DSH7265

<!-- generated:begin DSH7265 -->
**Severity** error

**Message**

```
'{0}' is set to a value the compiler can fold, a literal or an expression over literals, and this initializer is not one.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:773`
<!-- generated:end DSH7265 -->

**Cause.** An override's value is not something the compiler can evaluate: it reads a variable,
calls a node, or uses a run-time function. An instance stores constants.

**Fix.** Use a literal or arithmetic over literals: `uniform float3 Tint = float3(1, 0.5, 0) *
0.5;`.

## DSH7266

<!-- generated:begin DSH7266 -->
**Severity** error

**Message**

```
'@page' takes one whole number of zero or more, and '{0}' is not that.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:501`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:690`
<!-- generated:end DSH7266 -->

**Cause.** `/// @page` is not a whole number of zero or more, or it stands on an override that is
not a Font.

**Fix.** Write the font page index above a Font override -- `/// @page 2` -- and nowhere else.

## DSH7267

<!-- generated:begin DSH7267 -->
**Severity** warning

**Message**

```
'#pragma {0}' boxes or places graph nodes, and a '.dsi' has no graph; the line was ignored.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:222`
<!-- generated:end DSH7267 -->

**Cause.** `#pragma region` or `#pragma layout` stands in a `.dsi`. Both arrange graph nodes, and an
instance has no graph.

**Fix.** Remove the line.

## DSH7268

<!-- generated:begin DSH7268 -->
**Severity** error

**Message**

```
'{0}' exists in the parent only as a layer or blend parameter, and a '.dsi' overrides global parameters only.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:823`
<!-- generated:end DSH7268 -->

**Cause.** The name exists in the parent only as a parameter of a material layer or blend. A `.dsi`
sets global parameters; layer parameters are associated with a layer index the file has no syntax
for.

**Fix.** Override it on the instance asset by hand, or expose the value as a global parameter in the
parent.

## DSH7269

<!-- generated:begin DSH7269 -->
**Severity** error

**Message**

```
'{0}' is an array, and an instance override assigns one parameter; override each element's parameter on its own.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:567`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:623`
<!-- generated:end DSH7269 -->

**Cause.** An override is declared as an array, or with a type that is no material parameter type.
Each parameter of a material is one name and one value: a number, a bool, a texture, or one of
`RuntimeVirtualTexture`, `SparseVolumeTexture`, `TextureCollection`, `ParameterCollection` and
`Font`.

**Fix.** Override each element's parameter on its own line, with the type the parent declares it
with.

## DSH7270

<!-- generated:begin DSH7270 -->
**Severity** error

**Message**

```
'#pragma instance' takes 'Key = Value' pairs, and '{0}' has no key.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:390`
<!-- generated:end DSH7270 -->

**Cause.** `#pragma instance(...)` contains a bare value. Every entry is `Key = Value`.

**Fix.** Write, for example, `#pragma instance(Parent = "M_Base", TwoSided = true)`.

## DSH7300

<!-- generated:begin DSH7300 -->
**Severity** error

**Message**

```
'{0}' is not a key of '#pragma pipeline'; the keys are {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1021`
<!-- generated:end DSH7300 -->

**Cause.** `#pragma pipeline(...)` has a key that is not one of its five: `Order`, `Injection`,
`Views`, `Requires` and `Enabled`. Keys are case-sensitive, and a spelling that differs only in case
is suggested. A buffer's or a pass's key (`Scale`, `Material`) is not a pipeline key either.

**Fix.** Correct the key, or move it where it belongs: buffer keys go in
`buffer Name : Format(Key = Value)`, pass keys inside the `pass` block.

## DSH7301

<!-- generated:begin DSH7301 -->
**Severity** error

**Message**

```
'{0}' is set twice by '#pragma pipeline'; it was already set on line {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1005`
<!-- generated:end DSH7301 -->

**Cause.** One key is written twice in `#pragma pipeline(...)`. Keys are compared case-sensitively,
so `order` beside `Order` is DSH7300, not this.

**Fix.** Keep one of the two.

## DSH7302

<!-- generated:begin DSH7302 -->
**Severity** error

**Message**

```
(built at runtime)
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1216`
<!-- generated:end DSH7302 -->

**Cause.** A key of `#pragma pipeline` has a value it cannot take; the message says which:

- `Order` is not a whole number, or is quoted. It orders the pipelines that have passes at one
  injection point, smaller first.
- `Views` or `Requires` is quoted (`Views = "Game"`): both take names written without quotes, and a
  quoted value is refused rather than dropped, which would leave the pipeline on its default views or
  requirements. `Views = ""` has a message of its own: it names no view, so the pipeline would run
  nowhere.
- `Views` or `Requires` names a word that is not one of its own. `Views` takes `Game`, `Editor`,
  `SceneCapture`, `PlanarReflection`, `ReflectionCapture` and `Thumbnail`; `Requires` takes
  `PostProcess`, `SceneResolve` and `CustomStencil`. A case-only match is suggested.
- `Enabled` is not a name: it takes a `uniform bool` of the file, or `true`.

**Fix.** Write the values without quotes, the words of `Views` and `Requires` joined by `|`:

```hlsl
#pragma pipeline(Order = 100, Views = Game | Editor, Requires = PostProcess, Enabled = bShowOutline)
```

A pipeline that should not run is switched off through `Enabled` and a `uniform bool`, not by an
empty `Views`.

## DSH7303

<!-- generated:begin DSH7303 -->
**Severity** error

**Message**

```
'{0}' is not an injection point; the points are {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1172`
<!-- generated:end DSH7303 -->

**Cause.** `Injection` -- of `#pragma pipeline` or of a pass -- names no injection point. The names
are case-sensitive, and the points on the post-process chain carry its prefix: `AfterDOF` is
suggested as `PostProcess.AfterDOF`.

**Fix.** Use one of the points the message lists, spelled as listed and, in a pass, without quotes:
`Injection = PostProcess.AfterDOF;`. What each point sees is in `Docs/language-v2/passes.md`.

## DSH7304

<!-- generated:begin DSH7304 -->
**Severity** error

**Message**

```
'{0}' is not a pass kind; a pass is {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1708`
<!-- generated:end DSH7304 -->

**Cause.** A `pass` header names a kind that does not exist. The kinds are `fullscreen`, `compute`,
`mesh`, `clear` and `copy`, written in lower case; `Fullscreen` is suggested as `fullscreen`. Without a
kind, the pass's keys and the rules that depend on the kind are not checked.

**Fix.** Write one of the five: `pass Blur : compute { ... }`.

## DSH7305

<!-- generated:begin DSH7305 -->
**Severity** error

**Message**

```
'{0}' is not a buffer format; the formats are {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1456`
<!-- generated:end DSH7305 -->

**Cause.** A `buffer` declaration names a format that does not exist. The formats are `R8`, `RG8`,
`RGBA8`, `R16F`, `RG16F`, `RGBA16F`, `R32F`, `RG32F`, `RGBA32F`, `R32U`, `RG32U` and `Depth32`,
case-sensitive; engine spellings such as `PF_R8` or `R8_UNORM` are not among them.

**Fix.** Write one of them: `buffer Mask : R8;`. `R32U` and `RG32U` are reserved (DSH7321), and
`Depth32` is only a mesh pass's own depth (DSH7330).

## DSH7306

<!-- generated:begin DSH7306 -->
**Severity** error

**Message**

```
'{0}' is not a key of a buffer; the keys are {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1512`
<!-- generated:end DSH7306 -->

**Cause.** A buffer's argument list has a key that is not a buffer key. The keys are `Scale`, `Size`,
`Resolution`, `Clear`, `Mips`, `History` and `Export`, case-sensitive. The format is not a key: it is
written after the colon.

**Fix.** Correct the key: `buffer Blurred : R8(Scale = 0.5, Export = true);`.

## DSH7307

<!-- generated:begin DSH7307 -->
**Severity** error

**Message**

```
'{0}' is set twice for buffer '{1}'; it was already set on line {2}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1495`
<!-- generated:end DSH7307 -->

**Cause.** One key is written twice in a buffer's argument list.

**Fix.** Keep one of the two.

## DSH7308

<!-- generated:begin DSH7308 -->
**Severity** error

**Message**

```
(built at runtime)
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1219`
<!-- generated:end DSH7308 -->

**Cause.** A buffer key has a value it cannot take; the message says which. Numbers are folded when
the pipeline compiles, so a `static const` works and a `uniform` does not.

- `Scale` is not a number, or is outside 0.0625 to 4.
- `Size` is not two whole numbers, or a side is outside 1 to 16384.
- `Resolution` is not `Render` or `Output`. `Resolution = Fixed` is refused too: a fixed size is
  written `Size = int2(w, h)`.
- `Clear` is not a number, a `float2` to `float4`, or `None`.
- `Mips` is not a whole number from 1 to 14.
- `History` or `Export` is not `true` or `false`.

**Fix.** Write the value the message asks for:

```hlsl
buffer Wind : RG16F(Size = int2(256, 256), Clear = 0, History = true, Export = true);
```

## DSH7309

<!-- generated:begin DSH7309 -->
**Severity** error

**Message**

```
'Size' gives buffer '{0}' a fixed size, so '{1}' has nothing to say; remove one of them.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1633`
<!-- generated:end DSH7309 -->

**Cause.** A buffer has `Size` and `Scale` (or `Resolution`). `Size` makes it a fixed number of texels;
`Scale` and `Resolution` size it after the view, so one of the two has nothing to say.

**Fix.** Remove `Size` from a buffer that follows the view, or `Scale` and `Resolution` from a fixed
one.

## DSH7310

<!-- generated:begin DSH7310 -->
**Severity** error

**Message**

```
'{0}' is not a key of a {1} pass; its keys are {2}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1825`
<!-- generated:end DSH7310 -->

**Cause.** A pass has a key that no kind of pass has. Every pass takes `Injection` and `Enabled`;
beyond those, `fullscreen` takes `Material`, `Shader`, `Entry`; `compute` takes `Shader`, `Entry`,
`Threads`, `Dispatch`; `mesh` takes `Filter`, `Material`, `Mode`, `Depth`, `Cull`, `Blend`, `Usage`,
`Nanite`, `NaniteValue`; `clear` takes `Value`; `copy` takes none. Keys are case-sensitive, and a
case-only match is suggested.

**Fix.** Correct the key. A binding is not a key: it starts with `read`, `write` or `param`.

## DSH7311

<!-- generated:begin DSH7311 -->
**Severity** error

**Message**

```
'{0}' is a key of {1} passes, and '{2}' is a {3} pass.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1802`
<!-- generated:end DSH7311 -->

**Cause.** A pass has a key of another kind of pass -- `Threads` in a `fullscreen` pass, `Filter` in a
`compute` one, `Value` in a `copy`. The message names a kind the key belongs to.

**Fix.** Remove the key, or change the pass's kind if the key is what was meant.

## DSH7312

<!-- generated:begin DSH7312 -->
**Severity** error

**Message**

```
'{0}' is set twice in pass '{1}'; it was already set on line {2}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1771`
<!-- generated:end DSH7312 -->

**Cause.** One key is written twice in one pass. Keys are compared case-sensitively, so `injection`
beside `Injection` is DSH7310, not this.

**Fix.** Keep one of the two.

## DSH7313

<!-- generated:begin DSH7313 -->
**Severity** error

**Message**

```
(built at runtime)
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1222`
<!-- generated:end DSH7313 -->

**Cause.** A pass key has a value it cannot take; the message says which. Numbers are folded when the
pipeline compiles, so a `static const` works and a `uniform` does not.

- `Injection` is quoted or is not a name; `Enabled` is neither `true` nor a name (a number, a quoted
  string).
- `Material` or `Shader` is not a quoted, non-empty string; `Entry` is not a function name.
- `Threads` is not two or three whole numbers, at least 1 each way, at most 64 in z and 1024 in all.
- `Dispatch` is not a buffer, `<buffer> * n` or `<buffer> / n` with a positive `n`, or two or three
  whole numbers of at least 1.
- `Mode`, `Depth`, `Cull`, `Blend` or `Nanite` is not one of its words, or is quoted; `Depth = Own(...)`
  does not hold exactly one buffer name.
- `Usage` is not usages joined by `|`, or names one that does not exist (`Splines` is suggested as
  `SplineMesh`).
- `Nanite = AssignStencil(n)` has no single `n` from 1 to 255; `Nanite = StencilMask` stands in a pass
  whose `Filter` has no `Stencil(...)` term to mask by.
- `NaniteValue` (mesh) or `Value` (clear) is not one to four numbers.
- The value or the mask of a `Stencil(...)` filter term is not a whole number.

**Fix.** Write the value in the form the message gives. A pass that selects by layer or list and
wants its Nanite members covered uses `Nanite = AssignStencil(n)`, which gives them custom stencil
`n`.

## DSH7314

<!-- generated:begin DSH7314 -->
**Severity** error

**Message**

```
A filter joins its terms with '|' (either) and '&' (both), and nothing else.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2201`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2225`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2236`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2251`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2267`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2295`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2320`
<!-- generated:end DSH7314 -->

**Cause.** A mesh pass's `Filter` is not a filter; the message says which part:

- the terms are joined by something other than `|` (either) and `&` (both) -- `||`, `&&` and `+` are
  not filter operators;
- a term is not a call -- `Filter = Highlight;` for `Filter = Layer(Highlight);` -- or calls something
  other than `Stencil`, `Layer` and `List`;
- a term passes an argument by name: `Stencil(Value = 1)`;
- `Stencil` has no argument or more than two, or a value or mask outside 0 to 255;
- `Layer` is empty or holds something other than names joined by `|`; `List` holds other than one
  name.

**Fix.** Build the filter from the three terms; `&` binds tighter than `|`:

```hlsl
Filter = Stencil(4, 0x0F) | Layer(Enemies | Allies) & List(Visible);
```

## DSH7315

<!-- generated:begin DSH7315 -->
**Severity** error

**Message**

```
Fullscreen pass '{0}' needs 'Material = "..."' (a Post Process material) or 'Shader = "<file>.usf"' with 'Entry'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2814`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2824`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2915`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2925`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2971`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2985`
<!-- generated:end DSH7315 -->

**Cause.** A pass lacks a key its kind needs; the message says which:

- a `fullscreen` pass names neither a `Material` (a Post Process material) nor a `Shader`;
- a `fullscreen` pass with `Shader`, or a `compute` pass, has no `Entry`;
- a `compute` pass has no `Shader`;
- a `mesh` pass has no `Filter`;
- a `mesh` pass says `Mode = Override` or `Mode = OwnOrOverride` and names no `Material` to draw with.

**Fix.** Add the key: `Material = "PP_Composite";`, or `Shader = "BoxBlur.usf";` with
`Entry = BlurCS;`, or `Filter = Layer(Highlight);`. A mesh pass that draws each primitive with its own
material says `Mode = Own` (the default without a `Material`).

## DSH7316

<!-- generated:begin DSH7316 -->
**Severity** error

**Message**

```
Fullscreen pass '{0}' draws a 'Material' or runs a 'Shader', and it names both.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2804`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2834`
<!-- generated:end DSH7316 -->

**Cause.** Two keys of a `fullscreen` pass contradict each other. Two messages: the pass names both a
`Material` and a `Shader`, and it either draws the one or runs the other; or it names a `Material` and
an `Entry`, which is a function of a shader file.

**Fix.** Keep `Material` for a Post Process material, or `Shader` with `Entry` for HLSL, and remove
the other.

## DSH7317

<!-- generated:begin DSH7317 -->
**Severity** error

**Message**

```
A fullscreen material pass writes exactly one buffer, as 'write Buffer;', and '{0}' writes {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2847`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2858`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2884`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2935`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3017`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3027`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3048`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3060`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3081`
<!-- generated:end DSH7317 -->

**Cause.** A pass reads or writes a number or a shape of buffers its kind does not have; the message
says which:

- a `fullscreen` pass with `Material` writes other than exactly one buffer, or names its write
  (`write Out = Mask;`): a material has one output and no name for it;
- a `fullscreen` pass with `Shader`, or a `compute` pass, writes nothing;
- a `mesh` pass reads a buffer (its material samples what it needs itself), writes none or more than
  four, writes without naming the output of `UE.DreamPassOutput` it takes (`Output0` to `Output3`),
  or writes an integer buffer;
- a `clear` pass does not write exactly one buffer and read none, or a `copy` pass does not read one
  and write one.

**Fix.** Bind what the kind takes:

```hlsl
pass Composite : fullscreen { Material = "PP_Composite"; read Mask; write SceneColor; }
pass DrawMask  : mesh       { Filter = Layer(Highlight); Material = "M_Mask"; write Output0 = Mask; }
pass Grab      : copy       { read SceneColor; write Half; }
```

## DSH7318

<!-- generated:begin DSH7318 -->
**Severity** error

**Message**

```
'{0}.Previous' is last frame's contents, which nothing writes any more; write '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2352`
<!-- generated:end DSH7318 -->

**Cause.** A pass writes `B.Previous`. That is last frame's contents of a `History = true` buffer,
which only a `read` can name; this frame's writes go to `B`, which is next frame's `.Previous`.

**Fix.** Write `B`: `write Result = Wind;` beside `read Previous = Wind.Previous;`.

## DSH7319

<!-- generated:begin DSH7319 -->
**Severity** error

**Message**

```
'{0}' is bound twice in pass '{1}'; inside a pass every input, output and parameter has a name of its own.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3132`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3215`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3271`
<!-- generated:end DSH7319 -->

**Cause.** A name the pass binds is taken already. Inside one pass every name a `read`, a `write` or
a `param` binds is used once. A pass whose code is a `.usf` (`compute`, or `fullscreen` with
`Shader`) has one namespace for all three, because each binding becomes a `#define` of its HLSL
slot; there the message also covers:

- a name starting with `DP_`, the prefix of the slot's own parameters;
- a name that is the pass's `Entry`, or the slot's own entry point (`DreamPassMainCS`,
  `DreamPassMainPS`);
- `View` at `BeginView`, where the slot defines `View` itself;
- a name the slot derives from another binding: a `read X` also defines `XSize` and `XUVRect`, a
  `write Y` defines `YSize`.

In a material or a mesh pass, the reads, the writes and the params are each checked among themselves —
and ignoring case: the pass matches them to its material's UserSceneTexture inputs and parameters as
Unreal names, so `read Mask` and `read mask` would bind one input twice. A `.usf` pass's names keep
their case, as the `#define`s they become do.

**Fix.** Rename one of the two:

```hlsl
read  Source = Mask;
param Width  = OutlineWidth;   // not 'SourceSize', which the slot gives the size of 'Source'
```

## DSH7320

<!-- generated:begin DSH7320 -->
**Severity** error

**Message**

```
'{0}' is an array, and a pipeline has no array parameters or constants; declare one per element.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:766`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:778`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:789`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:802`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:812`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:863`
<!-- generated:end DSH7320 -->

**Cause.** A `uniform` or `static const` of the `.dsp` is not something a pipeline can hold. Six
messages:

- it is an array;
- a `uniform` is not `float` to `float4`, `int`, `bool` or `Texture2D`;
- a `static const` is not a number or a bool of one to four components;
- a `Texture2D` uniform has an initializer, and a texture takes an asset;
- a `static const` has no initializer;
- a `uniform`'s default cannot be folded (it reads another `uniform`, say), and the default is written
  into the pipeline asset.

**Fix.** Declare one value per element, in a type the pipeline takes, with a default built from
literals and `static const`s; a texture's default is a `/// @default`:

```hlsl
static const float HalfWidth = 1.5;

/// @default /Game/Textures/T_Noise
uniform Texture2D Noise;
uniform float     Radius = HalfWidth * 2.0;
```

## DSH7321

<!-- generated:begin DSH7321 -->
**Severity** error

**Message**

```
'{0}' is an integer format, which no pass can read or write yet: HLSL passes see float4 textures, and materials and mesh passes write floats. Use 'R32F' or 'RG32F' (an id is exact up to 16777216).
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1473`
<!-- generated:end DSH7321 -->

**Cause.** A buffer is declared `R32U` or `RG32U`. No pass can use an integer buffer yet: an HLSL slot
declares its inputs `Texture2D<float4>` and its compute outputs `RWTexture2D<float4>`, which an
integer view does not match, and materials, mesh passes, exports and the visualizer read and write
floats.

**Fix.** Use `R32F` or `RG32F`. A float holds a whole number exactly up to 16777216 (2^24), enough
for an object id.

## DSH7322

<!-- generated:begin DSH7322 -->
**Severity** error

**Message**

```
'Enabled' is false, so this would never run, and a pipeline asset has no switch that is always off; drive it with a 'uniform bool', or comment the declaration out.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:967`
<!-- generated:end DSH7322 -->

**Cause.** `Enabled` -- of `#pragma pipeline` or of a pass -- is `false`, or a `static const bool` that
folds to `false`. A pipeline asset has no switch that is always off: the pipeline or the pass would
never run.

**Fix.** Drive it with a `uniform bool`, which an activation can turn on and whose default may be
`false`, or comment the declaration out:

```hlsl
uniform bool bDebugView = false;

pass Debug : fullscreen
{
    Enabled = bDebugView;
    ...
}
```

## DSH7325

<!-- generated:begin DSH7325 -->
**Severity** error

**Message**

```
A fullscreen material pass needs the scene textures, which do not exist yet at {0}; run '{1}' at AfterBasePass or later.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3856`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3867`
<!-- generated:end DSH7325 -->

**Cause.** A kind of pass runs at an injection point where it cannot work. Three messages:

- a `fullscreen` pass with `Material` at `BeginView` or `BeforeBasePass`: a Post Process material
  needs the scene textures, which do not exist yet;
- a `mesh` pass at `BeginView`, where the view has no depth yet;
- a `mesh` pass at `EndOfView`, where the view is at output resolution and has no depth to draw
  against.

**Fix.** Move the pass: a fullscreen material to `AfterBasePass` or later (`BeforePostProcess` is
where it is known to work), a mesh pass to a point from `BeforeBasePass` to `PostProcess.AfterFXAA`.
Work that has to run at `BeginView` is a `compute` pass, or a `fullscreen` pass with `Shader`.

## DSH7326

<!-- generated:begin DSH7326 -->
**Severity** info

**Message**

```
At AfterBasePass scene colour holds the emissive light only; nothing is lit yet, so pass '{0}' {1} that.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3831`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3879`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3892`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3902`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3912`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3923`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4020`
<!-- generated:end DSH7326 -->

**Cause.** What a pass sees where it runs; nothing in the file is wrong. The message says which:

- the pass reads or writes `SceneColor` at `AfterBasePass`, where it holds the emissive light only;
- a `fullscreen` pass with `Material` runs at `AfterBasePass` or `AfterOpaque`, which has not been
  verified on this engine yet; `BeforePostProcess` is the point it is known to work at;
- an HLSL pass -- a `fullscreen` pass with `Shader`, or a `compute` pass -- runs at `BeginView`,
  where no scene texture exists yet (the scene textures are placeholders, and `View` cannot be used at
  all: `DP_Time` stands in for its timing), or at `BeforeBasePass`, where only the scene depth does;
- a `mesh` pass at `BeforeBasePass` tests against the scene depth (`Depth = TestScene`), which holds
  only what the depth prepass drew;
- a `mesh` pass after the upscaler (`PostProcess.ReplaceTonemapper` to `PostProcess.AfterFXAA`) tests
  against the scene depth, which is still at render resolution -- directly, or through a copy brought
  into the pixels of outputs of another size -- so its depth test is only as fine as the render
  resolution;
- a `mesh` pass writes a texture of the scene (`SceneColor`, the GBuffer at `AfterBasePass`,
  `Translucency`) and tests against its own depth, `Depth = Own(...)`, which is bound from its corner:
  in a view that does not start at the corner of the scene's textures -- the second view of split
  screen, the right eye in stereo -- the depth does not reach the view's pixels, and the runtime skips
  the pass there.

**Fix.** Nothing has to change if that is what the pass means to see. Otherwise move it to a later
point, or give a mesh pass at `BeforeBasePass` `Depth = None` or `Depth = Own(...)`. A mesh pass with
its own depth that has to run in split screen or in stereo writes a buffer of the pipeline instead of
the scene's texture, for a later pass to bring into the scene, or tests against the scene depth.

## DSH7327

<!-- generated:begin DSH7327 -->
**Severity** error

**Message**

```
'CustomStencil' is the stencil half of the custom depth texture, which no pass can bind as a texture of its own, and pass '{0}' {1} it; read it through the scene textures instead: a SceneTexture node in the material, CalcSceneCustomStencil in a '.usf'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3730`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3790`
<!-- generated:end DSH7327 -->

**Cause.** A pass binds a built-in texture where it cannot have it. Three messages:

- `CustomStencil`, anywhere: it is the stencil half of the custom depth texture, not a texture a pass
  can bind on its own;
- `Translucency` outside the post-process chain (`PostProcess.*`), the only place it exists;
- another built-in before it exists: `SceneDepth` from `BeforeBasePass` on; `SceneColor`, the GBuffer,
  `Velocity` and `CustomDepth` from `AfterBasePass` on (`CustomDepth` for certain from `AfterOpaque`,
  DSH7329).

**Fix.** Run the pass at a point where the texture exists. Custom stencil is read through the scene
textures: a `SceneTexture` node in the material (`UE.SceneTexture(SceneTextureId = CustomStencil)`),
`CalcSceneCustomStencil` in a `.usf`.

## DSH7328

<!-- generated:begin DSH7328 -->
**Severity** error

**Message**

```
(built at runtime)
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3819`
<!-- generated:end DSH7328 -->

**Cause.** A pass writes a built-in texture that cannot be written where it runs; the message says
which:

- `SceneColor` at `PostProcess.TranslucencyAfterDOF`, where the post-process chain carries the
  translucency and the scene colour is read-only;
- `Translucency` anywhere but `PostProcess.TranslucencyAfterDOF`;
- a texture that is the renderer's: `SceneDepth`, `CustomDepth` and `Velocity` anywhere, `GBufferA` to
  `GBufferF` anywhere but `AfterBasePass`, where the GBuffer is written in place.

**Fix.** Write a buffer of the pipeline instead, or move the pass to where the texture is writable:
`Translucency` at `PostProcess.TranslucencyAfterDOF`, `SceneColor` at any other point from
`AfterBasePass` on.

## DSH7329

<!-- generated:begin DSH7329 -->
**Severity** info

**Message**

```
'{0}' exists at AfterBasePass only when r.CustomDepth.Order draws custom depth before the base pass; where it does not, pass '{1}' reads a cleared placeholder. From AfterOpaque on it always exists.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3762`
<!-- generated:end DSH7329 -->

**Cause.** A pass at `AfterBasePass` reads `CustomDepth`, which is drawn before the base pass only when
`r.CustomDepth.Order` says so: `0`, or the default `2` with DBuffer decals on. With `1`, or `2`
without DBuffer decals, the pass reads a cleared placeholder.

**Fix.** Nothing to fix where the project draws custom depth first. Otherwise run the pass at
`AfterOpaque` or later, where custom depth always exists.

## DSH7330

<!-- generated:begin DSH7330 -->
**Severity** error

**Message**

```
'Depth = Own({0})' tests against a depth of this pipeline's own, and '{0}' is a built-in texture; declare 'buffer MyDepth : Depth32;', or write 'Depth = TestScene'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2018`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2033`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2388`
<!-- generated:end DSH7330 -->

**Cause.** A `Depth32` buffer is used as something other than a mesh pass's own depth, or an own depth
is not a `Depth32` buffer. Three messages:

- `Depth = Own(SceneDepth)`, or another built-in texture: `Own` tests the pass's primitives among
  themselves, against a depth of the pipeline's own;
- `Depth = Own(B)` where `B` is not `Depth32`;
- a `read` or a `write` binds a `Depth32` buffer -- in a `copy` pass as well. A depth buffer is only
  ever tested against, and reset: the one binding it takes is the `write` of a `clear` pass, whose
  `Value` is then the depth it is cleared to.

**Fix.** Declare a depth buffer for `Own`, and bind colour buffers everywhere else -- a `clear` pass
that resets the depth (`write ObjDepth;`) is the one exception; for the scene's depth, write
`Depth = TestScene`:

```hlsl
buffer ObjDepth   : Depth32;
buffer Silhouette : RG16F;

pass DrawObjects : mesh
{
    Filter   = List(Enemies);
    Material = "M_XRayDepth";
    Depth    = Own(ObjDepth);
    write Output0 = Silhouette;
}
```

## DSH7331

<!-- generated:begin DSH7331 -->
**Severity** error

**Message**

```
Pass '{0}' reads '{1}', which no pass writes, and '{1}' is 'Clear = None', so what it reads is undefined.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4183`
<!-- generated:end DSH7331 -->

**Cause.** A pass reads a buffer declared `Clear = None` before any pass has written it, in frame
order (injection point, then declaration order). Two messages: no pass writes it at all, or its first
writer runs after the reader. With no clear value, what such a read sees is undefined.

**Fix.** Move the reader after the writer, give the buffer a `Clear` value (`Clear = 0` is the
default), or read last frame's `B.Previous` of a `History = true` buffer.

## DSH7332

<!-- generated:begin DSH7332 -->
**Severity** warning

**Message**

```
Pass '{0}' reads '{1}' before '{2}' writes it in the frame, so it reads the buffer's 'Clear' value; move the reader after the writer, or read '{1}.Previous'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4193`
<!-- generated:end DSH7332 -->

**Cause.** A pass reads a buffer before the first pass that writes it, in frame order (injection
point, then declaration order), so it reads the buffer's `Clear` value rather than what that writer
draws. Often the two passes sit at injection points in the wrong order.

**Fix.** Move the reader after the writer -- later in the file, or at a later injection point. A reader
that means last frame's contents reads `B.Previous` of a `History = true` buffer.

## DSH7333

<!-- generated:begin DSH7333 -->
**Severity** error

**Message**

```
Pass '{0}' reads and writes '{1}', and one pass cannot have one texture as its input and its output; write another buffer, or read '{1}.Previous' of a 'History = true' buffer.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3957`
<!-- generated:end DSH7333 -->

**Cause.** A pass reads and writes the same texture, and one GPU pass cannot have one texture as its
input and its output. `B.Previous` and `B` are two textures and do not count. Nor does the chain's
colour: a pass of any kind -- a `fullscreen` or `compute` pass, or a `copy` -- may read `SceneColor`
while it writes it, and `Translucency` at `PostProcess.TranslucencyAfterDOF`, because a pass that
writes it draws into a scratch texture that is brought back afterwards.

**Fix.** Write another buffer and read that in a later pass, or read `B.Previous` of a
`History = true` buffer.

## DSH7334

<!-- generated:begin DSH7334 -->
**Severity** error

**Message**

```
'{0}' reads no UserSceneTexture of '{1}': its inputs are {2}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3404`
<!-- generated:end DSH7334 -->

**Cause.** A `fullscreen` pass binds `read X = B`, and its material has no `UE.UserSceneTexture` named
`X`; the message lists the names it has. `X` is the name in the material, `B` the buffer of the
pipeline, and `read B;` is short for `read B = B;`. An instance's renamed inputs count under their new
names. A `dsc check` of a `.dsp` whose material has never been built reads the material's inputs off
its source, and does not see a `UE.UserSceneTexture` inside a material function it calls.

**Fix.** Bind to a name the material reads -- `read Mask = Blurred;` -- or add the node to the
material:

```hlsl
float Halo = UE.UserSceneTexture(UserSceneTexture = "Blurred", Coordinates = UV).Color.r;
```

## DSH7335

<!-- generated:begin DSH7335 -->
**Severity** warning

**Message**

```
'{0}' reads the UserSceneTexture '{1}', and pass '{2}' binds nothing to it, so it samples black; add 'read {1} = <Buffer>;'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3419`
<!-- generated:end DSH7335 -->

**Cause.** The material of a `fullscreen` pass reads a `UE.UserSceneTexture` the pass binds no buffer
to. The engine still gives the input a slot, and it samples black.

**Fix.** Bind it -- `read <Name> = <Buffer>;` -- or remove the node from the material.

## DSH7336

<!-- generated:begin DSH7336 -->
**Severity** error

**Message**

```
Fullscreen material pass '{0}' reads {1} buffers, and a post-process material has {2} input slots, shared with its own SceneTexture nodes; split the pass in two, or write it with 'Shader =' (a '.usf' pass reads up to {3}).
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2868`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3438`
<!-- generated:end DSH7336 -->

**Cause.** A `fullscreen` pass with `Material` needs more inputs than a Post Process material has. The
engine gives it five post-process input slots, shared: every `SceneTexture` node reading
`PostProcessInput0` to `PostProcessInput4` takes one, and every `UE.UserSceneTexture` input of the
material, bound or not, takes one of the rest. Two messages: the pass reads more than five buffers;
or the material's `UserSceneTexture` inputs do not fit beside its own `SceneTexture` nodes -- a
material that reads `PostProcessInput0` for the scene colour has four left.

**Fix.** Read fewer buffers in one pass: split it in two, or pack several channels into one buffer.
Or write the pass with `Shader =`; a `.usf` pass reads up to eight.

## DSH7337

<!-- generated:begin DSH7337 -->
**Severity** error

**Message**

```
'{0}' is a {1} material, and a fullscreen pass draws a Post Process one: '#pragma material(Domain = PostProcess)'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3388`
<!-- generated:end DSH7337 -->

**Cause.** A `fullscreen` pass draws a material whose domain is not Post Process. The pass runs the
material through the engine's post-process material pass, which draws only that domain.

**Fix.** Give the material `#pragma material(Domain = PostProcess)`.

## DSH7338

<!-- generated:begin DSH7338 -->
**Severity** error

**Message**

```
Pass '{0}' writes a data buffer, and '{1}' scales what it reads and writes by the exposure; give it '#pragma material(bDisablePreExposureScale = true)'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3455`
<!-- generated:end DSH7338 -->

**Cause.** A `fullscreen` pass with `Material` writes a buffer of the pipeline rather than the scene
colour, and the material leaves pre-exposure on. The engine then scales what the material reads and
writes by the exposure, so a mask or a vector stored in the buffer changes with the scene's
brightness.

**Fix.** Turn it off in the material's `.dss`:

```hlsl
#pragma material(Domain = PostProcess, bDisablePreExposureScale = true)
```

## DSH7339

<!-- generated:begin DSH7339 -->
**Severity** warning

**Message**

```
'{0}' is compiled for BlendableLocation {1}, and pass '{2}' runs it at {3}, {4} tonemapping, so its colours are in another space than it expects; use {5}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3471`
<!-- generated:end DSH7339 -->

**Cause.** A `fullscreen` pass runs its material on the other side of tonemapping from the
`BlendableLocation` it is compiled for, so its colours are in another space than it expects: a
material compiled for `SceneColorAfterTonemapping` -- the engine's default -- or `ReplacingTonemapper`
at a point before tonemapping, or one compiled for another location at
`PostProcess.ReplaceTonemapper`, `PostProcess.AfterTonemap`, `PostProcess.AfterFXAA` or `EndOfView`.

**Fix.** Give the material the location that matches the pass's injection point --
`SceneColorAfterDOF` or `SceneColorBeforeDOF` before tonemapping, `SceneColorAfterTonemapping` after
it:

```hlsl
#pragma material(Domain = PostProcess, BlendableLocation = SceneColorAfterDOF)
```

## DSH7340

<!-- generated:begin DSH7340 -->
**Severity** error

**Message**

```
'{0}' has no UE.DreamPassOutput in its graph, so a mesh pass has nothing to write; add 'UE.DreamPassOutput(Output0 = ...);' to the material.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3505`
<!-- generated:end DSH7340 -->

**Cause.** A `mesh` pass's override `Material` has no `UE.DreamPassOutput` in its own graph. The node
is what a mesh pass writes its outputs from, and what makes the engine compile the mesh pass's
shaders for the material at all; one inside a material function the material calls does not count
(DSH5302). Checked on Unreal Engine 5.8 and later, where the Custom Pass runtime is.

**Fix.** Add the node to the material:

```hlsl
UE.DreamPassOutput(Output0 = float4(1, 0, 0, 0));
```

## DSH7341

<!-- generated:begin DSH7341 -->
**Severity** error

**Message**

```
'{0}' leaves Output{1} of its UE.DreamPassOutput unconnected, so '{2}' would receive nothing.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3522`
<!-- generated:end DSH7341 -->

**Cause.** A `mesh` pass writes an output of `UE.DreamPassOutput` -- `write Output1 = Id;` -- whose pin
its material leaves unconnected, so the buffer would receive nothing.

**Fix.** Connect the pin in the material -- `UE.DreamPassOutput(Output0 = Mask, Output1 = Id);` -- or
remove the write the material does not fill.

## DSH7342

<!-- generated:begin DSH7342 -->
**Severity** error

**Message**

```
Mesh pass '{0}' draws {1} primitives, and '{2}' is not compiled for them; give the material its usage flag ('#pragma material({3} = true)' in its '.dss'), or leave {1} out of 'Usage'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3543`
<!-- generated:end DSH7342 -->

**Cause.** A `mesh` pass draws primitives of a kind its override material is not compiled for. The
pass's `Usage` -- `StaticMesh | InstancedStaticMeshes | SkeletalMesh` when it is not written -- lists
what it draws, and each kind needs the material's usage flag; `Landscape` needs none. The message
names the missing flag. Checked on Unreal Engine 5.8 and later, where the Custom Pass runtime is.

**Fix.** Set the flag in the material's `.dss`, or narrow `Usage` to what the pass meets
(`Usage = StaticMesh | InstancedStaticMeshes;`):

```hlsl
#pragma material(ShadingModel = Unlit, bUsedWithSkeletalMesh = true, bUsedWithInstancedStaticMeshes = true)
```

## DSH7343

<!-- generated:begin DSH7343 -->
**Severity** error

**Message**

```
Mesh pass '{0}' writes '{1}', a texture of the scene, together with '{2}', a buffer of the pipeline. A mesh pass draws all its targets through one viewport, so they have to be one size holding the view at one place; the scene's textures are usually larger than the view they hold (rounded up, and in the editor grown to the largest view so far) while a buffer is the view's size, and the runtime skips the pass whenever the two differ. Write them in two mesh passes.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3985`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4002`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4035`
<!-- generated:end DSH7343 -->

**Cause.** The targets of a `mesh` pass are drawn together, through one viewport. Three messages:

- the pass writes a texture of the scene -- `SceneColor`, the GBuffer at `AfterBasePass`,
  `Translucency` -- together with a buffer of the pipeline. The scene's textures are usually larger
  than the view they hold (rounded up, and in the editor grown to the largest view so far) while a
  buffer is the view's size, and the runtime skips the pass whenever the two differ;
- two of its outputs differ in size (`Render` against `Render x 0.5`, a fixed `256 x 256` against the
  view);
- its own depth (`Depth = Own(...)`) is smaller than its outputs. An own depth is bound as it is, so
  it has to be at least their size: their resolution with a scale no smaller than theirs, or a fixed
  size no smaller than theirs. A depth of another resolution than the outputs (`Output` against
  `Render`, a fixed size against one that follows the view) cannot be compared before the frame, and
  is refused too.

`Depth = TestScene` asks nothing of the outputs' size: where they do not hold the view at the scene
depth's pixels, the runtime tests against a copy of the scene depth brought into theirs.

**Fix.** Give the outputs one `Scale`, `Resolution` or `Size`, and an own depth their resolution and a
scale (or a fixed size) no smaller than theirs; for a smaller result, draw at full size and `copy`
into a smaller buffer afterwards. Write a texture of the scene and a buffer of the pipeline in two
mesh passes.

## DSH7344

<!-- generated:begin DSH7344 -->
**Severity** error

**Message**

```
'{0}' is a {1} material, and a mesh pass draws primitives with a Surface one.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3493`
<!-- generated:end DSH7344 -->

**Cause.** A `mesh` pass's override `Material` is not a Surface material -- a Post Process, a decal, a
UI material. A mesh pass draws primitives with it, as their own materials draw them.

**Fix.** Make the override a Surface material (the default domain: remove `Domain` from its
`#pragma material`). A Post Process material is drawn by a `fullscreen` pass.

## DSH7345

<!-- generated:begin DSH7345 -->
**Severity** error

**Message**

```
'{0}' asks for the new material translator, which UE.DreamPassOutput does not support; remove 'bEnableNewHLSLGenerator' from it.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3557`
<!-- generated:end DSH7345 -->

**Cause.** A `mesh` pass's override `Material` sets `bEnableNewHLSLGenerator`, and `UE.DreamPassOutput`
does not implement the new material translator; the material's own `.dss` reports the same, as
DSH5301. Checked on Unreal Engine 5.8 and later, where the Custom Pass runtime is.

**Fix.** Remove `bEnableNewHLSLGenerator` from the material's `#pragma material`.

## DSH7346

<!-- generated:begin DSH7346 -->
**Severity** error

**Message**

```
Fullscreen pass '{0}' reads {1} and writes {2} buffers, and the pixel slot a '.usf' pass runs in has {3} inputs and {4} outputs.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2895`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2945`
<!-- generated:end DSH7346 -->

**Cause.** An HLSL pass binds more than its slot holds. A `compute` pass and a `fullscreen` pass with
`Shader` run in a fixed shader slot -- the pixel slot for a fullscreen `.usf`, whatever its output
count -- with 8 inputs (`DP_Input0` to `DP_Input7`) and 4 outputs.

**Fix.** Split the pass in two, or pack values into fewer buffers (an `RGBA16F` buffer holds four
channels).

## DSH7347

<!-- generated:begin DSH7347 -->
**Severity** error

**Message**

```
'{0}' is a {1}, and the parameter block of a shader slot holds numbers only, so no texture parameter reaches a '.usf' pass. A material takes one through 'param': draw this pass with a material ('Material = ...'), or let a fullscreen material pass that takes the texture by 'param' write it into a buffer this pass reads.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3301`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3316`
<!-- generated:end DSH7347 -->

**Cause.** A `param` of an HLSL pass does not fit the slot's parameter block, `DP_Params`: 16 `float4`,
numbers only. Two messages:

- the `param` passes a `Texture2D` uniform, which has no place among numbers;
- the parameters, packed in order -- a `float4` takes a vector of its own, a `float3` the `xyz` of an
  empty one, a `float2` a free half, a scalar the next free component -- need more than 16 vectors.

**Fix.** A texture parameter reaches a material through `param`, never a slot: draw the pass with
`Material = ...` and hand the material the texture with `param`, or let a fullscreen material pass
take it by `param` and write it into a buffer the `.usf` pass then `read`s. For too many numbers,
pass fewer, or pack them into `float4` uniforms yourself.

## DSH7350

<!-- generated:begin DSH7350 -->
**Severity** error

**Message**

```
Pass '{0}' writes scene colour, which is {1} at {2}, and '{3}' with it, which is {4}; targets drawn together have one size.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4065`
<!-- generated:end DSH7350 -->

**Cause.** A `fullscreen` pass with `Shader` and several outputs writes `SceneColor` together with a
buffer of another size, and targets drawn together have one size. The scene colour is at scale 1:
render resolution before the upscaler, output resolution from `PostProcess.ReplaceTonemapper` on. A
`mesh` pass is not checked here: it may not write a buffer of the pipeline beside a texture of the
scene at all (DSH7343).

**Fix.** Give the buffer the scene colour's size at that point: `Scale = 1` (the default) and the
matching `Resolution`, which a buffer takes from its first writer when it does not say. Write a buffer
of another size in a pass of its own.

## DSH7351

<!-- generated:begin DSH7351 -->
**Severity** error

**Message**

```
Pass '{0}' replaces the tonemapper, and so does '{1}'; one view runs one tonemapper, so a pipeline replaces it at most once.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4102`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4116`
<!-- generated:end DSH7351 -->

**Cause.** Two messages, both about `PostProcess.ReplaceTonemapper`: a second pass of the pipeline runs
there, and one view runs one tonemapper; or the pass that runs there writes no `SceneColor`, and the
tonemapper's replacement has to make the frame's final colour.

**Fix.** Keep one pass at `PostProcess.ReplaceTonemapper`, with `write SceneColor;`. Work that follows
the tonemapper goes to `PostProcess.AfterTonemap`.

## DSH7352

<!-- generated:begin DSH7352 -->
**Severity** error

**Message**

```
A 'param' is a parameter of the pipeline (times a number, plus a number), 'DreamPassWeight' (the same), or a value the compiler can fold, and this is none of them.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2548`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2591`
<!-- generated:end DSH7352 -->

**Cause.** A `param` value cannot be stored in the pipeline asset, which holds a constant, or a
parameter of the pipeline (or `DreamPassWeight`) times a number plus a number: the value is computed
on the CPU once per view, not in the shader. Two messages: an expression of constants the compiler
cannot fold; or one over a parameter in another shape -- `Radius * Gust` (two parameters),
`1.0 / Radius`, `sin(Radius)`, `Radius > 2 ? 1 : 0`.

**Fix.** Write `Source * a + b`, with `a` and `b` numbers the compiler can fold, and compute anything
else in the shader or the material from the plain parameter:

```hlsl
param Radius = OutlineWidth * 2 + 1;
```

## DSH7353

<!-- generated:begin DSH7353 -->
**Severity** error

**Message**

```
What scales or offsets a parameter in a 'param' is one number the compiler can fold; a vector, or a value known only at run time, has no place in the asset's 'Source * a + b'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2659`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2750`
<!-- generated:end DSH7353 -->

**Cause.** A `param` scales or offsets a parameter by something the asset cannot store. Two messages:
the factor or the offset is not one number the compiler can fold -- a vector, as in
`OutlineColor * float4(1, 1, 1, 0.5)`, or an expression it cannot evaluate; or the parameter is a
`bool` or a `Texture2D`, which is passed on as it is and cannot be scaled.

**Fix.** Scale by one number -- `param Color = OutlineColor * 0.5;` -- and do per-component work in the
shader or the material. Pass a `bool` or a texture unchanged.

## DSH7354

<!-- generated:begin DSH7354 -->
**Severity** info

**Message**

```
Buffer '{0}' is exported and last written at {1}: opaque and translucent materials that sample it see the previous frame's contents, UI sees this frame's.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4234`
<!-- generated:end DSH7354 -->

**Cause.** An exported buffer's last writer runs at `AfterBasePass` or later, so its render target is
updated after some materials that sample it have drawn. Last written at `AfterBasePass` or
`AfterOpaque`: opaque materials see last frame's contents, while translucent and post-process
materials and UI see this frame's. Last written at `BeforePostProcess` or later: opaque and
translucent materials see last frame's, UI this frame's.

**Fix.** Nothing to fix if a frame of delay is acceptable. To have every reader see this frame's
contents, write the buffer at `BeginView` or `BeforeBasePass` -- a `compute` pass, or a `mesh` pass at
`BeforeBasePass` with `Depth = None` or `Depth = Own(...)`.

## DSH7355

<!-- generated:begin DSH7355 -->
**Severity** error

**Message**

```
Buffer '{0}' is exported, and an exported buffer becomes a render target asset that materials sample, which a '{1}' buffer cannot be; export a float format.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:1645`
<!-- generated:end DSH7355 -->

**Cause.** A buffer is exported in a format that cannot be a sampled render target: `Depth32`, or an
integer format. An exported buffer becomes a render target asset that materials, UMG and Niagara
sample as a float texture.

**Fix.** Export a float or normalized format. For depth, have a pass write it into an `R32F` buffer and
export that; a mesh pass can write `UE.PixelDepth()` through `UE.DreamPassOutput`.

## DSH7356

<!-- generated:begin DSH7356 -->
**Severity** warning

**Message**

```
No pass writes buffer '{0}', so whoever reads it reads its 'Clear' value.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4209`
<!-- generated:end DSH7356 -->

**Cause.** No pass writes the buffer -- no `write`, and no mesh pass's `Depth = Own(...)` -- so every
read of it gets its `Clear` value. Usually a writer was renamed or removed.

**Fix.** Write it in a pass, or remove it and its readers. A value that is the same everywhere
reaches a pass as a `param`, not as a buffer.

## DSH7357

<!-- generated:begin DSH7357 -->
**Severity** warning

**Message**

```
No pass reads buffer '{0}' and it is not exported, so writing it is wasted work; read it, export it, or remove it.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4219`
<!-- generated:end DSH7357 -->

**Cause.** A pass writes the buffer and nothing reads it -- no `read` of it or of its `.Previous`, no
mesh pass's `Depth = Own(...)` -- and it is not exported, so writing it is wasted GPU work.

**Fix.** Read it in a later pass, export it (`Export = true`) for materials to sample, or remove it and
its writes.

## DSH7359

<!-- generated:begin DSH7359 -->
**Severity** warning

**Message**

```
'Mode = Own' draws every primitive with its own material, so the 'Material' of mesh pass '{0}' is never used.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:2996`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3091`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:3108`
<!-- generated:end DSH7359 -->

**Cause.** A key or a binding of a pass reaches nothing. Three messages:

- a `mesh` pass in `Mode = Own` names a `Material`, and draws every primitive with the primitive's own
  material, so the override is never used;
- a `mesh` pass in `Mode = Own` has `param` lines, and has no override material to give them to;
- a `clear` or `copy` pass has `param` lines, and has no shader or material at all.

Each is a warning: the pass still builds.

**Fix.** Remove what is unused, or switch the mesh pass to `Mode = Override` (everything drawn with the
`Material`) or `Mode = OwnOrOverride` (the primitive's own material where it has
`UE.DreamPassOutput`, the override elsewhere).

## DSH7360

<!-- generated:begin DSH7360 -->
**Severity** info

**Message**

```
The materials, shader files and pass layers this pipeline names are not available here, so they were taken as written and the checks that need them were skipped; compile the pipeline in the editor to have them checked.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:536`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:544`
<!-- generated:end DSH7360 -->

**Cause.** Some checks of the pipeline were skipped. Two messages:

- The pipeline was bound by a tool that runs without the editor, so the materials, `.usf` files and
  pass layers it names are taken as written: whether a material exists and what it offers a pass,
  whether a shader file and its `Entry` exist, and whether a layer is in the project settings are not
  checked.
- The engine is older than Unreal Engine 5.8 and has no Custom Pass runtime: what a material offers a
  pass -- its `UserSceneTexture` inputs, its `UE.DreamPassOutput` pins, its usage flags, its
  pre-exposure and translator settings -- is not read or checked, and the pipeline is not built on
  this engine.

**Fix.** Nothing to fix. Compile the pipeline in the editor, on 5.8 or later, to have every reference
checked.

