---
title: Your First Material
description: Write a UI Unlit material from scratch — once as a .dss and once in 1.x syntax — generate it, and find the result the editor deliberately hides.
---

This page writes a UI-domain, Unlit material with two parameters, then explains where the generated material actually
is — the thing that trips up everybody the first time. Write new sources as `.dss`; a project that already has `.dsm`
files can keep writing 1.x syntax, and the same compiler builds both.

## Create the file

```text
<Project>/DShader/Materials/M_DreamPulse.dss
```

```hlsl
#pragma material(Domain = UI, ShadingModel = Unlit)

/// @group Color   @desc Main tint
uniform float3 Tint = float3(0.2, 0.6, 1.0);
/// @group Color   @slider 0 4
uniform float Strength = 1.0;

export void M_DreamPulse(inout material m)
{
    float t = UE.Time();
    float pulse = UE.Sine(Input = t);
    m.EmissiveColor = Tint * (pulse * 0.5 + 0.5) * Strength;
}
```

Save it. With *Auto Compile On Save* on, DreamShader compiles the file after a short debounce and generates:

```text
/Game/Materials/M_DreamPulse
```

Or skip the typing: *Material Content Browser ▸ New ▾ ▸ Material (.dss)* writes the same shape from a template.

## What each part does

| Piece | Purpose | Reference |
| --- | --- | --- |
| `#pragma material(…)` | sets properties on the generated material — domain, shading model, blend mode, and any `UMaterial` property | [Material Settings](../settings/material.md), [Enum values](../settings/material-enums.md) |
| `///` lines and `uniform` | material parameters and their metadata: group, description, slider range | [DreamShaderLang 2.0](../language-v2/index.md#declarations) |
| `export void M_DreamPulse(inout material m)` | the entry: the one function that makes the file a material. The asset is named after it | [DreamShaderLang 2.0](../language-v2/index.md#two-short-examples) |
| `m.EmissiveColor = …` | a material output, checked against the engine's attribute list (a misspelling is `DSH5200`) | [Output Bindings](../generation/outputs.md) |
| `UE.Sine(Input = t)` | one material node, called by its engine class name; `UE.Expression(Class = "Sine", …)` is the same path | [UE.Expression](../builtins/ue-expression.md) |

The asset lands under `/Game` at the file's folder relative to the source root, named after the `export` function. A
`/// @name` above the entry names it otherwise — see [Asset Paths](../generation/asset-paths.md).

## The same material in 1.x syntax

```c
// <Project>/DShader/Materials/M_DreamPulse.dsm
Shader(Name="Materials/M_DreamPulse")
{
    Properties = {
        vec3 Tint = vec3(0.2, 0.6, 1.0) [Group="Color"; Description="Main tint"];
        float Strength = 1.0 [Group="Color"; Slider(0, 4)];
    }

    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float t = UE.Time();
        float pulse = UE.Expression(Class="Sine", Input=t);

        Color = Tint * (pulse * 0.5 + 0.5) * Strength;
    }
}
```

| Section | Purpose |
| --- | --- |
| `Shader(Name="…")` | declares the asset to generate. `Name` is the asset path relative to the root; `Root` is optional and defaults to `Game`. |
| `Properties` | creates material parameter nodes — a vector and a scalar here, with their metadata in `[ … ]`. |
| `Settings` | sets properties on the generated material. |
| `Outputs` | declares graph output variables and binds one to `Base.EmissiveColor`. |
| `Graph` | creates material nodes and assigns the final value to `Color`. |

Both files build the same `/Game/Materials/M_DreamPulse` — keep one. [`dsc migrate`](../tools/migrate.md)
rewrites a 1.x file as `.dss` and proves both build the same graph before it writes.

`UE.Expression(Class="Sine", …)` is the generic reflected call: it creates any material expression class of the running
engine by name. `Class` defaults to the function name, so the shorter `UE.Sine(Input=t)` is the identical call.
(since 2.0.0) `OutputType` is no longer required — the node catalog types the node — and `Sine`,
`MaterialExpressionSine`, `UMaterialExpressionSine` and `/Script/Engine.MaterialExpressionSine` all resolve. Class names
are case-sensitive; one that does not resolve is `DSH5212`.

## Finding the generated material

Looking in the Content Browser will not find it. That is not a failure.

Under the default `ThinCustom` backend a compile produces live `UObject`s and **no `.uasset` on disk** — the state is
called **Ephemeral**:

```text
UDreamShaderMaterialInstance          "M_DreamPulse"                              <- the asset you reference
  └─ parent: UMaterial (hidden, transient) "MB_DreamThinBase__Game_Materials_M_DreamPulse"
       └─ the generated node graph
```

The node graph — every parameter, every statement, every output — is built on that hidden base. The instance is a thin
wrapper that carries parameter values, provenance metadata and the compiled shader map. While the material is
Ephemeral, its `IsAsset()` returns `false`, so it hides from the Content Browser, asset pickers and save lists; its
package is marked not-dirty at the end of generation, so *Save All* does not sweep it to disk either.

The intent is deliberate: the source file is the thing you maintain. A `Backend = Graph` material is an ordinary
`UMaterial` instead, saved on every build, with no Ephemeral state.

There are three ways to get at an Ephemeral material.

### 1. The Material Content Browser

*Tools ▸ DreamShader ▸ Material Content Browser*. Sources mode lists every source file under every source root, with
its compile status, its diagnostics and the asset it built:

| Status | Meaning |
| --- | --- |
| `●` **up to date** | the asset exists and its recorded build key matches the current source |
| `●` **stale** | the asset exists, but the recorded build key differs |
| `○` **not compiled** | nothing at the resolved object path |
| `▲` **compile error** | the compile failed, or the bridge's diagnostics hold an error |
| `◆` **function / header** | the entry is a `.dsf` or `.dsh` |
| `▲` **unresolved** | what the source builds cannot be worked out: it has errors, or declares nothing |

With an entry selected you get **Compile**, **Create instance**, **Open**, **Materialize** and **Open source**; the
inspector shows every diagnostic, the imports and dependents, the inheritance chain and a live preview. This is the
normal way to work with generated materials. See [Editor tools](../tools/editor-integration.md#material-submenu).

### 2. Make them visible

*Tools ▸ DreamShader ▸ Show Ephemeral Materials*, or the same setting at *Project Settings ▸ DreamPlugin ▸ Dream Shader ▸
Compiler ▸ Show Ephemeral Materials*. Toggling it broadcasts asset-created / asset-removed events for every live
Ephemeral instance at once, so tiles appear or vanish without a rescan.

You need it on when picking the generated material as a material instance's Parent, or referencing it from a details
panel.

**Warning:**

With the toggle on, an Ephemeral material looks like any other tile — and an explicit **Save** on it really does write
a `.uasset`, materializing it. A product with a package on disk stays on disk: a compile rebuilds it and saves it,
logging `'…' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory.`. When you want a file,
use *Materialize*; to go back to Ephemeral, run *Tools ▸ DreamShader ▸ Make Ephemeral*, which deletes only ThinCustom
products carrying DreamShader provenance metadata.

### 3. Materialize it

**Materialize** writes the material and its hidden base to disk as one `.uasset`: it reruns the source's generation
Materialized and forced, then reloads the object at the resolved path. The button is in the Material Content Browser,
the details panel and the Content Browser context menu.

```text
on disk  <Project>/Content/Materials/M_DreamPulse.uasset
  export  M_DreamPulse                        UDreamShaderMaterialInstance
  export  MB_DreamThinBase_M_DreamPulse       UMaterial (hidden, a subobject in the same package)
```

Creating a child instance of an Ephemeral material, or compiling a `.dsi` whose parent it is, materializes the parent
first, because a transient base cannot be imported as a parent. Cook and the commandlet materialize automatically. The
full story is on [In-memory Materials](../generation/ephemeral.md).

## Controlling the parameter panel

In a `.dss`, `///` tags above a `uniform` write the group, sort order, description and range on Unreal's parameter
panel:

```hlsl
/// @group Color   @sort 10   @desc Main tint
uniform float4 Tint = float4(0.2, 0.6, 1.0, 1.0);
/// @group Color   @sort 20   @slider 0 4
uniform float Strength = 1.0;
```

In 1.x, switch to the explicit parameter nodes and add a metadata block:

```c
Properties = {
    VectorParameter Tint = float4(0.2, 0.6, 1.0, 1.0) [
        Group="Color";
        SortPriority=10;
        Description="Main tint";
    ];
    ScalarParameter Strength = 1.0 [
        Group="Color";
        Slider(0, 4);
    ];
}
```

`Group`, `SortPriority` and `Description` (written to the node's `Desc`) are recognised keys; any other key is written
straight to a reflected UPROPERTY of the generated node's class. `Slider(min, max)` (since 1.5.0) expands to
`SliderMin` / `SliderMax`; on a vector or switch parameter it is the warning `DSH7233` and the range is dropped. See
[Metadata](../parameters/metadata.md).

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Nothing in the Content Browser | Expected. The material is Ephemeral — use the Material Content Browser, *Show Ephemeral Materials*, or *Materialize*. |
| No material generated at all | Make sure the file is under *Source Directory*, has a `.dss` / `.dsm` extension, and *Auto Compile On Save* is on; a `.dss` needs an `export` entry, a `.dsm` a top-level `Shader`. |
| Saving a `.dsh` generates nothing | Correct — headers never generate assets; saving one queues every source that imports it. |
| The output is black | A `.dss` writes members of `m`; 1.x `Outputs` need a `Base.* = …` binding (no `Outputs` at all is the warning `DSH2256`). |
| A name is not found (`DSH4200`) | 2.0 names are case-sensitive; in a 1.x source a name that differs only in case still resolves, with the warning `DSH5275`. |
| A node class does not resolve (`DSH5210` / `DSH5212`) | Class names are case-sensitive and must be a class the running engine has. |
| A material function's callers disconnected | Renaming an input or output is a breaking change — pin identity is restored by name. |
| Anything else | Read the Output Log (`LogDreamShader`), or the error on the row in the Material Content Browser. Every message carries a `DSHnnnn` code — look it up on [Diagnostics](../diagnostics/index.md). |

## Next

- [Project layout](project-layout.md) — where files go once the project grows
- [Daily workflow](workflow.md) — what triggers a rebuild and what does not
- [DreamShaderLang 2.0](../language-v2/index.md) — what a `.dss` can say
- [Patterns](../examples/patterns.md) — every building block in `.dss` and 1.x form
- [Graph language](../graph/index.md) — what a 1.x `Graph = { … }` accepts
