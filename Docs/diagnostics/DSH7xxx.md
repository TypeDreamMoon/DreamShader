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
Unsupported UE builtin function '{0}'. Use OutputType=\\\"float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture\\\" for generic MaterialExpression calls.
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
Parameter node type '{0}' is recognized but not supported as a plain Properties declaration yet. Use UE.{1}(OutputType=\\\"float4\\\", ...) for reflected node creation.
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
StaticSwitchParameter '%s' cannot switch Substrate values.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:211`
<!-- generated:end DSH7103 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

## DSH7104

<!-- generated:begin DSH7104 -->
**Severity** error

**Message**

```
StaticSwitchParameter '%s' cannot mix MaterialAttributes and numeric branches.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:215`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:219`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:236`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:318`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:325`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:345`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeProperties.cpp:355`
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
ShadingModel=\"Substrate\" requires Unreal Engine 5.4 or newer.
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:97`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:108`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:119`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:130`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:141`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:163`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:178`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:183`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:197`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:212`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:222`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:236`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialLiteralPropertyWriter.cpp:243`
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

