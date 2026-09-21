# DSH2xxx --- Lexer and syntax

> The block between the generated markers is written by `.skill/gen-diagnostics.ps1`.
> Everything below a marker is written by hand and survives a regeneration.

## DSH2101

<!-- generated:begin DSH2101 -->
**Severity** error

**Message**

```
Unexpected character '{0}' in source.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:780`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:353`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:507`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:487`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:698`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:382`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:543`, `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:630`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:253`, `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:521`, `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:619`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:159`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:259`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:294`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:341`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:223`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:244`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:346`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:372`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:387`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:401`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:415`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:464`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:600`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:671`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:148`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:193`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:283`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:330`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:602`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:134`, `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:442`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:535`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:572`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:324`
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

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:479`
<!-- generated:end DSH2165 -->

**Cause.** a block was required and something else was found. In M1 this is reachable only when a
caller asks for a block without checking first — a function body whose `{` is missing reports it.

**Fix.** open the block with `{`. An `extern` function has no body at all and ends with `;`; a
`/// @custom` function still needs its braces, the parser simply does not read inside them.

## DSH2200

<!-- generated:begin DSH2200 -->
**Severity** error

**Message**

```
(built at runtime)
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:550`
<!-- generated:end DSH2200 -->

**Cause.** A 1.x Graph expression used a comparison, a logical operator or `?:` outside an `if`
condition. The 1.x expression reader knew `+ - * /`, calls and members; at anything else it stopped
and used what it had read so far, so `a < b ? x : y` silently became `a`. The legacy front end
refuses the text instead of reproducing that.

**Fix.** Put the choice in an `if` / `else` (1.x compares there), or use a node such as
`UE.If(...)`; or move the code to a `.dss` file, where these operators exist.

## DSH2201

<!-- generated:begin DSH2201 -->
**Severity** error

**Message**

```
(built at runtime)
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:546`
<!-- generated:end DSH2201 -->

**Cause.** A 1.x Graph expression used `%`, a shift or a bitwise operator. 1.x had none of them: it
stopped reading at the operator and silently dropped the rest of the expression.

**Fix.** Use `fmod(a, b)` for a remainder; for anything bitwise, move the code into a `Function`
(HLSL) or to a `.dss` file.

## DSH2202

<!-- generated:begin DSH2202 -->
**Severity** error

**Message**

```
Expected no '[ ]' in a 1.x Graph expression, found '{0}', where 1.x silently dropped the index and everything after it; use a swizzle such as '.r' or move this code to a .dss file.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:392`
<!-- generated:end DSH2202 -->

**Cause.** A 1.x Graph expression indexed a value with `[ ]`. 1.x dropped the index and everything
after it without a word, so the graph never did what the text says.

**Fix.** Take a component with a swizzle (`.r`, `.xy`); to read an output of a node by number write
`OutputIndex = k` in the call. Real indexing needs a `.dss` file.

## DSH2203

<!-- generated:begin DSH2203 -->
**Severity** error

**Message**

```
Expected a constructor such as 'float3(x)' in a 1.x Graph expression, found the cast '({0})', which 1.x never had; move this code to a .dss file.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:502`
<!-- generated:end DSH2203 -->

**Cause.** A C-style cast such as `(float3)x` stands in a 1.x Graph expression. 1.x had constructors
only.

**Fix.** Write the constructor, `float3(x)`, or move the code to a `.dss` file.

## DSH2204

<!-- generated:begin DSH2204 -->
**Severity** error

**Message**

```
Expected an assignment only as a whole 1.x Graph statement, found one inside an expression; move this code to a .dss file.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:513`
<!-- generated:end DSH2204 -->

**Cause.** An assignment was used as a value inside a 1.x Graph expression (`a = (b = c)`, or an
assignment as a call argument). In 1.x an assignment is a whole statement.

**Fix.** Split it into two statements, or move the code to a `.dss` file.

## DSH2205

<!-- generated:begin DSH2205 -->
**Severity** error

**Message**

```
Expected '=' in a 1.x Graph assignment, found '{0}', which 1.x read as the declaration of a variable named '{1}'; write 'x = x + y' or move this code to a .dss file.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:316`
<!-- generated:end DSH2205 -->

**Cause.** A compound assignment (`+=`, `*=`, ...) stands in a 1.x Graph body. 1.x did not know
these operators: it read `x += y` as the declaration of a variable, with `x` as its type, and failed
much later or not at all.

**Fix.** Write it out: `x = x + y;`. Compound assignment exists in a `.dss` file.

## DSH2206

<!-- generated:begin DSH2206 -->
**Severity** error

**Message**

```
Expected a variable or 'variable.member' on the left of a 1.x Graph assignment, found '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:341`
<!-- generated:end DSH2206 -->

**Cause.** The left side of a 1.x Graph assignment is neither a variable nor `variable.member`. 1.x
could assign to a local, an output, or one member of `Base` / a material variable, and to nothing
else (not to a swizzle of a swizzle, an index or a call result).

**Fix.** Assign to a plain variable and combine the parts with a constructor on the right-hand side.

## DSH2207

<!-- generated:begin DSH2207 -->
**Severity** error

**Message**

```
Expected no '++' or '--' in a 1.x Graph expression, found '{0}'; move this code to a .dss file.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:444`
<!-- generated:end DSH2207 -->

**Cause.** `++` or `--` stands in a 1.x Graph expression. 1.x had no such operators and no loops to
use them in.

**Fix.** Write `x = x + 1.0;`, or move the code to a `.dss` file.

## DSH2208

<!-- generated:begin DSH2208 -->
**Severity** error

**Message**

```
Expected a declaration, an assignment, a call or 'if' in a 1.x Graph body, found '{0}', which 1.x did not have; move this code to a .dss file.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:227`
<!-- generated:end DSH2208 -->

**Cause.** A statement 1.x never had opens this line of a Graph body: `for`, `while`, `return`,
`switch`, `break` and the like. A 1.x Graph body is declarations, assignments, calls and `if` /
`else`.

**Fix.** Move loops and early exits into a `Function` (HLSL), or migrate the file and write them in
the `.dss`.

## DSH2209

<!-- generated:begin DSH2209 -->
**Severity** error

**Message**

```
Expected braces around the body of a 1.x Graph 'if', found a single statement.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:256`, `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:281`
<!-- generated:end DSH2209 -->

**Cause.** The body of a 1.x Graph `if` or `else` is a single statement without braces, and 1.x
required them.

**Fix.** Put `{ }` around the body, even when it is one statement.

## DSH2210

<!-- generated:begin DSH2210 -->
**Severity** error

**Message**

```
Expected at most one comparison in a 1.x Graph 'if' condition, found '{0}' as well, where 1.x silently dropped the rest of the condition; move this code to a .dss file.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:298`
<!-- generated:end DSH2210 -->

**Cause.** A 1.x Graph `if` condition holds more than one comparison (`a > b && c < d`). 1.x read
the first comparison and silently dropped the rest, so the branch was taken on half of the
condition.

**Fix.** Nest two `if`s, or compute the combined mask first and compare that; `&&` and `||` work in
a `.dss` file.

## DSH2211

<!-- generated:begin DSH2211 -->
**Severity** error

**Message**

```
Expected a call or an assignment as a 1.x Graph statement, found an expression whose value is never used.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:150`
<!-- generated:end DSH2211 -->

**Cause.** A 1.x Graph statement is an expression whose value goes nowhere, such as `a + b;`. It
builds nodes nothing reads, which is almost always a lost assignment.

**Fix.** Assign the value to a variable or an output, or delete the line.

## DSH2212

<!-- generated:begin DSH2212 -->
**Severity** error

**Message**

```
Expected no storage keyword on a 1.x Graph variable, found '{0}'; move this code to a .dss file.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:96`
<!-- generated:end DSH2212 -->

**Cause.** A storage keyword (`static`, `const`, `uniform`) stands on a variable inside a 1.x Graph
body. 1.x locals had none; constants and parameters live in `Properties`.

**Fix.** Remove the keyword, or declare the value in `Properties`. A `.dss` file has `const` locals.

## DSH2213

<!-- generated:begin DSH2213 -->
**Severity** error

**Message**

```
Expected a single value in a 1.x Graph declaration, found the array declarator '{0}'; move this code to a .dss file.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:108`
<!-- generated:end DSH2213 -->

**Cause.** A 1.x Graph variable is declared as an array. The 1.x graph had no array values.

**Fix.** Use separate variables, or move the code to a `.dss` file or into a `Function`.

## DSH2214

<!-- generated:begin DSH2214 -->
**Severity** error

**Message**

```
Expected an expression as a 1.x Graph initializer, found an initializer list; use a constructor such as 'float3(a, b, c)'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:522`
<!-- generated:end DSH2214 -->

**Cause.** A braced list stands where 1.x did not read one. 1.x accepted `T x = {a, b, c};` as the
constructor `T(a, b, c)` and `T x = {};` as zero, in a declaration, where the type is written next
to it; in an assignment or as an argument the list has no type to construct.

**Fix.** Write the constructor: `float3(a, b, c)`.

## DSH2215

<!-- generated:begin DSH2215 -->
**Severity** error

**Message**

```
Expected an initializer on the 1.x Graph variable '{0}' of type '{1}', found none, and 1.x had no zero value for that type.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:1165`
<!-- generated:end DSH2215 -->

**Cause.** A 1.x Graph variable has no initializer and a type 1.x had no zero for (a texture, a
Substrate value, a sampler, a user type). Numbers started as zero and a `MaterialAttributes` as an
empty set; nothing else had a default.

**Fix.** Give the variable an initializer.

## DSH2216

<!-- generated:begin DSH2216 -->
**Severity** error

**Message**

```
Expected a name after '#Region', found none.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:1023`
<!-- generated:end DSH2216 -->

**Cause.** `#Region` has no name after it. The name is the title of the comment box the region
becomes in the graph.

**Fix.** Write `#Region "Name"`.

## DSH2217

<!-- generated:begin DSH2217 -->
**Severity** error

**Message**

```
Expected a '#Region' before this '#EndRegion', found none open.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:171`
<!-- generated:end DSH2217 -->

**Cause.** An `#EndRegion` has no `#Region` open before it in this Graph body.

**Fix.** Remove it, or add the `#Region "Name"` it was meant to close.

## DSH2218

<!-- generated:begin DSH2218 -->
**Severity** error

**Message**

```
Expected '#EndRegion' to close the region '{0}' before the end of the Graph body, found none.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:65`
<!-- generated:end DSH2218 -->

**Cause.** A `#Region` is still open where the Graph body ends.

**Fix.** Close it with `#EndRegion` before the body's `}`.

## DSH2219

<!-- generated:begin DSH2219 -->
**Severity** error

**Message**

```
Expected '#Region' or '#EndRegion' as the only '#' line in a 1.x Graph body, found '#{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:1007`
<!-- generated:end DSH2219 -->

**Cause.** A `#` line other than `#Region` / `#EndRegion` stands inside a 1.x Graph body.
Conditional compilation (`#if` ...) is resolved before the parser runs; anything that reaches it
here is a directive 1.x did not have, such as `#pragma` or `#include`.

**Fix.** Move `import` lines to the top of the file; `#pragma` belongs in a `.dss` file.

## DSH2220

<!-- generated:begin DSH2220 -->
**Severity** error

**Message**

```
Expected no bare block in a 1.x Graph body, found one; 1.x has blocks only after 'if' and 'else'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:191`
<!-- generated:end DSH2220 -->

**Cause.** A bare `{ ... }` block stands in a 1.x Graph body. 1.x had blocks only after `if` and
`else`, and its variables were all of one scope, so a block of its own meant nothing.

**Fix.** Remove the braces.

## DSH2222

<!-- generated:begin DSH2222 -->
**Severity** error

**Message**

```
Expected a decimal number in a 1.x Graph expression, found the hexadecimal literal '{0}', which 1.x read as 0 followed by a name; move this code to a .dss file.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:364`
<!-- generated:end DSH2222 -->

**Cause.** A hexadecimal literal stands in a 1.x Graph expression. The 1.x number reader stopped
after the `0` and read `x1F` as a name, so the value was silently 0.

**Fix.** Write the number in decimal.

## DSH2240

<!-- generated:begin DSH2240 -->
**Severity** error

**Message**

```
Expected a top-level Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend, Function, GraphFunction, Namespace, VirtualFunction or import, found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:476`
<!-- generated:end DSH2240 -->

**Cause.** Something other than a 1.x block opens a declaration at the top of a `.dsm` / `.dsf`
file, or of a 1.x part of a `.dsh`.

**Fix.** A 1.x file is a list of `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`,
`Function`, `GraphFunction`, `Namespace`, `VirtualFunction` blocks and `import` lines. Check for a
stray token or an unclosed block above this line.

## DSH2241

<!-- generated:begin DSH2241 -->
**Severity** error

**Message**

```
Expected '(' with the block's attributes, such as '(Name = "M_Example")', found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:525`
<!-- generated:end DSH2241 -->

**Cause.** A block word is not followed by its attribute list. Every 1.x block says at least its
name there.

**Fix.** Write `Shader(Name = "M_Example")` and open the body with `{` after it.

## DSH2242

<!-- generated:begin DSH2242 -->
**Severity** error

**Message**

```
Expected a 'Name = "..."' attribute on '{0}', found none.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:731`
<!-- generated:end DSH2242 -->

**Cause.** The block has an attribute list and no `Name` in it. The name is the asset the block
builds (or, for a VirtualFunction, the name calls use).

**Fix.** Add `Name = "..."`.

## DSH2243

<!-- generated:begin DSH2243 -->
**Severity** error

**Message**

```
Expected an attribute name such as 'Name', found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:544`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:553`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:564`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:610`
<!-- generated:end DSH2243 -->

**Cause.** The attribute list of a block is not `Key = value` pairs separated by commas: a key is
missing, the `=` is, or the value is not a string, a word or a number.

**Fix.** Write each attribute as `Key = "value"`.

## DSH2244

<!-- generated:begin DSH2244 -->
**Severity** warning

**Message**

```
The attribute '{0}' is written twice; the later value wins, as it did in 1.x.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:592`
<!-- generated:end DSH2244 -->

**Cause.** One attribute is written twice in a block's attribute list. 1.x kept the later value, and
so does this front end; the warning is there because the first value is dead text.

**Fix.** Remove one of them.

## DSH2245

<!-- generated:begin DSH2245 -->
**Severity** error

**Message**

```
Expected a Shader section (Properties, Settings, Outputs, Graph or Layout), found '{0}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2471`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:931`
<!-- generated:end DSH2245 -->

**Cause.** A word that is no section opens a line inside a block. A Shader has `Properties`,
`Settings`, `Outputs`, `Graph` and `Layout`; a ShaderFunction has `Inputs` instead of `Properties`;
a VirtualFunction has `Options`, `Inputs` and `Outputs`.

**Fix.** Check the spelling (sections are case-sensitive) and that the section before it is closed.

## DSH2246

<!-- generated:begin DSH2246 -->
**Severity** error

**Message**

```
Expected 'Graph' as the body section of '{0}', found 'Code', which 1.x accepted only inside a Function.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:923`
<!-- generated:end DSH2246 -->

**Cause.** A Shader or ShaderFunction block has a `Code` section. 1.x read `Code` only inside a
`Function`; in an asset block the body is `Graph`.

**Fix.** Rename the section to `Graph`, or move the HLSL into a `Function` and call it.

## DSH2247

<!-- generated:begin DSH2247 -->
**Severity** error

**Message**

```
Expected no body in the VirtualFunction '{0}', which declares an existing asset, found the section '{1}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2463`
<!-- generated:end DSH2247 -->

**Cause.** A VirtualFunction has a `Graph` or `Code` section. A VirtualFunction declares the
interface of an asset that already exists; it builds nothing.

**Fix.** Remove the body, or make the block a `ShaderFunction` if the source is meant to build the
asset.

## DSH2248

<!-- generated:begin DSH2248 -->
**Severity** error

**Message**

```
Expected a 1.x block in a '.{0}' file, found {1}, which is 2.0 syntax; 2.0 declarations belong in a .dss file or a .dsh header.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:467`
<!-- generated:end DSH2248 -->

**Cause.** 2.0 syntax (a `uniform`, an `export` function, a `#pragma`) stands in a `.dsm` or `.dsf`
file, which is read by the 1.x front end only.

**Fix.** Put 2.0 declarations in a `.dss` file, or in a `.dsh` header, which takes both dialects.
`dsc migrate` rewrites a whole 1.x file.

## DSH2249

<!-- generated:begin DSH2249 -->
**Severity** error

**Message**

```
Expected only Function, GraphFunction, Namespace and VirtualFunction blocks in a '.dsh' header, found the asset block '{0}', which belongs in a .dsm or .dsf file.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:490`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:687`
<!-- generated:end DSH2249 -->

**Cause.** A `.dsh` header holds a block that builds an asset (`Shader`, `ShaderFunction`, a layer).
A header is included into other files; an asset block in it would be built once per including file.

**Fix.** Move the block into a `.dsm` / `.dsf` of its own and keep functions and VirtualFunctions in
the header.

## DSH2250

<!-- generated:begin DSH2250 -->
**Severity** error

**Message**

```
Expected one Shader block in a file, found a second one.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:715`
<!-- generated:end DSH2250 -->

**Cause.** A file holds two `Shader` blocks. A material source is one material.

**Fix.** Give each material its own file.

## DSH2251

<!-- generated:begin DSH2251 -->
**Severity** warning

**Message**

```
'{0}' is the old spelling of '{1}'; it still reads the same.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:705`
<!-- generated:end DSH2251 -->

**Cause.** A block word of an earlier 1.x release is used (`MaterialLayer` for `ShaderLayer`,
`MaterialLayerBlend` for `ShaderLayerBlend`). It reads the same.

**Fix.** Nothing has to change; `dsc migrate` writes the current form.

## DSH2252

<!-- generated:begin DSH2252 -->
**Severity** error

**Message**

```
Expected a double-quoted path after 'import', found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:378`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:390`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:407`
<!-- generated:end DSH2252 -->

**Cause.** An `import` line the front end does not read. Three cases: `import` is not followed by a
double-quoted path (1.x also let it stand in single quotes); the path names a file that is no `.dsh`
header -- a material or function file is compiled on its own, never included; or the path is
root-qualified (`Project:Shared/Common.dsh`, `Plugin.X:...`), which the 2.0 include resolver, shared
by both front ends, does not read.

**Fix.** Write `import "Shared/Common.dsh";` with a path relative to this file or to its own source
root. A header that lives in another root has to be reached through a package or copied.

## DSH2253

<!-- generated:begin DSH2253 -->
**Severity** warning

**Message**

```
'{0}' is read as 'import'; write it in lower case.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:369`
<!-- generated:end DSH2253 -->

**Cause.** `Import` (or another casing) is used for `import`. 1.x matched the word loosely.

**Fix.** Write `import` in lower case.

## DSH2254

<!-- generated:begin DSH2254 -->
**Severity** error

**Message**

```
Expected a top-level Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend, Function, GraphFunction, Namespace or VirtualFunction block in this 1.x file, found none.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:307`
<!-- generated:end DSH2254 -->

**Cause.** The file has no block at all: it is empty, everything in it is commented out, or an `#if`
removed it.

**Fix.** A `.dsm` needs a `Shader`, a `.dsf` a `ShaderFunction`, `ShaderLayer` or
`ShaderLayerBlend`. Delete the file if it is a leftover.

## DSH2255

<!-- generated:begin DSH2255 -->
**Severity** error

**Message**

```
Expected a Graph section in the Shader '{0}', found none.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:974`
<!-- generated:end DSH2255 -->

**Cause.** A Shader has no `Graph` section, so there is nothing to build.

**Fix.** Add `Graph = { ... }`.

## DSH2256

<!-- generated:begin DSH2256 -->
**Severity** warning

**Message**

```
The Shader '{0}' has no Outputs section, so nothing its Graph computes reaches the material.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:981`
<!-- generated:end DSH2256 -->

**Cause.** A Shader has a Graph and no `Outputs` section, so no value reaches a material attribute.
The material builds, with nothing wired to it.

**Fix.** Add `Outputs = { ... }` with at least one `Base.<Attribute> = <variable>;` binding.

## DSH2257

<!-- generated:begin DSH2257 -->
**Severity** error

**Message**

```
Expected '`{' to open the '{0}' block, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2267`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2381`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2420`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2437`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:744`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:817`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:834`
<!-- generated:end DSH2257 -->

**Cause.** A block is not opened where one has to be: the attribute list of a `Shader`,
`ShaderFunction`, `Namespace` or `VirtualFunction` is not followed by `{`, a section name
(`Properties`, `Graph`, `Inputs`, ...) is missing inside the block, or the section name is not
followed by `{`.

**Fix.** Open the body with `{` on the same line or the next, and start each section with its name.

## DSH2258

<!-- generated:begin DSH2258 -->
**Severity** warning

**Message**

```
The section '{0}' is written twice; the later one wins, as it did in 1.x.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:897`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:911`
<!-- generated:end DSH2258 -->

**Cause.** One section is written twice in a block. 1.x kept the later one and dropped the first
silently; this front end does the same and says so.

**Fix.** Merge the two sections into one.

