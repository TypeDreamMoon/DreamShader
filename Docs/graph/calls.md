# Calls

> [DreamShader](../index.md) » [Graph](index.md) » **Calls**

Invoking a `Function`, `GraphFunction`, `ShaderFunction`, `VirtualFunction` or an input-bearing
parameter from a `Graph` block, either as a value expression or as a standalone statement.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body or an `Outputs` binding expression |
| Kind | expression form and statement form |
| Generates | `UMaterialExpressionCustom` (`Function`, `GraphFunction`), `UMaterialExpressionMaterialFunctionCall` (`ShaderFunction`, `VirtualFunction`), `UMaterialExpressionStaticSwitchParameter` (static-switch call), or the parameter's own node with its input pins wired (parameter call) |

A `.dsm` / `.dsf` is read by the legacy front end and bound by the 2.0 binder *(since 2.0.0)*. The
call spellings on this page build the same nodes they did; where a 1.x call reads differently from a
`.dss` call, a numbered legacy rule applies and says so with its own code. The `.dss` spelling of
each is in [`dsc migrate`](../tools/migrate.md#what-the-rewrite-does).

## Synopsis

```c
// value form — the call produces a value
<target> = <callee> ( [ <argument> [ , <argument> ] … ] ) ;

// statement form — the call writes into named out variables
<callee> ( [ <input-argument> , ] … <out-target> [ , <out-target> ] … ) ;

<callee>         := <identifier> | <namespace> :: <identifier>
<argument>       := <expression> | <identifier> = <expression> | default
<out-target>     := <identifier>
```

`( ) , = ::` and `default` are literal DreamShaderLang text. `[ … ]`, `{ a | b }` and `…` are
meta-notation. A positional argument after a named one is
[`DSH2158`](../diagnostics/DSH2xxx.md#dsh2158), in every call *(since 2.0.0)*.

## Callable kinds

| Kind | Declared by | Value form | Statement form | Named arguments | Node produced |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `Function` | [`Function`](../language/function.md) | yes — the return value, or the first `out` *(since 2.0.0)* | yes | yes *(since 2.0.0)* | `Custom` |
| `GraphFunction` | [`GraphFunction`](../language/graph-function.md) | yes — as `Function` *(since 2.0.0)* | yes | yes *(since 2.0.0)* | `Custom` |
| `ShaderFunction` | [`ShaderFunction`](../language/shader-function.md), in the same file | yes, any output count | yes | yes | `MaterialFunctionCall` |
| `ShaderLayer`, `ShaderLayerBlend` | [`ShaderLayer`](../language/shader-layer.md) | **not callable**: [`DSH6208`](../diagnostics/DSH6xxx.md#dsh6208) *(since 2.0.0)* | — | — | — |
| `VirtualFunction` | [`VirtualFunction`](../language/virtual-function.md) | yes, any output count | yes | yes | `MaterialFunctionCall` |
| `StaticSwitchParameter` property | [`Properties`](../language/properties.md) | yes | no | yes | `StaticSwitchParameter` |
| input-bearing parameter | [`Properties`](../language/properties.md) | yes | no | **only** named | the parameter's own node, configured |

A `ShaderFunction` of another file is reached through a `VirtualFunction` naming its asset: an
`import` brings in a `.dsh` header only, and a header holds no asset blocks.

`UE.*`, `Substrate.*`, math builtins, constructors and `SampleTexture2D` are also call syntax, but they
are resolved before any of the kinds above and are documented separately — see
[Builtins](../builtins/index.md), [Math builtins](../builtins/math.md) and
[Constructors](constructors.md). A function named like a math builtin is
[`DSH6206`](../diagnostics/DSH6xxx.md#dsh6206) *(since 2.0.0)*. The complete dispatch order is in
[Name resolution](name-resolution.md#call-names).

## Callee spellings

| Spelling | Resolves to |
| :-- | :-- |
| `Name` | the function declared under that name, compared **case-sensitively** *(since 2.0.0)*. A name that matches exactly one function only when case is ignored resolves to it with the warning [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275) |
| `Namespace::Name` | a `Function` / `GraphFunction` declared inside `Namespace(Name="Namespace")`. The parser reads `N::F` as the one identifier `N_F`, the name the function is declared under *(since 2.0.0)*, so `N_F(…)` reaches it too; the bare `F` does not. A `::` with no name after it is [`DSH5260`](../diagnostics/DSH5xxx.md#dsh5260) |
| `DreamShaderFn_Name` | no longer an alias of the function *(since 2.0.0)*: [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208) |
| trailing path segment | for a `ShaderFunction` declared as `Name="Functions/F_Tint"`, the segment after the final `/` — `F_Tint` — with every character that is not a letter, a digit or `_` replaced by `_`. A `VirtualFunction`'s `Name` has to be an identifier already ([`DSH6311`](../diagnostics/DSH6xxx.md#dsh6311)) |

There is no overloading. One name declares one thing: a second function, property or struct of a
name already declared is [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210). The exception is a
`ShaderFunction` block whose leaf name is taken by a function declared before it — typically one from
an imported header: the block's function is declared as `<Name>_Asset` instead, and the asset keeps
its name (info [`DSH5290`](../diagnostics/DSH5xxx.md#dsh5290)). Two names that differ only in case
are two declarations; a call spelled like neither then has no unique case-insensitive match and is
`DSH4208`.

## Value form

```c
float L = Luma(BaseColor);
vec3  C = Common::ApplyTint(BaseColor, Tint);
vec3  N = F_Normal(uv, Output="Normal");
```

*(single-output `Function` / `GraphFunction` value calls since 1.3.1)*

| Kind | Requirements |
| :-- | :-- |
| `Function`, `GraphFunction` | The value is the return value or, for a function with `out` parameters, its first `out` (legacy rule L3b) *(since 2.0.0; 1.x refused a value call of a function with several outputs)*. Another output is read with an [output selector](#output-selection). Arguments fill the parameters in declaration order and by name: an input left without one is [`DSH4217`](../diagnostics/DSH4xxx.md#dsh4217), an argument past the last parameter [`DSH4224`](../diagnostics/DSH4xxx.md#dsh4224). The order counts `out` parameters too: an argument that lands on one receives that output, as in a statement call. |
| `ShaderFunction`, `VirtualFunction` | The value is output 0 — the first declared output — unless an [output selector](#output-selection) picks another *(since 2.0.0; 1.x required a selector when several outputs were declared)*. An input declared `opt` may be left out or passed `default`. |

A `GraphFunction` call is one Custom node, like a `Function` call; the `UE.` calls lifted out of its
body become further inputs of that node, lowered with this call's arguments.

Calls with the same callee and the same arguments are one node; see [Node reuse](node-reuse.md).

## Statement form

```c
F_PulseTint(BaseColor, Tint, TintedColor, PulseAmount);
```

*(multi-output `ShaderFunction` / `VirtualFunction` statement calls since 1.3.5)*

A statement that is a call may call anything. A call whose value nothing receives — a builtin, a
constructor, a parameter — is accepted, and the node it makes is dropped because nothing reads it
*(since 2.0.0; 1.x refused such a statement)*. A statement that is an expression but neither a call
nor an assignment is [`DSH2211`](../diagnostics/DSH2xxx.md#dsh2211) (`F(a).Out;`, `x;`); `x++;` is
[`DSH2207`](../diagnostics/DSH2xxx.md#dsh2207) and `break;`
[`DSH2208`](../diagnostics/DSH2xxx.md#dsh2208).

How the arguments of a statement call to a function are read:

| Arguments | Read as |
| :-- | :-- |
| all positional; at least one per output and at most one per input and output | the last ones receive the outputs, one each, in declaration order — the return value first when the function has one; the ones before them are inputs, in order (legacy rule L5) |
| any named argument, or a count outside that range | matched to the parameters in declaration order and by name, as in a `.dss` call: too many is `DSH4224`, and an input or an output left without an argument is `DSH4217` |

In the first reading an input not covered must be declared `opt` or passed `default`, otherwise
`DSH4217`. Every declared output must receive a target — there is no way to discard one.

### Out-target rules

| Rule | Failure |
| :-- | :-- |
| An out target is something that can be assigned: a variable, or a member or swizzle of one *(since 2.0.0; 1.x took a plain name only)* | [`DSH4239`](../diagnostics/DSH4xxx.md#dsh4239) |
| A name declared nowhere is declared here, as a local of the output's type (legacy rule L5) | info [`DSH5283`](../diagnostics/DSH5xxx.md#dsh5283) |
| A declared variable has the output's type. A numeric output wider than the variable gives its leading components, and a scalar output fills it; anything else is refused | [`DSH4218`](../diagnostics/DSH4xxx.md#dsh4218) |
| One variable named for two outputs | not reported |

> [!NOTE]
> An out target spelled in another case than a declaration is that declaration, with `DSH5275` —
> not a second variable *(since 2.0.0)*. A name only becomes a new local when nothing is declared
> under it in any case.

## Arguments

### Positional and named

| Callee | Positional | Named |
| :-- | :-- | :-- |
| `Function`, `GraphFunction`, `ShaderFunction`, `VirtualFunction` | fill the parameters in declaration order | match a parameter by name *(since 2.0.0 for `Function` / `GraphFunction`)*; another case resolves with `DSH5275`; no such parameter is [`DSH4216`](../diagnostics/DSH4xxx.md#dsh4216) |
| input-bearing parameter | rejected: [`DSH5259`](../diagnostics/DSH5xxx.md#dsh5259) | required: a pin of the node |
| `StaticSwitchParameter` | index 0 = true branch, index 1 = false branch | `True`/`A`, `False`/`B` |

Positional arguments come first and named ones after them; the reverse is `DSH2158`. A parameter given
twice — by position and by name, or by name twice — is [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215).
In a statement call, a named argument switches off the receivers-last reading (see
[Statement form](#statement-form)).

Parameter names are compared exactly, with the `DSH5275` fallback above; a pin of a node matches in
another case with the warning [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276). The names the 1.x front
end reads itself — `Output`, `OutputName`, `OutputIndex`, `True`, `False`, `A`, `B` — are matched
ignoring case.

### `default`

*(since 1.2.3)*

`default` is a bare identifier that holds the place of an input left out on purpose (legacy rule L25).
It is matched **case-sensitively** *(since 2.0.0)*, and only while no variable of that name is
declared.

| Situation | Behaviour |
| :-- | :-- |
| `default` for an input of a `ShaderFunction` / `VirtualFunction` | The input pin is left unconnected; the function's own default applies. |
| `default` for an input that is not `opt` | The same *(since 2.0.0; 1.x refused it)*. |
| `default` in a `Function` / `GraphFunction` call | Accepted *(since 2.0.0)*; the Custom node gets no input for that parameter. |
| `Default`, `DEFAULT` | An ordinary name: [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200) when nothing of that name is declared. |

Omitting a trailing optional input entirely has the same effect as passing `default`.

## Output selection

`Output`, `OutputName` and `OutputIndex` are named arguments the 1.x front end reads itself: it removes
them from the call and selects the output instead, as `.Name` or `[k]` written after the call. They
work on a call of any function kind *(since 2.0.0 for `Function` and `GraphFunction`)* and on `UE.*`
calls.

| Argument | Accepts | Meaning |
| :-- | :-- | :-- |
| `Output` | a quoted name or an identifier | select the output of that name — exact first, then ignoring case with `DSH5275` |
| `OutputName` | the same | exact synonym of `Output` |
| `OutputIndex` | a whole-number literal, 0 or more | select by 0-based index: the outputs in declaration order, with a return value as output 0 |

| Rule | Failure |
| :-- | :-- |
| `Output`/`OutputName` and `OutputIndex` are mutually exclusive | [`DSH5252`](../diagnostics/DSH5xxx.md#dsh5252) |
| `OutputIndex` must be a whole number of zero or more | [`DSH5250`](../diagnostics/DSH5xxx.md#dsh5250) |
| `Output` / `OutputName` must be a quoted name or an identifier, not an expression | [`DSH5251`](../diagnostics/DSH5xxx.md#dsh5251) |
| A constructor takes no selector | [`DSH5253`](../diagnostics/DSH5xxx.md#dsh5253) |
| A parameter call takes no selector | [`DSH5265`](../diagnostics/DSH5xxx.md#dsh5265) |
| The name must match an output of the function | [`DSH5280`](../diagnostics/DSH5xxx.md#dsh5280). On a function that returns a value, a name that is no output is read as a swizzle of the value instead ([`DSH4230`](../diagnostics/DSH4xxx.md#dsh4230) when it is not one) |
| The index must be inside the outputs | [`DSH5282`](../diagnostics/DSH5xxx.md#dsh5282) |
| A function with several outputs called with no selector | not an error *(since 2.0.0)*: the call reads output 0 |
| The selected output must exist on the loaded asset — matched by name, then ignoring case, then (for a `VirtualFunction`) by its declared position | [`DSH8221`](../diagnostics/DSH8xxx.md#dsh8221), when the asset is emitted |

> [!WARNING]
> A selector makes the call a value. It cannot stand in a statement call: the selected call is an
> expression nothing receives, which is `DSH2211`.

`UE.Expression(…)` accepts the same three selectors; a name the node does not have is
[`DSH5201`](../diagnostics/DSH5xxx.md#dsh5201), an index past its outputs `DSH5282`. See
[UE.Expression](../builtins/ue-expression.md).

### `BreakOutFloatNComponents`

A call named `BreakOutFloat2Components`, `BreakOutFloat3Components` or `BreakOutFloat4Components`
(any case) whose first argument is positional and which carries an output selector is **read as a
swizzle** of that first argument. No `MaterialFunctionCall` node is made, and no function of that name
is looked up *(since 2.0.0)*. The selected output names a channel, its letter in any case:

| Output name | Channel |
| :-- | :-- |
| `x`, `r`, `"0"` | 0 |
| `y`, `g`, `"1"` | 1 |
| `z`, `b`, `"2"` | 2 |
| `w`, `a`, `"3"` | 3 |

`OutputIndex=` selects the same channel by index. The channel has to lie inside the width the name
says (0–1 for `BreakOutFloat2Components`). Otherwise — no selector, a named first argument, a channel
past the width — the call is an ordinary call of the function of that name. Both selectors at once
are `DSH5252`.

## Calling a parameter

### Input-pin wiring

*(since 1.4.1)*

A declared parameter whose node owns input pins may be "called" to wire those pins. The front end
writes **each** use of such a property — a call or a bare read — as its own
`UE.Expression(Class = "<type>", ParameterName = "<name>", …)` call, with the declaration's default and
metadata as arguments and the call's arguments on the pins *(since 2.0.0)*. Uses with the same
arguments are one node after the [dedupe pass](node-reuse.md); a bare read and a pin call carry
different arguments, so they are two nodes that address the same material parameter. A texture-sample
parameter is read through its `RGBA` output.

Complete list of parameter node types that accept this form:

| | | |
| :-- | :-- | :-- |
| `ChannelMaskParameter` | `StaticComponentMaskParameter` | `TextureSampleParameter2D` |
| `TextureSampleParameter2DArray` | `TextureSampleParameterCube` | `TextureSampleParameterCubeArray` |
| `TextureSampleParameterVolume` | `TextureSampleParameterSubUV` | `RuntimeVirtualTextureSampleParameter` |
| `SparseVolumeTextureSampleParameter` | | |

Pin names are the node's own, as the engine catalog lists them; another case resolves with
`DSH5276`. Every argument must be named (`DSH5259`), and each value is checked against its pin like
any `UE.` argument ([`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214)).

> [!NOTE]
> Asset slots — the texture, curve or font a sampler parameter points at — are set with
> `[TextureSlot=Path(…)]`-style declaration metadata, which becomes a property argument of the node.
> A call argument that names a property the declaration already sets is `DSH4215`. A name that is
> neither a pin nor a property of the node is [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213); when
> its value is a graph value it is the info [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291) instead,
> and the pin is connected by that name once the node exists
> ([`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212) if the node shows no such pin).

### `StaticSwitchParameter`

*(since 1.2.3)*

A `StaticSwitchParameter` property does **not** resolve as a bare identifier (`DSH4200`); it is
readable only through the call form, which supplies the two branches.

| Branch | Argument names, in lookup order |
| :-- | :-- |
| true | `True=`, then `A=`, then positional index 0 |
| false | `False=`, then `B=`, then positional index 1 |

Both branches must be present ([`DSH5258`](../diagnostics/DSH5xxx.md#dsh5258)); any other argument is
dropped with the warning [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254). The call becomes a
`UE.Expression(Class = "StaticSwitchParameter", …)` with the branches on its `A` and `B` pins, which
are checked like the pins of any `UE.` node *(since 2.0.0)*.

```c
vec3 Albedo = UseDetail(True = DetailColor, False = BaseColor);
```

## Recursion and nesting

| Situation | Behaviour |
| :-- | :-- |
| A `GraphFunction` whose lifted `UE.` calls call it again, directly or through another function | [`DSH6330`](../diagnostics/DSH6xxx.md#dsh6330): every call would make a new Custom node |
| `Function` bodies that call one another in a cycle | [`DSH6260`](../diagnostics/DSH6xxx.md#dsh6260), when their bodies are embedded in one Custom node |
| `ShaderFunction` blocks of one file that call one another in a cycle | [`DSH8299`](../diagnostics/DSH8xxx.md#dsh8299) |
| Nested calls in one expression | Legal; arguments are ordinary expressions, so `F(G(x), 2.0)` works for any callable kind. |
| A call used as an `Outputs` binding expression | Legal — bindings are full expressions. |

## Diagnostics

Every diagnostic has a stable code; the code's page has the message and what to do. Messages are
reported as `<file>(<line>,<col>): DSHnnnn: <message>`, at the call or argument they are about.

### Resolution and dispatch

| Code | Raised when |
| :-- | :-- |
| `DSH4208` | The callee names no function, builtin or struct, or is not a name at all (a call on a parenthesized expression). |
| `DSH5275` | *(warning)* The name matches a function, or a builtin, only when case is ignored. |
| [`DSH5277`](../diagnostics/DSH5xxx.md#dsh5277) | *(warning)* `mix`, `fract`, `mod` or `inversesqrt`: the GLSL spelling of a builtin. |
| `DSH4210` | Two declarations share a name. |
| `DSH5290` | *(info)* A `ShaderFunction` block is declared as `<Name>_Asset` because its name is taken. |
| `DSH6206` | A function is named like a builtin. |
| `DSH6208` | A call to the `Shader`, a `ShaderLayer` or a `ShaderLayerBlend`. |
| `DSH5260` | `::` not followed by a name. |
| `DSH2211` | A statement is an expression whose value nothing receives. |
| `DSH2158` | A positional argument follows a named one. |

### Arguments and out targets

| Code | Raised when |
| :-- | :-- |
| `DSH4224` | More arguments than the function has parameters. |
| `DSH4216` | A named argument names no parameter. |
| `DSH4215` | A parameter is given twice. |
| `DSH4217` | An input that is not `opt` gets no argument, or, in a call read by name, an output gets no target. |
| [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) | An input argument does not convert to the parameter's type. |
| [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289) | *(info)* An input argument wider than its parameter is cut to its leading components. |
| `DSH4239` | An out target cannot be assigned. |
| `DSH4218` | An out target's type does not take the output. |
| `DSH5283` | *(info)* An out target declared nowhere becomes a local. |
| `DSH4200` | `default` in another case, or any other undeclared name, read as a value. |

### Output selection

| Code | Raised when |
| :-- | :-- |
| `DSH5252` | Both `Output`/`OutputName` and `OutputIndex`. |
| `DSH5250` | `OutputIndex` is not a whole number of zero or more. |
| `DSH5251` | `Output` is neither a quoted name nor an identifier. |
| `DSH5253` | A selector on a constructor. |
| `DSH5265` | A selector on a parameter call. |
| `DSH5280` | The function has no output of that name. |
| `DSH5282` | The index is past the outputs. |

### `Function`

| Code | Raised when |
| :-- | :-- |
| [`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201) | A parameter or result type is not a type. |
| [`DSH6305`](../diagnostics/DSH6xxx.md#dsh6305) | The function neither returns a value nor has an `out` parameter. |
| [`DSH6304`](../diagnostics/DSH6xxx.md#dsh6304) | The function has both a return type and `out` parameters. |
| [`DSH6253`](../diagnostics/DSH6xxx.md#dsh6253) | A `Substrate` input or result: a Custom node has no Substrate pins. |
| [`DSH6254`](../diagnostics/DSH6xxx.md#dsh6254) | A texture result. |
| [`DSH6210`](../diagnostics/DSH6xxx.md#dsh6210) | A `MaterialAttributes` input. |
| [`DSH6257`](../diagnostics/DSH6xxx.md#dsh6257) | *(warning)* A return type, and a body that never returns a value. |
| [`DSH6308`](../diagnostics/DSH6xxx.md#dsh6308) | A bare `return;` in a function with a return type. |
| `DSH6260` | Function bodies call one another in a cycle. |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | The emitter could not create the Custom node. |

### `GraphFunction`

Everything under `Function`, and:

| Code | Raised when |
| :-- | :-- |
| [`DSH6314`](../diagnostics/DSH6xxx.md#dsh6314) | A `UE.` call in the body has no closing `)`. |
| [`DSH6316`](../diagnostics/DSH6xxx.md#dsh6316) | *(warning)* A `Substrate.` call in the body is not lifted into a node. |
| [`DSH6325`](../diagnostics/DSH6xxx.md#dsh6325) | A lifted `UE.` call reads a variable of the caller, which 1.x allowed. |
| [`DSH6326`](../diagnostics/DSH6xxx.md#dsh6326) | A lifted `UE.` call reads a name that is neither a parameter nor declared at file scope. |
| [`DSH6329`](../diagnostics/DSH6xxx.md#dsh6329) | A lifted `UE.` call makes a value no Custom node input carries (a texture is fine; a material or a Substrate value is not). |
| `DSH6330` | The function reaches itself through its lifted calls. |

A lifted call is bound like any other expression, so a mistake inside it is reported with that
expression's own code.

### `ShaderFunction` family and `VirtualFunction`

| Code | Raised when |
| :-- | :-- |
| [`DSH6312`](../diagnostics/DSH6xxx.md#dsh6312) | A `VirtualFunction` has no `Asset` option. |
| [`DSH6313`](../diagnostics/DSH6xxx.md#dsh6313) | A `VirtualFunction` declares no output. |
| `DSH6311` | A `VirtualFunction`'s `Name` is missing or not an identifier. |
| [`DSH4315`](../diagnostics/DSH4xxx.md#dsh4315) | A `ShaderFunction` produces no output. |
| [`DSH8270`](../diagnostics/DSH8xxx.md#dsh8270) | The `Asset` reference does not resolve to an asset path. |
| [`DSH8219`](../diagnostics/DSH8xxx.md#dsh8219) | The function asset cannot be loaded, or cannot be assigned to the call node (a function that calls itself). |
| [`DSH8220`](../diagnostics/DSH8xxx.md#dsh8220) | The asset has no input of that name (nor, for a `VirtualFunction`, at the declared position). |
| `DSH8221` | The asset has no output of that name. |
| `DSH8214` | The emitter could not create the `MaterialFunctionCall` node. |

### Parameter call forms

| Code | Raised when |
| :-- | :-- |
| `DSH5259` | A positional argument in a pin call. |
| `DSH5276` | *(warning)* A pin name matches only when case is ignored. |
| `DSH5214` | A value does not fit its pin. |
| `DSH4215` | A pin or property is given twice, by the call or by the declaration and the call. |
| `DSH5213` | A name that is neither a pin nor a property, with a value that is not a graph value. |
| `DSH5291` | *(info)* A name the catalog does not list, connected by name once the node exists. |
| `DSH8212` | The live node has no pin of that name. |
| `DSH5258` | A `StaticSwitchParameter` call without both branches. |
| `DSH5254` | *(warning)* An argument of a `StaticSwitchParameter` call that is no branch, dropped. |
| `DSH4200` | A `StaticSwitchParameter` read without the call. |

## Example

```c
import "Helpers.dsh";

Shader(Name="Docs/M_Calls")
{
    Properties {
        vec3                   Tint    = vec3(1.0, 0.4, 0.1);
        TextureSampleParameter2D Albedo = Path(Game, "Textures/T_Albedo");
        StaticSwitchParameter  UseTint = true;
    }

    Settings {
        Domain       = "Surface";
        ShadingModel = "Unlit";
    }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        vec2 UV  = UE.TexCoord(Index = 0);

        // Parameter pin call: wires the sampler's Coordinates pin, read through RGBA.
        vec4 Tex = Albedo(Coordinates = UV);

        // Value form, Function with a return value, declared in Helpers.dsh.
        float L  = Luma(Tex.rgb);

        // Value form, namespaced Function: the value is its first out parameter.
        vec3 Lit = Common::ApplyTint(Tex.rgb, Tint);

        // Statement form: inputs first, then one out target per output. Neither target is
        // declared, so each becomes a local of its output's type (info DSH5283).
        PulseTint(Lit, Tint, Pulsed, Amount);

        // StaticSwitchParameter call selects between the two.
        Color = UseTint(True = Pulsed, False = vec3(L, L, L));
    }
}
```

`Helpers.dsh`:

```c
Function float Luma(in vec3 color) { return dot(color, float3(0.299, 0.587, 0.114)); }

Function PulseTint(in vec3 color, in vec3 tint, out vec3 result, out float amount) {
    amount = 0.5 + 0.5 * sin(color.r * 6.28318);
    result = color * tint * amount;
}

Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

Generated nodes:

```text
TextureCoordinate                      -> UV
TextureSampleParameter2D  Albedo       -> Tex          (Coordinates pin wired to UV)
Custom  "Luma"                         -> L
Custom  "Common::ApplyTint"            -> Lit
Custom  "PulseTint"                    -> Pulsed (output result), Amount (output amount)
AppendVector, AppendVector             -> vec3(L, L, L)
StaticSwitchParameter  UseTint         -> Color
```

The `PulseTint` Custom node's outputs are named after the function's `out` parameters — `result` is
output 0 and `amount` an additional output — *(since 2.0.0; 1.x named the additional outputs after the
caller's variables)*.

## See also

- [Name resolution](name-resolution.md) — the full dispatch order and the shadowing rules
- [Statements](statements.md) — the statement forms, including the standalone call statement
- [Expressions](expressions.md) — how a call fits into the expression grammar
- [`Function`](../language/function.md) — declaring `Function`, `Inline` / `SelfContained`, return types
- [`GraphFunction`](../language/graph-function.md) — the `UE.*` hoisting rule and its restrictions
- [`ShaderFunction`](../language/shader-function.md) — the reusable material-function block
- [`ShaderLayer` / `ShaderLayerBlend`](../language/shader-layer.md) — layer function blocks and arity
- [`VirtualFunction`](../language/virtual-function.md) — declaring an existing `UMaterialFunction`
- [`Namespace`](../language/namespace.md) — `::` qualified names and the no-nesting rule
- [Inputs / Outputs / Results](../language/inputs-outputs.md) — `in` / `out` / `opt` and declared defaults
- [Parameters in Graph](../parameters/graph-usage.md) — reading parameters and the pin call form
- [UE.Expression](../builtins/ue-expression.md) — the generic node builtin and its output selectors
- [Node reuse](node-reuse.md) — which call nodes are deduplicated
- [`dsc migrate`](../tools/migrate.md) — the `.dss` spelling of every legacy rule on this page
- [Diagnostics index](../diagnostics/index.md) — every code, by stage
