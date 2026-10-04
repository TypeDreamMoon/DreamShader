# Namespace

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Namespace**

A top-level block that prefixes the names of the `Function` and `GraphFunction` declarations it
contains with `<Name>::`.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` |
| Kind | top-level block |
| Generates | nothing directly — its members generate exactly what they would generate at top level |
| Multiplicity | any number per file; the same name may be opened more than once |

## Synopsis

```c
Namespace(Name = "<identifier>")
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

`Name` is the only attribute the block reads; any other key is parsed and silently ignored. Attribute
keys are matched case-insensitively, so `Namespace(name="Common")` works. The value may be quoted or
bare; a bare value runs to the next `,` or `)` outside parentheses. A key written twice is a warning
([`DSH2244`](../diagnostics/DSH2xxx.md#dsh2244)) and the later value wins. A trailing comma
before `)` is accepted, as in 1.x *(2.0.0 – 2.1.0 refused it with
[`DSH2243`](../diagnostics/DSH2xxx.md#dsh2243))*.

`Name` must be an identifier — a letter or `_`, then letters, digits and `_`. A missing, empty or
whitespace-only name, and a name containing `::`, `.`, `-`, a space or any other character, is
[`DSH6309`](../diagnostics/DSH6xxx.md#dsh6309). There is therefore **no multi-segment declaration
form**: `Namespace(Name="A::B")` is an error.

## Body contents

| Construct | Accepted |
| :-- | :-- |
| [`Function`](function.md) | yes, any number |
| [`GraphFunction`](graph-function.md) | yes, any number |
| Nested `Namespace` | **no** |
| `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `VirtualFunction` | **no** |
| Sections (`Properties`, `Settings`, `Inputs`, `Outputs`, `Graph`, `Layout`, …) | **no** |
| `import` | **no** *(since 2.0.0)* — an import stands at file scope only |

Members may appear in any order and any number of times; a stray `;` between them is skipped. Both
keywords are matched case-sensitively inside the body, exactly as at top level. Anything else is
[`DSH6310`](../diagnostics/DSH6xxx.md#dsh6310), and the parser resumes at the next member keyword or
the closing `}`.

### No nesting

`Namespace` is only reachable from the top-level keyword loop; the body recognizes just the two
function keywords. A nested `Namespace` therefore hits `DSH6310`. There is no way to declare
`A::B::C`, and re-opening does not compose:

```c
Namespace(Name="A") { Namespace(Name="B") { Function f(out float r) { r = 0; } } }
// DSH6310 at the inner Namespace
```

## Name flattening

A `Namespace` is not an entity. No object is stored for it, and it creates no scope. Its only effect
is on the member's name, which *(since 2.0.0)* is the flattened identifier:

```text
Namespace(Name = "Common") { Function ApplyTint(…) }   →   Common::ApplyTint   →   function Common_ApplyTint
```

The flattening is the identifier sanitizer: every non-`[A-Za-z0-9_]` character becomes `_`, and runs
of consecutive underscores collapse to one (`Common__ApplyTint` → `Common_ApplyTint`). Members land
among the top-level functions under that name; name lookup, binder diagnostics and the generated HLSL
symbol `DreamShaderFn_Common_ApplyTint` all see `Common_ApplyTint`. The qualified spelling survives in
one place: the title (`Description`) of the member's Custom node is `Common::ApplyTint`, as in 1.x.

> [!WARNING]
> Because `::` and `_` flatten to the same thing, `Namespace(Name="Common") { Function ApplyTint … }`
> and a top-level `Function Common_ApplyTint …` declare the same name:
> [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210).

## Calling a namespaced function

From a [`Graph`](../graph/index.md) block, use `Ns::Fn(…)`. The `::` chain is read as one name and
flattened the same way, so the call reaches `Ns_Fn`.

```c
Graph = {
    vec3 Tinted;
    Common::ApplyTint(BaseColor, Tint, Tinted);   // statement call
    float K = Common::Remap01(Raw);               // value call
}
```

| Rule | Behaviour |
| :-- | :-- |
| Resolution | The qualified name, or its flattened spelling `Common_ApplyTint`. There is no `using` directive and no unqualified fallback: a bare `ApplyTint(…)` is [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208). |
| Case | *(since 2.0.0)* names are case-sensitive. `common::applytint(…)` still resolves when it matches one function, with [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275). |
| Mangled spelling | `DreamShaderFn_Common_ApplyTint(…)` no longer names the function *(since 2.0.0)*: `DSH4208`. |
| Missing name | `Common::` with nothing after it is [`DSH5260`](../diagnostics/DSH5xxx.md#dsh5260). |
| Call forms | Identical to unqualified calls. See [Calling functions](../graph/calls.md). |

**Inside another `Function` or `GraphFunction` body** a `Ns::Fn(…)` call works *(since 2.0.0)*. Body
normalisation rewrites `Common::ApplyTint` to `Common_ApplyTint`, which is now the function's own name,
so the call is embedded like any other `Function` call. In 1.x it reached the shader compiler as an
undefined `Common_ApplyTint` and failed there. The qualifier has to be written without spaces around
`::`.

## Notes

- **Re-opening is allowed and unchecked.** Two `Namespace(Name="Common")` blocks — in the same file or
  across imported headers — both prefix `Common::`. There is no duplicate-namespace diagnostic; two
  members with the same qualified name are `DSH4210`.
- **A namespace does not create a lookup scope.** A member calls a sibling by its qualified (or
  flattened) name. Inside a body, a bare sibling name is not recognised as a call to the sibling and is
  left for the shader compiler.
- **`Namespace(Name="X") { }` with an empty body** is accepted and declares nothing *(since 2.0.0; 1.x
  then failed the file for having no block)*.
- **Headers are parsed on their own.** Namespaces of an imported header are declared into the
  importing file, and their members collide across files exactly as within one. See [import](import.md).
- Parser diagnostics about a member's declaration name the member as written (`ApplyTint`); the ones
  about a lifted `UE.*` call name it `Common::ApplyTint`; everything after the parser names the
  flattened `Common_ApplyTint`.

## Diagnostics

Each code carries the line and column of the construct; the code's page has the message.

### Parse time

| Code | Raised when |
| :-- | :-- |
| [`DSH2241`](../diagnostics/DSH2xxx.md#dsh2241) | no `(` after `Namespace` |
| `DSH2243` | a malformed attribute list |
| `DSH2244` | an attribute written twice (warning) |
| `DSH6309` | no `Name`, an empty one, or one that is not an identifier |
| [`DSH2257`](../diagnostics/DSH2xxx.md#dsh2257) | no `{` after the header |
| `DSH6310` | anything but `Function` / `GraphFunction` in the body — a nested `Namespace` included |
| [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) | the body is never closed |
| [`DSH2103`](../diagnostics/DSH2xxx.md#dsh2103) | the `Name` value's `"` is never closed |

Member declarations report their own errors — see [`Function` § Diagnostics](function.md#diagnostics).

### Binding and call time

| Code | Raised when |
| :-- | :-- |
| `DSH4210` | two members flatten to one name, or a member and a top-level function do |
| `DSH5260` | no name follows `::` |
| `DSH4208` | the called name declares nothing — for example an unqualified call to a member |
| `DSH5275` | the called name matches a member only in case (warning) |

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
        vec3 Gray = vec3(0.5, 0.5, 0.5);

        vec3 Tinted;
        Common::ApplyTint(Gray, Tint, Tinted);

        float Key = Common::Remap01(Tinted.r);

        float P;
        Common::Pulse(Speed, P);

        Color = Tinted * Key * P;
    }
}
```

Resulting names:

```text
declaration                     function name        Custom node title     helper symbol when embedded
Namespace "Common" > ApplyTint  Common_ApplyTint     Common::ApplyTint     DreamShaderFn_Common_ApplyTint
Namespace "Common" > Remap01    Common_Remap01       Common::Remap01       DreamShaderFn_Common_Remap01
Namespace "Common" > Pulse      Common_Pulse         Common::Pulse         (none — it lifts a UE.* call, so it is never embedded)
```

## See also

- [Function](function.md) — the member declaration grammar and the `DreamShaderFn_*` helper symbol
- [GraphFunction](graph-function.md) — the other legal member kind
- [Calling functions](../graph/calls.md) — value vs statement calls
- [Name resolution](../graph/name-resolution.md) — the lookup order a `Graph` block uses
- [import](import.md) — how a header's declarations reach the importing file
- [Source files](source-files.md) — which of `.dsm` / `.dsh` / `.dsf` may hold a `Namespace`
- [Lexical elements](lexical.md) — identifiers, `::`, and the case-sensitivity matrix
- [Keywords](keywords.md) — the complete keyword index
- [Diagnostics index](../diagnostics/index.md) — every code, by stage
