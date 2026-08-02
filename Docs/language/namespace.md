# Namespace

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Namespace**

A top-level block that prefixes the names of the `Function` and `GraphFunction` declarations it
contains with `<Name>::`.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` |
| Kind | top-level block |
| Generates | nothing directly — its members generate exactly what they would generate at top level |
| Multiplicity | any number per parse unit; the same name may be opened more than once |

## Synopsis

```c
Namespace(Name = "<identifier>" [,])
{
    { <function-declaration> | <graph-function-declaration> } …
}
```

The keyword `Namespace` is matched **case-sensitively**. `namespace` and `NAMESPACE` are not the
keyword.

## Header attributes

| Attribute | Required | Value | Effect |
| :-- | :-- | :-- | :-- |
| **`Name`** | yes | string | The qualifier prepended to every member's name |

`Name` is the only attribute the block reads; any other key is parsed into the attribute map and
silently ignored. Attribute keys are matched case-insensitively, so `Namespace(name="Common")` works.
The value may be quoted or bare; a bare value ends at the first `,` or `)`. A trailing comma before
`)` is accepted. A duplicate key silently overwrites the earlier one.

`Name` must be a valid identifier, validated character by character:

| Position | Accepted characters |
| :-- | :-- |
| first | `A`–`Z`, `a`–`z`, `_` |
| rest | `A`–`Z`, `a`–`z`, `0`–`9`, `_` |

An empty or whitespace-only name is rejected. A name containing `::`, `.`, `-`, a space, or any other
character fails with `Namespace name '{Name}' is not a valid identifier.` There is therefore **no
multi-segment declaration form**: `Namespace(Name="A::B")` is a syntax error.

## Body contents

| Construct | Accepted |
| :-- | :-- |
| [`Function`](function.md) | yes, any number |
| [`GraphFunction`](graph-function.md) | yes, any number |
| Nested `Namespace` | **no** |
| `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `VirtualFunction` | **no** |
| Sections (`Properties`, `Settings`, `Inputs`, `Outputs`, `Graph`, `Layout`, …) | **no** |
| `import` | not a member — the directive is stripped line by line before parsing and its target is inlined ahead of the whole file, so writing one inside a body has no scoping effect. Put imports at file scope. |

Members may appear in any order and any number of times. Both keywords are matched case-sensitively
inside the body, exactly as at top level. Anything else fails with
`Namespace '{Name}' may only contain Function or GraphFunction blocks.`

### No nesting

`Namespace` is only reachable from the top-level keyword loop; the body parser recognizes just the
two function keywords. A nested `Namespace` therefore hits the "may only contain" error above. There
is no way to declare `A::B::C`, and re-opening does not compose:

```c
Namespace(Name="A") { Namespace(Name="B") { Function f(out float r) { r = 0; } } }
// Namespace 'A' may only contain Function or GraphFunction blocks.
```

## Name flattening

A `Namespace` is not an entity. No object is stored for it, and it creates no scope. Its only effect
is on the member's recorded name:

```text
Namespace(Name = "Common") { Function ApplyTint(…) }   →   name "Common::ApplyTint"
```

Members land in the same flat declaration arrays as top-level functions. Everything downstream — name
lookup, diagnostics, symbol mangling — sees the single string `Common::ApplyTint`.

For a `Function`, that string is then sanitized into the generated HLSL symbol: every
non-`[A-Za-z0-9_]` character becomes `_`, and runs of consecutive underscores collapse to one.

```text
Common::ApplyTint   →   Common__ApplyTint   →   Common_ApplyTint
                    →   symbol DreamShaderFn_Common_ApplyTint
```

`GraphFunction` members are never emitted into the generated include and have no HLSL symbol.

> [!WARNING]
> Because `::` and `_` collapse to the same thing, `Namespace(Name="Common") { Function ApplyTint … }`
> and a top-level `Function Common_ApplyTint …` produce the same symbol. The include writer rejects
> the pair with `DreamShader Function '{Name}' collides with another generated helper symbol
> '{Symbol}'. Rename the Function or Namespace.`

## Calling a namespaced function

From a [`Graph`](../graph/index.md) block, use `Ns::Fn(…)`. The expression lexer emits a dedicated
token for `::`, which is what distinguishes it from the `.` member-access form, and the qualified
chain is re-joined into the exact string `Ns::Fn` before lookup.

```c
Graph = {
    vec3 Tinted;
    Common::ApplyTint(BaseColor, Tint, Tinted);   // statement call
    float K = Common::Remap01(Raw);               // value call
}
```

| Rule | Behaviour |
| :-- | :-- |
| Resolution | Members are reachable **only** by their fully-qualified name. There is no `using` directive, no import of names, and no unqualified fallback. |
| Case | The name comparison is case-insensitive over the whole qualified string, so `common::applytint(…)` resolves. |
| Mangled spelling | The generated symbol is also an accepted spelling, so `DreamShaderFn_Common_ApplyTint(…)` resolves to the same declaration. |
| Single `:` | A lone `:` is not a token — it terminates the expression and produces `Unexpected token '{Text}' in Graph expression.` |
| Missing name | `Common::` with nothing after it produces `Expected function name after '::'.` |
| Call forms | Identical to unqualified calls. See [Calling functions](../graph/calls.md). |

> [!WARNING]
> **A `Ns::Fn(…)` call written inside another `Function` or `GraphFunction` body does not resolve.**
> Every function body is passed through identifier normalisation, which rewrites the qualified token
> `Common::ApplyTint` to the sanitized identifier `Common_ApplyTint` before codegen ever inspects it.
> The codegen rewrite table is keyed on the DSL name `Common::ApplyTint` and on the mangled symbol
> `DreamShaderFn_Common_ApplyTint` — never on `Common_ApplyTint` — so no substitution occurs.
>
> Observable symptom: the generated `.ush` (or the Custom node's code) contains a call to an
> undefined `Common_ApplyTint(…)`, and the material fails shader compilation with an
> undeclared-identifier error naming that symbol. There is no DreamShader diagnostic.
>
> Workarounds: call the namespaced function from the `Graph` block and pass its result in as a
> parameter; declare the helper at top level and call it unqualified; or write the mangled symbol
> `DreamShaderFn_Common_ApplyTint(…)` directly in the body, which the normalizer leaves untouched
> because it contains no `::`.
>
> Calls from a `Graph` block are unaffected — `Graph` text is not normalized.

## Notes

- **Re-opening is allowed and unchecked.** Two `Namespace(Name="Common")` blocks — in the same file or
  across imported files — both prefix `Common::`. There is no duplicate-namespace diagnostic. Two
  members with the same qualified name are caught later, when the include is written.
- **A namespace does not create a lookup scope.** A member calling a sibling by its bare name is
  resolving against the whole parse unit, not against the namespace.
- **`Namespace(Name="X") { }` with an empty body parses but the file then fails.** An empty body
  contributes no `Function` and no `GraphFunction`, so a file with nothing else fails the
  parse-unit check `A top-level Shader, Function, GraphFunction, Namespace, ShaderFunction,
  ShaderLayer, ShaderLayerBlend, or VirtualFunction block was not found.`
- **The parse unit is the whole import closure.** Namespaces from imported headers are visible without
  any further declaration, and collide across files exactly as they would within one. See
  [import](import.md).
- The qualified name is what appears in every runtime diagnostic. `Function 'Common::ApplyTint' must
  declare at least one out parameter.` names the member, not the block.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this section.

### Parse time

| Message | Cause |
| :-- | :-- |
| `Namespace(Name="...") is required.` | the header has no `Name` attribute |
| `Namespace name cannot be empty.` | `Name=""`, or a value that is whitespace only |
| `Namespace name '{Name}' is not a valid identifier.` | an illegal character, including `::`, `.`, `-` and space |
| `Namespace '{Name}' may only contain Function or GraphFunction blocks.` | any other token in the body — including a nested `Namespace` |
| `Expected '{Char}' near index {Index}.` | the `(` or `{` opener is missing |
| `Unterminated block.` | the body `{` is never closed |
| `Expected ',' or ')' near index {Index}.` | malformed attribute list |
| `Unterminated string literal.` | the `Name` value's `"` is never closed |

Member declarations report their own parse errors under their qualified names — see
[`Function` § Diagnostics](function.md#diagnostics).

### Generation time

| Message | Cause |
| :-- | :-- |
| `DreamShader Function '{Name}' collides with another generated helper symbol '{Symbol}'. Rename the Function or Namespace.` | two names that sanitize to the same `DreamShaderFn_*` symbol |
| `DreamShader Function '{Name}' is declared more than once.` | two members with the same qualified name, ignoring case |

### Call time

| Message | Cause |
| :-- | :-- |
| `Expected function name after '::'.` | no identifier follows `::` |
| `Unknown Graph function '{Name}'.` | a value call whose name resolves to no declaration — for example an unqualified call to a namespaced member |
| `Graph expression statement '{Text}' is unsupported. Only DreamShader Function, GraphFunction, ShaderFunction, ShaderLayer, ShaderLayerBlend, or VirtualFunction calls may use statement syntax.` | a statement call whose name resolves to no declaration |
| `Graph call '{Name}' is ambiguous because multiple definitions use that name: {Kinds}.` | the qualified name matches more than one callable kind |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
// DShader/Lib/Common.dsh

Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result)
    {
        result = color * tint;
    }

    Function float Remap01(in float value)
    {
        return saturate(value * 0.5 + 0.5);
    }

    GraphFunction Pulse(in float speed, out float value)
    {
        value = sin(UE.Time() * speed);
    }
}
```

```c
import "Lib/Common.dsh"

Shader(Name="Materials/M_Common")
{
    Properties = {
        vec3  Tint  = vec3(1.0, 0.6, 0.2);
        float Speed = 2.0;
    }
    Outputs = { vec3 Color; Base.EmissiveColor = Color; }
    Graph = {
        vec3 Base = vec3(0.5, 0.5, 0.5);

        vec3 Tinted;
        Common::ApplyTint(Base, Tint, Tinted);

        float Key = Common::Remap01(Tinted.r);

        float P;
        Common::Pulse(Speed, P);

        Color = Tinted * Key * P;
    }
}
```

Resulting names and symbols:

```text
declaration                     recorded name        generated HLSL symbol
Namespace "Common" > ApplyTint  Common::ApplyTint    DreamShaderFn_Common_ApplyTint
Namespace "Common" > Remap01    Common::Remap01      DreamShaderFn_Common_Remap01
Namespace "Common" > Pulse      Common::Pulse        (none — GraphFunctions are not emitted)
```

## See also

- [Function](function.md) — the member declaration grammar and the generated `DreamShaderFn_*` symbol
- [GraphFunction](graph-function.md) — the other legal member kind
- [Calling functions](../graph/calls.md) — value vs statement calls, and cross-kind ambiguity
- [Name resolution](../graph/name-resolution.md) — the lookup order a `Graph` block uses
- [Generated HLSL](../generation/generated-hlsl.md) — the include, its symbols, and the collision checks
- [import](import.md) — why the parse unit is the whole import closure
- [Source files](source-files.md) — which of `.dsm` / `.dsh` / `.dsf` may hold a `Namespace`
- [Lexical elements](lexical.md) — identifiers, `::`, and the case-sensitivity matrix
- [Keywords](keywords.md) — the complete keyword index
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
