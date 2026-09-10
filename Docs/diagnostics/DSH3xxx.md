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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:636`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:650`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:668`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:682`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:692`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:524`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:544`, `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:581`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:572`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:589`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:609`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:616`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:489`
<!-- generated:end DSH3012 -->

**Cause.** a top-level `return;` inside a `Function` that declares a return type

**Fix.** return a value, or drop the return type and use `out` parameters

**See** [Function](../language/function.md)

## DSH3020

<!-- generated:begin DSH3020 -->
**Severity** error

**Message**

```
Namespace(Name="...") is required.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:745`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:752`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:761`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:800`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:835`
<!-- generated:end DSH3030 -->

**Cause.** a second `Shader` keyword in the parse unit — enforced across the whole transitive import closure, not per file

**Fix.** split into separate `.dsm` files

**See** [Shader](../language/shader.md)

## DSH3031

<!-- generated:begin DSH3031 -->
**Severity** error

**Message**

```
Shader(Name="...") is required.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:850`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:1047`
<!-- generated:end DSH3032 -->

**Cause.** a `Shader` with an empty `Code` and no initialized output declaration

**Fix.** add `Graph = { … }`, or initialize an output declaration

**See** [Shader](../language/shader.md)

## DSH3040

<!-- generated:begin DSH3040 -->
**Severity** error

**Message**

```
VirtualFunction(Name="...") is required.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:907`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:912`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:940`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:946`
<!-- generated:end DSH3043 -->

**Cause.** the block declared no `Outputs` / `Results` entry

**Fix.** add an output

**See** [VirtualFunction](../language/virtual-function.md)

## DSH3050

<!-- generated:begin DSH3050 -->
**Severity** error

**Message**

```
{0}(Name="...") is required.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParser.cpp:1005`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:2150`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:2209`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:2306`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:2374`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:2369`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:2145`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:2301`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1167`, `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1177`, `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:2009`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1247`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1260`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1269`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1278`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1286`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1295`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1303`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1311`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1324`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1333`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1340`
<!-- generated:end DSH3090 -->

**Cause.** duplicate argument key after normalization

**Fix.** remove the duplicate

**See** [Output bindings](../language/output-bindings.md)

## DSH3091

<!-- generated:begin DSH3091 -->
**Severity** error

**Message**

```
Expression output target '{0}' must specify Class=\"...\".
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1355`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1440`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1452`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1727`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1734`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1742`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1755`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1764`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1771`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1791`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1809`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1843`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1862`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1872`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1882`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1938`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1950`
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

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1975`
<!-- generated:end DSH3122 -->

**Cause.** a region left open at the end of the `Graph` body; the innermost open region is reported

**Fix.** close the region

**See** [Layout](../language/layout.md)

## DSH3130

<!-- generated:begin DSH3130 -->
**Severity** error

**Message**

```
Unexpected '`{' in Properties near '{0}'. Only Group("Name") `{ ... `} may open a brace here.
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
Unterminated Group("{0}") `{ ... `} block.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1085`
<!-- generated:end DSH3132 -->

**Cause.** a `Group` scope left unclosed

**Fix.** balance the braces

**See** [Properties](../language/properties.md)

## DSH3133

<!-- generated:begin DSH3133 -->
**Severity** error

**Message**

```
Unexpected brace block in Outputs near '{0}'. Only Expression(Class="...") opens a brace block here; every other Outputs statement ends with ';'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1481`
<!-- generated:end DSH3133 -->

**Cause.** a `{` at statement level inside `Outputs` that is not preceded by an `Expression( ... )` head. A `.Pin[i]` suffix counts as "not a head": the pin selector belongs to the statement form, and the block form writes its pins one per line inside the braces

**Fix.** open the block with the bare call -- `Expression(Class="...", ...) { ... }` -- or end the preceding statement with `;` and delete the brace

**See** [Output bindings](../language/output-bindings.md#block-form)

## DSH3134

<!-- generated:begin DSH3134 -->
**Severity** error

**Message**

```
Unterminated Expression(...) block in Outputs after '{0}'.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1682`
<!-- generated:end DSH3134 -->

**Cause.** the `}` closing an `Expression( ... ) { ... }` block is missing. In practice an unbalanced brace is caught first by the enclosing `Shader` block, which reports an unterminated block instead; this code is the guard that keeps the `Outputs` scanner from slicing a range it never found

**Fix.** close the block

**See** [Output bindings](../language/output-bindings.md#block-form)

## DSH3135

<!-- generated:begin DSH3135 -->
**Severity** error

**Message**

```
Invalid statement '{0}' inside an Expression(...) block. Only Pin[index] = <source>; is allowed there.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1511`
<!-- generated:end DSH3135 -->

**Cause.** a statement inside an `Expression( ... ) { ... }` block that is not `Pin[index] = <source>;`. Output declarations, `Base.` bindings and nested blocks all belong outside the braces; only the pin bindings of that one node live inside

**Fix.** move the statement out of the block, or rewrite it as `Pin[index] = <source>;`. A block may hold comments and blank lines freely

**See** [Output bindings](../language/output-bindings.md#block-form)

## DSH3136

<!-- generated:begin DSH3136 -->
**Severity** error

**Message**

```
The Expression(...) block for '{0}' binds no pin. Write at least one Pin[index] = <source>; inside it, or delete the block.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1536`
<!-- generated:end DSH3136 -->

**Cause.** an `Expression( ... ) { }` block with no `Pin[index]` statement in it. Such a block would create nothing at all -- the node is materialized by its bindings, so a block with no binding is silently a no-op, which is exactly the class of mistake the block form exists to prevent

**Fix.** bind at least one pin, or delete the block

**See** [Output bindings](../language/output-bindings.md#block-form)

## DSH3137

<!-- generated:begin DSH3137 -->
**Severity** error

**Message**

```
Output target pin '{0}' is bound more than once: first to '{1}', then to '{2}'. An Expression(...) block and an Expression(...).Pin[i] statement that share a class and argument list describe one node, so their pins share one namespace.
```

**Raised by** `Source/DreamShader/Private/Parser/DreamShaderParserSections.cpp:1404`
<!-- generated:end DSH3137 -->

**Cause.** the same input pin of the same terminal node was bound twice. Identity is the node reuse key -- the resolved class plus the sorted argument list -- so the two bindings need not look alike: one may sit in a block and the other in a loose `Expression( ... ).Pin[i] = x;` statement, and they may even live in two different `Outputs` sections. Two `Expression( ... )` specifications whose argument lists differ are two nodes, each with its own `Pin[0]`; that is not a double bind

**Fix.** delete one of the two bindings, or point the second at a different pin. If the two were meant to be different nodes, give them different argument lists -- but note that a `UMaterialExpressionCustomOutput` is normally meant to exist once per material

**See** [Output bindings](../language/output-bindings.md#each-pin-once), [Node reuse](../graph/node-reuse.md)

## DSH3200

<!-- generated:begin DSH3200 -->
**Severity** error

**Message**

```
Unexpected {0} at file scope; expected a declaration, '#pragma', '#include' or 'import'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1229`
<!-- generated:end DSH3200 -->

**Cause.** The parser reached a token at file scope that cannot begin anything: a stray `)`, `}`,
operator, number or string outside any declaration. Typical sources are an extra closing brace
after a function, a statement written at file scope (`x = 1;` needs a type in front of it), or a
1.x block that lost its keyword.

**Fix.** Delete the stray token or turn the line into a declaration. Recovery skips to the next
`;`, matched `}` or line that starts a declaration, so the declarations after it still parse; fix
the first DSH3200 and recompile before chasing the ones that follow, they are usually the same
mistake seen from further down.

## DSH3201

<!-- generated:begin DSH3201 -->
**Severity** error

**Message**

```
Preprocessor directive '#{0}' reached the parser; only '#pragma' and '#include' belong here, and '#if' / '#define' lines must be resolved by the preprocessor first.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:549`
<!-- generated:end DSH3201 -->

**Cause.** A `#` line other than `#pragma` or `#include` (`#if`, `#define`, `#endif`, `#error`…)
arrived at the parser. The 2.0 parser expects preprocessed text: `#if` and `#define` are resolved
by the DreamShader preprocessor before parsing, line count preserved, so seeing one here means the
text was parsed raw or a directive is misspelt (`#pragam`).

**Fix.** Inside a compile this cannot happen; from a tool that calls `ParseDreamShaderLang`
directly, run the preprocessor first. If the directive is a typo, correct it. Inside a `/// @custom`
body every `#` line is kept verbatim and never reaches this check.

## DSH3202

<!-- generated:begin DSH3202 -->
**Severity** error

**Message**

```
'#pragma' needs a name: material, layout, region or endregion.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:569`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:610`
<!-- generated:end DSH3202 -->

**Cause.** A `#pragma` line is not in the shape the parser reads:

- no name at all (`#pragma` alone, or `#pragma (`),
- `#pragma material` / `#pragma layout` with an argument list that is not `(Key = Value, ...)`
  (missing `)`, a bare `=`, an unterminated string, a value that is not an identifier, number or
  quoted string),
- `#pragma material` with a positional argument: material settings are keyed, only `layout` takes
  a leading positional kind (`#pragma layout(Node, ...)`).

**Fix.** Write `#pragma material(ShadingModel = Unlit, BlendMode = Additive)`,
`#pragma layout(Node, Var = UV, X = -1100, Y = -120)`, `#pragma region Title` or
`#pragma endregion`. A `//` comment at the end of the line is fine and is stripped. An unknown
pragma name (`#pragma once`) is not an error: it is kept as `EPragmaKind::Unknown` with its text
and ignored by the compiler.

## DSH3203

<!-- generated:begin DSH3203 -->
**Severity** error

**Message**

```
'#include' needs a quoted path: #include "/Game/Shared/Common.dsh".
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:528`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:693`
<!-- generated:end DSH3203 -->

**Cause.** `#include` was not followed by a `"path"` in double quotes, or `import` was not followed
by a quoted string. Unquoted paths (`#include /Game/X.dsh`), paths with the quotes on one side
only, and the HLSL system-include spelling `#include <path>` are the usual cases. Angle brackets
are not accepted: DreamShader has no system include directory, so the spelling would mean nothing
and the printer would have to invent one.

**Fix.** Quote the path: `#include "/Game/Shared/Common.dsh"` or `import "Hash.dsh";`. `import`
needs the trailing `;`; `#include` must not have one. Both spellings produce the same
`FIncludeDecl`; only `bImportSpelling` differs, so the printer can write the file back as it was.

## DSH3204

<!-- generated:begin DSH3204 -->
**Severity** error

**Message**

```
Expected a type name, found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:360`
<!-- generated:end DSH3204 -->

**Cause.** A type name was expected and something else was found: `uniform = 1;` (type missing),
`float3 = ...` after a prefix keyword that ate the only identifier, `out x` in a parameter list, or
a keyword used as a type (`struct` inside a parameter list).

**Fix.** Put the type in: `uniform float Intensity = 1;`. In 2.0 every declaration is
`[uniform|static|const|extern|export] Type Name`, there is no untyped `var`.

## DSH3205

<!-- generated:begin DSH3205 -->
**Severity** error

**Message**

```
Expected a name after 'struct', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1141`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:728`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:762`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:821`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:997`
<!-- generated:end DSH3205 -->

**Cause.** A name was expected and the token there is not an identifier. Raised for the name of a
variable or function (`float ;`), a struct (`struct {`), a field, or a parameter (`float f(float)`
— parameters must be named even when unused).

**Fix.** Add the name. A parameter you do not use still needs one; it is what `@param` and the
generated material-function input pin are called.

## DSH3206

<!-- generated:begin DSH3206 -->
**Severity** error

**Message**

```
Expected '`{' or ';' after the parameter list of '{0}', found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1071`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:862`
<!-- generated:end DSH3206 -->

**Cause.** After a function's parameter list the parser found neither `{` nor `;`. Usually a stray
token between `)` and `{` (`float f() const {`, `float f() 5`), or a missing `{`.

**Fix.** Follow the parameter list directly with the body, or with `;` if the function is
`extern`. 2.0 has no qualifiers between the parameter list and the body.

## DSH3207

<!-- generated:begin DSH3207 -->
**Severity** error

**Message**

```
'{0}' is 'extern' and binds to an existing asset, so it cannot have a body; write a prototype ending in ';'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1033`
<!-- generated:end DSH3207 -->

**Cause.** An `extern` function has a body. `extern` means "bind this name to an existing
material-function asset" (through `/// @asset` or the library lookup), so the body would have
nowhere to go.

**Fix.** Either drop `extern` and keep the body (the function is then compiled from source), or
drop the body and end the prototype with `;`. The body is skipped as a whole during recovery, so
the declarations after it still parse.

## DSH3208

<!-- generated:begin DSH3208 -->
**Severity** error

**Message**

```
'{0}' has no body. Only an 'extern' prototype may end in ';'; a function you define needs '`{...`}'.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1062`
<!-- generated:end DSH3208 -->

**Cause.** A function that is not `extern` ends in `;` instead of a body. A 2.0 source file does
not have forward declarations: helpers may be defined in any order and are resolved by name
across the file and its includes.

**Fix.** Give the function its body, or mark it `extern` if it names an existing asset. Delete the
prototype if it was a forward declaration; it is not needed.

## DSH3210

<!-- generated:begin DSH3210 -->
**Severity** error

**Message**

```
A '.dsh' header cannot export '{0}'; only a '.dss' file produces assets.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParser.cpp:74`
<!-- generated:end DSH3210 -->

**Cause.** A `.dsh` header contains an `export` function. Headers are included into materials and
functions and are never compiled on their own, so an `export` in one has no asset to become and
would be duplicated into every includer.

**Fix.** Remove `export` (the function becomes a plain helper visible to every file that includes
the header), or move it into a `.dss` file of its own. Raised once per exported declaration, from
the module loop in `LangParser.cpp`; the declaration is kept in the tree so tooling still sees it.

## DSH3211

<!-- generated:begin DSH3211 -->
**Severity** error

**Message**

```
Expected the end of the expression, found {0}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParser.cpp:465`
<!-- generated:end DSH3211 -->

**Cause.** `ParseDreamShaderLangExpression` (tests, the language service, `#pragma` values) was
given text that has something after a complete expression: `a + b c`, `x = 1;` (the `;` is not part
of an expression).

**Fix.** Pass exactly one expression. From the editor this only appears for a `#pragma` argument
that is meant to be an expression and is not.

## DSH3213

<!-- generated:begin DSH3213 -->
**Severity** error

**Message**

```
'{0}' cannot be combined with the keywords before it; a declaration is 'uniform', 'static const', 'static', 'const', 'extern' or 'export', not a mix.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1007`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1085`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:948`
<!-- generated:end DSH3213 -->

**Cause.** The declaration's prefix keywords do not go together, or go with the wrong kind of
declaration:

- a repeated or conflicting prefix: `uniform static`, `const uniform`, `export extern`,
  `uniform uniform`;
- a storage class on a function: `uniform float f()` — `uniform`, `static` and `const` describe
  variables;
- a linkage on a variable: `export float x`, `extern float3 Dir` — `extern` and `export` describe
  functions.

**Fix.** Use one of the accepted shapes: `uniform T x`, `static const T x`, `static T x`,
`const T x`, `T x` for variables; `T f()`, `export T f()`, `extern T f();` for functions. A
constant shared by several materials is `static const`; a value the material instance edits is
`uniform`.

## DSH3214

<!-- generated:begin DSH3214 -->
**Severity** error

**Message**

```
Parameter '{0}' is 'out' and cannot have a default value; only inputs are optional.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:840`
<!-- generated:end DSH3214 -->

**Cause.** An `out` parameter has a default value: `out float Alpha = 1.0`. An output is written by
the function and cannot be optional for the caller, so the default has no meaning.

**Fix.** Remove the default. If the caller should be able to leave the pin unconnected, make the
parameter `in` and return the value some other way, or give the caller a `uniform` to pass. `inout`
parameters cannot have defaults either.

## DSH3215

<!-- generated:begin DSH3215 -->
**Severity** error

**Message**

```
Expected ']' to close the array dimension, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:388`
<!-- generated:end DSH3215 -->

**Cause.** An array dimension was opened with `[` and not closed: `float Weights[4;`,
`float3 P[;`. The dimension must be a single expression between `[` and `]`.

**Fix.** Close the bracket. Sizes are expressions and may reference `static const` values;
`float x[]` (no size) is accepted by the parser and left for the compiler to reject or infer.

## DSH3216

<!-- generated:begin DSH3216 -->
**Severity** error

**Message**

```
Expected ';' after the import path, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1121`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1165`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:707`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:770`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:779`
<!-- generated:end DSH3216 -->

**Cause.** A declaration is missing its `;`: after a variable and its initializer, after
`import "x.dsh"`, after a struct field, after a struct's closing `}`, or between two declarators in
`uniform float a, b`.

**Fix.** Add the `;`. The parser tells you what it was closing (the message says "after the
declaration", "after the import path", "after the field", "after the closing '}' of the struct").
The most common form is the missing `;` after `struct X { ... }` — HLSL and C both need it.

## DSH3217

<!-- generated:begin DSH3217 -->
**Severity** error

**Message**

```
Expected '{' after the struct name, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:732`
<!-- generated:end DSH3217 -->

**Cause.** `struct Name` was not followed by `{`. Usually a forward declaration (`struct X;`),
which 2.0 does not have, or a base-class colon (`struct X : Y`), which it does not support.

**Fix.** Define the struct in place: `struct X { float a; };`. Types are resolved by name across
the file, so a struct may be declared after the function that uses it.

## DSH3218

<!-- generated:begin DSH3218 -->
**Severity** error

**Message**

```
Expected ')' to close the parameter list, found {1}.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:854`
<!-- generated:end DSH3218 -->

**Cause.** A parameter list was not closed with `)`. Either the `)` is missing, or a parameter is
separated with something other than `,` (`float a; float b`), or a parameter has a qualifier the
parser does not know (`const float a` — 2.0 parameters take only `in`, `out`, `inout`).

**Fix.** Separate parameters with commas and close the list. Drop `const` from parameters; inputs
are already read-only in the generated graph.

## DSH3220

<!-- generated:begin DSH3220 -->
**Severity** warning

**Message**

```
A '@' in a '///' line must be followed by a directive name; the text is kept as description.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:488`
<!-- generated:end DSH3220 -->

**Cause.** *(warning)* A `///` line has a `@` at a directive position (start of the text or after a
space) that is not followed by a directive name: `/// see @ the docs`, `/// @ desc Foo`,
`/// @123`. The line is kept as description text, so nothing is lost, but if a directive was
intended it was not read.

**Fix.** Write directives as `@name value` with no space after the `@`: `/// @desc Overall gain`.
An `@` inside a word (`user@host`) or followed by a space is plain text and is only warned about
when it sits where a directive would start. Directive keys are lower-cased on read, so `@Desc`
and `@desc` are the same key.

## DSH3221

<!-- generated:begin DSH3221 -->
**Severity** warning

**Message**

```
This '///' block is not followed by a field and is ignored.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:1193`, `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:753`
<!-- generated:end DSH3221 -->

**Cause.** *(warning)* A `///` block is not attached to anything: it sits at the end of the file
with no declaration after it, or at the end of a struct body with no field after it. Doc blocks
belong to the declaration that follows them, so a block with nothing following is dropped.

**Fix.** Move the block above the declaration it describes, or turn it into a `//` comment if it is
just a note. A block above a `#pragma` or `#include` line attaches to that directive and is kept,
so a file-header comment written as `///` above the first `#pragma` is not orphaned.

## DSH3222

<!-- generated:begin DSH3222 -->
**Severity** error

**Message**

```
'{0}' is a 1.x declaration; the 2.0 front end does not parse it yet. Keep it in a .dsm/.dsf/.dsh compiled by the 1.x front end.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:979`
<!-- generated:end DSH3222 -->

**Cause.** A 1.x declaration word — `Function`, `GraphFunction`, `Namespace`, `VirtualFunction`,
`Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` — was found where a 2.0 declaration
starts. The 2.0 front end does not read the 1.x block syntax; until the second front end lands
(M4) a 1.x file must keep its 1.x extension.

**Fix.** Keep 1.x sources in `.dsm` / `.dsf` / `.dsh` files compiled by the 1.x front end, or port
the declaration to 2.0: `Function float Luma(in vec3 c) { ... }` becomes
`float Luma(float3 c) { ... }`, `Shader(Name = "X") { ... }` becomes `export void X(inout material m)
{ ... }` with `#pragma material(...)` for the settings. The message names the word it saw; the
whole block is skipped during recovery so the rest of the file still reports its own errors.

