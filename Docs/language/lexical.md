# Lexical elements

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Lexical elements**

The character-level rules of DreamShaderLang: whitespace, comments, identifiers, case sensitivity,
string and numeric literals, and how a section body is split into statements.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` |
| Kind | lexical structure |
| Applies to | the declaration grammar; differences in the `Graph` expression grammar are called out per section |

## Synopsis

```c
<token> := <identifier> | <keyword> | <string-literal> | <numeric-text> | <punctuation>

<identifier>     := { <letter> | _ } { <letter> | <digit> | _ }…
<qualified-name> := <identifier> :: <identifier>
<string-literal> := " { <character> | \<escape> }… "
<punctuation>    := { | } | ( | ) | [ | ] | ; | = | , | . | :: | #
```

Whitespace and comments may appear between any two tokens and are otherwise insignificant.

## Whitespace

Any character for which the engine's whitespace predicate is true separates tokens: space, tab,
carriage return, line feed and the other Unicode whitespace characters. Newlines carry no syntactic
weight in the declaration grammar; they matter only to line-oriented processing —
[`import`](import.md) directives, `#Region` directives inside a `Graph` body
([Layout](layout.md)), and diagnostic line numbers.

> [!WARNING]
> Two statement forms split on a **literal space character** rather than on whitespace, so a tab
> between a type and a name is a syntax error. See [Statement separation](#statement-separation).

## Comments

| Form | Rule |
| :-- | :-- |
| `// …` | line comment; runs to, but does not include, the next line feed |
| `/* … */` | block comment; ends at the first `*/` |

- **Block comments do not nest.** `/* a /* b */ c */` ends at the first `*/`; the trailing `c */` is
  code again.
- **An unterminated block comment is accepted silently.** A `/*` with no `*/` consumes the rest of
  the file and produces no diagnostic. Contrast an unterminated `{` block, which fails with
  `Unterminated block.`, and an unterminated string in an attribute value, which fails with
  `Unterminated string literal.`
- Comments are recognized identically everywhere in the declaration grammar, including while
  counting `{}`, `()` and `[]` nesting, so a brace or quote inside a comment never unbalances a
  block.
- Comment removal happens a second time, textually, before statements are split in `Properties`,
  `Settings`, `Outputs`, `Layout` and typed-parameter sections. That pass is string-aware and
  preserves the line feed that terminates a line comment, so line numbers survive.
- **`Graph` bodies are not comment-stripped by the declaration parser.** Comments inside a `Graph`
  block are stored verbatim with the body and are handled later by the expression tokenizer.
- There is no `#` preprocessor at the declaration level. `#Region` / `#EndRegion` are recognized
  only inside `Graph` bodies.

## Identifiers

| Position | Accepted characters |
| :-- | :-- |
| first | a letter or `_` |
| subsequent | a letter, a digit or `_` |

- There is no length limit.
- **Keywords are not reserved against identifiers.** Nothing rejects a property, variable, parameter
  or function named `Shader`, `Graph` or `float`; whether it then resolves is a matter for the
  context it appears in.
- A stricter check is applied to declaration *names* in `Inputs`, `Outputs`, `Results` and `Layout`
  calls: the whole trimmed token must match the identifier rule. Names in `Properties` are only
  required to be non-empty, so `Properties { float 1Bad = 0; }` parses.
- Namespace-qualified names use `::`, as in `Common::ApplyTint`. See [Namespace](namespace.md).

When a name reaches generated HLSL it is sanitized: every character outside `A–Z a–z 0–9 _` becomes
`_`, a leading digit gains a `_` prefix, runs of consecutive `__` collapse to one, and a result that
is empty or entirely underscores becomes `DreamShaderSymbol`. This is why `Common::ApplyTint` and
`Common_ApplyTint` collide in generated code.

## Case sensitivity

**Top-level block keywords are the only case-sensitive tokens.** Everything else keyword-like is
matched ignoring case.

| Construct | Case-sensitive | Reference |
| :-- | :-- | :-- |
| Top-level block keywords — `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `MaterialLayer`, `MaterialLayerBlend`, `VirtualFunction`, `Namespace`, `Function`, `GraphFunction` | **yes** | [Keyword index](keywords.md) |
| Section names — `Properties`, `Settings`, `Outputs`, `Inputs`, `Results`, `Options`, `Graph`, `Code`, `Layout` | no | [Keyword index](keywords.md) |
| Header attribute keys — `Name=`, `Root=`, `Asset=` | no | [Shader](shader.md) |
| `Settings` / `Options` keys, metadata keys, `UE.*` argument keys, `Expression( … )` argument keys, `Layout` argument keys | no — additionally lower-cased when stored | [Settings](../settings/index.md) |
| Type tokens — `float3`, `vec3`, `Texture2D`, `ScalarParameter`, … | no | [Types](types.md) |
| `const` prefix in `Properties` | no | [Properties](properties.md) |
| `opt` prefix in `Inputs` | no | [Inputs / Outputs](inputs-outputs.md) |
| `SelfContained` / `Inline` after `Function` | no | [Function](function.md) |
| `in` / `out` parameter qualifiers | no | [Function](function.md) |
| `true` / `false` in defaults | no | [Types](types.md) |
| `Path(` texture-reference keyword | no | [`Path(...)`](../parameters/path.md) |
| `Base.` binding prefix and `Expression(` | no | [Output bindings](output-bindings.md) |
| `.Pin[` pin selector | no | [Output bindings](output-bindings.md) |
| `Group("…")` property-scope head | no | [Properties](properties.md) |
| `Slider(` metadata shorthand | no | [Metadata block](../parameters/metadata.md) |
| `UE.` builtin prefix | no | [`UE.*` catalogue](../builtins/ue.md) |
| `#Region` / `#EndRegion` | no | [Layout](layout.md) |
| `Node(` / `Comment(` layout calls | no | [Layout](layout.md) |
| `import` | no | [`import`](import.md) |
| `default` call-argument sentinel | no | [Calls](../graph/calls.md) |
| `.dsm` / `.dsf` / `.dsh` extensions | no | [Source files](source-files.md) |

So `shader(Name="X")` and `SHADER(Name="X")` are both syntax errors, while
`properties = { … }`, `settings { domain = "ui"; }` and `Shader(name="X")` are all accepted.

A keyword match additionally requires a **right word boundary**: the character after the keyword must
not be a letter, a digit or `_`. This is what stops `ShaderFunction` from matching as `Shader`, and
`ShaderLayerBlend` from matching as `ShaderLayer`.

## String literals

A quoted value is opened by `"`, ends at the next unescaped `"`, and is unescaped on the way in.
A backslash inside the literal always consumes the following character, so `\"` never terminates it.

Anywhere a value may be quoted — settings values, metadata values, attribute values, argument values
— quoting is **optional**: the raw text is trimmed, and quotes are stripped only if the result is at
least two characters long and both starts and ends with `"`. `Domain = UI;` and `Domain = "UI";` are
therefore equivalent.

| Escape | Produces |
| :-- | :-- |
| `\n` | line feed |
| `\r` | carriage return |
| `\t` | tab |
| `\"` | `"` |
| `\\` | `\` |
| `\<any other character>` | that character; the backslash is dropped, with no diagnostic |
| a lone `\` at the end of the text | a literal backslash |

There are no numeric escapes: `\0`, `\xNN` and `\uNNNN` are not recognized and fall into the
"any other character" row (`\0` yields `0`, `\x41` yields `x41`).

The `Graph` expression grammar uses the same five escapes and the same pass-through rule for unknown
escapes; an unterminated string there is closed silently at end of input instead of erroring.

> [!NOTE]
> A raw line feed inside `"…"` is consumed like any other character — nothing rejects a string that
> spans lines. Whether the surrounding statement still parses depends on the consumer.

> [!NOTE]
> `Settings` and `Options` values are not trimmed again after the quotes are removed, so
> `Domain = " UI ";` stores `` UI `` with its spaces. Metadata values *are* trimmed after unquoting.
> When a value is written back out by the decompiler, exactly `\`, `"`, `\r`, `\n` and `\t` are
> re-escaped.

Diagnostic: an attribute value that reaches end of input while still inside `"` fails with
`Unterminated string literal.`

## Numeric literals

### Declaration grammar

The declaration grammar has **no numeric token**. Values are captured as raw text and converted only
when a consumer asks for a number.

| Consumer | Accepts |
| :-- | :-- |
| scalar default | any text the engine's double conversion accepts, plus `true` → `1.0` and `false` → `0.0` |
| integer argument | any text the engine's `int32` conversion accepts |
| boolean default | exactly `true` or `false` |
| vector default | `<anything>( <part> [, <part>]… )` — see below |

> [!WARNING]
> The underlying conversion is tolerant: it re-validates the text only when the parsed result is
> zero. Consequences, all silent:
>
> | Written | Parsed as | Diagnostic |
> | :-- | :-- | :-- |
> | `float Strength = 1.0f;` | `1.0` | none |
> | `float Strength = 1abc;` | `1.0` | none |
> | `float Strength = 0.0f;` | `0.0` | none |
> | `float Strength = abc;` | — | `Invalid scalar default value 'abc' for property 'Strength'.` |
>
> There is no hexadecimal, octal or binary literal form. `0x1F` is handed to the same tolerant
> conversion as any other text, with whatever result that conversion produces.

The vector-literal form is deliberately loose. The first `(` and the **last** `)` delimit the
components, and **the text before `(` is ignored entirely** — `float3(1,0,0)`, `vec3(1,0,0)`,
`(1,0,0)` and `Nonsense(1,0,0)` all parse identically. The interior is split on `,` without tracking
nesting, so a nested call in a component breaks the split. Each component may be a number or
`true` / `false`.

| Component count | Result |
| :-- | :-- |
| 1 | `(a, a, a, 1)` — splat to x, y, z |
| 2 | `(a, b, 0, 0)` |
| 3 | `(a, b, c, 1)` |
| 4 or more | the first four; extra components are parsed and discarded |

The unfilled default is `(0, 0, 0, 1)`.

### Graph expression grammar

Inside a `Graph` block a number is a real token.

| Element | Rule |
| :-- | :-- |
| start | a digit, **or** `.` immediately followed by a digit — so `.5` is legal |
| body | digits and `.`; several `.` are lexically accepted and fail later at conversion |
| exponent | at most one `e` or `E`, optionally followed by `+` or `-` |
| suffix | exactly one of `f` `F` `h` `H` `u` `U` `l` `L` |
| hexadecimal, octal, binary | not supported — `0x1F` lexes as the number `0` followed by the identifier `x1F` |

A suffix is consumed only when the character after it is not a letter, a digit or `_`, and it is
**excluded from the token text**: `0.55f` is the number `0.55`. Because only one suffix is consumed,
`1.0ul` lexes as the number `1.0` followed by the identifier `ul` — the `u` fails the boundary test
and is left in place.

The expression tokenizer accepts these single-character tokens: `(` `)` `,` `.` `+` `-` `*` `/` `=`,
plus the two-character `::`. **Every other character — including `:` alone, `{`, `}`, `[`, `]`, `<`,
`>`, `%`, `!`, `&` and `|` — ends the expression.** Where a value was still expected the parse then
fails with `Unexpected token '{Token}' in Graph expression.`; where the expression was already
complete the rest of the text is dropped without a diagnostic. See
[Unsupported constructs](../graph/unsupported.md).

## Statement separation

A section body is a list of `;`-separated statements.

- A `;` separates statements only at parenthesis depth 0 **and** bracket depth 0, and never inside a
  string literal.
- **Braces are not tracked** by the generic splitter. `Properties` uses its own brace-aware walker so
  that `Group("…") { … }` scopes work; every other section treats `{` as an ordinary character.
- Empty statements are dropped, so stray `;;` is harmless.
- The final statement is flushed even without a terminator — **the last `;` in a block is optional**.
- The `;` after a section's closing `}` is optional, and so is the `=` between a section name and its
  block *(since 1.5.0)*.

Three different splitters divide a statement into parts, and they do not agree on what counts as
whitespace:

| Split | Used by | Rule |
| :-- | :-- | :-- |
| first `=` at depth 0, outside strings | `Settings`, `Options`, metadata entries, `Outputs`, `Layout` arguments, `UE.*` arguments | `Color = (R=1,G=0,B=0);` splits on the outer `=` |
| last **whitespace** at depth 0 | `Properties` declarations | tabs are fine; `UE.TexCoord(Index = 0) UV` splits into type `UE.TexCoord(Index = 0)` and name `UV` |
| last **literal space** | `Inputs`, `Outputs`, `Results` typed parameters, and `Shader` output declarations | not depth-aware and not whitespace-aware |
| any run of whitespace | `Function` / `GraphFunction` parameter lists | each parameter must yield exactly 2 or 3 tokens |

> [!WARNING]
> `float3\tColor;` in `Inputs`, `Outputs`, `Results`, or a `Shader`'s `Outputs` fails with
> `Invalid typed declaration '{Statement}'.` because the splitter looks for a literal space
> character. The same declaration is accepted in `Properties`, which splits on any whitespace.
> Similarly, the optional `opt` prefix is recognized as `opt` followed by a **space**; `opt\tfloat X`
> does not mark the input optional.

Argument lists are split on `,` with the same depth and string tracking, which is why an unquoted
attribute value may not contain `,` or `)`: `Root=Game` works, `Name=Foo(1,2)` does not.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

| Message | Cause |
| :-- | :-- |
| `Expected identifier near index {Index}.` | an identifier was required and the next character is not a letter or `_` |
| `Expected '{Char}' near index {Index}.` | a required punctuation character is missing |
| `Expected '{' near index {Index}.` | a block was expected |
| `Unterminated block.` | end of input before the matching `}` |
| `Unterminated '{Char}' block.` | end of input before the matching delimiter, for example an unclosed parameter-list `(` |
| `Unterminated string literal.` | end of input inside a quoted attribute value |
| `Expected value near index {Index}.` | an attribute value is empty |
| `Expected ',' or ')' near index {Index}.` | malformed attribute list |
| `Unexpected token near index {Index}.` | no top-level keyword matched at this position |
| `Invalid typed declaration '{Statement}'.` | no literal space between type and name, an empty type, or a name that is not an identifier |
| `Invalid scalar default value '{Value}' for property '{Name}'.` | the text is not convertible to a number |
| `Invalid vector default value '{Value}' for property '{Name}'.` | a component is neither numeric nor `true` / `false` |
| `Invalid boolean default value '{Value}' for property '{Name}'.` | text other than `true` / `false` |
| `Unexpected token '{Token}' in Graph expression.` | a token where a value was expected, including a character outside the expression tokenizer's set |

Only messages ending in `near index {Index}` carry a position that the diagnostic mapper can turn
into a file, line and column. See [`import`](import.md#source-line-mapping).

## Example

```c
// Line comment before the top-level declaration.
Shader(Name="DreamShaderTests/Corpus/M_Comments")
{
    /* Block comment
       spanning multiple lines. */
    Settings = {
        Domain = "UI";        // trailing line comment
        ShadingModel = Unlit; // quotes are optional
    }

    Properties {
        // Any whitespace may separate a Properties type from its name.
        ScalarParameter Rough = 0.5 [Group="Surface"; Slider(0, 1)];
        vec3            Tint  = vec3(1.0, 0.4, 0.1);
        float           Fudge = 1.0f;   // parses as 1.0 — the suffix is ignored, not rejected
    }

    Outputs {
        vec3 Color;                     // a literal space is required here
        Base.EmissiveColor = Color
    }                                   // the last ';' in a block is optional

    Graph {
        vec2 UV = UE.TexCoord(Index = 0);
        Color = vec3(Rough, Rough, UV.x) * Tint;
    }
}
```

## See also

- [Keyword index](keywords.md) — every reserved word, with its case rule
- [Source files](source-files.md) — the file kinds these tokens live in
- [`import`](import.md) — the one line-oriented directive, and source-line mapping
- [Types](types.md) — the full type-token catalogue and the GLSL aliases
- [Properties](properties.md) — the section with the brace-aware statement walker
- [Inputs / Outputs / Results](inputs-outputs.md) — the sections with the literal-space rule
- [Layout](layout.md) — `#Region` / `#EndRegion`, the only directive-like syntax
- [Literals](../graph/literals.md) — literal forms inside a `Graph` block
- [Unsupported constructs](../graph/unsupported.md) — characters the expression tokenizer rejects
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
