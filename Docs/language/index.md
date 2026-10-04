# DreamShaderLang

> [DreamShader](../index.md) » **DreamShaderLang**

The language read from `.dsm`, `.dsf` and `.dsh` source files: an outer declaration grammar that
describes assets, and a statement grammar, embedded in `Graph` blocks, that describes the node graph
inside them.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` |
| Kind | language |
| Generates | `UMaterial`, `UMaterialFunction`, `UMaterialFunctionMaterialLayer`, `UMaterialFunctionMaterialLayerBlend` |
| Version | the 1.x language, last released in `1.9.1`; built by the 2.0 compiler *(since 2.0.0)* |
| Engines | Unreal Engine `5.3` – `5.8`; the accepted grammar is identical on every supported engine version |

> [!NOTE]
> New sources are written in [DreamShaderLang 2.0](../language-v2/index.md) (`.dss`). A 1.x source
> keeps building through the same compiler, and [`dsc migrate`](../tools/migrate.md) rewrites one as
> `.dss`, proving the rewrite builds the same graph.

## Synopsis

A source file is one file plus the headers it imports.

```c
[ import "<specifier>" ; ]…

<top-level-block>…
```

```c
<top-level-block> := { Shader | ShaderFunction | ShaderLayer | ShaderLayerBlend
                     | MaterialLayer | MaterialLayerBlend | VirtualFunction | Namespace }
                     ( <attribute> = <value> [, …] ) { <section>… }
                   | { Function [ SelfContained | Inline ] | GraphFunction }
                     [<return-type>] <name> ( [<parameter>, …] ) { <HLSL> }

<section>          := <section-name> [=] { <statement>… } [;]
```

Top-level blocks may appear in any order and, apart from `Shader`, any number of times. There is no
separator between them.

## Two grammars

*(since 2.0.0)* One lexer turns the whole file into tokens, and one **legacy front end** reads both
grammars from them into the same tree the [2.0 parser](../language-v2/index.md) builds — so a 1.x
file then goes through the same binder, IR and emitter as a `.dss`. Through 1.9.x the two grammars
were two separate machines, the declaration parser in the runtime module and the `Graph` expression
parser in the generator, each with its own character scanner.

| | Declaration grammar | `Graph` statement grammar |
| :-- | :-- | :-- |
| Implemented by | the legacy front end in module `DreamShaderLang` | the same front end, over the 2.0 statement and expression parser |
| Runs | when a source is compiled, after the preprocessor | in the same parse, after every other section of the block |
| Input | the tokens of the whole file | the tokens between `Graph = {` and its `}` |
| Produces | 2.0 declarations: `#pragma material`, `uniform`s, the entry function or function asset, `#pragma layout` | the statements of that function's body |
| Numeric literals | number tokens; each section reads a token's text the way its consumer asks — see [Numeric literals](lexical.md#numeric-literals) | number tokens |
| Comments | `//` and `/* */`, anywhere between tokens | the same |
| Statement separator | `;` outside brackets | `;` — see [Statements](../graph/statements.md) |
| Punctuation accepted | `{ } ( ) [ ] ; = , . :` and `::` where a section's grammar names them | every operator of the 2.0 grammar is a token; what a 1.x `Graph` did not have is refused by name, [`DSH2200`](../diagnostics/DSH2xxx.md#dsh2200)–[`DSH2222`](../diagnostics/DSH2xxx.md#dsh2222) |
| Error positions | the line and column of the construct, in every section | the line and column of the construct |

The `#if` family belongs to **neither** grammar: it is handled line by line before the file is
lexed, so none of its keywords is one the parser knows. See [Preprocessor](preprocessor.md).
*(since 2.0.0)* [`import`](import.md) is a top-level declaration of the declaration grammar; through
1.9.x it too was a line-oriented directive, handled before parsing.

```text
<file>.dsm
  │
  ├─ preprocessor           evaluate `#if` / `#ifdef` / `#elif` / `#else` / `#endif` against the
  │                         define table; cut untaken branches, preserving the line count;
  │                         `Function` bodies pass through untouched
  │
  ├─ legacy front end       tokens -> blocks, sections, `Graph` statements -> the 2.0 tree;
  │                         what 1.x read silently and built wrong is an error (DSH2200–DSH2222)
  │
  ├─ binder                 names, types, calls; an `import`ed `.dsh` is preprocessed and parsed on
  │                         its own and its declarations are declared into this file; the legacy
  │                         rules apply to 1.x text and say so (DSH5275–DSH5292)
  │
  ├─ IR                     lowering, passes, validation
  │
  └─ emitter                material nodes  ->  asset
```

The front end and the binder report all of their errors rather than the first, and a stage with
errors stops the pipeline before the next one; the preprocessor stops at its first error.
The preprocessor runs **first**, per file, which is what lets an `#if` decide whether an `import` is
taken at all — and why a `#define` is local to the file that writes it.

## Declaration language

| | |
| :-- | :-- |
| [Source files](source-files.md) | `.dsm` / `.dsf` / `.dsh`, what each may contain, discovery |
| [Lexical elements](lexical.md) | Comments, identifiers, case sensitivity, literals, statement splitting |
| [Keyword index](keywords.md) | Every reserved word, alias and identifier rewrite |
| [`import`](import.md) | Search roots, packages, cycles |
| [Preprocessor](preprocessor.md) | The eight `#if` / `#define` directives and the define table *(since 1.9.0)* |
| [Types](types.md) | Type-token catalogue and per-context validity |

**Top-level blocks**

| | |
| :-- | :-- |
| [`Shader`](shader.md) | Generates a `UMaterial` |
| [`ShaderFunction`](shader-function.md) | Generates a `UMaterialFunction` |
| [`ShaderLayer` / `ShaderLayerBlend`](shader-layer.md) | Generates native material layer functions *(since 1.3.0)* |
| [`VirtualFunction`](virtual-function.md) | Declares an existing `UMaterialFunction` *(since 1.2.0)* |
| [`Function`](function.md) | Reusable HLSL helper |
| [`GraphFunction`](graph-function.md) | HLSL helper whose `UE.*` calls become node inputs *(since 1.3.1)* |
| [`Namespace`](namespace.md) | Groups helpers under `Ns::Name` |

**Sections**

| | |
| :-- | :-- |
| [`Properties`](properties.md) | Parameter, `const` and `UE.*` declarations, `Group("…")` scopes |
| [`Inputs` / `Outputs` / `Results`](inputs-outputs.md) | Typed parameters of material functions |
| [Output bindings](output-bindings.md) | `Base.<Property> = <value>;` and `Expression( … ).Pin[<i>]` |
| [`Settings`](../settings/index.md) | Material and material-function settings |
| [`Options`](options.md) | `VirtualFunction` asset binding |
| [`Layout`](layout.md) | `Node` / `Comment` placement and `#Region` |

## Graph language

The statement and expression language inside `Graph = { … }`.

| | |
| :-- | :-- |
| [Graph overview](../graph/index.md) | What a `Graph` block is and how it is evaluated |
| [Statements](../graph/statements.md) | Every statement form |
| [Declarations](../graph/declarations.md) | Variables, initialisers, scope |
| [Expressions and operators](../graph/expressions.md) | Precedence, associativity, compound assignment |
| [Literals](../graph/literals.md) | Numeric forms, suffixes, `true` / `false` |
| [Constructors](../graph/constructors.md) | `float3( … )`, `vec4( … )`, splatting |
| [Swizzles](../graph/swizzle.md) | Channel sets, reorder, repeat |
| [Conversions](../graph/conversions.md) | Coercion and component-count rules |
| [`if` / `else`](../graph/if.md) | Conditions and branch semantics |
| [`MaterialAttributes`](../graph/material-attributes.md) | Member writes and reads *(since 1.2.5)* |
| [Calls](../graph/calls.md) | Calling functions and parameter pins |
| [Name resolution](../graph/name-resolution.md) | Lookup order and shadowing |
| [Node reuse](../graph/node-reuse.md) | Common-subexpression deduplication |
| [Unsupported constructs](../graph/unsupported.md) | Loops, `return`, ternary, `%`, comparisons, indexing |

## Notes

- **The same token can be read two ways.** `1.0f` is one float token everywhere. A `Properties`
  default reads its text — a sign, digits, one `.`, an optional `f` — so `float Strength = 1.0f;` is
  fine, and `float Strength = 1e3;` is [`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254); a `Graph`
  expression reads the token itself, exponent and all. See
  [Numeric literals](lexical.md#numeric-literals).
- **The preprocessor is not part of either grammar.** Eight lowercase directives — `#if`, `#ifdef`,
  `#ifndef`, `#elif`, `#else`, `#endif`, `#define`, `#undef` — are evaluated over the raw text before
  parsing and never reach either grammar *(since 1.9.0)*. Outside those eight, a `#` line is passed
  through only where it has a claimant: anywhere inside a `Function` body (that is HLSL, `#include`
  included), and `#Region` / `#EndRegion` in any case. Anything else is `DSH1035`, mis-cased
  directives such as `#IF` among them. See [Preprocessor](preprocessor.md) and [Layout](layout.md).
- **Top-level block keywords and the lower-case reserved words are the case-sensitive tokens.**
  Section names, type tokens, attribute keys, settings keys and every other keyword-like token are
  matched case-insensitively. See [Case sensitivity](lexical.md#case-sensitivity).
- **The parser is engine-version independent.** No lexical or grammatical construct is gated on the
  Unreal Engine version. Version gates exist only downstream of parsing — Substrate types and
  builtins (UE 5.4+), and a small number of node and setting gates.
- A `.dsm` or `.dsf` must contain at least one `Shader`, `ShaderFunction`, `ShaderLayer`,
  `ShaderLayerBlend`, `Function`, `GraphFunction`, `Namespace` or `VirtualFunction` block, otherwise it
  is [`DSH2254`](../diagnostics/DSH2xxx.md#dsh2254). *(since 2.0.0)* A `Namespace` counts even when it
  is empty, and a `.dsh` may hold nothing but imports.

## Example

One `.dsm` exercising both grammars: the declaration grammar owns everything outside `Graph = { }`,
the statement grammar everything inside it.

```c
Shader(Name="Materials/M_Comments")
{
    Settings = {
        Domain       = "UI";        // trailing line comment
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = vec3(1.0, 1.0, 1.0); // inline comment inside the graph body
    }
}
```

## See also

- [Source files](source-files.md) — which block kinds each of `.dsm` / `.dsf` / `.dsh` may contain
- [Lexical elements](lexical.md) — tokens, case rules, literals, splitting
- [Keyword index](keywords.md) — every reserved word with the page that documents it
- [`import`](import.md) — how a header's declarations reach the file that imports it
- [Preprocessor](preprocessor.md) — cutting the declaration layer with `#if`, and the define table
- [Graph](../graph/index.md) — the statement grammar
- [DreamShaderLang 2.0](../language-v2/index.md) — the compiler pipeline, and the `.dss` language
- [`dsc migrate`](../tools/migrate.md) — 1.x sources to `.dss`
- [Diagnostics index](../diagnostics/index.md) — every code, by pipeline stage
- [Generation](../generation/index.md) — what the tree is turned into
- [Getting started](../getting-started.md) — a first `.dsm` end to end
