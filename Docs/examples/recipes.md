---
title: Recipes
description: Complete DreamShaderLang materials — animated tint, panning UVs, branching, static-switch variants, translucent UI, PBR, MaterialAttributes, Substrate, parameter collections, a multi-output helper, a Settings tour and Runtime Virtual Texture, each as a .dsm and as a .dss.
---

Each recipe below is a finished `.dsm` that compiles on its own, followed by the same material written as a `.dss`.
They are written to be read top to bottom: parameters, settings, outputs, graph. Where a construct has a sharp edge,
it is called out next to the code rather than left for you to find.

| | |
| :-- | :-- |
| Applies to | DreamShaderLang 1.x (`.dsm`), built by the 2.0 compiler through its legacy front end (since 2.0.0); the `.dss` forms are [DreamShaderLang 2.0](../language-v2/index.md) |
| Engines | UE 5.3 – 5.8; the Substrate recipe needs (since UE 5.4) |
| Assumed source root | `<Project>/DShader` |
| Generated output | a `Graph`-backend material is saved on every build; a `ThinCustom` (default) material stays in memory — see [In-memory Materials](../generation/ephemeral.md) |

A `.dss` and the `.dsm` of the same name build the same asset path; keep one of the two. Diagnostics are cited by code;
how to read one is on [Diagnostics](../diagnostics/index.md). For the language constructs these are assembled from, see
[Patterns](patterns.md).

## Animated tint

`UE.Time()` gives a scalar clock; `sin` is one of the [math builtins](../builtins/math.md), so it is called bare
with positional arguments.

```c
// DShader/Materials/M_Pulse.dsm
Shader(Name="Materials/M_Pulse")
{
    Properties = {
        vec3            Tint  = vec3(1.0, 0.4, 0.1);
        ScalarParameter Speed = 2.0 [Slider(0, 10)];
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
        float t     = UE.Time();
        float pulse = sin(t * Speed) * 0.5 + 0.5;
        Color = Tint * pulse;
    }
}
```

- `sin` builds a `Sine` node with its default `Period` of 1, so it takes **cycles**, not radians: `Speed = 2.0` is two
  pulses a second. Constants fold in cycles too (since 2.1.2). For another period, call the node:
  `UE.Sine(Input = x, Period = 2.0)`.
- Two identical calls are one node, math builtin or `UE.*` alike (since 2.0.0): writing `UE.Time()` twice makes
  one `Time` node.

### Written as a `.dss`

```hlsl
// DShader/Materials/M_Pulse.dss
#pragma material(Domain = UI, ShadingModel = Unlit)

uniform float3 Tint = float3(1.0, 0.4, 0.1);
/// @slider 0 10
uniform float Speed = 2.0;

export void M_Pulse(inout material m)
{
    float t = UE.Time();
    float pulse = sin(t * Speed) * 0.5 + 0.5;
    m.EmissiveColor = Tint * pulse;
}
```

## Panning UVs

```c
// DShader/Materials/M_Panned.dsm
Shader(Name="Materials/M_Panned")
{
    Properties = {
        TextureSampleParameter2D BaseTex  = Path(Engine, "EngineResources/WhiteSquareTexture");
        ScalarParameter          PanSpeed = 0.1 [Slider(-1, 1)];
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
        vec2 uv    = UE.TexCoord(Index = 0);
        vec2 moved = UE.Panner(Coordinate = uv, Time = UE.Time(), Speed = vec2(PanSpeed, 0.0));
        vec4 texel = BaseTex(Coordinates = moved);

        Color = texel.rgb;
    }
}
```

**Note:**

`UE.Panner` takes two kinds of argument. `Coordinate`, `Time` and `Speed` are **input pins** and accept expressions —
which is why `PanSpeed` can drive the pan above. `SpeedX`, `SpeedY` and `FractionalPart` are **literal properties** of
the node and take constants only: `SpeedX = PanSpeed` is `DSH4373`. A misspelt argument name is dropped with the warning
`DSH5254` in a 1.x source, and is the error `DSH5213` in a `.dss`. See [UE.Panner](../builtins/ue.md#uepanner).

### Written as a `.dss`

```hlsl
// DShader/Materials/M_Panned.dss
#pragma material(Domain = UI, ShadingModel = Unlit)

/// @default /Engine/EngineResources/WhiteSquareTexture
uniform Texture2D BaseTex;
/// @slider -1 1
uniform float PanSpeed = 0.1;

export void M_Panned(inout material m)
{
    float2 uv = UE.TexCoord(Index = 0);
    float2 moved = UE.Panner(Coordinate = uv, Time = UE.Time(), Speed = float2(PanSpeed, 0.0));
    m.EmissiveColor = BaseTex.Sample(moved).rgb;
}
```

`.Sample` builds a `TextureObjectParameter` and a `TextureSample`, where the 1.x token built one
`TextureSampleParameter2D`; the material sees the same result.

## Branching on a threshold

Both branches are built into the graph, and one conditional node per changed variable — a `UMaterialExpressionIf`, or
a `StaticSwitch` on a static bool — selects between them at run time. The condition must be parenthesised and each
branch must be braced.

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
| `if (x)` | truthy — identical to `x != 0`, so **negative** values take the then-branch |

- The condition compares two numbers: a comparison of two vectors is `DSH4260`, an operand that is not a number
  `DSH4226`.
- A variable declared before the `if` and changed in one branch only is merged with the value it had before the `if`.
  A variable that had no value before it and is assigned in one branch only is `DSH4372` where it is read after the `if`.
- A variable *declared* inside a branch is local to that branch (since 2.0.0): it is not merged, and its name is
  unknown after the `if`.
- A value is converted to its variable's declared type at every assignment, so the two branch values of one variable
  always agree.
- Reading a parameter inside a branch is fine — parameters are never branch outputs.
- `if` is **case-sensitive**: `If (x) { … }` is a call to a function named `If`, and the `{` after it is `DSH2154`.

**Warning:**

`&&` and `||` are refused in a 1.x condition (since 2.0.0): `DSH2210`. 1.x dropped everything after the first
comparison without a word, so `if (a > 0 && b > 0)` tested `a > 0`. Nest two `if` statements instead. The other
constructs 1.x truncated — `%`, `?:`, `&`, `|`, `^`, `<<`, `v[i]` — are errors anywhere in an expression
(`DSH2200`–`DSH2202`); see [Unsupported constructs](../graph/unsupported.md#operators-1x-truncated). A `.dss` has all
of these operators.

### Written as a `.dss`

```hlsl
// DShader/Materials/M_Branch.dss
#pragma material(Domain = Surface, ShadingModel = Unlit, BlendMode = Opaque)

/// @group Surface
uniform float Threshold = 0.5;
/// @group Surface
uniform float4 Lit = float4(1.0, 0.85, 0.2, 1.0);
/// @group Surface
uniform float4 Dark = float4(0.05, 0.05, 0.1, 1.0);

export void M_Branch(inout material m)
{
    float2 uv = UE.TexCoord(Index = 0);
    float mask = uv.x;

    float3 Color = float3(0, 0, 0);
    if (mask > Threshold)
    {
        Color = Lit.rgb;
    }
    else if (mask > Threshold * 0.5)
    {
        Color = Lit.rgb * 0.5;
    }
    else
    {
        Color = Dark.rgb;
    }
    m.EmissiveColor = Color;
}
```

The variable the branches assign is declared before the `if`, as a 1.x `Outputs` declaration is.

## A static-switch variant

A `StaticSwitchParameter` is not a run-time branch — it produces separate permutations. In 1.x, unlike `if`, it is
written as a **call**.

```c
// DShader/Materials/M_Variant.dsm
Shader(Name="Materials/M_Variant")
{
    Properties = {
        Group("Surface") {
            VectorParameter BaseColor   = float4(0.1, 0.2, 0.3, 1.0);
            VectorParameter DetailColor = float4(1.0, 0.8, 0.3, 1.0);
        }

        Group("Switches") {
            StaticSwitchParameter UseDetail = true [Description="Use the detail colour"];
        }
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        float3 Color;
        Base.BaseColor = Color;
    }

    Graph = {
        Color = UseDetail(True = DetailColor.rgb, False = BaseColor.rgb);
    }
}
```

`True=` / `False=`, `A=` / `B=` and the positional form all work; a call without both sides is `DSH5258`. What does
**not** work is a bare `Color = UseDetail;` or `if (UseDetail) { … }` — both are `DSH4200`, because a 1.x switch
parameter is not a value. See [StaticSwitchParameter](../graph/calls.md#staticswitchparameter).

### Written as a `.dss`

```hlsl
// DShader/Materials/M_Variant.dss
#pragma material(Domain = Surface, ShadingModel = DefaultLit, BlendMode = Opaque)

/// @group Surface
uniform float4 BaseColor = float4(0.1, 0.2, 0.3, 1.0);
/// @group Surface
uniform float4 DetailColor = float4(1.0, 0.8, 0.3, 1.0);

/// @static
/// @group Switches   @desc Use the detail colour
uniform bool UseDetail = true;

export void M_Variant(inout material m)
{
    float3 Color = BaseColor.rgb;
    if (UseDetail)
    {
        Color = DetailColor.rgb;
    }
    m.BaseColor = Color;
}
```

In a `.dss` a `/// @static` `uniform bool` is a value: read by an `if`, its merge is a `StaticSwitch`, and the parameter
is a real static switch parameter.

## A translucent UI material

```c
// DShader/Materials/M_UI_Glass.dsm
Shader(Name="Materials/M_UI_Glass")
{
    Properties = {
        vec3            Tint    = vec3(1.0, 1.0, 1.0);
        ScalarParameter Opacity = 0.75 [Slider(0, 1)];
    }

    Settings = {
        Domain       = "UI";
        ShadingModel = "Unlit";
        BlendMode    = "Translucent";
    }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph = {
        Color = Tint;
        Alpha = Opacity;
    }
}
```

`Domain = "UI"` plus `ShadingModel = "Unlit"` is the combination UMG wants; `BlendMode` is what makes `Base.Opacity`
mean anything. Every accepted spelling is on [Enum values](../settings/material-enums.md).

### Written as a `.dss`

```hlsl
// DShader/Materials/M_UI_Glass.dss
#pragma material(Domain = UI, ShadingModel = Unlit, BlendMode = Translucent)

uniform float3 Tint = float3(1.0, 1.0, 1.0);
/// @slider 0 1
uniform float Opacity = 0.75;

export void M_UI_Glass(inout material m)
{
    m.EmissiveColor = Tint;
    m.Opacity = Opacity;
}
```

## A Surface PBR skeleton

```c
// DShader/Materials/M_Surface.dsm
Shader(Name="Materials/M_Surface")
{
    Properties = {
        Group("Surface") {
            VectorParameter Albedo    = float4(0.8, 0.6, 0.4, 1.0);
            ScalarParameter Roughness = 0.5 [Slider(0, 1)];
            ScalarParameter Metallic  = 0.0 [Slider(0, 1)];
        }
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        vec3  Color;
        float Rough;
        float Metal;

        Base.BaseColor = Color;
        Base.Roughness = Rough;
        Base.Metallic  = Metal;
    }

    Graph = {
        Color = Albedo.rgb;
        Rough = Roughness;
        Metal = Metallic;
    }
}
```

Output *variables* and binding *targets* live in separate namespaces, but naming them visibly differently
(`Color` → `Base.BaseColor`) keeps the file readable. The full target list is on
[Output Bindings](../generation/outputs.md); target names are case-sensitive, and a misspelt one is `DSH5200`.

### Written as a `.dss`

```hlsl
// DShader/Materials/M_Surface.dss
#pragma material(Domain = Surface, ShadingModel = DefaultLit, BlendMode = Opaque)

/// @group Surface
uniform float4 Albedo = float4(0.8, 0.6, 0.4, 1.0);
/// @group Surface   @slider 0 1
uniform float Roughness = 0.5;
/// @group Surface   @slider 0 1
uniform float Metallic = 0.0;

export void M_Surface(inout material m)
{
    m.BaseColor = Albedo.rgb;
    m.Roughness = Roughness;
    m.Metallic  = Metallic;
}
```

## Writing `MaterialAttributes`

Binding `Base.MaterialAttributes` turns on *Use Material Attributes* on the generated material.
`MaterialAttributes Attrs;` is an empty attribute set — one `MakeMaterialAttributes` node — and its members are
written by name; where the value is used, the writes become one `SetMaterialAttributes` node (since 2.0.0) (1.x
chained one per write).

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

        // A member already written reads back as the value written; no node is made.
        float Echo = Attrs.Roughness;
        Attrs.Specular = Echo;
    }
}
```

**Note:**

The `Outputs` bindings are applied after the `Graph`, so the binding takes the `Attrs` declared in the `Graph`, as
above. (since 2.0.0) An `Outputs` declaration `MaterialAttributes Attrs;` is that variable too, starting empty.
`MaterialAttributes` is a type of `Outputs`, `Inputs`, `Graph` variables and function results; as a `Properties` type
it is `DSH3252`, and as an input of a `Function` `DSH6210`.

- Member names are the material property names (`BaseColor`, `Metallic`, `Specular`, `Roughness`, `EmissiveColor`,
  `Opacity`, `Normal`, …) of the running engine; one it does not have is `DSH5200`.
- An arithmetic operator on a `MaterialAttributes` value is `DSH4226`.
- `Base.MaterialAttributes` and `Base.FrontMaterial` in one `Shader` are no longer refused (since 2.0.0): both are
  wired, and *Use Material Attributes* makes the material read the attribute set.

### Written as a `.dss`

```hlsl
// DShader/Materials/M_Attrs.dss
#pragma material(Domain = Surface, ShadingModel = DefaultLit, BlendMode = Opaque)

uniform float3 BaseTint = float3(0.6, 0.8, 1.0);
uniform float  R        = 0.35;

export void M_Attrs(inout material m)
{
    m.BaseColor = BaseTint;
    m.Roughness = R;
    m.Metallic  = 0.0;
    m.Specular  = m.Roughness;
}
```

The entry writes the members of `m`, which go straight to the material's pins: no `MakeMaterialAttributes` node is
made. A member is read back only after it is written (`DSH4370` otherwise).

## A Substrate surface

(since UE 5.4)

`Substrate.*` builds Substrate nodes; `Base.FrontMaterial` is the binding that consumes them.

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

- Substrate needs **UE 5.4 or newer**: the node catalog lists the Substrate classes of the running engine, and an older
  engine has none. There a `Substrate.*` call is `DSH5210` and `Base.FrontMaterial` `DSH5200`; `Substrate.Select` needs
  UE 5.6 (`DSH5294`). The project must also have Substrate on for the engine to compile the graph — see
  [Substrate sugar](../language-v2/substrate.md).
- DreamShader sets no shading model for `Base.FrontMaterial` (since 2.0.0) (1.x forced `Substrate` and refused any
  other `ShadingModel`): a `ShadingModel` setting is applied as written.
- A BSDF node takes its arguments by name; the five composition nodes (`Add`, `Weight`, `Mix`, `Layer`, `Select`) take
  them by position too (since 2.0.0). A positional argument elsewhere is `DSH5220`, and `Class=` is `DSH5216`: the
  name already is the class.
- A `Substrate.*` call in a `GraphFunction` body is not lifted into a node — no Custom node input carries a Substrate
  value — and says so with the warning `DSH6316` (since 2.0.0).
- (since 2.0.0) Substrate values combine: `A + B`, `A * w` and `lerp(A, B, t)` are `Substrate.Add`,
  `Substrate.Weight` and `Substrate.HorizontalMix`, and an `if` between two of them is a `StaticSwitch` under a static
  condition and a `Substrate.Select` otherwise. Any other operator is `DSH5293`, and a member read on a Substrate value
  `DSH4207`.

### Written as a `.dss`

```hlsl
// DShader/Materials/M_Substrate.dss
uniform float3 Color = float3(0.1, 0.6, 1.0);

export void M_Substrate(inout material m)
{
    m.FrontMaterial = Substrate.Unlit(EmissiveColor = Color);
}
```

## Driving a material from a parameter collection

```c
// DShader/Materials/M_Wind.dsm
Shader(Name="Materials/M_Wind")
{
    Properties = {
        vec3 Tint = vec3(0.4, 0.7, 0.3);
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float wind = UE.CollectionParam(
            Collection = Path(Game, "Collections/MPC_Wind"),
            Parameter  = "WindStrength");

        Color = Tint * wind;
    }
}
```

- `Collection` (alias `Asset`) and `Parameter` (alias `ParameterName`) are both **required**. `UE.CollectionParameter`
  is the engine's name, and the same class as `UE.CollectionParam` (since 2.1.1) (2.0.0 – 2.1.0 refused
  `UE.CollectionParam` in a `Graph` with `DSH5210`).
- The collection is loaded when the node is built and the parameter looked up by name: a collection that does not
  resolve or load is `DSH8213`, a name that is neither a scalar nor a vector parameter of it `DSH8254`.
- The engine does not type the node's output, so the value is as wide as the place it is read into
  (since 2.0.0): a `float` here.
- `Group` and `SortPriority` are taken only where the running engine's class has them ((since UE 5.7)); where it
  lacks them, `Group` is `DSH5213` and `SortPriority` fails with `DSH8212` when the node is built. See
  [UE.CollectionParam](../builtins/ue.md#uecollectionparam).

### Written as a `.dss`

```hlsl
// DShader/Materials/M_Wind.dss
#pragma material(Domain = Surface, ShadingModel = Unlit)

uniform float3 Tint = float3(0.4, 0.7, 0.3);

export void M_Wind(inout material m)
{
    float wind = UE.CollectionParameter(Collection = "/Game/Collections/MPC_Wind",
                                        ParameterName = "WindStrength");
    m.EmissiveColor = Tint * wind;
}
```

An asset property takes a quoted object path; `ParameterName` is the engine's name of that property.

## A multi-output helper

With two or more `out` parameters, the statement call form gets every result; the trailing arguments name the
variables that receive them.

```c
// DShader/Materials/M_Brightness.dsm
Function SplitBrightness(in vec3 color, out vec3 normalized, out float brightness) {
    brightness = max(max(color.r, color.g), color.b);
    normalized = color / max(brightness, 0.0001);
}

Shader(Name="Materials/M_Brightness")
{
    Properties = {
        vec3 Tint = vec3(1.0, 0.5, 0.25);
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
        SplitBrightness(Tint, Normalized, Brightness);
        Color = Normalized * Brightness;
    }
}
```

- The body is **HLSL**, so `max` here is the HLSL intrinsic, not the `Graph` math builtin.
- A 1.x `Function` has either a return type or `out` parameters: both is `DSH6304`, neither `DSH6305`.
- An out target nobody declared is declared by the call (info `DSH5283`). A value call —
  `vec3 n = SplitBrightness(Tint);` — gets the first `out` (since 2.0.0).

### Written as a `.dss`

```hlsl
// DShader/Materials/M_Brightness.dss
#pragma material(Domain = UI, ShadingModel = Unlit)

uniform float3 Tint = float3(1.0, 0.5, 0.25);

/// @custom
void SplitBrightness(float3 color, out float3 normalized, out float brightness)
{
    brightness = max(max(color.r, color.g), color.b);
    normalized = color / max(brightness, 0.0001);
}

export void M_Brightness(inout material m)
{
    float3 Normalized;
    float  Brightness;
    SplitBrightness(Tint, Normalized, Brightness);
    m.EmissiveColor = Normalized * Brightness;
}
```

`/// @custom` makes the body one Custom node verbatim, as a 1.x `Function` does; an `out` argument is a variable declared
first.

## Settings tour: glass with an explicit backend

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
| `Backend` | — | `Graph`, `ThinCustom`; `Instance` is the old spelling of `ThinCustom`, with the warning `DSH7204` |

- Enum values are matched with spaces, `_` and `-` stripped, case-insensitively: `"Default Lit"`, `"DefaultLit"`,
  `"default_lit"` and `"DEFAULT-LIT"` are one alias. Quotes are optional on every setting value.
- `MaterialDomain`, `ShadingModel` and `BlendMode` are resolved through the table above; `Backend` and
  (since 2.0.0) `Substrate` are DreamShader's own keys. **Every other key is reflected straight onto
  `UMaterial`** — `TwoSided` and `Wireframe` above are real `UMaterial` properties. A key that names no property fails
  the build: `DSH8215`, carrying `DSH7118`.
- A key written twice is the warning `DSH3262` (since 2.0.0) (silent before), and the later value wins. Repeated
  `Settings` sections merge.
- Omitting `Backend` falls back to the project's **Default Compiler Backend** (`ThinCustom` as shipped). With
  `Backend = "Graph"` the material is an ordinary `UMaterial`, saved on every successful build.

**Warning:**

`Backend = "";` is the error `DSH7201` (since 2.0.0) (1.x read it as `Graph`), and a value that is no backend is
`DSH7202`. Only *omitting* the key falls back to the project setting. See [Backend](../settings/backend.md).

### Written as a `.dss`

```hlsl
// DShader/Materials/M_Glass.dss
#pragma material(Domain = Surface, ShadingModel = DefaultLit, BlendMode = Translucent)
#pragma material(TwoSided = true, Wireframe = false, Backend = Graph)

uniform float3 Tint = float3(0.7, 0.9, 1.0);
uniform float Opacity = 0.35;

export void M_Glass(inout material m)
{
    m.BaseColor = Tint;
    m.Opacity = Opacity;
}
```

`#pragma material` takes the same keys and values as `Settings`, and may be split over several lines; a key set twice
is an error there, `DSH7200`.

## Runtime Virtual Texture: sampling and writing

The RVT nodes have no dedicated builtin family and need none. The sample, replace and feature-switch nodes are ordinary
expressions reached through [`UE.<ClassName>`](../builtins/ue-expression.md); `RuntimeVirtualTextureOutput` is a
*custom output* with no output pin at all, so it is never the right-hand side of a `Graph` assignment — it is bound in
`Outputs` through `Expression( … ).Pin[i]`.

```c
// DShader/Materials/M_RVTSurface.dsm
Shader(Name="Materials/M_RVTSurface", Root="Game")
{
    Properties = {
        vec3 Fallback = vec3(0.5, 0.5, 0.5);
    }

    Settings = {
        Backend      = "Graph";
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        float3 SurfaceColor;
        float3 RVTBaseColor;
        float  RVTRoughness;

        Base.BaseColor = SurfaceColor;

        // Both statements reuse one RuntimeVirtualTextureOutput node.
        Expression(Class="RuntimeVirtualTextureOutput").Pin[0] = RVTBaseColor;
        Expression(Class="RuntimeVirtualTextureOutput").Pin[2] = RVTRoughness;
    }

    Graph = {
        float3 Sampled = UE.RuntimeVirtualTextureSample(
            OutputType     = "float3",
            Output         = "BaseColor",
            MaterialType   = "BaseColor_Normal_Roughness",
            VirtualTexture = Path(Game, "RVT/RVT_Terrain"));

        // Where the virtual texture has no page, fall back to the parameter ...
        float3 Blended = UE.RuntimeVirtualTextureReplace(
            OutputType = "float3", Default = Fallback, VirtualTextureOutput = Sampled);

        // ... and where virtual texturing is off entirely, fall back to it again.
        SurfaceColor = UE.VirtualTextureFeatureSwitch(
            OutputType = "float3", Yes = Blended, No = Fallback);

        RVTBaseColor = SurfaceColor;
        RVTRoughness = 0.5;
    }
}
```

**Note:**

This example references `/Game/RVT/RVT_Terrain`, a project `URuntimeVirtualTexture` asset. Substitute one that exists
in your project.

| Node | `Class` specifier | Written as |
| :-- | :-- | :-- |
| Runtime Virtual Texture Sample | `RuntimeVirtualTextureSample` | `UE.…` in `Graph` |
| Runtime Virtual Texture Sample Parameter | `RuntimeVirtualTextureSampleParameter` | a [parameter declaration](../language/properties.md) in `Properties` |
| Runtime Virtual Texture Replace | `RuntimeVirtualTextureReplace` | `UE.…` in `Graph` |
| Virtual Texture Feature Switch | `VirtualTextureFeatureSwitch` | `UE.…` in `Graph` |
| Runtime Virtual Texture Output | `RuntimeVirtualTextureOutput` | `Expression( … ).Pin[i]` in `Outputs` |

- `RuntimeVirtualTextureSample` carries eight outputs: `0` BaseColor, `1` Specular, `2` Roughness, `3` Normal,
  `4` WorldHeight, `5` Mask, `6` Displacement, `7` Mask4. Select one by name with `Output="BaseColor"` or by position
  with `OutputIndex=`.
- `OutputType` is taken off each call (since 2.0.0); the node catalog types the nodes.
- `RuntimeVirtualTextureOutput` input pins, in `Pin[i]` order: `0` BaseColor, `1` Specular, `2` Roughness, `3` Normal,
  `4` WorldHeight, `5` Opacity, `6` Mask, `7` Displacement, `8` Mask4.
- `VirtualTexture` is a reflected asset property, so it takes a [`Path(...)`](../parameters/path.md) reference or a
  bare quoted object path. `MaterialType` and the node's other enum and bool properties are written by the same
  reflection pass, with the usual [enum spelling](../settings/material-enums.md) rules.
- A material that *writes* to RVT keeps `Domain = "Surface"` and adds the output node, exactly as above. The separate
  `Domain = "RuntimeVirtualTexture"` spelling maps to `MD_RuntimeVirtualTexture`, which the engine marks **deprecated
  and hidden**; DreamShader still accepts it so existing assets keep decompiling, but new materials should not use it.
- `Backend = "Graph"` is pinned here so the result is a plain `UMaterial`, saved on every build, whose nodes you can open
  and compare against the source. The nodes themselves do not depend on the backend.

### Written as a `.dss`

```hlsl
// DShader/Materials/M_RVTSurface.dss
#pragma material(Backend = Graph, Domain = Surface, ShadingModel = DefaultLit, BlendMode = Opaque)

uniform float3 Fallback = float3(0.5, 0.5, 0.5);

export void M_RVTSurface(inout material m)
{
    float3 Sampled = UE.RuntimeVirtualTextureSample(
        MaterialType = "BaseColor_Normal_Roughness",
        VirtualTexture = "/Game/RVT/RVT_Terrain").BaseColor;
    float3 Blended = UE.RuntimeVirtualTextureReplace(Default = Fallback, VirtualTextureOutput = Sampled);
    float3 SurfaceColor = UE.VirtualTextureFeatureSwitch(Yes = Blended, No = Fallback);

    m.BaseColor = SurfaceColor;
    UE.Expression(Class = "RuntimeVirtualTextureOutput", Pin[0] = SurfaceColor, Pin[2] = 0.5);
}
```

An output is selected by member access (`.BaseColor`), and an asset property takes a quoted object path. A
custom-output node is a statement of the entry; `Pin[i] = …` connects input `i` by its engine index.

## Running these files

1. Save them under `<Project>/DShader`, or wherever `SourceDirectory` points.
2. With *Auto Compile On Save* enabled — the default — the editor compiles a source 0.25 s after it is saved (*Save
   Debounce Seconds*). A `Graph`-backend material and every material function, layer and blend are saved to disk by each
   successful build. A `ThinCustom` material — the default backend — is built Ephemeral, in memory, until a cook, an
   explicit *Materialize*, a child material instance or the commandlet gives it a package.
3. Headless compile of one file:

```powershell
& "<Engine>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" `
  "<Project>/MyProject.uproject" `
  -run=DreamShader compile -Source="<Project>/DShader/Materials/M_Minimal.dsm" -Force `
  -unattended -nopause -nosplash -stdout -log
```

Substitute `-All` for `-Source=` to compile every source of the project: every `.dsf` first, then the other sources in
path order; a `.dsh` is compiled through the files that include it. The commandlet materializes every `ThinCustom`
product, so it writes a package for each material. See [Commandlet](../tools/commandlet.md#-all-discovery-and-ordering).

## Where to next

  - [Patterns](patterns.md) — The building blocks these materials are made from.
  - [Custom Pass examples](custom-pass.md) — .dsp pipelines: outline, frosted glass, X-ray, wind field.
  - [if / else](../graph/if.md) — Conditions, truthiness and the branch-merge rules.
  - [Material Settings](../settings/material.md) — Every Settings key and how reflection resolves it.
  - [Output Bindings](../generation/outputs.md) — Every Base.* target and what it accepts.
  - [Commandlet](../tools/commandlet.md) — Headless compilation and its switches.
