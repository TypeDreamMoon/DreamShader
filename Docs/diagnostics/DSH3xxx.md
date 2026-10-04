# DSH3xxx --- Sections and declarations

> The block between the generated markers is written by `.skill/gen-diagnostics.ps1`.
> Everything below a marker is written by hand and survives a regeneration.

## DSH3200

<!-- generated:begin DSH3200 -->
**Severity** error

**Message**

```
Unexpected {0} at file scope; expected a declaration, '#pragma', '#include' or 'import'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1384`
<!-- generated:end DSH3200 -->

**Cause.** The parser reached a token at file scope that cannot begin anything: a stray `)`, `}`,
operator, number or string outside any declaration. Typical sources are an extra closing brace
after a function, a statement written at file scope (`x = 1;` needs a type in front of it), or a
1.x block that lost its keyword.

**Fix.** Delete the stray token or turn the line into a declaration. Recovery skips to the next
`;`, matched `}` or line that starts a declaration, so the declarations after it still parse; fix
the first DSH3200 and recompile before chasing the ones that follow, they are usually the same
mistake seen from further down.

## DSH3201

<!-- generated:begin DSH3201 -->
**Severity** error

**Message**

```
Preprocessor directive '#{0}' reached the parser; only '#pragma' and '#include' belong here, and '#if' / '#define' lines must be resolved by the preprocessor first.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:648`
<!-- generated:end DSH3201 -->

**Cause.** A `#` line other than `#pragma` or `#include` (`#if`, `#define`, `#endif`, `#error`…)
arrived at the parser. The 2.0 parser expects preprocessed text: `#if` and `#define` are resolved
by the DreamShader preprocessor before parsing, line count preserved, so seeing one here means the
text was parsed raw or a directive is misspelt (`#pragam`).

**Fix.** Inside a compile this cannot happen; from a tool that calls `ParseDreamShaderLang`
directly, run the preprocessor first. If the directive is a typo, correct it. Inside a `/// @custom`
body every `#` line is kept verbatim and never reaches this check.

## DSH3202

<!-- generated:begin DSH3202 -->
**Severity** error

**Message**

```
'#pragma' needs a name: material, instance, pipeline, layout, region or endregion.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:668`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:717`
<!-- generated:end DSH3202 -->

**Cause.** A `#pragma` line is not in the shape the parser reads:

- no name at all (`#pragma` alone, or `#pragma (`),
- `#pragma material` / `#pragma layout` with an argument list that is not `(Key = Value, ...)`
  (missing `)`, a bare `=`, an unterminated string, a value that is not an identifier, number or
  quoted string),
- `#pragma material` with a positional argument: material settings are keyed, only `layout` takes
  a leading positional kind (`#pragma layout(Node, ...)`).

**Fix.** Write `#pragma material(ShadingModel = Unlit, BlendMode = Additive)`,
`#pragma layout(Node, Var = UV, X = -1100, Y = -120)`, `#pragma region Title` or
`#pragma endregion`. A `//` comment at the end of the line is fine and is stripped. An unknown
pragma name (`#pragma once`) is not an error: it is kept as `EPragmaKind::Unknown` with its text
and ignored by the compiler.

## DSH3203

<!-- generated:begin DSH3203 -->
**Severity** error

**Message**

```
'#include' needs a quoted path: #include "/Game/Shared/Common.dsh".
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:627`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:807`
<!-- generated:end DSH3203 -->

**Cause.** `#include` was not followed by a `"path"` in double quotes, or `import` was not followed
by a quoted string. Unquoted paths (`#include /Game/X.dsh`), paths with the quotes on one side
only, and the HLSL system-include spelling `#include <path>` are the usual cases. Angle brackets
are not accepted: DreamShader has no system include directory, so the spelling would mean nothing
and the printer would have to invent one.

**Fix.** Quote the path: `#include "/Game/Shared/Common.dsh"` or `import "Hash.dsh";`. `import`
needs the trailing `;`; `#include` must not have one. Both spellings produce the same
`FIncludeDecl`; only `bImportSpelling` differs, so the printer can write the file back as it was.

## DSH3204

<!-- generated:begin DSH3204 -->
**Severity** error

**Message**

```
Expected a type name, found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:415`
<!-- generated:end DSH3204 -->

**Cause.** A type name was expected and something else was found: `uniform = 1;` (type missing),
`float3 = ...` after a prefix keyword that ate the only identifier, `out x` in a parameter list, or
a keyword used as a type (`struct` inside a parameter list).

**Fix.** Put the type in: `uniform float Intensity = 1;`. In 2.0 every declaration is
`[uniform|static|const|extern|export] Type Name`, there is no untyped `var`.

## DSH3205

<!-- generated:begin DSH3205 -->
**Severity** error

**Message**

```
Expected a name after 'struct', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1120`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1283`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:842`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:877`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:936`
<!-- generated:end DSH3205 -->

**Cause.** A name was expected and the token there is not an identifier. Raised for the name of a
variable or function (`float ;`), a struct (`struct {`), a field, or a parameter (`float f(float)`
— parameters must be named even when unused).

**Fix.** Add the name. A parameter you do not use still needs one; it is what `@param` and the
generated material-function input pin are called.

## DSH3206

<!-- generated:begin DSH3206 -->
**Severity** error

**Message**

```
Expected '`{' or ';' after the parameter list of '{0}', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1210`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:980`
<!-- generated:end DSH3206 -->

**Cause.** After a function's parameter list the parser found neither `{` nor `;`. Usually a stray
token between `)` and `{` (`float f() const {`, `float f() 5`), or a missing `{`.

**Fix.** Follow the parameter list directly with the body, or with `;` if the function is
`extern`. 2.0 has no qualifiers between the parameter list and the body.

## DSH3207

<!-- generated:begin DSH3207 -->
**Severity** error

**Message**

```
'{0}' is 'extern' and binds to an existing asset, so it cannot have a body; write a prototype ending in ';'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1164`
<!-- generated:end DSH3207 -->

**Cause.** An `extern` function has a body. `extern` means "bind this name to an existing
material-function asset" (through `/// @asset` or the library lookup), so the body would have
nowhere to go.

**Fix.** Either drop `extern` and keep the body (the function is then compiled from source), or
drop the body and end the prototype with `;`. The body is skipped as a whole during recovery, so
the declarations after it still parse.

## DSH3208

<!-- generated:begin DSH3208 -->
**Severity** error

**Message**

```
'{0}' has no body. Only an 'extern' prototype may end in ';'; a function you define needs '`{...`}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1201`
<!-- generated:end DSH3208 -->

**Cause.** A function that is not `extern` ends in `;` instead of a body. A 2.0 source file does
not have forward declarations: helpers may be defined in any order and are resolved by name
across the file and its includes.

**Fix.** Give the function its body, or mark it `extern` if it names an existing asset. Delete the
prototype if it was a forward declaration; it is not needed.

## DSH3210

<!-- generated:begin DSH3210 -->
**Severity** error

**Message**

```
A '.dsh' header cannot export '{0}'; only a '.dss' file produces assets.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParser.cpp:74`
<!-- generated:end DSH3210 -->

**Cause.** A `.dsh` header contains an `export` function. Headers are included into materials and
functions and are never compiled on their own, so an `export` in one has no asset to become and
would be duplicated into every includer.

**Fix.** Remove `export` (the function becomes a plain helper visible to every file that includes
the header), or move it into a `.dss` file of its own. Raised once per exported declaration, from
the module loop in `LangParser.cpp`; the declaration is kept in the tree so tooling still sees it.

## DSH3211

<!-- generated:begin DSH3211 -->
**Severity** error

**Message**

```
Expected the end of the expression, found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParser.cpp:572`
<!-- generated:end DSH3211 -->

**Cause.** `ParseDreamShaderLangExpression` (tests, the language service, `#pragma` values) was
given text that has something after a complete expression: `a + b c`, `x = 1;` (the `;` is not part
of an expression).

**Fix.** Pass exactly one expression. From the editor this only appears for a `#pragma` argument
that is meant to be an expression and is not.

## DSH3213

<!-- generated:begin DSH3213 -->
**Severity** error

**Message**

```
'{0}' cannot be combined with the keywords before it; a declaration is 'uniform', 'static const', 'static', 'const', 'extern' or 'export', not a mix.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1070`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1138`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1224`
<!-- generated:end DSH3213 -->

**Cause.** The declaration's prefix keywords do not go together, or go with the wrong kind of
declaration:

- a repeated or conflicting prefix: `uniform static`, `const uniform`, `export extern`,
  `uniform uniform`;
- a storage class on a function: `uniform float f()` — `uniform`, `static` and `const` describe
  variables;
- a linkage on a variable: `export float x`, `extern float3 Dir` — `extern` and `export` describe
  functions.

**Fix.** Use one of the accepted shapes: `uniform T x`, `static const T x`, `static T x`,
`const T x`, `T x` for variables; `T f()`, `export T f()`, `extern T f();` for functions. A
constant shared by several materials is `static const`; a value the material instance edits is
`uniform`.

## DSH3214

<!-- generated:begin DSH3214 -->
**Severity** error

**Message**

```
Parameter '{0}' is 'out' and cannot have a default value; only inputs are optional.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:955`
<!-- generated:end DSH3214 -->

**Cause.** An `out` parameter has a default value: `out float Alpha = 1.0`. An output is written by
the function and cannot be optional for the caller, so the default has no meaning.

**Fix.** Remove the default. If the caller should be able to leave the pin unconnected, make the
parameter `in` and return the value some other way, or give the caller a `uniform` to pass. `inout`
parameters cannot have defaults either.

## DSH3215

<!-- generated:begin DSH3215 -->
**Severity** error

**Message**

```
Expected ']' to close the array dimension, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:453`
<!-- generated:end DSH3215 -->

**Cause.** An array dimension was opened with `[` and not closed: `float Weights[4;`,
`float3 P[;`. The dimension must be a single expression between `[` and `]`.

**Fix.** Close the bracket. Sizes are expressions and may reference `static const` values;
`float x[]` (no size) is accepted by the parser and left for the compiler to reject or infer.

## DSH3216

<!-- generated:begin DSH3216 -->
**Severity** error

**Message**

```
Expected ';' after the import path, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1260`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1307`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:821`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:885`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:894`
<!-- generated:end DSH3216 -->

**Cause.** A declaration is missing its `;`: after a variable and its initializer, after
`import "x.dsh"`, after a struct field, after a struct's closing `}`, or between two declarators in
`uniform float a, b`.

**Fix.** Add the `;`. The parser tells you what it was closing (the message says "after the
declaration", "after the import path", "after the field", "after the closing '}' of the struct").
The most common form is the missing `;` after `struct X { ... }` — HLSL and C both need it.

## DSH3217

<!-- generated:begin DSH3217 -->
**Severity** error

**Message**

```
Expected '{' after the struct name, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:846`
<!-- generated:end DSH3217 -->

**Cause.** `struct Name` was not followed by `{`. Usually a forward declaration (`struct X;`),
which 2.0 does not have, or a base-class colon (`struct X : Y`), which it does not support.

**Fix.** Define the struct in place: `struct X { float a; };`. Types are resolved by name across
the file, so a struct may be declared after the function that uses it.

## DSH3218

<!-- generated:begin DSH3218 -->
**Severity** error

**Message**

```
Expected ')' to close the parameter list, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:969`
<!-- generated:end DSH3218 -->

**Cause.** A parameter list was not closed with `)`. Either the `)` is missing, or a parameter is
separated with something other than `,` (`float a; float b`), or a parameter has a qualifier the
parser does not know (`const float a` — 2.0 parameters take only `in`, `out`, `inout`).

**Fix.** Separate parameters with commas and close the list. Drop `const` from parameters; inputs
are already read-only in the generated graph.

## DSH3220

<!-- generated:begin DSH3220 -->
**Severity** warning

**Message**

```
A '@' in a '///' line must be followed by a directive name; the text is kept as description.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:577`
<!-- generated:end DSH3220 -->

**Cause.** *(warning)* A `///` line has a `@` at a directive position (start of the text or after a
space) that is not followed by a directive name: `/// see @ the docs`, `/// @ desc Foo`,
`/// @123`. The line is kept as description text, so nothing is lost, but if a directive was
intended it was not read.

**Fix.** Write directives as `@name value` with no space after the `@`: `/// @desc Overall gain`.
An `@` inside a word (`user@host`) or followed by a space is plain text and is only warned about
when it sits where a directive would start. Directive keys are lower-cased on read, so `@Desc`
and `@desc` are the same key.

## DSH3221

<!-- generated:begin DSH3221 -->
**Severity** warning

**Message**

```
This '///' block is not followed by a field and is ignored.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1335`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:867`
<!-- generated:end DSH3221 -->

**Cause.** *(warning)* A `///` block is not attached to anything: it sits at the end of the file
with no declaration after it, or at the end of a struct body with no field after it. Doc blocks
belong to the declaration that follows them, so a block with nothing following is dropped.

**Fix.** Move the block above the declaration it describes, or turn it into a `//` comment if it is
just a note. A block above a `#pragma` or `#include` line attaches to that directive and is kept,
so a file-header comment written as `///` above the first `#pragma` is not orphaned.

## DSH3222

<!-- generated:begin DSH3222 -->
**Severity** error

**Message**

```
Expected a 2.0 declaration, found the 1.x declaration '{0}'; 1.x declarations belong in a .dsh header or in a .dsm or .dsf file.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1102`
<!-- generated:end DSH3222 -->

**Cause.** A 1.x declaration word — `Function`, `GraphFunction`, `Namespace`, `VirtualFunction`,
`Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` — was found where a 2.0 declaration
starts. The 2.0 front end does not read the 1.x block syntax: the extension chooses the front end, and
a `.dss` has asked for this one.

**Fix.** Keep 1.x sources in `.dsm` / `.dsf` / `.dsh` files compiled by the 1.x front end, or port
the declaration to 2.0: `Function float Luma(in vec3 c) { ... }` becomes
`float Luma(float3 c) { ... }`, `Shader(Name = "X") { ... }` becomes `export void X(inout material m)
{ ... }` with `#pragma material(...)` for the settings. The message names the word it saw; the
whole block is skipped during recovery so the rest of the file still reports its own errors.

## DSH3250

<!-- generated:begin DSH3250 -->
**Severity** error

**Message**

```
Expected a property type and a name, found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1042`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:719`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:731`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:773`
<!-- generated:end DSH3250 -->

**Cause.** A line of a `Properties` section does not start with a type and a name. Each property is
`<Type> <Name> [= default] [ [metadata] ];`.

**Fix.** Check the line above for a missing `;`, and that the type is one word (`float3`,
`Texture2D`, `ScalarParameter`, ...).

## DSH3251

<!-- generated:begin DSH3251 -->
**Severity** error

**Message**

```
Expected a property type after 'const', found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:674`
<!-- generated:end DSH3251 -->

**Cause.** `const` is not followed by a property type.

**Fix.** Write `const float K = 1.0;` or `const Texture2D Noise = Path(...);`.

## DSH3252

<!-- generated:begin DSH3252 -->
**Severity** error

**Message**

```
Expected a property type such as 'float', 'float4', 'Texture2D' or 'ScalarParameter', found '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:839`
<!-- generated:end DSH3252 -->

**Cause.** The word in type position is not a type a 1.x property can have: not an HLSL value type,
not a texture type, and not one of the parameter node classes 1.x knew by name.

**Fix.** Use a value type (`float` ... `float4`, `vec*`), a texture type, or a parameter class such
as `ScalarParameter`, `VectorParameter`, `TextureObjectParameter`, `StaticSwitchParameter`. A 1.x
property type is matched ignoring case, as 1.x matched it, so it is the word itself that is wrong.

## DSH3253

<!-- generated:begin DSH3253 -->
**Severity** error

**Message**

```
Expected a property type with a 2.0 spelling, found '{0}{1}', which has none; move this material to a .dss file and write the node with UE.Expression.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:851`
<!-- generated:end DSH3253 -->

**Cause.** The property is a node class 1.x could declare and 2.0 has no declaration for, or `const`
stands on a property that cannot be a constant (a parameter node). Both front ends build one AST, so
a property needs a 2.0 form: a `uniform`, a `static const`, or a node expanded where it is read.

**Fix.** Declare it as a plain value or parameter; for any other node class migrate the material to
a `.dss` file and write the node where it is used, as `UE.<Class>(...)`.

## DSH3254

<!-- generated:begin DSH3254 -->
**Severity** error

**Message**

```
Expected a default value after '{0} =', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:753`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:869`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:911`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:935`
<!-- generated:end DSH3254 -->

**Cause.** A property has `=` and nothing the front end can read as a default after it. A default is
a number, a constructor (`float4(1, 0, 0, 1)`), `true` / `false`, an asset reference (`Path(...)`, a
quoted object path, a Content Browser reference), or a `UE.*` node for a builtin property.

**Fix.** Write the default in one of those forms, or remove the `=`.

## DSH3255

<!-- generated:begin DSH3255 -->
**Severity** error

**Message**

```
Expected ']' to close the metadata block, found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:186`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:199`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:253`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:273`
<!-- generated:end DSH3255 -->

**Cause.** A metadata block `[ ... ]` after a property or parameter is not closed, or an entry in it
is not `Key = value;`.

**Fix.** Close the block with `]` and end each entry with `;`.

## DSH3256

<!-- generated:begin DSH3256 -->
**Severity** error

**Message**

```
Expected the metadata key '{0}' once, found it again.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:307`
<!-- generated:end DSH3256 -->

**Cause.** One metadata key is written twice in the same `[ ... ]` block.

**Fix.** Keep one of them.

## DSH3257

<!-- generated:begin DSH3257 -->
**Severity** error

**Message**

```
Expected 'Slider(min, max)' with two numbers, found '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:236`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:299`
<!-- generated:end DSH3257 -->

**Cause.** `Slider` does not have the form `Slider(min, max)` with two numbers, or the range is
given twice: `Slider(...)` together with a `SliderMin` / `SliderMax` style bound in the same
metadata block.

**Fix.** Write the range once, for example `Slider(0.0, 4.0)`.

## DSH3258

<!-- generated:begin DSH3258 -->
**Severity** error

**Message**

```
Expected 'SortPriority' to be a whole number, found '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1797`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:473`
<!-- generated:end DSH3258 -->

**Cause.** `SortPriority` (or its short form `Sort`) is not a whole number. The engine sorts
parameters by an integer.

**Fix.** Write an integer: `SortPriority = 10;`.

## DSH3259

<!-- generated:begin DSH3259 -->
**Severity** error

**Message**

```
Expected ')' to close the arguments of 'UE.{0}', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:703`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:817`
<!-- generated:end DSH3259 -->

**Cause.** A builtin property (`float3 Cam = UE.CameraPositionWS();`) opens the argument list of its
`UE.*` node and never closes it, or gives the node's arguments as a default value after the name
instead of inside the parentheses.

**Fix.** Close the `)`, and write the node's arguments inside it: `float2 UV = UE.TexCoord(Index =
1);`.

## DSH3260

<!-- generated:begin DSH3260 -->
**Severity** error

**Message**

```
Expected a name inside 'Group("...")', found an empty string.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:635`
<!-- generated:end DSH3260 -->

**Cause.** `Group("") { ... }` has an empty name. The members of such a block take the name as their
parameter group.

**Fix.** Give the group a name, or drop the block and let the properties stand ungrouped.

## DSH3261

<!-- generated:begin DSH3261 -->
**Severity** error

**Message**

```
Expected a setting name such as 'BlendMode', found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1110`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1135`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1147`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1161`
<!-- generated:end DSH3261 -->

**Cause.** A line of a `Settings` section does not start with a setting name. Each setting is
`<Name> = <value>;`.

**Fix.** Check the line above for a missing `;`.

## DSH3262

<!-- generated:begin DSH3262 -->
**Severity** warning

**Message**

```
The setting '{0}' is written twice; the later value wins, as it did in 1.x.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1181`
<!-- generated:end DSH3262 -->

**Cause.** One setting is written twice. 1.x kept the later value, and so does this front end.

**Fix.** Remove one of them.

## DSH3263

<!-- generated:begin DSH3263 -->
**Severity** warning

**Message**

```
'{0}' is not a setting of '{1}'; 1.x ignored it and so does this front end.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1147`
<!-- generated:end DSH3263 -->

**Cause.** The setting is not one the block's kind has (a material setting in a ShaderFunction, a
misspelt function setting). 1.x ignored what it did not know, so the source builds as it always did
-- without that setting.

**Fix.** Check the spelling against `Docs/settings`, or remove the line.

## DSH3264

<!-- generated:begin DSH3264 -->
**Severity** warning

**Message**

```
'UserExposedCaption' has no 2.0 spelling and is not applied; its value is kept for migration.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1135`
<!-- generated:end DSH3264 -->

**Cause.** `UserExposedCaption` is set on a function. 2.0 has no directive for it and the compiler
does not write it: a new asset has no caption, and an asset an earlier build captioned keeps the one it
has. The value is kept in the migration record.

**Fix.** Nothing to do for the build. If the caption matters, set it on the asset by hand after
migrating.

## DSH3265

<!-- generated:begin DSH3265 -->
**Severity** warning

**Message**

```
Expected 'true' or 'false' for 'ExposeToLibrary', found '{0}'; 1.x ignored the setting and so does this front end.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1121`
<!-- generated:end DSH3265 -->

**Cause.** `ExposeToLibrary` is neither `true` nor `false`. 1.x ignored such a value and left the
function where it was.

**Fix.** Write `ExposeToLibrary = true;`.

## DSH3266

<!-- generated:begin DSH3266 -->
**Severity** error

**Message**

```
Expected ';' after the output declaration '{0}', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1202`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1322`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1627`
<!-- generated:end DSH3266 -->

**Cause.** An `Outputs` section is not what the front end reads: the section is not opened with `{`,
a declaration has no `;` after it, or a line is neither an output declaration, `Base.<Attribute> =
<source>;`, nor `Expression(...).Pin[<index>] = <source>;`.

**Fix.** End each line with `;` and use one of the three forms.

## DSH3267

<!-- generated:begin DSH3267 -->
**Severity** error

**Message**

```
Expected 'Pin[<index>] = <source>' for an Expression(...) output target, found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1230`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1415`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1425`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1437`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1448`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1464`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1493`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1559`
<!-- generated:end DSH3267 -->

**Cause.** An `Outputs` binding to a custom-output node is not of the form `Expression(Class =
"...").Pin[<index>] = <source>;` (or, in block form, `Pin[<index>] = <source>;` inside the braces).

**Fix.** Name the pin by its index: `Pin[0] = Color;`.

## DSH3268

<!-- generated:begin DSH3268 -->
**Severity** error

**Message**

```
Expected each pin of Expression(Class = "{0}") to be bound once, found Pin[{1}] bound again.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1606`
<!-- generated:end DSH3268 -->

**Cause.** The block form of an `Expression(...)` output target binds one pin twice.

**Fix.** Keep one binding per pin.

## DSH3269

<!-- generated:begin DSH3269 -->
**Severity** error

**Message**

```
Expected at least one 'Pin[<index>] = <source>;' in the Expression(...) block, found none.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1549`
<!-- generated:end DSH3269 -->

**Cause.** The block form of an `Expression(...)` output target has no pin binding in it, so the
node would be built with nothing wired.

**Fix.** Add at least one `Pin[<index>] = <source>;`.

## DSH3270

<!-- generated:begin DSH3270 -->
**Severity** error

**Message**

```
Expected a source after 'Pin[{0}] =', found none.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1246`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1302`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1367`
<!-- generated:end DSH3270 -->

**Cause.** An `=` in an `Outputs` section has nothing after it: `Pin[<index>] =`, `Base.<Attribute>
=`, or the initializer of an output declaration.

**Fix.** Name a variable of the Graph, or a constant, as the source.

## DSH3271

<!-- generated:begin DSH3271 -->
**Severity** error

**Message**

```
Expected a parameter type and name such as 'float Amount', found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1659`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1694`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1730`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1756`
<!-- generated:end DSH3271 -->

**Cause.** A line of an `Inputs` or `Outputs` section of a function is not `[opt] <Type> <Name> [=
default] [ [metadata] ];`.

**Fix.** Check the type spelling and the `;` of the line above.

## DSH3272

<!-- generated:begin DSH3272 -->
**Severity** warning

**Message**

```
'opt' on the output '{0}' means nothing; 1.x ignored it and so does this front end.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1770`
<!-- generated:end DSH3272 -->

**Cause.** `opt` stands on an output. Only an input can be optional; 1.x read the word and did
nothing with it.

**Fix.** Remove it.

## DSH3273

<!-- generated:begin DSH3273 -->
**Severity** warning

**Message**

```
A default on the output '{0}' means nothing; 1.x ignored it, so it is dropped.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1778`
<!-- generated:end DSH3273 -->

**Cause.** An output has a default. An output is what the Graph assigns; 1.x read the default and
dropped it.

**Fix.** Remove the default, or assign the value in the Graph.

## DSH3274

<!-- generated:begin DSH3274 -->
**Severity** error

**Message**

```
Expected 'Node(...)' or 'Comment(...)' in the Layout section, found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1838`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1863`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1884`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1894`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1906`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1922`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1938`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1957`
<!-- generated:end DSH3274 -->

**Cause.** A `Layout` section holds something other than `Node(...)` and `Comment(...)` entries.

**Fix.** Write `Node(Var = "Name", X = 0, Y = 0);` or `Comment(Name = "Title", X = 0, Y = 0, W =
400, H = 200);`.

## DSH3275

<!-- generated:begin DSH3275 -->
**Severity** error

**Message**

```
Expected the argument '{0}' in '{1}(...)', found none.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:1998`, `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:2010`
<!-- generated:end DSH3275 -->

**Cause.** A `Node(...)` or `Comment(...)` entry lacks an argument it cannot do without -- `Var`,
`X`, `Y` for a node; `Name`, `X`, `Y`, `W`, `H` for a comment -- or a position or size is not a
whole number.

**Fix.** Add the argument; positions and sizes are integers in graph units.

## DSH3276

<!-- generated:begin DSH3276 -->
**Severity** error

**Message**

```
Expected 'Color' to be a vector literal such as '(0.1, 0.16, 0.22, 0.35)', found '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacySections.cpp:2036`
<!-- generated:end DSH3276 -->

**Cause.** The `Color` of a `Comment(...)` is not four numbers in parentheses.

**Fix.** Write `Color = (0.1, 0.16, 0.22, 0.35)` -- red, green, blue, alpha.

## DSH3277

<!-- generated:begin DSH3277 -->
**Severity** warning

**Message**

```
The layer input '{0}' becomes the 'inout material' parameter named after the output '{1}', so the input pin changes its name.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1200`
<!-- generated:end DSH3277 -->

**Cause.** A 1.x layer names its MaterialAttributes input one thing and its output another. A 2.0
layer has one `inout material` parameter, which is both pins, and it takes the output's name -- so
the layer asset's input pin is renamed. Inside the body the old input name still reads the incoming
material.

**Fix.** Nothing has to change in the source. A layer stack wires a layer's one input by position,
so existing stacks keep working; a graph that wired the function call by pin name needs reconnecting
once.

## DSH3278

<!-- generated:begin DSH3278 -->
**Severity** error

**Message**

```
Expected a MaterialAttributes output on '{0}', found none.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1170`
<!-- generated:end DSH3278 -->

**Cause.** A `ShaderLayer` or `ShaderLayerBlend` has no output of type `MaterialAttributes`. The
layer stack reads exactly one such output from a layer or a blend.

**Fix.** Declare it in `Outputs`: `MaterialAttributes Result;`.

## DSH3300

<!-- generated:begin DSH3300 -->
**Severity** error

**Message**

```
Expected the buffer's name after 'buffer', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:122`
<!-- generated:end DSH3300 -->

**Cause.** `buffer` starts a declaration of a `.dsp` and no name follows it: the name is missing
(`buffer : R8;`), quoted (`buffer "Mask" : R8;`), or a keyword of the language (`buffer in : R8;`).

**Fix.** Write `buffer <Name> : <Format>;` with a plain identifier: `buffer Mask : R8;`. The name is
what passes bind (case-sensitively), what `r.DreamPass.Visualize <Pipeline>.<Buffer>` takes, and,
for an exported buffer, part of its render target's name, `<Pipeline>_<Buffer>`.

## DSH3301

<!-- generated:begin DSH3301 -->
**Severity** error

**Message**

```
Expected ':' and a format after 'buffer {0}', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:126`
<!-- generated:end DSH3301 -->

**Cause.** A buffer's name is not followed by `:` and its format: the format is missing
(`buffer Mask;`), the `:` is (`buffer Mask R8;`, `buffer Mask = R8;`), the argument list comes
before the format (`buffer Mask(Clear = 0) : R8;`), or two names share one declaration
(`buffer Mask, Edge : R8;`).

**Fix.** Declare each buffer on its own, the format first and the arguments, if any, after it:

```hlsl
buffer Mask : R8(Clear = 0);
buffer Edge : R8;
```

## DSH3302

<!-- generated:begin DSH3302 -->
**Severity** error

**Message**

```
Expected a buffer format such as 'R8' or 'RGBA16F', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:130`
<!-- generated:end DSH3302 -->

**Cause.** The `:` after a buffer's name is not followed by a format word: the format is missing
(`buffer Mask : ;`, `buffer Mask : (Clear = 0);`) or quoted (`buffer Mask : "R8";`). Any name
parses here; one that is not a format (`RGBA16`, `rgba16f`) is the binder's DSH7305.

**Fix.** Write the format without quotes: `R8`, `RG8`, `RGBA8`, `R16F`, `RG16F`, `RGBA16F`, `R32F`,
`RG32F`, `RGBA32F`, or `Depth32` for a mesh pass's own depth. `R32U` and `RG32U` are reserved: no
pass reads or writes an integer buffer yet (DSH7321), and an id fits `R32F` exactly up to 16777216.

## DSH3303

<!-- generated:begin DSH3303 -->
**Severity** error

**Message**

```
Expected the pass's name after 'pass', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:226`
<!-- generated:end DSH3303 -->

**Cause.** `pass` starts a declaration of a `.dsp` and no name follows it: `pass : mesh { ... }`,
`pass "DrawMask" : mesh { ... }`, or a keyword where the name goes.

**Fix.** Name the pass with a plain identifier: `pass DrawMask : mesh { ... }`. GPU captures and
Insights show the pass as `<Pipeline>.<Pass>`, so each name is used once in a pipeline (DSH4401).

## DSH3304

<!-- generated:begin DSH3304 -->
**Severity** error

**Message**

```
Expected ':' and a pass kind after 'pass {0}', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:230`
<!-- generated:end DSH3304 -->

**Cause.** A pass's name is not followed by `:` and its kind: the kind is missing
(`pass Blur { ... }`), the `:` is (`pass Blur compute { ... }`), or the kind is written as an
argument (`pass Blur(compute) { ... }`).

**Fix.** Write `pass <Name> : <kind>` before the block: `pass Blur : compute { ... }`. The kinds are
`fullscreen`, `compute`, `mesh`, `clear` and `copy`.

## DSH3305

<!-- generated:begin DSH3305 -->
**Severity** error

**Message**

```
Expected a pass kind: fullscreen, compute, mesh, clear or copy, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:234`
<!-- generated:end DSH3305 -->

**Cause.** The `:` after a pass's name is not followed by a kind: it is missing
(`pass Blur : { ... }`) or quoted (`pass Blur : "compute" { ... }`). Any name parses here; one that
is not a kind (`Compute`, `postprocess`) is the binder's DSH7304.

**Fix.** Write the kind without quotes, in lower case: `fullscreen` (a Post Process material, or a
`.usf` run as a pixel shader), `compute` (a `.usf` compute shader), `mesh` (selected primitives drawn
again), `clear` or `copy`.

## DSH3310

<!-- generated:begin DSH3310 -->
**Severity** error

**Message**

```
'buffer {0}' declares a buffer of a Custom Pass pipeline, which only a '.dsp' file holds; move it into the pipeline's '.dsp'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:99`
<!-- generated:end DSH3310 -->

**Cause.** A `.dss`, `.dsh` or `.dsi` declares `buffer <Name> : ...` or `pass <Name> : ...` (the
message then says `'pass ...' declares a pass`). Buffers and passes belong to a Custom Pass pipeline,
and `buffer` and `pass` are declaration words in a `.dsp` only; anywhere else they are ordinary names
and this text was always a syntax error -- the message now says where it belongs. A variable merely
called `buffer` or `pass` is fine: it is the `:` after the name that makes this a pipeline
declaration.

**Fix.** Move the declaration into the pipeline's `.dsp`. A material does not declare the buffers it
reads: one a fullscreen pass draws reads them through `UE.UserSceneTexture`, which the pass's `read`
lines bind, and any other material reads an exported buffer with
`UE.DreamPassBuffer(Pipeline = "...", Buffer = "...")`.

## DSH3311

<!-- generated:begin DSH3311 -->
**Severity** error

**Message**

```
'#pragma pipeline' configures a Custom Pass pipeline and belongs in a '.dsp' file of its own, and this line is in '{0}'; move it, with the pipeline's buffers and passes, into a '.dsp'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4813`
<!-- generated:end DSH3311 -->

**Cause.** `#pragma pipeline(...)` stands in a `.dss`, a `.dsh` or a `.dsi` -- the message names the
file. A Custom Pass pipeline is a source file of its own: a `.dsp` holds the pragma with the
pipeline's parameters, buffers and passes, and becomes one `UDreamPassPipeline` named after the file.

**Fix.** Move the pragma, with the buffers and passes it configures, into a `.dsp`; **New Source** in
the material browser offers three pipeline templates to start from. The materials the passes draw
stay in their `.dss` files, with their `#pragma material(...)`.

## DSH3312

<!-- generated:begin DSH3312 -->
**Severity** error

**Message**

```
An 'hlsl' block at file scope is the HLSL of a Custom Pass pipeline, which only a '.dsp' file holds, and '{0}' is not one; a pipeline cannot be included.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4826`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:4850`
<!-- generated:end DSH3312 -->

**Cause.** A `buffer` or `pass` declaration, or a file-level `hlsl` block, reached the binder of a `.dss` or
`.dsh` -- that is, a `.dsp` was included. The parser makes these declarations in a `.dsp` only (elsewhere the text is
DSH3310), so the one way here is a host whose include resolver parses a `.dsp`; the resolver of the
editor and `dsc` refuses to include anything but a `.dsh` or a `.dss` (DSH8295) before this can
happen. It is reported in the `.dsp`, once per buffer and pass, and names the file being compiled.

**Fix.** Remove the include. A pipeline is not shared by inclusion: a material reads its results
through `UE.DreamPassBuffer` (an exported buffer), or through the `read` and `param` lines of the
pass that draws it.

## DSH3313

<!-- generated:begin DSH3313 -->
**Severity** error

**Message**

```
'#pragma pipeline' is written a second time, and one '.dsp' is one pipeline; the line {0} already configures it.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:691`
<!-- generated:end DSH3313 -->

**Cause.** A `.dsp` has a second `#pragma pipeline(...)`. The first one configures the pipeline;
this one is refused whole, and none of its keys are merged into the first.

**Fix.** Merge the keys into one pragma, as in
`#pragma pipeline(Order = 100, Views = Game | Editor | SceneCapture)`. Two pipelines that should run
on their own are two `.dsp` files.

## DSH3314

<!-- generated:begin DSH3314 -->
**Severity** error

**Message**

```
'#pragma material' configures a material, and a '.dsp' is configured by '#pragma pipeline(...)'; the material a pass draws with is a '.dss' of its own.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:704`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:737`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:747`, `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:757`
<!-- generated:end DSH3314 -->

**Cause.** A `.dsp` holds a declaration only a material source can use. A pipeline holds
`#pragma pipeline`, `uniform`, `static const`, `buffer` and `pass` declarations and an `hlsl` block; four other
kinds are this error, each with its own message:

- `#pragma material(...)` -- settings of a material, which belong to the `.dss` of the material a
  pass draws;
- a function outside an `hlsl` block -- an HLSL pass's code is an `hlsl { }` block or its `.usf`, a material's
  is its `.dss`, and a `param` value is folded from the pipeline's parameters;
- a `struct` outside an `hlsl` block;
- an `#include` or `import` outside an `hlsl` block -- nothing in a `.dsp` could use what it declares; an
  `#include` HLSL needs goes inside the block.

**Fix.** Move each where it belongs: material settings into the `#pragma material(...)` of the
material's `.dss`, a function into a `.dss` or `.dsh` (graph code) or an `hlsl` block or the pass's `.usf`
(HLSL), a `struct` into a `.dsh` or an `hlsl` block; remove the include, or move it into an `hlsl` block. The pipeline's own settings are the keys of
`#pragma pipeline(...)`: `Order`, `Injection`, `Views`, `Requires` and `Enabled`.

## DSH3315

<!-- generated:begin DSH3315 -->
**Severity** warning

**Message**

```
'#pragma {0}' boxes or places graph nodes, and a '.dsp' has no graph; the line was ignored.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:718`
<!-- generated:end DSH3315 -->

**Cause.** `#pragma layout`, `#pragma region` or `#pragma endregion` stands in a `.dsp`. In a `.dss`
these place generated nodes and box them in comments; a pipeline generates no graph, so the line is
ignored and the pipeline builds as if it were not there.

**Fix.** Delete it. To group the declarations of a long `.dsp`, use `//` comments, which survive
`dsc fmt` and Adopt; to group parameters in the details panel, use `/// @group` on the `uniform`s.

## DSH3316

<!-- generated:begin DSH3316 -->
**Severity** error

**Message**

```
'{0}' is a file-scope variable of a '.dsp', which is a 'uniform' (a parameter an activation may override) or a 'static const' (a compile-time value).
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:822`
<!-- generated:end DSH3316 -->

**Cause.** A file-scope variable of a `.dsp` is declared without `uniform` or `static const`:
`float Radius = 2.0;`, `static float Scale = 0.5;`. A pipeline has two kinds of value: parameters,
which an activation -- a volume, a component, the project settings, Blueprint -- may override, and
constants, folded where they are used (a plain `const` is taken as one). A plain variable is neither.
The declaration is dropped, so a `param` or `Enabled` that names it is DSH4404 as well.

**Fix.** Write `uniform` when an activation should be able to change the value, `static const` when
it is fixed:

```hlsl
/// @group Blur   @slider 1 8
uniform float BlurRadius = 3.0;
static const float HalfRes = 0.5;
```

## DSH3317

<!-- generated:begin DSH3317 -->
**Severity** warning

**Message**

```
'@{0}' has no effect on {1} in a '.dsp'; remove it.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderPipeline.cpp:789`
<!-- generated:end DSH3317 -->

**Cause.** A `///` directive above a declaration of a `.dsp` means nothing to a pipeline. What each
declaration takes:

| Above | Directives |
| :-- | :-- |
| a `uniform` | `@group`, `@desc`, `@slider`, `@sort`; `@default` on a `Texture2D` |
| a `static const`, a `buffer`, a `pass` | `@desc` |
| `#pragma pipeline` | none; its `///` block documents the file |

On a `uniform` or `static const` the warning is for `@name`, `@sampler`, `@static` and `@page`, which
mean something for the parameter of a material or an instance, and for keys the language does not
define, which a `.dss` hands to the parameter node by their engine name -- a pipeline parameter is no
node. The language's other directives in the wrong place are DSH7224, in any file.

**Fix.** Remove the directive. Plain text in a `///` block is never warned about: above a parameter,
a buffer or a pass it is the description, as `@desc` would be.

