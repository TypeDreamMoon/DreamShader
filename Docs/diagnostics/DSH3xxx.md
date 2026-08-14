# DSH3xxx --- Sections and declarations

> The block between the generated markers is written by `.skill/gen-diagnostics.ps1`.
> Everything below a marker is written by hand and survives a regeneration.

## DSH3001

<!-- generated:begin DSH3001 -->
**Severity** error

**Message**

```
GraphFunction declaration is missing a valid function name.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:522`
<!-- generated:end DSH3001 -->

**Cause.** the token after `GraphFunction` is not an identifier

**Fix.** supply a name; note that `GraphFunction` accepts no `SelfContained` / `Inline` modifier

**See** [GraphFunction](../language/graph-function.md)

## DSH3002

<!-- generated:begin DSH3002 -->
**Severity** error

**Message**

```
Function declaration is missing a valid function name after SelfContained.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:536`
<!-- generated:end DSH3002 -->

**Cause.** `Function SelfContained(` or `Function Inline(`

**Fix.** supply a name after the modifier

**See** [Function](../language/function.md)

## DSH3003

<!-- generated:begin DSH3003 -->
**Severity** error

**Message**

```
{0} declaration is missing a function name after the return type '{1}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:554`
<!-- generated:end DSH3003 -->

**Cause.** a return type was read but no name followed

**Fix.** supply a name

**See** [Function](../language/function.md)

## DSH3004

<!-- generated:begin DSH3004 -->
**Severity** error

**Message**

```
{0} '{1}' is missing a valid parameter list. {2}
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:568`
<!-- generated:end DSH3004 -->

**Cause.** the `( … )` parameter list could not be extracted

**Fix.** balance the parentheses

**See** [Function](../language/function.md)

## DSH3005

<!-- generated:begin DSH3005 -->
**Severity** error

**Message**

```
{0} '{1}' is missing a valid body block. {2}
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:578`
<!-- generated:end DSH3005 -->

**Cause.** the `{ … }` body of a `Function` / `GraphFunction` could not be extracted

**Fix.** balance the braces

**See** [Function](../language/function.md)

## DSH3006

<!-- generated:begin DSH3006 -->
**Severity** error

**Message**

```
Function '{0}' has an invalid return type '{1}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:410`
<!-- generated:end DSH3006 -->

**Cause.** the return-type token normalized to the empty string

**Fix.** use a real type token

**See** [Types](../language/types.md)

## DSH3007

<!-- generated:begin DSH3007 -->
**Severity** error

**Message**

```
Function '{0}' has an invalid parameter declaration '{1}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:430`, `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:467`
<!-- generated:end DSH3007 -->

**Cause.** the parameter did not split into 2 or 3 whitespace-separated tokens, or its type or name was empty

**Fix.** write `[in\

**See** out] <Type> <Name>` | [Function](../language/function.md)

## DSH3008

<!-- generated:begin DSH3008 -->
**Severity** error

**Message**

```
Function '{0}' parameter '{1}' uses unsupported qualifier '{2}'. Supported qualifiers are in and out.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:458`
<!-- generated:end DSH3008 -->

**Cause.** a qualifier other than `in` / `out` — `inout` included

**Fix.** use `in` or `out`

**See** [Function](../language/function.md)

## DSH3009

<!-- generated:begin DSH3009 -->
**Severity** error

**Message**

```
Function '{0}' parameter name '__return' is reserved for return-type lowering.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:475`
<!-- generated:end DSH3009 -->

**Cause.** a user parameter named `__return` (matched ignoring case)

**Fix.** rename the parameter

**See** [Function](../language/function.md)

## DSH3010

<!-- generated:begin DSH3010 -->
**Severity** error

**Message**

```
Function '{0}' has a return type and cannot also declare out parameters. Use out parameters without a return type for multiple outputs.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:495`
<!-- generated:end DSH3010 -->

**Cause.** a return-typed `Function` also declared `out`

**Fix.** pick one form

**See** [Function](../language/function.md)

## DSH3011

<!-- generated:begin DSH3011 -->
**Severity** error

**Message**

```
Function '{0}' must declare at least one out parameter.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:502`
<!-- generated:end DSH3011 -->

**Cause.** no `out` parameter and no return type

**Fix.** add an `out` parameter or a return type

**See** [Function](../language/function.md)

## DSH3012

<!-- generated:begin DSH3012 -->
**Severity** error

**Message**

```
A function with a return type cannot use a bare 'return;'. Return a value, e.g. 'return expr;'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:375`
<!-- generated:end DSH3012 -->

**Cause.** a top-level `return;` inside a `Function` that declares a return type

**Fix.** return a value, or drop the return type and use `out` parameters

**See** [Function](../language/function.md)

## DSH3020

<!-- generated:begin DSH3020 -->
**Severity** error

**Message**

```
Namespace(Name=\"...\") is required.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:630`
<!-- generated:end DSH3020 -->

**Cause.** the header has no `Name` attribute

**Fix.** add `Name="…"`

**See** [Namespace](../language/namespace.md)

## DSH3021

<!-- generated:begin DSH3021 -->
**Severity** error

**Message**

```
Namespace name cannot be empty.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:637`
<!-- generated:end DSH3021 -->

**Cause.** `Namespace(Name="")`

**Fix.** supply a name

**See** [Namespace](../language/namespace.md)

## DSH3022

<!-- generated:begin DSH3022 -->
**Severity** error

**Message**

```
Namespace name '{0}' is not a valid identifier.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:646`
<!-- generated:end DSH3022 -->

**Cause.** the name contains an illegal character

**Fix.** use `[A-Za-z_][A-Za-z0-9_]*`

**See** [Namespace](../language/namespace.md)

## DSH3023

<!-- generated:begin DSH3023 -->
**Severity** error

**Message**

```
Namespace '{0}' may only contain Function or GraphFunction blocks.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:685`
<!-- generated:end DSH3023 -->

**Cause.** any other token in a `Namespace` body, including a nested `Namespace`

**Fix.** move the block out; namespaces do not nest

**See** [Namespace](../language/namespace.md)

## DSH3030

<!-- generated:begin DSH3030 -->
**Severity** error

**Message**

```
Only one top-level Shader block is currently supported.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:720`
<!-- generated:end DSH3030 -->

**Cause.** a second `Shader` keyword in the parse unit — enforced across the whole transitive import closure, not per file

**Fix.** split into separate `.dsm` files

**See** [Shader](../language/shader.md)

## DSH3031

<!-- generated:begin DSH3031 -->
**Severity** error

**Message**

```
Shader(Name=\"...\") is required.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:735`
<!-- generated:end DSH3031 -->

**Cause.** the `Shader` header has no `Name` attribute

**Fix.** add `Name="…"`

**See** [Shader](../language/shader.md)

## DSH3032

<!-- generated:begin DSH3032 -->
**Severity** error

**Message**

```
Shader must provide a Graph block.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:932`
<!-- generated:end DSH3032 -->

**Cause.** a `Shader` with an empty `Code` and no initialized output declaration

**Fix.** add `Graph = { … }`, or initialize an output declaration

**See** [Shader](../language/shader.md)

## DSH3040

<!-- generated:begin DSH3040 -->
**Severity** error

**Message**

```
VirtualFunction(Name=\"...\") is required.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:792`
<!-- generated:end DSH3040 -->

**Cause.** the header has no `Name` attribute

**Fix.** add `Name="…"`

**See** [VirtualFunction](../language/virtual-function.md)

## DSH3041

<!-- generated:begin DSH3041 -->
**Severity** error

**Message**

```
VirtualFunction name cannot be empty.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:797`
<!-- generated:end DSH3041 -->

**Cause.** `VirtualFunction(Name="")`

**Fix.** supply a name

**See** [VirtualFunction](../language/virtual-function.md)

## DSH3042

<!-- generated:begin DSH3042 -->
**Severity** error

**Message**

```
VirtualFunction '{0}' must provide Options = {{ Asset = Path(...); }}.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:825`
<!-- generated:end DSH3042 -->

**Cause.** neither the header `Asset=` attribute nor `Options.Asset` supplied a non-empty asset

**Fix.** add one of them

**See** [VirtualFunction](../language/virtual-function.md)

## DSH3043

<!-- generated:begin DSH3043 -->
**Severity** error

**Message**

```
VirtualFunction '{0}' must declare at least one output.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:831`
<!-- generated:end DSH3043 -->

**Cause.** the block declared no `Outputs` / `Results` entry

**Fix.** add an output

**See** [VirtualFunction](../language/virtual-function.md)

## DSH3050

<!-- generated:begin DSH3050 -->
**Severity** error

**Message**

```
{0}(Name=\"...\") is required.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:890`
<!-- generated:end DSH3050 -->

**Cause.** a `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` / `MaterialLayer` / `MaterialLayerBlend` header has no `Name`; `{Block}` echoes the spelling actually typed

**Fix.** add `Name="…"`

**See** [ShaderFunction](../language/shader-function.md)

## DSH3060

<!-- generated:begin DSH3060 -->
**Severity** error

**Message**

```
Unknown shader section '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1805`
<!-- generated:end DSH3060 -->

**Cause.** a section other than `Properties` / `Settings` / `Outputs` / `Graph` / `Layout` / `Code`

**Fix.** check the spelling; `Inputs`, `Results` and `Options` are not accepted in a `Shader`

**See** [Shader](../language/shader.md)

## DSH3061

<!-- generated:begin DSH3061 -->
**Severity** error

**Message**

```
Unknown shader function section '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1864`
<!-- generated:end DSH3061 -->

**Cause.** a section name a `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` body does not accept

**Fix.** use `Properties`, `Inputs`, `Outputs`, `Settings`, `Graph` or `Layout`

**See** [ShaderFunction](../language/shader-function.md)

## DSH3062

<!-- generated:begin DSH3062 -->
**Severity** error

**Message**

```
Unknown material function section '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1961`
<!-- generated:end DSH3062 -->

**Cause.** a section other than `Properties` / `Inputs` / `Outputs` / `Results` / `Settings` / `Graph` / `Layout` / `Code`

**Fix.** check the spelling; `Options` is not accepted here

**See** [ShaderFunction](../language/shader-function.md)

## DSH3063

<!-- generated:begin DSH3063 -->
**Severity** error

**Message**

```
Unknown VirtualFunction section '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:2029`
<!-- generated:end DSH3063 -->

**Cause.** a section other than `Inputs` / `Properties` / `Outputs` / `Results` / `Options` / `Settings`

**Fix.** remove it; `Graph`, `Code` and `Layout` are not accepted here

**See** [VirtualFunction](../language/virtual-function.md)

## DSH3064

<!-- generated:begin DSH3064 -->
**Severity** error

**Message**

```
VirtualFunction declares an existing MaterialFunction asset and does not support Graph or Code sections.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:2024`
<!-- generated:end DSH3064 -->

**Cause.** a `Graph` or `Code` section inside `VirtualFunction`

**Fix.** remove it — a `VirtualFunction` only *declares* an existing asset

**See** [VirtualFunction](../language/virtual-function.md)

## DSH3065

<!-- generated:begin DSH3065 -->
**Severity** error

**Message**

```
Shader graph sections now use Graph = { ... }. Function Code = { ... } is still supported.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1800`
<!-- generated:end DSH3065 -->

**Cause.** a `Code` section inside `Shader`

**Fix.** rename it to `Graph`

**See** [Shader](../language/shader.md)

## DSH3066

<!-- generated:begin DSH3066 -->
**Severity** error

**Message**

```
ShaderFunction, ShaderLayer, and ShaderLayerBlend graph sections now use Graph = { ... }. Function Code = { ... } is still supported.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1956`
<!-- generated:end DSH3066 -->

**Cause.** a `Code` section inside a material-function block

**Fix.** rename it to `Graph`

**See** [ShaderFunction](../language/shader-function.md)

## DSH3070

<!-- generated:begin DSH3070 -->
**Severity** error

**Message**

```
Invalid typed declaration '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1167`, `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1177`, `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1664`
<!-- generated:end DSH3070 -->

**Cause.** the left side does not split into `<Type> <Name>`, or the name is not an identifier. **A tab between the type and the name fails here** — this splitter looks for a literal space

**Fix.** replace the tab with a space

**See** [Inputs / Outputs / Results](../language/inputs-outputs.md)

## DSH3080

<!-- generated:begin DSH3080 -->
**Severity** error

**Message**

```
Output binding target cannot be empty.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1196`
<!-- generated:end DSH3080 -->

**Cause.** the left side of a binding is empty

**Fix.** supply a target

**See** [Output bindings](../language/output-bindings.md)

## DSH3081

<!-- generated:begin DSH3081 -->
**Severity** error

**Message**

```
Output binding target '{0}' is empty.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1209`
<!-- generated:end DSH3081 -->

**Cause.** `Base.` with nothing after it

**Fix.** name a material property

**See** [Output bindings](../language/output-bindings.md)

## DSH3082

<!-- generated:begin DSH3082 -->
**Severity** error

**Message**

```
Output binding target '{0}' must start with Base. for material outputs or Expression(...) for output nodes.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1218`
<!-- generated:end DSH3082 -->

**Cause.** a binding target that is neither form

**Fix.** use `Base.<Property>` or `Expression( … ).Pin[i]`

**See** [Output bindings](../language/output-bindings.md)

## DSH3083

<!-- generated:begin DSH3083 -->
**Severity** error

**Message**

```
Invalid output expression target '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1227`
<!-- generated:end DSH3083 -->

**Cause.** `Expression` was not followed by a balanced `( … )`

**Fix.** fix the parentheses

**See** [Output bindings](../language/output-bindings.md)

## DSH3084

<!-- generated:begin DSH3084 -->
**Severity** error

**Message**

```
Unsupported output target '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1235`
<!-- generated:end DSH3084 -->

**Cause.** the text before `(` is not exactly `Expression`

**Fix.** use `Expression( … )`

**See** [Output bindings](../language/output-bindings.md)

## DSH3085

<!-- generated:begin DSH3085 -->
**Severity** error

**Message**

```
Expression output target '{0}' must select a pin with .Pin[index].
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1244`
<!-- generated:end DSH3085 -->

**Cause.** the text after `)` does not start with `.`

**Fix.** append `.Pin[<index>]`

**See** [Output bindings](../language/output-bindings.md)

## DSH3086

<!-- generated:begin DSH3086 -->
**Severity** error

**Message**

```
Expression output target '{0}' must use .Pin[index] syntax.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1252`
<!-- generated:end DSH3086 -->

**Cause.** the suffix after `)` is not `Pin[ … ]`

**Fix.** use exactly `.Pin[<index>]`

**See** [Output bindings](../language/output-bindings.md)

## DSH3087

<!-- generated:begin DSH3087 -->
**Severity** error

**Message**

```
Expression output target '{0}' has an invalid pin index.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1260`
<!-- generated:end DSH3087 -->

**Cause.** the `.Pin[…]` index is not a non-negative integer

**Fix.** use `.Pin[0]`, `.Pin[1]`, …

**See** [Output bindings](../language/output-bindings.md)

## DSH3088

<!-- generated:begin DSH3088 -->
**Severity** error

**Message**

```
Expression output target argument '{0}' must use Key=Value syntax.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1273`
<!-- generated:end DSH3088 -->

**Cause.** a positional argument inside `Expression( … )`

**Fix.** every argument must be `Key=Value`

**See** [Output bindings](../language/output-bindings.md)

## DSH3089

<!-- generated:begin DSH3089 -->
**Severity** error

**Message**

```
Invalid expression output target argument '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1282`
<!-- generated:end DSH3089 -->

**Cause.** empty key or value inside `Expression( … )`

**Fix.** supply both sides

**See** [Output bindings](../language/output-bindings.md)

## DSH3090

<!-- generated:begin DSH3090 -->
**Severity** error

**Message**

```
Expression output target argument '{0}' is declared more than once.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1289`
<!-- generated:end DSH3090 -->

**Cause.** duplicate argument key after normalization

**Fix.** remove the duplicate

**See** [Output bindings](../language/output-bindings.md)

## DSH3091

<!-- generated:begin DSH3091 -->
**Severity** error

**Message**

```
Expression output target '{0}' must specify Class=\\\"...\\\".
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1304`
<!-- generated:end DSH3091 -->

**Cause.** the `Expression( … )` argument list has no `Class`

**Fix.** add `Class="MaterialExpressionName"`

**See** [Output bindings](../language/output-bindings.md)

## DSH3092

<!-- generated:begin DSH3092 -->
**Severity** error

**Message**

```
Invalid output declaration initializer '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1333`
<!-- generated:end DSH3092 -->

**Cause.** an initialized output declaration whose right-hand side is empty

**Fix.** supply an initializer

**See** [Output bindings](../language/output-bindings.md)

## DSH3093

<!-- generated:begin DSH3093 -->
**Severity** error

**Message**

```
Invalid output binding '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1345`
<!-- generated:end DSH3093 -->

**Cause.** a binding statement whose right-hand side is empty

**Fix.** supply a source variable or expression

**See** [Output bindings](../language/output-bindings.md)

## DSH3100

<!-- generated:begin DSH3100 -->
**Severity** error

**Message**

```
Invalid Layout statement '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1382`
<!-- generated:end DSH3100 -->

**Cause.** the statement is not a balanced `Name( … )` call

**Fix.** fix the parentheses

**See** [Layout](../language/layout.md)

## DSH3101

<!-- generated:begin DSH3101 -->
**Severity** error

**Message**

```
Unexpected text after Layout statement '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1389`
<!-- generated:end DSH3101 -->

**Cause.** text after the closing `)` of a `Layout` call

**Fix.** end the statement at `)`

**See** [Layout](../language/layout.md)

## DSH3102

<!-- generated:begin DSH3102 -->
**Severity** error

**Message**

```
Invalid Layout statement name in '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1397`
<!-- generated:end DSH3102 -->

**Cause.** the call name is not a bare identifier

**Fix.** use `Node` or `Comment`

**See** [Layout](../language/layout.md)

## DSH3103

<!-- generated:begin DSH3103 -->
**Severity** error

**Message**

```
Layout argument '{0}' must use Key=Value syntax.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1410`
<!-- generated:end DSH3103 -->

**Cause.** a positional argument in a `Layout` call

**Fix.** use `Key=Value`

**See** [Layout](../language/layout.md)

## DSH3104

<!-- generated:begin DSH3104 -->
**Severity** error

**Message**

```
Invalid Layout argument '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1419`
<!-- generated:end DSH3104 -->

**Cause.** empty key or empty value in a `Layout` call

**Fix.** supply both sides

**See** [Layout](../language/layout.md)

## DSH3105

<!-- generated:begin DSH3105 -->
**Severity** error

**Message**

```
Layout argument '{0}' is declared more than once.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1426`
<!-- generated:end DSH3105 -->

**Cause.** duplicate argument key

**Fix.** remove the duplicate

**See** [Layout](../language/layout.md)

## DSH3106

<!-- generated:begin DSH3106 -->
**Severity** error

**Message**

```
Layout argument '{0}' is required.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1446`
<!-- generated:end DSH3106 -->

**Cause.** a required argument is absent

**Fix.** supply it

**See** [Layout](../language/layout.md)

## DSH3107

<!-- generated:begin DSH3107 -->
**Severity** error

**Message**

```
Layout argument '{0}' must be an integer.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1464`
<!-- generated:end DSH3107 -->

**Cause.** `X` / `Y` / `W` / `H` is not an integer

**Fix.** use an integer

**See** [Layout](../language/layout.md)

## DSH3108

<!-- generated:begin DSH3108 -->
**Severity** error

**Message**

```
Invalid Layout Node statement '{0}'. {1}
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1498`
<!-- generated:end DSH3108 -->

**Cause.** a `Node( … )` call failed argument validation

**Fix.** supply `Var`, `X`, `Y`

**See** [Layout](../language/layout.md)

## DSH3109

<!-- generated:begin DSH3109 -->
**Severity** error

**Message**

```
Invalid Layout Comment statement '{0}'. {1}
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1517`
<!-- generated:end DSH3109 -->

**Cause.** a `Comment( … )` call failed argument validation

**Fix.** supply `Name`, `X`, `Y`, `W`, `H`; `Color` is optional

**See** [Layout](../language/layout.md)

## DSH3110

<!-- generated:begin DSH3110 -->
**Severity** error

**Message**

```
Layout Comment Color must be a float4 literal in '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1527`
<!-- generated:end DSH3110 -->

**Cause.** `Color=` is not a four-component literal

**Fix.** write `Color=float4(r, g, b, a)`

**See** [Layout](../language/layout.md)

## DSH3111

<!-- generated:begin DSH3111 -->
**Severity** error

**Message**

```
Unknown Layout statement '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1537`
<!-- generated:end DSH3111 -->

**Cause.** a call other than `Node` or `Comment`

**Fix.** use `Node` or `Comment`

**See** [Layout](../language/layout.md)

## DSH3120

<!-- generated:begin DSH3120 -->
**Severity** error

**Message**

```
Graph #Region on line {0} must include a name.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1593`
<!-- generated:end DSH3120 -->

**Cause.** `#Region` with a blank name

**Fix.** write `#Region "Name"` or `#Region Name`

**See** [Layout](../language/layout.md)

## DSH3121

<!-- generated:begin DSH3121 -->
**Severity** error

**Message**

```
Graph #EndRegion on line {0} has no matching #Region.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1605`
<!-- generated:end DSH3121 -->

**Cause.** an unbalanced `#EndRegion`

**Fix.** remove it or add the opening directive

**See** [Layout](../language/layout.md)

## DSH3122

<!-- generated:begin DSH3122 -->
**Severity** error

**Message**

```
Graph #Region '{0}' is missing #EndRegion.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1630`
<!-- generated:end DSH3122 -->

**Cause.** a region left open at the end of the `Graph` body; the innermost open region is reported

**Fix.** close the region

**See** [Layout](../language/layout.md)

## DSH3130

<!-- generated:begin DSH3130 -->
**Severity** error

**Message**

```
Unexpected '{{' in Properties near '{0}'. Only Group(\"Name\") {{ ... }} may open a brace here.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1027`
<!-- generated:end DSH3130 -->

**Cause.** a `{` inside `Properties` that is not a `Group("Name")` head

**Fix.** remove the brace, or write a proper `Group("Name") { … }`

**See** [Properties](../language/properties.md)

## DSH3131

<!-- generated:begin DSH3131 -->
**Severity** error

**Message**

```
Group(...) requires a non-empty name.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1034`
<!-- generated:end DSH3131 -->

**Cause.** `Group("")`

**Fix.** supply a name

**See** [Properties](../language/properties.md)

## DSH3132

<!-- generated:begin DSH3132 -->
**Severity** error

**Message**

```
Unterminated Group(\"{0}\") {{ ... }} block.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1085`
<!-- generated:end DSH3132 -->

**Cause.** a `Group` scope left unclosed

**Fix.** balance the braces

**See** [Properties](../language/properties.md)

