# DSH9xxx --- Tools, sync and internal invariants

> The block between the generated markers is written by `.skill/gen-diagnostics.ps1`.
> Everything below a marker is written by hand and survives a regeneration.

## DSH9001

<!-- generated:begin DSH9001 -->
**Severity** error

**Message**

```
DSH9001: '{0}' uses conditional compilation, and VirtualFunction sync rewrites a source in place at byte offsets taken from the file as written -- it cannot tell which of your branches a definition belongs to, and refuses rather than risk writing one branch over the others. Refresh these definitions by moving them into a source without directives, or edit them by hand.
```

**Raised by** `Source/DreamShaderEditor/Private/VirtualFunction/DreamShaderVirtualFunctionSyncService.cpp:339`
<!-- generated:end DSH9001 -->

**Cause.** the VirtualFunction startup sync found a source that uses conditional compilation and refused to touch it. The sync rewrites declarations in place at **byte offsets** recorded from the file as written; the preprocessor keeps line counts but not byte counts (a cut line becomes a shorter empty line), so every offset past the first cut is wrong, and a preprocessed string reaching the file writer would flatten every dead branch permanently. Unlike Adopt nobody asks for this -- it runs unattended at bridge startup across every writable source -- which is why it is a refusal rather than a prompt

**Fix.** keep `VirtualFunction` definitions in a source without directives, or refresh them by hand: **Copy VirtualFunction Definition** on the material function gives the current declaration to paste. The refusal is per file and does not stop the sync of the others

**See** [VirtualFunction tools](../tools/virtual-function-tools.md), [Preprocessor](../language/preprocessor.md)

## DSH9010

<!-- generated:begin DSH9010 -->
**Severity** error

**Message**

```
Generation of '%s' was cancelled by the user; the asset was left unchanged.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGenerator.cpp:108`
<!-- generated:end DSH9010 -->

**Cause.** you pressed **Cancel** on a generation progress dialog. DreamShader checks for that after every progress frame and stops at the first check that sees it, which is why the code can appear at any point in a compile -- while the source is being read, while the graph is being built, or while the shaders are being compiled

**Fix.** nothing to fix: this is the cancel doing what it says. The asset is left exactly as it was before the compile started -- the atomic-rebuild rollback restores the graph, and nothing is stamped or saved, so a cancelled rebuild can never leave a half-built material or an emptied material function behind. Recompile the source when you want the change. Two things a cancel does not undo: shaders Unreal had already dispatched keep compiling in `ShaderCompileWorker.exe`, and the generated `DreamShader: ` comment boxes on a material function stay gone until the next successful compile recreates them. If you did not press Cancel and see this code anyway, an automation test left the test-only cancel seam armed; it is never set in a normal editor session

**See** [Generation](../generation/index.md#progress-reporting)

## DSH9011

<!-- generated:begin DSH9011 -->
**Severity** warning

**Message**

```
Compiling shaders for '%s' took %.0f seconds. A stall of this length is almost always a Custom node whose loop bound is an input (a 'for' or 'while' whose limit is not a literal or a #define) combined with implicit-mip texture sampling -- Texture2DSample / Texture3DSample / .Sample inside divergent flow -- which forces the compiler to fully unroll an iteration count it cannot know. To confirm it is still working rather than hung, check whether ShaderCompileWorker.exe is busy in Task Manager. To fix it, bound the loop with a literal or a #define, or switch the samples to SampleLevel.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGenerator.cpp:157`
<!-- generated:end DSH9011 -->

**Cause.** the shader-compilation half of one asset's generation -- `UpdateStaticPermutation`, `PostEditChange`, the material recompile and the save -- ran for more than thirty seconds. Thirty seconds is not a performance budget; plenty of legitimate materials pass it on a cold shader cache. It is the point past which a still progress bar stops reading as "working" and starts reading as "hung", and this warning exists so that the difference is stated rather than guessed at

**Fix.** first decide whether anything is wrong. If `ShaderCompileWorker.exe` is busy in Task Manager the compile is running and the only question is how long it will take; a cold DDC, a Substrate material or a large permutation count can all legitimately cost minutes. If it is idle and nothing progresses, that is a different problem. When the wait is real and you want it shorter, the shape to look for is the one [DSH9012](#dsh9012) names: a `Custom` node whose loop bound is an input pin next to an implicit-mip texture sample. Bound the loop with a literal or a `#define`d constant, or call `SampleLevel` / `SampleGrad`, which take the mip level as an argument and do not force the unroll. Advisory; emitted once per asset; never fails a build

**See** [Generation](../generation/index.md#progress-reporting)

## DSH9012

<!-- generated:begin DSH9012 -->
**Severity** warning

**Message**

```
'%s' loops on the input '%s' and samples with '%s', which takes its mip level from screen-space derivatives. The shader compiler cannot know how many iterations to expect, so it fully unrolls the loop to keep the derivatives defined, and compilation can take minutes. Bound the loop with a literal or a #define, or call SampleLevel / SampleGrad instead.
```

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGenerator.cpp:198`
<!-- generated:end DSH9012 -->

**Cause.** the HLSL going onto a `Custom` node contains two things that are harmless alone and expensive together: a `for` or `while` whose bound is one of the node's **input pins** (an identifier the shader compiler cannot resolve to a constant, unlike a literal or a `#define`d name), and a sampling call whose mip level is **implicit** -- `Texture2DSample`, `Texture3DSample`, the other `Texture*Sample` helpers or a member `.Sample()`. Implicit-mip sampling derives the level from screen-space derivatives, which are undefined inside divergent control flow, so the compiler keeps them defined by fully unrolling the surrounding loop -- with no iteration count to unroll to. That is what turns a ray-march body into a multi-minute compile

**Fix.** break the pair. Either make the bound static -- a literal or a `#define`d constant, with the input used to `break` out early rather than to bound the loop -- or make the sampling explicit with `SampleLevel`, `SampleGrad` or `SampleBias`, which take the level (or the gradients) as an argument. In a ray march the second is almost always what you want anyway: the derivatives inside the loop are meaningless, and `SampleLevel(..., 0)` says so. The check is a heuristic: the sampling call is looked for anywhere in the body, not only inside the loop, and pin names match case-insensitively. It is a warning and never fails a build; if you have measured your shader and it compiles fine, ignore it

**See** [Generation](../generation/index.md#progress-reporting)

## DSH9020

<!-- generated:begin DSH9020 -->
**Severity** error

**Message**

```
Could not back up '%s' to '%s'; its asset references were left pointing at the old path.
```

**Raised by** `Source/DreamShaderEditor/Private/SourceFiles/DreamShaderAssetRenameSyncService.cpp:1149`
<!-- generated:end DSH9020 -->

**Cause.** an asset a source file references was renamed or moved, so the file had to be rewritten -- but the copy to `<file>.bak` failed, so there was nothing to fall back to. Usually the source (or the `.bak` next to it) is read-only, checked out by version control, held open by another process, or on a full or disconnected drive

**Fix.** nothing was written: the source still names the old path, and it will fail to load the asset at its next compile. Clear whatever blocked the copy -- check the file out, drop the read-only flag, close whatever holds `<file>.bak` -- and rename the asset back and forth, or edit the path by hand. Turning *Sync Source References On Asset Rename* off in Project Settings stops the plugin attempting this at all

**See** [Asset rename sync](../tools/asset-rename-sync.md#backups)

## DSH9021

<!-- generated:begin DSH9021 -->
**Severity** error

**Message**

```
Could not write '%s' after renaming an asset it references; the file as it was is in '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/SourceFiles/DreamShaderAssetRenameSyncService.cpp:1165`
<!-- generated:end DSH9021 -->

**Cause.** the backup was taken and then the rewritten source could not be saved over the original. The window between the two is small, so this is nearly always the file becoming unwritable in between -- a version-control lock, or an editor holding it open exclusively

**Fix.** the file as it was is in `<file>.bak`; the on-disk source is untouched, since the failing step is the write itself. Restore or ignore the `.bak`, make the file writable, and edit the path by hand or rename the asset back and forth to trigger the sync again

**See** [Asset rename sync](../tools/asset-rename-sync.md#backups)

## DSH9030

<!-- generated:begin DSH9030 -->
**Severity** error

**Message**

```
DreamShader failed to create graph dump directory '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1437`
<!-- generated:end DSH9030 -->

**Cause.** `dump-graph` could not create the folder the dump belongs in. The dump tree mirrors the source tree -- `<Out>/<root>/<source path>.<asset>.graph.json` -- so one folder is created per source subdirectory, and this is that `MakeDirectory` failing: a `-Out` under a drive that does not exist, a folder the process cannot write to, or a *file* sitting where the dump needs a directory

**Fix.** check the `-Out` path exists and is writable, and that no file already occupies one of the directory names the dump needs. With no `-Out` the destination is `<Project>/Saved/DreamShader/GraphBaseline`, which fails only if `Saved/` itself is read-only

**See** [Commandlet](../tools/commandlet.md#dump-graph)

## DSH9031

<!-- generated:begin DSH9031 -->
**Severity** error

**Message**

```
DreamShader failed to write graph dump '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1448`
<!-- generated:end DSH9031 -->

**Cause.** the folder was there but the JSON could not be written into it. Almost always the file is open in another program, or read-only because a previous capture was committed to version control and checked out read-only

**Fix.** close whatever holds the file, or clear the read-only flag, and re-run. Capturing into a fresh empty directory sidesteps both -- a baseline is written whole, so there is nothing to preserve in an old one

**See** [Commandlet](../tools/commandlet.md#dump-graph)

## DSH9032

<!-- generated:begin DSH9032 -->
**Severity** error

**Message**

```
%s: %s
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1345`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1394`
<!-- generated:end DSH9032 -->

**Cause.** the file was read and preprocessed, but it does not resolve to an asset to dump: either the parse failed, or the source declares no `Shader`, `ShaderFunction`, `ShaderLayer` or `ShaderLayerBlend` block at all. A `.dsh` header never reaches this point, but a `.dsm` that only declares `Function` or `VirtualFunction` bodies does -- those generate a `.ush` include or nothing, not a graph, so there is nothing for a fingerprint to describe

**Fix.** if the message carries a parse error, fix the source; `compile` on the same file reports the same failure with its own code. If the file is a helper that legitimately produces no asset, it has nothing to dump and can be left out of the capture -- `-All` visits it and reports it, which is why a `-All` run over a tree of helper sources exits `1` without anything being wrong with the tree

**See** [Commandlet](../tools/commandlet.md#dump-graph)

## DSH9033

<!-- generated:begin DSH9033 -->
**Severity** error

**Message**

```
DreamShader could not resolve generated asset '%s' from '%s' after generation.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1413`
<!-- generated:end DSH9033 -->

**Cause.** generation reported success, but the asset it should have produced could not be loaded back from the object path the source names. The usual cause is that the asset already exists on disk, `dump-graph`'s write guard refused to rebuild it, *and* it failed to load -- a broken or missing package behind a path the source still claims

**Fix.** run `compile -Force` on the source and see what the compiler says about the same path; a package that cannot be loaded fails there too, with a message about the package rather than about the dump. If the asset was deleted by hand, compiling it once recreates it

**See** [Commandlet](../tools/commandlet.md#dump-graph)

## DSH9034

<!-- generated:begin DSH9034 -->
**Severity** error

**Message**

```
DreamShader cannot dump '%s': %s is not a Material, MaterialFunction or DreamShader instance material.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1425`
<!-- generated:end DSH9034 -->

**Cause.** the object at the source's asset path is not a class the dump covers -- not a `UMaterial`, not a `UMaterialFunction` (or layer / layer blend), and not a `UDreamShaderMaterialInstance`. Something else is squatting on the path the source resolves to; generation itself refuses to overwrite a foreign asset (`DSH8102` / `DSH8103` and friends)

**Fix.** look at the asset the message names and either move it aside or change the source's `Name=` / `Root=` so the two stop colliding. The dump is reporting the same clash generation would; it just gets there by a different route because it did not have to write

**See** [Commandlet](../tools/commandlet.md#dump-graph)

