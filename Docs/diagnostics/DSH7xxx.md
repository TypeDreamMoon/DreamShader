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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1310`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:731`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:758`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:770`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:781`, `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:569`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:803`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:792`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:891`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:138`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1019`, `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1045`, `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:997`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:152`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:290`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:329`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:356`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:352`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:462`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1263`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:972`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:302`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:333`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:417`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:432`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:398`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:213`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:245`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:259`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:272`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:285`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:316`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:657`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:632`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:643`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:843`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:857`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:916`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:572`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:603`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:890`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:901`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:873`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:366`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:506`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:669`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:778`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:832`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:765`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:493`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:682`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:815`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:559`, `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:615`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderInstance.cpp:382`
<!-- generated:end DSH7270 -->

**Cause.** `#pragma instance(...)` contains a bare value. Every entry is `Key = Value`.

**Fix.** Write, for example, `#pragma instance(Parent = "M_Base", TwoSided = true)`.

