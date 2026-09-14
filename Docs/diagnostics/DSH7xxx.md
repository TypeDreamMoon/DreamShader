# DSH7xxx --- Properties, parameters and settings

> The block between the generated markers is written by `.skill/gen-diagnostics.ps1`.
> Everything below a marker is written by hand and survives a regeneration.

## DSH7001

<!-- generated:begin DSH7001 -->
**Severity** error

**Message**

```
Metadata must follow a declaration.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:258`
<!-- generated:end DSH7001 -->

**Cause.** a statement that is only a `[ … ]` block

**Fix.** attach it to a declaration

**See** [Metadata](../parameters/metadata.md)

## DSH7002

<!-- generated:begin DSH7002 -->
**Severity** error

**Message**

```
Metadata 'Slider(min, max)' requires exactly two numeric bounds: '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:361`
<!-- generated:end DSH7002 -->

**Cause.** wrong arity, or non-numeric bounds

**Fix.** write `Slider(0, 1)`

**See** [Metadata](../parameters/metadata.md)

## DSH7003

<!-- generated:begin DSH7003 -->
**Severity** error

**Message**

```
Metadata SliderMin/SliderMax is declared more than once (entry '{0}').
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:370`
<!-- generated:end DSH7003 -->

**Cause.** `Slider( … )` combined with explicit `SliderMin` / `SliderMax`

**Fix.** use one form

**See** [Metadata](../parameters/metadata.md)

## DSH7004

<!-- generated:begin DSH7004 -->
**Severity** error

**Message**

```
Metadata entry '{0}' must use Key=Value syntax.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:383`
<!-- generated:end DSH7004 -->

**Cause.** a metadata entry with no top-level `=`, other than `Slider( … )`

**Fix.** use `Key=Value`

**See** [Metadata](../parameters/metadata.md)

## DSH7005

<!-- generated:begin DSH7005 -->
**Severity** error

**Message**

```
Invalid metadata entry '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:393`
<!-- generated:end DSH7005 -->

**Cause.** the metadata key normalized to empty

**Fix.** supply a key

**See** [Metadata](../parameters/metadata.md)

## DSH7006

<!-- generated:begin DSH7006 -->
**Severity** error

**Message**

```
Metadata key '{0}' is declared more than once.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:400`
<!-- generated:end DSH7006 -->

**Cause.** duplicate key after normalization; the message echoes the original spelling

**Fix.** remove the duplicate

**See** [Metadata](../parameters/metadata.md)

## DSH7007

<!-- generated:begin DSH7007 -->
**Severity** error

**Message**

```
Metadata SortPriority value '{0}' is not an integer.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:419`
<!-- generated:end DSH7007 -->

**Cause.** a non-integer `SortPriority` / `Sort`

**Fix.** use an integer

**See** [Metadata](../parameters/metadata.md)

## DSH7010

<!-- generated:begin DSH7010 -->
**Severity** error

**Message**

```
UE builtin property declarations must specify a function name, for example UE.TexCoord UV.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:492`
<!-- generated:end DSH7010 -->

**Cause.** a bare `UE.` type token

**Fix.** name the builtin

**See** [Properties](../language/properties.md)

## DSH7011

<!-- generated:begin DSH7011 -->
**Severity** error

**Message**

```
Invalid UE builtin declaration '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:504`, `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:522`
<!-- generated:end DSH7011 -->

**Cause.** unbalanced parentheses, or an empty function name before `(`

**Fix.** fix the declaration

**See** [Properties](../language/properties.md)

## DSH7012

<!-- generated:begin DSH7012 -->
**Severity** error

**Message**

```
Unexpected characters after UE builtin argument list in '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:511`
<!-- generated:end DSH7012 -->

**Cause.** trailing text after the closing `)` of a `UE.*` property declaration

**Fix.** end the declaration at `)`

**See** [Properties](../language/properties.md)

## DSH7013

<!-- generated:begin DSH7013 -->
**Severity** error

**Message**

```
UE builtin argument '{0}' must use named syntax like Key=Value in '{1}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:537`
<!-- generated:end DSH7013 -->

**Cause.** a positional argument in a `Properties` `UE.*` declaration

**Fix.** use `Key=Value`

**See** [Properties](../language/properties.md)

## DSH7014

<!-- generated:begin DSH7014 -->
**Severity** error

**Message**

```
Invalid UE builtin argument '{0}' in '{1}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:547`
<!-- generated:end DSH7014 -->

**Cause.** an empty key or empty value in a `Properties` `UE.*` declaration

**Fix.** supply both sides

**See** [Properties](../language/properties.md)

## DSH7015

<!-- generated:begin DSH7015 -->
**Severity** error

**Message**

```
UE builtin argument '{0}' is declared more than once in '{1}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:555`
<!-- generated:end DSH7015 -->

**Cause.** duplicate argument key after normalization

**Fix.** remove the duplicate

**See** [Properties](../language/properties.md)

## DSH7016

<!-- generated:begin DSH7016 -->
**Severity** error

**Message**

```
Unsupported UE builtin function '{0}'. Use OutputType=\"float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture\" for generic MaterialExpression calls.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:582`
<!-- generated:end DSH7016 -->

**Cause.** the same, for the `Properties` declaration form

**Fix.** add `OutputType="…"`; note that `MaterialAttributes` is **not** accepted by the declaration form

**See** [Properties](../language/properties.md)

## DSH7020

<!-- generated:begin DSH7020 -->
**Severity** error

**Message**

```
Invalid property declaration '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:610`
<!-- generated:end DSH7020 -->

**Cause.** no top-level whitespace separating the type from the name

**Fix.** separate them

**See** [Properties](../language/properties.md)

## DSH7021

<!-- generated:begin DSH7021 -->
**Severity** error

**Message**

```
Missing property name in declaration '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:619`
<!-- generated:end DSH7021 -->

**Cause.** the name token is empty

**Fix.** supply a name

**See** [Properties](../language/properties.md)

## DSH7022

<!-- generated:begin DSH7022 -->
**Severity** error

**Message**

```
Missing property type after const in declaration '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:633`
<!-- generated:end DSH7022 -->

**Cause.** `const` with nothing after it

**Fix.** supply a type

**See** [Properties](../language/properties.md)

## DSH7023

<!-- generated:begin DSH7023 -->
**Severity** error

**Message**

```
Invalid boolean default value '{0}' for property '{1}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:656`
<!-- generated:end DSH7023 -->

**Cause.** a `StaticBoolParameter` / `StaticSwitchParameter` default that is not `true` / `false`

**Fix.** use `true` or `false`

**See** [Parameter nodes](../parameters/parameter-nodes.md)

## DSH7024

<!-- generated:begin DSH7024 -->
**Severity** error

**Message**

```
Invalid scalar default value '{0}' for property '{1}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:665`, `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:768`
<!-- generated:end DSH7024 -->

**Cause.** the default did not parse as a number, `true` or `false`

**Fix.** use a numeric literal

**See** [Compact types](../parameters/compact-types.md)

## DSH7025

<!-- generated:begin DSH7025 -->
**Severity** error

**Message**

```
Invalid vector default value '{0}' for property '{1}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:686`, `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:818`
<!-- generated:end DSH7025 -->

**Cause.** the default did not parse as a 1–4 component literal

**Fix.** use `float3(…)`, `vec3(…)` or `( … )`

**See** [Compact types](../parameters/compact-types.md)

## DSH7026

<!-- generated:begin DSH7026 -->
**Severity** error

**Message**

```
Invalid texture default value '{0}' for property '{1}'. {2}
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:700`, `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:854`
<!-- generated:end DSH7026 -->

**Cause.** the texture default failed to resolve

**Fix.** see the inner `Texture Path …` message

**See** [Path](../parameters/path.md)

## DSH7027

<!-- generated:begin DSH7027 -->
**Severity** error

**Message**

```
Invalid texture sample default value '{0}' for property '{1}'. {2}
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:726`
<!-- generated:end DSH7027 -->

**Cause.** the texture-sample parameter default failed to resolve

**Fix.** see the inner message

**See** [Path](../parameters/path.md)

## DSH7028

<!-- generated:begin DSH7028 -->
**Severity** error

**Message**

```
Parameter node type '{0}' is recognized but not supported as a plain Properties declaration yet. Use UE.{1}(OutputType=\"float4\", ...) for reflected node creation.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:735`
<!-- generated:end DSH7028 -->

**Cause.** a known expression-class token that has no `Properties` declaration form

**Fix.** declare it as a `UE.*` builtin property instead

**See** [Parameter nodes](../parameters/parameter-nodes.md)

## DSH7029

<!-- generated:begin DSH7029 -->
**Severity** error

**Message**

```
UE builtin property '{0}' does not support inline defaults. Put arguments inside UE.{1}(...).
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:750`
<!-- generated:end DSH7029 -->

**Cause.** `UE.X Name = value;`

**Fix.** move the value into the argument list

**See** [Properties](../language/properties.md)

## DSH7030

<!-- generated:begin DSH7030 -->
**Severity** error

**Message**

```
Unsupported property type '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:864`
<!-- generated:end DSH7030 -->

**Cause.** the type token matched no compact type, parameter node token or `UE.` prefix

**Fix.** check the token against the type catalogue

**See** [Types](../language/types.md)

## DSH7040

<!-- generated:begin DSH7040 -->
**Severity** error

**Message**

```
Invalid setting declaration '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1141`
<!-- generated:end DSH7040 -->

**Cause.** the statement has no top-level `=`

**Fix.** write `Key = Value;`

**See** [Settings](../settings/index.md)

## DSH7041

<!-- generated:begin DSH7041 -->
**Severity** error

**Message**

```
Invalid empty setting key in '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1150`
<!-- generated:end DSH7041 -->

**Cause.** the key normalized to the empty string

**Fix.** supply a key

**See** [Settings](../settings/index.md)

## DSH7101

<!-- generated:begin DSH7101 -->
**Severity** error

**Message**

```
StaticSwitchParameter '%s' requires True=... and False=... inputs.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:190`
<!-- generated:end DSH7101 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7102

<!-- generated:begin DSH7102 -->
**Severity** error

**Message**

```
StaticSwitchParameter '%s' cannot switch Texture object values.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:207`
<!-- generated:end DSH7102 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7103

<!-- generated:begin DSH7103 -->
**Severity** error

**Message**

```
StaticSwitchParameter '%s' cannot mix Substrate and numeric branches.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:216`
<!-- generated:end DSH7103 -->

**Cause.** One branch of the switch is a `Substrate` value and the other is not. Switching between
two Substrate closures **is** allowed — the engine resolves the static bool while it builds the
material topology tree and descends only the taken side, so the compiler still knows the single
topology it has to translate. What it cannot do is reconcile a closure with a number, because the
two sides would not even be the same kind of value.

**Fix.** Make both branches closures, or make both of them numeric. If one side is meant to be "no
closure", give it the closure you want in that case (a plain `Substrate.Slab(…)`, or for Moon toon a
`Kind = Default` BSDF) rather than a constant.

> [!NOTE]
> Before 2026-09-13 this code refused *any* Substrate branch. A two-closure switch is the shape a
> master material uses to pick a shading variant from a static parameter, so it is allowed now.

## DSH7104

<!-- generated:begin DSH7104 -->
**Severity** error

**Message**

```
StaticSwitchParameter '%s' cannot mix MaterialAttributes and numeric branches.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:220`
<!-- generated:end DSH7104 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7105

<!-- generated:begin DSH7105 -->
**Severity** error

**Message**

```
StaticSwitchParameter '%s' branches must have the same component count, got %d and %d.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:224`
<!-- generated:end DSH7105 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7106

<!-- generated:begin DSH7106 -->
**Severity** error

**Message**

```
Failed to create StaticSwitchParameter node '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:241`
<!-- generated:end DSH7106 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7107

<!-- generated:begin DSH7107 -->
**Severity** error

**Message**

```
Parameter '%s' did not produce an expression node.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:326`
<!-- generated:end DSH7107 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7108

<!-- generated:begin DSH7108 -->
**Severity** error

**Message**

```
Parameter '%s' must be called with named arguments wiring its input pins (e.g. %s(Coordinates=...) or %s(Input=...)).
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:333`
<!-- generated:end DSH7108 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7109

<!-- generated:begin DSH7109 -->
**Severity** error

**Message**

```
Parameter '%s' (%s) has no input pin named '%s'. Asset slots (Texture/Curve/Font/...) are set via [%s=Path(...)] metadata, not call arguments.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:353`
<!-- generated:end DSH7109 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7110

<!-- generated:begin DSH7110 -->
**Severity** error

**Message**

```
Parameter '%s' input '%s' must be a numeric value.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:363`
<!-- generated:end DSH7110 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7111

<!-- generated:begin DSH7111 -->
**Severity** error

**Message**

```
Invalid boolean value '%s' for %s.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:120`
<!-- generated:end DSH7111 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7112

<!-- generated:begin DSH7112 -->
**Severity** error

**Message**

```
Setting path segment cannot be empty.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:187`
<!-- generated:end DSH7112 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7113

<!-- generated:begin DSH7113 -->
**Severity** error

**Message**

```
Invalid array setting segment '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:200`
<!-- generated:end DSH7113 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7114

<!-- generated:begin DSH7114 -->
**Severity** error

**Message**

```
Invalid array index '%s' in setting segment '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:207`
<!-- generated:end DSH7114 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7115

<!-- generated:begin DSH7115 -->
**Severity** error

**Message**

```
Invalid array setting segment '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:215`
<!-- generated:end DSH7115 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7116

<!-- generated:begin DSH7116 -->
**Severity** error

**Message**

```
Invalid material setting target.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:310`
<!-- generated:end DSH7116 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7117

<!-- generated:begin DSH7117 -->
**Severity** error

**Message**

```
Invalid material setting path '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:316`
<!-- generated:end DSH7117 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7118

<!-- generated:begin DSH7118 -->
**Severity** error

**Message**

```
Unsupported material setting '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:333`
<!-- generated:end DSH7118 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7119

<!-- generated:begin DSH7119 -->
**Severity** error

**Message**

```
Setting '%s' is not an indexed array property.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:340`
<!-- generated:end DSH7119 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7120

<!-- generated:begin DSH7120 -->
**Severity** error

**Message**

```
Array index %d is out of range for setting '%s' (max %d).
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:345`
<!-- generated:end DSH7120 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7121

<!-- generated:begin DSH7121 -->
**Severity** error

**Message**

```
Setting '%s' requires an explicit [index].
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:350`
<!-- generated:end DSH7121 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7122

<!-- generated:begin DSH7122 -->
**Severity** error

**Message**

```
Setting path '%s' cannot continue through '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:366`
<!-- generated:end DSH7122 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7123

<!-- generated:begin DSH7123 -->
**Severity** error

**Message**

```
Invalid material setting path '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:373`
<!-- generated:end DSH7123 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7124

<!-- generated:begin DSH7124 -->
**Severity** error

**Message**

```
Failed to create a transient material for Settings validation.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:381`
<!-- generated:end DSH7124 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7125

<!-- generated:begin DSH7125 -->
**Severity** error

**Message**

```
Invalid value '%s' for setting '%s'. %s
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:394`
<!-- generated:end DSH7125 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7126

<!-- generated:begin DSH7126 -->
**Severity** error

**Message**

```
Invalid value '%s' for setting '%s'. %s
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:412`
<!-- generated:end DSH7126 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7127

<!-- generated:begin DSH7127 -->
**Severity** error

**Message**

```
Unsupported BlendMode/RenderType '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:430`
<!-- generated:end DSH7127 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7128

<!-- generated:begin DSH7128 -->
**Severity** error

**Message**

```
ShadingModel="Substrate" requires Unreal Engine 5.4 or newer.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:441`
<!-- generated:end DSH7128 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7129

<!-- generated:begin DSH7129 -->
**Severity** error

**Message**

```
Unsupported ShadingModel '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:447`
<!-- generated:end DSH7129 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7130

<!-- generated:begin DSH7130 -->
**Severity** error

**Message**

```
Unsupported MaterialDomain '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialSettings.cpp:456`
<!-- generated:end DSH7130 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7131

<!-- generated:begin DSH7131 -->
**Severity** error

**Message**

```
Invalid reflected property target.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:75`
<!-- generated:end DSH7131 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7132

<!-- generated:begin DSH7132 -->
**Severity** error

**Message**

```
'%s' is not a valid boolean value for '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:103`
<!-- generated:end DSH7132 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7133

<!-- generated:begin DSH7133 -->
**Severity** error

**Message**

```
'%s' is not a valid integer value for '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:114`
<!-- generated:end DSH7133 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7134

<!-- generated:begin DSH7134 -->
**Severity** error

**Message**

```
'%s' is not a valid unsigned integer value for '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:125`
<!-- generated:end DSH7134 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7135

<!-- generated:begin DSH7135 -->
**Severity** error

**Message**

```
'%s' is not a valid numeric value for '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:136`
<!-- generated:end DSH7135 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7136

<!-- generated:begin DSH7136 -->
**Severity** error

**Message**

```
'%s' is not a valid numeric value for '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:147`
<!-- generated:end DSH7136 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7137

<!-- generated:begin DSH7137 -->
**Severity** error

**Message**

```
Object property '%s' expects Path(...) or an absolute Unreal object path.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:169`
<!-- generated:end DSH7137 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7138

<!-- generated:begin DSH7138 -->
**Severity** error

**Message**

```
Failed to load asset '%s' for '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:184`
<!-- generated:end DSH7138 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7139

<!-- generated:begin DSH7139 -->
**Severity** error

**Message**

```
Asset '%s' is not compatible with '%s'. Expected '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:189`
<!-- generated:end DSH7139 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7140

<!-- generated:begin DSH7140 -->
**Severity** error

**Message**

```
'%s' is not a valid enum value for '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:203`
<!-- generated:end DSH7140 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7141

<!-- generated:begin DSH7141 -->
**Severity** error

**Message**

```
'%s' is not a valid enum value for '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:218`
<!-- generated:end DSH7141 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7142

<!-- generated:begin DSH7142 -->
**Severity** error

**Message**

```
'%s' is not a valid byte value for '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:228`
<!-- generated:end DSH7142 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7143

<!-- generated:begin DSH7143 -->
**Severity** error

**Message**

```
Property '%s' on '%s' is not a supported literal type yet.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:242`
<!-- generated:end DSH7143 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7144

<!-- generated:begin DSH7144 -->
**Severity** error

**Message**

```
Invalid reflected property target.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:249`
<!-- generated:end DSH7144 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7145

<!-- generated:begin DSH7145 -->
**Severity** error

**Message**

```
%s texture property '%s' expects %s but '%s' is a '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderExpressionFactory.cpp:145`
<!-- generated:end DSH7145 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7146

<!-- generated:begin DSH7146 -->
**Severity** error

**Message**

```
Metadata property '%s' is not a reflected property on '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderExpressionFactory.cpp:232`
<!-- generated:end DSH7146 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7147

<!-- generated:begin DSH7147 -->
**Severity** error

**Message**

```
Metadata property '%s' on '%s': %s
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderExpressionFactory.cpp:238`
<!-- generated:end DSH7147 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7148

<!-- generated:begin DSH7148 -->
**Severity** error

**Message**

```
Invalid parameter expression.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderExpressionFactory.cpp:249`
<!-- generated:end DSH7148 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7149

<!-- generated:begin DSH7149 -->
**Severity** error

**Message**

```
'%s' does not expose a ParameterName property.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderExpressionFactory.cpp:274`
<!-- generated:end DSH7149 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7150

<!-- generated:begin DSH7150 -->
**Severity** error

**Message**

```
'%s' does not expose a texture/asset property for %s.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderExpressionFactory.cpp:324`
<!-- generated:end DSH7150 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7151

<!-- generated:begin DSH7151 -->
**Severity** error

**Message**

```
Failed to create a scalar constant expression.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderExpressionFactory.cpp:695`
<!-- generated:end DSH7151 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7152

<!-- generated:begin DSH7152 -->
**Severity** error

**Message**

```
Unsupported vector literal '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderExpressionFactory.cpp:706`
<!-- generated:end DSH7152 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7153

<!-- generated:begin DSH7153 -->
**Severity** error

**Message**

```
Failed to create a float%d constant expression.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderExpressionFactory.cpp:712`
<!-- generated:end DSH7153 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7154

<!-- generated:begin DSH7154 -->
**Severity** error

**Message**

```
'%s' is not a valid property reference or literal input.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderExpressionFactory.cpp:717`
<!-- generated:end DSH7154 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7200

<!-- generated:begin DSH7200 -->
**Severity** error

**Message**

```
'{0}' is set twice by '#pragma material'; it was already set on line {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:428`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:476`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:505`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1169`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:497`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:412`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:285`, `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:585`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:696`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:723`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:735`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:744`, `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:552`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:380`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:766`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:187`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:197`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:220`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:255`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:755`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:823`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:136`
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
'@param {0}' does not name a parameter of '{1}'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:904`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:344`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:150`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:304`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1122`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:120`
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

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:530`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:550`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:591`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:603`, `Source/DreamShaderLang/Private/Semantic/LangBinderDirectives.cpp:614`
<!-- generated:end DSH7230 -->

**Cause.** A `#pragma layout(...)` line could not be read: no `Node`/`Comment` selector, an unknown
key, or a coordinate that is not a whole number. The line is ignored and nothing else is affected.

**Fix.** Layout is written by the decompiler and is rarely edited by hand; the shape is
`#pragma layout(Node, Var = UV, X = -1100, Y = -120)` and
`#pragma layout(Comment, Text = "Sampling", X = …, Y = …, Width = …, Height = …)`. A node with no
layout entry is simply placed by the layout pass.

