# Diagnostics index

> [DreamShader](../index.md) » **Diagnostics**

Every message the DreamShader parser, generator, commandlet and VirtualFunction sync can emit,
grouped by the pipeline stage that produces it. Messages raised by the preview renderer, the
decompiler, the workspace service and the Material Content Browser's own actions are not compile
diagnostics and are documented on their tool pages instead.

| | |
| :-- | :-- |
| Produced by | `DreamShader` (parser, runtime module) and `DreamShaderEditor` (generator, bridge, tools) |
| Log category | `LogDreamShader` |
| Severity | every stored diagnostic is `error` — the store has no warning, info or hint level |
| Bridge artifacts | `<Project>/Saved/DreamShader/Bridge/diagnostics.json`, `.../Bridge/diagnostics/`, `.../Bridge/bridge.db` |

## Where diagnostics appear

| Surface | What it shows | Implemented by |
| :-- | :-- | :-- |
| **Output Log** | the raw compile message, at `Error` on failure and `Display` on success, under `LogDreamShader` | plugin |
| **Material Content Browser ▸ Dream Shader Gen page** | the per-source-file diagnostic list, read back from `diagnostics.json` | plugin |
| **`Bridge/diagnostics.json`** | `{ version, updatedAtUtc, files[] }`, one entry per source file | plugin |
| **`Bridge/diagnostics/`** | one `<md5-of-normalized-path>.json` shard per file plus `index.json`; stale shards are deleted on every write | plugin |
| **`Bridge/bridge.db`** | SQLite table `diagnostics(path, json, updated_at_utc)`, replaced wholesale in one transaction | plugin |
| **VSCode / Rider extension squiggles** | rendered from the three bridge artifacts above | **extension side**, not the plugin |

All three bridge sinks are written together on every diagnostics update, so they never disagree. The
Output Log is a separate path and is the only surface that shows *success* messages.

Diagnostics are owned by the source file that produced them. Recompiling `A.dsm` clears exactly the
records `A.dsm` produced — including records attributed to an imported `.dsh` — without disturbing
another material's diagnostics for the same header. See [Bridge](../tools/bridge.md).

> [!NOTE]
> The bridge never runs inside a commandlet. `-run=DreamShader`, `-run=Cook` and any other commandlet
> process writes no `diagnostics.json`, no shards and no `bridge.db` rows; the messages exist only in
> the log. The bridge is also suppressed by `-NoDreamShaderEditorBridge`. See
> [Commandlet](../tools/commandlet.md).

## Severity

`FDreamShaderDiagnosticRecord::Severity` defaults to `"error"` and is never assigned any other value
anywhere in the plugin. Consequences worth knowing:

- Parse **warnings** (deprecated spellings, the missing-`Outputs` warning) never enter the store. They
  are appended to the compile result message and surface in the Output Log only.
- Log-only warnings (`UE_LOG(LogDreamShader, Warning, …)`) likewise never enter the store.
- An extension that colours diagnostics by severity will paint every DreamShader entry as an error.

The Gen page nevertheless tolerates a missing or non-`error` severity by filtering rather than
failing, so a future severity level would not break it.

## Message locations

When a diagnostic carries a position it is formatted MSVC-style:

```text
I:/Project/DShader/Materials/M_Sample.dsm(37,9): Unknown Graph identifier 'Tin'.
```

| Stage | How the location is obtained |
| :-- | :-- |
| Top-level parse | the message itself ends in `near index {Index}`; the index is mapped back through the prepared (import-inlined) source to a real file, line and column |
| `Graph` block | the block's recorded start offset plus the statement's block-relative line/column; the column is offset only on the block's first line |
| Material compile | the engine's own `<path>(<line>,<col>): ` prefix, re-parsed and re-attributed |
| Everything else | no position — the message falls back to `<file>: <message>` at line 1, column 1 |

> [!WARNING]
> Section-body scanners are constructed over the *section body substring*, so a `near index {Index}`
> raised inside a `Properties`, `Settings`, `Outputs` or `Layout` body is body-local while the mapper
> treats it as a global prepared-source index. Line and column numbers reported for in-section parse
> errors are therefore wrong. The file is correct; the position is not. Locate the statement by the
> text quoted in the message instead.

## Reading these tables

- Runtime substitutions are shown as `{Placeholder}` throughout this page. A brace run that is
  followed by `...` — as in `Graph = { ... }` — is literal message text, not a placeholder.
- Messages are quoted **verbatim**, including the ones with inconsistent punctuation.
- Each table is sorted alphabetically by message so it works as a lookup. Rows whose message begins
  with a placeholder sort last.
- A trailing `{Detail}` means the message wraps an inner diagnostic; look the inner text up in its own
  stage table.
- `{File}` is always the normalized absolute path of the `.dsm` / `.dsf` / `.dsh` being compiled.
- `{Kind}` is `ShaderFunction`, `ShaderLayer` or `ShaderLayerBlend` — the deprecated `MaterialLayer` /
  `MaterialLayerBlend` spellings never appear in a diagnostic.

---

## Parse

Lexical scanning, top-level block headers, and the editor's import inliner. These run before any
section body is examined; a failure here aborts the whole file.

| Message | Cause | Fix | See |
| :-- | :-- | :-- | :-- |
| `A function with a return type cannot use a bare 'return;'. Return a value, e.g. 'return expr;'.` | a top-level `return;` inside a `Function` that declares a return type | return a value, or drop the return type and use `out` parameters | [Function](../language/function.md) |
| `A top-level Shader, Function, GraphFunction, Namespace, ShaderFunction, ShaderLayer, ShaderLayerBlend, or VirtualFunction block was not found.` | the parse unit declared no recognized top-level block; an empty `Namespace` body also lands here | add a top-level block, or check that the keyword's case is exact | [Keywords](../language/keywords.md) |
| `DreamShader could not read '{File}'.` | an imported file exists but `LoadFileToString` failed | check file locks and encoding | [import](../language/import.md) |
| `DreamShader function file '{File}' may only declare imports, Function/Namespace/GraphFunction/VirtualFunction blocks, and ShaderFunction/ShaderLayer/ShaderLayerBlend blocks.` | a `.dsf` whose text contains the substring `Shader(` anywhere — comments and string literals included | move the `Shader` block to a `.dsm`, or reword the comment | [Source files](../language/source-files.md) |
| `DreamShader header '{File}' may only declare Function/Namespace/GraphFunction/VirtualFunction blocks and imports.` | a `.dsh` whose text contains `Shader(`, `ShaderFunction(`, `ShaderLayer(`, `ShaderLayerBlend(`, `MaterialLayer(` or `MaterialLayerBlend(` anywhere | move the block to a `.dsf` or `.dsm` | [Source files](../language/source-files.md) |
| `DreamShader import cycle detected at '{File}'.` | a file re-enters its own import stack | break the cycle; diamond imports are fine, cycles are not | [import](../language/import.md) |
| `DreamShader import '{Specifier}' referenced from '{File}' could not be resolved.` | none of the three candidate roots contained the specifier, or a candidate escaped its containment root | check the extension (a specifier with no extension implies `.dsh`) and that the target is under `DShader` or `DShader/Packages` | [import](../language/import.md) |
| `Expected ',' or ')' near index {Index}.` | malformed header attribute list | separate attributes with `,`; a trailing `,` before `)` is allowed | [Shader](../language/shader.md) |
| `Expected '{' near index {Index}.` | a block body was expected | add the `{ … }` body | [Source files](../language/source-files.md) |
| `Expected '{Char}' near index {Index}.` | a delimited region (`(` for a parameter list, `{` for a body) did not open where required | add the delimiter | [Function](../language/function.md) |
| `Expected identifier near index {Index}.` | an identifier was expected — attribute key, section name, block name | identifiers are `[A-Za-z_][A-Za-z0-9_]*` | [Lexical elements](../language/lexical.md) |
| `Expected value near index {Index}.` | an attribute key was followed by `=` and then nothing | supply a value; an unquoted value ends at the first `,` or `)` | [Shader](../language/shader.md) |
| `Function '{Name}' has a return type and cannot also declare out parameters. Use out parameters without a return type for multiple outputs.` | a return-typed `Function` also declared `out` | pick one form | [Function](../language/function.md) |
| `Function '{Name}' has an invalid parameter declaration '{Parameter}'.` | the parameter did not split into 2 or 3 whitespace-separated tokens, or its type or name was empty | write `[in\|out] <Type> <Name>` | [Function](../language/function.md) |
| `Function '{Name}' has an invalid return type '{Type}'.` | the return-type token normalized to the empty string | use a real type token | [Types](../language/types.md) |
| `Function '{Name}' must declare at least one out parameter.` | no `out` parameter and no return type | add an `out` parameter or a return type | [Function](../language/function.md) |
| `Function '{Name}' parameter '{Parameter}' uses unsupported qualifier '{Qualifier}'. Supported qualifiers are in and out.` | a qualifier other than `in` / `out` — `inout` included | use `in` or `out` | [Function](../language/function.md) |
| `Function '{Name}' parameter name '__return' is reserved for return-type lowering.` | a user parameter named `__return` (matched ignoring case) | rename the parameter | [Function](../language/function.md) |
| `Function declaration is missing a valid function name.` | the token after `Function` is not an identifier | supply a name | [Function](../language/function.md) |
| `Function declaration is missing a valid function name after SelfContained.` | `Function SelfContained(` or `Function Inline(` | supply a name after the modifier | [Function](../language/function.md) |
| `GraphFunction declaration is missing a valid function name.` | the token after `GraphFunction` is not an identifier | supply a name; note that `GraphFunction` accepts no `SelfContained` / `Inline` modifier | [GraphFunction](../language/graph-function.md) |
| `Namespace '{Name}' may only contain Function or GraphFunction blocks.` | any other token in a `Namespace` body, including a nested `Namespace` | move the block out; namespaces do not nest | [Namespace](../language/namespace.md) |
| `Namespace name '{Name}' is not a valid identifier.` | the name contains an illegal character | use `[A-Za-z_][A-Za-z0-9_]*` | [Namespace](../language/namespace.md) |
| `Namespace name cannot be empty.` | `Namespace(Name="")` | supply a name | [Namespace](../language/namespace.md) |
| `Namespace(Name="...") is required.` | the header has no `Name` attribute | add `Name="…"` | [Namespace](../language/namespace.md) |
| `Only one top-level Shader block is currently supported.` | a second `Shader` keyword in the parse unit — enforced across the whole transitive import closure, not per file | split into separate `.dsm` files | [Shader](../language/shader.md) |
| `Shader must provide a Graph block.` | a `Shader` with an empty `Code` and no initialized output declaration | add `Graph = { … }`, or initialize an output declaration | [Shader](../language/shader.md) |
| `Shader(Name="...") is required.` | the `Shader` header has no `Name` attribute | add `Name="…"` | [Shader](../language/shader.md) |
| `Unexpected token near index {Index}.` | no top-level keyword matched at this position; an `import` line handed straight to the parser also lands here | check keyword spelling and case — top-level keywords are the only case-**sensitive** tokens in the language | [Keywords](../language/keywords.md) |
| `Unterminated block.` | EOF reached before a `}` closed | balance the braces | [Lexical elements](../language/lexical.md) |
| `Unterminated string literal.` | EOF reached inside a quoted attribute value | close the `"` | [Lexical elements](../language/lexical.md) |
| `Unterminated '{Char}' block.` | EOF reached before the matching delimiter of a generic delimited block, e.g. an unclosed `(` parameter list | balance the delimiters | [Function](../language/function.md) |
| `VirtualFunction '{Name}' must declare at least one output.` | the block declared no `Outputs` / `Results` entry | add an output | [VirtualFunction](../language/virtual-function.md) |
| `VirtualFunction '{Name}' must provide Options = { Asset = Path(...); }.` | neither the header `Asset=` attribute nor `Options.Asset` supplied a non-empty asset | add one of them | [VirtualFunction](../language/virtual-function.md) |
| `VirtualFunction name cannot be empty.` | `VirtualFunction(Name="")` | supply a name | [VirtualFunction](../language/virtual-function.md) |
| `VirtualFunction(Name="...") is required.` | the header has no `Name` attribute | add `Name="…"` | [VirtualFunction](../language/virtual-function.md) |
| `{Block}(Name="...") is required.` | a `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` / `MaterialLayer` / `MaterialLayerBlend` header has no `Name`; `{Block}` echoes the spelling actually typed | add `Name="…"` | [ShaderFunction](../language/shader-function.md) |
| `{Kind} '{Name}' is missing a valid body block. {Detail}` | the `{ … }` body of a `Function` / `GraphFunction` could not be extracted | balance the braces | [Function](../language/function.md) |
| `{Kind} '{Name}' is missing a valid parameter list. {Detail}` | the `( … )` parameter list could not be extracted | balance the parentheses | [Function](../language/function.md) |
| `{Kind} declaration is missing a function name after the return type '{Type}'.` | a return type was read but no name followed | supply a name | [Function](../language/function.md) |

---

## Sections and declarations

Section dispatch, the `Outputs` section's own grammar, `Layout`, `#Region`, `Group(…)` scopes, and
the post-parse validation of output declarations and bindings.

| Message | Cause | Fix | See |
| :-- | :-- | :-- | :-- |
| `Base.FrontMaterial requires Unreal Engine 5.4 or newer.` | `Base.FrontMaterial` bound on UE 5.3 | remove the binding, or move to UE 5.4+ | [Output bindings](../language/output-bindings.md) |
| `Expression output target '{Target}' has an invalid pin index.` | the `.Pin[…]` index is not a non-negative integer | use `.Pin[0]`, `.Pin[1]`, … | [Output bindings](../language/output-bindings.md) |
| `Expression output target '{Target}' must select a pin with .Pin[index].` | the text after `)` does not start with `.` | append `.Pin[<index>]` | [Output bindings](../language/output-bindings.md) |
| `Expression output target '{Target}' must specify Class="...".` | the `Expression( … )` argument list has no `Class` | add `Class="MaterialExpressionName"` | [Output bindings](../language/output-bindings.md) |
| `Expression output target '{Target}' must use .Pin[index] syntax.` | the suffix after `)` is not `Pin[ … ]` | use exactly `.Pin[<index>]` | [Output bindings](../language/output-bindings.md) |
| `Expression output target argument '{Argument}' must use Key=Value syntax.` | a positional argument inside `Expression( … )` | every argument must be `Key=Value` | [Output bindings](../language/output-bindings.md) |
| `Expression output target argument '{Key}' is declared more than once.` | duplicate argument key after normalization | remove the duplicate | [Output bindings](../language/output-bindings.md) |
| `Graph #EndRegion on line {Line} has no matching #Region.` | an unbalanced `#EndRegion` | remove it or add the opening directive | [Layout](../language/layout.md) |
| `Graph #Region '{Name}' is missing #EndRegion.` | a region left open at the end of the `Graph` body; the innermost open region is reported | close the region | [Layout](../language/layout.md) |
| `Graph #Region on line {Line} must include a name.` | `#Region` with a blank name | write `#Region "Name"` or `#Region Name` | [Layout](../language/layout.md) |
| `Group(...) requires a non-empty name.` | `Group("")` | supply a name | [Properties](../language/properties.md) |
| `Invalid Layout Comment statement '{Statement}'. {Detail}` | a `Comment( … )` call failed argument validation | supply `Name`, `X`, `Y`, `W`, `H`; `Color` is optional | [Layout](../language/layout.md) |
| `Invalid Layout Node statement '{Statement}'. {Detail}` | a `Node( … )` call failed argument validation | supply `Var`, `X`, `Y` | [Layout](../language/layout.md) |
| `Invalid Layout argument '{Argument}'.` | empty key or empty value in a `Layout` call | supply both sides | [Layout](../language/layout.md) |
| `Invalid Layout statement '{Statement}'.` | the statement is not a balanced `Name( … )` call | fix the parentheses | [Layout](../language/layout.md) |
| `Invalid Layout statement name in '{Statement}'.` | the call name is not a bare identifier | use `Node` or `Comment` | [Layout](../language/layout.md) |
| `Invalid expression output target argument '{Argument}'.` | empty key or value inside `Expression( … )` | supply both sides | [Output bindings](../language/output-bindings.md) |
| `Invalid output binding '{Statement}'.` | a binding statement whose right-hand side is empty | supply a source variable or expression | [Output bindings](../language/output-bindings.md) |
| `Invalid output declaration initializer '{Statement}'.` | an initialized output declaration whose right-hand side is empty | supply an initializer | [Output bindings](../language/output-bindings.md) |
| `Invalid output expression target '{Target}'.` | `Expression` was not followed by a balanced `( … )` | fix the parentheses | [Output bindings](../language/output-bindings.md) |
| `Invalid typed declaration '{Statement}'.` | the left side does not split into `<Type> <Name>`, or the name is not an identifier. **A tab between the type and the name fails here** — this splitter looks for a literal space | replace the tab with a space | [Inputs / Outputs / Results](../language/inputs-outputs.md) |
| `Layout Comment Color must be a float4 literal in '{Statement}'.` | `Color=` is not a four-component literal | write `Color=float4(r, g, b, a)` | [Layout](../language/layout.md) |
| `Layout argument '{Argument}' must use Key=Value syntax.` | a positional argument in a `Layout` call | use `Key=Value` | [Layout](../language/layout.md) |
| `Layout argument '{Key}' is declared more than once.` | duplicate argument key | remove the duplicate | [Layout](../language/layout.md) |
| `Layout argument '{Name}' is required.` | a required argument is absent | supply it | [Layout](../language/layout.md) |
| `Layout argument '{Name}' must be an integer.` | `X` / `Y` / `W` / `H` is not an integer | use an integer | [Layout](../language/layout.md) |
| `Output binding target '{Target}' is empty.` | `Base.` with nothing after it | name a material property | [Output bindings](../language/output-bindings.md) |
| `Output binding target '{Target}' must start with Base. for material outputs or Expression(...) for output nodes.` | a binding target that is neither form | use `Base.<Property>` or `Expression( … ).Pin[i]` | [Output bindings](../language/output-bindings.md) |
| `Output binding target cannot be empty.` | the left side of a binding is empty | supply a target | [Output bindings](../language/output-bindings.md) |
| `Output variable '{Name}' is bound to incompatible material properties.` | one variable bound to two `Base.*` targets of different types | split into two variables | [Output bindings](../language/output-bindings.md) |
| `Output variable '{Name}' is declared as '{Type}' but bound material property '{Property}' expects a different type.` | the declared type and the target's type disagree | change the declaration to match the target | [Output bindings](../language/output-bindings.md) |
| `Output variable '{Name}' is declared with conflicting types.` | the same output name declared twice with different types | remove one declaration | [Output bindings](../language/output-bindings.md) |
| `Output variable '{Name}' must declare an explicit type before binding to expression target '{Target}'.` | a variable bound to `Expression( … ).Pin[i]` with no declaration | declare the variable in `Outputs` first | [Output bindings](../language/output-bindings.md) |
| `Output '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | a `Substrate`-typed output declaration on UE 5.3 | remove it, or move to UE 5.4+ | [Substrate](../builtins/substrate.md) |
| `Outputs declarations cannot use the reserved name 'return'.` | an `Outputs` declaration named `return` (ignoring case) | rename it | [Output bindings](../language/output-bindings.md) |
| `Parameter node type '{Type}' is recognized but not supported as a plain Properties declaration yet. Use UE.{Type}(OutputType="float4", ...) for reflected node creation.` | a known expression-class token that has no `Properties` declaration form | declare it as a `UE.*` builtin property instead | [Parameter nodes](../parameters/parameter-nodes.md) |
| `Shader graph sections now use Graph = { ... }. Function Code = { ... } is still supported.` | a `Code` section inside `Shader` | rename it to `Graph` | [Shader](../language/shader.md) |
| `ShaderFunction, ShaderLayer, and ShaderLayerBlend graph sections now use Graph = { ... }. Function Code = { ... } is still supported.` | a `Code` section inside a material-function block | rename it to `Graph` | [ShaderFunction](../language/shader-function.md) |
| `The return value is bound to material properties with incompatible types.` | `return` bound to two `Base.*` targets of different types | bind `return` once | [Output bindings](../language/output-bindings.md) |
| `The reserved output name 'return' can only bind to Base material properties.` | `return` bound to an `Expression( … ).Pin[i]` target | bind a named variable instead | [Output bindings](../language/output-bindings.md) |
| `Unexpected '{' in Properties near '{Text}'. Only Group("Name") { ... } may open a brace here.` | a `{` inside `Properties` that is not a `Group("Name")` head | remove the brace, or write a proper `Group("Name") { … }` | [Properties](../language/properties.md) |
| `Unexpected text after Layout statement '{Statement}'.` | text after the closing `)` of a `Layout` call | end the statement at `)` | [Layout](../language/layout.md) |
| `Unknown Layout statement '{Name}'.` | a call other than `Node` or `Comment` | use `Node` or `Comment` | [Layout](../language/layout.md) |
| `Unknown VirtualFunction section '{Section}'.` | a section other than `Inputs` / `Properties` / `Outputs` / `Results` / `Options` / `Settings` | remove it; `Graph`, `Code` and `Layout` are not accepted here | [VirtualFunction](../language/virtual-function.md) |
| `Unknown material function section '{Section}'.` | a section other than `Properties` / `Inputs` / `Outputs` / `Results` / `Settings` / `Graph` / `Layout` / `Code` | check the spelling; `Options` is not accepted here | [ShaderFunction](../language/shader-function.md) |
| `Unknown shader section '{Section}'.` | a section other than `Properties` / `Settings` / `Outputs` / `Graph` / `Layout` / `Code` | check the spelling; `Inputs`, `Results` and `Options` are not accepted in a `Shader` | [Shader](../language/shader.md) |
| `Unsupported material output '{Name}'.` | the name after `Base.` is not a recognized material property | consult the target catalogue | [Output bindings](../language/output-bindings.md) |
| `Unexpected characters after UE builtin argument list in '{Token}'.` | trailing text after the closing `)` of a `UE.*` property declaration | end the declaration at `)` | [Properties](../language/properties.md) |
| `Unknown shader function section '{Name}'.` | a section name a `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` body does not accept | use `Properties`, `Inputs`, `Outputs`, `Settings`, `Graph` or `Layout` | [ShaderFunction](../language/shader-function.md) |
| `Unsupported output target '{Target}'.` | the text before `(` is not exactly `Expression` | use `Expression( … )` | [Output bindings](../language/output-bindings.md) |
| `Unsupported output type '{Type}' for '{Name}'.` | an `Outputs` declaration type that does not resolve | use a supported type token | [Types](../language/types.md) |
| `Unterminated Group("{Name}") { ... } block.` | a `Group` scope left unclosed | balance the braces | [Properties](../language/properties.md) |
| `VirtualFunction declares an existing MaterialFunction asset and does not support Graph or Code sections.` | a `Graph` or `Code` section inside `VirtualFunction` | remove it — a `VirtualFunction` only *declares* an existing asset | [VirtualFunction](../language/virtual-function.md) |

---

## Graph statements and expressions

The statement/expression language inside `Graph = { … }`, plus constructors, swizzles and coercion.
Parse-stage messages here are usually wrapped in `In Graph statement '{Statement}': {Detail}`.

| Message | Cause | Fix | See |
| :-- | :-- | :-- | :-- |
| `AppendVector cannot build {Count} components; Unreal material vectors support at most 4.` | a swizzle or constructor asked for more than four components | reduce the component count | [Swizzle](../graph/swizzle.md) |
| `AppendVector inputs must be numeric scalar/vector values.` | a texture, `MaterialAttributes` or `Substrate` value fed into vector assembly | use numeric values | [Constructors](../graph/constructors.md) |
| `Arithmetic operators cannot be applied to MaterialAttributes values.` | `+ - * /` on a `MaterialAttributes` value | read a member first | [MaterialAttributes](../graph/material-attributes.md) |
| `Arithmetic operators cannot be applied to Substrate values.` | `+ - * /` on a `Substrate` value | compose with `Substrate.*` nodes | [Substrate](../builtins/substrate.md) |
| `Arithmetic operators cannot be applied to texture values.` | `+ - * /` on a texture object | sample the texture first | [Expressions](../graph/expressions.md) |
| `Brace initializer assignment for '{Name}' requires a declared scalar or vector target type.` | `{ … }` assigned to a name with no resolvable numeric type | declare the variable's type | [Declarations](../graph/declarations.md) |
| `Brace initializer assignment is not supported for Substrate output '{Name}'.` | `{ … }` assigned to a `Substrate` output | assign a `Substrate.*` value | [Substrate](../builtins/substrate.md) |
| `Brace initializer assignment is not supported for Substrate variable '{Name}'.` | `{ … }` assigned to a `Substrate` variable | assign a `Substrate.*` value | [Substrate](../builtins/substrate.md) |
| `Brace initializer assignment is not supported for texture output '{Name}'.` | `{ … }` assigned to a texture output | assign a texture object | [Declarations](../graph/declarations.md) |
| `Brace initializer assignment is not supported for texture variable '{Name}'.` | `{ … }` assigned to a texture variable | assign a texture object | [Declarations](../graph/declarations.md) |
| `BreakMaterialAttributes does not expose member '{Member}'.` | a `MaterialAttributes` member read that the Break node has no output for | use a member the engine exposes | [MaterialAttributes](../graph/material-attributes.md) |
| `Cannot build an empty vector.` | vector assembly with no components | supply components | [Constructors](../graph/constructors.md) |
| `Channel {Index} is invalid for a value with {Count} components.` | internal guard in the single-channel mask helper: the requested channel is not one that the value's *existing* channel mask exposes. An ordinary out-of-range swizzle is rejected before this point and reports `Swizzle '{Swizzle}' is invalid for a value with {Count} components.` instead | report as a bug if observed | [Swizzle](../graph/swizzle.md) |
| `Constructor '{Name}' cannot use MaterialAttributes arguments.` | a `MaterialAttributes` value passed to a constructor | read a member first | [Constructors](../graph/constructors.md) |
| `Constructor '{Name}' cannot use Substrate arguments.` | a `Substrate` value passed to a constructor | use `Substrate.*` composition | [Constructors](../graph/constructors.md) |
| `Constructor '{Name}' cannot use Texture2D arguments.` | a texture object passed to a constructor | sample the texture first | [Constructors](../graph/constructors.md) |
| `Constructor '{Name}' does not accept named arguments.` | `float3(x = 1)` | constructors are positional only | [Constructors](../graph/constructors.md) |
| `Constructor '{Name}' expects a single scalar input.` | a splat constructor given a non-scalar | pass one scalar, or the exact component count | [Constructors](../graph/constructors.md) |
| `Constructor '{Name}' expects {Expected} total components but got {Actual}.` | the argument widths do not add up | adjust the arguments | [Constructors](../graph/constructors.md) |
| `Empty Graph expression.` | an expression position with no tokens | supply an expression | [Expressions](../graph/expressions.md) |
| `Encountered a Graph assignment without a target variable.` | a statement such as `= 5;` | supply a target | [Statements](../graph/statements.md) |
| `Encountered an invalid empty Graph statement.` | a statement that is neither an expression, a declaration nor a brace initializer | remove the stray `;` | [Statements](../graph/statements.md) |
| `Expected 'if'.` | internal guard while measuring an `if` statement's extent | report as a bug if observed | [if](../graph/if.md) |
| `Expected a float{N}-style literal like '(...)' but got '{Text}'.` | a vector-valued position was given text that is not a parenthesised component list | write `(a, b, c)` or a constructor | [Literals](../graph/literals.md) |
| `Expected a MaterialAttributes value.` | coercion target is `MaterialAttributes`, source is not | assign a `MaterialAttributes` value | [Conversions](../graph/conversions.md) |
| `Expected a scalar literal but got '{Text}'.` | a scalar-valued position was given text that does not parse as a number | supply a numeric literal, or reference a declared property | [Literals](../graph/literals.md) |
| `Expected a Substrate value.` | coercion target is `Substrate`, source is not | assign a `Substrate.*` result | [Conversions](../graph/conversions.md) |
| `Expected a texture object value with a matching texture type.` | a texture of the wrong dimension | match `Texture2D` / `TextureCube` / `Texture2DArray` / `VolumeTexture` | [Conversions](../graph/conversions.md) |
| `Expected a texture object value.` | coercion target is a texture object, source is not | assign a texture property or `UE.*` texture value | [Conversions](../graph/conversions.md) |
| `Expected function name after '::'.` | `Namespace::` with no identifier | supply the function name | [Namespace](../language/namespace.md) |
| `Expected member name after '.'.` | `value.` with no identifier | supply a swizzle or member name | [Swizzle](../graph/swizzle.md) |
| `Expected token type {Type} in Graph expression near '{Text}'.` | a missing `,` or `)` in a call argument list | balance the argument list | [Calls](../graph/calls.md) |
| `Expected {Expected} component(s) but got {Actual}.` | a numeric coercion of the wrong width | widen, narrow or splat explicitly | [Conversions](../graph/conversions.md) |
| `Expected {Expected} components but got {Actual} in literal '{Text}'.` | a vector literal whose component count does not match the expected width | write the right number of components | [Literals](../graph/literals.md) |
| `Failed to assign Graph member '{Name}'. {Detail}` | a `MaterialAttributes` member write failed | see the inner message | [MaterialAttributes](../graph/material-attributes.md) |
| `Failed to compose swizzle channel mask.` | the component-mask node could not be built | report as a bug | [Swizzle](../graph/swizzle.md) |
| `Failed to connect '{Name}' as the SetMaterialAttributes base value.` | the Set node's base input could not be wired | report as a bug | [MaterialAttributes](../graph/material-attributes.md) |
| `Failed to connect MaterialAttributes member '{Member}'.` | the member input could not be wired | report as a bug | [MaterialAttributes](../graph/material-attributes.md) |
| `Failed to create a BreakMaterialAttributes node.` | node creation failed | report as a bug | [MaterialAttributes](../graph/material-attributes.md) |
| `Failed to create a MakeMaterialAttributes node.` | node creation failed | report as a bug | [MaterialAttributes](../graph/material-attributes.md) |
| `Failed to create a Material If node.` | node creation failed | report as a bug | [if](../graph/if.md) |
| `Failed to create a SetMaterialAttributes node.` | node creation failed | report as a bug | [MaterialAttributes](../graph/material-attributes.md) |
| `Failed to create a StaticBool node for literal '{Literal}'.` | node creation failed for `true` / `false` | report as a bug | [Literals](../graph/literals.md) |
| `Failed to create a constant float{N} node for constructor '{Name}'.` | constant folding failed | report as a bug | [Constructors](../graph/constructors.md) |
| `Failed to create a default literal node.` | the implicit zero for a bare declaration could not be created | report as a bug | [Declarations](../graph/declarations.md) |
| `Failed to create a zero literal for Graph if condition.` | the implicit `0` of a truthy condition could not be created | report as a bug | [if](../graph/if.md) |
| `Failed to create an AppendVector node.` | node creation failed | report as a bug | [Swizzle](../graph/swizzle.md) |
| `Failed to declare Graph variable '{Name}'. {Detail}` | wrapper around a declaration failure | see the inner message | [Declarations](../graph/declarations.md) |
| `Failed to evaluate Graph assignment for '{Name}'. {Detail}` | wrapper around a right-hand-side failure | see the inner message | [Statements](../graph/statements.md) |
| `Failed to evaluate Graph if condition. {Detail}` | one side of the condition failed to evaluate | see the inner message | [if](../graph/if.md) |
| `Graph builder is not initialized.` | a build API was used with no active graph context | report as a bug | [Graph](../graph/index.md) |
| `Graph calls must target a named function.` | the callee expression does not flatten to a name | call a named function | [Calls](../graph/calls.md) |
| `Graph expression statement '{Statement}' is ambiguous because multiple callable definitions exist.` | the name resolves to more than one callable kind | rename one of them, or qualify with a namespace | [Name resolution](../graph/name-resolution.md) |
| `Graph expression statement '{Statement}' is unsupported. Only DreamShader Function, GraphFunction, ShaderFunction, ShaderLayer, ShaderLayerBlend, or VirtualFunction calls may use statement syntax.` | a statement-form call to something else | assign the result to a variable instead | [Calls](../graph/calls.md) |
| `Graph expression statements currently support only Function calls with explicit out arguments.` | a bare expression statement that is not a call — `break;`, `a++;`, a lone identifier | remove it or turn it into an assignment | [Unsupported constructs](../graph/unsupported.md) |
| `Graph expression statements must call a named Function.` | the callee is not a name | call a named function | [Calls](../graph/calls.md) |
| `Graph if branches assign incompatible values to '{Name}'. {Detail}` | both branches assign the name, but the values cannot be merged | make the widths and kinds match | [if](../graph/if.md) |
| `Graph if branches assign variable '{Name}' with inconsistent types` | the two branch values differ in kind or width *(message has no trailing period)* | make the branch types match | [if](../graph/if.md) |
| `Graph if branches cannot mix MaterialAttributes and numeric values.` | one branch yields attributes, the other a number | make both the same kind | [if](../graph/if.md) |
| `Graph if condition is empty.` | `if ()` | supply a condition | [if](../graph/if.md) |
| `Graph if condition left side must evaluate to a scalar value.` | a vector on the left of the comparison | use a scalar or a swizzle | [if](../graph/if.md) |
| `Graph if condition right side must evaluate to a scalar value.` | a vector on the right of the comparison | use a scalar | [if](../graph/if.md) |
| `Graph if statement cannot select Substrate value '{Name}'.` | a `Substrate` value merged across branches | select before building the Substrate value | [if](../graph/if.md) |
| `Graph if statement cannot select texture value '{Name}'.` | a texture object merged across branches | select the sampled result instead | [if](../graph/if.md) |
| `Graph if statement could not resolve both branch values for '{Name}'.` | a name assigned in only one branch — branch-local declarations leak into the merge | assign the name in both branches, or declare it before the `if` | [if](../graph/if.md) |
| `Graph if statement failed to merge '{Name}'. {Detail}` | the `If` node could not be wired | see the inner message | [if](../graph/if.md) |
| `Graph if statement has an unterminated body block.` | unbalanced `{` in the then-body | balance the braces | [if](../graph/if.md) |
| `Graph if statement has an unterminated condition block.` | unbalanced `(` | balance the parentheses | [if](../graph/if.md) |
| `Graph if statement is missing a '{ ... }' body.` | no `{` after the condition — single-statement bodies are not accepted | wrap the body in braces | [if](../graph/if.md) |
| `Graph if statement is missing a condition block.` | no `(` after `if` | add a parenthesized condition | [if](../graph/if.md) |
| `Graph else statement has an unterminated body block.` | unbalanced `{` in the else-body | balance the braces | [if](../graph/if.md) |
| `Graph else statement is missing a '{ ... }' body.` | `else` followed by neither `{` nor `if` | wrap the body in braces | [if](../graph/if.md) |
| `Graph output variable '{Name}' was assigned an incompatible value. {Detail}` | an assignment to an `Outputs` name that does not coerce to its declared type | match the declared type | [Conversions](../graph/conversions.md) |
| `Graph variable '{Name}' is declared as '{Type}' but assigned an incompatible value. {Detail}` | the initializer does not coerce to the declared type | change one of them | [Conversions](../graph/conversions.md) |
| `Graph variable '{Name}' is declared more than once.` | a redeclaration; the lookup is case-**insensitive**, so `Tint` and `tint` collide | rename one | [Declarations](../graph/declarations.md) |
| `Graph variable '{Name}' is not a MaterialAttributes value.` | a member write to a non-attributes variable | declare it as `MaterialAttributes` | [MaterialAttributes](../graph/material-attributes.md) |
| `Graph variable '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | a `Substrate` declaration on UE 5.3 | remove it, or move to UE 5.4+ | [Substrate](../builtins/substrate.md) |
| `Graph variable '{Name}' was previously assigned an incompatible value. {Detail}` | a reassignment whose shape differs from the variable's first value | match the original shape | [Conversions](../graph/conversions.md) |
| `Graph variable type '{Type}' requires an explicit initializer.` | a bare declaration of a texture or `Substrate` type — only scalars and vectors default-initialize | supply an initializer | [Declarations](../graph/declarations.md) |
| `In Graph else body: {Detail}` | wrapper injected after the location prefix of a failure inside an `else` body | see the inner message | [if](../graph/if.md) |
| `In Graph if body: {Detail}` | wrapper injected after the location prefix of a failure inside a then-body | see the inner message | [if](../graph/if.md) |
| `In Graph if condition '{Condition}': {Detail}` | one side of the condition failed to parse | see the inner message | [if](../graph/if.md) |
| `In Graph statement '{Statement}': '{Token}' is not a valid declarator in a comma-separated declaration.` | a declarator that is not a bare identifier | use identifiers; all declarators share the first declarator's type | [Declarations](../graph/declarations.md) |
| `In Graph statement '{Statement}': {Detail}` | statement-level wrapper | see the inner message | [Statements](../graph/statements.md) |
| `In output expression '{Expression}': {Detail}` | an `Outputs` binding source failed to evaluate | see the inner message | [Output bindings](../language/output-bindings.md) |
| `Initializer '{Text}' is not a valid brace initializer.` | the `{ … }` could not be read | balance the braces; nested brace initializers are not supported | [Statements](../graph/statements.md) |
| `Integer division is not supported by the material graph; use float() or floor(a/b).` | `int(a) / int(b)` | cast to float, or use `floor(a/b)` | [Expressions](../graph/expressions.md) |
| `Invalid Graph else body in '{Statement}'.` | malformed else-body braces | balance the braces | [if](../graph/if.md) |
| `Invalid Graph if body in '{Statement}'.` | malformed then-body braces | balance the braces | [if](../graph/if.md) |
| `Invalid Graph if statement '{Statement}'.` | malformed condition parentheses | balance the parentheses | [if](../graph/if.md) |
| `Invalid MaterialAttributes member assignment target '{Target}'.` | the `A.B` split failed | write `<Variable>.<Member>` | [MaterialAttributes](../graph/material-attributes.md) |
| `Invalid brace initializer for type '{Type}'. {Detail}` | the desugared constructor call failed | see the inner message | [Constructors](../graph/constructors.md) |
| `Invalid numeric literal '{Literal}'.` | a `Graph` number token that does not convert to a `double`. The number lexer consumes digits, `.` and one `e`/`E` exponent greedily and the converter stops at the first character it cannot use, so a stray extra decimal point never reaches this message — `0.5.5` converts to `0.5`. What does reach it is an exponent that underflows to zero from a non-zero mantissa, such as `1e-9999` | write a value the `double` range can represent | [Literals](../graph/literals.md) |
| `MaterialAttributes member '{Member}' cannot be assigned from Graph code.` | a member the Make/Set nodes do not expose for writing | write a different member | [MaterialAttributes](../graph/material-attributes.md) |
| `MaterialAttributes member '{Member}' cannot be read as a numeric value.` | a non-numeric member read | read a numeric member | [MaterialAttributes](../graph/material-attributes.md) |
| `MaterialAttributes member '{Member}' does not have a numeric scalar/vector type.` | the member is not numeric | write a numeric member | [MaterialAttributes](../graph/material-attributes.md) |
| `MaterialAttributes member '{Member}' expects {Count} component(s). {Detail}` | the assigned value has the wrong width | match the member's width | [MaterialAttributes](../graph/material-attributes.md) |
| `MaterialAttributes member assignment '{Target}' requires a value.` | `Attrs.BaseColor = ;` | supply a value | [MaterialAttributes](../graph/material-attributes.md) |
| `MaterialAttributes values cannot be assigned to numeric outputs.` | attributes assigned to a numeric target | read a member first | [Conversions](../graph/conversions.md) |
| `Operator '{Op}' requires matching vector sizes or a scalar/vector pair, got {A} and {B} component(s).` | mismatched operand widths | swizzle or splat one operand | [Expressions](../graph/expressions.md) |
| `Output declaration '{Name}' has an empty initializer.` | an `Outputs` default value that is empty | supply an initializer | [Output bindings](../language/output-bindings.md) |
| `Output declaration initializer requires a type and name.` | the synthesized declaration had a blank type or name | fix the `Outputs` declaration | [Output bindings](../language/output-bindings.md) |
| `String literals can only be used in named UE builtin arguments.` | a `"…"` used as a value | strings are only valid as `Key="Value"` arguments to `UE.*` | [UE.Expression](../builtins/ue-expression.md) |
| `Substrate requires Unreal Engine 5.4 or newer.` | a `Substrate` value type on UE 5.3 | move to UE 5.4+ | [Substrate](../builtins/substrate.md) |
| `Substrate values cannot be assigned to numeric outputs.` | a `Substrate` value assigned to a numeric target | bind it to `Base.FrontMaterial` | [Substrate](../builtins/substrate.md) |
| `Substrate values cannot be selected by Graph if statements.` | a `Substrate` value across `if` branches | branch earlier | [if](../graph/if.md) |
| `Substrate values do not support swizzle/member access in Graph.` | `.rgb` on a `Substrate` value | use `Substrate.*` utilities | [Substrate](../builtins/substrate.md) |
| `Swizzle '{Swizzle}' is invalid for a value with {Count} components.` | the swizzle names a channel the value does not have | widen the value or shorten the swizzle | [Swizzle](../graph/swizzle.md) |
| `Texture objects cannot be assigned to numeric outputs.` | a texture object assigned to a numeric target | sample it first | [Conversions](../graph/conversions.md) |
| `Texture values cannot be selected by Graph if statements.` | a texture object across `if` branches | branch on the sampled result | [if](../graph/if.md) |
| `Texture values do not support swizzle/member access in Code.` | `.rgb` on a texture object | sample the texture first | [Swizzle](../graph/swizzle.md) |
| `Unexpected text after Graph if statement: '{Text}'.` | trailing text after the statement | move it to its own statement | [if](../graph/if.md) |
| `Unexpected token '{Token}' in Graph expression.` | tokens left over after a complete expression, or a non-primary token in primary position | remove the token. **Characters the lexer does not know never reach this message** — see [Silent behaviour](#silent-behaviour) | [Unsupported constructs](../graph/unsupported.md) |
| `Unknown Graph identifier '{Name}'.` | the name is not a variable, a property, an output, or `true` / `false` | check the spelling; a bare read of a `StaticSwitchParameter` also lands here — it must be called | [Name resolution](../graph/name-resolution.md) |
| `Unknown MaterialAttributes variable '{Name}'.` | a member write to an undeclared name | declare the variable first | [MaterialAttributes](../graph/material-attributes.md) |
| `Unsupported Graph expression kind.` | an expression node the evaluator does not handle | report as a bug | [Expressions](../graph/expressions.md) |
| `Unsupported Graph if comparison operator '{Op}'.` | an operator outside `> < >= <= == !=` | use a supported comparison | [if](../graph/if.md) |
| `Unsupported Graph variable type '{Type}'.` | a bare declaration whose type token does not resolve | use a supported type token. `return x;`, `for (…) { }`, `while (…) { }`, `do { }` and `switch (…) { }` all surface here, because they parse as declarations | [Unsupported constructs](../graph/unsupported.md) |
| `Unsupported Graph variable type '{Type}' for '{Name}'.` | the same, for a declaration that has an initializer; `a += b` (with spaces) surfaces here | use a supported type token; compound assignment is not supported | [Unsupported constructs](../graph/unsupported.md) |
| `Unsupported MaterialAttributes member '{Member}'.` | the member name is not in the attribute table | check the member name | [MaterialAttributes](../graph/material-attributes.md) |
| `Unsupported or failed binary operator '{Op}'.` | the operator node could not be built | report as a bug | [Expressions](../graph/expressions.md) |
| `Unsupported swizzle '{Swizzle}'.` | characters outside `xyzw` / `rgba`, or more than four of them | use up to four channels from one set | [Swizzle](../graph/swizzle.md) |
| `Unsupported unary operator '{Op}'.` | a unary operator other than `-` | only unary minus is supported | [Expressions](../graph/expressions.md) |
| `{Detail} It must reference a previously declared property or use a compatible literal.` | a value position resolved to neither a declared property nor a parseable literal; `{Detail}` is the inner literal diagnostic | declare the property first, or fix the literal | [Literals](../graph/literals.md) |

---

## Builtins (`UE.*`, math, Substrate)

The `UE.*` and `Substrate.*` call surfaces in `Graph`, the `UE.*` declaration form in `Properties`,
the math builtins, and the shared reflected-value writer.

`{Namespace}` is `UE` or `Substrate`. Messages beginning `UE.{Name} for property '{Property}':` wrap a
property-form message; the wrapped texts are listed here as their own rows and never appear alone.

| Message | Cause | Fix | See |
| :-- | :-- | :-- | :-- |
| `'{Value}' is not a valid boolean value for '{Property}'.` | reflected `bool` property written with a non-boolean | use `true` or `false` | [UE.Expression](../builtins/ue-expression.md) |
| `'{Value}' is not a valid byte value for '{Property}'.` | non-enum `uint8` property outside `0…255` | use `0…255` | [UE.Expression](../builtins/ue-expression.md) |
| `'{Value}' is not a valid enum value for '{Property}'.` | none of the four accepted enum spellings matched | try the short name, full name, display name, or the short name with the prefix removed | [UE.Expression](../builtins/ue-expression.md) |
| `'{Value}' is not a valid integer value for '{Property}'.` | `int32` property written with a non-integer | use a decimal integer | [UE.Expression](../builtins/ue-expression.md) |
| `'{Value}' is not a valid non-negative UV channel index.` | `UE.TexCoord` `Index` / `CoordinateIndex` is negative or not an integer | use `0`, `1`, … | [UE builtins](../builtins/ue.md) |
| `'{Value}' is not a valid numeric value for '{Property}'.` | `float` / `double` property written with a non-number | use a numeric literal | [UE.Expression](../builtins/ue-expression.md) |
| `'{Value}' is not a valid unsigned integer value for '{Property}'.` | `uint32` property outside `0 … MAX_uint32` | use a value in range | [UE.Expression](../builtins/ue-expression.md) |
| `'{Text}' is not a valid property reference or literal input.` | a property-form generic input that is neither a declared property nor a literal | reference a `Properties` entry, or use a numeric literal | [UE builtins](../builtins/ue.md) |
| `Asset '{Path}' is not compatible with '{Property}'. Expected '{Class}'.` | the loaded asset is the wrong UClass | point at an asset of the expected class | [Path](../parameters/path.md) |
| `CameraVectorWS does not take any arguments.` | any argument on the property-form `UE.CameraVectorWS` | remove the arguments | [UE builtins](../builtins/ue.md) |
| `Collection '{Collection}' does not contain parameter '{Parameter}'.` | the MPC has no such scalar or vector parameter | check the parameter name in the collection asset | [UE builtins](../builtins/ue.md) |
| `Collection is invalid: {Detail}` | the property-form `Collection=` value is not a resolvable asset reference | use `Path(Game, "…")` or an object path | [Path](../parameters/path.md) |
| `CollectionParam requires Collection=Path(...).` | property-form `UE.CollectionParam` with no `Collection` / `Asset` | supply the collection | [UE builtins](../builtins/ue.md) |
| `CollectionParam requires Parameter="Name".` | property-form `UE.CollectionParam` with no `Parameter` / `ParameterName` | supply the parameter name | [UE builtins](../builtins/ue.md) |
| `ConstCoordinate value '{Value}' is invalid.` | `UE.Panner` `ConstCoordinate` is not a non-negative integer | use `0`, `1`, … | [UE builtins](../builtins/ue.md) |
| `Coordinate input is invalid. {Detail}` | `UE.Panner` `Coordinate` is neither an integer nor a valid 2-component expression | supply one of the two forms | [UE builtins](../builtins/ue.md) |
| `Could not load MaterialParameterCollection '{Path}'.` | the collection asset failed to load | check the path | [UE builtins](../builtins/ue.md) |
| `Failed to create UE.CollectionParam node.` | node creation failed | report as a bug | [UE builtins](../builtins/ue.md) |
| `Failed to create UE.{Name}.` | node creation failed for a registered Graph builtin | report as a bug | [UE builtins](../builtins/ue.md) |
| `Failed to create a float{N} constant expression.` | a property-form inline constant could not be created | report as a bug | [UE builtins](../builtins/ue.md) |
| `Failed to create math function '{Name}'.` | the math node could not be created | report as a bug | [Math builtins](../builtins/math.md) |
| `Failed to create the native CameraVectorWS node.` | node creation failed | report as a bug | [UE builtins](../builtins/ue.md) |
| `Failed to create the native CollectionParameter node.` | node creation failed | report as a bug | [UE builtins](../builtins/ue.md) |
| `Failed to create the native ObjectPositionWS node.` | node creation failed | report as a bug | [UE builtins](../builtins/ue.md) |
| `Failed to create the native Panner node.` | node creation failed | report as a bug | [UE builtins](../builtins/ue.md) |
| `Failed to create the native ScreenPosition node.` | node creation failed | report as a bug | [UE builtins](../builtins/ue.md) |
| `Failed to create the native TexCoord node.` | node creation failed | report as a bug | [UE builtins](../builtins/ue.md) |
| `Failed to create the native Time node.` | node creation failed | report as a bug | [UE builtins](../builtins/ue.md) |
| `Failed to create the native VertexColor node.` | node creation failed | report as a bug | [UE builtins](../builtins/ue.md) |
| `Failed to create the native WorldPosition node.` | node creation failed | report as a bug | [UE builtins](../builtins/ue.md) |
| `Failed to load asset '{Path}' for '{Property}'.` | a reflected object property's asset failed to load | check the path | [Path](../parameters/path.md) |
| `FractionalPart value '{Value}' is invalid.` | `UE.Panner` `FractionalPart` is not a boolean | use `true` or `false` | [UE builtins](../builtins/ue.md) |
| `Generic {Namespace}.{Name} calls require named arguments.` | a positional argument on the generic `UE.*` / `Substrate.*` path | use `Key=Value` for every argument | [UE.Expression](../builtins/ue-expression.md) |
| `IgnorePause value '{Value}' is invalid.` | `UE.Time` `IgnorePause` is not a boolean | use `true` or `false` | [UE builtins](../builtins/ue.md) |
| `Invalid UE builtin argument '{Argument}' in '{Statement}'.` | an empty key or empty value in a `Properties` `UE.*` declaration | supply both sides | [Properties](../language/properties.md) |
| `Invalid UE builtin declaration '{Statement}'.` | unbalanced parentheses, or an empty function name before `(` | fix the declaration | [Properties](../language/properties.md) |
| `Invalid reflected property target.` | the literal writer was handed a null property | report as a bug | [UE.Expression](../builtins/ue-expression.md) |
| `Math function '{Name}' argument {Index}: {Detail}` | an argument sub-expression failed; the index is 1-based | see the inner message | [Math builtins](../builtins/math.md) |
| `Math function '{Name}' could not bind input '{Input}'.` | the unary node has no such reflected input property | report as a bug | [Math builtins](../builtins/math.md) |
| `Math function '{Name}' expects exactly 1 argument.` | wrong arity for a unary builtin — **or a named argument**, which reports as an arity error | pass exactly one positional argument | [Math builtins](../builtins/math.md) |
| `Math function '{Name}' expects exactly 2 arguments.` | wrong arity for `dot`, `pow`, `min`, `max`, `fmod` / `mod` | pass two positional arguments | [Math builtins](../builtins/math.md) |
| `Math function '{Name}' expects exactly 3 arguments.` | wrong arity for `lerp` / `mix`, `clamp` | pass three positional arguments | [Math builtins](../builtins/math.md) |
| `Math function '{Name}' failed to access input '{Input}'.` | the reflected input pointer was null | report as a bug | [Math builtins](../builtins/math.md) |
| `Math function '{Name}' is missing argument {Index}.` | an argument index out of range; the index is 1-based | pass the missing argument | [Math builtins](../builtins/math.md) |
| `Math function '{Name}' only accepts positional arguments.` | a named argument on a math builtin | drop the `Name =` prefixes | [Math builtins](../builtins/math.md) |
| `Math function '{Name}' only accepts numeric scalar/vector arguments.` | a texture, `MaterialAttributes` or `Substrate` argument | pass numeric values | [Math builtins](../builtins/math.md) |
| `Object property '{Property}' expects Path(...) or an absolute Unreal object path.` | a reflected object slot given something else | use `Path( … )` or `/Game/…` | [Path](../parameters/path.md) |
| `Origin value '{Value}' is invalid.` | `UE.ObjectPositionWS` `Origin` is not `Absolute` / `World` / `CameraRelative` | use one of those | [UE builtins](../builtins/ue.md) |
| `OutputIndex is out of range for '{Class}'.` | property-form Custom-node `OutputIndex` is out of range | use a valid index | [UE.Expression](../builtins/ue-expression.md) |
| `Period value '{Value}' is invalid.` | property-form `UE.Time` `Period` is not a number, or is negative | use a non-negative number. The Graph form has no non-negative check | [UE builtins](../builtins/ue.md) |
| `Property '{Property}' on '{Class}' is not a supported literal type yet.` | a struct or array property whose text failed Unreal's own import | use Unreal's literal syntax, e.g. `(R=1,G=0,B=0,A=1)` | [Metadata](../parameters/metadata.md) |
| `SampleTexture2D expects exactly two positional arguments: (textureObject, uv).` | wrong argument count; the name is matched **case-sensitively** | write `SampleTexture2D(Tex, UV)` | [UE builtins](../builtins/ue.md) |
| `ScreenPosition does not take any arguments.` | any argument on the property-form `UE.ScreenPosition` | remove the arguments | [UE builtins](../builtins/ue.md) |
| `ShaderOffsets value '{Value}' is invalid.` | `UE.WorldPosition` `ShaderOffsets` is not a recognized token | use one of the accepted offset tokens | [UE builtins](../builtins/ue.md) |
| `Speed input is invalid. {Detail}` | `UE.Panner` `Speed` did not evaluate to a 2-component value | supply a `float2` | [UE builtins](../builtins/ue.md) |
| `SpeedX value '{Value}' is invalid.` | `UE.Panner` `SpeedX` is not a number | use a numeric literal | [UE builtins](../builtins/ue.md) |
| `SpeedY value '{Value}' is invalid.` | `UE.Panner` `SpeedY` is not a number | use a numeric literal | [UE builtins](../builtins/ue.md) |
| `Substrate builtin call '{Name}' requires Unreal Engine 5.4 or newer.` | any `Substrate.*` call on UE 5.3 | move to UE 5.4+ | [Substrate](../builtins/substrate.md) |
| `Substrate.{Name} resolved to non-Substrate class '{Class}'.` | the descriptor's class is not a Substrate BSDF or utility | report as a bug | [Substrate](../builtins/substrate.md) |
| `Substrate.{Name} uses a fixed MaterialExpression class and does not accept Class.` | `Class=` passed to a `Substrate.*` wrapper | remove `Class=` | [Substrate](../builtins/substrate.md) |
| `This builtin is not implemented by the material generator yet. For generic MaterialExpression support, add OutputType="float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture".` | a property-form `UE.*` name the parser accepts but the generator has no implementation for — `UE.VertexNormalWS` and `UE.VertexTangentWS` are the two live cases | add `OutputType="…"` to route through the generic path, e.g. `UE.VertexNormalWS(OutputType="float3")` | [UE builtins](../builtins/ue.md) |
| `Time input is invalid. {Detail}` | `UE.Panner` `Time` did not evaluate to a scalar | supply a `float` | [UE builtins](../builtins/ue.md) |
| `UE builtin argument '{Argument}' must use named syntax like Key=Value in '{Statement}'.` | a positional argument in a `Properties` `UE.*` declaration | use `Key=Value` | [Properties](../language/properties.md) |
| `UE builtin argument '{Key}' is declared more than once in '{Statement}'.` | duplicate argument key after normalization | remove the duplicate | [Properties](../language/properties.md) |
| `UE builtin property '{Name}' does not support inline defaults. Put arguments inside UE.{Function}(...).` | `UE.X Name = value;` | move the value into the argument list | [Properties](../language/properties.md) |
| `UE builtin property declarations must specify a function name, for example UE.TexCoord UV.` | a bare `UE.` type token | name the builtin | [Properties](../language/properties.md) |
| `UE.CollectionParam Collection is invalid: {Detail}` | the Graph-form `Collection=` value is not a resolvable asset reference | use `Path(Game, "…")` or an object path | [Path](../parameters/path.md) |
| `UE.CollectionParam Collection must be Path(...) or an Unreal object path.` | `Collection=` is not an asset reference at all | use `Path( … )` | [Path](../parameters/path.md) |
| `UE.CollectionParam Parameter must be a text value.` | `Parameter=` is not a literal string | quote the parameter name | [UE builtins](../builtins/ue.md) |
| `UE.CollectionParam SortPriority must be an integer literal.` | `SortPriority=` is not an integer | use an integer. The value is validated then dropped below UE 5.7 | [UE builtins](../builtins/ue.md) |
| `UE.CollectionParam collection '{Collection}' does not contain parameter '{Parameter}'.` | the MPC has no such parameter | check the collection asset | [UE builtins](../builtins/ue.md) |
| `UE.CollectionParam could not load MaterialParameterCollection '{Path}'.` | the collection failed to load | check the path | [UE builtins](../builtins/ue.md) |
| `UE.CollectionParam requires Collection=Path(...).` | Graph-form `UE.CollectionParam` with no `Collection` / `Asset` | supply the collection | [UE builtins](../builtins/ue.md) |
| `UE.CollectionParam requires Parameter="Name".` | Graph-form `UE.CollectionParam` with no `Parameter` / `ParameterName` | supply the parameter name | [UE builtins](../builtins/ue.md) |
| `UE.Expression requires Class="MaterialExpressionName".` | `UE.Expression( … )` with no `Class` | add `Class="…"`, or call `UE.<ClassName>( … )` and let `Class` default to the function name | [UE.Expression](../builtins/ue-expression.md) |
| `UE.SceneTexture expects exactly Id="..." (e.g. Id="PostProcessInput0").` | `UE.SceneTexture` called with anything other than a single `Id=` | supply `Id="…"` | [UE builtins](../builtins/ue.md) |
| `UE.StaticSwitchParameter Default/DefaultValue must be true or false.` | the default is not a boolean | use `true` or `false` | [Parameter nodes](../parameters/parameter-nodes.md) |
| `UE.StaticSwitchParameter Name must be a text value.` | `Name=` is not a literal string | quote the name | [Parameter nodes](../parameters/parameter-nodes.md) |
| `UE.StaticSwitchParameter SortPriority must be an integer literal.` | `SortPriority=` is not an integer | use an integer | [Parameter nodes](../parameters/parameter-nodes.md) |
| `UE.StaticSwitchParameter requires Name="ParameterName".` | no `Name=` argument | supply the parameter name | [Parameter nodes](../parameters/parameter-nodes.md) |
| `UE.Time Period must be a numeric literal.` | `Period=` on `UE.Time` is not a scalar literal; `UE.Time` is the only registered builtin with a `Period` argument | pass a number | [`UE.*` catalogue](../builtins/ue.md) |
| `UE.TransformPosition FirstPersonInterpolationAlpha requires Unreal Engine 5.6 or newer.` | that input used below UE 5.6 | remove it, or move to UE 5.6+ | [Transform](../builtins/transform.md) |
| `UE.TransformPosition Source/Destination is invalid.` | a basis name that does not resolve — including `PeriodicWorld` below UE 5.5 and `FirstPerson` below UE 5.6 | use a basis the engine version supports | [Transform](../builtins/transform.md) |
| `UE.TransformVector Source/Destination is invalid.` | a basis name that does not resolve | use a supported basis name | [Transform](../builtins/transform.md) |
| `UE.{Name} Class must be a literal value.` | `Class=` is an expression, not a literal | quote the class name | [UE.Expression](../builtins/ue-expression.md) |
| `UE.{Name} Custom input '{Input}' does not accept Substrate values.` | a `Substrate` value fed to a Custom node input | use a numeric value | [Substrate](../builtins/substrate.md) |
| `UE.{Name} OutputIndex is out of range for '{Class}'.` | `OutputIndex` is negative or beyond the node's output count | use a valid index | [UE.Expression](../builtins/ue-expression.md) |
| `UE.{Name} OutputName must be a literal value.` | `Output=` / `OutputName=` is not a literal | quote the output name | [UE.Expression](../builtins/ue-expression.md) |
| `UE.{Name} OutputName must be a non-empty literal value.` | an empty `OutputName` on a Custom node | supply a name | [UE.Expression](../builtins/ue-expression.md) |
| `UE.{Name} OutputType '{Type}' is not a valid Custom node output type.` | a Custom node declared `Texture2D`, `StaticBool` or another non-`CMOT_*` type | use `float1` … `float4` or `MaterialAttributes` | [Output types](../builtins/output-type.md) |
| `UE.{Name} OutputType '{Type}' is not supported.` | the token does not resolve to a declared type | consult the accepted `OutputType` list | [Output types](../builtins/output-type.md) |
| `UE.{Name} OutputType must be a literal value.` | `OutputType=` is an expression | quote the token | [Output types](../builtins/output-type.md) |
| `UE.{Name} OutputType="Substrate" is not supported by UMaterialExpressionCustom.` | a Custom node asked for a Substrate output | use a `Substrate.*` node instead | [Substrate](../builtins/substrate.md) |
| `UE.{Name} OutputType="Substrate" requires Unreal Engine 5.4 or newer.` | `OutputType="Substrate"` on UE 5.3 | move to UE 5.4+ | [Substrate](../builtins/substrate.md) |
| `UE.{Name} cannot use OutputName/Output together with OutputIndex.` | both output selectors given | keep one | [UE.Expression](../builtins/ue-expression.md) |
| `UE.{Name} could not resolve MaterialExpression class '{Class}'.` | no loaded, non-abstract `UMaterialExpression` subclass matched any candidate spelling | try `Sine`, `MaterialExpressionSine`, `UMaterialExpressionSine` or `/Script/Engine.MaterialExpressionSine` | [Class resolution](../builtins/ue-expression.md) |
| `UE.{Name} created '{Class}', but it has no material outputs.` | the node exposes no output 0 | pick a different class | [UE.Expression](../builtins/ue-expression.md) |
| `UE.{Name} failed to bind input '{Input}'.` | the pin could not be connected | report as a bug | [UE.Expression](../builtins/ue-expression.md) |
| `UE.{Name} failed to create '{Class}'.` | node creation failed | report as a bug | [UE.Expression](../builtins/ue-expression.md) |
| `UE.{Name} for property '{Property}' does not support argument '{Argument}'.` | an argument outside the builtin's whitelist, in the `Properties` declaration form | remove it, or switch to the generic form with `OutputType="…"` | [Properties](../language/properties.md) |
| `UE.{Name} for property '{Property}': could not resolve MaterialExpression class '{Class}'.` | the property-form `Class=` matched no expression class | check the class spelling | [Properties](../language/properties.md) |
| `UE.{Name} for property '{Property}': failed to bind input '{Input}'.` | the pin could not be connected, in the `Properties` declaration form | report as a bug | [Properties](../language/properties.md) |
| `UE.{Name} for property '{Property}': failed to create '{Class}'.` | node creation failed, in the `Properties` declaration form | report as a bug | [Properties](../language/properties.md) |
| `UE.{Name} for property '{Property}': OutputIndex is out of range for '{Class}'.` | `OutputIndex=` addresses an output the node does not have | pick a valid index | [Properties](../language/properties.md) |
| `UE.{Name} for property '{Property}': OutputType '{Type}' is not a valid Custom node output type.` | the property-form `OutputType=` is not one of the Custom-node output types | use `float1`–`float4` or `MaterialAttributes` | [OutputType](../builtins/output-type.md) |
| `UE.{Name} for property '{Property}': {Detail}` | wrapper around a property-form builtin failure whose inner text is produced elsewhere; the five rows above are complete messages, not wrappers | see the inner message | [Properties](../language/properties.md) |
| `UE.{Name} input '{Input}': {Detail}` | an argument sub-expression failed | see the inner message | [UE.Expression](../builtins/ue-expression.md) |
| `UE.{Name} output '{Output}' was not found on '{Class}'.` | `Output=` / `OutputName=` matched no output; unnamed outputs also accept the mask pseudo-names `R`, `G`, `B`, `A`, `RG`, `RGB`, `RGBA` | use a real output name or `OutputIndex` | [UE.Expression](../builtins/ue-expression.md) |
| `UE.{Name} property '{Property}' must use Path(...) or an Unreal object path.` | a reflected object property given a non-asset value | use `Path( … )` or `/Game/…` | [Path](../parameters/path.md) |
| `UE.{Name} property '{Property}' must use a literal value.` | a reflected property given an expression | use a literal | [UE.Expression](../builtins/ue-expression.md) |
| `UE.{Name} property '{Property}': {Detail}` | the reflected write failed | see the inner message | [UE.Expression](../builtins/ue-expression.md) |
| `UE.{Name} requires parameter: {Detail}` | a registered Graph builtin is missing a required argument | supply it | [UE builtins](../builtins/ue.md) |
| `UE.{Name} {Argument} must be a boolean literal.` | a registered builtin's boolean argument is not `true` / `false` | use a boolean literal | [UE builtins](../builtins/ue.md) |
| `UE.{Name} {Argument} must be a numeric literal.` | a registered builtin's numeric argument is not a number | use a numeric literal | [UE builtins](../builtins/ue.md) |
| `UE.{Name} {Argument} must be a text value.` | a registered builtin's text argument is not a literal string | quote the value | [UE builtins](../builtins/ue.md) |
| `UE.{Name} {Argument} must be an integer literal.` | a registered builtin's integer argument is not an integer | use an integer | [UE builtins](../builtins/ue.md) |
| `UE.{Name}: '{Argument}' is not a property on '{Class}'.` | an argument that matched neither an input pin nor a reflected property, on a non-Custom node | check the name against the node's pins and UPROPERTYs | [UE.Expression](../builtins/ue-expression.md) |
| `UTiling value '{Value}' is invalid.` | `UE.TexCoord` `UTiling` is not a number | use a numeric literal | [UE builtins](../builtins/ue.md) |
| `UnMirrorU value '{Value}' is invalid.` | `UE.TexCoord` `UnMirrorU` is not a boolean | use `true` or `false` | [UE builtins](../builtins/ue.md) |
| `UnMirrorV value '{Value}' is invalid.` | `UE.TexCoord` `UnMirrorV` is not a boolean | use `true` or `false` | [UE builtins](../builtins/ue.md) |
| `Unsupported Substrate builtin call '{Name}' in Graph.` | the name is not in the `Substrate.*` table | check the catalogue | [Substrate](../builtins/substrate.md) |
| `Unsupported UE builtin call '{Name}' in Graph. For generic MaterialExpression calls, add OutputType="float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture/Substrate".` | an unregistered `UE.*` name with no `OutputType` / `ResultType` | add `OutputType="…"`. The hint string is incomplete — `MaterialAttributes`, `SamplerState`, `StaticBool` and the `half*` / `vec*` / `ivec*` / `uvec*` / `bvec*` / `int*` / `uint*` / `bool*` families are also accepted | [Output types](../builtins/output-type.md) |
| `Unsupported UE builtin function '{Name}'. Use OutputType="float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture" for generic MaterialExpression calls.` | the same, for the `Properties` declaration form | add `OutputType="…"`; note that `MaterialAttributes` is **not** accepted by the declaration form | [Properties](../language/properties.md) |
| `Unsupported vector literal '{Literal}'.` | a property-form generic input literal with fewer than 2 or more than 4 components | use a 2–4 component literal | [UE builtins](../builtins/ue.md) |
| `Use either Index or CoordinateIndex, not both.` | both `UE.TexCoord` spellings supplied in the property form | keep one | [UE builtins](../builtins/ue.md) |
| `VTiling value '{Value}' is invalid.` | `UE.TexCoord` `VTiling` is not a number | use a numeric literal | [UE builtins](../builtins/ue.md) |
| `VertexColor does not take any arguments.` | any argument on the property-form `UE.VertexColor` | remove the arguments | [UE builtins](../builtins/ue.md) |
| `{Namespace}.{Name} input '{Input}' does not accept MaterialAttributes values.` | attributes fed to a numeric pin | read a member first | [MaterialAttributes](../graph/material-attributes.md) |
| `{Namespace}.{Name} input '{Input}' does not accept Substrate values.` | a `Substrate` value fed to a numeric pin | use a numeric value | [Substrate](../builtins/substrate.md) |
| `{Namespace}.{Name} input '{Input}' expects a MaterialAttributes value.` | a numeric value fed to an attributes pin | supply `MaterialAttributes` | [MaterialAttributes](../graph/material-attributes.md) |
| `{Namespace}.{Name} input '{Input}' expects a Substrate value.` | a numeric value fed to a Substrate pin | supply a `Substrate.*` result | [Substrate](../builtins/substrate.md) |
| `{Namespace}.{Name} output is not a Substrate value.` | `OutputType="Substrate"` but the selected pin is not Substrate | select the Substrate output, or drop the declared type | [Substrate](../builtins/substrate.md) |

---

## Functions and HLSL codegen

Calling `Function`, `GraphFunction`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` and
`VirtualFunction` from `Graph`, and the generated HLSL helper include.

In this table `{Kind}` is the call kind — `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, or the
literal `VirtualFunction`.

| Message | Cause | Fix | See |
| :-- | :-- | :-- | :-- |
| `DreamShader Function '{Name}' cannot write multiple out results into '{Target}' in the same call.` | the same out variable used twice | use distinct out targets | [Calls](../graph/calls.md) |
| `DreamShader Function '{Name}' currently uses positional arguments only.` | a named argument in a `Function` call | pass arguments positionally | [Calls](../graph/calls.md) |
| `DreamShader Function '{Name}' expects {Total} arguments ({Inputs} inputs, {Outs} out targets) but got {Actual}.` | wrong argument count in statement form | pass every input followed by every out target | [Calls](../graph/calls.md) |
| `DreamShader Function '{Name}' has {Count} outputs and must be called with explicit out variables, for example {Name}(..., ResultA, ResultB).` | a multi-output `Function` used as a value expression | use statement form with out targets | [Calls](../graph/calls.md) |
| `DreamShader Function '{Name}' has an empty out target name.` | an out argument that is blank | supply a variable name | [Calls](../graph/calls.md) |
| `DreamShader Function '{Name}' has unsupported result type '{Type}'.` | the result type token does not resolve | use a supported type token | [Types](../language/types.md) |
| `DreamShader Function '{Name}' input '{Input}' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead.` | a `Substrate` input on an HLSL `Function` | switch to `GraphFunction` or `ShaderFunction` | [Substrate](../builtins/substrate.md) |
| `DreamShader Function '{Name}' input '{Input}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | a `Substrate` input on UE 5.3 | move to UE 5.4+ | [Substrate](../builtins/substrate.md) |
| `DreamShader Function '{Name}' input '{Input}' uses unsupported type '{Type}'.` | the input type token does not resolve | use a supported type token | [Types](../language/types.md) |
| `DreamShader Function '{Name}' input '{Input}': {Detail}` | an argument sub-expression failed | see the inner message | [Calls](../graph/calls.md) |
| `DreamShader Function '{Name}' must declare at least one out result.` | statement-form call to a function with no results | add an `out` parameter | [Function](../language/function.md) |
| `DreamShader Function '{Name}' out argument {Index} must be a plain variable name.` | an expression where an out target was expected | pass a bare identifier | [Calls](../graph/calls.md) |
| `DreamShader Function '{Name}' result '{Result}' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead.` | a `Substrate` result on an HLSL `Function` | switch block kind | [Substrate](../builtins/substrate.md) |
| `DreamShader Function '{Name}' result '{Result}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | a `Substrate` result on UE 5.3 | move to UE 5.4+ | [Substrate](../builtins/substrate.md) |
| `DreamShader Function '{Name}' returns one value and expects {Expected} input argument(s) when used as a value expression, but got {Actual}.` | wrong argument count in value form | match the input count | [Calls](../graph/calls.md) |
| `DreamShader Function '{Name}' is declared more than once.` | two `Function` declarations with the same qualified name reach codegen | rename one, or namespace it | [Namespace](../language/namespace.md) |
| `DreamShader Function '{Name}' collides with another generated helper symbol '{Symbol}'. Rename the Function or Namespace.` | two names sanitize to the same HLSL symbol | rename the function or its namespace | [Generated HLSL](../generation/generated-hlsl.md) |
| `DreamShader GraphFunction '{Name}' UE input '{Input}' cannot be passed into a Custom node input.` | a hoisted `UE.*` value that cannot become a Custom-node pin | restructure the body | [GraphFunction](../language/graph-function.md) |
| `DreamShader GraphFunction '{Name}' UE input '{Input}': {Detail}` | a hoisted `UE.*` argument failed to evaluate | see the inner message | [GraphFunction](../language/graph-function.md) |
| `DreamShader GraphFunction '{Name}' cannot write multiple out results into '{Target}' in the same call.` | the same out variable used twice | use distinct out targets | [Calls](../graph/calls.md) |
| `DreamShader GraphFunction '{Name}' contains an unterminated UE.* call.` | an unbalanced `(` in a `UE.*` call inside the body | balance the parentheses | [GraphFunction](../language/graph-function.md) |
| `DreamShader GraphFunction '{Name}' Graph body is invalid: {Detail}` | the body could not be split into `Graph` statements before `UE.*` hoisting; `{Detail}` is the inner statement-parse diagnostic | fix the statement the inner message names | [GraphFunction](../language/graph-function.md) |
| `DreamShader GraphFunction '{Name}' currently uses positional arguments only.` | a named argument | pass arguments positionally | [Calls](../graph/calls.md) |
| `DreamShader GraphFunction '{Name}' did not produce a value result.` | the single-output value call yielded nothing | check the body assigns the result | [GraphFunction](../language/graph-function.md) |
| `DreamShader GraphFunction '{Name}' expects {Total} arguments ({Inputs} inputs, {Outs} out targets) but got {Actual}.` | wrong argument count | match the signature | [Calls](../graph/calls.md) |
| `DreamShader GraphFunction '{Name}' has {Count} outputs and must be called with explicit out variables, for example {Name}(..., ResultA, ResultB).` | a multi-output `GraphFunction` used as a value | use statement form | [Calls](../graph/calls.md) |
| `DreamShader GraphFunction '{Name}' has an empty out target name.` | a blank out argument | supply a variable name | [Calls](../graph/calls.md) |
| `DreamShader GraphFunction '{Name}' has unsupported result type '{Type}'.` | the result type token does not resolve | use a supported type token | [Types](../language/types.md) |
| `DreamShader GraphFunction '{Name}' input '{Input}' uses Substrate, which is only supported by GraphFunction Graph blocks.` | a `Substrate` input on this path | restructure | [Substrate](../builtins/substrate.md) |
| `DreamShader GraphFunction '{Name}' input '{Input}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | UE 5.3 | move to UE 5.4+ | [Substrate](../builtins/substrate.md) |
| `DreamShader GraphFunction '{Name}' input '{Input}' uses unsupported type '{Type}'.` | the input type token does not resolve | use a supported type token | [Types](../language/types.md) |
| `DreamShader GraphFunction '{Name}' input '{Input}': {Detail}` | an argument sub-expression failed | see the inner message | [Calls](../graph/calls.md) |
| `DreamShader GraphFunction '{Name}' must declare at least one out result.` | statement-form call to a function with no results | add an `out` parameter | [GraphFunction](../language/graph-function.md) |
| `DreamShader GraphFunction '{Name}' out argument {Index} must be a plain variable name.` | an expression where an out target was expected | pass a bare identifier | [Calls](../graph/calls.md) |
| `DreamShader GraphFunction '{Name}' result '{Result}' uses Substrate, which is only supported by GraphFunction Graph blocks.` | a `Substrate` result on this path | restructure | [Substrate](../builtins/substrate.md) |
| `DreamShader GraphFunction '{Name}' result '{Result}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | UE 5.3 | move to UE 5.4+ | [Substrate](../builtins/substrate.md) |
| `DreamShader GraphFunction '{Name}' result '{Result}' uses unsupported type '{Type}'.` | the result type token does not resolve | use a supported type token | [Types](../language/types.md) |
| `DreamShader GraphFunction '{Name}' result '{Result}' was never assigned.` | the body never wrote the result | assign it | [GraphFunction](../language/graph-function.md) |
| `DreamShader GraphFunction '{Name}' result '{Result}': {Detail}` | the result value failed | see the inner message | [GraphFunction](../language/graph-function.md) |
| `DreamShader GraphFunction '{Name}' returns one value and expects {Expected} input argument(s) when used as a value expression, but got {Actual}.` | wrong argument count in value form | match the input count | [Calls](../graph/calls.md) |
| `Failed to create a Custom node for DreamShader Function '{Name}'.` | node creation failed | report as a bug | [Generated HLSL](../generation/generated-hlsl.md) |
| `Failed to create a Custom node for DreamShader GraphFunction '{Name}'.` | node creation failed | report as a bug | [GraphFunction](../language/graph-function.md) |
| `Failed to assign material function '{Name}' to the generated call node.` | the `MaterialFunctionCall` node rejected the asset | check the asset's usage kind | [Calls](../graph/calls.md) |
| `Failed to create a MaterialFunctionCall node for '{Name}'.` | node creation failed | report as a bug | [Calls](../graph/calls.md) |
| `Failed to write generated helper include '{Path}'.` | the `.ush` could not be written | check the generated-shader directory and permissions | [Generated HLSL](../generation/generated-hlsl.md) |
| `Graph call '{Name}' is ambiguous because multiple definitions use that name: {Names}.` | one name declared by more than one callable | rename, or qualify with a namespace | [Name resolution](../graph/name-resolution.md) |
| `GraphFunction call requires an active Graph build context.` | a statement-form `GraphFunction` call outside a graph build | call it from a `Graph` block | [GraphFunction](../language/graph-function.md) |
| `GraphFunction cycle detected: {Chain}.` | a `GraphFunction` calls itself, directly or transitively | break the cycle | [GraphFunction](../language/graph-function.md) |
| `GraphFunction value call requires an active Graph build context.` | a value-form `GraphFunction` call outside a graph build | call it from a `Graph` block | [GraphFunction](../language/graph-function.md) |
| `SelfContained Function cycle detected: {Chain}. HLSL Custom nodes cannot compile recursive DreamShader functions.` | recursion among `SelfContained` / `Inline` functions | break the cycle | [Function](../language/function.md) |
| `Unknown Graph function '{Name}'.` | the callee is not a declared `Function` | check the spelling and the `import` | [Name resolution](../graph/name-resolution.md) |
| `Unknown SelfContained DreamShader Function '{Name}'.` | a `SelfContained` closure references a function that no longer exists | check the declaration | [Function](../language/function.md) |
| `VirtualFunction '{Name}' asset reference is invalid: {Detail}` | `Options.Asset` did not resolve | fix the `Path( … )` | [VirtualFunction](../language/virtual-function.md) |
| `{Kind} '{Name}' OutputIndex is out of range.` | `OutputIndex=` beyond the asset's output count | use a valid index | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' OutputName must be a literal value.` | `Output=` / `OutputName=` is not a literal | quote the output name | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' cannot use OutputName/Output together with OutputIndex.` | both selectors given | keep one | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' cannot write multiple outputs into '{Target}' in the same call.` | the same out variable used twice | use distinct out targets | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' could not load MaterialFunction asset '{Path}'.` | the referenced asset is missing | check the asset path | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' does not expose an output named '{Output}'.` | `Output=` matched no output on the asset | use an existing output name | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' does not have an input named '{Input}'.` | a named argument matched no declared input | check the input name | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' expects at most {Inputs} input argument(s) followed by {Outputs} output target(s), but got {Actual} input argument(s).` | too many inputs in statement form | match the signature | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' expects output target arguments after its inputs, but got {Actual} total argument(s) for {Outputs} output(s).` | too few arguments in statement form | append one target per output | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' exposes multiple outputs. Specify Output="Name" or OutputIndex=N.` | a value-form call to a multi-output asset | add `Output=` or `OutputIndex=` | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' has an empty output target name.` | a blank out argument | supply a variable name | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' input '{Input}' does not exist on MaterialFunction asset '{Path}'.` | the declared input is absent from the asset | re-sync the declaration | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `{Kind} '{Name}' input '{Input}' is not optional and cannot use default.` | `default` passed for a non-`opt` input | pass a value, or mark the input `opt` | [Inputs / Outputs / Results](../language/inputs-outputs.md) |
| `{Kind} '{Name}' input '{Input}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | UE 5.3 | move to UE 5.4+ | [Substrate](../builtins/substrate.md) |
| `{Kind} '{Name}' input '{Input}' uses unsupported type '{Type}'.` | the input type token does not resolve | use a supported type token | [Types](../language/types.md) |
| `{Kind} '{Name}' input '{Input}': {Detail}` | an argument sub-expression failed | see the inner message | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' input arguments cannot mix positional and named forms.` | some arguments named, some positional | pick one form | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' is missing required input '{Input}'.` | a non-`opt` input was not supplied | supply it, or mark it `opt` | [Inputs / Outputs / Results](../language/inputs-outputs.md) |
| `{Kind} '{Name}' must declare at least one output.` | the declaration has no outputs | add an output | [ShaderFunction](../language/shader-function.md) |
| `{Kind} '{Name}' output '{Output}' does not exist on MaterialFunction asset '{Path}'.` | the declared output is absent from the asset | re-sync the declaration | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `{Kind} '{Name}' output '{Output}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | UE 5.3 | move to UE 5.4+ | [Substrate](../builtins/substrate.md) |
| `{Kind} '{Name}' output '{Output}' uses unsupported type '{Type}'.` | the output type token does not resolve | use a supported type token | [Types](../language/types.md) |
| `{Kind} '{Name}' output argument {Index} must be a plain variable name.` | an expression where an output target was expected | pass a bare identifier | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' received {Actual} positional input argument(s), but only {Declared} input(s) are declared.` | too many positional inputs | match the signature | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' statement call requires an active Graph build context.` | a statement-form call outside a graph build | call it from a `Graph` block | [Calls](../graph/calls.md) |
| `{Kind} '{Name}' statement calls currently use positional arguments only.` | a named argument in statement form | pass arguments positionally | [Calls](../graph/calls.md) |

---

## Properties and parameters

`Properties` declarations, the `[ … ]` metadata block, parameter-node construction, `Path( … )` asset
references, and parameter reads from `Graph`.

| Message | Cause | Fix | See |
| :-- | :-- | :-- | :-- |
| `'{Class}' does not expose a ParameterName property.` | the resolved node class has no `ParameterName` and is not a `DynamicParameter` | use a parameter class that exposes one | [Parameter nodes](../parameters/parameter-nodes.md) |
| `'{Class}' does not expose a texture/asset property for property '{Name}'.` | none of `Texture`, `TextureObject`, `SparseVolumeTexture`, `VirtualTexture`, `TextureCollection`, `Font` exists on the node | drop the default, or use a class with an asset slot | [Parameter nodes](../parameters/parameter-nodes.md) |
| `Asset Path root '{Root}' has an invalid plugin name.` | the plugin name does not survive `SanitizeObjectName` | use a valid plugin name | [Path](../parameters/path.md) |
| `Asset Path root '{Root}' references plugin '{Plugin}', but its Content directory does not exist: '{Dir}'.` | the plugin has no `Content` folder on disk | create it, or point elsewhere | [Path](../parameters/path.md) |
| `Asset Path root '{Root}' references plugin '{Plugin}', but no enabled plugin with that name was found.` | `FindPlugin` returned nothing | check the plugin name | [Path](../parameters/path.md) |
| `Asset Path root '{Root}' references plugin '{Plugin}', but the plugin cannot contain content.` | `CanContainContent()` is false | enable content in the plugin descriptor | [Path](../parameters/path.md) |
| `Asset Path root '{Root}' references plugin '{Plugin}', but the plugin content is not mounted.` | *(UE 5.6+ only)* the plugin's content is not mounted | enable and mount the plugin | [Path](../parameters/path.md) |
| `Asset Path root '{Root}' references plugin '{Plugin}', but the plugin is not enabled.` | `IsEnabled()` is false | enable the plugin | [Path](../parameters/path.md) |
| `Asset Path(...) contains an unterminated string literal.` | an unclosed `"` inside `Path( … )` | close the quote | [Path](../parameters/path.md) |
| `Asset Path(...) expects either 1 argument (/Game/... path) or 2 arguments (Game\|Engine\|Plugin.PluginName, asset path).` | wrong argument count | use one of the two forms | [Path](../parameters/path.md) |
| `Asset Path(...) reference is missing a closing ')'.` | unbalanced parentheses | close the call | [Path](../parameters/path.md) |
| `Asset reference cannot be empty.` | an empty asset value | supply a path | [Path](../parameters/path.md) |
| `Asset reference requires a non-empty path.` | `Path( … )` with an empty path argument | supply a path | [Path](../parameters/path.md) |
| `Const property '{Name}' must use a plain scalar, vector, or texture type instead of a parameter node or UE builtin declaration.` | `const` applied to a `*Parameter` token or a `UE.*` declaration | drop `const`, or use a compact type token | [Properties](../language/properties.md) |
| `Const texture property '{Name}' could not load asset '{Path}'.` | the explicit default failed to load | check the path | [Path](../parameters/path.md) |
| `Const texture property '{Name}' could not load default {Type} asset '{Path}'.` | the engine fallback texture failed to load | supply an explicit default | [Compact types](../parameters/compact-types.md) |
| `Const texture property '{Name}' with type Texture2DArray requires an explicit default asset.` | no engine default exists for `Texture2DArray` | supply `= Path( … )` | [Compact types](../parameters/compact-types.md) |
| `Could not resolve MaterialExpression class for parameter type '{Type}'.` | the parameter-node token did not resolve to a class | check the token spelling | [Parameter nodes](../parameters/parameter-nodes.md) |
| `Could not resolve parameter '{Name}' for configuration.` | the call-form target is not a declared property | declare it in `Properties` | [Graph usage](../parameters/graph-usage.md) |
| `Failed to create a '{Type}' node for property '{Name}'.` | node creation failed | report as a bug | [Parameter nodes](../parameters/parameter-nodes.md) |
| `Failed to create a const node for property '{Name}'.` | node creation failed | report as a bug | [Properties](../language/properties.md) |
| `Failed to create a parameter node for property '{Name}'.` | the parameter path returned null with no message | report as a bug | [Parameter nodes](../parameters/parameter-nodes.md) |
| `Failed to create a scalar parameter node for property '{Name}'.` | node creation failed | report as a bug | [Compact types](../parameters/compact-types.md) |
| `Failed to create a texture object node for const property '{Name}'.` | node creation failed | report as a bug | [Properties](../language/properties.md) |
| `Failed to create a texture parameter node for property '{Name}'.` | node creation failed | report as a bug | [Compact types](../parameters/compact-types.md) |
| `Failed to create a vector parameter node for property '{Name}'.` | node creation failed | report as a bug | [Compact types](../parameters/compact-types.md) |
| `Failed to create StaticSwitchParameter node '{Name}'.` | node creation failed | report as a bug | [Parameter nodes](../parameters/parameter-nodes.md) |
| `Input '{Name}' default expression '{Expression}' does not match declared type '{Type}'.` | an `Inputs` default whose graph expression has the wrong shape | match the declared type | [Inputs / Outputs / Results](../language/inputs-outputs.md) |
| `Invalid asset path '{Path}'.` | the resolved path is not a valid Unreal object path | fix the path | [Path](../parameters/path.md) |
| `Invalid boolean default value '{Value}' for property '{Name}'.` | a `StaticBoolParameter` / `StaticSwitchParameter` default that is not `true` / `false` | use `true` or `false` | [Parameter nodes](../parameters/parameter-nodes.md) |
| `Invalid metadata entry '{Entry}'.` | the metadata key normalized to empty | supply a key | [Metadata](../parameters/metadata.md) |
| `Invalid parameter expression.` | the expression handed to the parameter-name writer was null | report as a bug | [Parameter nodes](../parameters/parameter-nodes.md) |
| `Invalid property declaration '{Statement}'.` | no top-level whitespace separating the type from the name | separate them | [Properties](../language/properties.md) |
| `Invalid scalar default value '{Value}' for property '{Name}'.` | the default did not parse as a number, `true` or `false` | use a numeric literal | [Compact types](../parameters/compact-types.md) |
| `Invalid texture asset path '{Path}'.` | the resolved texture path is not a valid object path | fix the path | [Path](../parameters/path.md) |
| `Invalid texture default value '{Value}' for property '{Name}'. {Detail}` | the texture default failed to resolve | see the inner `Texture Path …` message | [Path](../parameters/path.md) |
| `Invalid texture sample default value '{Value}' for property '{Name}'. {Detail}` | the texture-sample parameter default failed to resolve | see the inner message | [Path](../parameters/path.md) |
| `Invalid vector default value '{Value}' for property '{Name}'.` | the default did not parse as a 1–4 component literal | use `float3(…)`, `vec3(…)` or `( … )` | [Compact types](../parameters/compact-types.md) |
| `Metadata 'Slider(min, max)' requires exactly two numeric bounds: '{Entry}'.` | wrong arity, or non-numeric bounds | write `Slider(0, 1)` | [Metadata](../parameters/metadata.md) |
| `Metadata SliderMin/SliderMax is declared more than once (entry '{Entry}').` | `Slider( … )` combined with explicit `SliderMin` / `SliderMax` | use one form | [Metadata](../parameters/metadata.md) |
| `Metadata SortPriority value '{Value}' is not an integer.` | a non-integer `SortPriority` / `Sort` | use an integer | [Metadata](../parameters/metadata.md) |
| `Metadata entry '{Entry}' must use Key=Value syntax.` | a metadata entry with no top-level `=`, other than `Slider( … )` | use `Key=Value` | [Metadata](../parameters/metadata.md) |
| `Metadata key '{Key}' is declared more than once.` | duplicate key after normalization; the message echoes the original spelling | remove the duplicate | [Metadata](../parameters/metadata.md) |
| `Metadata must follow a declaration.` | a statement that is only a `[ … ]` block | attach it to a declaration | [Metadata](../parameters/metadata.md) |
| `Metadata property '{Property}' is not a reflected property on '{Class}'.` | an unrecognized metadata key that is not one of the soft-failing organization fields | remove it, or use a real UPROPERTY name | [Metadata](../parameters/metadata.md) |
| `Metadata property '{Property}' on '{Class}': {Detail}` | the reflected write failed | see the value-writer message in the [Builtins](#builtins-ue-math-substrate) table | [Metadata](../parameters/metadata.md) |
| `Missing property name in declaration '{Statement}'.` | the name token is empty | supply a name | [Properties](../language/properties.md) |
| `Missing property type after const in declaration '{Statement}'.` | `const` with nothing after it | supply a type | [Properties](../language/properties.md) |
| `Parameter '{Name}' ({Type}) has no input pin named '{Pin}'. Asset slots (Texture/Curve/Font/...) are set via [{Pin}=Path(...)] metadata, not call arguments.` | a call-form argument that matches no engine pin — `TextureObject` on a texture-sample **parameter** is the common case | set asset slots through metadata; use a real pin name for wiring | [Graph usage](../parameters/graph-usage.md) |
| `Parameter '{Name}' did not produce an expression node.` | the parameter node was null | report as a bug | [Graph usage](../parameters/graph-usage.md) |
| `Parameter '{Name}' input '{Pin}' must be a numeric value.` | a texture, `MaterialAttributes` or `Substrate` argument | pass a numeric value | [Graph usage](../parameters/graph-usage.md) |
| `Parameter '{Name}' input '{Pin}': {Detail}` | an argument sub-expression failed | see the inner message | [Graph usage](../parameters/graph-usage.md) |
| `Parameter '{Name}' must be called with named arguments wiring its input pins (e.g. {Name}(Coordinates=...) or {Name}(Input=...)).` | a positional argument in the parameter call form | use named arguments | [Graph usage](../parameters/graph-usage.md) |
| `Property '{Name}' has a recursive UE builtin dependency.` | a `UE.*` property argument references a property that references it back | break the cycle | [Properties](../language/properties.md) |
| `Property '{Name}': {Detail}` | wrapper around a property-node construction failure seen from `Graph` | see the inner message | [Graph usage](../parameters/graph-usage.md) |
| `Relative asset Path(...) references require a root such as Game, Engine, or Plugin.PluginName.` | a relative metadata/collection path with no root | add `Game`, `Engine` or `Plugin.<Name>` | [Path](../parameters/path.md) |
| `Relative texture Path(...) references require a root such as Game, Engine, or Plugin.PluginName.` | a relative texture default with no root | add a root | [Path](../parameters/path.md) |
| `StaticSwitchParameter '{Name}' branches must have the same component count, got {A} and {B}.` | the two branch widths differ | make them equal | [Parameter nodes](../parameters/parameter-nodes.md) |
| `StaticSwitchParameter '{Name}' cannot mix MaterialAttributes and numeric branches.` | one branch yields attributes, the other a number | make both the same kind | [Parameter nodes](../parameters/parameter-nodes.md) |
| `StaticSwitchParameter '{Name}' cannot switch Substrate values.` | a `Substrate` branch | switch earlier | [Parameter nodes](../parameters/parameter-nodes.md) |
| `StaticSwitchParameter '{Name}' cannot switch Texture object values.` | a texture-object branch | switch the sampled result | [Parameter nodes](../parameters/parameter-nodes.md) |
| `StaticSwitchParameter '{Name}' False input: {Detail}` | the false branch failed to evaluate | see the inner message | [Parameter nodes](../parameters/parameter-nodes.md) |
| `StaticSwitchParameter '{Name}' True input: {Detail}` | the true branch failed to evaluate | see the inner message | [Parameter nodes](../parameters/parameter-nodes.md) |
| `StaticSwitchParameter '{Name}' requires True=... and False=... inputs.` | one or both branches missing; `A=` / `B=` and positional 0 / 1 are accepted aliases | supply both branches | [Parameter nodes](../parameters/parameter-nodes.md) |
| `StaticSwitchParameter '{Name}': {Detail}` | wrapper around a switch-node failure | see the inner message | [Parameter nodes](../parameters/parameter-nodes.md) |
| `Texture Path root '{Root}' has an invalid plugin name.` | the plugin name contains characters outside `[A-Za-z0-9_]` | use a valid plugin name | [Path](../parameters/path.md) |
| `Texture Path root '{Root}' references plugin '{Plugin}', but no enabled plugin with that name was found.` | `FindPlugin` returned nothing | check the plugin name | [Path](../parameters/path.md) |
| `Texture Path root '{Root}' references plugin '{Plugin}', but the plugin cannot contain content.` | `CanContainContent()` is false | enable content in the plugin descriptor | [Path](../parameters/path.md) |
| `Texture Path root '{Root}' references plugin '{Plugin}', but the plugin is not enabled.` | `IsEnabled()` is false | enable the plugin | [Path](../parameters/path.md) |
| `Texture Path(...) requires a non-empty asset path.` | `Path( … )` with an empty path | supply a path | [Path](../parameters/path.md) |
| `Texture defaults must use Path(Game\|Engine\|Plugin.PluginName, "/Folder/Asset"), Path("/Game/Folder/Asset"), or a bare "/Game/Folder/Asset".` | the default is not one of the three accepted forms | use one of them | [Path](../parameters/path.md) |
| `Texture property '{Name}' could not load asset '{Path}'.` | the explicit default failed to load | check the path | [Path](../parameters/path.md) |
| `Texture property '{Name}' could not load default {Type} asset '{Path}'.` | the engine fallback texture failed to load | supply an explicit default | [Compact types](../parameters/compact-types.md) |
| `Texture property '{Name}' with type Texture2DArray requires an explicit default asset.` | no engine default exists for `Texture2DArray` | supply `= Path( … )` | [Compact types](../parameters/compact-types.md) |
| `Unexpected trailing tokens after texture Path(...) reference.` | text after the closing `)` | end the value at `)` | [Path](../parameters/path.md) |
| `Unsupported asset Path root '{Root}'. Use Game, Engine, or Plugin.PluginName.` | an unrecognized root in a metadata/collection reference | use a supported root | [Path](../parameters/path.md) |
| `Unsupported property type '{Type}'.` | the type token matched no compact type, parameter node token or `UE.` prefix | check the token against the type catalogue | [Types](../language/types.md) |
| `Unsupported texture Path root '{Root}'. Use Game, Engine, or Plugin.PluginName.` | an unrecognized root in a texture default | use a supported root | [Path](../parameters/path.md) |
| `property '{Name}': {Detail}` | wrapper around a generic parameter-node default-value write | see the inner message | [Parameter nodes](../parameters/parameter-nodes.md) |
| `{Context} texture property '{Name}' expects {Expected} but '{Path}' is a '{Class}'.` | the assigned texture's dimension does not match the declared type; `{Context}` is `Const` or `Texture` | assign a texture of the right dimension, or use `TextureObjectParameter`, which takes its dimension from the asset | [Compact types](../parameters/compact-types.md) |
| `{File}: Property '{Name}' is declared more than once. Property names must be unique.` | two `Properties` entries whose names are equal ignoring case | rename one | [Properties](../language/properties.md) |
| `{Kind} '{Name}' property '{Property}' conflicts with another property or input name.` | a material-function property collides with an input name, ignoring case | rename one | [ShaderFunction](../language/shader-function.md) |
| `{Kind} '{Name}' property '{Property}': {Detail}` | a material-function property node failed to build | see the inner message | [ShaderFunction](../language/shader-function.md) |

---

## Settings

The `Settings` section of a `Shader` (special keys plus the reflected `UMaterial` property path), and
the four keys a material-function `Settings` honours.

`{Key}` echoes the **lower-cased** stored key, not the spelling that was typed — the parser
normalizes keys before storing them.

| Message | Cause | Fix | See |
| :-- | :-- | :-- | :-- |
| `Array index {Index} is out of range for setting '{Setting}' (max {Max}).` | `[N]` beyond the property's `ArrayDim` | use an index below the maximum | [Shader settings](../settings/material.md) |
| `Failed to create a transient material for Settings validation.` | the probe `UMaterial` could not be allocated | report as a bug | [Shader settings](../settings/material.md) |
| `Invalid array index '{Index}' in setting segment '{Segment}'.` | the `[ … ]` index is not a non-negative integer | use an integer | [Shader settings](../settings/material.md) |
| `Invalid array setting segment '{Segment}'.` | malformed `[` / `]` — missing `]`, `]` before `[`, `]` not last, or empty name before `[` | fix the segment | [Shader settings](../settings/material.md) |
| `Invalid boolean value '{Value}' for {Key}.` | a setting the generator reads as a boolean was given text that is neither `true` nor `false` (matched case-insensitively) | write `true` or `false` | [Material settings](../settings/material.md) |
| `Invalid empty setting key in '{Statement}'.` | the key normalized to the empty string | supply a key | [Settings](../settings/index.md) |
| `Invalid material setting path '{Key}'.` | the key produced no path segments | supply a key | [Shader settings](../settings/material.md) |
| `Invalid material setting target.` | the root object handed to the resolver was null | report as a bug | [Shader settings](../settings/material.md) |
| `Invalid setting declaration '{Statement}'.` | the statement has no top-level `=` | write `Key = Value;` | [Settings](../settings/index.md) |
| `Invalid value '{Value}' for setting '{Key}'. {Detail}` | the reflected write failed | see the value-writer message in the [Builtins](#builtins-ue-math-substrate) table | [Shader settings](../settings/material.md) |
| `Setting '{Setting}' is not an indexed array property.` | `[N]` used on a property whose `ArrayDim` is 1 | drop the index | [Shader settings](../settings/material.md) |
| `Setting '{Setting}' requires an explicit [index].` | a fixed-array property addressed without `[N]` | add `[0]`, `[1]`, … | [Shader settings](../settings/material.md) |
| `Setting path '{Key}' cannot continue through '{Segment}'.` | a non-terminal segment is not a struct property | shorten the path | [Shader settings](../settings/material.md) |
| `Setting path segment cannot be empty.` | a `.`-delimited segment is empty, as in `Foo..Bar` | remove the double dot | [Shader settings](../settings/material.md) |
| `ShadingModel="Substrate" requires Unreal Engine 5.4 or newer.` | `Substrate` or `Strata` on UE 5.3 | use another shading model, or move to UE 5.4+ | [Material enums](../settings/material-enums.md) |
| `Unsupported Backend '{Value}'. Supported values: Graph, Instance, ThinCustom.` | an unrecognized `Backend` value | use `Graph` or `ThinCustom`; `Instance` is a deprecated alias for `ThinCustom` | [Backend](../settings/backend.md) |
| `Unsupported BlendMode/RenderType '{Value}'.` | the value matched no `EBlendMode` name, alias or project mapping | consult the enum catalogue | [Material enums](../settings/material-enums.md) |
| `Unsupported MaterialDomain '{Value}'.` | the value matched no `EMaterialDomain` name, alias or project mapping | consult the enum catalogue | [Material enums](../settings/material-enums.md) |
| `Unsupported ShadingModel '{Value}'.` | the value matched no `EMaterialShadingModel` name, alias or project mapping | consult the enum catalogue | [Material enums](../settings/material-enums.md) |
| `Unsupported material setting '{Key}'.` | no `FProperty` on `UMaterial` matched the key by alias, normalized name, `b`-prefix strip, or `DisplayName` | check the property name in the material's details panel | [Shader settings](../settings/material.md) |
| `{File}: Base.FrontMaterial and Base.MaterialAttributes cannot be used by the same Shader.` | both bindings present | bind only one | [Output bindings](../language/output-bindings.md) |
| `{File}: Base.FrontMaterial requires ShadingModel="Substrate" or no explicit ShadingModel setting.` | a conflicting explicit `ShadingModel` alongside a `Base.FrontMaterial` binding | remove the setting, or set it to `Substrate` | [Material enums](../settings/material-enums.md) |
| `{Kind} '{Name}': ExposeToLibrary must be true or false.` | a non-boolean `ExposeToLibrary` in a material-function `Settings` | use `true` or `false` | [Function settings](../settings/function.md) |

---

## Asset generation and saving

Pipeline gates, asset naming and root resolution, node-graph population, the ownership guard, and
package saving.

| Message | Cause | Fix | See |
| :-- | :-- | :-- | :-- |
| `Asset '{ObjectPath}' already exists and is not a DreamShader instance material. Delete it (or remove Backend="Instance") before switching backends.` | ThinCustom backend, target path holds another UClass | delete or move the existing asset | [Backend](../settings/backend.md) |
| `Asset '{ObjectPath}' already exists and is not a Material.` | Graph backend, target path holds another UClass | delete or move the existing asset | [Asset paths](../generation/asset-paths.md) |
| `Asset '{ObjectPath}' already exists and is not a MaterialFunction asset.` | the target path of a function block holds another UClass | delete or move the existing asset | [Asset paths](../generation/asset-paths.md) |
| `Asset '{ObjectPath}' already exists and was not generated by DreamShader. Rename your function or move/delete the existing asset before regenerating.` | ownership guard — the saved function asset carries no `DreamShader.SourceFile` metadata | rename the block, or delete the asset | [Regeneration](../generation/regeneration.md) |
| `Asset '{ObjectPath}' already exists and was not generated by DreamShader. Rename your shader or move/delete the existing asset before regenerating.` | ownership guard — the saved material carries no `DreamShader.SourceFile` metadata | rename the `Shader`, or delete the asset | [Regeneration](../generation/regeneration.md) |
| `Asset '{ObjectPath}' already exists as '{Actual}', but {Kind} generation requires '{Expected}'. Delete or move the existing asset and regenerate it.` | the block kind changed, e.g. `ShaderFunction` → `ShaderLayer` | delete the old asset and regenerate | [ShaderLayer](../language/shader-layer.md) |
| `Cannot create a persisted ThinCustom base without an instance for '{Name}'.` | the ThinCustom persist path ran with no instance to host the hidden base | report as a bug | [In-memory materials](../generation/in-memory.md) |
| `DreamShader asset name '{Name}' produced an invalid asset name.` | the leaf segment became empty after `SanitizeObjectName` | use a name with legal characters | [Asset paths](../generation/asset-paths.md) |
| `DreamShader asset name must resolve to a non-empty asset path.` | `Name` is empty after trimming and slash stripping | supply a name | [Asset paths](../generation/asset-paths.md) |
| `DreamShader asset path '{Path}' is not a valid Unreal object path.` | the assembled package path failed `IsValidObjectPath` | shorten or clean the name and root | [Asset paths](../generation/asset-paths.md) |
| `DreamShader Root '{Root}' has an invalid package root.` | an explicit `/Root` that does not survive `SanitizeObjectName` | use a legal root | [Asset paths](../generation/asset-paths.md) |
| `DreamShader Root '{Root}' has an invalid plugin name.` | the plugin name is empty or does not survive `SanitizeObjectName` | use a valid plugin name | [Asset paths](../generation/asset-paths.md) |
| `DreamShader Root '{Root}' must reference a project plugin under '{Dir}'.` | the plugin is an engine or marketplace plugin, not a project plugin | move the plugin under `<Project>/Plugins`, or pick another root | [Asset paths](../generation/asset-paths.md) |
| `DreamShader Root '{Root}' references project plugin '{Plugin}', but its Content directory does not exist: '{Dir}'.` | the plugin has no `Content` folder | create it | [Asset paths](../generation/asset-paths.md) |
| `DreamShader Root '{Root}' references project plugin '{Plugin}', but no enabled plugin with that name was found.` | `FindPlugin` returned nothing | check the plugin name | [Asset paths](../generation/asset-paths.md) |
| `DreamShader Root '{Root}' references project plugin '{Plugin}', but the plugin cannot contain content.` | `CanContainContent()` is false | enable content in the plugin descriptor | [Asset paths](../generation/asset-paths.md) |
| `DreamShader Root '{Root}' references project plugin '{Plugin}', but the plugin content is not mounted.` | *(UE 5.6+ only)* the plugin content is not mounted | enable and mount the plugin | [Asset paths](../generation/asset-paths.md) |
| `DreamShader Root '{Root}' references project plugin '{Plugin}', but the plugin is not enabled.` | `IsEnabled()` is false | enable the plugin | [Asset paths](../generation/asset-paths.md) |
| `DreamShader file '{File}' did not contain any material, ShaderFunction, ShaderLayer, or ShaderLayerBlend assets to generate.` | the file declared nothing generatable and nothing skippable | add a generatable block | [Generation](../generation/index.md) |
| `DreamShader header '{File}' does not generate assets directly. Recompile dependent .dsm or .dsf files instead.` | asset generation was requested for a `.dsh` | compile the `.dsm` / `.dsf` that imports it | [Source files](../language/source-files.md) |
| `DreamShader source '{File}' cannot generate a material asset directly.` | material generation was requested for a `.dsh` or `.dsf` | request function-asset generation instead | [Source files](../language/source-files.md) |
| `Failed to create a MakeMaterialAttributes node for '{Name}'.` | the seed node for a `MaterialAttributes` output could not be created | report as a bug | [MaterialAttributes](../graph/material-attributes.md) |
| `Failed to create instance material '{ObjectPath}'.` | `NewObject` failed for the thin instance | report as a bug | [Backend](../settings/backend.md) |
| `Failed to create material '{ObjectPath}'.` | the material factory returned null | report as a bug | [Generation](../generation/index.md) |
| `Failed to create material function '{ObjectPath}'.` | the function factory returned null | report as a bug | [Generation](../generation/index.md) |
| `Failed to create package '{PackageName}'.` | `CreatePackage` failed | check the path and permissions | [Asset paths](../generation/asset-paths.md) |
| `Failed to create ThinCustom base material for '{Name}'.` | the transient hidden base `UMaterial` could not be created | report as a bug | [In-memory materials](../generation/in-memory.md) |
| `Failed to create ThinCustom base material for instance '{ObjectPath}'.` | the persisted hidden base subobject could not be created | report as a bug | [In-memory materials](../generation/in-memory.md) |
| `Generated DreamShader asset '{ObjectPath}' could not be saved.` | `SavePackages` failed for a single asset | check source control and file permissions | [Generation](../generation/index.md) |
| `Generated DreamShader asset packages could not be saved.` | `SavePackages` failed for a dependent asset pair; each failed path is appended | check source control and file permissions | [Generation](../generation/index.md) |
| `Invalid output source or target expression.` | an `Outputs` binding had neither a usable source nor a usable target | fix the binding | [Output bindings](../language/output-bindings.md) |
| `Output target '{Target}' could not resolve MaterialExpression class '{Class}'.` | the `Class=` of an `Expression( … )` binding did not resolve | check the class name | [Output bindings](../language/output-bindings.md) |
| `Output target '{Target}' does not have Pin[{Index}].` | the pin index is beyond the node's input count | use a valid pin index | [Output bindings](../language/output-bindings.md) |
| `Output target '{Target}' failed to create '{Class}'.` | node creation failed | report as a bug | [Output bindings](../language/output-bindings.md) |
| `Output target '{Target}': '{Key}' is not a property on '{Class}'.` | an `Expression( … )` argument that matched no UPROPERTY | check the property name | [Output bindings](../language/output-bindings.md) |
| `Output target '{Target}': inline input property '{Key}' is not supported yet. Bind through .Pin[index] instead.` | an input pin addressed as an argument | bind it as a separate `.Pin[i]` statement | [Output bindings](../language/output-bindings.md) |
| `Output target '{Target}': {Detail}` | the reflected write failed | see the value-writer message | [Output bindings](../language/output-bindings.md) |
| `Output target pin '{Target}' is bound more than once.` | two bindings target the same node pin | bind each pin once | [Output bindings](../language/output-bindings.md) |
| `Output '{Name}': {Detail}` | wrapper around a material-function output failure | see the inner message | [ShaderFunction](../language/shader-function.md) |
| `ShaderLayer '{Name}' must declare at most one input, and it must be MaterialAttributes. Use Properties for layer controls.` | a layer with extra or wrongly typed inputs | move the controls into `Properties` | [ShaderLayer](../language/shader-layer.md) |
| `ShaderLayerBlend '{Name}' must declare exactly two inputs, both MaterialAttributes. Use Properties for blend controls.` | a blend with the wrong input shape | declare exactly two `MaterialAttributes` inputs | [ShaderLayer](../language/shader-layer.md) |
| `{Context} contains an invalid folder segment.` | a folder segment of `Name=` or `Root=` is empty after `ObjectTools::SanitizeObjectName`; `{Context}` names the offending path | remove the illegal characters from that folder | [Asset paths](../generation/asset-paths.md) |
| `{File}: .dsf files cannot define top-level Shader blocks.` | a `Shader` block reached the generator from a `.dsf` | move it to a `.dsm` | [Source files](../language/source-files.md) |
| `{File}: Base.FrontMaterial expects a Substrate value and cannot be driven by a material Custom node. Use a Graph block and Substrate.* nodes.` | the ThinCustom/HLSL path cannot produce a Substrate value | use the Graph backend with `Substrate.*` nodes | [Substrate](../builtins/substrate.md) |
| `{File}: Failed to create the material Custom node.` | node creation failed | report as a bug | [Backend](../settings/backend.md) |
| `{File}: Failed to find material property '{Property}' while connecting '{Name}'.` | the material property input could not be located | report as a bug | [Output bindings](../language/output-bindings.md) |
| `{File}: Failed to find material property '{Property}' while connecting Graph output '{Name}'.` | the material property input could not be located | report as a bug | [Output bindings](../language/output-bindings.md) |
| `{File}: Failed to resolve Custom output '{Name}'.` | a Custom-node output could not be resolved | report as a bug | [Backend](../settings/backend.md) |
| `{File}: Graph blocks do not support binding Outputs to the reserved name 'return'.` | `return` bound in a `Shader` that has a `Graph` block | bind a named variable | [Output bindings](../language/output-bindings.md) |
| `{File}: Graph output '{Name}' does not match its declared type.` | the value assigned in `Graph` has the wrong shape | match the declaration | [Conversions](../graph/conversions.md) |
| `{File}: Material output '{Name}' expects a MaterialAttributes value.` | a numeric value bound to `Base.MaterialAttributes` | supply a `MaterialAttributes` value | [MaterialAttributes](../graph/material-attributes.md) |
| `{File}: Material output '{Name}' expects a Substrate value and cannot be driven by a material Custom node. Use a Graph block and Substrate.* nodes.` | the Custom-node path cannot produce a Substrate value | use the Graph backend | [Substrate](../builtins/substrate.md) |
| `{File}: Material output '{Name}' expects a Substrate value.` | a numeric value bound to a Substrate target | supply a `Substrate.*` result | [Substrate](../builtins/substrate.md) |
| `{File}: Material output '{Name}' expects a numeric value, but got Substrate.` | a `Substrate` value bound to a numeric target | bind it to `Base.FrontMaterial` | [Substrate](../builtins/substrate.md) |
| `{File}: Output '{Name}' is declared as Substrate and cannot be generated by a material Custom node. Use a Graph block and Substrate.* nodes.` | a `Substrate` output on the Custom-node path | use the Graph backend | [Substrate](../builtins/substrate.md) |
| `{File}: Outputs block is required.` | the `Shader` declared no output bindings | add at least one `Base.<Property> = …;` | [Output bindings](../language/output-bindings.md) |
| `{File}: This file does not define a top-level Shader block.` | material generation was requested for a file with no `Shader` | compile it as a function file, or add a `Shader` | [Shader](../language/shader.md) |
| `{Kind} '{Name}' failed to create input '{Input}'.` | the `FunctionInput` node could not be created | report as a bug | [ShaderFunction](../language/shader-function.md) |
| `{Kind} '{Name}' failed to create output '{Output}'.` | the `FunctionOutput` node could not be created | report as a bug | [ShaderFunction](../language/shader-function.md) |
| `{Kind} '{Name}' failed to create the function Custom node.` | node creation failed | report as a bug | [ShaderFunction](../language/shader-function.md) |
| `{Kind} '{Name}' failed to resolve generated input '{Input}'.` | a generated input could not be looked up | report as a bug | [ShaderFunction](../language/shader-function.md) |
| `{Kind} '{Name}' input '{Input}': {Detail}` | an input's preview default failed | see the inner message | [Inputs / Outputs / Results](../language/inputs-outputs.md) |
| `{Kind} '{Name}' must declare exactly one MaterialAttributes output.` | a `ShaderLayer` / `ShaderLayerBlend` with the wrong output shape | declare exactly one `MaterialAttributes` output | [ShaderLayer](../language/shader-layer.md) |
| `{Kind} '{Name}' must provide a Graph block.` | a material-function block with an empty `Code` | add `Graph = { … }` | [ShaderFunction](../language/shader-function.md) |
| `{Kind} '{Name}' output '{Output}' does not match its declared type '{Type}'.` | the assigned value has the wrong shape | match the declaration | [Conversions](../graph/conversions.md) |
| `{Kind} '{Name}' output '{Output}' uses Substrate, which is not supported by HLSL Custom node functions. Use a Graph block and Substrate.* nodes.` | a `Substrate` output on the HLSL path | restructure | [Substrate](../builtins/substrate.md) |
| `{Kind} '{Name}' output '{Output}' was never assigned an expression.` | the `Graph` body never assigned the output | assign it | [ShaderFunction](../language/shader-function.md) |
| `{Kind} '{Name}' output '{Output}': {Detail}` | an output value failed to evaluate | see the inner message | [ShaderFunction](../language/shader-function.md) |
| `{Kind} '{Name}': {Detail}` | wrapper around a Custom-node preparation failure | see the inner message | [Generated HLSL](../generation/generated-hlsl.md) |

Success and status messages returned by the same pipeline — useful when scripting a compile:

| Message | Meaning |
| :-- | :-- |
| `DreamShader file '{File}' contains GraphFunction declarations only; no assets were generated.` | success; only a helper include was produced |
| `DreamShader file '{File}' contains VirtualFunction declarations only; no assets were generated.` | success; the file only declares existing assets |
| `Generated DreamShader helper include '{Path}' from {File}.` | the generated `.ush` was written |
| `Generated DreamShader thin-custom material {ObjectPath} from {File}.` | ThinCustom backend succeeded |
| `Generated {ObjectPath} from {File}.{Suffix}` | Graph backend succeeded; `{Suffix}` is ` (virtual)` for an in-memory material |
| `Generated {Kind} {ObjectPath} from {File}.` | a material function succeeded |
| `Skipped {ObjectPath} from {File}; source hash is unchanged.` | the source-hash cache short-circuited; pass `-Force` to override |

---

## Commandlet

`-run=DreamShader` and the cook-time generation hook. None of these reach the diagnostics store —
the bridge does not run in a commandlet process.

| Message | Cause | Fix | See |
| :-- | :-- | :-- | :-- |
| `Decompile did not produce source text.` | the decompiler reported failure with no error text | report as a bug | [Decompiler](../tools/decompiler.md) |
| `DreamShader compile requires a .dsm or .dsf file: {File}` | the file is not a DreamShader source, or is a `.dsh` header | pass a `.dsm` or `.dsf`; the run continues but is marked failed | [Commandlet](../tools/commandlet.md) |
| `DreamShader cook generation failed for {Count} source file(s); aborting the cook. See the [Cook] Failed entries above.` | one or more sources failed during cook-time generation; logged at **Fatal**, which aborts the cook | fix the sources, then re-cook | [Commandlet](../tools/commandlet.md) |
| `DreamShader could not load asset '{Path}'.` | `StaticLoadObject` failed for both the normalized and the raw path | check the object path; `/Game/Path/Asset` is auto-expanded to `/Game/Path/Asset.Asset` | [Commandlet](../tools/commandlet.md) |
| `DreamShader decompile supports Material and MaterialFunction assets only: {Path}` | the asset is neither a `UMaterial` nor a `UMaterialFunction` family asset | pick a supported asset | [Decompiler](../tools/decompiler.md) |
| `DreamShader failed to create output directory '{Dir}'.` | `MakeDirectory` failed | check permissions | [Decompiler](../tools/decompiler.md) |
| `DreamShader failed to decompile '{Path}': {Detail}` | the decompiler returned failure | see the inner message | [Decompiler](../tools/decompiler.md) |
| `DreamShader failed to resolve an output file path.` | no `-Out` and no computable default | pass `-Out="…"` | [Decompiler](../tools/decompiler.md) |
| `DreamShader failed to write decompiled source '{File}'.` | the file could not be written | check permissions and file locks | [Decompiler](../tools/decompiler.md) |
| `Unknown DreamShader command '{Command}'.` *(followed by the usage banner)* | the first bare token is not `compile`, `generate`, `decompile` or `export` | use a supported sub-command. A stray bare token in first position is consumed as the command name | [Commandlet](../tools/commandlet.md) |

The usage banner, printed verbatim whenever no command is given, when `compile` has neither
`-Source`/`-File` nor `-All`, and when `decompile` has no `-Asset`:

```text
Usage:
  -run=DreamShader compile -Source="C:/Project/DShader/File.dsm" [-Force]
  -run=DreamShader compile -All [-Force]
  -run=DreamShader decompile -Asset="/Game/Path/Asset.Asset" [-Out="C:/Project/DShader/Decompiled/File.dsm"]
Supported asset types: Material -> .dsm, MaterialFunction -> .dsf.
```

---

## VirtualFunction sync

The startup service that re-reads every `VirtualFunction` declaration and refreshes it from its
`UMaterialFunction` asset, plus the editor actions on the Material Function toolbar and context menu.

Sync diagnostics reach the store with `stage = virtualFunctionSync`, `code = virtual-function-sync`
and `source = DreamShader VirtualFunction`. The editor-action messages are toast notifications and
log lines; they are not stored.

| Message | Cause | Fix | See |
| :-- | :-- | :-- | :-- |
| `Created VirtualFunction file but could not open it: {File}` | the `.dsh` was written but VSCode could not be launched | open the file manually | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `DreamShader could not find a VirtualFunction definition for {Asset}.` | *Open VirtualFunction* on an asset with no declaration | use *CreateVirtualFunction* instead | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `DreamShader could not find the selected Material Function.` | the selected asset vanished between menu build and action | reselect the asset | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `DreamShader could not open VirtualFunction file: {File}` | the editor launch failed | open the file manually | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `DreamShader could not read VirtualFunction source file '{File}'.` | the source file is unreadable; reported at line 1, column 1 | check file locks and permissions | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `DreamShader failed to build VirtualFunction call: {Detail}` | the call snippet could not be produced | see the inner message | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `DreamShader failed to build VirtualFunction reference: {Detail}` | the reference snippet could not be produced | see the inner message | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `DreamShader failed to build VirtualFunction: {Detail}` | the declaration text could not be produced | see the inner message | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `DreamShader failed to create directory: {Dir}` | `DShader/VirtualFunctions` could not be created | check permissions | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `DreamShader failed to update VirtualFunction source file '{File}'.` | the refreshed declaration could not be written back; reported at line 1, column 1 | check file locks and source control | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `DreamShader failed to write VirtualFunction file: {File}` | the new `.dsh` could not be written | check permissions | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `Expected exactly one VirtualFunction block.` | a re-parsed declaration region yielded zero or several blocks | keep one `VirtualFunction` per declaration region | [VirtualFunction](../language/virtual-function.md) |
| `MaterialFunction '{Name}' does not have a valid package path.` | the asset's outermost package name is empty or does not start with `/` | re-save the asset somewhere under a mounted content root | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `MaterialFunction '{Name}' does not expose any outputs.` | the asset has no `FunctionOutput` node | add an output to the material function | [VirtualFunction](../language/virtual-function.md) |
| `VirtualFunction '{Name}' asset reference is invalid: {Detail}` | `Options.Asset` did not resolve; the raw literal is reported as the diagnostic's `assetPath` | fix the `Path( … )` | [Path](../parameters/path.md) |
| `VirtualFunction '{Name}' could not be refreshed from MaterialFunction '{Path}': {Detail}` | the declaration builder failed | see the inner message | [VirtualFunction tools](../tools/virtual-function-tools.md) |
| `VirtualFunction '{Name}' does not expose any outputs.` | the declaration builder found no outputs to emit | add an output to the material function | [VirtualFunction](../language/virtual-function.md) |
| `VirtualFunction '{Name}' references missing MaterialFunction '{Path}'.` | `LoadObject` returned null | restore the asset, or update `Options.Asset` | [VirtualFunction](../language/virtual-function.md) |
| `VirtualFunction attributes are missing a closing ')'.` | unbalanced `(` in the header | balance the parentheses | [VirtualFunction](../language/virtual-function.md) |
| `VirtualFunction body is missing a closing '}'.` | unbalanced `{` in the body | balance the braces | [VirtualFunction](../language/virtual-function.md) |
| `VirtualFunction declaration is invalid: {Detail}` | the re-parse failed; the parse error is carried in `detail` | fix the declaration | [VirtualFunction](../language/virtual-function.md) |
| `VirtualFunction name cannot be empty.` | the declaration builder was given an empty name | supply a name | [VirtualFunction](../language/virtual-function.md) |

> [!NOTE]
> The sync service matches the bare keyword `VirtualFunction` **case-sensitively**, with identifier
> boundaries on both sides — the same rule the parser uses for top-level keywords. A lower-case
> `virtualfunction` is invisible to both.

---

## Warnings

Warnings never fail a compile and never enter the diagnostics store. They are appended to the compile
result message or logged under `LogDreamShader`.

### Deprecation warnings

| Message | Deprecated construct | Replacement | Deprecated in |
| :-- | :-- | :-- | :-- |
| `MaterialLayer is deprecated; use ShaderLayer instead.` | `MaterialLayer( … ) { … }` | `ShaderLayer( … ) { … }` | 1.3.0 |
| `MaterialLayerBlend is deprecated; use ShaderLayerBlend instead.` | `MaterialLayerBlend( … ) { … }` | `ShaderLayerBlend( … ) { … }` | 1.3.0 |

> [!WARNING]
> These are the **only** two deprecations that warn. Every other deprecated or aliased spelling is
> accepted in silence:
>
> | Deprecated / aliased spelling | Replacement | Diagnostic |
> | :-- | :-- | :-- |
> | `Settings = { Backend = "Instance"; }` | `Backend = "ThinCustom"` *(alias since 1.5.0)* | none |
> | project `Default Compiler Backend = Instance` | `ThinCustom` *(alias since 1.5.0)* | none |
> | `Results = { … }` in a function block | `Outputs = { … }` | none |
> | `Properties = { … }` in a `VirtualFunction` | `Inputs = { … }` | none |
> | `Settings = { … }` in a `VirtualFunction` | `Options = { … }` | none |
>
> The generated declarations that the decompiler and the VirtualFunction sync service emit always use
> the modern spelling, and `{Kind}` in every diagnostic reports the modern name even when the legacy
> keyword was typed. Migrating is therefore invisible in the output.

### Other warnings

| Message | Meaning |
| :-- | :-- |
| `'{Class}' does not expose the '{Field}' organization field; ignoring it for this parameter.` | `Group`, `SortPriority` or `Desc` metadata was written to a node class that has no such property — for example `UMaterialExpressionDynamicParameter`. Only these three fields soft-fail; any other missing key is an error |
| `DreamShader commandlet found no source files to compile.` | `compile -All` resolved an empty list. The commandlet still exits **0** |
| `Failed to open DreamShader bridge database for diagnostics: {Detail}` | `bridge.db` could not be opened; the JSON sinks are still written |
| `Failed to persist the parameter-visibility flag on the instance host material: {Detail}` | the ThinCustom host material could not be updated |
| `In-memory material mode: '{ObjectPath}' already exists as a saved asset, which shadows in-memory regeneration. Delete the saved asset to make it fully in-memory.` | a saved `.uasset` at the target path takes precedence over the in-memory material |
| `No Outputs block was provided. Generation requires explicit material property bindings.` | a `Shader` declared no output bindings. The parse succeeds; generation then fails with `{File}: Outputs block is required.` |
| `Skipping automatic layout for large DreamShader graph ({Count} nodes). Existing generated positions will be used.` | logged at `Display`; auto-layout was skipped for a large graph |

---

## Silent behaviour

Conditions that produce **no** diagnostic at all. Each is real, observable behaviour of the current
implementation.

> [!WARNING]
> **A `Graph` expression silently truncates at any character the lexer does not know.**
> The Graph tokenizer recognizes `( ) , . + - * / =` and `::`; every other character — `% ! < > & | ^
> ~ ? [ ] { } ; # @ $ '` and a lone `:` — becomes an end-of-expression token, and parsing accepts an
> expression that ends there. So `a % b`, `a & b`, `a | b`, `a ^ b`, `a << b`, `a ? b : c` and `v[0]`
> all evaluate as just `a` / `v`, with no message. Inside an `if` condition, `if (a > 0 && b > 0)`
> silently becomes `if (a > 0)`. There is no workaround other than avoiding the operators — use
> nested `if` statements for conjunctions, and `lerp` / `min` / `max` in place of a ternary. See
> [Unsupported constructs](../graph/unsupported.md).

| Condition | Observable result |
| :-- | :-- |
| `a+=b` written with no spaces | parses as an assignment to a variable literally named `a+`; `a` is unchanged |
| An unterminated `/* … */` comment | consumed to end of file and accepted. An unterminated `{` block and an unterminated `"` attribute value *do* error |
| A duplicate header attribute (`Shader(Name="A", Name="B")`) | the last value wins |
| A duplicate `Settings` key | the last value wins. Duplicate **metadata** keys, **UE builtin** arguments, **Expression** arguments and **Layout** arguments are all hard errors |
| An unknown key in a material-function `Settings` block | ignored. Only `Description`, `UserExposedCaption`, `ExposeToLibrary` and `LibraryCategories` are read |
| An unknown or positional argument to a *registered* `UE.*` Graph builtin | ignored. The generic `UE.Expression` path rejects both |
| `[Texture=Path(Game,"Typo")]` on a texture-sample node | the slot is set to `nullptr` and the write reports success; the material compiles with an unbound sampler |
| A default value (`= …`) on a material function's `Outputs` / `Results` entry | parsed and never used |
| `UE.CollectionParam` `Group` / `SortPriority` below UE 5.7 | validated, then dropped |
| `UE.TransformPosition` `PeriodicWorldTileSize` below UE 5.5 | dropped |
| `float Strength = 1.0f;` or `float Strength = 1abc;` in `Properties` | both parse as `1.0`; the numeric converter only re-validates a zero result |
| `float3(1,0,0)`, `vec3(1,0,0)`, `(1,0,0)` and `Nonsense(1,0,0)` as a property default | all identical — the text before `(` is ignored |
| A second `Layout = { … }` section | replaces the first; `Properties`, `Inputs` and `Outputs` append instead |
| A `.dsh` / `.dsf` under `DShader/Packages` | never auto-compiled and never shown in the Gen page |

---

## Example

A parse error inside a `Graph` block, as it reaches each surface.

```c
// DShader/Materials/M_Sample.dsm
Shader(Name="Materials/M_Sample")
{
    Properties = { vec3 Tint = vec3(1.0, 0.4, 0.1); }
    Outputs    = { vec3 Color; Base.EmissiveColor = Color; }
    Graph      = {
        vec2 UV = UE.TexCoord(Index = 0);
        Color   = Tin * UV.x;          // typo: Tin, not Tint
    }
}
```

Output Log:

```text
LogDreamShader: Error: I:/Project/DShader/Materials/M_Sample.dsm(8,19): Unknown Graph identifier 'Tin'.
```

`Saved/DreamShader/Bridge/diagnostics.json`:

```json
{
  "version": 1,
  "updatedAtUtc": "2026-07-29T11:04:22Z",
  "files": [
    {
      "path": "I:/Project/DShader/Materials/M_Sample.dsm",
      "diagnostics": [
        {
          "message": "Unknown Graph identifier 'Tin'.",
          "detail": "I:/Project/DShader/Materials/M_Sample.dsm(8,19): Unknown Graph identifier 'Tin'.",
          "stage": "generate",
          "code": "generate-error",
          "line": 8,
          "column": 19,
          "severity": "error",
          "source": "DreamShader Generate"
        }
      ]
    }
  ]
}
```

The same record is written to `Bridge/diagnostics/<md5>.json` and to the `diagnostics` table of
`Bridge/bridge.db`, and appears in the Dream Shader Gen page's source list.

## See also

- [Bridge](../tools/bridge.md) — the request files, the WebSocket protocol, and the diagnostic artifacts
- [Material Content Browser](../tools/material-browser.md) — the tab that renders the source list
- [Commandlet](../tools/commandlet.md) — `-run=DreamShader`, its switches and exit codes
- [Generation pipeline](../generation/index.md) — the stages these tables are grouped by
- [Unsupported constructs](../graph/unsupported.md) — the silent-truncation pitfalls in full
- [Keywords](../language/keywords.md) — the case-sensitivity rules behind several "not found" errors
- [Source files](../language/source-files.md) — the `.dsm` / `.dsh` / `.dsf` restriction that several messages enforce
- [import](../language/import.md) — resolution order, cycles, and how line numbers survive inlining
- [Types](../language/types.md) — the type-token catalogue behind every "unsupported type" message
- [Testing](../contributing/testing.md) — the corpus fixtures that pin many of these messages
