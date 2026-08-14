# DSH1xxx --- Driver, source files and imports

> The block between the generated markers is written by `.skill/gen-diagnostics.ps1`.
> Everything below a marker is written by hand and survives a regeneration.

## DSH1010

<!-- generated:begin DSH1010 -->
**Severity** error

**Message**

```
Texture Path root '{0}' has an invalid plugin name.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:888`
<!-- generated:end DSH1010 -->

**Cause.** the plugin name contains characters outside `[A-Za-z0-9_]`

**Fix.** use a valid plugin name

**See** [Path](../parameters/path.md)

## DSH1011

<!-- generated:begin DSH1011 -->
**Severity** error

**Message**

```
Texture Path root '{0}' references plugin '{1}', but no enabled plugin with that name was found.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:896`
<!-- generated:end DSH1011 -->

**Cause.** `FindPlugin` returned nothing

**Fix.** check the plugin name

**See** [Path](../parameters/path.md)

## DSH1012

<!-- generated:begin DSH1012 -->
**Severity** error

**Message**

```
Texture Path root '{0}' references plugin '{1}', but the plugin is not enabled.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:903`
<!-- generated:end DSH1012 -->

**Cause.** `IsEnabled()` is false

**Fix.** enable the plugin

**See** [Path](../parameters/path.md)

## DSH1013

<!-- generated:begin DSH1013 -->
**Severity** error

**Message**

```
Texture Path root '{0}' references plugin '{1}', but the plugin cannot contain content.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:910`
<!-- generated:end DSH1013 -->

**Cause.** `CanContainContent()` is false

**Fix.** enable content in the plugin descriptor

**See** [Path](../parameters/path.md)

## DSH1014

<!-- generated:begin DSH1014 -->
**Severity** error

**Message**

```
Relative texture Path(...) references require a root such as Game, Engine, or Plugin.PluginName.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:953`
<!-- generated:end DSH1014 -->

**Cause.** a relative texture default with no root

**Fix.** add a root

**See** [Path](../parameters/path.md)

## DSH1015

<!-- generated:begin DSH1015 -->
**Severity** error

**Message**

```
Unsupported texture Path root '{0}'. Use Game, Engine, or Plugin.PluginName.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:989`
<!-- generated:end DSH1015 -->

**Cause.** an unrecognized root in a texture default

**Fix.** use a supported root

**See** [Path](../parameters/path.md)

## DSH1016

<!-- generated:begin DSH1016 -->
**Severity** error

**Message**

```
Texture defaults must use Path(Game|Engine|Plugin.PluginName, \"/Folder/Asset\"), Path(\"/Game/Folder/Asset\"), or a bare \"/Game/Folder/Asset\".
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:1023`
<!-- generated:end DSH1016 -->

**Cause.** the default is not one of the three accepted forms

**Fix.** use one of them

**See** [Path](../parameters/path.md)

## DSH1017

<!-- generated:begin DSH1017 -->
**Severity** error

**Message**

```
Unexpected trailing tokens after texture Path(...) reference.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:1060`
<!-- generated:end DSH1017 -->

**Cause.** text after the closing `)`

**Fix.** end the value at `)`

**See** [Path](../parameters/path.md)

## DSH1018

<!-- generated:begin DSH1018 -->
**Severity** error

**Message**

```
Texture Path(...) requires a non-empty asset path.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:1069`
<!-- generated:end DSH1018 -->

**Cause.** `Path( … )` with an empty path

**Fix.** supply a path

**See** [Path](../parameters/path.md)

## DSH1019

<!-- generated:begin DSH1019 -->
**Severity** error

**Message**

```
Invalid texture asset path '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:1098`
<!-- generated:end DSH1019 -->

**Cause.** the resolved texture path is not a valid object path

**Fix.** fix the path

**See** [Path](../parameters/path.md)

## DSH1020

<!-- generated:begin DSH1020 -->
**Severity** error

**Message**

```
(built at runtime)
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:1114`
<!-- generated:end DSH1020 -->

**Cause.** the reference parsed as a `Path(...)` but `FPackageName::IsValidObjectPath` rejected the
result. The message is the engine's own wording rather than DreamShader's, so it varies: an unmounted
root, an illegal character, or a package name that resolves to no object.

**Fix.** check the path against the Content Browser — *Copy Reference* on the asset always yields a
form that validates.

**See** [Path](../parameters/path.md)

