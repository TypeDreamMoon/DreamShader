# DreamShaderCompiler

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderCompiler**

The compile abstraction layer: a request struct, a result struct, the `IDreamShaderCompiler`
interface, a thin service wrapper, and a module class. It contains no material-generation code — its
whole purpose is to let code request a compile without linking `DreamShaderEditor`.

| | |
| :-- | :-- |
| Module | `DreamShaderCompiler` (Runtime, `Default` loading phase) |
| Public headers | `DreamShaderCompilerInterfaces.h`, `DreamShaderCompileService.h`, `DreamShaderCompilerModule.h` |
| Namespace | `UE::DreamShader::Compiler` (interfaces and service) · global scope (module class) |
| Export macro | `DREAMSHADERCOMPILER_API` |
| Build dependencies | `Core`, `CoreUObject`, `DreamShader`, `Engine` — all **public** |
| Reflection | none |

Because `DreamShader` is a *public* dependency, a module that lists `DreamShaderCompiler` also gets
the parser, the types and the settings header transitively.

## `DreamShaderCompilerInterfaces.h`

```cpp
#include "CoreMinimal.h"

#ifndef DREAMSHADERCOMPILER_API
#define DREAMSHADERCOMPILER_API
#endif

namespace UE::DreamShader::Compiler
{
    struct DREAMSHADERCOMPILER_API FDreamShaderCompileRequest
    {
        FString SourceFilePath;
        bool bForce = false;
        bool bTransient = false;
    };

    struct DREAMSHADERCOMPILER_API FDreamShaderCompileResult
    {
        bool bSucceeded = false;
        FString Message;
    };

    class DREAMSHADERCOMPILER_API IDreamShaderCompiler
    {
    public:
        virtual ~IDreamShaderCompiler() = default;

        virtual FDreamShaderCompileResult CompileAssets(const FDreamShaderCompileRequest& Request) = 0;
        virtual FDreamShaderCompileResult CompileMaterial(const FDreamShaderCompileRequest& Request) = 0;
    };
}
```

The `#ifndef DREAMSHADERCOMPILER_API` guard makes the header compile outside a UnrealBuildTool
context, where the macro is not pre-defined.

### `FDreamShaderCompileRequest`

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `SourceFilePath` | `FString` | `""` | Path to a `.dsm` or `.dsf`. The shipped implementation normalizes it, so a relative path is accepted. A `.dsh` is rejected by both entry points. |
| `bForce` | `bool` | `false` | Bypass the source-hash skip check. With `false`, an asset whose package metadata still matches is left untouched and the call **succeeds** with a `Skipped …` message. |
| `bTransient` | `bool` | `false` | `true` — everything is created in the transient package or as a subobject, nothing is saved, and the package's dirty flag is explicitly cleared so a *Save All* cannot persist it. `false` — real packages, `MarkPackageDirty`, source metadata stamped, package saved. |

> [!NOTE]
> `bTransient == true` is the editor's **normal** mode, not an exotic flag. The bridge and the
> preview renderer both pass `true`; only the commandlet, the cook path and the explicit
> *Materialize* action pass `false`. See [In-memory materials](../generation/in-memory.md).

### `FDreamShaderCompileResult`

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `bSucceeded` | `bool` | `false` | Whether the compile completed. A skipped compile counts as success. |
| `Message` | `FString` | `""` | On success: one line per generated asset, joined with `\n`, optionally followed by `"\nWarnings:\n"` and the joined parser warnings. On failure: the single diagnostic. |

There is no structured diagnostic list, no severity, and no line/column. `Message` is human-readable
text; the machine-readable diagnostics go to the bridge's JSON files instead. See
[Editor bridge](../tools/bridge.md).

### `IDreamShaderCompiler`

The extension point. Two pure virtuals and a defaulted virtual destructor; no other members, no
copy/move policy declared.

```cpp
virtual FDreamShaderCompileResult CompileAssets(const FDreamShaderCompileRequest& Request) = 0;
```

| | |
| :-- | :-- |
| Contract | Generate **everything** the source file declares: the helper `.ush` include if the unit has any `Function` block, every `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend`, and the material if a `Shader` block is present. |
| Must reject | `.dsh` inputs — a header never generates assets directly. |
| Success with no assets | A unit that declares only `VirtualFunction` or only `GraphFunction` blocks succeeds and generates nothing. A unit that declares nothing generatable **fails**. |

```cpp
virtual FDreamShaderCompileResult CompileMaterial(const FDreamShaderCompileRequest& Request) = 0;
```

| | |
| :-- | :-- |
| Contract | Generate only the material — the top-level `Shader` block. |
| Must reject | Both `.dsh` **and** `.dsf` inputs. |
| Requires | A non-empty `Shader(Name=…)` and a non-empty `Outputs` section. |

The interface expresses **no thread-affinity contract**. The only shipped implementation is
game-thread- and editor-only; see [Thread and context requirements](#thread-and-context-requirements).

## `DreamShaderCompileService.h`

```cpp
#include "DreamShaderCompilerInterfaces.h"

namespace UE::DreamShader::Compiler
{
    class DREAMSHADERCOMPILER_API FDreamShaderCompileService
    {
    public:
        explicit FDreamShaderCompileService(IDreamShaderCompiler& InCompiler)
            : Compiler(InCompiler)
        {
        }

        FDreamShaderCompileResult CompileAssets(const FString& SourceFilePath,
                                                bool bForce = false,
                                                bool bTransient = false);
        FDreamShaderCompileResult CompileMaterial(const FString& SourceFilePath,
                                                  bool bForce = false,
                                                  bool bTransient = false);

    private:
        IDreamShaderCompiler& Compiler;
    };
}
```

| Aspect | Detail |
| :-- | :-- |
| Constructor | `explicit`, header-inline, stores a **reference** |
| `CompileAssets` | Packs the three arguments into an `FDreamShaderCompileRequest` and forwards to `Compiler.CompileAssets` |
| `CompileMaterial` | The same, forwarding to `Compiler.CompileMaterial` |
| Added logic | **None.** The service is purely argument packing, so a caller need not name the request struct. |
| Copy / move | Copy construction is allowed; the reference member makes the class non-assignable |

> [!WARNING]
> The service holds a **reference**, not a shared pointer. The referenced `IDreamShaderCompiler`
> must outlive the service. Every shipped call site avoids the problem by constructing the service
> on the stack immediately before use and letting it die at the end of the scope. Follow that idiom.

### Shipped call sites

| Caller | Call | Effect |
| :-- | :-- | :-- |
| Editor bridge, on file save or a queued request | `CompileAssets(SourceFilePath, false, /*bTransient*/ true)` | Hash-skip active, memory-only — the common path |
| Commandlet `-run=DreamShader compile` | `CompileAssets(SourceFile, bForce)` | `bTransient` defaults to `false`, so assets are persisted |
| Preview renderer | `CompileMaterial(SourceFilePath, true, /*bTransient*/ true)` | Always forced, memory-only |

## `DreamShaderCompilerModule.h`

```cpp
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#ifndef DREAMSHADERCOMPILER_API
#define DREAMSHADERCOMPILER_API
#endif

class DREAMSHADERCOMPILER_API FDreamShaderCompilerModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
```

Registered with `IMPLEMENT_MODULE(FDreamShaderCompilerModule, DreamShaderCompiler)`.

> [!NOTE]
> **Both methods are empty.** The module exists so the headers have a module to live in; it
> registers nothing, allocates nothing, and holds no state. There are no `Get()` or `IsAvailable()`
> accessors.

## How the editor module plugs in

`DreamShaderEditor` implements the interface with a private adapter that is **not exported**:

```cpp
// Private to DreamShaderEditor — not reachable from another module.
namespace UE::DreamShader::Editor
{
    class FEditorCompileAdapter final : public Compiler::IDreamShaderCompiler
    {
    public:
        virtual Compiler::FDreamShaderCompileResult CompileAssets  (const Compiler::FDreamShaderCompileRequest&) override;
        virtual Compiler::FDreamShaderCompileResult CompileMaterial(const Compiler::FDreamShaderCompileRequest&) override;
    };

    FEditorCompileAdapter& GetEditorCompileAdapter();
}
```

| Adapter method | Delegates to |
| :-- | :-- |
| `CompileAssets` | `FMaterialGenerator::GenerateAssetsFromFile(Request.SourceFilePath, Result.Message, Request.bForce, Request.bTransient)` |
| `CompileMaterial` | `FMaterialGenerator::GenerateMaterialFromFile(Request.SourceFilePath, Result.Message, Request.bForce, Request.bTransient)` |
| `GetEditorCompileAdapter()` | Returns a function-local `static FEditorCompileAdapter` — a lazily constructed process-wide singleton. Initialization is thread-safe through magic statics; the adapter itself is not thread-safe. |

Each adapter method does exactly one thing: call the generator static and copy its `bool` return into
`bSucceeded` and its out-string into `Message`.

### What a third party can and cannot do today

| | |
| :-- | :-- |
| **Can** implement `IDreamShaderCompiler` and drive it through `FDreamShaderCompileService`, or call it directly | ✔ |
| **Can** link `DreamShaderCompiler` from a Runtime module and pass around `FDreamShaderCompileRequest` / `FDreamShaderCompileResult` without any editor dependency | ✔ |
| **Can** wrap the shipped behaviour by implementing the interface and forwarding to a compile the editor triggers by other means | ✔ |
| **Cannot** obtain the shipped implementation from outside `DreamShaderEditor` — `GetEditorCompileAdapter()` is declared in a private header of a module that exports nothing | ✘ |
| **Cannot** register an implementation with the plugin — **there is no registry, no factory and no delegate.** The three shipped call sites name `GetEditorCompileAdapter()` directly | ✘ |
| **Cannot** replace or intercept what the bridge, the preview or the commandlet compile with | ✘ |
| **Cannot** call the generator directly — `FMaterialGenerator` lives in a private editor header | ✘ |

> [!WARNING]
> `IDreamShaderCompiler` is an abstraction boundary, not a plug-in point. Implementing it lets your
> own code speak the same vocabulary; it does not let you substitute a backend into DreamShader's
> own pipeline. To trigger the shipped generator from C++ outside the editor module, use the
> [commandlet](../tools/commandlet.md) or the bridge's
> [request files](../tools/bridge.md).

## Thread and context requirements

Obligations that apply to anyone calling through the shipped adapter:

- **Game thread, editor build only.** Both generator entry points open an `FScopedSlowTask` and,
  outside a commandlet, show a modal progress dialog after a short delay.
- They create and modify `UPackage`s and `UMaterial`s, and call `Modify()`, `PostEditChange()` and
  the asset save path — all game-thread-only operations.
- Progress frames: 6 for `CompileAssets`, 11 for `CompileMaterial`.
- The calls are **synchronous**. There is no async variant, no future, and no completion delegate.

### Control flow of the shipped implementation

`CompileAssets`: reject `.dsh` → load and import-expand the source → parse → CRC32 the prepared text
→ reject a `Shader` block in a `.dsf` → write the generated helper `.ush` include if the unit has any
`Function` block → generate each material-function asset in declaration order → generate the material
if the unit declares one → assemble the message.

`CompileMaterial`: reject `.dsh` and `.dsf` → load and expand → parse → hash → require a `Shader`
name → require a non-empty `Outputs` → validate settings, then outputs → reject `Base.FrontMaterial`
together with `Base.MaterialAttributes` → write the helper include → resolve the backend → take the
ThinCustom or the Graph path → source-hash skip check → build → persist, or clear the dirty flag.

The full pipeline is on [Generation](../generation/index.md).

## Result messages

Every string the shipped adapter can put in `FDreamShaderCompileResult::Message`. Runtime
substitutions are shown as `{Placeholder}`.

### Success

| Message | Condition |
| :-- | :-- |
| `Generated {Kind} {AssetPath} from {File}.` | one line per `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` asset |
| `Generated DreamShader thin-custom material {AssetPath} from {File}.` | ThinCustom-backend material — the default path. No `(virtual)` suffix, whatever `bTransient` is |
| `Generated {AssetPath} from {File}.{Suffix}` | Graph-backend material; `{Suffix}` is ` (virtual)` when `bTransient` |
| `Generated DreamShader helper include '{Path}' from {File}.` | the unit declared only `Function` blocks |
| `DreamShader file '{File}' contains VirtualFunction declarations only; no assets were generated.` | nothing to generate, but not an error |
| `DreamShader file '{File}' contains GraphFunction declarations only; no assets were generated.` | nothing to generate, but not an error |
| `Skipped {AssetPath} from {File}; source hash is unchanged.` | `bForce == false` and the package metadata still matches |
| `\nWarnings:\n` + the joined parser warnings | appended to any successful message when warnings were emitted |

### Failure

| Message | Condition |
| :-- | :-- |
| `DreamShader header '{File}' does not generate assets directly. Recompile dependent .dsm or .dsf files instead.` | `CompileAssets` on a `.dsh` |
| `DreamShader source '{File}' cannot generate a material asset directly.` | `CompileMaterial` on a `.dsh` or `.dsf` |
| `{File}: .dsf files cannot define top-level Shader blocks.` | a `.dsf` containing a `Shader` block |
| `{File}: This file does not define a top-level Shader block.` | `CompileMaterial` on a unit with no `Shader` |
| `{File}: Outputs block is required.` | the `Shader` declared no output bindings |
| `{File}: Base.FrontMaterial and Base.MaterialAttributes cannot be used by the same Shader.` | both bindings present |
| `DreamShader file '{File}' did not contain any material, ShaderFunction, ShaderLayer, or ShaderLayerBlend assets to generate.` | `CompileAssets` produced nothing generatable |
| `{File}: {InnerError}` | the generic wrapper for validation, backend-resolution, include-writing and save errors |

The skip check that produces the `Skipped …` message returns `true` only when the asset exists, the
new hash is non-empty, the package metadata `DreamShader.SourceFile` equals the project-relative
source path (compared ignoring case) **and** `DreamShader.SourceHash` equals the new hash (compared
case-sensitively). Material functions additionally require the asset's material-function usage to
match the expected kind. See [Caching](../generation/caching.md).

## Notes

- The module declares no delegates, so a compile cannot be observed asynchronously. Poll the return
  value, or read the bridge's diagnostics files.
- `FDreamShaderCompileRequest` and `FDreamShaderCompileResult` carry `DREAMSHADERCOMPILER_API` even
  though they are header-only aggregates. That is harmless and makes them safe to name across a DLL
  boundary in any configuration.
- An editor-private decompiler abstraction mirrors this design almost exactly — a request struct, a
  result struct, an `IDreamShaderDecompiler` interface with two methods, and an
  `FDreamShaderDecompileService`. It is **not** public and not linkable. See
  [Decompiler](../tools/decompiler.md).
- Nothing in this module reads project settings, touches the file system or logs. All of that
  happens inside the implementation.

## Example

Implementing the interface — for example, to route compiles through your own queue:

```cpp
#include "DreamShaderCompileService.h"
#include "DreamShaderCompilerInterfaces.h"
#include "DreamShaderModule.h"

using namespace UE::DreamShader;

class FLoggingCompiler final : public Compiler::IDreamShaderCompiler
{
public:
    explicit FLoggingCompiler(Compiler::IDreamShaderCompiler& InInner) : Inner(InInner) {}

    virtual Compiler::FDreamShaderCompileResult CompileAssets(const Compiler::FDreamShaderCompileRequest& Request) override
    {
        const double Start = FPlatformTime::Seconds();
        Compiler::FDreamShaderCompileResult Result = Inner.CompileAssets(Request);
        UE_LOG(LogDreamShader, Display, TEXT("CompileAssets(%s) -> %s in %.1f ms"),
            *Request.SourceFilePath,
            Result.bSucceeded ? TEXT("ok") : TEXT("failed"),
            (FPlatformTime::Seconds() - Start) * 1000.0);
        return Result;
    }

    virtual Compiler::FDreamShaderCompileResult CompileMaterial(const Compiler::FDreamShaderCompileRequest& Request) override
    {
        return Inner.CompileMaterial(Request);
    }

private:
    Compiler::IDreamShaderCompiler& Inner;
};
```

Driving it through the service — note that the service is a stack value built immediately before use,
so the referenced compiler cannot dangle:

```cpp
void CompileOne(Compiler::IDreamShaderCompiler& InCompiler, const FString& InPath)
{
    Compiler::FDreamShaderCompileService Service(InCompiler);

    const Compiler::FDreamShaderCompileResult Result =
        Service.CompileAssets(NormalizeSourceFilePath(InPath), /*bForce*/ false, /*bTransient*/ true);

    UE_LOG(LogDreamShader, Display, TEXT("%s"), *Result.Message);
}
```

A typical successful message for a file declaring one function asset and one material:

```text
Generated ShaderFunction /Game/Functions/F_Tint from I:/Project/DShader/Materials/M_Emissive.dsm.
Generated DreamShader thin-custom material /Game/Materials/M_Emissive from I:/Project/DShader/Materials/M_Emissive.dsm.

Warnings:
No Outputs block was provided. Generation requires explicit material property bindings.
```

## See also

- [C++ API](index.md) — modules, headers, linkage, and build dependencies
- [`DreamShaderParser.h`](parser.md) — the front end this module's implementations run first
- [`DreamShaderTypes.h`](types.md) — the definition a compile produces from the source text
- [`DreamShaderModule.h`](dreamshader-module.md) — `NormalizeSourceFilePath` for building a request
- [Generation](../generation/index.md) — the pipeline the shipped implementation runs
- [In-memory materials](../generation/in-memory.md) — what `bTransient` means in practice
- [Caching](../generation/caching.md) — the hash check `bForce` bypasses
- [Commandlet](../tools/commandlet.md) — `-run=DreamShader`, the persisting caller
- [Editor bridge](../tools/bridge.md) — the memory-only caller and the JSON diagnostics
- [Preview](../tools/preview.md) — the forced memory-only material caller
- [Decompiler](../tools/decompiler.md) — the mirrored, editor-private abstraction
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
