# DSH2xxx --- Lexer and syntax

> The block between the generated markers is written by `.skill/gen-diagnostics.ps1`.
> Everything below a marker is written by hand and survives a regeneration.

## DSH2001

<!-- generated:begin DSH2001 -->
**Severity** error

**Message**

```
Expected '{0}' near index {1}.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:23`, `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:82`
<!-- generated:end DSH2001 -->

**Cause.** a delimited region (`(` for a parameter list, `{` for a body) did not open where required

**Fix.** add the delimiter

**See** [Function](../language/function.md)

## DSH2002

<!-- generated:begin DSH2002 -->
**Severity** error

**Message**

```
Expected identifier near index {0}.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:113`
<!-- generated:end DSH2002 -->

**Cause.** an identifier was expected — attribute key, section name, block name

**Fix.** identifiers are `[A-Za-z_][A-Za-z0-9_]*`

**See** [Lexical elements](../language/lexical.md)

## DSH2003

<!-- generated:begin DSH2003 -->
**Severity** error

**Message**

```
Unterminated string literal.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:147`
<!-- generated:end DSH2003 -->

**Cause.** EOF reached inside a quoted attribute value

**Fix.** close the `"`

**See** [Lexical elements](../language/lexical.md)

## DSH2004

<!-- generated:begin DSH2004 -->
**Severity** error

**Message**

```
Expected value near index {0}.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:170`
<!-- generated:end DSH2004 -->

**Cause.** an attribute key was followed by `=` and then nothing

**Fix.** supply a value; an unquoted value ends at the first `,` or `)`

**See** [Shader](../language/shader.md)

## DSH2005

<!-- generated:begin DSH2005 -->
**Severity** error

**Message**

```
Expected ',' or ')' near index {0}.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:223`
<!-- generated:end DSH2005 -->

**Cause.** malformed header attribute list

**Fix.** separate attributes with `,`; a trailing `,` before `)` is allowed

**See** [Shader](../language/shader.md)

## DSH2006

<!-- generated:begin DSH2006 -->
**Severity** error

**Message**

```
Expected '{{' near index {0}.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:240`
<!-- generated:end DSH2006 -->

**Cause.** a block body was expected

**Fix.** add the `{ … }` body

**See** [Source files](../language/source-files.md)

## DSH2007

<!-- generated:begin DSH2007 -->
**Severity** error

**Message**

```
Unterminated block.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:325`
<!-- generated:end DSH2007 -->

**Cause.** EOF reached before a `}` closed

**Fix.** balance the braces

**See** [Lexical elements](../language/lexical.md)

## DSH2008

<!-- generated:begin DSH2008 -->
**Severity** error

**Message**

```
Unterminated '{0}' block.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:108`
<!-- generated:end DSH2008 -->

**Cause.** EOF reached before the matching delimiter of a generic delimited block, e.g. an unclosed `(` parameter list

**Fix.** balance the delimiters

**See** [Function](../language/function.md)

## DSH2009

<!-- generated:begin DSH2009 -->
**Severity** error

**Message**

```
Unexpected token near index {0}.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:986`
<!-- generated:end DSH2009 -->

**Cause.** no top-level keyword matched at this position; an `import` line handed straight to the parser also lands here

**Fix.** check keyword spelling and case — top-level keywords are the only case-**sensitive** tokens in the language

**See** [Keywords](../language/keywords.md)

## DSH2010

<!-- generated:begin DSH2010 -->
**Severity** error

**Message**

```
A top-level Shader, Function, GraphFunction, Namespace, ShaderFunction, ShaderLayer, ShaderLayerBlend, or VirtualFunction block was not found.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:1032`
<!-- generated:end DSH2010 -->

**Cause.** the parse unit declared no recognized top-level block; an empty `Namespace` body also lands here

**Fix.** add a top-level block, or check that the keyword's case is exact

**See** [Keywords](../language/keywords.md)

## DSH2101

<!-- generated:begin DSH2101 -->
**Severity** error

**Message**

```
Unexpected character '{0}' in source.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:717`
<!-- generated:end DSH2101 -->

**Cause.** a character that is not part of any DreamShaderLang token: a stray `@`, `$`, a backtick, a
full-width punctuation mark pasted in from a document, or a non-breaking space that a browser copy
left behind. The lexer is deliberately ASCII-only — a Cyrillic `а` or a full-width `Ａ` is *not* an
identifier character, because a symbol built from one would look right in the editor and match nothing
downstream.

**Fix.** delete the character. When the message shows a character that looks ordinary, it is almost
always the invisible kind: retype the line rather than editing it, or turn on "render whitespace" in
the editor. `@` inside a `///` block is a directive and is fine — this error only fires on code.

**Note.** the character is still emitted as an `Unknown` token, so the parser reports what it expected
at that spot as well; both messages describe the same typo.

**See** [DreamShaderLang 2.0](../language-v2/index.md)

## DSH2102

<!-- generated:begin DSH2102 -->
**Severity** error

**Message**

```
Unterminated block comment; expected a closing '*/'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:329`
<!-- generated:end DSH2102 -->

**Cause.** a `/*` with no `*/` after it. Block comments do not nest: the first `*/` closes the
*outermost* comment, so a commented-out region that itself contains `/* ... */` ends early and the
tail of the region becomes code — and, more often, a later `/*` then runs to the end of the file and
lands here.

**Fix.** close the comment. The position reported is the **opening** `/*`, not the end of the file:
that is the place to look, because the end of the file says nothing about which comment was left open.
To comment out a region that already contains block comments, use `///`-free `//` line comments or
`#if 0` … `#endif`, which the preprocessor handles.

**See** [DreamShaderLang 2.0](../language-v2/index.md)

## DSH2103

<!-- generated:begin DSH2103 -->
**Severity** error

**Message**

```
Unterminated string literal; expected a closing '"'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:468`
<!-- generated:end DSH2103 -->

**Cause.** a `"` with no closing `"` before the end of its line. A DreamShaderLang string never spans
lines: that is a deliberate limit, and it is what keeps one missing quote from swallowing the rest of
the file. Two common shapes reach here — a genuinely forgotten quote, and a path that ends in a
backslash (`"C:\Assets\"`), where the `\"` is read as an escaped quote and the literal runs on.

**Fix.** close the string. In a path, use forward slashes (`"/Game/Materials/M_A"`) or double the
backslash (`"C:\\Assets\\"`); DreamShaderLang asset paths are `/`-separated everywhere.

**Note.** the text up to the end of the line is still emitted as one string token, so the statement
around it is still parsed and any further mistakes in the file are still reported.

**See** [DreamShaderLang 2.0](../language-v2/index.md)

## DSH2104

<!-- generated:begin DSH2104 -->
**Severity** error

**Message**

```
Unknown escape sequence '\{0}' in a string literal.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:448`
<!-- generated:end DSH2104 -->

**Cause.** a backslash inside a string followed by something other than the six escapes the language
knows: `\\`, `\"`, `\n`, `\r`, `\t`, `\0`. Nearly always a Windows path written with single
backslashes — `"D:\new\textures"` contains `\n` and `\t` (which silently become a newline and a tab)
and `\x`, which lands here.

**Fix.** double every backslash, or — better — write the path with forward slashes. There are no
`\x41`, `\u0041` or octal escapes: a string that needs a character outside the six escapes should
carry that character literally (the source is read as UTF-8).

**Note.** the backslash is **kept** in the resolved value, so `"\q"` yields the two characters `\q`.
Nothing is silently lost, and a source that round-trips through the printer comes back unchanged.

**See** [DreamShaderLang 2.0](../language-v2/index.md)

## DSH2105

<!-- generated:begin DSH2105 -->
**Severity** error

**Message**

```
Malformed number literal '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:635`
<!-- generated:end DSH2105 -->

**Cause.** something that starts like a number but is not one. The shapes that reach here:

| Written | Why |
| :-- | :-- |
| `1.2.3` | two decimal points |
| `0x` | a hex prefix with no digits |
| `1e`, `1e+` | an exponent with no digits |
| `1fu`, `1uf` | a float suffix and an integer suffix on the same literal |
| `1.0u`, `1e3l` | an integer suffix on a value that has a fraction or an exponent |
| `0x1h`, `0x2f` … `h`/`f` after hex | a float suffix on a hexadecimal literal — hex is always an integer |
| `2abc`, `1px` | letters glued to the end of a number |

The last row is the one that surprises people: `2abc` is **one** malformed number token, not `2`
followed by the identifier `abc`. Splitting it would hand the parser a plausible-looking expression
and hide the real mistake.

**Fix.** write the literal the way the language spells it: decimal integers with an optional `u`/`U`
(unsigned) or `l`/`L` (long) suffix, hex as `0x…` (integer only), floats with a `.`, an exponent, or an
`f`/`F`/`h`/`H` suffix. `1.0f`, `0x10`, `2u`, `.5`, `1e-3` and `2.0h` are all valid. When the intent
was a member access or a swizzle, put the number in front of the dot: `x.5` is a member access,
`2.5` is a float.

**Note.** a malformed literal still produces exactly one token with its value parsed as far as it can
be (`1e` reads as 1), so the surrounding statement is still parsed and the rest of the file still
reports its own errors.

**See** [DreamShaderLang 2.0](../language-v2/index.md)

## DSH2106

<!-- generated:begin DSH2106 -->
**Severity** error

**Message**

```
A '#' directive must be the first thing on its line.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:351`
<!-- generated:end DSH2106 -->

**Cause.** a `#` that is not the first non-whitespace character on its physical line — `int a = 1;
#pragma x`, or a `#` used as an operator (DreamShaderLang has no `#`, `##` or stringize operator).
Leading spaces and tabs are fine; anything else on the line is not, and that includes a comment:
`/* c */ #pragma x` is **not** a directive line, because the comment is something the lexer has
already seen.

**Fix.** put the directive on a line of its own. If the `#` was meant as an operator, it is not one —
use the DreamShaderLang spelling of whatever was intended (`#pragma material(...)` for material
settings, `import "..."` or `#include "..."` for includes).

**Note.** the `#` is emitted as an `Unknown` token and lexing continues on the same line, so the words
after it are still lexed normally and any second mistake on that line is still reported.

**See** [DreamShaderLang 2.0](../language-v2/index.md)

## DSH2150

<!-- generated:begin DSH2150 -->
**Severity** error

**Message**

```
Unexpected end of file while parsing {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParser.cpp:246`
<!-- generated:end DSH2150 -->

**Cause.** the file ended in the middle of something that was not finished: an expression, an
argument list, an initializer list, a statement or a block. Almost always an unbalanced `{`, `(` or
`[` earlier in the file — the parser kept looking for the closer and ran out of text.

**Fix.** close the construct the message names. When the message says "a block", look for the
function whose `}` is missing; the reported position is the end of the file, not the mistake.

## DSH2151

<!-- generated:begin DSH2151 -->
**Severity** error

**Message**

```
Expected an expression, found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:490`, `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:561`
<!-- generated:end DSH2151 -->

**Cause.** a token that cannot begin a value turned up where a value was required: a stray `,` or
`;`, a keyword that starts a statement (`if`, `return`), a closing bracket, or the leftovers of a
line that lost its `;` earlier.

**Fix.** write the missing operand, or delete the stray token. When the token named is a keyword,
the previous statement is usually the one missing its `;`.

## DSH2152

<!-- generated:begin DSH2152 -->
**Severity** error

**Message**

```
Expected ')' to close a cast, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:253`, `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:468`, `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:550`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:114`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:214`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:249`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:296`
<!-- generated:end DSH2152 -->

**Cause.** an unbalanced `(`. The parser read a complete expression and then found something other
than the `)` it was owed.

**Fix.** add the `)`. When the message names an argument list, check for a missing `,` between two
arguments — `f(a b)` reports this code at `b`.

## DSH2153

<!-- generated:begin DSH2153 -->
**Severity** error

**Message**

```
Expected ']' to close an index, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:325`
<!-- generated:end DSH2153 -->

**Cause.** an index expression `a[…` whose `]` is missing, or an index that swallowed the `]` by
starting an expression the author did not intend (`a[i, j]` — the comma operator is not part of
DreamShaderLang, so the `,` ends the index).

**Fix.** add the `]`. A multi-dimensional index is written `a[i][j]`, never `a[i, j]`.

## DSH2154

<!-- generated:begin DSH2154 -->
**Severity** error

**Message**

```
Expected ';' after the 'for' initializer, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:178`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:199`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:301`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:327`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:342`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:356`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:370`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:419`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:554`
<!-- generated:end DSH2154 -->

**Cause.** a statement that does not end with `;`. Unlike 1.x, the 2.0 grammar has no
newline-terminated statement form: every statement ends with `;` or a `}`.

**Fix.** add the `;`. The reported position is the FIRST token of the next line, so the mistake is
on the line above the one the message points at.

**Recovery.** the statement that lost its `;` is dropped together with the tokens up to the next
`;`, and the block keeps parsing — one missing semicolon costs one statement, not the rest of the
function.

## DSH2155

<!-- generated:begin DSH2155 -->
**Severity** error

**Message**

```
Expected '}' to close an initializer list, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:602`
<!-- generated:end DSH2155 -->

**Cause.** a `{ … }` initializer whose `}` is missing, or an element that is not an expression
(a `;` inside the braces, for instance).

**Fix.** close the list. Elements are separated by `,` and a trailing `,` before the `}` is allowed;
nested lists are written `{ { 1, 2 }, { 3, 4 } }`.

## DSH2157

<!-- generated:begin DSH2157 -->
**Severity** error

**Message**

```
Expected '(' after 'if', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:103`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:148`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:238`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:285`
<!-- generated:end DSH2157 -->

**Cause.** a control-flow keyword whose parenthesised header is missing. In DreamShaderLang, as in
HLSL, the condition of `if`, `for`, `while` and `do … while` is always parenthesised.

**Fix.** write `if (cond)`, `for (init; cond; step)`, `while (cond)`, `do … while (cond);`.

## DSH2158

<!-- generated:begin DSH2158 -->
**Severity** error

**Message**

```
A positional argument cannot follow a named argument; give this argument a name too.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:533`
<!-- generated:end DSH2158 -->

**Cause.** a call that mixes the two argument forms in the wrong order — `Fn(A = 1, x)`. A named
argument binds a pin by name, so once one argument is named the position of the following ones
carries no information the reader can trust.

**Fix.** name the remaining arguments (`Fn(A = 1, B = x)`), or move the named ones to the end
(`Fn(x, A = 1)`, which is legal). An argument that is genuinely an assignment is written in
parentheses: `Fn((a = b), c)`.

## DSH2159

<!-- generated:begin DSH2159 -->
**Severity** error

**Message**

```
Expected ':' to complete the conditional operator, found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:153`
<!-- generated:end DSH2159 -->

**Cause.** a `?` without its `:`. Usually a `:` typed as something else, or a conditional whose
middle operand ran past the `:` because a bracket inside it is unbalanced.

**Fix.** write `cond ? a : b`. The operator is right-associative, so a chain needs no parentheses:
`a ? b : c ? d : e` means `a ? b : (c ? d : e)`.

## DSH2160

<!-- generated:begin DSH2160 -->
**Severity** error

**Message**

```
Unsupported statement: the preprocessor line '#{0}' cannot appear inside a function body; mark the function /// @custom to hand its body to the shader compiler.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:397`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:89`
<!-- generated:end DSH2160 -->

**Message (statement)** `Unsupported statement '{0}': DreamShaderLang 2.0 has no switch statement,
write if / else if instead.`

**Message (directive)** `Unsupported statement: the preprocessor line '#{0}' cannot appear inside a
function body; mark the function /// @custom to hand its body to the shader compiler.`

**Cause.** `switch`, `case` or `default` at statement level, or a `#` line inside a parsed function
body. Neither is part of the language: 2.0 lowers a function body to a material graph, and a graph
has no jump table; a `#` line should have been consumed by the preprocessor before the parser saw
it.

**Fix.** rewrite a `switch` as an `if` / `else if` chain. For a `#` line, either move it to file
scope (`#pragma`, `#include` are declarations, not statements) or, when the body really is HLSL,
mark the function `/// @custom` — its body is then captured verbatim and handed to the shader
compiler untouched.

## DSH2161

<!-- generated:begin DSH2161 -->
**Severity** error

**Message**

```
Expected a member or swizzle name after '.', found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:295`
<!-- generated:end DSH2161 -->

**Cause.** a `.` that is not followed by an identifier: a number (`v.1`), a keyword, or a `.` that
was meant to be part of a float literal (`1 .5`).

**Fix.** write the member or swizzle name. Swizzles are ordinary member accesses to the parser, so
`v.xy`, `v.rgb` and `Struct.Field` all take the same form.

## DSH2162

<!-- generated:begin DSH2162 -->
**Severity** error

**Message**

```
An initializer list is only allowed as a variable initializer, not as a general expression.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:482`
<!-- generated:end DSH2162 -->

**Cause.** a `{ … }` used as a value — as a call argument, in a `return`, or on the right of an
operator. In HLSL, as in C, a brace list is a syntactic form of an initializer, not an expression
with a type.

**Fix.** declare a variable and pass that, or use a constructor: `float3(1, 2, 3)` instead of
`{ 1, 2, 3 }`.

## DSH2163

<!-- generated:begin DSH2163 -->
**Severity** error

**Message**

```
Expected a variable name, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:526`
<!-- generated:end DSH2163 -->

**Cause.** a declaration whose declarator has no name: a doubled `,` (`float a,, b;`), a `,` before
the `=` (`float a, = 1;`), or a type spelling that was actually meant to be a variable and shadowed
the name.

**Fix.** write the name. Every declarator of a declaration is `Name`, `Name[dims]`, `Name = init`
or `Name[dims] = init`.

## DSH2164

<!-- generated:begin DSH2164 -->
**Severity** error

**Message**

```
Expected 'while' after the body of a 'do' statement, found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:279`
<!-- generated:end DSH2164 -->

**Cause.** a `do` whose `while` is missing or misspelled.

**Fix.** write `do { … } while (cond);` — the trailing `;` is part of the statement and its absence
is DSH2154, not this code.

## DSH2165

<!-- generated:begin DSH2165 -->
**Severity** error

**Message**

```
Expected '{' to open a block, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:434`
<!-- generated:end DSH2165 -->

**Cause.** a block was required and something else was found. In M1 this is reachable only when a
caller asks for a block without checking first — a function body whose `{` is missing reports it.

**Fix.** open the block with `{`. An `extern` function has no body at all and ends with `;`; a
`/// @custom` function still needs its braces, the parser simply does not read inside them.

## DSH2199

<!-- generated:begin DSH2199 -->
**Severity** error

**Message**

```
The 1.x front end is not available in this build.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParser.cpp:560`
<!-- generated:end DSH2199 -->

**Cause.** a `.dsm` or `.dsf` file was handed to the 2.0 front end, which does not read the 1.x block syntax. The 1.x syntax becomes a second front end producing the same AST, but that front end is not in this build yet, so the honest answer is one error rather than a 2.0 parse of 1.x text failing token by token and explaining nothing.

**Fix.** nothing to fix in the source. Until the second front end lands, `.dsm` and `.dsf` files are compiled by the 1.x pipeline, which is still the default one; only `.dss` and `.dsh` reach this front end. If you meant to write 2.0 syntax, give the file a `.dss` extension.

**Note.** the front end is chosen by extension: `.dss` and `.dsh` (and an unknown or absent extension) take the 2.0 front end, `.dsm` and `.dsf` ask for the 1.x one. A caller can override that with `FLangParseOptions::Frontend`, and asking for `ELangFrontend::Legacy` explicitly reports this same code. A module is still returned, holding no declarations, so a language service can keep working on the file.

**See** [DreamShaderLang 2.0](../language-v2/index.md)

