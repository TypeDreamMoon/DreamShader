# Getting started

> [DreamShader](index.md) » **Getting started**

This page takes a project from "plugin copied in" to "a material generated from a text file".

## Install

1. Copy the plugin into the project:

   ```text
   MyProject/Plugins/DreamShader/
   ```

2. Enable **DreamShader** in *Edit ▸ Plugins* and restart the editor. DreamShader depends on the
   engine plugins `WebSocketNetworking` and `SQLiteCore`; both are enabled automatically.
3. Confirm the settings page exists: *Project Settings ▸ DreamPlugin ▸ Dream Shader*.

| | |
| :-- | :-- |
| Engines | Unreal Engine `5.3` – `5.8`, Win64 verified |
| Modules loaded | `DreamShader`, `DreamShaderCompiler` (runtime) and `DreamShaderEditor` (editor) |

## Create the source directory

DreamShader reads source files from one directory under the project root, `DShader` by default
(*Project Settings ▸ DreamPlugin ▸ Dream Shader ▸ Paths ▸ Source Directory*).

```text
MyProject/
├─ DShader/
│  ├─ Materials/        # .dsm  — material implementations
│  ├─ Functions/        # .dsf  — reusable material function assets
│  ├─ Shared/           # .dsh  — headers: Function, GraphFunction, Namespace, VirtualFunction
│  └─ Packages/         # installed shared libraries
└─ Plugins/
   └─ DreamShader/
```

The three extensions are not interchangeable — see [Source files](language/source-files.md).

## Write a material

`MyProject/DShader/Materials/M_Minimal.dsm`:

```c
Shader(Name="DreamMaterials/M_Minimal")
{
    Properties = {
        vec3 Tint = vec3(1.0, 0.2, 0.2);
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
        Color = Tint;
    }
}
```

Save the file. With *Auto Compile On Save* enabled (the default), DreamShader parses the source after
a short debounce and builds the material.

| Piece | Reference |
| :-- | :-- |
| `Shader(Name=…)` | [`Shader`](language/shader.md) |
| `Properties = { … }` | [`Properties`](language/properties.md), [parameter catalogue](parameters/index.md) |
| `Settings = { … }` | [Material settings](settings/material.md), [enum values](settings/material-enums.md) |
| `Base.EmissiveColor = …` | [Output bindings](language/output-bindings.md) |
| `Graph = { … }` | [Graph language](graph/index.md) |

`Name` is the asset path relative to the root. `Root` defaults to `Game`, so this file produces
`/Game/DreamMaterials/M_Minimal`. Use `Root="Plugin.MyPlugin"` to generate into a content plugin —
see [Asset paths](generation/asset-paths.md).

## Find the generated material

By default DreamShader generates materials **in memory**: the editor does not write a per-material
`.uasset`, and the result does not appear in the Content Browser. This is deliberate — the source
file is the authoring surface, and hiding the asset prevents an accidental *Save* from materializing
it.

To work with the result:

- Open *Tools ▸ DreamShader ▸ Material Content Browser* and switch to the **Dream Shader Gen** page.
  It lists the source files, their compile status, and a preview.
- Or set *Project Settings ▸ DreamPlugin ▸ Dream Shader ▸ Compiler ▸ Show In-Memory Materials In
  Content Browser* to make them visible like unsaved assets.
- Or materialize a material to disk from the browser when you need a real asset on disk.

Cooking materializes the assets automatically. See [In-memory materials](generation/in-memory.md).

## When something fails

Parse and generation errors go to the Output Log and to the Material Content Browser's source list.
Look the message up in the [diagnostics index](diagnostics/index.md).

## Next steps

| | |
| :-- | :-- |
| [Examples](examples/index.md) | Complete sources for parameters, functions, layers, Substrate, and more |
| [Language reference](language/index.md) | The full declaration grammar |
| [Graph language](graph/index.md) | What you can write inside `Graph = { … }` — and what you cannot |
| [`import`](language/import.md) | Sharing helpers across files |
| [Editor tools](tools/index.md) | Browser, decompiler, VSCode workspace, headless commandlet |
| [Project settings](settings/project.md) | Every setting and its default |

## See also

- [Main page](index.md)
- [Regeneration](generation/regeneration.md) — what a rebuild overwrites
- [Contributing](contributing/index.md) — building the plugin from source
