# DreamShaderLang

> [DreamShader](../index.md) » **DreamShaderLang**

The language read from `.dsm`, `.dsf` and `.dsh` source files: an outer declaration grammar that
describes assets, and a separate expression grammar, embedded in `Graph` blocks, that describes the
node graph inside them.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` |
| Kind | language |
| Generates | `UMaterial`, `UMaterialFunction`, `UMaterialFunctionMaterialLayer`, `UMaterialFunctionMaterialLayerBlend` |
| Version | `1.5.1` |
| Engines | Unreal Engine `5.3` – `5.8`; the accepted grammar is identical on every supported engine version |

## Synopsis

A translation unit is one source file plus everything it imports.

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

DreamShaderLang is not parsed by one machine. Three independent stages process a source file, and
each has its own rules. **Do not infer the rules of one from another.**

| | Declaration grammar | `Graph` expression grammar |
| :-- | :-- | :-- |
| Implemented by | module `DreamShader` (Runtime) | module `DreamShaderEditor` (Editor) |
| Runs | when a source file is parsed | during material generation, over the text stored by the parser |
| Input | the whole prepared source text | the text between `Graph = {` and its matching `}` |
| Produces | the block / section tree, plus the `Graph` body as verbatim text | `UMaterialExpression` nodes and their connections |
| Sees the other's input | never — the declaration parser does not look inside `Graph` | never — the expression parser only sees the `Graph` body |
| Numeric literals | no numeric token; values are captured as raw text and converted late, tolerantly | a real number token with exponents and one optional type suffix |
| Comments | `//` and `/* */`; stripped again textually before statement splitting in `Properties`, `Settings`, `Outputs`, `Layout` and typed-parameter sections | `//` and `/* */`; comments survive into the stored body and are handled by the expression tokenizer |
| Statement separator | `;` at parenthesis and bracket depth 0 | see [Statements](../graph/statements.md) |
| Punctuation accepted | `{ } ( ) [ ] ; = , .` and `::` | `( ) , . + - * / =` and `::` only — every other character ends the expression |
| Error positions | messages ending in `near index <N>` carry a position | positions are line/column inside the `Graph` body, then offset onto the file |

`import` belongs to **neither** grammar. It is stripped line by line by the editor source loader
before the declaration parser is called, so it is not a keyword the parser knows. See
[`import`](import.md).

```text
<file>.dsm
  │
  ├─ editor source loader   strip `import` lines, inline targets depth-first,
  │                         enforce the .dsh / .dsf content rules
  │
  ├─ declaration parser     blocks, sections, statements, headers  ->  definition tree
  │                         `Graph` bodies stored as verbatim text
  │
  └─ material generator     Graph expression grammar  ->  material nodes  ->  asset
```

## Declaration language

| | |
| :-- | :-- |
| [Source files](source-files.md) | `.dsm` / `.dsf` / `.dsh`, what each may contain, discovery |
| [Lexical elements](lexical.md) | Comments, identifiers, case sensitivity, literals, statement splitting |
| [Keyword index](keywords.md) | Every reserved word, alias and identifier rewrite |
| [`import`](import.md) | Search roots, packages, cycles, line mapping |
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
| [Output bindings](output-bindings.md) | `Base.<Property> = <var>;` and `Expression( … ).Pin[<i>]` |
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

- **The same text can mean different things to the two grammars.** `float Strength = 1.0f;` in a
  `Properties` section parses only because the outer grammar converts the raw text `1.0f` with a
  tolerant numeric conversion that stops at the `f`; `1.0f` inside a `Graph` block is a genuine
  suffixed literal whose suffix is a lexical element. Both work; the reasons differ, and so do the
  edge cases. See [Numeric literals](lexical.md#numeric-literals).
- **There is no preprocessor at the declaration level.** `#Region` / `#EndRegion` are recognized only
  inside `Graph` bodies; a `#` anywhere else is not a directive. See [Layout](layout.md).
- **Top-level block keywords are the only case-sensitive tokens in the declaration grammar.** Section
  names, type tokens, attribute keys, settings keys and every other keyword-like token are matched
  case-insensitively. See [Case sensitivity](lexical.md#case-sensitivity).
- **The parser is engine-version independent.** No lexical or grammatical construct is gated on the
  Unreal Engine version. Version gates exist only downstream of parsing — Substrate types and
  builtins (UE 5.4+), and a small number of generator features.
- A parse unit must contain at least one `Shader`, `Function`, `GraphFunction`, `ShaderFunction`,
  `ShaderLayer`, `ShaderLayerBlend` or `VirtualFunction` block, otherwise it fails with
  `A top-level Shader, Function, GraphFunction, Namespace, ShaderFunction, ShaderLayer, ShaderLayerBlend, or VirtualFunction block was not found.`
  A `Namespace` satisfies this only through the functions it contains — an empty `Namespace` does not.

## Example

One `.dsm` exercising both grammars: the declaration grammar owns everything outside `Graph = { }`,
the expression grammar everything inside it.

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
- [`import`](import.md) — how a translation unit is assembled from several files
- [Graph](../graph/index.md) — the expression grammar
- [Diagnostics index](../diagnostics/index.md) — every message, by pipeline stage
- [Generation](../generation/index.md) — what the definition tree is turned into
- [Getting started](../getting-started.md) — a first `.dsm` end to end
