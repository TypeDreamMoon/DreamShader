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

