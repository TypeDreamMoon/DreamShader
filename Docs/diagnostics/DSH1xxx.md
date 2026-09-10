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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:889`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:897`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:904`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:911`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:954`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:990`
<!-- generated:end DSH1015 -->

**Cause.** an unrecognized root in a texture default

**Fix.** use a supported root

**See** [Path](../parameters/path.md)

## DSH1016

<!-- generated:begin DSH1016 -->
**Severity** error

**Message**

```
Texture defaults must use Path(Game|Engine|Plugin.PluginName, "Folder/Asset"), Path("/Game/Folder/Asset"), a bare "/Game/Folder/Asset", or a Class'/Game/Folder/Asset.Asset' reference.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:1092`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:1129`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:1153`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:1186`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:1202`
<!-- generated:end DSH1020 -->

**Cause.** the reference parsed as a `Path(...)` but `FPackageName::IsValidObjectPath` rejected the
result. The message is the engine's own wording rather than DreamShader's, so it varies: an unmounted
root, an illegal character, or a package name that resolves to no object.

**Fix.** check the path against the Content Browser — *Copy Reference* on the asset always yields a
form that validates.

**See** [Path](../parameters/path.md)

## DSH1030

<!-- generated:begin DSH1030 -->
**Severity** error

**Message**

```
{0}({1}): this '#if' is never closed; the file ends with {2} conditional block(s) still open.
```

**Raised by** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:1154`
<!-- generated:end DSH1030 -->

**Cause.** a `#if` / `#ifdef` / `#ifndef` chain was opened and the file ended before its `#endif`; the count is how many chains are still open. Directives inside a `Function` or `GraphFunction` body are not counted -- those belong to the HLSL compiler -- so a chain cannot be closed from inside a body

**Fix.** add the missing `#endif` at the point where the conditional region should end; if the `#if` was meant for the shader compiler, move the whole chain inside the function body

**See** [Preprocessor](../language/preprocessor.md)

## DSH1031

<!-- generated:begin DSH1031 -->
**Severity** error

**Message**

```
{0}({1}): '#endif' without a matching '#if'.
```

**Raised by** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:1054`
<!-- generated:end DSH1031 -->

**Cause.** an `#endif` was found while no DreamShader chain was open. The usual cause is an `#if` that sits inside a `Function` body (opaque to this preprocessor) with its `#endif` after the closing brace

**Fix.** delete the stray `#endif`, or move the `#if` out of the body so both ends are on the same side of the brace

**See** [Preprocessor](../language/preprocessor.md)

## DSH1032

<!-- generated:begin DSH1032 -->
**Severity** error

**Message**

```
{0}({1}): '#{2}' without a matching '#if'.
```

**Raised by** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:978`
<!-- generated:end DSH1032 -->

**Cause.** `#elif` or `#else` appeared while no `#if` chain was open

**Fix.** open the chain with `#if` / `#ifdef` / `#ifndef` first, or remove the branch

**See** [Preprocessor](../language/preprocessor.md)

## DSH1033

<!-- generated:begin DSH1033 -->
**Severity** error

**Message**

```
{0}({1}): '#{2}' after the '#else' on line {3}, which already closed this chain.
```

**Raised by** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:993`
<!-- generated:end DSH1033 -->

**Cause.** the chain already took its `#else` on the line shown, and `#else` closes a chain: nothing but `#endif` may follow it

**Fix.** move the extra branch above the `#else`, or close the chain with `#endif` and start a new one

**See** [Preprocessor](../language/preprocessor.md)

## DSH1034

<!-- generated:begin DSH1034 -->
**Severity** error

**Message**

```
{0}({1}): invalid '{2}' condition: {3}
```

**Raised by** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessorExpression.cpp:57`
<!-- generated:end DSH1034 -->

**Cause.** the condition text ended before the expression was complete -- an unclosed parenthesis, a dangling operator (`#if 1 &&`), or an operator with a missing operand; the detail after the colon is the expression parser's own account

**Fix.** finish the expression. Conditions support `defined(NAME)`, `!`, `&&`, `||`, comparisons, `+ - * / %`, parentheses and string equality; a name that is not defined reads `0`

**See** [Preprocessor](../language/preprocessor.md)

## DSH1035

<!-- generated:begin DSH1035 -->
**Severity** error

**Message**

```
{0}({1}): unknown preprocessor directive '#{2}'. {3}
```

**Raised by** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:861`
<!-- generated:end DSH1035 -->

**Cause.** a `#` line outside a function body did not spell one of the eight directives (`#if #ifdef #ifndef #elif #else #endif #define #undef`) or the pass-through pair `#Region` / `#EndRegion`. Directives are case-sensitive, so `#IF` lands here too, and `#include` is not one of them (imports are spelled `import`). This is an error rather than a warning because a directive that was silently skipped -- a typo such as `#endfi` -- would leave every line below it unconditionally compiled

**Fix.** fix the spelling or case; write `import "...";` for an include; if the line is an HLSL directive, it must live inside a `Function` body, where this preprocessor does not look

**See** [Preprocessor](../language/preprocessor.md)

## DSH1036

<!-- generated:begin DSH1036 -->
**Severity** error

**Message**

```
{0}({1}): '#{2}' requires a define name.
```

**Raised by** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:923`, `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessorExpression.cpp:67`
<!-- generated:end DSH1036 -->

**Cause.** `#ifdef`, `#ifndef`, `#define` or `#undef` was written with nothing after it

**Fix.** give the directive a define name

**See** [Preprocessor](../language/preprocessor.md)

## DSH1037

<!-- generated:begin DSH1037 -->
**Severity** error

**Message**

```
{0}({1}): '#{2}' nesting is deeper than the limit of {3}.
```

**Raised by** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:876`
<!-- generated:end DSH1037 -->

**Cause.** conditional chains are nested deeper than the fixed limit shown

**Fix.** flatten the nesting: combine the inner tests into one condition with `&&`, or split the file

**See** [Preprocessor](../language/preprocessor.md)

## DSH1038

<!-- generated:begin DSH1038 -->
**Severity** error

**Message**

```
{0}({1}): '#{2}' needs a name made of letters, digits and underscores and not starting with a digit; got '{3}'.
```

**Raised by** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:1090`, `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:932`
<!-- generated:end DSH1038 -->

**Cause.** the name after the directive is not an identifier: it must be letters, digits and underscores and must not start with a digit

**Fix.** rename the define to match `[A-Za-z_][A-Za-z0-9_]*`

**See** [Preprocessor](../language/preprocessor.md)

## DSH1039

<!-- generated:begin DSH1039 -->
**Severity** error

**Message**

```
{0}({1}): '{3}' is a read-only built-in constant, so '#{2}' cannot change it. The 'DS_' prefix is reserved by DreamShader.
```

**Raised by** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessor.cpp:1104`
<!-- generated:end DSH1039 -->

**Cause.** the `DS_` prefix is reserved for the read-only built-in facts about the compiling process (`DS_ENGINE_MAJOR`, `DS_ENGINE_MINOR`, `DS_ENGINE_PATCH`, `DS_SUBSTRATE`, `DS_PLATFORM`, `DS_PLUGIN_VERSION`, ...). They cannot be defined or undefined from a source file, and the reservation is by prefix so that a built-in added later can never lose to a name a project registered first

**Fix.** use a name without the `DS_` prefix. To try a hypothetical engine or platform, define your own switch (in the project settings or with `dsc -Define`) and branch on that

**See** [Preprocessor](../language/preprocessor.md)

## DSH1040

<!-- generated:begin DSH1040 -->
**Severity** error

**Message**

```
{0}({1}): type mismatch in '{2}' condition: {3}
```

**Raised by** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessorExpression.cpp:76`
<!-- generated:end DSH1040 -->

**Cause.** the two sides of an operator have different types: a value that parses as an integer is a number, anything else is a string, and the two only meet in `==` / `!=`. Arithmetic or ordering on a string, or comparing a string with a number, raises this

**Fix.** compare like with like: give the define a numeric value, or compare two strings with `==` / `!=` only

**See** [Preprocessor](../language/preprocessor.md)

## DSH1041

<!-- generated:begin DSH1041 -->
**Severity** error

**Message**

```
{0}({1}): the right operand of '{2}' in this '{3}' condition is zero.
```

**Raised by** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessorExpression.cpp:86`
<!-- generated:end DSH1041 -->

**Cause.** the divisor of `/` or `%` evaluated to zero. Remember that an undefined name reads `0`, so `#if TOTAL / COUNT` with `COUNT` never defined is the common shape

**Fix.** define the name, or guard the expression with `defined(COUNT) && COUNT != 0 && ...` -- `&&` short-circuits, so the division is never evaluated

**See** [Preprocessor](../language/preprocessor.md)

## DSH1042

<!-- generated:begin DSH1042 -->
**Severity** error

**Message**

```
{0}({1}): '{2}' is already complete before '{3}'. Nothing may follow a directive but a '//' comment.
```

**Raised by** `Source/DreamShaderLang/Private/Preprocessor/DreamShaderPreprocessorExpression.cpp:1097`
<!-- generated:end DSH1042 -->

**Cause.** the directive was complete and more tokens followed it (`#if 1 2`, `#ifdef A B`, `#endif LABEL`). Only a `//` comment may follow a directive; an unfinished expression is [DSH1034](#dsh1034) instead. `#define` is exempt because its value runs to the end of the line

**Fix.** delete the extra tokens, or turn them into a `//` comment

**See** [Preprocessor](../language/preprocessor.md)

## DSH1043

<!-- generated:begin DSH1043 -->
**Severity** error

**Message**

```
Asset reference is written as '{0}', which is not a texture class; a texture default requires {1}.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:1039`
<!-- generated:end DSH1043 -->

**Cause.** a texture default was written as a `Class'/Game/Folder/Asset.Asset'` reference whose class is one DreamShader knows is not a texture -- `MaterialFunction`, `CurveLinearColor`, `Font`, `MaterialParameterCollection` and the like. Copying a reference from the wrong Content Browser row produces exactly this: the path is well formed, the asset exists, and it is simply not something a texture slot can hold. Only classes on DreamShader's short "definitely not a texture" list raise this; a class the plugin has never heard of is stripped and forgotten, never refused

**Fix.** copy the reference again from the texture asset itself, or delete the `Class'` prefix and the quotes and leave the bare object path -- the class in front of the quotes is decoration that Unreal ignores when it resolves the path, so removing it is always safe

**See** [Path](../parameters/path.md)

## DSH1044

<!-- generated:begin DSH1044 -->
**Severity** error

**Message**

```
Asset reference is written as '{0}', but this property is declared as {1}.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:1029`
<!-- generated:end DSH1044 -->

**Cause.** a texture default was written as a `Class'/Game/Folder/Asset.Asset'` reference whose class is a texture of a different dimension than the property declares -- a `Texture2D'...'` assigned to a `VolumeTexture` or `TextureSampleParameterVolume`, for instance. The check only runs when the declared type names its dimension; `TextureObjectParameter` and friends carry no dimension of their own and are not judged this way

**Fix.** if the property type is right, copy the reference from an asset of that dimension; if the asset is right, change the declared type to match it. The dimension DreamShader believes comes from the declaration, never from the pasted prefix

**See** [Path](../parameters/path.md)

## DSH1045

<!-- generated:begin DSH1045 -->
**Severity** error

**Message**

```
Asset reference is written as '%s', but this slot requires a '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderAssetReferenceResolution.cpp:292`, `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderAssetReferenceResolution.cpp:301`
<!-- generated:end DSH1045 -->

**Cause.** an asset reference in a metadata entry, a `UE.CollectionParam` argument or a `VirtualFunction`'s `Options.Asset` was written as a `Class'/Game/Folder/Asset.Asset'` reference whose class is unrelated to the class that slot declares -- a `Texture2D'...'` written into a `Curve` entry, say. The two classes are compared in both directions, so a base class written where a subclass is wanted (`Texture'...'` into a `UTexture2D` property) is accepted and left for the load to settle

**Fix.** copy the reference from an asset of the class the slot wants, or drop the `Class'` prefix and the quotes and leave the bare object path

**See** [Path](../parameters/path.md)

