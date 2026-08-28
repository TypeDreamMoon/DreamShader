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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:23`, `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:81`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:112`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:146`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:169`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:222`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:239`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserScanner.cpp:324`
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

