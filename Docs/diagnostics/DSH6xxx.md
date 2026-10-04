# DSH6xxx --- Functions and HLSL codegen

> The block between the generated markers is written by `.skill/gen-diagnostics.ps1`.
> Everything below a marker is written by hand and survives a regeneration.

## DSH6200

<!-- generated:begin DSH6200 -->
**Severity** error

**Message**

```
'{0}' is a second material entry; '{1}' above it is already the entry, and one file makes one material.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1216`
<!-- generated:end DSH6200 -->

**Cause.** Two exported functions in one file have the signature `void (inout material)`. That
signature *is* the declaration of the material entry, so a file with two of them describes two
materials. The message names both.

**Fix.** Keep one; move the other to its own file, or drop `export` to make it a helper that takes
a material — an internal function with that signature is not an entry.

## DSH6201

<!-- generated:begin DSH6201 -->
**Severity** error

**Message**

```
'{0}' is exported from a file whose entry is '{1}'; a file makes a material or it makes functions, not both. Move it to its own file, or drop 'export' to make it a helper.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1309`
<!-- generated:end DSH6201 -->

**Cause.** A file has a material entry **and** exported functions, layers or layer blends. One file
makes one kind of product.

**Fix.** Move the functions to their own file and `#include` it, or drop `export` so they become
helpers that are inlined into the material.

## DSH6202

<!-- generated:begin DSH6202 -->
**Severity** error

**Message**

```
'extern {0}' has nothing to bind to; add '/// @asset /Game/.../MF_Name' above it.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1144`
<!-- generated:end DSH6202 -->

**Cause.** An `extern` prototype has no `/// @asset` above it, so there is nothing for it to bind
to.

**Fix.** Add `/// @asset /Game/.../MF_Name` naming the existing material function.

## DSH6203

<!-- generated:begin DSH6203 -->
**Severity** error

**Message**

```
'@layer' makes '{0}' a material layer asset, so it has to be 'export'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1158`, `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1186`
<!-- generated:end DSH6203 -->

**Cause.** `@layer` or `@layerblend` on a function that is not `export`. Both directives name an
asset the file produces, and only an exported function produces one.

**Fix.** Add `export`, or delete the directive.

## DSH6204

<!-- generated:begin DSH6204 -->
**Severity** error

**Message**

```
A '@layer' function is written 'export void {0}(inout material m)'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1168`
<!-- generated:end DSH6204 -->

**Cause.** A `@layer` function does not have the signature `void Name(inout material m)`.

**Fix.** Write it with exactly that signature. A layer takes the material it is applied to and
modifies it in place.

## DSH6205

<!-- generated:begin DSH6205 -->
**Severity** error

**Message**

```
A '@layerblend' function is written 'export void {0}(material Base, material Top, ..., inout material Result)': at least one 'material' input and a final 'inout material'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1196`
<!-- generated:end DSH6205 -->

**Cause.** A `@layerblend` function does not have the shape
`void Name(material A, material B, …, inout material Result)`: at least one `material` input, every
other parameter an input, and a final `inout material`.

**Fix.** Reorder the parameters so the result comes last and is `inout material`.

## DSH6206

<!-- generated:begin DSH6206 -->
**Severity** error

**Message**

```
'{0}' is a builtin operation and cannot be redeclared; rename the function.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:837`
<!-- generated:end DSH6206 -->

**Cause.** A function is declared with the name of a builtin operation (`dot`, `lerp`, `saturate`,
…, and the GLSL spellings the language rejects). There is no overloading, so the two would be
ambiguous at every call site.

**Fix.** Rename the function.

## DSH6208

<!-- generated:begin DSH6208 -->
**Severity** error

**Message**

```
'{0}' is a {1} asset, not a function this file may call.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderExpressions.cpp:3553`
<!-- generated:end DSH6208 -->

**Cause.** A call to the material entry, a `@layer` or a `@layerblend`. Those are assets, not
functions this file may call — a layer is applied by the material that uses it.

**Fix.** Factor the shared code into a helper and call that from both.

## DSH6209

<!-- generated:begin DSH6209 -->
**Severity** error

**Message**

```
'{0}' has no body; a prototype has to be 'extern' and carry '/// @asset'.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1245`
<!-- generated:end DSH6209 -->

**Cause.** A function has no body and is not `extern`. Normally the parser catches this first
(DSH3208); this is the semantic backstop for a tree that reached the binder some other way.

**Fix.** Give it a body, or declare it `extern` with `/// @asset`.

## DSH6210

<!-- generated:begin DSH6210 -->
**Severity** error

**Message**

```
'{0}' is '@custom', so '{1}' becomes an input pin of a Custom node, and a Custom node cannot take a material; pass the fields it needs instead.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1265`
<!-- generated:end DSH6210 -->

**Cause.** A `/// @custom` function takes a `material` as an input (`material m` or
`inout material m`). A `@custom` body becomes a Custom node, whose inputs are pins the material
translator types, and there is no MaterialAttributes input type for one.

**Fix.** Pass the fields the body needs — `float3 BaseColor, float Roughness` — and write the
results back at the call site. A `material` **return** and an `out material` parameter stay legal:
a Custom node may produce attributes even though it cannot consume them.

## DSH6211

<!-- generated:begin DSH6211 -->
**Severity** error

**Message**

```
'{0}' is an 'out' parameter of '{1}' but the body never assigns it, so a caller would read a value nothing produced. Assign it before the function returns, or remove the parameter.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderStatements.cpp:392`
<!-- generated:end DSH6211 -->

**Cause.** A function declares an `out` parameter and its body never writes it — not as a whole,
not a swizzle, element or field of it, and not by passing it to another call's `out` / `inout`
parameter. A caller reading that output would get a value nothing produced; in the graph it is an
unconnected pin. A write on one branch only is enough to satisfy this check: it is "never", not
"not on every path".

**Fix.** Assign the parameter before the function returns, or remove it. Functions with an opaque
body (`/// @custom`, 1.x `Function`) and `extern` prototypes are not checked; their bodies are not
DreamShaderLang statements.

## DSH6220

<!-- generated:begin DSH6220 -->
**Severity** error

**Message**

```
'{0}' calls itself, and an inlined function has no stack to recurse on; rewrite it as a loop with a constant trip count, or as a '/// @custom' function.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:258`, `Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:267`
<!-- generated:end DSH6220 -->

**Cause.** An inlined function calls itself, directly or through others. Inlining has no stack:
a call is replaced by the callee's body, so a recursive call would expand forever.

**Fix.** Rewrite the recursion as a loop with a constant trip count (which unrolls), or move it
into a `/// @custom` function, where HLSL's own rules apply -- though HLSL has no recursion either,
so the loop is usually the real answer. A mutual recursion between two helpers reports at whichever
call closes the cycle; both functions are named in the message across the two reports.

## DSH6221

<!-- generated:begin DSH6221 -->
**Severity** error

**Message**

```
Inlining '{0}' would go {1} calls deep, past the limit of {2}; flatten the call chain or move part of it into a '/// @custom' function.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:275`
<!-- generated:end DSH6221 -->

**Cause.** Inlining went deeper than `MaxInlineDepth` (32 by default) without recursing. A chain of
thirty-two helpers each calling the next is legal and just very deep; the limit is there so a
pathological file fails with a message rather than exhausting memory.

**Fix.** Flatten the call chain, or turn one of the middle functions into an `export`ed material
function, which becomes its own asset and a single `FunctionCall` node rather than more inlined
nodes. Raising the limit is a pipeline option, not a source one.

## DSH6222

<!-- generated:begin DSH6222 -->
**Severity** error

**Message**

```
'{0}' is an '{1}' parameter of {2}, so the argument has to be something that can be assigned to; this expression cannot.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:128`
<!-- generated:end DSH6222 -->

**Cause.** An argument passed to an `out` or `inout` parameter is not something that can be
assigned to. The parameter writes back into the caller when the call finishes, so the argument has
to name a variable, a field, a material attribute or a swizzle of one -- not a computed value.

**Fix.** Put the result in a local and pass that: `float3 Value; Compute(Value); Use(Value);`.
Passing a literal, a call's result or an arithmetic expression to an `out` parameter cannot work.

## DSH6223

<!-- generated:begin DSH6223 -->
**Severity** error

**Message**

```
'{0}' is this file's material entry and is called by the engine, not by the shader.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:234`, `Source/DreamShaderLang/Private/IR/IRBuilderInline.cpp:284`
<!-- generated:end DSH6223 -->

**Cause.** Either a call to this file's material entry -- the `void (inout material)` function the
engine calls, not the shader -- or a call to a function that has no body to inline: a prototype
that is neither `extern` with `/// @asset` nor `/// @custom`.

**Fix.** For the entry, call the helper the entry calls, not the entry itself. For a body-less
function, give it a body, mark it `extern` and point `/// @asset` at the material function asset it
stands for, or mark it `/// @custom` and write its HLSL.

## DSH6250

<!-- generated:begin DSH6250 -->
**Severity** error

**Message**

```
'{0}' has no verbatim HLSL body, so it cannot become a custom node; only a '/// @custom' function can.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1094`, `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1945`, `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1957`, `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1968`
<!-- generated:end DSH6250 -->

**Cause.** `BuildDreamShaderCustomNodeCode` was called for a function index that does not exist, or
for a function the binder did not classify as `EBoundFunctionKind::Custom`, or — when a helper is
reached through a call — for a `@custom` function whose body the parser did not capture verbatim
(`FFunctionDecl::bOpaqueBody` is false, so `RawBody` is empty).

**Fix.** Inside a compile this is an internal error, not an authoring mistake: the IR builder decided
to make a Custom node for something that is not a custom function, or the parser and the binder
disagree about which bodies are opaque. Report it with the source file attached. From a tool that
calls the entry point directly, pass the index of a function whose `///` block carries `@custom`.

## DSH6251

<!-- generated:begin DSH6251 -->
**Severity** error

**Message**

```
'{0}' declares '{1}' as 'inout', which a custom node cannot carry; split it into an 'in' parameter and an 'out' parameter.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1135`
<!-- generated:end DSH6251 -->

**Cause.** A `@custom` parameter is declared `inout`. A custom node carries its `out` parameters as
*additional outputs*, which the engine already declares as `inout` parameters of the function it
generates; an input pin of the same name would declare that name a second time, and the shader would
not compile. There is no form of a custom node that both reads and writes one pin.

**Fix.** Split it: take the value in through one `in` parameter and hand the result back through a
separate `out` parameter.

```hlsl
/// @custom
void Adjust(float3 InColor, out float3 OutColor) { OutColor = InColor * 2; }
```

Note that `inout` remains the right spelling on a material entry (`void M(inout material m)`), which
is not a custom node.

## DSH6252

<!-- generated:begin DSH6252 -->
**Severity** error

**Message**

```
'{0}' takes the material '{1}' as an input; a custom node cannot accept material attributes on a pin, so read the fields the body needs and pass them as floats.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1172`
<!-- generated:end DSH6252 -->

**Cause.** A `@custom` function takes `material` (material attributes) as an input. The engine's
translator types every Custom-node input pin and has no case for material attributes — the shader
compile ends with `Bad type MaterialAttributes for <node> input <pin>`. Material attributes are fine
as a custom node's *output*; they are not accepted on the way in.

**Fix.** Read the attributes you need in the graph and pass them as floats:

```hlsl
/// @custom
float3 Tint(float3 BaseColor, float Roughness) { ... }
```

A helper `@custom` function that is embedded into another one's node may still take a
`FMaterialAttributes` argument — the restriction is about pins, not about HLSL — so this is reported
only for the function the node is being built for.

## DSH6253

<!-- generated:begin DSH6253 -->
**Severity** error

**Message**

```
'{0}' uses Substrate on '{1}'; a custom node has no Substrate pins, so build that part of the material out of reflected Substrate nodes.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1146`, `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1190`
<!-- generated:end DSH6253 -->

**Cause.** A `@custom` signature mentions `Substrate`. A custom node has no Substrate pins in any
engine version; 1.x refused the same thing on a `Function`.

**Fix.** Build the Substrate part of the material out of reflected `Substrate.*` / `UE.*` nodes and
keep the `@custom` function to the float maths around it.

## DSH6254

<!-- generated:begin DSH6254 -->
**Severity** error

**Message**

```
'{0}' returns a texture through '{1}'; a custom node output carries float1..4 or material attributes, never a texture object.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1158`, `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1200`
<!-- generated:end DSH6254 -->

**Cause.** A `@custom` function returns a texture or a sampler, or hands one back through an `out`
parameter. A custom node's outputs are `float1`..`float4` or material attributes; there is no pin
that carries a texture object, and HLSL will not return one from a function either.

**Fix.** Sample the texture inside the body and return the sampled value. A texture may be an
*input*; it is the output direction that has no form.

## DSH6255

<!-- generated:begin DSH6255 -->
**Severity** error

**Message**

```
'{0}' declares '{1}' twice in the HLSL it generates; a texture parameter also claims '{1}Sampler', which the engine declares alongside it.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1111`
<!-- generated:end DSH6255 -->

**Cause.** Two parameters of one `@custom` function end up with the same name in the HLSL that is
generated for it. The usual cause is the sampler pairing: a `Texture2D Tex` parameter also claims
`TexSampler`, because that is the name the engine declares beside it, so a second parameter actually
called `TexSampler` collides. Names are compared case-sensitively, as HLSL compares them.

**Fix.** Rename one of them. A `SamplerState` parameter is never paired automatically, so it is the
one to rename when a texture and an explicit sampler meet.

## DSH6256

<!-- generated:begin DSH6256 -->
**Severity** error

**Message**

```
'{0}' returns void but its body uses 'return;'; a custom node always returns its first output, so give the function a return type or restructure the body.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1213`
<!-- generated:end DSH6256 -->

**Cause.** A `@custom` function declared `void` uses a bare `return;` in its body. The function the
engine generates around that body always returns the node's first output, so a `return;` with no
value does not compile — at any brace depth, not only at the top.

**Fix.** Give the function a return type and return a value, or restructure the body so the early
exit becomes an `if`. A `void` `@custom` is legal — its node simply outputs 0 and does its work
through `out` parameters — it just cannot `return`.

## DSH6257

<!-- generated:begin DSH6257 -->
**Severity** warning

**Message**

```
'{0}' declares a return type but its body never returns a value; the node's first output will be 0.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1226`
<!-- generated:end DSH6257 -->

**Cause.** A `@custom` function declares a return type, but its body contains no `return` anywhere
(the warning does not fire for a body that is a single bare expression, which is given its `return`
automatically). The node's first output will be the constant 0.

**Fix.** Return the value the signature promises, or declare the function `void` and use `out`
parameters if the body is only there for its side effects.

## DSH6258

<!-- generated:begin DSH6258 -->
**Severity** error

**Message**

```
'{0}' has an '#include' with an empty path; write the virtual shader path the header lives at, for example "/Engine/Private/Common.ush".
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:956`
<!-- generated:end DSH6258 -->

**Cause.** A `#include` at the top of a `@custom` body has an empty path (`#include ""`). Leading
includes are lifted onto the node's include list, and an empty entry would reach the shader
compiler as `#include ""`.

**Fix.** Write the virtual shader path the header lives at — `/Engine/Private/Common.ush`,
`/Plugin/DreamShader/DreamShaderBuiltins.ush`, `/Plugin/<Name>/...`. Only `#include` lines at the
very top of a body (whitespace and comments may come before them) are hoisted; one that follows a
statement stays in the body, where it lands inside a function and only works for macro headers.

## DSH6259

<!-- generated:begin DSH6259 -->
**Severity** warning

**Message**

```
'{0}' differs from the function '{1}' only in case; HLSL is case-sensitive, so this call is left for the shader compiler. Did you mean '{1}'?
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1322`
<!-- generated:end DSH6259 -->

**Cause.** A call inside a `@custom` body names something that differs from a function of this module
only in case — `remap01(x)` where the module declares `Remap01`. DreamShaderLang 1.x matched function
names case-insensitively; 2.0 bodies are HLSL, and HLSL is case-sensitive, so the call is left for
the shader compiler to resolve.

**Fix.** Spell the name the way the function is declared. If the call is genuinely meant for a symbol
from one of the body's own `#include`s and the collision is a coincidence, rename one of the two —
there is no way to tell them apart at this level.

## DSH6260

<!-- generated:begin DSH6260 -->
**Severity** error

**Message**

```
The '@custom' functions {0} call each other in a cycle; HLSL has no recursion, so their bodies cannot be embedded in a custom node.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1361`
<!-- generated:end DSH6260 -->

**Cause.** Two or more `@custom` functions call each other, directly or through others. Their bodies
are embedded into the calling node as members of one struct, and HLSL has no recursion, so there is
no order that works. The message names the cycle.

**Fix.** Break the cycle: pull the shared part into a third `@custom` function that calls nobody, or
move the recursion into a hand-written `.ush` header the body `#include`s (where the shader compiler
will still refuse real recursion, but a bounded unrolled form can be written by hand).

## DSH6261

<!-- generated:begin DSH6261 -->
**Severity** error

**Message**

```
'{0}' is not a '@custom' function and cannot be called from the HLSL body of '{1}'; a custom node sees no graph values, so mark '{0}' '@custom' as well or move the call out of the body.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1307`
<!-- generated:end DSH6261 -->

**Cause.** A `@custom` body calls a function of this module that is not itself `@custom` — a helper,
an `export`ed material function, or an `extern`. Those become graph nodes; a custom node's body is
HLSL that the shader compiler sees long after the graph is built, and it has no way to reach a graph
value.

**Fix.** Mark the callee `/// @custom` too, so its body can be embedded alongside; or take the value
as a parameter of the custom function and compute it in the graph:

```hlsl
/// @custom
float3 Shade(float3 Color, float Mask) { ... }   // Mask computed by the caller, in the graph
```

## DSH6262

<!-- generated:begin DSH6262 -->
**Severity** error

**Message**

```
'{0}' takes {1} argument(s) but this call passes {2}; a call that carries a texture cannot be matched up by position otherwise.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1618`
<!-- generated:end DSH6262 -->

**Cause.** A call to a `@custom` function that takes a texture passes the wrong number of arguments.
The companion sampler has to be spliced in next to each texture argument, which means the arguments
have to line up with the parameters by position; with the wrong count there is no way to know which
argument is the texture.

**Fix.** Pass exactly one argument per parameter. A default value on a parameter does not let you
leave it out here — the sampler pairing needs every position.

## DSH6263

<!-- generated:begin DSH6263 -->
**Severity** error

**Message**

```
The texture argument for '{0}' of '{1}' has to be a plain texture name, because the sampler that goes with it is named after it.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1653`
<!-- generated:end DSH6263 -->

**Cause.** The argument in a texture position of a call to a `@custom` function is not a plain name
(`Pick(a, b)`, `bUseA ? A : B`, `Textures[i]`). The sampler that goes with a texture is named after
the texture — `Tex` brings `TexSampler` — so the argument has to be a name for that to mean anything.
1.x built the sampler name by pasting `Sampler` onto whatever the argument's text was, which turned
`a ? b : c` into `a ? b : cSampler`; 2.0 refuses instead of emitting that.

**Fix.** Assign the texture to a name first, or choose between textures in the graph and pass the
chosen one in as a parameter of the outer custom function.

## DSH6264

<!-- generated:begin DSH6264 -->
**Severity** warning

**Message**

```
'{0}' is 'selfcontained', so the '@custom' function '{1}' it calls is not embedded in its node; the call is left for the shader compiler to resolve out of this body's own includes.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1422`
<!-- generated:end DSH6264 -->

**Cause.** A `@custom selfcontained` body calls another `@custom` function of the module. A
`selfcontained` body is taken exactly as written — nothing of the module is embedded into it — so the
call is left for the shader compiler, which will only find the symbol if one of this body's own
`#include`s defines it.

**Fix.** Drop `selfcontained` if the call is meant to be embedded (that is the default behaviour, and
the closure is pulled in for you). Keep `selfcontained` if the symbol really comes from an included
header, and rename the local `@custom` function so the two do not look like the same thing.

## DSH6300

<!-- generated:begin DSH6300 -->
**Severity** error

**Message**

```
Expected a function name after '{0}', found '('.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1877`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1900`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1929`
<!-- generated:end DSH6300 -->

**Cause.** `Function` or `GraphFunction` (with its optional modifier and return type) is followed by
`(` where the function's name should be.

**Fix.** Write `Function float3 MyFunction(in float3 value) { ... }`.

## DSH6301

<!-- generated:begin DSH6301 -->
**Severity** error

**Message**

```
Expected '[in|out] Type Name' in the parameter list of '{0}', found '{1}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1986`
<!-- generated:end DSH6301 -->

**Cause.** A parameter of a 1.x function is not `[in|out] Type Name`. 1.x parameters have no
defaults and no metadata; those belong to the `Inputs` of a ShaderFunction.

**Fix.** Write each parameter as `in float3 color` or `out float result`.

## DSH6302

<!-- generated:begin DSH6302 -->
**Severity** error

**Message**

```
Expected 'in' or 'out' before the parameter '{0}' of '{1}', found '{2}'; a 1.x function has no 'inout'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1999`
<!-- generated:end DSH6302 -->

**Cause.** A parameter of a 1.x function carries `inout`, or another word where `in` / `out` goes.
1.x functions had inputs and results, never a parameter that was both.

**Fix.** Split it into an `in` and an `out` parameter. A `.dss` function may use `inout`.

## DSH6303

<!-- generated:begin DSH6303 -->
**Severity** error

**Message**

```
Expected a parameter name other than '__return', which 1.x reserved, in '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2012`
<!-- generated:end DSH6303 -->

**Cause.** A parameter is called `__return`. 1.x lowered a function's return value into an `out`
parameter of that name, so the name is taken.

**Fix.** Rename the parameter.

## DSH6304

<!-- generated:begin DSH6304 -->
**Severity** error

**Message**

```
Expected either a return type or 'out' parameters on '{0}', found both.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2044`
<!-- generated:end DSH6304 -->

**Cause.** A 1.x function has a return type and `out` parameters. 1.x allowed one or the other: a
returned value is the Custom node's output, and so is the first `out`.

**Fix.** Return the first result and keep no `out` parameter, or drop the return type and make every
result an `out`.

## DSH6305

<!-- generated:begin DSH6305 -->
**Severity** error

**Message**

```
Expected '{0}' to return a value or to have at least one 'out' parameter, found neither.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2051`
<!-- generated:end DSH6305 -->

**Cause.** A 1.x function returns nothing and has no `out` parameter, so its Custom node would have
no value for the graph to read.

**Fix.** Give it a return type or an `out` parameter.

## DSH6306

<!-- generated:begin DSH6306 -->
**Severity** warning

**Message**

```
'Inline' is the old spelling of 'SelfContained'; the function becomes '@custom selfcontained'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1888`
<!-- generated:end DSH6306 -->

**Cause.** `Function Inline` is used. In 1.x `Inline` is an exact synonym of `SelfContained` -- the
function's HLSL is embedded into each caller's Custom node instead of being referenced through the
include -- and 2.0 keeps one spelling.

**Fix.** Write `Function SelfContained`. Migrated files say `/// @custom selfcontained`.

## DSH6307

<!-- generated:begin DSH6307 -->
**Severity** error

**Message**

```
Expected a return type or a name after 'GraphFunction', found the modifier '{0}', which only a Function takes.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1868`
<!-- generated:end DSH6307 -->

**Cause.** `GraphFunction` is followed by `SelfContained` or `Inline`. Those modifiers belong to
`Function`; a GraphFunction is always one Custom node per call.

**Fix.** Remove the modifier.

## DSH6308

<!-- generated:begin DSH6308 -->
**Severity** error

**Message**

```
Expected a value after 'return' in '{0}', which returns '{1}', found a bare 'return;'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2096`
<!-- generated:end DSH6308 -->

**Cause.** A 1.x function with a return type contains a bare `return;`. 1.x lowered `return x;` into
an assignment to its result, and a `return` without a value has nothing to assign.

**Fix.** Return a value on every path.

## DSH6309

<!-- generated:begin DSH6309 -->
**Severity** error

**Message**

```
Expected a 'Name = "..."' attribute with a name on 'Namespace', found none.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2260`
<!-- generated:end DSH6309 -->

**Cause.** `Namespace` has no `Name = "..."` attribute, or the name is not an identifier. The name
becomes the prefix of every function in the block (`N::F`, flattened to `N_F`).

**Fix.** Write `Namespace(Name = "BL") { ... }`.

## DSH6310

<!-- generated:begin DSH6310 -->
**Severity** error

**Message**

```
Expected only Function and GraphFunction blocks inside the namespace '{0}', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2309`
<!-- generated:end DSH6310 -->

**Cause.** A `Namespace` block holds something other than `Function` and `GraphFunction` blocks. A
namespace groups HLSL functions and nothing else: no assets, no VirtualFunctions, no nested
namespaces.

**Fix.** Move the other block out of the namespace.

## DSH6311

<!-- generated:begin DSH6311 -->
**Severity** error

**Message**

```
Expected a 'Name = "..."' attribute with a name on 'VirtualFunction', found none.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2374`
<!-- generated:end DSH6311 -->

**Cause.** `VirtualFunction` has no `Name = "..."` attribute. The name is what Graph code calls.

**Fix.** Add the name.

## DSH6312

<!-- generated:begin DSH6312 -->
**Severity** error

**Message**

```
Expected an 'Asset = Path(...)' option on the VirtualFunction '{0}', found none.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2513`
<!-- generated:end DSH6312 -->

**Cause.** A VirtualFunction has no `Asset = Path(...)` in its `Options`. The whole point of the
block is to say which existing material function the name stands for.

**Fix.** Add `Options = { Asset = Path(Game, "Functions/MF_Example"); }`.

## DSH6313

<!-- generated:begin DSH6313 -->
**Severity** error

**Message**

```
Expected at least one output on the VirtualFunction '{0}', found none.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2520`
<!-- generated:end DSH6313 -->

**Cause.** A VirtualFunction declares no output. A call to it would be a node nothing can read.

**Fix.** Declare the outputs of the asset under `Outputs`, in the asset's order.

## DSH6314

<!-- generated:begin DSH6314 -->
**Severity** error

**Message**

```
Expected a ')' to close the 'UE.' call in the body of '{0}', found the end of the body.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2150`
<!-- generated:end DSH6314 -->

**Cause.** A `UE.` call in a verbatim body -- a 1.x GraphFunction, or a `/// @custom` function --
opens a parenthesis that the body never closes, so the call that is to be lifted into a node input
has no end.

**Fix.** Close the call. If the text is not meant as a node call, it cannot start with `UE.`
followed by a name and `(`.

## DSH6316

<!-- generated:begin DSH6316 -->
**Severity** warning

**Message**

```
A 'Substrate.' call in the body of '{0}' is not lifted into a node, because no custom node input carries a Substrate value; it reaches the shader compiler as text.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2157`
<!-- generated:end DSH6316 -->

**Cause.** A verbatim body calls `Substrate.<Node>(...)`. `UE.` calls in such a body are lifted into
inputs of the Custom node; a Substrate value cannot travel through a Custom node input, so the call
is left where it is and the shader compiler will not know the name.

**Fix.** Build the Substrate node in a graph body and keep only numbers in the HLSL.

## DSH6319

<!-- generated:begin DSH6319 -->
**Severity** error

**Message**

```
Expected '(' to open the parameter list of '{0}', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1938`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2060`
<!-- generated:end DSH6319 -->

**Cause.** A 1.x function's name is not followed by `(`, or its parameter list is not followed by
the `{` that opens its body.

**Fix.** Write the parameter list, empty if need be, and the body: `Function float Pi() { return
3.14159; }`.

## DSH6325

<!-- generated:begin DSH6325 -->
**Severity** error

**Message**

```
'{0}' lifts a 'UE.' call out of its body that reads '{1}', which 1.x took from the caller's scope and 2.0 does not; pass '{1}' to '{0}' as a parameter.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:780`
<!-- generated:end DSH6325 -->

**Cause.** Rule L8. A `UE.` call lifted out of a GraphFunction body reads a variable of the CALLER.
1.x evaluated lifted calls in the caller's scope, so this worked by accident of naming; the lifted
call is now bound in the function's own scope -- its parameters and the file's globals -- so that a
function means the same thing wherever it is called.

**Fix.** Add a parameter for the value and pass it at the call.

## DSH6326

<!-- generated:begin DSH6326 -->
**Severity** error

**Message**

```
'{0}' lifts a 'UE.' call out of its body that reads '{1}', which is neither a parameter of '{0}' nor declared at file scope.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:791`
<!-- generated:end DSH6326 -->

**Cause.** A `UE.` call lifted out of a verbatim body reads a name that is neither one of the
function's parameters nor declared at file scope. A lifted call is evaluated in the graph, outside
the HLSL, so it cannot see a variable the body declares.

**Fix.** Pass the value in as a parameter, or compute it in the graph and hand the result to the
function.

## DSH6327

<!-- generated:begin DSH6327 -->
**Severity** error

**Message**

```
'{0}' takes the 'UE.' calls lifted out of its body as inputs of its own custom node, so the HLSL body of '{1}' cannot call it; call it from a graph body instead.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1291`
<!-- generated:end DSH6327 -->

**Cause.** An HLSL body calls a function whose own body has `UE.` calls lifted out of it. Those
calls are inputs of that function's OWN Custom node; embedded as a helper in another node's code it
would have nobody to wire them.

**Fix.** Call the function from a graph body and pass its result in, or move the `UE.` calls out of
it into parameters.

## DSH6328

<!-- generated:begin DSH6328 -->
**Severity** error

**Message**

```
'{0}' lifts the call behind its input '{1}' out of a place its body does not have, so its custom node's code cannot be built.
```

**Raised by** `Source/DreamShaderLang/Private/IR/IRCustomHlsl.cpp:1015`
<!-- generated:end DSH6328 -->

**Cause.** A lifted call's recorded place lies outside the body text it belongs to. The front end
records where each `UE.` call stands in the body and the code builder replaces that range by the
input's name; the two disagree only when a declaration was altered between them. Internal error.

**Fix.** Report it with the source.

## DSH6329

<!-- generated:begin DSH6329 -->
**Severity** error

**Message**

```
'{0}' carries 'UE.' calls lifted out of its body, and only a '@custom' function with a verbatim body lifts calls into its node's inputs.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:695`, `Source/DreamShaderLang/Private/Semantic/LangBinderLegacy.cpp:727`
<!-- generated:end DSH6329 -->

**Cause.** A function that is not a `/// @custom` function with a verbatim body carries lifted
calls, or a lifted call makes a value no Custom input carries (a material, a Substrate value). The
first is an internal inconsistency; the second means the body calls a node whose result is not a
number or a texture.

**Fix.** For the second case, build that node in a graph body instead of inside the HLSL.

## DSH6330

<!-- generated:begin DSH6330 -->
**Severity** error

**Message**

```
'{0}' reaches itself through the 'UE.' calls lifted out of its body, and each call makes a new custom node, so the graph would never end.
```

**Raised by** `Source/DreamShaderLang/Private/Semantic/LangBinder.cpp:1473`
<!-- generated:end DSH6330 -->

**Cause.** A function reaches itself through a call lifted out of its body: the lifted `UE.` call's
arguments call the function again. Each call of such a function is a new Custom node with its own
lifted inputs, so the expansion would not end.

**Fix.** Break the cycle: compute the inner value in the graph and pass it in.

