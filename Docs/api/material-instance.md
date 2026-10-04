# DreamShaderMaterialInstance.h

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderMaterialInstance.h**

`UDreamShaderMaterialInstance` — the addressable asset the ThinCustom backend produces: a constant
material instance whose parent is a hidden per-material base `UMaterial` carrying the real node
graph.

Defined in header `DreamShaderMaterialInstance.h`.

| | |
| :-- | :-- |
| Module | `DreamShader` (Runtime) |
| Include | `#include "DreamShaderMaterialInstance.h"` |
| Namespace | global scope |
| Base class | `UMaterialInstanceConstant` |
| Export macro | `DREAMSHADER_API` |
| Since | `1.5.0` — the ThinCustom backend |
| Pulls in | `CoreMinimal.h`, `Materials/MaterialInstanceConstant.h` |

## Synopsis

```cpp
UCLASS(ClassGroup = DreamShader)
class DREAMSHADER_API UDreamShaderMaterialInstance : public UMaterialInstanceConstant
{
    GENERATED_BODY()

public:
    /** DreamShader source file this instance was generated from. */
    UPROPERTY(VisibleAnywhere, Category = "DreamShader")
    FString SourceFilePath;

    /** Hash of the source text (the regeneration skip check compares against it). */
    UPROPERTY(VisibleAnywhere, Category = "DreamShader")
    FString SourceHash;

    //~ Begin UMaterialInstance interface
    virtual bool HasOverridenBaseProperties() const override;
    //~ End UMaterialInstance interface

    //~ Begin UObject interface
    virtual bool IsAsset() const override;
    //~ End UObject interface
};
```

The class adds no methods beyond the two overrides, no constructor, and no `PostLoad`. Everything
else — parameters, settings, domain, scene reads — lives on the hidden base as ordinary expressions
and material properties, enumerated natively by the engine.

## Properties

| Property | Type | Specifiers | Written with | Meaning |
| :-- | :-- | :-- | :-- | :-- |
| `SourceFilePath` | `FString` | `VisibleAnywhere`, `Category="DreamShader"` | the **absolute normalized** source path, as produced by [`NormalizeSourceFilePath`](dreamshader-module.md#normalizesourcefilepath) | The source this instance was generated from. Read back by the instance factory and the compiler service, and shown in the details panel. |
| `SourceHash` | `FString` | `VisibleAnywhere`, `Category="DreamShader"` | an 8-hex-digit CRC32 of the **build key**, formatted `%08x`: the preprocessed source and the headers it includes, plus the plugin and engine versions, the settings that change what is built and the defines the source read ([Caching](../generation/caching.md#it-is-a-build-key-not-just-a-source-hash)) | Informational. See the warning below. |

Both are read-only in the details panel — `VisibleAnywhere`, not `EditAnywhere` — and neither is
`Config` or `Transient`.

> [!WARNING]
> The header comment on `SourceHash` says "the regeneration skip check compares against it". That is
> imprecise. The skip check reads the **package metadata** keys `DreamShader.SourceFile` and
> `DreamShader.SourceHash`, not these `UPROPERTY`s. The values coincide, but the two paths differ:
> the metadata stores a **project-relative** source path while `SourceFilePath` stores the
> **absolute** one. Do not implement your own skip logic against these properties. See
> [Caching](../generation/caching.md).

## `HasOverridenBaseProperties`

```cpp
virtual bool HasOverridenBaseProperties() const override;
```

| | |
| :-- | :-- |
| Returns | `true` when `Cast<UMaterial>(Parent) != nullptr` — i.e. this instance's immediate parent is a base `UMaterial`, which makes it the **root** of its inheritance chain |
| Otherwise | defers to `Super::HasOverridenBaseProperties()` |

The engine recomputes `bHasStaticPermutationResource` from this on every load, inside
`InitStaticPermutation`, so the test must be durable state rather than a transient flag — hence the
parent-class test rather than a stored boolean. The pattern mirrors
`ULandscapeMaterialInstanceConstant`.

| Position in the chain | `HasOverridenBaseProperties()` | Shader map |
| :-- | :-- | :-- |
| The generated instance (parent is the hidden base `UMaterial`) | `true` | owns its own static-permutation shader map |
| A child `UMaterialInstanceConstant` parented to that instance | stock behaviour | **shares** the root's compiled map |

That is the point of the override: any number of colour and parameter variants created off one
DreamShader material reuse a single compiled shader map instead of each compiling its own, which
bounds the shader-map count for a project that instances heavily. See
[Material Content Browser](../tools/material-browser.md) for the instance-creation action.

## `IsAsset`

```cpp
virtual bool IsAsset() const override;
```

| | |
| :-- | :-- |
| Returns | `false` when the owning package has `PKG_NewlyCreated` **and** `bShowEphemeralMaterials` is `false` |
| Otherwise | defers to `Super::IsAsset()` |

Returning `false` removes the object from the Content Browser, from the asset registry's
live-object-iterator discovery path, and from save pickers — which also prevents an accidental
explicit *Save* from materializing a memory-only material to disk. Object-path references still
resolve normally; only enumeration is affected. A persisted instance has no `PKG_NewlyCreated` flag
and behaves like any other asset.

> [!NOTE]
> The setting is read on **every call**, through `GetDefault<UDreamShaderSettings>()`. Toggling
> *Show Ephemeral Materials* changes the answer immediately, with no reload; the
> editor re-broadcasts asset creation and removal for every memory-only instance so the browser
> refreshes. See [Project settings](../settings/project.md).

## Role in the ThinCustom result

A Materialized ThinCustom compile produces **one package** containing:

```text
/Game/Materials/M_Emissive                      UDreamShaderMaterialInstance   (the addressable asset)
  └─ MB_DreamThinBase_M_Emissive                UMaterial                      (subobject, hidden base)
       └─ the generated node graph, parameters, settings, output bindings
```

An Ephemeral one keeps the hidden base out of that package: it is a transient `UMaterial` in the
transient package, named `MB_DreamThinBase_` plus the instance's package path run through
[`SanitizeIdentifier`](dreamshader-module.md#sanitizeidentifier) (`MB_DreamThinBase__Game_Materials_M_Emissive`),
so two instances with the same leaf name in different folders never share a base. See
[Ephemeral materials](../generation/ephemeral.md).

| Concern | Lives on |
| :-- | :-- |
| Node graph, `Properties` nodes, output bindings, `Settings` (shading model, blend mode, domain, flags) | the hidden base `UMaterial` |
| Asset identity, package, the addressable object path | the `UDreamShaderMaterialInstance` |
| The static-permutation shader map | the instance, as chain root |
| Parameter overrides authored by hand | the instance — read off before a rebuild and written back after it, by name and kind *(since 1.9.0)* |

> [!WARNING]
> Regeneration under the ThinCustom backend rebuilds the hidden base's graph and clears the
> instance's parameter values; the overrides are captured first and restored afterwards by name. An
> override whose parameter was renamed or removed in the source is dropped, and reported once as
> [`DSH8155`](../diagnostics/DSH8xxx.md#dsh8155). Create a **child** instance for values a rebuild
> must never touch. See [Regeneration](../generation/regeneration.md#parameter-overrides-on-a-generated-instance).

The instance is created with
`NewObject<UDreamShaderMaterialInstance>(InstancePackage, FName(*AssetName), RF_Public | RF_Standalone)`,
or reused in place when an instance of this class already occupies the target path. Any other object
at that path is refused rather than replaced ([`DSH8106`](../diagnostics/DSH8xxx.md#dsh8106)), and so
is a saved instance of this class that carries no DreamShader source metadata
([`DSH8107`](../diagnostics/DSH8xxx.md#dsh8107)).

## Notes

- `ClassGroup = DreamShader` places the class in its own group in class pickers. There is no
  `BlueprintType`, no `Blueprintable`, and no factory registered for manual creation — the class is
  only ever instantiated by the compiler's asset factory.
- The class ships in the **Runtime** module, not the editor module, because a cooked build must be
  able to load the instances that cooking materialized.
- Neither override consults the source properties. `SourceFilePath` and `SourceHash` are inert with
  respect to engine behaviour; they exist for tooling and for the details panel.
- Under the `Graph` backend no instance is produced at all — the asset at the target path is a plain
  `UMaterial`. See [Backend](../settings/backend.md).

## Example

Finding the source file behind a selected asset:

```cpp
#include "DreamShaderMaterialInstance.h"

FString GetDreamShaderSource(const UMaterialInterface* Material)
{
    if (const UDreamShaderMaterialInstance* Instance = Cast<UDreamShaderMaterialInstance>(Material))
    {
        return Instance->SourceFilePath;   // absolute, '/' separators
    }
    return FString();
}
```

Details-panel view of a generated instance:

```text
Parent                 MB_DreamThinBase_M_Emissive
DreamShader
  Source File Path     I:/Project/DShader/Materials/M_Emissive.dsm
  Source Hash          9f2c41ab
```

## See also

- [Ephemeral materials](../generation/ephemeral.md) — the hidden base, visibility, and materializing
- [Backend](../settings/backend.md) — `ThinCustom` versus `Graph`, and the deprecated `Instance` alias
- [Regeneration](../generation/regeneration.md) — what a rebuild destroys on the instance
- [Caching](../generation/caching.md) — the package metadata keys the skip check actually uses
- [Project settings](../settings/project.md) — the visibility toggle behind `IsAsset()`
- [`DreamShaderSettings.h`](settings.md) — `bShowEphemeralMaterials` in C++
- [Material Content Browser](../tools/material-browser.md) — creating child instances and materializing
- [`Shader`](../language/shader.md) — the block that generates this asset
- [Asset paths](../generation/asset-paths.md) — where the package lands
