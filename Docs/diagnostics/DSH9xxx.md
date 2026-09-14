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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGenerator.cpp:111`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGenerator.cpp:160`
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

**Raised by** `Source/DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGenerator.cpp:201`
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

## DSH9022

<!-- generated:begin DSH9022 -->
**Severity** error

**Message**

```
The IR dump could not be written: {0}.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderCompilerTools.cpp:550`
<!-- generated:end DSH9022 -->

**Cause.** `dump-ir` could not create its output directory or write the dump file. The message says
which. The default directory is `<Project>/Saved/DreamShader/IR`; `-Out=` overrides it.

**Fix.** Check that the directory is writable and that nothing holds the file open. A dump path is
`<out>/<root>/<path relative to that root>.ir.txt`, so a root whose display name contains a path
separator would produce an unexpected directory — that is worth checking if the path in the message
looks wrong.

## DSH9023

<!-- generated:begin DSH9023 -->
**Severity** error

**Message**

```
The symbol index could not be written: {0}.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderCompilerTools.cpp:638`
<!-- generated:end DSH9023 -->

**Cause.** `index` could not create its output directory or write the index file. The default
directory is `<Project>/Saved/DreamShader/Index`.

**Fix.** As DSH9022.

## DSH9024

<!-- generated:begin DSH9024 -->
**Severity** error

**Message**

```
The builtin catalog manifest could not be written: {0}.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderCompilerTools.cpp:673`
<!-- generated:end DSH9024 -->

**Cause.** `export-catalog` could not create its directory or write the manifest. The default path is
`<Project>/Saved/DreamShader/Bridge/dreamshader-builtin-catalog.json`, beside the other bridge
manifests.

**Fix.** As DSH9022. If the bridge is running, the directory certainly exists, so this points at a
lock or at a read-only `Saved/`.

## DSH9025

<!-- generated:begin DSH9025 -->
**Severity** error

**Message**

```
The builtin catalog came back empty, so '{0}' describes no expression at all. Reflection found no UMaterialExpression classes, which normally means the Engine module is not loaded.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderCompilerTools.cpp:687`
<!-- generated:end DSH9025 -->

**Cause.** The manifest was written and describes no expression at all. A language service that binds
against an empty catalog reports every `UE.*` name in the project as unknown, which reads as a
language bug rather than as a missing export — so this is an error even though the file exists.

The cause is the same as DSH8297's: reflection found no `UMaterialExpression` subclass.

**Fix.** See DSH8297.

## DSH9026

<!-- generated:begin DSH9026 -->
**Severity** error

**Message**

```
'{0}' is not a shader platform this engine knows. Write SM6, SM5, ES3_1, or a shader format name such as PCD3D_SM6.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderShaderCheck.cpp:411`
<!-- generated:end DSH9026 -->

**Cause.** `-Platform=` named something this engine has no shader platform for. The accepted
spellings are the short profiles (`SM6`, `SM5`, `ES3_1`), which map to `PCD3D_SM6`, `PCD3D_SM5` and
`PCD3D_ES3_1`, and any shader FORMAT name the engine knows (`PCD3D_SM6`, `SF_VULKAN_SM6`,
`SF_METAL_SM5`). Note that a format name is not the same string as the `EShaderPlatform` enumerator
name: `PCD3D_SM6`, not `SP_PCD3D_SM6`.

**Fix.** Use one of the short profiles, or the exact shader-format name. Several are comma
separated: `-Platform=SM6,SM5`.

## DSH9027

<!-- generated:begin DSH9027 -->
**Severity** error

**Message**

```
'{0}' is not a material quality level. Write Low, Medium, High or Epic.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderShaderCheck.cpp:428`
<!-- generated:end DSH9027 -->

**Cause.** `-Quality=` named something that is not a material quality level. There are four: `Low`,
`Medium`, `High`, `Epic`.

**Fix.** Use one of the four. Several are comma separated. Omitting the switch checks the project's
current scalability level, which is what an editor shows.

## DSH9028

<!-- generated:begin DSH9028 -->
**Severity** error

**Message**

```
Shader compilation for '{0}' did not finish within {1} seconds per material. A compile that never finishes is usually a dynamic loop or a texture read whose mip cannot be resolved in a divergent branch; move it into a '@custom' body with an explicit SampleLevel.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderShaderCheck.cpp:732`
<!-- generated:end DSH9028 -->

**Cause.** Shader compilation did not finish within the timeout — 120 seconds per material by
default, `-Timeout=` to change it. The run stops waiting and reports; it does not wait it out,
because a compile that never finishes is exactly the failure this gate exists to catch.

The two shapes that cause it are both real and both named in the message: a loop whose trip count
the compiler cannot bound, and a texture read whose mip level cannot be resolved because the read is
inside a divergent branch. Both make the shader compiler do unbounded work rather than fail.

**Fix.** Find the loop or the texture read. A loop belongs in a `@custom` body if its trip count is
not constant (the front end refuses it in the graph with DSH4360, so this one is nearly always an
HLSL loop already). A texture read in a branch needs an explicit level — `Tex.SampleLevel(UV, 0)`
rather than `Tex.Sample(UV)`.

## DSH9029

<!-- generated:begin DSH9029 -->
**Severity** error

**Message**

```
[{0} / {1}] {2}
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderShaderCheck.cpp:633`
<!-- generated:end DSH9029 -->

**Cause.** The shader compiler rejected the generated HLSL. The message is the compiler's own text,
prefixed with the shader platform and quality level it came from, and the diagnostic is located as
precisely as the material allows:

1. through the asset's `DreamShader.SourceSpans` table, when the error names an expression — exact
   file, line, column and length;
2. through the `// Begin/End DreamShader source:` markers inside a `@custom` node's code, by counting
   lines within the block, when the error names a line of generated HLSL;
3. at line 1 of the source file, with the compiler's raw text in `detail`, when neither worked.

A location of line 1 therefore means "this error belongs to this file but could not be pinned",
never "the mistake is on line 1".

**Fix.** Read the compiler's own message. The overwhelmingly common cause in a DreamShader source is
a symbol used in a `@custom` body that the graph does not define — a `UE.*` call inside a `@custom` body is not lifted to an input pin, so writing one there produces
an undeclared identifier here rather than a node.

## DSH9030

<!-- generated:begin DSH9030 -->
**Severity** error

**Message**

```
DreamShader failed to create graph dump directory '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1431`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1586`
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

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1440`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1597`
<!-- generated:end DSH9031 -->

**Cause.** the folder was there but the JSON could not be written into it. Almost always the file is open in another program, or read-only because a previous capture was committed to version control and checked out read-only

**Fix.** close whatever holds the file, or clear the read-only flag, and re-run. Capturing into a fresh empty directory sidesteps both -- a baseline is written whole, so there is nothing to preserve in an old one

**See** [Commandlet](../tools/commandlet.md#dump-graph)

## DSH9032

<!-- generated:begin DSH9032 -->
**Severity** error

**Message**

```
DreamShader could not compile '%s', so there is no graph to dump.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1385`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1394`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1493`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1542`
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

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1405`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1562`
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

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1419`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1574`
<!-- generated:end DSH9034 -->

**Cause.** the object at the source's asset path is not a class the dump covers -- not a `UMaterial`, not a `UMaterialFunction` (or layer / layer blend), and not a `UDreamShaderMaterialInstance`. Something else is squatting on the path the source resolves to; generation itself refuses to overwrite a foreign asset (`DSH8102` / `DSH8103` and friends)

**Fix.** look at the asset the message names and either move it aside or change the source's `Name=` / `Root=` so the two stop colliding. The dump is reporting the same clash generation would; it just gets there by a different route because it did not have to write

**See** [Commandlet](../tools/commandlet.md#dump-graph)

## DSH9035

<!-- generated:begin DSH9035 -->
**Severity** error

**Message**

```
'{0}' is not a 2.0 source, so '{1}' has nothing to do with it; '.dsm' and '.dsf' stay on 'compile'.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderCompilerTools.cpp:250`
<!-- generated:end DSH9035 -->

**Cause.** `check`, `dump-ir` or `index` was given a file that is not a `.dss`. These three verbs
drive the 2.0 pipeline; `.dsm` and `.dsf` belong to `compile`, which routes them to the 1.x
generator.

Note that `-All` never produces this: it enumerates `.dss` files only. It is always an explicit
`-Source=` or a positional path.

**Fix.** Point the verb at a `.dss`, or use `compile` for 1.x sources.

## DSH9036

<!-- generated:begin DSH9036 -->
**Severity** error

**Message**

```
The diagnostics JSON could not be written: {0}.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderCompilerTools.cpp:470`
<!-- generated:end DSH9036 -->

**Cause.** `-DiagnosticsOut=` was given and the JSON could not be written. With one source the path
is used as given; with `-All` it becomes a directory and one file is written per source, so that
each does not overwrite the last.

**Fix.** Check that the path is writable. With `-All`, pass a directory rather than a file name.

## DSH9037

<!-- generated:begin DSH9037 -->
**Severity** info

**Message**

```
'{0}' produced no material, so there are no shaders to compile; a function library is checked by the material that calls it.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderShaderCheck.cpp:512`
<!-- generated:end DSH9037 -->

**Cause.** Informational. `check -Shaders` was run on a source that produces no material — a file of
exported functions only. There is nothing to compile shaders for: a material function has no shader
of its own, it is compiled as part of every material that calls it.

**Fix.** Nothing. To cover a function library, check a material that calls it.

## DSH9038

<!-- generated:begin DSH9038 -->
**Severity** warning

**Message**

```
Shader errors cannot be read in this configuration: '-nullrhi' switches the rendering shader maps off, and no cook target platform matched the requested platforms. Re-run without '-nullrhi', or pass a '-Platform=' an active target platform supports.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderShaderCheck.cpp:741`
<!-- generated:end DSH9038 -->

**Cause.** `check -Shaders` ran in a configuration where no compile error can be read. `-nullrhi`
turns `FApp::CanEverRender()` off, which is what `UMaterial::CacheResourceShadersForRendering` is
gated on, so the rendering shader maps — the only ones whose `FMaterialResource` a plugin has a
public accessor for — are never built; and no cook target platform matched the requested platforms,
so the other half had nothing to drive either.

The run still reports a timeout, and still reports anything the engine logged, so it is not useless —
but it cannot promise that a silent run means a clean one.

**Fix.** Re-run without `-nullrhi` (`.skill/dsc.ps1` drops it for `check -Shaders` automatically;
pass `-NullRhi` to force it back on), or pass a `-Platform=` that one of the project's active target
platforms supports.

## DSH9039

<!-- generated:begin DSH9039 -->
**Severity** error

**Message**

```
%s: DSH9039: the DreamShader 2.0 pipeline failed without raising a diagnostic.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderCompilerPipeline.cpp:607`
<!-- generated:end DSH9039 -->

**Cause.** Internal invariant. The 2.0 pipeline returned failure without putting a single error in
its diagnostic sink, so there is nothing to report about the source. Every stage of the pipeline
raises before it refuses, so reaching this means a stage returned false on a path that does not.

**Fix.** Not a source problem — report it. The message names the file, which is enough to reproduce.
Running `dsc dump-ir` on the same file usually shows how far the run got, since that verb keeps
whatever the pipeline produced even when it failed.

## DSH9050

<!-- generated:begin DSH9050 -->
**Severity** error

**Message**

```
reveal-node needs a non-empty 'file' and a 'line' of 1 or more; got file '{File}' and line {Line}.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderSourceNavigation.cpp:885`
<!-- generated:end DSH9050 -->

**Cause.** A `reveal-node` request reached the bridge without the two fields it is made of: `file`
must be a non-empty source path and `line` must be 1 or more. A request that omits `line` entirely
reads as line 0 and lands here too, which is deliberate — "somewhere in this file" is not a node.

**Fix.** Send both fields. `file` may be absolute or project-relative; it is normalized the same way
the compiler normalizes a source path, so either spelling matches the same file. `line` is 1-based,
like every line number in the DreamShader wire, so an editor that counts lines from 0 must add one
before sending.

## DSH9051

<!-- generated:begin DSH9051 -->
**Severity** error

**Message**

```
No asset loaded in this editor was generated from '{File}', so there is no graph to reveal a node in; compile the file first, or open one of its assets once so the editor knows about it.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderSourceNavigation.cpp:908`
<!-- generated:end DSH9051 -->

**Cause.** Nothing currently loaded in the editor carries `DreamShader.SourceFile` metadata naming
that source. Either the file has never been compiled in this session, or it compiled to assets that
are on disk but have not been loaded — a package the asset registry knows about is still not a live
`UObject`, and the source-file stamp lives in package metadata, which cannot be queried without
loading the package.

**Fix.** Compile the file first (`recompile` with `scope: "file"`, *Recompile DSM*, or a save with
auto-compile on), or open one of its assets once so the package is loaded. If the file genuinely
produces no asset — a `.dsh` header, or a `.dss` whose products all live in another file — then
there is nothing to reveal and the request is aimed at the wrong file.

## DSH9052

<!-- generated:begin DSH9052 -->
**Severity** error

**Message**

```
The {Count} asset(s) generated from '{File}' carry no DreamShader.SourceSpans metadata, so no node on them can be located; they predate node navigation -- rebuild them with -Force.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderSourceNavigation.cpp:944`
<!-- generated:end DSH9052 -->

**Cause.** Assets generated from that source were found, but none of them carries a
`DreamShader.SourceSpans` table. The table is written by the 2.0 emitter only: an asset built by the
1.x generator, or by a 2.0 build from before node navigation landed, has the source-file stamp but
no per-node spans.

**Fix.** Rebuild the source with `-Force` (the source hash says the asset is current, so an ordinary
rebuild skips it and the table is never written). `.dsm` / `.dsf` sources will keep answering this
until M4 moves them onto the 2.0 pipeline; navigation is a `.dss` feature in this release.

## DSH9053

<!-- generated:begin DSH9053 -->
**Severity** error

**Message**

```
The DreamShader.SourceSpans metadata of '{Asset}' is not a JSON object, so no node on it can be mapped back to a source line; rebuild the asset from its source.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderSourceNavigation.cpp:387`
<!-- generated:end DSH9053 -->

**Cause.** The asset's `DreamShader.SourceSpans` metadata is present but is not a JSON object — it
failed to deserialize at all. Package metadata is a plain string map, so anything can be written
into that key; in practice this means the value was truncated or edited by hand.

**Fix.** Rebuild the asset from its source with `-Force`, which rewrites the whole table. If it comes
back, the value is being written by something other than the emitter — check for a tool or script
that stamps package metadata on generated assets.

## DSH9054

<!-- generated:begin DSH9054 -->
**Severity** error

**Message**

```
No node generated from '{File}' has a source span on line {Line}; the line produced no graph node (a declaration, a comment, or a statement that folded away).
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderSourceNavigation.cpp:953`
<!-- generated:end DSH9054 -->

**Cause.** The table was read and the file matched, but no node's span starts on that line, is
reached from a helper call on that line, or begins before it. Most lines of a real source are like
this: a `uniform` declaration that became a parameter node is on its own line, but a blank line, a
comment, a closing brace, a `struct` field, a `#pragma`, or a statement the constant folder removed
produce no node at all.

**Fix.** Aim at a line that carries an expression. Note that the dedupe pass merges two identical
expressions into one node whose span is the FIRST of them, so the second spelling of
a repeated expression has no node of its own and answers this.

## DSH9055

<!-- generated:begin DSH9055 -->
**Severity** error

**Message**

```
The material editor would not open for '{Asset}', so the node could not be revealed.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderSourceNavigation.cpp:1003`
<!-- generated:end DSH9055 -->

**Cause.** `UAssetEditorSubsystem::OpenEditorForAsset` was called for the asset and no editor came
back for it at all. The usual cause is an asset whose editor cannot open in this session: a
ThinCustom base material that is Ephemeral and has no package on disk, or an asset the editor is
mid-shutdown on. An editor that opens but is the *wrong* editor is DSH9056, not this.

**Fix.** Open the asset by hand once to see the real refusal, which the asset editor reports itself.
For a ThinCustom product, materialize it first — the hidden base is the thing with a graph, and an
Ephemeral base has no editor until it exists as an asset.

## DSH9056

<!-- generated:begin DSH9056 -->
**Severity** error

**Message**

```
'{Asset}' opened in an editor that is not the material editor, which has no node graph to select in; only a node of a Material or a Material Function can be revealed.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderSourceNavigation.cpp:1009`
<!-- generated:end DSH9056 -->

**Cause.** An editor opened for the asset, but not the material editor — so there is no node graph
to select in. A `UMaterialInstance` does this: it opens the material *instance* editor, which has a
parameter list and no graph, and is not an `IMaterialEditor`. The reveal path aims at the
graph-bearing asset on purpose, so reaching this means the asset carrying the span table is not the
kind of asset it claims to be.

**Fix.** Reveal in the Material or Material Function the source generates, not in an instance of it.
For a ThinCustom product the addressable asset is the instance but the graph is on its hidden base
material, and that base is what a `reveal-node` answer names in `assetPath` — the instance is
reported separately in `instanceAssetPath`. If `assetPath` already names a Material and this still
fires, the asset's class and its metadata disagree; rebuild it from source with `-Force`.

## DSH9057

<!-- generated:begin DSH9057 -->
**Severity** error

**Message**

```
The expression {Guid} recorded for line {Line} is not in '{Asset}' any more, so the node could not be selected; the asset changed since it was generated -- rebuild it from its source.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderSourceNavigation.cpp:1017`
<!-- generated:end DSH9057 -->

**Cause.** The span table names an expression GUID that the asset's graph no longer contains. The
table and the graph are written together, so this means one of them changed afterwards: a node was
deleted by hand, or the asset was reverted to an older version while its metadata was not.

**Fix.** Rebuild the asset from its source with `-Force`. If the asset was deliberately hand-edited,
resolve the divergence first (*Revert to Source*, *Adopt Into Source*, or *Detach From DreamShader*)
— a detached asset keeps neither the stamp nor the table, and stops answering navigation at all,
which is the correct outcome for an asset that is no longer generated.

## DSH9058

<!-- generated:begin DSH9058 -->
**Severity** error

**Message**

```
The source file '{File}' this node was generated from is not on disk any more, so it could not be opened; regenerate the asset, or restore the file.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderSourceNavigation.cpp:588`, `Source/DreamShaderEditor/Private/Compiler/DreamShaderSourceNavigation.cpp:594`
<!-- generated:end DSH9058 -->

**Cause.** *Open Source Line* (or *Open Call Site*) could not put a text editor on the file. Two
separate causes share the code because the user's next move is the same for both: the file named in
the span is not on disk any more, or every launcher in the chain refused — VSCode, the OS default
editor for the extension, and Notepad.

**Fix.** If the file is missing, the asset outlived its source: regenerate it, or restore the file
from version control. If the file is there, the launcher chain is the problem — check that the
extension has a default application, and see [Workspace](../../Docs/tools/workspace.md) for how
VSCode is discovered.

## DSH9059

<!-- generated:begin DSH9059 -->
**Severity** warning

**Message**

```
The DreamShader.SourceSpans entry '{Key}' of '{Asset}' is not an expression GUID and was skipped; that node cannot be navigated to.
```

**Raised by** `Source/DreamShaderEditor/Private/Compiler/DreamShaderSourceNavigation.cpp:402`, `Source/DreamShaderEditor/Private/Compiler/DreamShaderSourceNavigation.cpp:416`, `Source/DreamShaderEditor/Private/Compiler/DreamShaderSourceNavigation.cpp:440`
<!-- generated:end DSH9059 -->

**Cause.** One row of the `DreamShader.SourceSpans` table could not be read: its key is not a GUID,
its value is not an object, or the row names no file (or a line below 1). The row is skipped and the
rest of the table is used, so navigation still works for every other node — this is a warning
precisely because one bad row is not a reason to refuse the whole asset.

**Fix.** Rebuild the asset from its source with `-Force`. A row that keeps coming back malformed is
an emitter bug, not an authoring mistake: report it with the asset path, which the message carries.

