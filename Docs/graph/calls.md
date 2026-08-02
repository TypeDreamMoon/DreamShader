# Calls

> [DreamShader](../index.md) » [Graph](index.md) » **Calls**

Invoking a `Function`, `GraphFunction`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`,
`VirtualFunction` or an input-bearing parameter from a `Graph` block, either as a value expression or
as a standalone statement.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body or an `Outputs` binding expression |
| Kind | expression form and statement form |
| Generates | `UMaterialExpressionCustom` (`Function`, `GraphFunction`), `UMaterialExpressionMaterialFunctionCall` (`ShaderFunction` family, `VirtualFunction`), `UMaterialExpressionStaticSwitchParameter` (static-switch call), or the parameter's own node with its input pins wired (parameter call) |

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
meta-notation.

## Callable kinds

| Kind | Declared by | Value form | Statement form | Named arguments | Node produced |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `Function` | [`Function`](../language/function.md) | only when exactly one output is declared | yes | **no** | `Custom` |
| `GraphFunction` | [`GraphFunction`](../language/graph-function.md) | only when exactly one output is declared | yes | **no** | `Custom` |
| `ShaderFunction` | [`ShaderFunction`](../language/shader-function.md) | yes, any output count | yes | yes (value form only) | `MaterialFunctionCall` |
| `ShaderLayer` | [`ShaderLayer`](../language/shader-layer.md) | yes, any output count | yes | yes (value form only) | `MaterialFunctionCall` |
| `ShaderLayerBlend` | [`ShaderLayer`](../language/shader-layer.md) | yes, any output count | yes | yes (value form only) | `MaterialFunctionCall` |
| `VirtualFunction` | [`VirtualFunction`](../language/virtual-function.md) | yes, any output count | yes | yes (value form only) | `MaterialFunctionCall` |
| `StaticSwitchParameter` property | [`Properties`](../language/properties.md) | yes | no | yes | `StaticSwitchParameter` |
| input-bearing parameter | [`Properties`](../language/properties.md) | yes | no | **only** named | the parameter's own node, configured |

`UE.*`, `Substrate.*`, math builtins, constructors and `SampleTexture2D` are also call syntax, but they
are resolved before any of the kinds above and are documented separately — see
[Builtins](../builtins/index.md), [Math builtins](../builtins/math.md) and
[Constructors](constructors.md). The complete dispatch order is in
[Name resolution](name-resolution.md#call-names).

## Callee spellings

| Spelling | Resolves to |
| :-- | :-- |
| `Name` | a `Function` / `GraphFunction` whose declared name matches (case-insensitively), or a `ShaderFunction`-family / `VirtualFunction` block whose declared name matches |
| `Namespace::Name` | a `Function` / `GraphFunction` declared inside `Namespace(Name="Namespace")`. Namespaced functions are reachable **only** by their fully qualified name. |
| `DreamShaderFn_Name` | the same `Function` — its generated HLSL symbol name is accepted as an alias |
| trailing path segment | for a `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` / `VirtualFunction` declared as `Name="Functions/F_Tint"`, the segment after the final `/` — `F_Tint` |

There is no overload resolution. Names are not distinguished by argument count or type; the first
declaration whose name matches wins.

## Value form

```c
float L = Luma(BaseColor);
vec3  C = Common::ApplyTint(BaseColor, Tint);
vec3  N = F_Normal(uv, Output="Normal");
```

*(single-output `Function` / `GraphFunction` value calls since 1.3.1)*

| Kind | Requirements |
| :-- | :-- |
| `Function` | Exactly one declared output, otherwise `DreamShader Function '{Name}' has {Count} outputs and must be called with explicit out variables, for example {Name}(..., ResultA, ResultB).` The argument count must equal the declared input count **exactly**. |
| `GraphFunction` | Same two rules. Requires an active `Graph` build context. Internally the call is rewritten to a statement call whose out target is a generated temporary named `__ds_<sanitized function name>_value<N>`. |
| `ShaderFunction` family, `VirtualFunction` | Any output count. Exactly one output is selected — implicitly when only one is declared, otherwise with `Output=` / `OutputName=` / `OutputIndex=`. Optional inputs may be omitted entirely or passed as `default`. |

The call node itself is built once per distinct argument list for material-function-backed calls; see
[Node reuse](node-reuse.md).

## Statement form

```c
F_PulseTint(BaseColor, Tint, TintedColor, PulseAmount);
```

*(multi-output `ShaderFunction` / `VirtualFunction` statement calls since 1.3.5)*

A statement whose whole text is a call is an **expression statement**. It must satisfy the entry gate
before any kind-specific rule applies:

| Gate | Failure |
| :-- | :-- |
| The statement is a call expression | `Graph expression statements currently support only Function calls with explicit out arguments.` |
| The callee flattens to a name | `Graph expression statements must call a named Function.` |
| The name matches at least one callable definition | `Graph expression statement '{Name}' is unsupported. Only DreamShader Function, GraphFunction, ShaderFunction, ShaderLayer, ShaderLayerBlend, or VirtualFunction calls may use statement syntax.` |
| The name matches **exactly one** definition | `Graph expression statement '{Name}' is ambiguous because multiple callable definitions exist.` |

Arguments are **inputs first, then one out target per output**, in declaration order.

| Kind | Arity rule |
| :-- | :-- |
| `Function`, `GraphFunction` | Exact: `inputs + outputs`. `DreamShader Function '{Name}' expects {Total} arguments ({Inputs} inputs, {Outputs} out targets) but got {Got}.` |
| `ShaderFunction` family, `VirtualFunction` | At least `outputs` arguments; the leading `arguments − outputs` are inputs and must not exceed the declared input count. Every input **not** covered must be declared `opt`, otherwise `{Kind} '{Name}' is missing required input '{Input}'.` |

Every declared output must receive a target — there is no way to discard one.

### Out-target rules

| Rule | Failure |
| :-- | :-- |
| Each out target is a **plain variable name**, not an expression, a member path or a swizzle | `DreamShader Function '{Name}' out argument {Index} must be a plain variable name.` *(1-based index)* — or `{Kind} '{Name}' output argument {Index} must be a plain variable name.` |
| The name is non-empty after trimming | `DreamShader Function '{Name}' has an empty out target name.` / `{Kind} '{Name}' has an empty output target name.` |
| The names are distinct within one call, compared **case-sensitively** | `DreamShader Function '{Name}' cannot write multiple out results into '{Target}' in the same call.` / `{Kind} '{Name}' cannot write multiple outputs into '{Target}' in the same call.` |

An out target does not need to exist beforehand and does not need to be declared. Each target is bound
in the current scope to the call's output with that output's declared shape, **replacing** any previous
value of that name without a type check. Declare the variable first if a specific width matters.

> [!NOTE]
> Because out targets are matched case-sensitively for uniqueness but Graph variables are looked up
> case-insensitively, `F(a, Result, result)` passes the uniqueness check and then binds `Result` and
> `result` — two entries whose later reads resolve to whichever the case-insensitive scan finds first.

## Arguments

### Positional and named

| Callee | Positional | Named | Mixing |
| :-- | :-- | :-- | :-- |
| `Function`, value or statement | required | rejected: `DreamShader Function '{Name}' currently uses positional arguments only.` | — |
| `GraphFunction`, value or statement | required | rejected: `DreamShader GraphFunction '{Name}' currently uses positional arguments only.` | — |
| `ShaderFunction` family / `VirtualFunction`, **value** form | allowed | allowed | **forbidden** — `{Kind} '{Name}' input arguments cannot mix positional and named forms.` |
| `ShaderFunction` family / `VirtualFunction`, **statement** form | required | rejected: `{Kind} '{Name}' statement calls currently use positional arguments only.` | — |
| input-bearing parameter | rejected: `Parameter '{Name}' must be called with named arguments wiring its input pins (e.g. {Name}(Coordinates=...) or {Name}(Input=...)).` | required | — |
| `StaticSwitchParameter` | allowed (index 0 = true branch, index 1 = false branch) | allowed (`True`/`A`, `False`/`B`) | allowed |

Argument names are normalized before comparison: leading and trailing whitespace is trimmed and the
name is lower-cased. `Coordinates=`, `coordinates=` and ` COORDINATES =` are the same argument.

The "no mixing" rule is per call, not per argument: if **any** argument is named, **no** positional
argument may appear. A named argument that matches no declared input fails with
`{Kind} '{Name}' does not have an input named '{Argument}'.`; too many positional arguments fail with
`{Kind} '{Name}' received {Got} positional input argument(s), but only {Declared} input(s) are declared.`

### `default`

*(since 1.2.3)*

`default` is a bare identifier, matched case-insensitively, that explicitly requests an optional
input's declared default instead of supplying a value.

| Situation | Behaviour |
| :-- | :-- |
| `default` for an input declared `opt` | The input pin is left unconnected; the function's own default applies. |
| `default` for a required input | `{Kind} '{Name}' input '{Input}' is not optional and cannot use default.` |
| `default` in a `Function` / `GraphFunction` call | Not recognized — it is evaluated as an ordinary identifier and fails with `Unknown Graph identifier 'default'.` |

Omitting a trailing optional input entirely has the same effect as passing `default`.

## Output selection

`Output`, `OutputName` and `OutputIndex` are reserved named arguments on the **value form** of
`ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` and `VirtualFunction` calls. They are removed from
the input argument list before inputs are bound.

| Argument | Accepts | Meaning |
| :-- | :-- | :-- |
| `Output` | a literal | select the output whose declared name matches, case-insensitively |
| `OutputName` | a literal | exact synonym of `Output` |
| `OutputIndex` | an integer literal | select by 0-based index into the declared `Outputs` list |

| Rule | Failure |
| :-- | :-- |
| `Output`/`OutputName` and `OutputIndex` are mutually exclusive | `{Kind} '{Name}' cannot use OutputName/Output together with OutputIndex.` |
| Neither given while more than one output is declared | `{Kind} '{Name}' exposes multiple outputs. Specify Output="Name" or OutputIndex=N.` |
| `OutputIndex` must be an integer literal within range | `{Kind} '{Name}' OutputIndex is out of range.` |
| `Output` / `OutputName` must be a literal, not an expression | `{Kind} '{Name}' OutputName must be a literal value.` |
| The name must match a declared output | `{Kind} '{Name}' does not expose an output named '{Output}'.` |
| The selected output must exist on the loaded asset — matched by name first, then by ordinal | `{Kind} '{Name}' output '{Output}' does not exist on MaterialFunction asset '{Asset}'.` |

> [!WARNING]
> These three names are **not** available on `Function` or `GraphFunction` calls, which reject named
> arguments outright. A multi-output `Function` must use the statement form with explicit out targets.
> They are also unavailable in the statement form of every kind, because statement calls reject named
> arguments.

`UE.Expression(…)` accepts the same three selectors with the same rules; see
[UE.Expression](../builtins/ue-expression.md).

### `BreakOutFloatNComponents`

A `VirtualFunction` call whose declared name is `BreakOutFloat2Components`,
`BreakOutFloat3Components` or `BreakOutFloat4Components` (case-insensitive) is **inlined as a swizzle**
instead of generating a `MaterialFunctionCall` node, provided it has a usable first input argument and
an output selector. The selected output name maps to a channel:

| Output name | Channel |
| :-- | :-- |
| `x`, `r` | 0 |
| `y`, `g` | 1 |
| `z`, `b` | 2 |
| `w`, `a` | 3 |

`OutputIndex=` selects the same channel by index. If any precondition is not met — a `default` first
argument, both selectors given, no selector at all — the call falls through to the ordinary
`MaterialFunctionCall` path.

## Calling a parameter

### Input-pin wiring

*(since 1.4.1)*

A declared parameter whose node owns input pins may be "called" to wire those pins. The call
materializes the parameter node exactly as a bare reference does and connects each named argument to
the input pin of the same name; the node is cached under the parameter's name, so a later bare
reference shares the configured node.

Complete list of parameter node types that accept this form:

| | | |
| :-- | :-- | :-- |
| `ChannelMaskParameter` | `StaticComponentMaskParameter` | `TextureSampleParameter2D` |
| `TextureSampleParameter2DArray` | `TextureSampleParameterCube` | `TextureSampleParameterCubeArray` |
| `TextureSampleParameterVolume` | `TextureSampleParameterSubUV` | `RuntimeVirtualTextureSampleParameter` |
| `SparseVolumeTextureSampleParameter` | | |

Pin names are matched with the same normalization as argument names. Every argument must be named, and
every value must be numeric.

> [!NOTE]
> Asset slots — the texture, curve or font a sampler parameter points at — are **not** call arguments.
> They are set with `[TextureSlot=Path(…)]`-style declaration metadata. Passing one as a call argument
> fails with `Parameter '{Name}' ({NodeType}) has no input pin named '{Argument}'. Asset slots
> (Texture/Curve/Font/...) are set via [{Argument}=Path(...)] metadata, not call arguments.`

### `StaticSwitchParameter`

*(since 1.2.3)*

A `StaticSwitchParameter` property does **not** resolve as a bare identifier; it is readable only
through the call form, which supplies the two branches.

| Branch | Argument names, in lookup order |
| :-- | :-- |
| true | `True=`, then `A=`, then positional index 0 |
| false | `False=`, then `B=`, then positional index 1 |

Both branches must be present, must not be texture objects or `Substrate` values, must agree on the
`MaterialAttributes` flag, and must have the same component count.

```c
vec3 Albedo = UseDetail(True = DetailColor, False = BaseColor);
```

## Recursion and nesting

| Situation | Behaviour |
| :-- | :-- |
| `GraphFunction` calling itself, directly or through another `GraphFunction` | Detected at build time: `GraphFunction cycle detected: {Path}.` with the active call stack joined by ` -> `. Names are compared case-insensitively. |
| `Function` marked `SelfContained` / `Inline` calling itself | Detected while the Custom-node code is assembled: `SelfContained Function cycle detected: {Path}. HLSL Custom nodes cannot compile recursive DreamShader functions.` |
| Nested calls in one expression | Legal; arguments are ordinary expressions, so `F(G(x), 2.0)` works for any callable kind. |
| A call used as an `Outputs` binding expression | Legal — bindings are full expressions. |

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout these tables. `{Kind}` is one of
`ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `VirtualFunction`.

### Resolution and dispatch

| Message | Cause |
| :-- | :-- |
| `Graph calls must target a named function.` | The callee is not an identifier or a `::` / `.` qualified name. |
| `Unknown Graph function '{Name}'.` | The name matched nothing on any surface. |
| `Graph call '{Name}' is ambiguous because multiple definitions use that name: {Kinds}.` | Two or more of `Function`, `GraphFunction`, the `ShaderFunction` family and `VirtualFunction` declare the same name. |
| `Graph expression statements currently support only Function calls with explicit out arguments.` | A statement that is not a call, such as `a++;` or `break;`. |
| `Graph expression statements must call a named Function.` | A statement call whose callee is not a name. |
| `Graph expression statement '{Name}' is unsupported. Only DreamShader Function, GraphFunction, ShaderFunction, ShaderLayer, ShaderLayerBlend, or VirtualFunction calls may use statement syntax.` | A statement call to a builtin, a constructor or a parameter. |
| `Graph expression statement '{Name}' is ambiguous because multiple callable definitions exist.` | As above, for the statement path. |
| `Expected function name after '::'.` | `::` not followed by an identifier. |

### `Function`

| Message | Cause |
| :-- | :-- |
| `DreamShader Function '{Name}' has {Count} outputs and must be called with explicit out variables, for example {Name}(..., ResultA, ResultB).` | Value form on a multi-output function. |
| `DreamShader Function '{Name}' returns one value and expects {Expected} input argument(s) when used as a value expression, but got {Got}.` | Wrong argument count in the value form. |
| `DreamShader Function '{Name}' currently uses positional arguments only.` | A named argument in either form. |
| `DreamShader Function '{Name}' must declare at least one out result.` | Statement form on a function with no outputs. |
| `DreamShader Function '{Name}' expects {Total} arguments ({Inputs} inputs, {Outputs} out targets) but got {Got}.` | Wrong argument count in the statement form. |
| `DreamShader Function '{Name}' out argument {Index} must be a plain variable name.` | An out target that is not a bare identifier. |
| `DreamShader Function '{Name}' has an empty out target name.` | An out target that trims to nothing. |
| `DreamShader Function '{Name}' cannot write multiple out results into '{Target}' in the same call.` | The same out target used twice. |
| `DreamShader Function '{Name}' input '{Input}': {Error}` | An input argument failed to evaluate or to coerce to the declared type. |
| `DreamShader Function '{Name}' input '{Input}' uses unsupported type '{Type}'.` | The declared input type is not a recognized Graph type token. |
| `DreamShader Function '{Name}' input '{Input}' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead.` | A `Substrate` input on an HLSL-backed `Function`. |
| `DreamShader Function '{Name}' input '{Input}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | The same declaration on UE 5.3. |
| `DreamShader Function '{Name}' has unsupported result type '{Type}'.` | A result type outside `float1..4` / `half*` / `int*` / `uint*` / `bool*` / `MaterialAttributes`. |
| `DreamShader Function '{Name}' result '{Result}' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead.` | A `Substrate` result on an HLSL-backed `Function`. |
| `DreamShader Function '{Name}' result '{Result}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | The same declaration on UE 5.3. |
| `Failed to create a Custom node for DreamShader Function '{Name}'.` | The `UMaterialExpressionCustom` node could not be created. |
| `Unknown SelfContained DreamShader Function '{Name}'.` | The embedded-function request named a `Function` that does not exist. |
| `SelfContained Function cycle detected: {Path}. HLSL Custom nodes cannot compile recursive DreamShader functions.` | Recursion among `SelfContained` / `Inline` functions. |

### `GraphFunction`

| Message | Cause |
| :-- | :-- |
| `GraphFunction value call requires an active Graph build context.` | A `GraphFunction` value call outside a `Graph` build. |
| `GraphFunction call requires an active Graph build context.` | The same for the statement form. |
| `DreamShader GraphFunction '{Name}' has {Count} outputs and must be called with explicit out variables, for example {Name}(..., ResultA, ResultB).` | Value form on a multi-output `GraphFunction`. |
| `DreamShader GraphFunction '{Name}' returns one value and expects {Expected} input argument(s) when used as a value expression, but got {Got}.` | Wrong argument count in the value form. |
| `DreamShader GraphFunction '{Name}' currently uses positional arguments only.` | A named argument in either form. |
| `DreamShader GraphFunction '{Name}' did not produce a value result.` | The generated temporary out target was not written. |
| `DreamShader GraphFunction '{Name}' must declare at least one out result.` | Statement form on a `GraphFunction` with no outputs. |
| `GraphFunction cycle detected: {Path}.` | Direct or indirect recursion. |
| `DreamShader GraphFunction '{Name}' expects {Total} arguments ({Inputs} inputs, {Outputs} out targets) but got {Got}.` | Wrong argument count in the statement form. |
| `DreamShader GraphFunction '{Name}' input '{Input}': {Error}` | An input argument failed to evaluate or coerce. |
| `DreamShader GraphFunction '{Name}' input '{Input}' uses unsupported type '{Type}'.` | Unrecognized input type token. |
| `DreamShader GraphFunction '{Name}' input '{Input}' uses Substrate, which is only supported by GraphFunction Graph blocks.` | A `Substrate` input reached the Custom-node path. |
| `DreamShader GraphFunction '{Name}' input '{Input}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | The same declaration on UE 5.3. |
| `DreamShader GraphFunction '{Name}' out argument {Index} must be a plain variable name.` | An out target that is not a bare identifier. |
| `DreamShader GraphFunction '{Name}' has an empty out target name.` | An out target that trims to nothing. |
| `DreamShader GraphFunction '{Name}' cannot write multiple out results into '{Target}' in the same call.` | The same out target used twice. |
| `DreamShader GraphFunction '{Name}' result '{Result}' was never assigned.` | The body did not produce the declared result — in practice, an empty body. |
| `DreamShader GraphFunction '{Name}' result '{Result}': {Error}` | The result value could not be coerced to its declared type. |
| `DreamShader GraphFunction '{Name}' result '{Result}' uses unsupported type '{Type}'.` | Unrecognized result type token. |
| `DreamShader GraphFunction '{Name}' result '{Result}' uses Substrate, which is only supported by GraphFunction Graph blocks.` | A `Substrate` result reached the Custom-node path. |
| `DreamShader GraphFunction '{Name}' result '{Result}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | The same declaration on UE 5.3. |
| `DreamShader GraphFunction '{Name}' has unsupported result type '{Type}'.` | The primary result type is outside the accepted set. |
| `Failed to create a Custom node for DreamShader GraphFunction '{Name}'.` | The node could not be created. |
| `DreamShader GraphFunction '{Name}' contains an unterminated UE.* call.` | A `UE.` call in the body has no closing `)`. |
| `DreamShader GraphFunction '{Name}' UE input '{Input}': {Error}` | A hoisted `UE.*` call in the body failed to parse or evaluate. |
| `DreamShader GraphFunction '{Name}' UE input '{Input}' cannot be passed into a Custom node input.` | A hoisted `UE.*` call produced a texture object, a `MaterialAttributes` value or a `Substrate` value. |

### `ShaderFunction` family and `VirtualFunction`

| Message | Cause |
| :-- | :-- |
| `VirtualFunction '{Name}' asset reference is invalid: {Error}` | `Options.Asset` did not resolve to a `UMaterialFunction`. |
| `{Kind} '{Name}' must declare at least one output.` | The declaration has an empty `Outputs` section. |
| `{Kind} '{Name}' could not load MaterialFunction asset '{Asset}'.` | The generated or referenced asset is missing. |
| `Failed to create a MaterialFunctionCall node for '{Name}'.` | The call node could not be created. |
| `Failed to assign material function '{Name}' to the generated call node.` | The loaded asset was rejected by the call node. |
| `{Kind} '{Name}' input arguments cannot mix positional and named forms.` | Both forms present in one value call. |
| `{Kind} '{Name}' input '{Input}' does not exist on MaterialFunction asset '{Asset}'.` | The declaration and the asset disagree about the inputs. |
| `{Kind} '{Name}' is missing required input '{Input}'.` | A non-`opt` input received no argument. |
| `{Kind} '{Name}' input '{Input}' is not optional and cannot use default.` | `default` passed for a required input. |
| `{Kind} '{Name}' input '{Input}': {Error}` | The argument failed to evaluate or to coerce. |
| `{Kind} '{Name}' input '{Input}' uses unsupported type '{Type}'.` | Unrecognized input type token. |
| `{Kind} '{Name}' input '{Input}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | A `Substrate` input on UE 5.3. |
| `{Kind} '{Name}' received {Got} positional input argument(s), but only {Declared} input(s) are declared.` | Too many positional arguments. |
| `{Kind} '{Name}' does not have an input named '{Argument}'.` | A named argument matching no declared input. |
| `{Kind} '{Name}' statement call requires an active Graph build context.` | A statement call outside a `Graph` build. |
| `{Kind} '{Name}' statement calls currently use positional arguments only.` | A named argument in the statement form. |
| `{Kind} '{Name}' expects output target arguments after its inputs, but got {Got} total argument(s) for {Outputs} output(s).` | Fewer arguments than declared outputs. |
| `{Kind} '{Name}' expects at most {Inputs} input argument(s) followed by {Outputs} output target(s), but got {Got} input argument(s).` | More leading arguments than declared inputs. |
| `{Kind} '{Name}' output argument {Index} must be a plain variable name.` | An out target that is not a bare identifier. |
| `{Kind} '{Name}' has an empty output target name.` | An out target that trims to nothing. |
| `{Kind} '{Name}' cannot write multiple outputs into '{Target}' in the same call.` | The same out target used twice. |
| `{Kind} '{Name}' output '{Output}' uses unsupported type '{Type}'.` | Unrecognized output type token. |
| `{Kind} '{Name}' output '{Output}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | A `Substrate` output on UE 5.3. |
| `{Kind} '{Name}' output '{Output}' does not exist on MaterialFunction asset '{Asset}'.` | The declaration and the asset disagree about the outputs. |
| `{Kind} '{Name}' cannot use OutputName/Output together with OutputIndex.` | Both selectors given. |
| `{Kind} '{Name}' OutputIndex is out of range.` | Not an integer literal, negative, or beyond the declared output count. |
| `{Kind} '{Name}' OutputName must be a literal value.` | `Output=`/`OutputName=` given a non-literal expression. |
| `{Kind} '{Name}' does not expose an output named '{Output}'.` | No declared output has that name. |
| `{Kind} '{Name}' exposes multiple outputs. Specify Output="Name" or OutputIndex=N.` | Value form with several outputs and no selector. |

### Parameter call forms

| Message | Cause |
| :-- | :-- |
| `Could not resolve parameter '{Name}' for configuration.` | The parameter node could not be materialized. |
| `Parameter '{Name}' did not produce an expression node.` | The parameter resolved to no node. |
| `Parameter '{Name}' must be called with named arguments wiring its input pins (e.g. {Name}(Coordinates=...) or {Name}(Input=...)).` | A positional argument in a parameter call. |
| `Parameter '{Name}' ({NodeType}) has no input pin named '{Argument}'. Asset slots (Texture/Curve/Font/...) are set via [{Argument}=Path(...)] metadata, not call arguments.` | The argument name matches no input pin on the node. |
| `Parameter '{Name}' input '{Argument}': {Error}` | The argument expression failed to evaluate. |
| `Parameter '{Name}' input '{Argument}' must be a numeric value.` | A texture object, `MaterialAttributes` or `Substrate` value passed to an input pin. |
| `StaticSwitchParameter '{Name}' requires True=... and False=... inputs.` | One or both branches missing. |
| `StaticSwitchParameter '{Name}' True input: {Error}` | The true branch failed to evaluate. |
| `StaticSwitchParameter '{Name}' False input: {Error}` | The false branch failed to evaluate. |
| `StaticSwitchParameter '{Name}' cannot switch Texture object values.` | A branch is a texture object. |
| `StaticSwitchParameter '{Name}' cannot switch Substrate values.` | A branch is a `Substrate` value. |
| `StaticSwitchParameter '{Name}' cannot mix MaterialAttributes and numeric branches.` | The two branches disagree on the attribute flag. |
| `StaticSwitchParameter '{Name}' branches must have the same component count, got {Left} and {Right}.` | The two branches have different widths. |
| `Failed to create StaticSwitchParameter node '{Name}'.` | The node could not be created. |
| `StaticSwitchParameter '{Name}': {Error}` | The parameter node could not be configured. |

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

        // Parameter pin call: wires the sampler's Coordinates pin.
        vec4 Tex = Albedo(Coordinates = UV);

        // Value form, single-output Function declared in Helpers.dsh.
        float L  = Luma(Tex.rgb);

        // Value form, namespaced Function.
        vec3 Lit = Common::ApplyTint(Tex.rgb, Tint);

        // Statement form: two out targets, inputs first. The targets need no declaration.
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
Custom  "PulseTint"                    -> Pulsed (output 0), Amount (output 1)
StaticSwitchParameter  UseTint         -> Color
```

The two additional output pins on the `PulseTint` Custom node are named after the **caller's** out
variables, not after the declared result names.

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
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
