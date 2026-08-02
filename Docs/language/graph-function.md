# GraphFunction

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **GraphFunction**

A top-level block whose body is HLSL, but whose `UE.*` calls are lifted out of the text, built as real
material nodes, and wired into the generated Custom node as auto-named input pins.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` — and inside a [`Namespace`](namespace.md) body |
| Kind | top-level block |
| Generates | nothing by itself. Each call site generates one `UMaterialExpressionCustom` node plus the material expressions hoisted out of its body. |
| Since | `1.3.1` |

## Synopsis

```c
GraphFunction [ <return-type> ] <name> ( [ <parameter-list> ] )
{
    <hlsl-with-UE-calls>
}
```

```c
<parameter-list> ::= <parameter> [ , <parameter> ] …
<parameter>      ::= [ { in | out } ] <type-token> <parameter-name>
```

The keyword `GraphFunction` is matched **case-sensitively**. The parameter grammar, the `in` / `out`
qualifiers, the "at least one output" rule, the return-type form and its `return` lowering, the type
tokens, and the body's identifier normalisation are **identical to [`Function`](function.md)** — read
that page for all of them. This page documents only what differs.

## How a GraphFunction differs from a Function

| Aspect | `Function` | `GraphFunction` |
| :-- | :-- | :-- |
| `Inline` / `SelfContained` | accepted | **not accepted** — see below |
| Emitted into the generated `.ush` include | yes | **never** |
| Embeddable into a caller's wrapper struct | yes | no — only `Function` declarations are embeddable |
| `UE.*` calls in the body | left as literal HLSL text | evaluated as material expressions and passed in as Custom-node inputs |
| Body placement | a separate `DreamShaderFn_*` HLSL function | inlined directly into the calling Custom node's code |
| Recursion | detected only for `SelfContained` closures | detected at every call, direct or indirect |
| `Substrate` parameters and results | never | never (the diagnostics differ in wording) |
| Requires an active `Graph` build | no | yes |

> [!WARNING]
> `GraphFunction SelfContained Foo(…)` and `GraphFunction Inline Foo(…)` are not rejected as
> modifiers — the modifier branch is skipped for `GraphFunction`, so the token is consumed as a
> **return type** instead. Two symptoms follow:
>
> - with any `out` parameter, the parse fails: `Function 'Foo' has a return type and cannot also
>   declare out parameters. Use out parameters without a return type for multiple outputs.`
> - with no `out` parameter, the declaration parses and fails at generation:
>   `DreamShader GraphFunction 'Foo' has unsupported result type 'SelfContained'.`
>
> There is no self-contained mode for `GraphFunction`.

## `UE.*` hoisting

Before the Custom node's code is assembled, the body is scanned and every `UE.*` call is replaced by
the name of a generated input pin. The value that pin carries is the material expression the call
evaluates to.

A call is hoisted only when **all** of these hold, in order:

| Step | Condition |
| :-- | :-- |
| 1 | The character before the `U` is an identifier boundary — not `[A-Za-z0-9_]`, or start of text |
| 2 | The next three characters are `U`/`u`, `E`/`e`, `.` — the prefix is matched **case-insensitively**, so `UE.`, `ue.`, `Ue.` and `uE.` all trigger |
| 3 | The character after the `.` starts an identifier: `[A-Za-z_]`, then `[A-Za-z0-9_]` |
| 4 | After optional whitespace, the next character is `(` |
| 5 | A matching `)` exists (the search is string-aware) |

If step 1–4 fails, the text is copied through **verbatim and silently**. If step 5 fails, generation
stops with `DreamShader GraphFunction '{Name}' contains an unterminated UE.* call.`

The scan skips `//` comments, `/* */` comments, `"…"` strings and `'…'` character literals, so a
`UE.Time()` written inside a comment is not hoisted.

The extracted text — the whole `UE.Foo(…)` including its parentheses — is re-parsed as a
[Graph expression](../graph/expressions.md) and evaluated against a scope that contains the enclosing
`Graph` block's variables plus this call's arguments bound under their declared parameter names. Any
[`UE.*` builtin](../builtins/ue.md), including the generic
[`UE.Expression(…)`](../builtins/ue-expression.md), is therefore available.

> [!NOTE]
> The hoist consumes exactly `UE.Name( … )` up to the matching `)` — nothing more. A trailing swizzle
> or member access stays behind as HLSL text applied to the pin, so `UE.CameraVector().xy` becomes
> `_ds_<Fn>_UE0.xy` and is evaluated by the shader compiler, not by the graph builder. The pin
> therefore carries the builtin's full component count.

> [!WARNING]
> Only `UE.` is hoisted. `Substrate.*` calls are **not** recognized by the scan and are copied into
> the Custom node's HLSL verbatim, where `Substrate.Slab(…)` is not valid HLSL and the shader
> compiler rejects it. Substrate values cannot cross a Custom node boundary at all. Build Substrate
> material graphs in a [`ShaderFunction`](shader-function.md) or directly in the `Shader`'s
> [`Graph`](../graph/index.md).

### Values the hoist refuses

A hoisted expression must produce a plain numeric value. If the evaluated value is a texture object,
a `MaterialAttributes` value, or a Substrate material, generation fails with
`DreamShader GraphFunction '{Name}' UE input '{CallText}' cannot be passed into a Custom node input.`

`{CallText}` in that message is the source text of the offending call, not the pin name.

## Generated input pins

Each hoisted call adds one `FCustomInput` to the Custom node. The pin name is derived, then
deduplicated:

| Step | Rule |
| :-- | :-- |
| 1 | Base name is `__ds_<SanitizedFunctionName>_UE<N>`, where `N` is a counter that starts at **0 for each call site** and increments in the source order of the body scan |
| 2 | The whole base name is identifier-sanitized: non-`[A-Za-z0-9_]` → `_`, then **runs of consecutive underscores are collapsed to one** — which turns the leading `__` into a single `_` |
| 3 | If the result is empty, it becomes `__ds_input` |
| 4 | While the name collides with an existing input name (compared case-insensitively, seeded with every declared `in` parameter name), `_1`, `_2`, … is appended |

So a `GraphFunction WindPulse` whose body contains one `UE.Time()` call produces a pin literally
named:

```text
_ds_WindPulse_UE0
```

and a namespaced `Common::Pulse` produces `_ds_Common_Pulse_UE0`.

> [!NOTE]
> The visible pin name has a **single** leading underscore. `__ds_` appears in the pre-sanitisation
> base name only; the underscore-collapsing step in the identifier sanitizer removes one of them.
> These pin names are user-visible on the generated Custom node and in any shader-compiler error that
> mentions them.

The declared `in` parameters occupy the first pins, in declaration order, and keep their declared
names unsanitized. Hoisted pins follow.

## Generated Custom-node code

```hlsl
<Result0Type> <Result0Name> = (<Result0Type>)0;
<Result1Type> <Result1Name> = (<Result1Type>)0;
…
<body, with every UE.* call replaced by its pin name>
<CallerOutTarget1> = <Result1Name>;
…
return <Result0Name>;
```

| Element | Rule |
| :-- | :-- |
| Result declarations | one per declared `out` result, in declaration order, zero-initialized with a cast |
| Body | the normalized body text; a newline is appended if it does not already end with one |
| Writeback | results 1..n are copied into the **caller's** out-target variable names |
| Return | result 0 is the Custom node's return value |
| Additional output pins | named after the **caller's** out-target variables, not the declared result names |
| Trailing-return insertion | not applied here — the `return` is always emitted explicitly |

Because the body is inlined into this code, a `return expr;` written inside a nested `if { … }` in
the body is not lowered and returns from the Custom node itself, skipping the writeback lines and the
final `return`. Only depth-0 `return`s are rewritten. See
[`Function` § return lowering](function.md#return-lowering).

The assembled code is then passed through the same embedding pass every Custom node gets, with no
embedding roots requested: plain `Function` calls in the body are rewritten to their
`DreamShaderFn_*` symbols and the include is attached, and any `SelfContained` callee is embedded in
a wrapper struct.

> [!NOTE]
> Variables that a `GraphFunction` call introduces into its local scope are propagated back to the
> enclosing `Graph` scope only when the name matches a declared property. Everything else stays local
> to the call.

## Restrictions

| Restriction | Consequence |
| :-- | :-- |
| No `Inline` / `SelfContained` | consumed as a return type — see the warning above |
| No named arguments, in either call form | `DreamShader GraphFunction '{Name}' currently uses positional arguments only.` |
| No recursion, direct or indirect | `GraphFunction cycle detected: {A -> B -> A}.` — names compared case-insensitively |
| Must be called from a `Graph` block | `GraphFunction call requires an active Graph build context.` / `GraphFunction value call requires an active Graph build context.` |
| Result type must resolve to 1–4 components or `MaterialAttributes` | `DreamShader GraphFunction '{Name}' has unsupported result type '{Token}'.` |
| No `Substrate` inputs or results | `… uses Substrate, which is only supported by GraphFunction Graph blocks.` (UE 5.4+) or `… requires Unreal Engine 5.4 or newer.` (UE 5.3) |
| Body must not be empty | an empty `{ }` body assigns nothing, so generation fails with `DreamShader GraphFunction '{Name}' result '{Result}' was never assigned.` unless a variable of that name already exists in the calling `Graph` scope |
| Never reachable from HLSL | a `GraphFunction` has no generated symbol; another function's body cannot call it |

### Argument validation

Both call forms validate before any node is created:

| Form | Rules |
| :-- | :-- |
| Value call, `x = Fn(a);` *(since 1.3.1)* | exactly one declared output; argument count must equal the input count |
| Statement call, `Fn(a, OutX, OutY);` | argument count must equal inputs + results; every out target must be a plain non-empty variable name; targets must be distinct within the call (compared case-insensitively) |

A value call is sugar: it appends a synthetic out target named `__ds_<SanitizedName>_value<N>` (this
one is a `Graph` variable name and keeps both underscores) and then performs a statement call. If
that produces no value, generation fails with
`DreamShader GraphFunction '{Name}' did not produce a value result.`

## Notes

- A `.dsm` or `.dsf` that contains *only* `GraphFunction` blocks generates no assets and reports
  `DreamShader file '{File}' contains GraphFunction declarations only; no assets were generated.`
  Unlike `Function`, there is no include to write.
- Lookup accepts the mangled spelling too, so `DreamShaderFn_WindPulse(…)` resolves to the
  `GraphFunction` in a `Graph` block even though no such HLSL symbol is ever generated.
- If a `Function` and a `GraphFunction` share a name, the call is ambiguous:
  `Graph call '{Name}' is ambiguous because multiple definitions use that name: {Kinds}.`
- On UE 5.3 the generated Custom node displays its code in the material graph; from UE 5.4 onward
  `ShowCode` is set to `false`. From UE 5.6 onward the additional output pins are rebuilt through the
  engine's own Custom-node API rather than being hand-built.
- The hoist evaluates each `UE.*` call independently. Two textually identical calls in one body
  produce two pins, though the underlying material expressions may still be shared by
  [node reuse](../graph/node-reuse.md).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this section. Parse-time messages are
shared with [`Function`](function.md#parse-time); only the four below name `GraphFunction` in their
text.

### Parse time

| Message | Cause |
| :-- | :-- |
| `GraphFunction declaration is missing a valid function name.` | the token after `GraphFunction` is not an identifier |
| `GraphFunction declaration is missing a function name after the return type '{Token}'.` | a return type not followed by an identifier |
| `GraphFunction '{Name}' is missing a valid parameter list. {Inner}` | the `( … )` block could not be extracted |
| `GraphFunction '{Name}' is missing a valid body block. {Inner}` | the `{ … }` block could not be extracted |

Every other declaration diagnostic — invalid parameter, unsupported qualifier, `__return`, return
type plus `out`, no output, bare `return;` — is emitted with the literal word `Function`, even for a
`GraphFunction`. See [`Function` § Diagnostics](function.md#diagnostics).

### Generation time

| Message | Cause |
| :-- | :-- |
| `GraphFunction call requires an active Graph build context.` | statement call outside a `Graph` block |
| `GraphFunction value call requires an active Graph build context.` | value call outside a `Graph` block |
| `GraphFunction cycle detected: {Path}.` | direct or indirect recursion; `{Path}` is the active call stack joined by ` -> ` |
| `DreamShader GraphFunction '{Name}' has {N} outputs and must be called with explicit out variables, for example {Name}(..., ResultA, ResultB).` | value-call form on a multi-output function |
| `DreamShader GraphFunction '{Name}' returns one value and expects {N} input argument(s) when used as a value expression, but got {M}.` | wrong argument count in a value call |
| `DreamShader GraphFunction '{Name}' currently uses positional arguments only.` | a `Key = Value` argument was passed |
| `DreamShader GraphFunction '{Name}' did not produce a value result.` | the synthesized value target was never assigned |
| `DreamShader GraphFunction '{Name}' must declare at least one out result.` | statement call on a function with no results |
| `DreamShader GraphFunction '{Name}' expects {N} arguments ({I} inputs, {O} out targets) but got {M}.` | wrong argument count in a statement call |
| `DreamShader GraphFunction '{Name}' out argument {N} must be a plain variable name.` | an out target that is not a bare identifier (index is 1-based) |
| `DreamShader GraphFunction '{Name}' has an empty out target name.` | an empty out target |
| `DreamShader GraphFunction '{Name}' cannot write multiple out results into '{Target}' in the same call.` | the same target named twice |
| `DreamShader GraphFunction '{Name}' input '{Param}': {Inner}` | the argument expression failed to evaluate or coerce |
| `DreamShader GraphFunction '{Name}' input '{Param}' uses unsupported type '{Token}'.` | the declared parameter type does not resolve |
| `DreamShader GraphFunction '{Name}' input '{Param}' uses Substrate, which is only supported by GraphFunction Graph blocks.` | `Substrate` parameter, UE 5.4+ |
| `DreamShader GraphFunction '{Name}' input '{Param}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` parameter, UE 5.3 |
| `DreamShader GraphFunction '{Name}' has unsupported result type '{Token}'.` | a result type outside the float1–4 / `MaterialAttributes` set |
| `DreamShader GraphFunction '{Name}' result '{Result}' uses unsupported type '{Token}'.` | as above, reported per result |
| `DreamShader GraphFunction '{Name}' result '{Result}' uses Substrate, which is only supported by GraphFunction Graph blocks.` | `Substrate` result, UE 5.4+ |
| `DreamShader GraphFunction '{Name}' result '{Result}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` result, UE 5.3 |
| `DreamShader GraphFunction '{Name}' result '{Result}': {Inner}` | the result value failed to publish |
| `DreamShader GraphFunction '{Name}' result '{Result}' was never assigned.` | reached with an empty body |
| `DreamShader GraphFunction '{Name}' contains an unterminated UE.* call.` | no matching `)` after a `UE.*(` |
| `DreamShader GraphFunction '{Name}' UE input '{CallText}': {Inner}` | the hoisted call failed to parse or evaluate |
| `DreamShader GraphFunction '{Name}' UE input '{CallText}' cannot be passed into a Custom node input.` | the hoisted value is a texture object, `MaterialAttributes`, or a Substrate material |
| `Failed to create a Custom node for DreamShader GraphFunction '{Name}'.` | node creation failed |
| `DreamShader file '{File}' contains GraphFunction declarations only; no assets were generated.` | informational success |

`{Name}` is the fully-qualified name — `Ns::Fn` inside a [`Namespace`](namespace.md).

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
// DShader/Lib/Motion.dsh

GraphFunction WindPulse(in float2 uv, out float pulse)
{
    float t = UE.Time();
    pulse = sin(uv.x * 8.0 + t);
}

GraphFunction float Parallax(in float2 uv, in float depth)
{
    float2 view = UE.CameraVector().xy;
    return uv.x + view.x * depth;
}
```

Used from a `Shader`:

```c
import "Lib/Motion.dsh"

Shader(Name="Materials/M_Wind")
{
    Properties = { float Depth = 0.25; }
    Outputs    = { vec3 Color; Base.EmissiveColor = Color; }
    Graph = {
        vec2  UV = UE.TexCoord(Index = 0);
        float Pulse;
        WindPulse(UV, Pulse);
        float Shift = Parallax(UV, Depth);
        Color = vec3(Pulse, Shift, 0.0);
    }
}
```

Generated material for the `WindPulse` call:

```text
UMaterialExpressionTime                    →  Custom pin "_ds_WindPulse_UE0"
UMaterialExpressionTextureCoordinate       →  Custom pin "uv"
UMaterialExpressionCustom  Description="WindPulse"  OutputType=CMOT_Float1
```

with `Code`:

```hlsl
float pulse = (float)0;
float t = _ds_WindPulse_UE0;
pulse = sin(uv.x * 8.0 + t);
return pulse;
```

## See also

- [Function](function.md) — the shared declaration grammar, parameters, return type, and body normalisation
- [Namespace](namespace.md) — `Ns::Fn` qualification, and why `::` calls fail inside a body
- [Calling functions](../graph/calls.md) — value vs statement calls, ambiguity across callable kinds
- [UE builtins](../builtins/ue.md) — every `UE.*` call the hoist can evaluate
- [UE.Expression](../builtins/ue-expression.md) — the generic reflected-node builtin
- [Substrate builtins](../builtins/substrate.md) — why `Substrate.*` cannot be hoisted
- [Graph](../graph/index.md) — the statement and expression language of a `Graph` block
- [Node reuse](../graph/node-reuse.md) — when two identical hoisted expressions share a node
- [Generated HLSL](../generation/generated-hlsl.md) — the include `GraphFunction`s are excluded from
- [Type tokens](types.md) — the complete token catalogue
- [ShaderFunction](shader-function.md) — the alternative when a real node graph, or Substrate, is needed
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
