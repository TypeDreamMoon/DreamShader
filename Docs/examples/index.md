# Examples

> [DreamShader](../index.md) » **Examples**

Fourteen complete, self-contained DreamShaderLang sources, ordered from the smallest possible
material to the constructs that need a `.dsh` header, a `.dsf` function file, or a specific engine
version.

| | |
| :-- | :-- |
| Applies to | DreamShaderLang `1.5.1` |
| Engines | UE `5.3` – `5.8`; anything version-gated is marked inline |
| Assumed source root | `<Project>/DShader` — the `SourceDirectory` [project setting](../settings/project.md) |
| Generated output | in memory by default; see [In-memory materials](../generation/in-memory.md) |

Every snippet below is a whole file. Paths in the leading comment are the on-disk location the
example assumes; `import` specifiers resolve against that layout. Assets referenced with
`Path(Engine, …)` ship with the engine, so those examples load as written; the one that references
`Path(Game, …)` is marked.

Diagnostics quoted in the notes are shown with **this example's own identifiers already
substituted**; a bare `…` stands for a value the message fills in at runtime, and the leading
`<file>: ` prefix that generation-time messages carry is omitted. The verbatim catalogue is the
[diagnostics index](../diagnostics/index.md).

## Contents

| # | Example | Exercises |
| :-- | :-- | :-- |
| [1](#1-minimal-unlit-material) | Minimal unlit material | [`Shader`](../language/shader.md), [output bindings](../language/output-bindings.md) |
| [2](#2-parameters-scalar-vector-static-switch-texture) | Parameters, metadata, group scopes | [`Properties`](../language/properties.md), [metadata](../parameters/metadata.md) |
| [3](#3-a-shared-dsh-header-namespace--function) | A shared `.dsh` header | [`Namespace`](../language/namespace.md), [`import`](../language/import.md) |
| [4](#4-calling-functions-value-form-and-statement-form) | Calling functions | [Graph calls](../graph/calls.md) |
| [5](#5-ue-nodes-and-a-generic-ueexpression) | `UE.*` and `UE.Expression` | [`UE.*`](../builtins/ue.md), [`UE.Expression`](../builtins/ue-expression.md) |
| [6](#6-if--else-in-graph) | `if` / `else` | [`if`](../graph/if.md) |
| [7](#7-materialattributes-output-binding) | `MaterialAttributes` | [MaterialAttributes](../graph/material-attributes.md) |
| [8](#8-substrate-material-ue-54) | Substrate material *(UE 5.4+)* | [`Substrate.*`](../builtins/substrate.md) |
| [9](#9-shaderfunction-in-a-dsf-imported-and-called) | `ShaderFunction` in a `.dsf` | [`ShaderFunction`](../language/shader-function.md) |
| [10](#10-shaderlayer--shaderlayerblend) | `ShaderLayer` / `ShaderLayerBlend` | [Layers](../language/shader-layer.md) |
| [11](#11-virtualfunction-wrapping-an-existing-asset) | `VirtualFunction` | [`VirtualFunction`](../language/virtual-function.md) |
| [12](#12-graphfunction-hoisting-a-ue-call-into-a-custom-node) | `GraphFunction` hoisting | [`GraphFunction`](../language/graph-function.md) |
| [13](#13-settings-domain-shading-model-blend-mode-backend) | `Settings` and `Backend` | [Shader settings](../settings/material.md), [Backend](../settings/backend.md) |
| [14](#14-layout-placement-and-region) | `Layout` and `#Region` | [`Layout`](../language/layout.md) |

---

## 1. Minimal unlit material

The smallest file that produces an asset: one parameter, one binding, one assignment.

```c
// DShader/Materials/M_Minimal.dsm
Shader(Name="Materials/M_Minimal", Root="Game")
{
    Properties = {
        vec3 Tint = vec3(1.0, 0.2, 0.2);
    }

    Settings = {
        Domain       = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = Tint;
    }
}
```

Generated asset:

```text
package     /Game/Materials/M_Minimal
object path /Game/Materials/M_Minimal.M_Minimal
```

- `Shader` is matched **case-sensitively**; `Properties`, `Settings`, `Outputs`, `Graph` are not.
  See [Lexical elements](../language/lexical.md#case-sensitivity).
- The `=` after a section name is optional sugar *(since 1.5.0)*: `Properties { … }` parses
  identically. The final `;` in a section body is optional too.
- `Root` defaults to `/Game`, so `Root="Game"` above is redundant. Full rules:
  [Asset paths](../generation/asset-paths.md).
- A `Shader` needs either a `Graph` block or at least one initialized output declaration; without
  both the parse fails with `Shader must provide a Graph block.`

*See also:* [`Shader`](../language/shader.md) · [Output bindings](../language/output-bindings.md) ·
[Material enums](../settings/material-enums.md)

---

## 2. Parameters: scalar, vector, static switch, texture

Every parameter family, with a `Group("…")` scope, `[ … ]` metadata and the `Slider(min, max)`
shorthand.

```c
// DShader/Materials/M_Params.dsm
Shader(Name="Materials/M_Params")
{
    Properties = {
        Group("Surface") {
            ScalarParameter Roughness = 0.55 [Slider(0, 1)];
            VectorParameter Albedo    = float4(0.8, 0.8, 0.8, 1.0) [Description="Base albedo"];
        }

        Group("Detail") {
            TextureSampleParameter2D DetailMap = Path(Engine, "EngineResources/WhiteSquareTexture") [
                SamplerType   = "LinearColor";
                SamplerSource = "FromTextureAsset";
                SortPriority  = 99;
            ];
            StaticSwitchParameter UseDetail = true;
        }

        Texture2D   NoiseTex   = Path(Engine, "EngineResources/WhiteSquareTexture");
        const float DebugScale = 1.0;
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        float3 Color;
        float  Rough;

        Base.BaseColor = Color;
        Base.Roughness = Rough;
    }

    Graph = {
        vec2 UV     = UE.TexCoord(Index = 0);
        vec4 Detail = DetailMap(Coordinates = UV);
        vec4 Noise  = SampleTexture2D(NoiseTex, UV);

        Color = UseDetail(True = Detail.rgb * Albedo.rgb, False = Albedo.rgb);
        Rough = Roughness * DebugScale * Noise.r;
    }
}
```

- `float`/`vec3`/`Texture2D` are the **compact** spellings; `ScalarParameter`/`VectorParameter`/
  `TextureSampleParameter2D` name the Unreal node explicitly. Both are documented in
  [Compact types](../parameters/compact-types.md) and [Parameter nodes](../parameters/parameter-nodes.md).
- A `Group("…") { … }` scope stamps its name onto every parameter inside and assigns
  `SortPriority` `0, 10, 20, …` from **one counter shared by all groups in the block**. An explicit
  `SortPriority` wins and does not consume a slot — which is why `UseDetail` here gets the next
  automatic value rather than `100`. Nested groups compose with `|` (`Outer|Inner`).
- `const` declares a `Constant` node instead of a parameter. It may only be used with a plain
  scalar, vector or texture type.

> [!NOTE]
> The call form `Name(Pin = …)` only works for the ten parameter tokens that own input pins —
> `ChannelMaskParameter`, `StaticComponentMaskParameter` and the eight `*SampleParameter*` tokens.
> A **texture object** parameter (`Texture2D NoiseTex`, `TextureObjectParameter`) has no pins:
> `NoiseTex(Coordinates = UV)` fails with `Unknown Graph function 'NoiseTex'.` Sample it with the
> reserved `SampleTexture2D(textureObject, uv)` form instead, as above. `SampleTexture2D` is matched
> **case-sensitively** and takes exactly two positional arguments.

> [!WARNING]
> A bare read of a `StaticSwitchParameter` is not a value. `Color = UseDetail;` fails with
> `Unknown Graph identifier 'UseDetail'.` — the parameter must be called with `True=` and `False=`
> (or `A=`/`B=`, or positionally). Both branches must have the same component count.

*See also:* [`Properties`](../language/properties.md) · [Metadata](../parameters/metadata.md) ·
[`SamplerType`](../parameters/sampler-type.md) · [`Path(...)`](../parameters/path.md) ·
[Reading parameters in `Graph`](../parameters/graph-usage.md)

---

## 3. A shared `.dsh` header: `Namespace` + `Function`

A `.dsh` header may contain `Function`, `GraphFunction`, `Namespace` and `VirtualFunction` blocks
and `import` directives — nothing else. It generates no asset of its own; its contents are inlined
into whatever imports it.

```c
// DShader/Shared/Common.dsh
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }

    Function float Luma(in vec3 color) {
        return dot(color, float3(0.299, 0.587, 0.114));
    }
}

Function SelfContained Remap01(in float value, out float result) {
    result = saturate(value * 0.5 + 0.5);
}

Function SplitChannels(in vec4 src, out vec3 rgb, out float alpha) {
    rgb   = src.rgb;
    alpha = src.a;
}
```

Importing it:

```c
// DShader/Materials/M_Tinted.dsm
import "Shared/Common.dsh";

Shader(Name="Materials/M_Tinted")
{
    Properties = {
        vec3 Albedo = vec3(0.6, 0.8, 1.0);
        vec3 Tint   = vec3(1.0, 0.4, 0.1);
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Common::ApplyTint(Albedo, Tint, Tinted);
        Color = Tinted;
    }
}
```

- `import "Shared/Common"` is equivalent: when the specifier carries **no extension at all**, `.dsh`
  is appended. Importing a `.dsf` or `.dsm` therefore requires the explicit extension.
- The specifier is resolved against three roots, in order: the importing file's own directory, the
  source root (`DShader`), then `DShader/Packages`. A candidate that escapes its root is rejected.
- The `;` is optional, `'single quotes'` are accepted, and a trailing `//` comment is allowed — but
  an `import` must be **alone on its line**.
- `Function` bodies are HLSL, not `Graph` statements: `dot`, `saturate` and `float3(…)` here are
  HLSL intrinsics. GLSL spellings are rewritten inside those bodies (`vec3`→`float3`, `mix`→`lerp`,
  `fract`→`frac`, `mod`→`fmod`).
- `Inline` is an exact alias of `SelfContained`. Neither is accepted on a `GraphFunction`.

> [!WARNING]
> An `import` line inside a `/* … */` block comment **is still processed** — the import scanner only
> recognises the `//` prefix. Comment an import out with `//`, not with a block comment.

> [!WARNING]
> A namespace-qualified call written **inside another `Function` or `GraphFunction` body** is
> flattened to `Common_ApplyTint` and never rewritten to the generated symbol, so the emitted HLSL
> references an undefined function. Call namespaced helpers from a `Graph` block (as above), or
> duplicate the body into the calling function.

*See also:* [`import`](../language/import.md) · [`Namespace`](../language/namespace.md) ·
[`Function`](../language/function.md) · [Source files](../language/source-files.md) ·
[Packages](../tools/packages.md)

---

## 4. Calling functions: value form and statement form

A function with exactly one output may be called as a value. Two or more outputs require the
statement form, whose trailing arguments are plain variable names that receive the results.

```c
// DShader/Materials/M_Calls.dsm
import "Shared/Common.dsh";

Shader(Name="Materials/M_Calls")
{
    Properties = {
        vec4 Source = vec4(0.4, 0.6, 0.9, 0.75);
        vec3 Tint   = vec3(1.0, 0.4, 0.1);
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Translucent"; }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph = {
        // statement form: two out results, two target names
        SplitChannels(Source, Rgb, A);

        // statement form: one out result
        Common::ApplyTint(Rgb, Tint, Tinted);

        // value form: single-output functions
        float L    = Common::Luma(Tinted);
        float Soft = Remap01(L);

        Color = Tinted * Soft;
        Alpha = A;
    }
}
```

| Callee kind | Value form | Statement form | Named arguments |
| :-- | :-- | :-- | :-- |
| `Function` | 1 result only | yes | **no** |
| `GraphFunction` | 1 result only | yes | **no** |
| `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` | yes | yes | value form only |
| `VirtualFunction` | yes | yes | value form only |

- Out targets need not be declared first — the call creates them. They must be bare identifiers,
  and two results may not be written into the same name in one call.
- Argument count is exact: `Inputs.Num()` for the value form, `Inputs.Num() + Results.Num()` for the
  statement form.
- Function names are matched **case-insensitively** and there is **no overload resolution**. If the
  same name resolves to more than one declaration kind the call fails with
  `Graph call '…' is ambiguous because multiple definitions use that name: ….`
- The math builtins (`lerp`, `mix`, `dot`, `pow`, `min`, `max`, `clamp`, `saturate`, `sin`, `cos`,
  `abs`, `floor`, `ceil`, `frac`, `fract`, `sqrt`, `normalize`, `fmod`, `mod`) are matched **before**
  user functions and can never be shadowed.

*See also:* [Graph calls](../graph/calls.md) · [Name resolution](../graph/name-resolution.md) ·
[Math builtins](../builtins/math.md) · [Statements](../graph/statements.md)

---

## 5. `UE.*` nodes and a generic `UE.Expression(...)`

`UE.<Name>(…)` creates a `UMaterialExpression`. Twenty-seven names are registered with fixed
argument sets; anything else falls through to the generic reflected path, where `Class=` names the
expression class and `OutputType=` declares the value shape.

```c
// DShader/Materials/M_UEBuiltins.dsm
Shader(Name="Materials/M_UEBuiltins")
{
    Settings = {
        Domain       = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float2 uv    = UE.TexCoord(Index = 0);
        float  t     = UE.Time();
        float3 view  = UE.CameraVectorWS();

        // Generic form: any MaterialExpression class, by name.
        float pulse  = UE.Expression(Class="Sine", OutputType="float1", Input=t);

        // Class= may be omitted when the function name IS the class name.
        float pulse2 = UE.Sine(OutputType="float1", Input=t * 0.5);

        // Reflected literal properties are written by name.
        float3 local = UE.TransformVector(Input = view, Source = "World", Destination = "Local");

        Color = float3(uv.x, uv.y, pulse + pulse2 + local.z);
    }
}
```

- `Class="Sine"`, `"MaterialExpressionSine"`, `"UMaterialExpressionSine"` and
  `"/Script/Engine.MaterialExpressionSine"` all resolve to the same class. Quotes are optional:
  `Class=Sine` and `OutputType=float1` work identically.
- An argument is matched first against the node's **input pin** names, then against its reflected
  `UPROPERTY` names; the pin wins on a tie. Input arguments are expressions, property arguments must
  be literals.
- `UE.Expression(…)` with no `Class=` fails with `UE.Expression requires Class="MaterialExpressionName".`
  `UE.Expression()` with neither argument reports the **`OutputType`** error first.
- `OutputType` accepts `float`/`float1`/`float2..4` (and the `half*`, `vec*`, `int*`, `uint*`,
  `bool*`, `ivec*`, `uvec*`, `bvec*` spellings), `StaticBool`, `MaterialAttributes`, `Texture2D`,
  `TextureCube`, `Texture2DArray`, `Texture3D`, `VolumeTexture`, `SamplerState` and `Substrate`.
  `ResultType` is an exact alias.

> [!WARNING]
> Registered `UE.*` builtins do **not** validate their argument list. `UE.Time(Bogus = 1)` and
> `UE.TexCoord(0)` (positional) are accepted and the unrecognised argument is silently dropped.
> Only the generic and `Substrate.*` paths reject positional arguments, with
> `Generic UE.Sine calls require named arguments.`

*See also:* [`UE.*` catalogue](../builtins/ue.md) · [`UE.Expression`](../builtins/ue-expression.md) ·
[`OutputType`](../builtins/output-type.md) · [Transform bases](../builtins/transform.md)

---

## 6. `if` / `else` in `Graph`

The condition must be parenthesised and each body must be braced. Both branches are built
unconditionally into the graph; a `UMaterialExpressionIf` selects between them at runtime.

```c
// DShader/Materials/M_Branch.dsm
Shader(Name="Materials/M_Branch")
{
    Properties = {
        ScalarParameter Threshold = 0.5  [Group="Surface"];
        VectorParameter Lit       = float4(1.0, 0.85, 0.2, 1.0) [Group="Surface"];
        VectorParameter Dark      = float4(0.05, 0.05, 0.1, 1.0) [Group="Surface"];
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "Unlit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float2 uv   = UE.TexCoord(Index = 0);
        float  mask = uv.x;

        if (mask > Threshold) {
            Color = Lit.rgb;
        } else if (mask > Threshold * 0.5) {
            Color = Lit.rgb * 0.5;
        } else {
            Color = Dark.rgb;
        }
    }
}
```

| Condition | Meaning |
| :-- | :-- |
| `a > b`, `a < b`, `a >= b`, `a <= b`, `a == b`, `a != b` | the six comparison operators |
| `if (x)` | truthy — identical to `x != 0` |

- Both sides of the comparison must evaluate to a **scalar**.
- A variable written in one branch must also be written in the other, or the merge fails with
  `Graph if statement could not resolve both branch values for '…'.` This includes variables
  *declared* only inside one branch.
- Branch values must agree in shape, otherwise
  `Graph if branches assign variable '…' with inconsistent types`.
- Reading a parameter inside a branch is fine — parameters are never branch outputs.
- `if` is matched **case-sensitively**; `If (x) { … }` is not a conditional.

> [!WARNING]
> `&&` and `||` do not exist and are **silently dropped**. `if (a > 0 && b > 0)` compiles as
> `if (a > 0)` with no diagnostic. Nest two `if` statements instead. The same truncation applies to
> `%`, `?:`, `&`, `|`, `^`, `<<` and `v[i]` in ordinary expressions — see
> [Unsupported constructs](../graph/unsupported.md).

*See also:* [`if` / `else`](../graph/if.md) · [Expressions](../graph/expressions.md) ·
[Conversions](../graph/conversions.md)

---

## 7. `MaterialAttributes` output binding

Binding `Base.MaterialAttributes` auto-enables *Use Material Attributes* on the generated material.
A `MaterialAttributes` variable is a `MakeMaterialAttributes` node; its members are written by name.

```c
// DShader/Materials/M_Attrs.dsm
Shader(Name="Materials/M_Attrs")
{
    Properties = {
        vec3  BaseTint = vec3(0.6, 0.8, 1.0);
        float R        = 0.35;
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        Base.MaterialAttributes = Attrs;
    }

    Graph = {
        MaterialAttributes Attrs;

        Attrs.BaseColor = BaseTint;
        Attrs.Roughness = R;
        Attrs.Metallic  = 0.0;

        // Members can be read back through a BreakMaterialAttributes node.
        float Echo = Attrs.Roughness;
        Attrs.Specular = Echo;
    }
}
```

> [!NOTE]
> `Attrs` must be a graph value before the binding is evaluated. Declare it **inside `Graph`**, as
> above, or give the `Outputs` declaration an initializer. `MaterialAttributes` is a valid type
> token in `Outputs`, `Inputs` and function signatures, but **not** in `Properties`.

- Member names are the material property names (`BaseColor`, `Metallic`, `Specular`, `Roughness`,
  `EmissiveColor`, `Opacity`, `Normal`, …) — the same catalogue as the `Base.<X>` binding targets.
- Arithmetic operators reject `MaterialAttributes` operands:
  `Arithmetic operators cannot be applied to MaterialAttributes values.`
- `Base.MaterialAttributes` and `Base.FrontMaterial` cannot both be used by one `Shader`.

*See also:* [MaterialAttributes](../graph/material-attributes.md) ·
[Output bindings](../language/output-bindings.md) · [Types](../language/types.md)

---

## 8. Substrate material *(UE 5.4+)*

`Substrate.*` builds Substrate BSDF nodes; `Base.FrontMaterial` is the binding that consumes them.

```c
// DShader/Materials/M_Substrate.dsm
Shader(Name="Materials/M_Substrate")
{
    Properties = {
        vec3 Color = vec3(0.1, 0.6, 1.0);
    }

    Outputs = {
        Substrate Surface;
        Base.FrontMaterial = Surface;
    }

    Graph = {
        Surface = Substrate.Unlit(EmissiveColor = Color);
    }
}
```

- The `Substrate` type token, the `Substrate.*` namespace and `Base.FrontMaterial` all require
  **UE 5.4 or newer**. Below that the compile fails with
  `Substrate builtin call '…' requires Unreal Engine 5.4 or newer.`
- `Base.FrontMaterial` force-sets the shading model to Substrate. An explicit
  `ShadingModel` setting must therefore be absent or `"Substrate"`; anything else fails with
  `Base.FrontMaterial requires ShadingModel="Substrate" or no explicit ShadingModel setting.`
- Every `Substrate.*` argument must be **named** — the registered `UE.*` sugar does not apply — and
  `Class=` is rejected, because each name maps to a fixed expression class.
- `Substrate.*` calls are **not** hoisted out of a `GraphFunction` body; only `UE.*` is.
- Substrate values cannot be swizzled, cannot take arithmetic operators, and cannot be selected by
  an `if` statement.

*See also:* [`Substrate.*`](../builtins/substrate.md) ·
[Output bindings](../language/output-bindings.md) · [Types](../language/types.md)

---

## 9. `ShaderFunction` in a `.dsf`, imported and called

A `.dsf` may declare `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `Function`,
`GraphFunction`, `Namespace` and `VirtualFunction` blocks — everything except a top-level `Shader`.

```c
// DShader/Functions/F_Tint.dsf
ShaderFunction(Name="Functions/F_Tint")
{
    Inputs = {
        vec3 InColor;
        opt float Strength = 1.0 [
            Description  = "Preview strength";
            SortPriority = 10;
        ];
    }

    Outputs = {
        vec3 OutColor [Description="Tinted colour"];
    }

    Settings = {
        Description     = "Tint helper";
        ExposeToLibrary = true;
    }

    Graph = {
        OutColor = InColor * Strength;
    }
}
```

Calling it from a material:

```c
// DShader/Materials/M_UsesTint.dsm
import "Functions/F_Tint.dsf";

Shader(Name="Materials/M_UsesTint")
{
    Properties = {
        vec3 Albedo = vec3(0.6, 0.8, 1.0);
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        // named form; the opt input may be omitted or passed `default`
        Color = F_Tint(InColor = Albedo, Strength = 0.5);
    }
}
```

- The import needs the explicit `.dsf` extension.
- A `ShaderFunction` is looked up by its **full `Name`** (`Functions/F_Tint`) or by the last
  `/`-separated segment (`F_Tint`), case-insensitively.
- Arguments are either **all positional or all named**; mixing fails with
  `ShaderFunction 'F_Tint' input arguments cannot mix positional and named forms.`
- `opt` is what makes an input optional on the generated `UMaterialFunction`; its default value
  drives the input's Preview pin. A non-`opt` input that is omitted fails with
  `ShaderFunction 'F_Tint' is missing required input 'InColor'.`
- Compiling the `.dsm` also generates the imported `ShaderFunction` asset — material functions are
  written before the material that calls them.
- The four `Settings` keys a material function honours are `Description`, `UserExposedCaption`,
  `ExposeToLibrary` and `LibraryCategories`. Other keys are silently ignored here (unlike a
  `Shader`, where an unknown key is a hard error).

*See also:* [`ShaderFunction`](../language/shader-function.md) ·
[`Inputs` / `Outputs`](../language/inputs-outputs.md) ·
[Function settings](../settings/function.md) · [Graph calls](../graph/calls.md)

---

## 10. `ShaderLayer` / `ShaderLayerBlend`

These generate native `UMaterialFunctionMaterialLayer` and `UMaterialFunctionMaterialLayerBlend`
assets *(since 1.3.0)*, and their interfaces are fixed by arity rules.

```c
// DShader/Layers/L_SimpleSurface.dsf
ShaderLayer(Name="Layers/L_SimpleSurface")
{
    Properties = {
        VectorParameter LayerColor = float4(0.8, 0.2, 0.1, 1.0) [Group="Layer"];
        ScalarParameter LayerRough = 0.5                        [Group="Layer"; Slider(0, 1)];
    }

    Outputs = {
        MaterialAttributes Attrs;
    }

    Graph = {
        Attrs.BaseColor = LayerColor.rgb;
        Attrs.Roughness = LayerRough;
    }
}

ShaderLayerBlend(Name="Layers/LB_Overlay")
{
    Properties = {
        ScalarParameter Alpha = 0.5 [Group="Blend"; Slider(0, 1)];
    }

    Inputs = {
        MaterialAttributes Bottom;
        MaterialAttributes Top;
    }

    Outputs = {
        MaterialAttributes Attrs;
    }

    Graph = {
        Attrs.BaseColor = lerp(Bottom.BaseColor, Top.BaseColor, Alpha);
        Attrs.Roughness = lerp(Bottom.Roughness, Top.Roughness, Alpha);
    }
}
```

| Block | Inputs | Output |
| :-- | :-- | :-- |
| `ShaderLayer` | **at most one**, and it must be `MaterialAttributes` | exactly one `MaterialAttributes` |
| `ShaderLayerBlend` | **exactly two**, both `MaterialAttributes` | exactly one `MaterialAttributes` |

- Layer *controls* go in `Properties`, never in `Inputs` — that is what the two diagnostics
  (`… Use Properties for layer controls.` / `… Use Properties for blend controls.`) are telling you.
- On **UE 5.7+** a blend input named `Top` / `TopLayer` or `Bottom` / `BottomLayer` / `Base` /
  `BaseLayer` is tagged with the matching `BlendInputRelevance`. On earlier engines the names are
  ordinary and only the order matters.
- `MaterialLayer(...)` / `MaterialLayerBlend(...)` still parse as compatibility aliases but emit
  `MaterialLayer is deprecated; use ShaderLayer instead.` *(deprecated in 1.3.0)*.

*See also:* [`ShaderLayer` / `ShaderLayerBlend`](../language/shader-layer.md) ·
[MaterialAttributes](../graph/material-attributes.md) · [Source files](../language/source-files.md)

---

## 11. `VirtualFunction`: wrapping an existing asset

A `VirtualFunction` generates nothing. It declares the interface of a `UMaterialFunction` that
already exists so that `Graph` blocks can call it with type checking.

```c
// DShader/VirtualFunctions/BufferWriter.dsh
VirtualFunction(Name="BufferWriter")
{
    Options = {
        Asset       = Path(Game, "MaterialFunctions/F_BufferWriter");
        Description = "Existing material function declared for Graph calls.";
    }

    Inputs = {
        float3 Color;
        opt float Alpha = 1.0;
    }

    Outputs = {
        float3 Result;
    }
}
```

```c
// DShader/Materials/M_Buffered.dsm
import "VirtualFunctions/BufferWriter.dsh";

Shader(Name="Materials/M_Buffered")
{
    Properties = { vec3 Tint = vec3(1.0, 0.4, 0.1); }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = BufferWriter(Color = Tint, Alpha = 0.5);
    }
}
```

> [!NOTE]
> This example references `/Game/MaterialFunctions/F_BufferWriter`, a project asset. Substitute a
> path that exists in your project, or let the editor write the declaration for you: the
> *DreamShader ▸ Create Virtual Function* action on a `UMaterialFunction` emits a matching `.dsh`
> under `DShader/VirtualFunctions`.

- The asset may come from `Options = { Asset = … }` or from the header attribute
  `VirtualFunction(Name="…", Asset="…")`. Without one, the parse fails with
  `VirtualFunction 'BufferWriter' must provide Options = { Asset = Path(...); }.`
- `Settings` is an accepted alias for `Options`, and `Properties` is an accepted alias for `Inputs`
  inside this block only. `Graph` and `Code` sections are rejected outright.
- At least one output is required.
- Declared input/output names are matched against the asset's pin names case-insensitively, with an
  ordinal fallback. A name that resolves to neither fails with
  `VirtualFunction 'BufferWriter' output 'Result' does not exist on MaterialFunction asset '…'.`
- `Path` roots: `Game`, `Engine`, `Plugin.<Name>` / `Plugins.<Name>`, a full object path, or a bare
  quoted `"/Game/…"`.

*See also:* [`VirtualFunction`](../language/virtual-function.md) ·
[`Options`](../language/options.md) · [`Path(...)`](../parameters/path.md) ·
[VirtualFunction tools](../tools/virtual-function-tools.md)

---

## 12. `GraphFunction`: hoisting a `UE.*` call into a Custom node

A `GraphFunction` body is HLSL like a `Function`, but every `UE.*` call inside it is evaluated as a
real material node and wired into the generated Custom node as an extra input pin.

```c
// DShader/Shared/Wind.dsh
GraphFunction WindPulse(in float2 uv, out float pulse) {
    float t = UE.Time();
    pulse = sin(uv.x * 8.0 + t);
}
```

```c
// DShader/Materials/M_Wind.dsm
import "Shared/Wind.dsh";

Shader(Name="Materials/M_Wind")
{
    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        vec2  UV    = UE.TexCoord(Index = 0);
        float Pulse = WindPulse(UV);          // value form: one out result
        Color = vec3(Pulse, Pulse, Pulse);
    }
}
```

Generated Custom-node code, in outline:

```hlsl
float pulse = (float)0;
float t = __ds_WindPulse_UE0;
pulse = sin(uv.x * 8.0 + t);
return pulse;
```

- The generated pin is named `__ds_<Function>_UE<N>`, uniquified against the declared parameter
  names.
- Only the `UE.` prefix is scanned. `Substrate.*` calls, math builtins and user function calls in a
  `GraphFunction` body are left as plain HLSL text.
- A hoisted value may not be a texture object, `MaterialAttributes` or `Substrate` — those cannot
  cross a Custom-node input pin.
- `GraphFunction` accepts no `SelfContained`/`Inline` modifier, no named call arguments, and no
  recursion (`GraphFunction cycle detected: …`). An empty body is an error.
- The statement form works too: `WindPulse(UV, Pulse);` creates `Pulse`.

*See also:* [`GraphFunction`](../language/graph-function.md) · [`Function`](../language/function.md) ·
[Generated HLSL](../generation/generated-hlsl.md) · [`UE.*` catalogue](../builtins/ue.md)

---

## 13. Settings: domain, shading model, blend mode, backend

```c
// DShader/Materials/M_Glass.dsm
Shader(Name="Materials/M_Glass")
{
    Properties = {
        vec3  Tint    = vec3(0.7, 0.9, 1.0);
        float Opacity = 0.35;
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Translucent";
        TwoSided     = true;
        Wireframe    = false;
        Backend      = "Graph";
    }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.BaseColor = Color;
        Base.Opacity   = Alpha;
    }

    Graph = {
        Color = Tint;
        Alpha = Opacity;
    }
}
```

| Key | Aliases | Values |
| :-- | :-- | :-- |
| `MaterialDomain` | `Domain` | `Surface`, `DeferredDecal` / `Decal`, `LightFunction`, `Volume`, `PostProcess`, `UI` / `UserInterface`, `RuntimeVirtualTexture` / `VirtualTexture` |
| `ShadingModel` | — | `Unlit`, `DefaultLit` / `Lit`, `Subsurface`, `PreintegratedSkin`, `ClearCoat`, `SubsurfaceProfile`, `TwoSidedFoliage`, `Hair`, `Cloth`, `Eye`, `SingleLayerWater`, `ThinTranslucent`, plus `Substrate` / `Strata` on UE 5.4+ |
| `BlendMode` | `RenderType` | `Opaque`, `Masked` / `Cutout`, `Translucent` / `Transparent`, `Additive`, `Modulate`, `AlphaComposite` / `PremultipliedAlpha` / `Premultiplied`, `AlphaHoldout`, `TranslucentColoredTransmittance` |
| `Backend` | — | `Graph`, `ThinCustom`, `Instance` *(deprecated alias for `ThinCustom`)* |

- Enum values are matched with spaces, `_` and `-` stripped, case-insensitively: `"Default Lit"`,
  `"DefaultLit"`, `"default_lit"` and `"DEFAULT-LIT"` are one alias. Quotes are optional on every
  setting value.
- These four keys plus `RenderType` and `Domain` are the only hand-handled ones. **Every other key
  is reflected straight onto `UMaterial`** — `TwoSided` and `Wireframe` above are real
  `UMaterial` properties. An unrecognised key in a `Shader` `Settings` block is a hard error.
- Duplicate keys silently overwrite, last one wins. Repeated `Settings` sections merge.
- Omitting `Backend` falls back to the project's **Default Compiler Backend** (`ThinCustom`).

> [!WARNING]
> `Backend = "";` resolves to **`Graph`**, not to the project default. Only *omitting* the key falls
> back to the project setting.

- A modified engine adds its own shading models automatically — the reflected table is read from
  `EMaterialShadingModel` at runtime, and the project's mapping tables can add or shadow spellings.

*See also:* [`Settings`](../settings/index.md) · [Shader settings](../settings/material.md) ·
[Material enums](../settings/material-enums.md) · [Backend](../settings/backend.md) ·
[Project settings](../settings/project.md)

---

## 14. Layout placement and `#Region`

`Layout` pins generated nodes to fixed positions and draws comment boxes. `#Region` / `#EndRegion`
group statements inside a `Graph` block; the region names become comment boxes in the generated
graph.

```c
// DShader/Materials/M_Laid.dsm
Shader(Name="Materials/M_Laid")
{
    Properties = {
        VectorParameter BaseColor = float4(0.8, 0.8, 0.8, 1.0) [Group="Surface"; SortPriority=10];
        ScalarParameter Roughness = 0.55                       [Group="Surface"; SortPriority=20];
        Texture2D       NoiseTex  = Path(Engine, "EngineResources/WhiteSquareTexture");
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        float3 Color;
        float  Rough;

        Base.BaseColor = Color;
        Base.Roughness = Rough;
    }

    Graph = {
        #Region "Sampling"
        vec2 UV    = UE.TexCoord(Index = 0);
        vec4 Noise = SampleTexture2D(NoiseTex, UV);
        #EndRegion

        #Region "Surface"
        Color = BaseColor.rgb * Noise.rgb;
        Rough = Roughness;
        #EndRegion
    }

    Layout = {
        Comment(Name="Sampling", X=-1200, Y=-200, W=900, H=400, Color=float4(0.10, 0.16, 0.22, 0.35));
        Comment(Name="Surface",  X=-1200, Y=260,  W=900, H=400);
        Node(Var="UV",    X=-1100, Y=-120);
        Node(Var="Noise", X=-760,  Y=-120);
    }
}
```

| Call | Required arguments | Optional |
| :-- | :-- | :-- |
| `Node` | `Var` (text), `X`, `Y` (integers) | — |
| `Comment` | `Name` (text), `X`, `Y`, `W`, `H` (integers) | `Color` (a `float4` literal) |

- `Var` names a `Graph` variable. Nothing else is a valid `Layout` statement:
  `Unknown Layout statement '…'.`
- `Comment` defaults, when the section is absent, are `W=420`, `H=240`,
  `Color = (0.10, 0.16, 0.22, 0.35)`.
- A second `Layout` section **resets** the first rather than appending — unlike `Properties`,
  `Inputs` and `Outputs`, which append.
- `#Region` names may be quoted or bare; the directives are matched case-insensitively and are
  replaced by equal-length runs of spaces, so diagnostic line **and** column numbers are unaffected.
  Regions nest.
- Region directives are only processed inside a `Graph` block — never inside a `Function` or
  `GraphFunction` body.
- The decompiler re-emits both `Layout` and `#Region` when the *Export Decompiled Layout* project
  setting is on.

> [!WARNING]
> Regeneration clears the target graph. Node positions not pinned by `Layout`, hand-added nodes,
> node property tweaks and comment boxes whose text begins with `DreamShader: ` are destroyed. Only
> comment boxes without that prefix survive.

*See also:* [`Layout`](../language/layout.md) · [Graph layout](../generation/graph-layout.md) ·
[Regeneration](../generation/regeneration.md) · [Decompiler](../tools/decompiler.md)

---

## Running the examples

1. Save the files under `<Project>/DShader` (or wherever `SourceDirectory` points).
2. With *Auto Compile On Save* enabled — the default — the editor recompiles on save after a 0.25 s
   debounce and generates the material **in memory**. Nothing is written to `.uasset` until a cook,
   an explicit *Materialize* action, or the commandlet.
3. Headless compile of one file:

```powershell
& "<Engine>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" `
  "<Project>/MyProject.uproject" `
  -run=DreamShader compile -Source="<Project>/DShader/Materials/M_Minimal.dsm" -Force `
  -unattended -nopause -nosplash -stdout -log
```

Substitute `-All` for `-Source=` to compile every `.dsf` and `.dsm` in the project. Unlike the
editor, the commandlet writes **persistent** assets.

*See also:* [Getting started](../getting-started.md) ·
[In-memory materials](../generation/in-memory.md) · [Commandlet](../tools/commandlet.md) ·
[Editor integration](../tools/editor-integration.md)

## See also

- [Getting started](../getting-started.md) — installation and the first compile
- [DreamShaderLang](../language/index.md) — the declaration grammar, block by block
- [Graph language](../graph/index.md) — the statement and expression language inside `Graph`
- [Builtins](../builtins/index.md) — `UE.*`, `Substrate.*`, math, and the HLSL library
- [Parameters](../parameters/index.md) — every parameter token, metadata key and `Path` root
- [Settings](../settings/index.md) — per-file and project-wide configuration
- [Generation](../generation/index.md) — how a source file becomes an asset
- [Tools](../tools/index.md) — editor integration, decompiler, commandlet, bridge
- [Diagnostics index](../diagnostics/index.md) — every message, by pipeline stage
- [Testing](../contributing/testing.md) — the `Tests/Corpus` fixtures these examples are adapted from
