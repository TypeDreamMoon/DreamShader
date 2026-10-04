# Lexical elements

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Lexical elements**

The character-level rules of DreamShaderLang: whitespace, comments, identifiers, case sensitivity,
string and numeric literals, and how a section body is split into statements.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` |
| Kind | lexical structure |
| Applies to | the whole file — blocks, sections and `Graph` bodies alike; `Function` / `GraphFunction` bodies are HLSL |

*(since 2.0.0)* One lexer reads every source, `.dss` and 1.x alike, into one token stream; the 1.x
grammar is read from those tokens by the legacy front end. Through 1.9.x the declaration grammar and
the `Graph` grammar each had their own character scanner, with rules of their own. Three options
apply to a `.dsm` / `.dsf`: a `///` line is an ordinary comment, a `'…'` pair on one line is a string
(the Content Browser's `Texture2D'…'` shell), and a `#` line at the start of a line is a directive
token.

## Synopsis

```c
<token> := <identifier> | <keyword> | <string-literal> | <number> | <punctuation>

<identifier>     := { <letter> | _ } { <letter> | <digit> | _ }…
<qualified-name> := <identifier> :: <identifier>
<string-literal> := " { <character> | \<escape> }… "
<punctuation>    := { | } | ( | ) | [ | ] | ; | = | , | . | : | # | the operators of a Graph expression
```

Whitespace and comments may appear between any two tokens and are otherwise insignificant.

## Whitespace

Space, tab, vertical tab, form feed, carriage return and line feed separate tokens *(since 2.0.0:
ASCII only; other Unicode whitespace, such as a no-break space, is
[`DSH2101`](../diagnostics/DSH2xxx.md#dsh2101))*. Newlines carry no syntactic weight; they matter only
to [preprocessor](preprocessor.md) directives, `#Region` directives inside a `Graph` body
([Layout](layout.md)), and diagnostic line numbers.

*(since 2.0.0)* Any whitespace separates any two words, in every section: a tab between a type and a
name is as good as a space.

## Comments

| Form | Rule |
| :-- | :-- |
| `// …` | line comment; runs to, but does not include, the next line break. `///` is an ordinary line comment in a 1.x file |
| `/* … */` | block comment; ends at the first `*/` |

- **Block comments do not nest.** `/* a /* b */ c */` ends at the first `*/`; the trailing `c */` is
  code again.
- **An unterminated block comment is an error** *(since 2.0.0)*:
  [`DSH2102`](../diagnostics/DSH2xxx.md#dsh2102), reported at the opening `/*`. 1.x accepted it and
  dropped the rest of the file.
- Comments are recognized identically everywhere, including inside `Graph` bodies and while braces,
  parentheses and brackets are counted, so a brace or quote inside a comment never unbalances a
  block.
- **A comment hides an `import`** *(since 2.0.0)*: `import` is a token, so one inside `/* … */` is
  commented out. See [`import`](import.md#recognition). **It does not hide a preprocessor
  directive**: the `#if` family is found by a line scan that runs before any comment is understood,
  so a `#if` inside `/* … */` is still a directive. A line whose first non-whitespace characters are
  `//` never is. See [Preprocessor](preprocessor.md#recognition).

## Identifiers

| Position | Accepted characters |
| :-- | :-- |
| first | an ASCII letter or `_` |
| subsequent | an ASCII letter, a digit or `_` |

- There is no length limit. *(since 2.0.0)* Only ASCII letters count; a letter outside ASCII is
  `DSH2101`.
- **Block words, section names and type names are not reserved.** A property, variable, parameter or
  function may be named `Shader`, `Graph` or `float`; whether it then resolves is a matter for the
  context it appears in. The lower-case [reserved words](keywords.md#reserved-words) — `in`, `out`,
  `if`, `return`, `const`, … — are reserved *(since 2.0.0)*.
- A declaration name is one identifier token in every section *(since 2.0.0)*; 1.x checked only that a
  `Properties` name was non-empty, so `Properties { float 1Bad = 0; }` parsed. Now `1Bad` is a
  malformed number ([`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105)) and the declaration is
  [`DSH3250`](../diagnostics/DSH3xxx.md#dsh3250).
- Namespace-qualified names use `::`, as in `Common::ApplyTint`. See [Namespace](namespace.md).

When a name reaches generated HLSL it is sanitized: every character outside `A–Z a–z 0–9 _` becomes
`_`, a leading digit gains a `_` prefix, runs of consecutive `__` collapse to one, and a result that
is empty or entirely underscores becomes `DreamShaderSymbol`. This is why `Common::ApplyTint` and
`Common_ApplyTint` collide in generated code.

## Case sensitivity

**Top-level block keywords and the lower-case reserved words are case-sensitive.** Everything else
keyword-like in the 1.x grammar is matched ignoring case, with the exceptions marked below. Names the
engine defines — material attributes, node classes, pins — are matched exactly and, in a 1.x file,
also ignoring case with the warning [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276); names the file
declares, with [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275) *(since 2.0.0, legacy rule L19)*. The
[preprocessor](preprocessor.md), which runs before the grammar and is not part of it, is
case-sensitive throughout.

| Construct | Case-sensitive | Reference |
| :-- | :-- | :-- |
| Top-level block keywords — `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `MaterialLayer`, `MaterialLayerBlend`, `VirtualFunction`, `Namespace`, `Function`, `GraphFunction` | **yes** | [Keyword index](keywords.md) |
| Reserved words — `in`, `out`, `const`, `return`, `true`, `import`, … | **yes**, as reserved words; `In`, `Const`, `True` are ordinary words that the 1.x grammar still reads where it expects the keyword | [Keyword index](keywords.md#reserved-words) |
| Section names — `Properties`, `Settings`, `Outputs`, `Inputs`, `Results`, `Options`, `Graph`, `Code`, `Layout` | no | [Keyword index](keywords.md) |
| Header attribute keys — `Name=`, `Root=`, `Asset=` | no | [Shader](shader.md) |
| `Settings` / `Options` keys, metadata keys, `Expression( … )` argument keys, `Layout` argument keys | no | [Settings](../settings/index.md) |
| `UE.*` argument names | no, with `DSH5276` | [`UE.*` catalogue](../builtins/ue.md) |
| Type tokens — `float3`, `vec3`, `Texture2D`, `ScalarParameter`, … | no | [Types](types.md) |
| `const` prefix in `Properties` | no | [Properties](properties.md) |
| `opt` prefix in `Inputs` | no | [Inputs / Outputs](inputs-outputs.md) |
| `SelfContained` / `Inline` after `Function` | no | [Function](function.md) |
| `in` / `out` parameter qualifiers | no | [Function](function.md) |
| `true` / `false` | no | [Types](types.md) |
| `Path(` texture-reference keyword | no | [`Path(...)`](../parameters/path.md) |
| `Base.` binding prefix and `Expression(` | no | [Output bindings](output-bindings.md) |
| `Base.` attribute names | no, with `DSH5276` | [Output bindings](output-bindings.md#basetarget-catalogue) |
| `.Pin[` pin selector | no | [Output bindings](output-bindings.md) |
| `Group("…")` property-scope head | no | [Properties](properties.md) |
| `Slider(` metadata shorthand | no | [Metadata block](../parameters/metadata.md) |
| `UE.` builtin prefix | no in `Properties` and in a `GraphFunction` body; **yes** in a `Graph` call *(since 2.0.0)* | [`UE.*` catalogue](../builtins/ue.md) |
| `#Region` / `#EndRegion` | no | [Layout](layout.md) |
| `Node(` / `Comment(` layout calls | no | [Layout](layout.md) |
| `import` | no, with the warning [`DSH2253`](../diagnostics/DSH2xxx.md#dsh2253) for another case | [`import`](import.md) |
| `#if` / `#ifdef` / `#ifndef` / `#elif` / `#else` / `#endif` / `#define` / `#undef` | **yes — lowercase only**; `#IF` is `DSH1035`, not a silent no-op | [Preprocessor](preprocessor.md#what-is-not-a-directive) |
| define **names**, and the reserved `DS_` prefix test | **yes** — `Foo` and `FOO` are two defines, and `ds_foo` is not reserved | [Preprocessor](preprocessor.md#where-defines-come-from) |
| string comparison inside a `#if` | **yes** — `DS_PLATFORM == "windows"` is false on Windows | [Preprocessor](preprocessor.md#values) |
| `default` call-argument sentinel | **yes** — lower case only *(since 2.0.0)* | [Calls](../graph/calls.md) |
| `.dsm` / `.dsf` / `.dsh` extensions | no | [Source files](source-files.md) |

So `shader(Name="X")` and `SHADER(Name="X")` are both syntax errors
([`DSH2240`](../diagnostics/DSH2xxx.md#dsh2240)), while `properties = { … }`,
`settings { domain = "ui"; }` and `Shader(name="X")` are all accepted.

A keyword is a whole word: `ShaderFunction` is its own identifier, never `Shader` followed by
something, and `ShaderLayerBlend` is never `ShaderLayer`.

## String literals

A string is opened by `"`, ends at the next unescaped `"` on the same line, and is unescaped by the
lexer.

| Escape | Produces |
| :-- | :-- |
| `\n` | line feed |
| `\r` | carriage return |
| `\t` | tab |
| `\"` | `"` |
| `\\` | `\` |
| `\0` | nothing — a string cannot hold a NUL |
| `\<any other character>` | [`DSH2104`](../diagnostics/DSH2xxx.md#dsh2104) *(since 2.0.0; 1.x dropped the backslash)* |

- *(since 2.0.0)* A string never spans lines: a line break before the closing `"` ends it, and an
  unclosed string is [`DSH2103`](../diagnostics/DSH2xxx.md#dsh2103), in a section and in a `Graph`
  body alike. 1.x let a string run across lines and closed one in a `Graph` body silently at the end
  of the input.
- In a 1.x file and a `.dsh`, a pair of `'` on one line is a string too, with no escapes — the shell
  of a Content Browser reference, `Texture2D'/Game/T.T'`. A lone `'` is `DSH2101`.

Anywhere a value may be quoted — settings values, metadata values, attribute values, argument values
— quoting is **optional**: `Domain = UI;` and `Domain = "UI";` are equivalent. A quoted value is the
string's text; an unquoted one is the text of its tokens.

> [!NOTE]
> A quoted `Settings` or `Options` value is not trimmed: `Domain = " UI ";` stores `` UI `` with its
> spaces.

## Numeric literals

### Declaration grammar

*(since 2.0.0)* A number is a token everywhere, lexed by the rules [below](#graph-expression-grammar);
a section then reads the token's text the way its consumer asks:

| Consumer | Accepts |
| :-- | :-- |
| scalar default | an optional sign, digits, at most one `.`, an optional trailing `f` / `F`; or `true` / `false` |
| integer argument (`SortPriority`, `Layout` coordinates) | an optional sign and digits |
| boolean default | exactly `true` or `false`, in any case |
| vector default | `<anything>( <part> [, <part>]… )` — see below |

What is not accepted is [`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254) for a default,
[`DSH3258`](../diagnostics/DSH3xxx.md#dsh3258) for a `SortPriority` and
[`DSH3275`](../diagnostics/DSH3xxx.md#dsh3275) for a `Layout` coordinate.

| Written | Read as | Diagnostic |
| :-- | :-- | :-- |
| `float Strength = 1.0f;` | `1.0` | none |
| `float Strength = 0.0f;` | `0.0` | none |
| `float Strength = 1abc;` | — | `DSH2105` and `DSH3254` *(since 2.0.0; 1.x read `1.0`)* |
| `float Strength = abc;` | — | `DSH3254` |
| `float Strength = 0x1F;` | — | `DSH3254` |
| `float Strength = 1e3;` | — | `DSH3254` — a scalar default takes no exponent |

The vector-literal form is deliberately loose, as in 1.x. The first `(` and the **last** `)` delimit
the components, and **the text before `(` is ignored entirely** — `float3(1,0,0)`, `vec3(1,0,0)`,
`(1,0,0)` and `Nonsense(1,0,0)` all read identically. The interior is split on `,` without tracking
nesting, so a nested call in a component breaks the split. Each component may be a number (with an
exponent and a trailing `f`) or `true` / `false`.

| Component count | Result |
| :-- | :-- |
| 1 | `(a, a, a, 1)` — splat to x, y, z |
| 2 | `(a, b, 0, 0)` |
| 3 | `(a, b, c, 1)` |
| 4 or more | the first four; components past the fourth are ignored |

The unfilled default is `(0, 0, 0, 1)`.

### Graph expression grammar

| Element | Rule |
| :-- | :-- |
| start | a digit, **or** `.` immediately followed by a digit — so `.5` is legal |
| body | digits, then at most one `.` and more digits |
| exponent | `e` or `E`, optionally followed by `+` or `-`, then digits |
| suffix | `f` `F` `h` `H` make a float, `u` `U` `l` `L` an integer |
| hexadecimal | `0x1F` is one hexadecimal integer token; in a `Graph` expression it is [`DSH2222`](../diagnostics/DSH2xxx.md#dsh2222) *(since 2.0.0; 1.x read `0` followed by the name `x1F`)* |
| anything glued on | `1.2.3`, `1abc`, `1e`, `1.0u`, `1fu` are one malformed token: `DSH2105` |

The suffix is part of the token text, so `0.55f` is the number `0.55`.

*(since 2.0.0)* Every operator of the 2.0 grammar is a token: `%`, `<`, `!`, `&&`, `?`, `[`, `]` no
longer end a `Graph` expression and drop the rest of it. What a 1.x `Graph` did not have is refused
by name instead — [`DSH2200`](../diagnostics/DSH2xxx.md#dsh2200) to `DSH2222`. See
[Unsupported constructs](../graph/unsupported.md).

## Statement separation

A section body is a list of `;`-separated statements, read token by token *(since 2.0.0)*.

- A `;` separates statements only outside `()`, `[]` and `{}`. A string is one token, so a `;` inside
  one is just text.
- Empty statements are skipped, so stray `;;` is harmless.
- The `;` of the last statement before the section's closing `}` is optional.
- The `;` after a section's closing `}` is optional, and so is the `=` between a section name and its
  block *(since 1.5.0)*.
- A value — a default, a setting, a binding source, an argument — runs to the `;`, `,` or `)` that
  ends it outside brackets, so an unquoted attribute value may hold parentheses and commas:
  `Asset = Path(Game, "MaterialFunctions/F_X")` is one value.

*(since 2.0.0)* 1.x split statements with three different splitters, two of which looked for a
literal space, so `float3\tColor;` failed in `Inputs`, `Outputs`, `Results` and a `Shader`'s `Outputs`,
and `opt\tfloat X` did not mark an input optional. Both work now.

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH2101` | a character that begins no token: outside ASCII, `$`, `@`, a lone `'`, … |
| `DSH2102` | a `/*` with no `*/` |
| `DSH2103` | a string with no closing `"` on its line |
| `DSH2104` | an unknown escape in a string |
| `DSH2105` | a malformed number |
| [`DSH2106`](../diagnostics/DSH2xxx.md#dsh2106) | a `#` that is not the first thing on its line |
| `DSH2240` | text at top level that is no block keyword |
| [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) | the file ends inside a block, a section or a list |
| `DSH3254`, `DSH3258`, `DSH3275` | a default, a sort priority or a layout coordinate that is not a number of the kind asked for |

Every diagnostic carries the file, line and column of the construct it is about, and is printed
`<file>(<line>,<column>): DSHnnnn: <message>` *(since 2.0.0)*; only a few that concern a whole file,
such as an include that cannot be resolved, report line 1. 1.x located only the messages that ended in
`near index {Index}`. See [`import`](import.md#source-line-mapping).

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
        // Any whitespace may separate a type from its name.
        ScalarParameter Rough = 0.5 [Group="Surface"; Slider(0, 1)];
        vec3            Tint  = vec3(1.0, 0.4, 0.1);
        float           Fudge = 1.0f;   // reads as 1.0
    }

    Outputs {
        vec3	Color;                  // a tab is whitespace too
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
- [`import`](import.md) — a token of the language since 2.0.0, and where positions come from
- [Preprocessor](preprocessor.md) — the line-oriented directives, and the one case-sensitive keyword set
- [Types](types.md) — the full type-token catalogue and the GLSL aliases
- [Properties](properties.md) — the section with `Group("…") { … }` scopes
- [Inputs / Outputs / Results](inputs-outputs.md) — the typed-parameter sections
- [Layout](layout.md) — `#Region` / `#EndRegion`, the only directive-like syntax
- [Literals](../graph/literals.md) — literal forms inside a `Graph` block
- [Unsupported constructs](../graph/unsupported.md) — what a 1.x `Graph` expression refuses
- [Diagnostics index](../diagnostics/index.md) — every code
