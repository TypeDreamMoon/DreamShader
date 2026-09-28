# DSH5xxx --- Builtins -- UE.*, math, Substrate

> The block between the generated markers is written by `.skill/gen-diagnostics.ps1`.
> Everything below a marker is written by hand and survives a regeneration.

## DSH5200

<!-- generated:begin DSH5200 -->
**Severity** error

**Message**

```
A material has no '{0}' pin; did you mean '{1}'? Attribute names are case-sensitive.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1071`, `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1082`
<!-- generated:end DSH5200 -->

**Cause.** `m.<Name>` names no material pin. The table comes from the engine, so the message can
suggest a spelling that differs only in case — the usual cause, since 1.x matched these
case-insensitively and 2.0 does not.

**Fix.** Use the exact spelling. `EmissiveColor`, `BaseColor`, `Roughness`, `Normal`,
`WorldPositionOffset`, `FrontMaterial` and the rest keep their engine names; the 1.x short forms
(`Emissive`, `AO`, `WPO`, `PDO`) are carried as aliases in the catalog.

## DSH5201

<!-- generated:begin DSH5201 -->
**Severity** error

**Message**

```
This Custom node declares no output called '{0}'; its outputs are '{1}'. An output is declared by 'AdditionalOutputs'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1164`, `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1218`, `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1424`, `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1825`
<!-- generated:end DSH5201 -->

**Cause.** Three shapes, one code — all say "this node's outputs need naming". The message lists
the outputs the class has.

- `.Name` on a multi-output node names no output of that class.
- A multi-output node was used as a value without naming an output, and it is not the value the
  place wants. A node takes its **default** (first) output where that output's type is exactly the
  type wanted. A node whose outputs are all **channel views** of one value — `UE.VertexColor()`'s
  `RGB`, `R`, `G`, `B`, `A`, all cut from one `float4` — also stands for that whole value where
  exactly its type is wanted, so `float4 VC = UE.VertexColor();` binds as it did in 1.x (when the
  default output leads the value, as `RGB` does). A node whose outputs are different values —
  `UE.SceneTexture(...)`'s `Color`, `Size`, `InvSize` — has no whole to stand for: anything but its
  default output's exact type has to name an output.
- A swizzle on a node of channel views asked for channels that sit on different outputs
  (`UE.VertexColor().ga`: `G` and `A` are separate pins). A swizzle that one view publishes exactly
  is that view (`.a` is `A`), and a swizzle inside the default output is a swizzle of it (`.rg`);
  across views there is no one pin to read.

**Fix.** Name one of the listed outputs — `UE.SceneTexture(SceneTextureId = PostProcessInput0).Color`,
`UE.VertexColor().A` — or read the channels one output at a time and combine them
(`float2(UE.VertexColor().g, UE.VertexColor().a)`; two identical calls are one node).
`dsc export-catalog` writes out every class with its pins and its outputs; an unnamed masked output
is listed under the channels it keeps.

## DSH5202

<!-- generated:begin DSH5202 -->
**Severity** warning

**Message**

```
The builtin catalog is empty, so no 'UE.' expression and no material attribute can be resolved; export it with 'dsc export-catalog'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:280`
<!-- generated:end DSH5202 -->

**Cause.** The binder was given an empty builtin catalog, so no `UE.` node and no material pin can
be resolved and everything that names one will also report. This is normal for a tool running
outside the editor; inside a compile it means the catalog was not loaded.

**Fix.** Export the catalog with `dsc export-catalog` and point the tool at it. Nothing else in the
file is affected — the symbol index is still produced.

## DSH5210

<!-- generated:begin DSH5210 -->
**Severity** error

**Message**

```
'{0}.{1}' is not a node; did you mean '{0}.{2}'? Node names are case-sensitive.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4220`, `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4232`
<!-- generated:end DSH5210 -->

**Cause.** `UE.X` / `Substrate.X` names no class this engine has. Node names are **case-sensitive**;
a case-only match is suggested.

**Fix.** Correct the spelling, or reach the class by its engine name with
`UE.Expression(Class = "MaterialExpressionX")`. `Substrate.` only resolves the Substrate table.

## DSH5211

<!-- generated:begin DSH5211 -->
**Severity** error

**Message**

```
'{0}.{1}' is a node and has to be called: write '{0}.{1}(...)'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1014`
<!-- generated:end DSH5211 -->

**Cause.** `UE.X` was written without an argument list. A node is made by calling it, even when it
takes nothing.

**Fix.** Add the parentheses: `UE.Time()`.

## DSH5212

<!-- generated:begin DSH5212 -->
**Severity** error

**Message**

```
'{0}' is not a material expression class this engine has.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4169`
<!-- generated:end DSH5212 -->

**Cause.** The `Class` argument does not name a material expression class. The class name is
accepted with or without its `MaterialExpression` prefix and as a full `/Script/Engine.…` path.

**Fix.** Check the spelling against the exported catalog.

## DSH5213

<!-- generated:begin DSH5213 -->
**Severity** error

**Message**

```
'{0}.{1}' has no pin or property called '{2}'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4646`
<!-- generated:end DSH5213 -->

**Cause.** A named argument to a reflected node matches neither an input pin nor a reflected
property of that class. Pins are tried first, then properties; a case-only match on either is
suggested.

**Fix.** Correct the name. A literal that should be written into the node rather than connected to
it usually has a `Const`-prefixed property name — `ConstA` beside the `A` pin.

## DSH5214

<!-- generated:begin DSH5214 -->
**Severity** error

**Message**

```
{0} expects {1}, and this is {2}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1932`
<!-- generated:end DSH5214 -->

**Cause.** A value does not fit the pin it feeds: the wrong width, or a kind the pin does not take
(a number into a MaterialAttributes pin, a texture into a numeric one). Also raised for the
coordinates and the mip level of a texture sample.

**Fix.** Match the pin's type — the message names it. A pin that does not constrain its width takes
whatever arrives.

## DSH5215

<!-- generated:begin DSH5215 -->
**Severity** error

**Message**

```
'{0}' is not a value of '{1}' on '{2}'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4031`
<!-- generated:end DSH5215 -->

**Cause.** The value of an enumerated property is not one of that enum's values. A case-only match
is suggested.

**Fix.** Use one of the listed spellings, written without the enum's prefix
(`SamplerType = Normal`, not `SAMPLERTYPE_Normal`).

## DSH5216

<!-- generated:begin DSH5216 -->
**Severity** error

**Message**

```
'Substrate.' already names the node, so it takes no 'Class' argument.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4141`
<!-- generated:end DSH5216 -->

**Cause.** A `Class` argument was given to a `Substrate.` call. The `Substrate.` prefix already
names the class.

**Fix.** Drop the `Class` argument, or write the call as `UE.Expression(Class = "…")` instead.

## DSH5217

<!-- generated:begin DSH5217 -->
**Severity** error

**Message**

```
'Class' takes the expression class as a quoted string.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4149`
<!-- generated:end DSH5217 -->

**Cause.** The `Class` argument is not a literal the binder can read. It accepts a quoted string, a
bare identifier and a dotted name.

**Fix.** Write `Class = "MaterialExpressionConstant3Vector"`.

## DSH5218

<!-- generated:begin DSH5218 -->
**Severity** error

**Message**

```
'UE.Expression' reaches a node this language has no name for, so it needs 'Class = "MaterialExpressionName"'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4182`
<!-- generated:end DSH5218 -->

**Cause.** `UE.Expression(...)` without `Class`. `Expression` is the escape hatch for a node the
language has no name for, so the class is the one thing it cannot infer.

**Fix.** Add `Class = "MaterialExpressionName"`, or call the node by its short name.

## DSH5219

<!-- generated:begin DSH5219 -->
**Severity** warning

**Message**

```
'{0}.{1}' leaves its required '{2}' pin unconnected; unless the node reads a default for it, the engine reports it when the material compiles.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4715`, `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:416`
<!-- generated:end DSH5219 -->

**Cause.** A pin the engine draws as required was left unconnected, and its literal twin was not set
either. It is a warning, not an error: `required` only means the property has no `RequiredInput =
"false"`. Most nodes do fail to compile with such a pin open, but some read a default instead
(MakeMaterialAttributes, the per-level inputs of a switch, nodes of an engine fork that never set
the metadata), and only the material compile knows. A class whose nodes show pins per setting has no
statically required pin at all.

**Fix.** Connect the pin, or set its `Const...` property to a literal. If the node is meant to run
with the pin open, nothing has to change; `dsc check -Shaders` compiles the material and reports a
real `missing input`.

## DSH5220

<!-- generated:begin DSH5220 -->
**Severity** error

**Message**

```
'{0}.{1}' takes named arguments: write 'Pin = value'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4348`
<!-- generated:end DSH5220 -->

**Cause.** A positional argument was given to a node that has no canonical argument order. Most
reflected classes are named-only — the pin order an engine class happens to have is not a contract.

**Fix.** Name the argument: `Pin = value`.

## DSH5221

<!-- generated:begin DSH5221 -->
**Severity** error

**Message**

```
'{0}.{1}' takes {2} arguments in order; name the rest.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4365`
<!-- generated:end DSH5221 -->

**Cause.** More positional arguments than the node's canonical order defines.

**Fix.** Name the remaining ones.

## DSH5223

<!-- generated:begin DSH5223 -->
**Severity** error

**Message**

```
'{0}' is abstract and cannot be made into a node.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4260`
<!-- generated:end DSH5223 -->

**Cause.** The class is abstract and cannot be instantiated.

**Fix.** Use one of its concrete subclasses.

## DSH5224

<!-- generated:begin DSH5224 -->
**Severity** error

**Message**

```
'{0}' is an enumerated property; write one of its values, as in 'SamplerType = Normal'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3991`, `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4062`, `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4104`
<!-- generated:end DSH5224 -->

**Cause.** A literal property was given something that is not a literal: an expression the binder
cannot fold, or a word where a number was wanted. A property is written into the node when the
graph is built, so its value has to exist at that moment.

**Fix.** Write a literal, or a `static const` expression that folds to one. A value that has to be
computed belongs on a pin, not in a property.

## DSH5230

<!-- generated:begin DSH5230 -->
**Severity** error

**Message**

```
The first argument of a texture sample is the texture, and this is {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4971`
<!-- generated:end DSH5230 -->

**Cause.** The first argument of `Texture2DSample` is not a texture.

**Fix.** Pass the texture — a `uniform Texture2D`, or a texture parameter of the function.

## DSH5231

<!-- generated:begin DSH5231 -->
**Severity** error

**Message**

```
The second argument of 'Texture2DSample' is the sampler, and this is {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4987`
<!-- generated:end DSH5231 -->

**Cause.** The second argument of `Texture2DSample` is not a sampler.

**Fix.** Pass `<TextureName>Sampler`, the sampler the translator pairs with the texture, or use the
shorter `Tex.Sample(UV)`, which takes it from the texture itself.

## DSH5250

<!-- generated:begin DSH5250 -->
**Severity** error

**Message**

```
Expected 'OutputIndex' to be a whole number of zero or more, found '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1009`
<!-- generated:end DSH5250 -->

**Cause.** `OutputIndex` selects an output of a 1.x call by position, and its value is not a whole
number from 0 up.

**Fix.** Write `OutputIndex = 1`, or name the output: `Output = "Alpha"`.

## DSH5251

<!-- generated:begin DSH5251 -->
**Severity** error

**Message**

```
Expected 'Output' to name an output with a quoted name or an identifier, found '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1030`
<!-- generated:end DSH5251 -->

**Cause.** `Output` selects an output of a 1.x call by name, and its value is neither a quoted
string nor an identifier.

**Fix.** Write `Output = "RGB"`.

## DSH5252

<!-- generated:begin DSH5252 -->
**Severity** error

**Message**

```
Expected either 'Output' or 'OutputIndex' on this call, found both.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:998`
<!-- generated:end DSH5252 -->

**Cause.** One 1.x call carries both `Output` and `OutputIndex`. They are two ways of choosing one
output.

**Fix.** Keep one of them.

## DSH5253

<!-- generated:begin DSH5253 -->
**Severity** error

**Message**

```
Expected an output selector only on a call that has outputs, found one on the constructor '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1046`
<!-- generated:end DSH5253 -->

**Cause.** An output selector (`Output` / `OutputIndex`) stands on a constructor such as
`float3(...)`, which has no outputs.

**Fix.** Remove the selector, or take the component with a swizzle.

## DSH5254

<!-- generated:begin DSH5254 -->
**Severity** warning

**Message**

```
'{0}' takes no positional argument here; 1.x ignored it, so it is dropped.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1061`, `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1070`
<!-- generated:end DSH5254 -->

**Cause.** A 1.x call spelling was given an argument it does not read: a positional argument where
the spelling takes named ones only, or a named argument that is none of its own. 1.x skipped both
without a word, so the node was built without that value; the front end drops it too and says so.

**Fix.** Name the argument the way the spelling reads it (`Pin = value`), or delete it.

## DSH5255

<!-- generated:begin DSH5255 -->
**Severity** error

**Message**

```
Expected 'UE.SceneTexture' to take exactly one argument, 'Id = ...', found {0} argument(s).
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1252`
<!-- generated:end DSH5255 -->

**Cause.** The 1.x shorthand `UE.SceneTexture(Id = ...)` takes its scene texture id and nothing
else.

**Fix.** Write `UE.SceneTexture(Id = PPI_SceneColor)`; any other property of the node needs
`UE.Expression(Class = "SceneTexture", ...)`.

## DSH5256

<!-- generated:begin DSH5256 -->
**Severity** error

**Message**

```
Expected 'SampleTexture2D' to take exactly two positional arguments, a texture and coordinates, found {0} argument(s).
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1179`
<!-- generated:end DSH5256 -->

**Cause.** The 1.x shorthand `SampleTexture2D(Texture, UV)` takes exactly a texture and coordinates.

**Fix.** Pass both and nothing else; for a mip level or derivatives use `UE.Expression(Class =
"TextureSample", ...)`.

## DSH5257

<!-- generated:begin DSH5257 -->
**Severity** error

**Message**

```
Expected 'UE.StaticSwitchParameter' to name its parameter with 'Name = "..."', found no name.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1284`
<!-- generated:end DSH5257 -->

**Cause.** `UE.StaticSwitchParameter(...)` has no `Name`. The name is the parameter an instance
overrides, and what makes two calls the same switch.

**Fix.** Add `Name = "Use Detail"`.

## DSH5258

<!-- generated:begin DSH5258 -->
**Severity** error

**Message**

```
Expected the static switch '{0}' to be called with a 'True = ...' and a 'False = ...' input, found at most one of them.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1122`, `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1309`
<!-- generated:end DSH5258 -->

**Cause.** A static switch is called without both of its sides. The node chooses between its `True`
and `False` inputs; with one of them missing there is nothing to choose.

**Fix.** Pass `True = ...` and `False = ...`.

## DSH5259

<!-- generated:begin DSH5259 -->
**Severity** error

**Message**

```
Expected every argument of the parameter call '{0}' to name an input pin, found a positional argument.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1150`
<!-- generated:end DSH5259 -->

**Cause.** A property that is a parameter node (a `TextureSampleParameter2D`, a
`StaticSwitchParameter`, ...) is called with a positional argument. Such a call is the node with its
pins wired, and pins go by name.

**Fix.** Name each argument: `MyTex(Coordinates = uv)`.

## DSH5260

<!-- generated:begin DSH5260 -->
**Severity** error

**Message**

```
Expected a name after '::', found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:561`
<!-- generated:end DSH5260 -->

**Cause.** `Namespace::` is not followed by a function name.

**Fix.** Write `N::F(...)`.

## DSH5261

<!-- generated:begin DSH5261 -->
**Severity** error

**Message**

```
Expected 'OutputType' of a Custom expression to be float1 to float4 or MaterialAttributes, found '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1488`
<!-- generated:end DSH5261 -->

**Cause.** The `OutputType` of a `UE.Expression(Class = "Custom", ...)` call is not a type a Custom
node can return.

**Fix.** Use `float1` .. `float4` (or `float` .. `float4`), or `MaterialAttributes`.

## DSH5263

<!-- generated:begin DSH5263 -->
**Severity** error

**Message**

```
Expected 'Default' of 'UE.StaticSwitchParameter' to be true or false, found '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1329`
<!-- generated:end DSH5263 -->

**Cause.** `Default` of a `UE.StaticSwitchParameter(...)` call is not `true` or `false`.

**Fix.** Write `Default = true`.

## DSH5264

<!-- generated:begin DSH5264 -->
**Severity** error

**Message**

```
Expected 'SortPriority' of 'UE.StaticSwitchParameter' to be a whole number, found '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1343`
<!-- generated:end DSH5264 -->

**Cause.** `SortPriority` of a `UE.StaticSwitchParameter(...)` call is not a whole number.

**Fix.** Write an integer.

## DSH5265

<!-- generated:begin DSH5265 -->
**Severity** error

**Message**

```
Expected the parameter call '{0}' to take only its inputs, found an output selector.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyExpressions.cpp:1090`
<!-- generated:end DSH5265 -->

**Cause.** A call to a parameter-node property carries `Output` / `OutputIndex`. Such a call is
rewritten into the node with its pins wired, and the selector has nowhere to go.

**Fix.** Select the output after the call with a member: `MyTex(Coordinates = uv).rgb`.

## DSH5275

<!-- generated:begin DSH5275 -->
**Severity** warning

**Message**

```
'{0}' matches '{1}' only in case; a 1.x source is read ignoring case, so this is '{1}', and a '.dss' needs the exact spelling.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:83`
<!-- generated:end DSH5275 -->

**Cause.** Rule L19. A name in a 1.x source matches a declaration only when case is ignored. 1.x
compared most names ignoring case, so the source builds; a `.dss` is case-sensitive.

**Fix.** Spell the name the way it is declared. `dsc migrate` does this for every such name.

## DSH5276

<!-- generated:begin DSH5276 -->
**Severity** warning

**Message**

```
'{0}' matches the engine name '{1}' only in case; 1.x matched engine names ignoring case, so this is '{1}', and a '.dss' needs the exact spelling.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:95`
<!-- generated:end DSH5276 -->

**Cause.** Rule L19, for engine names: a `UE.` class, a pin, a property or an output matches only
when case is ignored. 1.x looked these up ignoring case.

**Fix.** Use the engine's spelling, which the message shows. `dsc migrate` writes it.

## DSH5277

<!-- generated:begin DSH5277 -->
**Severity** warning

**Message**

```
'{0}' is the GLSL spelling of '{1}'; a 1.x source may use it and it is read as '{1}', and a '.dss' writes '{1}'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2989`
<!-- generated:end DSH5277 -->

**Cause.** Rule L2. A GLSL function or type spelling is used in a 1.x source (`mix`, `fract`, `mod`,
`vec3`). 1.x rewrote them; a `.dss` takes HLSL only.

**Fix.** Write the HLSL name the message gives. `dsc migrate` does.

## DSH5278

<!-- generated:begin DSH5278 -->
**Severity** warning

**Message**

```
'{0}' is not spelled like a value of '{1}', and 1.x matched enumerators loosely, so this is '{2}'; a '.dss' writes '{2}'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4017`
<!-- generated:end DSH5278 -->

**Cause.** Rule L12. An enum value is written the engine's way (`PPI_SceneColor`,
`TRANSFORM_Tangent`, `SAMPLERTYPE_Color`) or with its `Enum::` scope. 1.x matched enumerators
loosely -- prefix, scope and case aside -- and this front end resolves the same value; a `.dss`
takes the short form the catalog lists.

**Fix.** Write the value as the message spells it.

## DSH5279

<!-- generated:begin DSH5279 -->
**Severity** warning

**Message**

```
'{0}.{1}' leaves its required '{2}' pin unconnected, which 1.x allowed and the engine reports when the material compiles.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4699`, `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:403`
<!-- generated:end DSH5279 -->

**Cause.** Rule L13. A `UE.` node is built in a 1.x body with a pin open that the engine draws as
required. 1.x never checked, and many such sources compile because the node reads a default. In a
`.dss` the same thing is DSH5219, also a warning.

**Fix.** Wire the pin if the material fails to compile with 'missing input'; otherwise nothing has
to change.

## DSH5280

<!-- generated:begin DSH5280 -->
**Severity** error

**Message**

```
'{0}' has no output to select, and this call selects '{1}'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:592`
<!-- generated:end DSH5280 -->

**Cause.** A 1.x call selects an output (`Output = ...`, `OutputIndex = ...`, or a receiver passed
last) on something that has none: a helper with no result, or a call that is no node and no
function.

**Fix.** Remove the selector, or call something that has the output.

## DSH5281

<!-- generated:begin DSH5281 -->
**Severity** error

**Message**

```
An output is selected by a whole number the compiler knows, and this index is computed.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1542`, `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1577`
<!-- generated:end DSH5281 -->

**Cause.** `node[k]` reads output `k` of a node, and `k` is not a constant the compiler can
evaluate. Which wire is connected cannot depend on a run-time value.

**Fix.** Use a literal index, or name the output.

## DSH5282

<!-- generated:begin DSH5282 -->
**Severity** error

**Message**

```
This Custom node declares {0} output(s), counted from 0, and this selects output {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1588`, `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1608`, `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:611`
<!-- generated:end DSH5282 -->

**Cause.** An output index is past the outputs the node or function declares. Outputs count from 0;
a Custom node has its return value at 0 and its additional outputs after it.

**Fix.** Use an index inside the range, or name the output.

## DSH5283

<!-- generated:begin DSH5283 -->
**Severity** info

**Message**

```
'{0}' is not declared, and as in 1.x it is declared here as a local of type {1} receiving '{2}' of '{3}'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:652`
<!-- generated:end DSH5283 -->

**Cause.** Rule L5. A 1.x statement call passes a receiver (`F(a, Out)`) that is declared nowhere.
1.x declared it on the spot with the type of the output it receives, and the front end does the
same.

**Fix.** Nothing has to change. `dsc migrate` writes the declaration out.

## DSH5284

<!-- generated:begin DSH5284 -->
**Severity** error

**Message**

```
'{0}' becomes an input of the custom node, which carries a number or a texture, and this argument is {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4539`
<!-- generated:end DSH5284 -->

**Cause.** An argument of a `Custom` node call is a value a Custom node input cannot carry: a
material, a Substrate value, a sampler. A Custom input takes a number or a texture.

**Fix.** Pass the components the code needs (`m.BaseColor`), not the whole material.

## DSH5285

<!-- generated:begin DSH5285 -->
**Severity** error

**Message**

```
'{0}.{1}' has {2} input pin(s), counted from 0, and this argument connects pin {3}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4323`
<!-- generated:end DSH5285 -->

**Cause.** `Pin[k] = value` names an input pin by its engine index, and the class has fewer pins
than that.

**Fix.** Use an index the class has, or name the pin.

## DSH5286

<!-- generated:begin DSH5286 -->
**Severity** error

**Message**

```
'Pin[{0}] = ...' connects a node's input pin by its engine index, and only a 'UE.' or 'Substrate.' node call has one; pass this argument by name or by position.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2894`
<!-- generated:end DSH5286 -->

**Cause.** `Pin[k] = value` is used on a call that is no `UE.` / `Substrate.` node: a user function,
an intrinsic, a constructor. Only a reflected node has engine pin indices.

**Fix.** Pass the argument by name or by position.

## DSH5287

<!-- generated:begin DSH5287 -->
**Severity** info

**Message**

```
'{0}' has more than one output and is read as its first, '{1}', which is what 1.x did; a '.dss' names the output.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1807`
<!-- generated:end DSH5287 -->

**Cause.** Rule L3c. A node with several outputs is used as a value in a 1.x body without saying
which output. 1.x read the first one; so does this front end, and it says which that is.

**Fix.** Nothing has to change. A `.dss` names the output (`UE.ScreenPosition().ViewportUV`), and
`dsc migrate` writes it.

## DSH5288

<!-- generated:begin DSH5288 -->
**Severity** warning

**Message**

```
'{0}.{1}' has no 'DefaultValue', so the default written for this parameter is dropped, as 1.x dropped it.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4566`
<!-- generated:end DSH5288 -->

**Cause.** A 1.x property is a parameter node without a `DefaultValue` property (a collection
parameter, for one) and has a default written after `=`. 1.x dropped it.

**Fix.** Remove the default; set the value where the node takes it (the collection asset).

## DSH5289

<!-- generated:begin DSH5289 -->
**Severity** info

**Message**

```
{0} expects {1}, and this is {2}: its leading components are taken, which is what 1.x did; a '.dss' writes the swizzle.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:1870`
<!-- generated:end DSH5289 -->

**Cause.** Rule L22. A value is wider than the place it goes -- a `float4` parameter assigned to a
`vec3` variable, passed as a `float3` argument -- and 1.x cut it down to its leading components
without a word. Only a wider value into a narrower place: two operands of different widths are still
an error (DSH4226).

**Fix.** Nothing has to change. Writing the swizzle (`Tint.rgb`) says what is meant, and `dsc
migrate` writes it.

## DSH5290

<!-- generated:begin DSH5290 -->
**Severity** info

**Message**

```
The asset of this block and a function this file can call are both named '{0}', which 1.x kept apart; the block is declared as '{1}', and a '.dss' writes that name.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:844`
<!-- generated:end DSH5290 -->

**Cause.** Rule L23. A block builds an asset named like a function this file can call (a
`ShaderFunction(Name = ".../MF_X")` next to a `Function MF_X`). 1.x kept assets and functions in two
tables, so both existed; 2.0 has one name table, and the block's function is declared as
`<Name>_Asset` while the asset keeps its name.

**Fix.** Nothing has to change. The migrated file declares the export under the new identifier with
a `/// @name` for the asset.

## DSH5291

<!-- generated:begin DSH5291 -->
**Severity** info

**Message**

```
'{0}.{1}' lists no pin called '{2}'; it is connected by that name once the node exists, because a node may name its pins after its properties.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4595`
<!-- generated:end DSH5291 -->

**Cause.** Rule L24. A named argument is not a pin or property the catalog lists for the class. The
catalog is read off each class's default object, and some nodes name their pins after a property
(MoonToonModifier shows other pins per modifier, TextureSample shows `CoordinatesDX` only under
`MipValueMode = Derivative`, a Landscape layer blend calls its pins `Layer Grass` / `Height Rock` after
its `Layers`). The pin is wired by that name on the node once it exists; if the node
shows no such pin, the build fails there (DSH8212). In a `.dss` this applies only to classes the
catalog flags as naming their pins per node; elsewhere an unknown name is DSH5213. In a `.dss`, such a
pin fixes no width either: a numeric output is as wide as the widest value on one, and a `material`
on one makes the output a `material`.

**Fix.** Nothing to do if the name is what the node shows. The pin's own property name (`ChannelW`)
always resolves and does not need the lookup.

## DSH5292

<!-- generated:begin DSH5292 -->
**Severity** info

**Message**

```
'{0}' is not declared, and as in 1.x this assignment declares it, as a local of type {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2315`
<!-- generated:end DSH5292 -->

**Cause.** Rule L26. `x = value;` in a 1.x body assigns to a name declared nowhere. 1.x declared the
variable there, with the type of the value.

**Fix.** Nothing has to change; `dsc migrate` writes `T x = value;`. In a `.dss` an undeclared name
is an error.

## DSH5293

<!-- generated:begin DSH5293 -->
**Severity** error

**Message**

```
A Substrate value has no compound assignment; write 'S = S + T' (Substrate.Add) or 'S = S * w' (Substrate.Weight).
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:2347`, `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:274`, `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:302`
<!-- generated:end DSH5293 -->

**Cause.** An operator that Substrate values do not have. They take `A + B` (`Substrate.Add`), `A *
w` and `w * A` with a scalar (`Substrate.Weight`) and `lerp(A, B, t)` (`Substrate.HorizontalMix`),
and nothing else. Three messages: another operator (`A - B`, `A * B`, `-A`); a `lerp` whose first
two arguments are not both Substrate values or both numbers; a compound assignment (`A += B`), which
would make the declaration of `A` mean two nodes.

**Fix.** Write the node you mean: `Substrate.Layer(Top, Base, Thickness)` for layering, `lerp` for
mixing, and `A = A + B;` instead of `A += B;`.

## DSH5294

<!-- generated:begin DSH5294 -->
**Severity** error

**Message**

```
'Substrate.{0}' is a node Unreal Engine has from {1} on; this engine does not have it.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:4206`, `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:148`, `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:161`
<!-- generated:end DSH5294 -->

**Cause.** A piece of Substrate sugar needs a node this engine does not have. Three messages: a
`Substrate.X(...)` call whose node exists from a later Unreal Engine version on (the message names
the version); an operator, a run-time branch or a virtual argument that stands for such a node; and
the same on an engine that has no Substrate nodes at all (before 5.4).

**Fix.** Build against an engine that has the node, or write what the sugar stands for with the
nodes this engine has -- for a run-time branch, a `/// @static` condition or a `lerp`. Guard the
code with `#if DS_ENGINE_MINOR >= ...` when one source has to build on both.

## DSH5295

<!-- generated:begin DSH5295 -->
**Severity** error

**Message**

```
'{0}.{1}': '{2}' and '{3}' parameterize the same pins; give one of them.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:600`, `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:631`, `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:825`
<!-- generated:end DSH5295 -->

**Cause.** Two arguments parameterize the same pins. `BaseColor` / `Metallic` / `Specular` become
`DiffuseAlbedo` and `F0` through a conversion node, so giving `DiffuseAlbedo` or `F0` as well leaves
two values for one pin; `IOR` computes `F0`, so it conflicts with `F0` and with the metalness
arguments; `Haziness` becomes `SecondRoughness` and `SecondRoughnessWeight`; `Transmittance` becomes
`SSSMFP`. The same rule holds for a value built member by member, where the two writes are on
different lines.

**Fix.** Keep one parameterization: either the legacy one (`BaseColor`, `Metallic`, `Specular`) or
the pins (`DiffuseAlbedo`, `F0`).

## DSH5296

<!-- generated:begin DSH5296 -->
**Severity** error

**Message**

```
'{0}.Thickness' is how deep 'Transmittance' is measured, and '{0}' was given no 'Transmittance' before this use.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:454`, `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:464`, `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:784`, `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:840`
<!-- generated:end DSH5296 -->

**Cause.** A virtual argument came without the one it is measured against. `Haziness` converts the
BSDF's own `Roughness` into a second lobe, so it needs `Roughness`; `Thickness` is how deep
`Transmittance` is measured, so it needs `Transmittance`. For a value built member by member the
question is asked when the value is first used: the companion has to be written by then.

**Fix.** Give the companion -- `Roughness` with `Haziness`, `Transmittance` with `Thickness` -- in
the same call, or before the first use of the value.

## DSH5297

<!-- generated:begin DSH5297 -->
**Severity** error

**Message**

```
'{0}' has been assigned a whole Substrate value since it was declared, and that value has no members; build a new value, or write the members before the assignment.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:506`, `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:555`
<!-- generated:end DSH5297 -->

**Cause.** A member of a Substrate value is written after the value stopped being a builder. Two
messages: the value has been used already (passed, assigned, returned, combined with another), which
made its node, and a later write could not change that node; or the local has been assigned a whole
Substrate value since it was declared, and a finished value has no members.

**Fix.** Write every member before the first use of the value. To vary a value after using it, build
a second one.

## DSH5298

<!-- generated:begin DSH5298 -->
**Severity** error

**Message**

```
A member of the Substrate value '{0}' cannot be written inside an 'if': that would be one node in two versions. Build two values and choose between them.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:569`
<!-- generated:end DSH5298 -->

**Cause.** A member of a Substrate value is written inside an `if` that the value's declaration is
outside of. The value is one node, and a pin connected on one path only would be that node in two
versions. (A builder declared inside the arm is that arm's own, and may be written there.)

**Fix.** Compute the pin's value with the branch and write it once -- `S.Roughness = wet ? 0.1 :
0.8;` -- or build two values and choose between them.

## DSH5299

<!-- generated:begin DSH5299 -->
**Severity** error

**Message**

```
'{0}' builds a '{1}.{2}', which has no pin called '{3}'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:536`, `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:581`, `Source/DreamShaderLang/Private/Semantic/LangBinderSubstrate.cpp:649`
<!-- generated:end DSH5299 -->

**Cause.** A member access on a Substrate value being built does not work out. Three messages: the
node the value builds has no pin or virtual argument of that name; the member is read before
anything wrote it -- a member reads back what was written to it, and the node's own default is not a
value in the graph; the member is the left side of `*=`, `+=`, `++` and their kin before anything
wrote it.

**Fix.** Check the pin's name against the node (`dsc export-catalog`, or hover in the editor
extension), and assign a member before reading it.

