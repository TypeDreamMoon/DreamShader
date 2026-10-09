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

**Raised by** `Source/DreamShaderEditor/Private/VirtualFunction/DreamShaderVirtualFunctionSyncService.cpp:458`
<!-- generated:end DSH9001 -->

**Cause.** the VirtualFunction startup sync found a source that uses conditional compilation and refused to touch it. The sync rewrites declarations in place at **byte offsets** recorded from the file as written; the preprocessor keeps line counts but not byte counts (a cut line becomes a shorter empty line), so every offset past the first cut is wrong, and a preprocessed string reaching the file writer would flatten every dead branch permanently. Unlike Adopt nobody asks for this -- it runs unattended at bridge startup across every writable source -- which is why it is a refusal rather than a prompt

**Fix.** keep `VirtualFunction` definitions in a source without directives, or refresh them by hand: **Copy VirtualFunction Definition** on the material function gives the current declaration to paste. The refusal is per file and does not stop the sync of the others

**See** [VirtualFunction tools](../tools/virtual-function-tools.md), [Preprocessor](../language/preprocessor.md)

## DSH9011

<!-- generated:begin DSH9011 -->
**Severity** warning

**Message**

```
Compiling shaders for '%s' took %.0f seconds. A stall of this length is almost always a Custom node whose loop bound is an input (a 'for' or 'while' whose limit is not a literal or a #define) combined with implicit-mip texture sampling -- Texture2DSample / Texture3DSample / .Sample inside divergent flow -- which forces the compiler to fully unroll an iteration count it cannot know. To confirm it is still working rather than hung, check whether ShaderCompileWorker.exe is busy in Task Manager. To fix it, bound the loop with a literal or a #define, or switch the samples to SampleLevel.
```

**Raised by** `Source/DreamShaderCompiler/Private/DreamShaderCompilerService.cpp:252`
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

**Raised by** `Source/DreamShaderCompiler/Private/DreamShaderCompilerService.cpp:280`
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

**Raised by** `Source/DreamShaderEditor/Private/SourceFiles/DreamShaderAssetRenameSyncService.cpp:1147`
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

**Raised by** `Source/DreamShaderEditor/Private/SourceFiles/DreamShaderAssetRenameSyncService.cpp:1163`
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

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:559`
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

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:805`
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

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:1262`
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

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:1276`
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

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderShaderCheck.cpp:437`
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

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderShaderCheck.cpp:454`
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

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderShaderCheck.cpp:781`
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

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderShaderCheck.cpp:682`
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

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1888`
<!-- generated:end DSH9030 -->

**Cause.** `dump-graph` could not create the folder the dump belongs in. The dump tree mirrors the source tree -- `<Out>/<root>/<source path>.<asset>.graph.json` -- so one folder is created per source subdirectory, and this is that `MakeDirectory` failing: a `-Out` under a drive that does not exist, a folder the process cannot write to, or a *file* sitting where the dump needs a directory

**Fix.** check the `-Out` path exists and is writable, and that no file already occupies one of the directory names the dump needs. With no `-Out` the destination is `<Project>/Saved/DreamShader/GraphBaseline`, which fails only if `Saved/` itself is read-only

**See** [Commandlet](../tools/commandlet.md#dump-graph-since-190)

## DSH9031

<!-- generated:begin DSH9031 -->
**Severity** error

**Message**

```
DreamShader failed to write graph dump '%s'.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1899`
<!-- generated:end DSH9031 -->

**Cause.** the folder was there but the JSON could not be written into it. Almost always the file is open in another program, or read-only because a previous capture was committed to version control and checked out read-only

**Fix.** close whatever holds the file, or clear the read-only flag, and re-run. Capturing into a fresh empty directory sidesteps both -- a baseline is written whole, so there is nothing to preserve in an old one

**See** [Commandlet](../tools/commandlet.md#dump-graph-since-190)

## DSH9032

<!-- generated:begin DSH9032 -->
**Severity** error

**Message**

```
DreamShader could not work out which assets '%s' builds, so there is no graph to dump.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1792`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1801`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1840`
<!-- generated:end DSH9032 -->

**Cause.** the file was read and preprocessed, but it does not resolve to an asset to dump: either the parse failed, or the source declares no `Shader`, `ShaderFunction`, `ShaderLayer` or `ShaderLayerBlend` block at all. A `.dsh` header never reaches this point, but a `.dsm` that only declares `Function` or `VirtualFunction` bodies does -- those generate a `.ush` include or nothing, not a graph, so there is nothing for a fingerprint to describe

**Fix.** if the message carries a parse error, fix the source; `compile` on the same file reports the same failure with its own code. If the file is a helper that legitimately produces no asset, it has nothing to dump and can be left out of the capture -- `-All` visits it and reports it, which is why a `-All` run over a tree of helper sources exits `1` without anything being wrong with the tree

**See** [Commandlet](../tools/commandlet.md#dump-graph-since-190)

## DSH9033

<!-- generated:begin DSH9033 -->
**Severity** error

**Message**

```
DreamShader could not resolve generated asset '%s' from '%s' after generation.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1864`
<!-- generated:end DSH9033 -->

**Cause.** generation reported success, but the asset it should have produced could not be loaded back from the object path the source names. The usual cause is that the asset already exists on disk, `dump-graph`'s write guard refused to rebuild it, *and* it failed to load -- a broken or missing package behind a path the source still claims

**Fix.** run `compile -Force` on the source and see what the compiler says about the same path; a package that cannot be loaded fails there too, with a message about the package rather than about the dump. If the asset was deleted by hand, compiling it once recreates it

**See** [Commandlet](../tools/commandlet.md#dump-graph-since-190)

## DSH9034

<!-- generated:begin DSH9034 -->
**Severity** error

**Message**

```
DreamShader cannot dump '%s': %s is not a Material, MaterialFunction, material instance or pass pipeline.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderGraphDump.cpp:1876`
<!-- generated:end DSH9034 -->

**Cause.** the object at the source's asset path is not a class the dump covers -- not a `UMaterial`, not a `UMaterialFunction` (or layer / layer blend), and not a `UDreamShaderMaterialInstance`. Something else is squatting on the path the source resolves to; generation itself refuses to overwrite a foreign asset (`DSH8102` / `DSH8103` and friends)

**Fix.** look at the asset the message names and either move it aside or change the source's `Name=` / `Root=` so the two stop colliding. The dump is reporting the same clash generation would; it just gets there by a different route because it did not have to write

**See** [Commandlet](../tools/commandlet.md#dump-graph-since-190)

## DSH9035

<!-- generated:begin DSH9035 -->
**Severity** error

**Message**

```
'{0}' is not a compilable DreamShader source (.dss, .dsi, .dsp, .dsm or .dsf), so '{1}' has nothing to do with it; a .dsh header is checked through a source that includes it.
```

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:214`
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

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderCommandletRunner.cpp:800`, `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:473`
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

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderShaderCheck.cpp:561`
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

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderShaderCheck.cpp:790`
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

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:1225`
<!-- generated:end DSH9039 -->

**Cause.** Internal invariant. The 2.0 pipeline returned failure without putting a single error in
its diagnostic sink, so there is nothing to report about the source. Every stage of the pipeline
raises before it refuses, so reaching this means a stage returned false on a path that does not.

**Fix.** Not a source problem — report it. The message names the file, which is enough to reproduce.
Running `dsc dump-ir` on the same file usually shows how far the run got, since that verb keeps
whatever the pipeline produced even when it failed.

## DSH9040

<!-- generated:begin DSH9040 -->
**Severity** error

**Message**

```
The layout dump could not be written: {0}.
```

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:708`
<!-- generated:end DSH9040 -->

**Cause.** `dsc dump-layout` could not write one of its files -- the SVG, or the JSON beside it
under `-Json`. The message carries the file system's reason.

**Fix.** Point `-Out` at a directory that can be written, and close whatever holds the file open.

## DSH9041

<!-- generated:begin DSH9041 -->
**Severity** error

**Message**

```
'{0}' is not a layout style; -Style takes Blocks, SourceBands, Layered or All.
```

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:631`
<!-- generated:end DSH9041 -->

**Cause.** `dsc dump-layout -Style` was given something other than `Blocks`, `SourceBands`, `Layered`
or `All`. `Classic`, the 1.x layout, is not among them: it works on the finished graph and cannot be
drawn from the IR.

**Fix.** Leave `-Style` out for one picture of each, or name one of the three.

## DSH9042

<!-- generated:begin DSH9042 -->
**Severity** info

**Message**

```
'{0}' has 1.x declarations, and what the printer writes for those is 2.0 text; rewriting 1.x as 2.0 is 'dsc migrate', so 'fmt' leaves the file as it is.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangFormat.cpp:108`
<!-- generated:end DSH9042 -->

**Cause.** `dsc fmt` met a header that still has 1.x declarations. The printer writes 2.0 text, so
formatting such a file would migrate it by the back door -- without the checks `dsc migrate` runs.
Information, not an error: the file is left as it is and the run goes on.

**Fix.** Nothing. Run `dsc migrate` when the header is to become 2.0; `fmt` formats it from then on.

## DSH9043

<!-- generated:begin DSH9043 -->
**Severity** info

**Message**

```
'{0}' uses the preprocessor outside a custom body; 'fmt' reads the file as it is on disk and would have to drop one side of every '#if', so it leaves the file as it is.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangFormat.cpp:94`
<!-- generated:end DSH9043 -->

**Cause.** `dsc fmt` met a file that uses `#if`, `#ifdef` or their kin outside a `/// @custom` body.
The formatter reads the file as it is on disk, without preprocessing, and the parser keeps only the
text between directives -- it would have to drop one side of every conditional. Information, not an
error: the file is left as it is.

**Fix.** Nothing. Format such a file by hand, or move the conditional part into a header of its own.

## DSH9044

<!-- generated:begin DSH9044 -->
**Severity** error

**Message**

```
The formatted text of '{0}' failed its own check -- {1} -- so nothing was written. This is a fault of the formatter, not of the file.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangFormat.cpp:130`
<!-- generated:end DSH9044 -->

**Cause.** The text `dsc fmt` produced failed the formatter's own check: it has to parse, parse to
the same declarations as the original, keep every `//` and `/* */` comment, and come out unchanged
when formatted again. Nothing was written. This is a fault of the formatter, never of the file.

**Fix.** Report it with the file. The message names which of the four checks failed.

## DSH9045

<!-- generated:begin DSH9045 -->
**Severity** error

**Message**

```
'{0}' could not be read, so it was not formatted.
```

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:915`, `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:959`
<!-- generated:end DSH9045 -->

**Cause.** `dsc fmt` could not read a source, or could not write the formatted text back. A file
that is read-only -- checked in to Perforce and not checked out -- is the usual reason for the
second.

**Fix.** Check the file out, or make it writable, and run `fmt` again. `fmt -Check` reads only.

## DSH9046

<!-- generated:begin DSH9046 -->
**Severity** error

**Message**

```
'{0}' is not in the formatter's layout; 'dsc fmt' would rewrite it.
```

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:949`
<!-- generated:end DSH9046 -->

**Cause.** `dsc fmt -Check` found a file that `fmt` would rewrite. `-Check` writes nothing and fails
the run, which is the form for CI.

**Fix.** Run `dsc fmt` on the file (or `-All`) and commit the result.

## DSH9047

<!-- generated:begin DSH9047 -->
**Severity** error

**Message**

```
The list of generated assets could not be written: {0}.
```

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:1222`
<!-- generated:end DSH9047 -->

**Cause.** `dsc list-generated -Out` could not write the list. The message carries the file system's
reason.

**Fix.** Point `-Out` at a file that can be written; a relative path is taken against the directory
`dsc.ps1` was called from.

## DSH9048

<!-- generated:begin DSH9048 -->
**Severity** error

**Message**

```
'-As={0}' is no list format; the four are Packages, Files, GitIgnore and Json.
```

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:1052`
<!-- generated:end DSH9048 -->

**Cause.** `dsc list-generated -As` (`-ListAs` in `dsc.ps1`) was given something other than the four
formats.

**Fix.** `Packages` for package names, `Files` for project-relative files, `GitIgnore` for a ready
ignore block, `Json` for everything.

## DSH9049

<!-- generated:begin DSH9049 -->
**Severity** warning

**Message**

```
{0} generated asset(s) lie outside the project directory -- an engine plugin's content -- and have no project-relative path; '-As=Packages' or '-As=Json' lists them.
```

**Raised by** `Source/DreamShaderEditor/Private/Tools/DreamShaderCompilerTools.cpp:1210`
<!-- generated:end DSH9049 -->

**Cause.** Some generated assets lie outside the project directory -- the content of a plugin
installed in the engine -- so they have no project-relative path and were left out of a `Files` or
`GitIgnore` list. A warning: the rest of the list is complete.

**Fix.** Use `-As=Packages` or `-As=Json` to see them. Ignore rules for an engine plugin's content
belong in that plugin's repository.

## DSH9050

<!-- generated:begin DSH9050 -->
**Severity** error

**Message**

```
reveal-node needs a non-empty 'file' and a 'line' of 1 or more; got file '{File}' and line {Line}.
```

**Raised by** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:885`
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

**Raised by** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:908`
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

**Raised by** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:944`
<!-- generated:end DSH9052 -->

**Cause.** Assets generated from that source were found, but none of them carries a
`DreamShader.SourceSpans` table. The table is written by the 2.0 emitter only: an asset built by the
1.x generator, or by a 2.0 build from before node navigation landed, has the source-file stamp but
no per-node spans.

**Fix.** Rebuild the source with `-Force` (the source hash says the asset is current, so an ordinary
rebuild skips it and the table is never written). A `.dsm` / `.dsf` source is built by the same
emitter and gets the table too; only an asset the 1.x generator left behind answers this.

## DSH9053

<!-- generated:begin DSH9053 -->
**Severity** error

**Message**

```
The DreamShader.SourceSpans metadata of '{Asset}' is not a JSON object, so no node on it can be mapped back to a source line; rebuild the asset from its source.
```

**Raised by** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:387`
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

**Raised by** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:953`
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

**Raised by** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:1003`
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

**Raised by** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:1009`
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

**Raised by** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:1017`
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

**Raised by** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:588`, `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:594`
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

**Raised by** `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:402`, `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:416`, `Source/DreamShaderEditor/Private/Navigation/DreamShaderSourceNavigation.cpp:440`
<!-- generated:end DSH9059 -->

**Cause.** One row of the `DreamShader.SourceSpans` table could not be read: its key is not a GUID,
its value is not an object, or the row names no file (or a line below 1). The row is skipped and the
rest of the table is used, so navigation still works for every other node — this is a warning
precisely because one bad row is not a reason to refuse the whole asset.

**Fix.** Rebuild the asset from its source with `-Force`. A row that keeps coming back malformed is
an emitter bug, not an authoring mistake: report it with the asset path, which the message carries.

## DSH9060

<!-- generated:begin DSH9060 -->
**Severity** error

**Message**

```
'{0}' is neither a material nor a material function, so it has no graph to read. A material instance decompiles to a '.dsi'.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:3101`
<!-- generated:end DSH9060 -->

**Cause.** The asset handed to the 2.0 decompiler is neither a material nor a material function
(layers and blends included), so there is no expression graph to read. A plain material instance is
handled by the instance decompiler and comes out as a `.dsi`.

**Fix.** Decompile the material or function itself; for an instance use the same command, which
routes it to `.dsi` output on its own.

## DSH9061

<!-- generated:begin DSH9061 -->
**Severity** info

**Message**

```
{0} expression(s) of '{1}' feed no output and are not part of the source; a build would prune them all the same.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2894`
<!-- generated:end DSH9061 -->

**Cause.** The graph holds expressions that feed no output: leftovers of editing. A source cannot
say 'a node nothing reads' -- the compiler would prune it again -- so they are left out.

**Fix.** Nothing to fix. Wire them to something in the asset first if they are meant to survive.

## DSH9062

<!-- generated:begin DSH9062 -->
**Severity** warning

**Message**

```
The reroute {0} feeds itself; the pin that reads it is treated as unconnected.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:511`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:605`
<!-- generated:end DSH9062 -->

**Cause.** The graph has a cycle: a Reroute (or named reroute) chain leads back into itself, or a
node reads a node that reads it back. The engine compiles such a wire as unconnected; the decompiler
treats the reroute pin as unconnected and writes a zero for the read that closes a node cycle.

**Fix.** Delete the looping wire in the material editor and decompile again.

## DSH9063

<!-- generated:begin DSH9063 -->
**Severity** warning

**Message**

```
The named reroute {0} has no declaration; the pin that reads it is treated as unconnected.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1666`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1939`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1975`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:487`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:525`
<!-- generated:end DSH9063 -->

**Cause.** A node reads something that is not there: a NamedRerouteUsage whose declaration no longer
exists in the graph (deleted, or lost in a copy from another material), a Reroute with nothing wired
into it, or a Break/GetMaterialAttributes node with no material wired in. A Convert node says it too,
for a mapping onto or out of a pin or channel the node does not have, and for a channel read past the
width of the value wired to its input -- both of which the engine refuses to compile.

**Fix.** Reconnect or delete the node in the asset. The pin that read a reroute decompiles as
unconnected; a read of no material's attributes is written as zero. A Convert mapping the node has no
pin for is left out, so that channel takes the output's default; a channel past the wired value is a
zero, which is what the new material translator reads there.

## DSH9064

<!-- generated:begin DSH9064 -->
**Severity** warning

**Message**

```
{0} calls a material function that is missing; a zero stands in for every value read from it.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1419`
<!-- generated:end DSH9064 -->

**Cause.** A MaterialFunctionCall node's function asset is missing (deleted, or its plugin is not
mounted). There is no interface to read pins from, so every value read from the call is written as
zero to keep the rest of the text compilable.

**Fix.** Restore the function asset, or fix the call in the material, and decompile again.

## DSH9065

<!-- generated:begin DSH9065 -->
**Severity** warning

**Message**

```
{0} is of a class the builtin catalog does not list (abstract, deprecated, or from a module loaded after the catalog was built); a zero stands in for every value read from it.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2351`
<!-- generated:end DSH9065 -->

**Cause.** An expression is of a class the builtin catalog does not list -- abstract, deprecated, or
from a module that was loaded after the catalog was built. The language can only name classes the
catalog has, so a zero stands in for the node's values.

**Fix.** If the class comes from a plugin, make sure the plugin is loaded before DreamShader builds
its catalog (restart after enabling it), then decompile again.

## DSH9066

<!-- generated:begin DSH9066 -->
**Severity** warning

**Message**

```
The input '{0}' of '{1}' is a dynamic bool pin, which the language has no parameter for; it is written as 'bool' and rebuilds as a scalar pin.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1390`
<!-- generated:end DSH9066 -->

**Cause.** A function input is of the engine's dynamic `Bool` pin type. The language has `bool`
parameters that are scalar pins, and `/// @static` ones that are StaticBool pins; it has no spelling
for the dynamic bool pin.

**Fix.** Accept the scalar pin (callers may need reconnecting once), or change the input's type in
the asset to StaticBool or Scalar before decompiling.

## DSH9067

<!-- generated:begin DSH9067 -->
**Severity** warning

**Message**

```
{0} carries {1} additional define(s), which a '/// @custom' function cannot declare; the rebuilt node has none. Move them into the body as '#define' lines.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1556`
<!-- generated:end DSH9067 -->

**Cause.** A Custom node carries Additional Defines. A `/// @custom` function declares inputs,
outputs and code, and has no directive for defines.

**Fix.** Move each define into the function body as a `#define` line.

## DSH9068

<!-- generated:begin DSH9068 -->
**Severity** warning

**Message**

```
{0} changes '{1}', which a '///' directive cannot carry; the rebuilt parameter has the default.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1069`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2455`
<!-- generated:end DSH9068 -->

**Cause.** A node changes a property the text cannot carry: on a parameter, one that no `///`
directive stands for (a channel name, a curve atlas, a custom primitive data index ...); on any
other node, a property of a kind a call argument cannot hold (an array of pins, a map, a delegate).
An array of structs is written as the text ImportText reads, without the pins in it
(`Layers = "((LayerName=\"Grass\"))"`). The rebuilt node has the engine default for it.

**Fix.** Set that property on the rebuilt asset by hand, or keep this one node out of the source
(hand-made function + `extern`).

## DSH9069

<!-- generated:begin DSH9069 -->
**Severity** info

**Message**

```
{0} has nothing wired to Value, so it is its {1} branch and nothing else; the rebuilt graph has no switch there.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1793`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2170`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2181`
<!-- generated:end DSH9069 -->

**Cause.** A StaticSwitch has nothing wired to its `Value` pin, so it always takes the branch its
`DefaultValue` names. The source says that branch directly; the rebuilt graph has no switch there.
The same goes for a Switch with nothing wired to `SwitchValue`, whose `ConstSwitchValue` picks one
input (or the default) for good, and for a Switch with no input besides its default.

**Fix.** Nothing to fix; wire a static bool to `Value` (a value to `SwitchValue`) in the asset if the
switch is meant to be switchable.

## DSH9070

<!-- generated:begin DSH9070 -->
**Severity** warning

**Message**

```
{0} has {1} wired input(s) the class does not declare as named pins, so a call cannot connect them; the rebuilt node leaves them unconnected.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2414`
<!-- generated:end DSH9070 -->

**Cause.** A node has inputs wired that its class does not declare as named pins (dynamic inputs of
a class the catalog reads by reflection). A call in source connects pins by name, so there is no way
to write those wires. A class the catalog knows to name its pins per node (a Landscape layer blend,
grass or physical-material output) is written with the name the node shows instead; this is said
only for a pin whose name is empty, a keyword, or one another pin or argument also answers to.

**Fix.** Rebuild that part by hand after decompiling; the message names the node.

## DSH9071

<!-- generated:begin DSH9071 -->
**Severity** warning

**Message**

```
The input '{0}' of '{1}' has its Preview pin wired. Source says that as a default that is an expression ('float {0} = UE.TexCoord(Index = 1).r'), which the decompiler does not write yet; the rebuilt input previews its number.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1397`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:3024`
<!-- generated:end DSH9071 -->

**Cause.** A material function states something about its interface that source does not write yet.
A FunctionInput has its Preview pin wired: the input's default is a graph expression, which source
says as a parameter default that is an expression -- the compiler builds it, the decompiler does not
write it yet, and the rebuilt input previews its number. Or the function has a user-exposed caption,
which no directive carries.

**Fix.** Write the default by hand in the decompiled text: `float Name = <expression>`. A caption
has to be set on the rebuilt asset.

## DSH9072

<!-- generated:begin DSH9072 -->
**Severity** warning

**Message**

```
{0} is read through its output {1}, which is no material attribute the catalog knows; a zero stands in for that read.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1597`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1628`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:695`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:728`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:735`
<!-- generated:end DSH9072 -->

**Cause.** A node is used through something the catalog does not know: a
Break/GetMaterialAttributes output that is no material attribute (added by a plugin, or an index
past the table) is read as zero; an output index the node has no slot for is read as the node's
first output, and a read of a node whose class lists no output at all is read as zero; a
Set/MakeMaterialAttributes input that is no known attribute is dropped.

**Fix.** Check whether the attribute belongs to a plugin that is not loaded; otherwise rewire the
node in the asset.

## DSH9073

<!-- generated:begin DSH9073 -->
**Severity** info

**Message**

```
{0} is a StaticSwitchParameter, which the language writes as a '/// @static' uniform and a static branch; the rebuilt graph has those two nodes in its place.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1191`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1329`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:1716`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2064`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2227`
<!-- generated:end DSH9073 -->

**Cause.** One engine node the language writes as two, or under another class. A
StaticSwitchParameter is both a parameter and a switch: source says a `/// @static uniform bool` and
a static branch, and a build makes a StaticBoolParameter plus a StaticSwitch. A texture sample
parameter becomes a texture uniform and a sample of it (TextureObjectParameter + TextureSample). A
GetMaterialAttributes node rebuilds as a BreakMaterialAttributes. A Convert node (Make / Break
FloatN) and a Switch with an unnamed case keep their pins in arrays no call can name, so they are
written as what they compute (a Switch whose cases are all named is written as the Switch itself): each Convert output as the channels it is put together from (ComponentMask and AppendVector
nodes), a Switch as the branches the engine makes of it (Floor and If nodes). Instances keep
working: parameter names and static permutations are the same.

**Fix.** Nothing to fix.

## DSH9074

<!-- generated:begin DSH9074 -->
**Severity** info

**Message**

```
'{0}' reads its attributes as one set, so the {1} individual pin(s) that are also wired are ignored, by the engine and here.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderGraphImport.cpp:2985`
<!-- generated:end DSH9074 -->

**Cause.** A material with *Use Material Attributes* on reads its attributes from the one
MaterialAttributes pin; the individual attribute pins that are also wired are ignored by the engine,
and so by the decompiler.

**Fix.** Nothing to fix; disconnect the dead wires in the asset to stop the note.

## DSH9075

<!-- generated:begin DSH9075 -->
**Severity** info

**Message**

```
The uniform '{0}' is written as '{1}': the name is already taken in this file.
```

**Raised by** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1277`, `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:664`, `Source/DreamShaderLang/Private/Decompile/IRToAstStatements.cpp:69`
<!-- generated:end DSH9075 -->

**Cause.** Something in the decompiled file could not keep its name: a parameter whose name is taken
by another declaration, a local named like a parameter or not an identifier the language allows, a
custom function whose name is taken. It is written under another name; a uniform keeps its real
parameter name through `/// @name`.

**Fix.** Nothing to fix for uniforms and locals. A custom function that was renamed may still be
called by its old name from inside another custom body, which is verbatim HLSL: change that call by
hand.

## DSH9076

<!-- generated:begin DSH9076 -->
**Severity** warning

**Message**

```
A region of '{0}' is titled '{1}', which does not fit on a '#pragma region' line; it is written as '{2}'.
```

**Raised by** `Source/DreamShaderLang/Private/Decompile/IRToAstStatements.cpp:575`
<!-- generated:end DSH9076 -->

**Cause.** A comment box's title cannot stand on a `#pragma region` line as it is (it has line
breaks, or is empty). It is written in a form that can.

**Fix.** Nothing to fix; edit the region's title in the text if you like.

## DSH9077

<!-- generated:begin DSH9077 -->
**Severity** info

**Message**

```
{0} node position(s) of '{1}' belong to values the source writes inline, and a position is kept by variable name; those nodes are placed by the layout pass when the file is built.
```

**Raised by** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1698`, `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1927`
<!-- generated:end DSH9077 -->

**Cause.** Some node positions are not kept. A position is stored by variable name (`#pragma
layout(Node, Var = ...)`) for the whole file: a value the source writes inline has no name to hang
one on, and a name two functions of the file place differently is ambiguous. Those nodes are placed
by the layout pass on the next build.

**Fix.** Nothing to fix. Give a value a variable of its own to keep its position.

## DSH9078

<!-- generated:begin DSH9078 -->
**Severity** error

**Message**

```
The output '{0}' of '{1}' did not become a parameter; nothing is written to it.
```

**Raised by** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1837`, `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1889`, `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1894`, `Source/DreamShaderLang/Private/Decompile/IRToAstExpressions.cpp:1098`, `Source/DreamShaderLang/Private/Decompile/IRToAstExpressions.cpp:801`, `Source/DreamShaderLang/Private/Decompile/IRToAstStatements.cpp:121`, `Source/DreamShaderLang/Private/Decompile/IRToAstStatements.cpp:319`, `Source/DreamShaderLang/Private/Decompile/IRToAstStatements.cpp:360`
<!-- generated:end DSH9078 -->

**Cause.** Something in the graph has no place in the text being written: a function output that did
not become a parameter, an attribute or pin whose name is not one the language has, an input that is
neither a pin of its class nor a name an argument can carry, a node that cannot be written as source
at all (`0.0` stands in its place), or a material instance handed to the `.dss` writer (it is
written as a `.dsi`).

**Fix.** Rename the pin, output or attribute in the asset to an identifier and decompile again. For
a node that cannot be written, the message says why.

## DSH9079

<!-- generated:begin DSH9079 -->
**Severity** info

**Message**

```
{0} is an If whose branches no comparison selects between; it is written as 'UE.If(...)'.
```

**Raised by** `Source/DreamShaderLang/Private/Decompile/IRToAstExpressions.cpp:1406`
<!-- generated:end DSH9079 -->

**Cause.** An If node's A/B inputs are not a comparison the language can write as `a > b ? x : y`
(its three branches differ in a way no single operator selects). It is written as the node call
`UE.If(...)`, which builds the same node.

**Fix.** Nothing to fix.

## DSH9080

<!-- generated:begin DSH9080 -->
**Severity** warning

**Message**

```
The parameter '{0}' appears more than once and its nodes do not agree on its default, group or kind; the uniform is written from the first one, in '{1}'.
```

**Raised by** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:549`
<!-- generated:end DSH9080 -->

**Cause.** A parameter name is used by more than one node and the nodes disagree on default, group
or kind. The engine takes one of them; the uniform is written from the first one met.

**Fix.** Make the nodes agree in the asset, or check that the written default is the one you want.

## DSH9081

<!-- generated:begin DSH9081 -->
**Severity** warning

**Message**

```
'{0}' is called and its interface was not available, so its 'extern' prototype is written from the calls alone: pins no call connects are missing from it, and their order is the order the calls wire them in.
```

**Raised by** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1136`
<!-- generated:end DSH9081 -->

**Cause.** The text calls a material function whose asset was not available to read an interface
from (a Core-only decompile, or a missing asset), so the `extern` prototype is inferred from the
calls: pins no call connects are missing, their order is the order of first use, and defaults are
unknown.

**Fix.** Decompile in the editor with the function asset present, or correct the prototype by hand
against the asset's pins.

## DSH9082

<!-- generated:begin DSH9082 -->
**Severity** warning

**Message**

```
The code of the custom node '{0}' carries DreamShader's markers and does not read as what the compiler writes; it is kept verbatim as the body of one function, the functions it embeds included.
```

**Raised by** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1220`, `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1431`
<!-- generated:end DSH9082 -->

**Cause.** A Custom node's code could not be taken apart cleanly. Either it carries DreamShader's
begin/end markers and does not parse as what the code builder writes -- it was edited by hand after
generation -- and is kept verbatim as the body of one function, embedded helpers included; or a pin
has a name the language cannot declare, so the parameter is written under another name while the
body still says the old one.

**Fix.** For the first, nothing to fix if the code is right; tidy the helpers out into functions of
their own by hand if you want them shared. For the second, rename the identifier in the body to
match the parameter.

## DSH9083

<!-- generated:begin DSH9083 -->
**Severity** warning

**Message**

```
'{0}' is a {1}, and its pins are not the ones the language writes that kind with; it is written as a plain exported function, which builds a material function.
```

**Raised by** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:950`, `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:959`
<!-- generated:end DSH9083 -->

**Cause.** The asset is a material layer or blend whose pins are not the shape the language writes
that kind with (a layer with extra inputs, a blend without exactly two material inputs). It is
written as a plain exported function, which builds a MaterialFunction rather than a layer asset.

**Fix.** Give the layer/blend the standard pins in the asset, or keep that asset hand-made and
declare it `extern`.

## DSH9084

<!-- generated:begin DSH9084 -->
**Severity** warning

**Message**

```
The output '{0}' of '{1}' is not connected to anything; nothing is written to it.
```

**Raised by** `Source/DreamShaderLang/Private/Decompile/IRToAst.cpp:1814`, `Source/DreamShaderLang/Private/Decompile/IRToAstExpressions.cpp:1126`, `Source/DreamShaderLang/Private/Decompile/IRToAstExpressions.cpp:1149`, `Source/DreamShaderLang/Private/Decompile/IRToAstExpressions.cpp:1506`, `Source/DreamShaderLang/Private/Decompile/IRToAstExpressions.cpp:1545`, `Source/DreamShaderLang/Private/Decompile/IRToAstStatements.cpp:204`
<!-- generated:end DSH9084 -->

**Cause.** Something is left at a default because the graph or the language gives nothing to write:
a function output, custom-output pin or material attribute with nothing wired to it; a node property
that is a list, or whose name an argument cannot carry (left at the class default); a required input
left unconnected (`0.0` is passed); an `inout` input left unconnected (the variable starts at zero);
a wired input its class does not declare (dropped).

**Fix.** Nothing to fix unless the output or input was meant to carry a value; then wire it in the
asset and decompile again.

## DSH9085

<!-- generated:begin DSH9085 -->
**Severity** error

**Message**

```
'{0}' ends in '.{1}', which is read as {2} source, and the decompile was asked for {3} text; name the file after the text, or leave the format to the extension.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderDecompileService.cpp:295`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderIRDecompiler.cpp:212`
<!-- generated:end DSH9085 -->

**Cause.** The output file's extension and the text asked for disagree: `.dsm` / `.dsf` are read as
1.x text, anything else as 2.0, and the decompile was asked for the other one; or the asset is a
material instance, which decompiles to a `.dsi` and to nothing else.

**Fix.** Name the file after the text you want, or leave `-Format` at `Auto` (and `-Out` off) and
let the asset decide.

## DSH9086

<!-- generated:begin DSH9086 -->
**Severity** error

**Message**

```
The decompile was asked for 2.0 text and was handed the 1.x decompiler, which writes '.dsm' and '.dsf' only; build the service with GetIRDecompiler() for Format = Dss.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderDecompileService.cpp:312`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderIRDecompiler.cpp:356`
<!-- generated:end DSH9086 -->

**Cause.** The decompile cannot take what it was handed. From the command line: the asset is of a
class no DreamShader source describes -- a decompile takes a material, a material function, layer or
blend, or a material instance. From code: a request for 2.0 text was routed to the 1.x decompiler,
which writes `.dsm` / `.dsf` only.

**Fix.** Pass one of the supported asset classes. The second case is a wiring error in the caller:
build the service with `GetIRDecompiler()` for `Format = Dss`, and report it if it comes from the
editor.

## DSH9087

<!-- generated:begin DSH9087 -->
**Severity** error

**Message**

```
'{0}' does not resolve to the assets it builds, so there is nothing to decompile for it.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderIRDecompiler.cpp:297`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderIRDecompiler.cpp:328`
<!-- generated:end DSH9087 -->

**Cause.** `-SourceFile` names a source, and the assets it builds could not be resolved: the source
does not compile far enough to list its products, or none of them exists yet.

**Fix.** Compile the source first, or name the asset directly.

## DSH9088

<!-- generated:begin DSH9088 -->
**Severity** error

**Message**

```
The graph of '{0}' did not read into a valid module; the errors above say where. This is a defect of the decompiler, not of the asset.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderIRDecompiler.cpp:428`
<!-- generated:end DSH9088 -->

**Cause.** The imported graph failed IR validation. The importer produced something the compiler's
own rules reject, which is a defect of the decompiler; the errors above it say what.

**Fix.** Report it with the asset. `-Format Legacy` still gives 1.x text for the same asset.

## DSH9089

<!-- generated:begin DSH9089 -->
**Severity** warning

**Message**

```
The decompiled text does not parse back: {0}: {1}. It is written as it is; this is a defect of the decompiler.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderIRDecompiler.cpp:483`
<!-- generated:end DSH9089 -->

**Cause.** The text the decompiler printed does not parse back. It is written anyway so that nothing
is lost; the message names the first parse error.

**Fix.** Report it with the asset; fixing the named line by hand usually gets a working file.

## DSH9090

<!-- generated:begin DSH9090 -->
**Severity** error

**Message**

```
'{0}' fails conditional compilation ({1}: {2}), and a source with '#if' lines is not migrated in any case.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:104`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:115`
<!-- generated:end DSH9090 -->

**Cause.** The source uses `#if` conditional compilation (or fails it). A migration would only see
the branch taken with today's defines and silently drop the others.

**Fix.** Remove or resolve the conditionals first, or migrate the file by hand.

## DSH9091

<!-- generated:begin DSH9091 -->
**Severity** error

**Message**

```
'{0}' names a source root in front of the path, which '#include' cannot; write the path from the root's own folder (a leading '/') or relative to this file, then migrate again.
```

**Raised by** `Source/DreamShaderLang/Private/Migrate/LangMigrate.cpp:451`
<!-- generated:end DSH9091 -->

**Cause.** An `import` names a source root in front of the path (`import
"MoonToon:Shared/Common.dsh"`). `#include` has no such form.

**Fix.** Write the path from the root's own folder with a leading `/`, or relative to the file, then
migrate again.

## DSH9092

<!-- generated:begin DSH9092 -->
**Severity** error

**Message**

```
{0} comment(s) of '{1}' would not be in the migrated file ({2}), so nothing was written; this is a fault of the migration, not of the source.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:601`
<!-- generated:end DSH9092 -->

**Cause.** A comment of the 1.x file would be missing from the migrated text. Every rewrite is
checked against the comments of its source, and nothing is written when one is lost.

**Fix.** This is a defect of the migrator, not of the source. Report it with the file; the message
quotes the comments.

## DSH9093

<!-- generated:begin DSH9093 -->
**Severity** error

**Message**

```
'{0}' has no 1.x declaration left; there is nothing to migrate.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:540`
<!-- generated:end DSH9093 -->

**Cause.** The file is a `.dsh` that holds 2.0 declarations only. There is nothing left to migrate.

**Fix.** Nothing to do.

## DSH9094

<!-- generated:begin DSH9094 -->
**Severity** warning

**Message**

```
'{0}' keeps its '/// @root', the 1.x spelling of where its asset goes, because the place the new file would put it could not be worked out; check the asset path the first build reports.
```

**Raised by** `Source/DreamShaderLang/Private/Migrate/LangMigrate.cpp:1636`, `Source/DreamShaderLang/Private/Migrate/LangMigrate.cpp:386`, `Source/DreamShaderLang/Private/Migrate/LangMigrate.cpp:432`, `Source/DreamShaderLang/Private/Migrate/LangMigrate.cpp:557`
<!-- generated:end DSH9094 -->

**Cause.** The migrated text differs from the 1.x source in a way the migrator could not avoid and
that does not stop the build: `/// @root` kept because the new file's place could not be worked out,
an optional input of a type that has no default to write (it becomes required), a multi-line text
flattened onto one `///` line, a selected output with no variable to read from.

**Fix.** Read the message: each case says what changed. Check the asset path or the input the first
build reports.

## DSH9095

<!-- generated:begin DSH9095 -->
**Severity** error

**Message**

```
'{0}' is not a 1.x source; migrate takes '.dsm', '.dsf' and '.dsh' files.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:506`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:514`
<!-- generated:end DSH9095 -->

**Cause.** `dsc migrate` was given a file that is not a 1.x source, or that cannot be read.

**Fix.** Pass a `.dsm`, `.dsf` or `.dsh`.

## DSH9097

<!-- generated:begin DSH9097 -->
**Severity** error

**Message**

```
The migrated text of '{0}' does not build as 2.0 source ({1}), so nothing was written; the text is in '{2}'.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:618`
<!-- generated:end DSH9097 -->

**Cause.** The rewritten text was parsed, bound and lowered as 2.0 source before being written, and
it does not build. Nothing was written; the rejected text is kept under
`Saved/DreamShader/Migrated/Rejected/` and the diagnostics that follow point into it.

**Fix.** Read the first error in the rejected file. It is either a construct the migrator does not
handle yet (report it) or a 1.x leniency the source leaned on that has no 2.0 spelling -- fix that
in the 1.x source and migrate again.

## DSH9098

<!-- generated:begin DSH9098 -->
**Severity** warning

**Message**

```
The migrated text of '{0}' does not build the graph the 1.x file builds: {1}
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:652`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:667`
<!-- generated:end DSH9098 -->

**Cause.** The migrated text builds, and its IR is not equivalent to the 1.x file's: either the
graph differs (the message walks down to the first differing node) or the asset would land at
another path. Differences that are one asset written two ways -- a constant on a pin against its
`Const*` twin, an identity swizzle, spacing in Custom code -- are not reported.

**Fix.** For a graph difference, compare the two nodes named; typical causes are a 1.x `OutputType`
that lied about a node's width, and an explicit swizzle where 1.x wired a wider value as it was. For
a path difference, add the `/// @name` the message gives.

## DSH9099

<!-- generated:begin DSH9099 -->
**Severity** error

**Message**

```
'{0}' already exists and is not written over; move it away, or migrate into another folder with -Out.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:146`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:547`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:695`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:711`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:723`, `Source/DreamShaderEditor/Private/Commandlet/DreamShaderMigrate.cpp:741`
<!-- generated:end DSH9099 -->

**Cause.** The migration could not read, write or replace a file: the builtin catalog is empty
(nothing names a `UE.*` node), the `.dss` already exists, the output or the backup location is not
writable, or the 1.x file could not be moved away -- in which case the new file is not written
either, because the two would declare the same assets.

**Fix.** Free the path the message names, or migrate into another folder with `-Out`.

## DSH9100

<!-- generated:begin DSH9100 -->
**Severity** error

**Message**

```
'{0}' has no parent material, and a '.dsi' is nothing but overrides of one.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderInstanceDecompiler.cpp:175`
<!-- generated:end DSH9100 -->

**Cause.** The material instance has no parent. A `.dsi` is nothing but overrides of a parent, so
there is nothing to write.

**Fix.** Set the instance's parent in the asset.

## DSH9101

<!-- generated:begin DSH9101 -->
**Severity** warning

**Message**

```
'{0}' overrides {1} parameter(s) of its material layers or blends, which a '.dsi' cannot address; they are left out.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderInstanceDecompiler.cpp:300`
<!-- generated:end DSH9101 -->

**Cause.** The instance overrides parameters of its material layers or blends. A `.dsi` addresses
global parameters only, so those overrides are left out of the text and would be lost on a rebuild.

**Fix.** Keep such an instance hand-made, or move the values to global parameters in the parent.

## DSH9102

<!-- generated:begin DSH9102 -->
**Severity** info

**Message**

```
The parameter '{0}' is not a name a variable can have; it is written under another with '/// @name {0}'.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderInstanceDecompiler.cpp:289`
<!-- generated:end DSH9102 -->

**Cause.** A parameter's name is not an identifier (`Base Color`, `UV-Scale`). The override is
declared under an identifier and carries `/// @name <real name>`.

**Fix.** Nothing to fix.

## DSH9103

<!-- generated:begin DSH9103 -->
**Severity** error

**Message**

```
DSH9103: The Parent of '{0}' no longer resolves, so the overrides of '{1}' cannot be written back into it: {2}
```

**Raised by** `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:717`
<!-- generated:end DSH9103 -->

**Cause.** Adopt (or a tweak action) has to rewrite a `.dsi`, and the file's `Parent` no longer
resolves, so the instance's overrides cannot be checked against a parameter list.

**Fix.** Fix the `Parent` key in the `.dsi`, then adopt again.

## DSH9104

<!-- generated:begin DSH9104 -->
**Severity** warning

**Message**

```
'{0}' overrides '{1}', which '#pragma instance' has no key for; the rebuilt instance has the parent's.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderInstanceDecompiler.cpp:320`
<!-- generated:end DSH9104 -->

**Cause.** The instance overrides a base property `#pragma instance` has no key for. The rebuilt
instance takes the parent's value.

**Fix.** Set it on the rebuilt asset by hand, or on the parent.

## DSH9105

<!-- generated:begin DSH9105 -->
**Severity** warning

**Message**

```
'{0}' is a curve atlas row, and '{1}' picks its curve; a '.dsi' can state the row's number and nothing else, so the rebuilt instance loses the curve.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderInstanceDecompiler.cpp:281`
<!-- generated:end DSH9105 -->

**Cause.** A scalar override is a curve-atlas row: the engine stores the curve and atlas beside the
number. A `.dsi` can state the number only.

**Fix.** Keep that instance hand-made if the curve matters.

## DSH9106

<!-- generated:begin DSH9106 -->
**Severity** warning

**Message**

```
'{0}' is declared 'bool' by the parent's source and '{1}' sets it to {2}; the override is written as a 'float'.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderInstanceDecompiler.cpp:270`
<!-- generated:end DSH9106 -->

**Cause.** The parent's source declares the parameter `bool` (a scalar 0/1 parameter), and the
instance holds a value that is neither 0 nor 1. It is written as a `float` override so that the
value survives.

**Fix.** Nothing to fix; correct the value in the asset if 0 or 1 was meant.

## DSH9107

<!-- generated:begin DSH9107 -->
**Severity** error

**Message**

```
Expected '{0}' to be declared alone to rewrite its value, found it in a declaration shared with other names; split the declaration first.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangInstanceSource.cpp:656`, `Source/DreamShaderLang/Private/Lang/LangPipelineSource.cpp:1227`
<!-- generated:end DSH9107 -->

**Cause.** An editor action has to rewrite one override's value in a `.dsi`, and that name shares
its declaration with others (`uniform float a = 1, b = 2;`). The rewriter edits one declaration at a
time.

**Fix.** Split the declaration into one line per name.

## DSH9108

<!-- generated:begin DSH9108 -->
**Severity** error

**Message**

```
Expected every change to this file to touch its own stretch of text, found an edit at line {0} that overlaps another or runs past the end; the file was left unchanged.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangInstanceSource.cpp:803`, `Source/DreamShaderLang/Private/Lang/LangPipelineSource.cpp:1150`
<!-- generated:end DSH9108 -->

**Cause.** Two text edits computed for one `.dsi` overlap, or one runs past the end of the file. The
file is left as it was. Internal error of the instance source rewriter.

**Fix.** Report it with the file and the action that was taken.

## DSH9109

<!-- generated:begin DSH9109 -->
**Severity** error

**Message**

```
Expected a '#pragma instance(...)' line in this .dsi file, found none; the file was left unchanged.
```

**Raised by** `Source/DreamShaderLang/Private/Lang/LangInstanceSource.cpp:1109`, `Source/DreamShaderLang/Private/Lang/LangInstanceSource.cpp:1122`, `Source/DreamShaderLang/Private/Lang/LangInstanceSource.cpp:954`, `Source/DreamShaderLang/Private/Lang/LangPipelineSource.cpp:2355`
<!-- generated:end DSH9109 -->

**Cause.** An editor action that rewrites a source in place found nothing to anchor its edit on: a
`.dsi` without a `#pragma instance(...)` line, or -- when an instance's value is written back as the
default of its parent's uniform -- no `uniform` of that parameter name declared in the file itself
(one in an included header cannot be spliced), or one declared as another kind of parameter. The
action also refuses a scalar value that the `.dss` uniform's `bool`, `int` or `uint` type cannot
represent: changing a shader input's type implicitly could change its callers.

**Fix.** Add the pragma (`#pragma instance(Parent = "...")`), or declare the uniform in the file the
action targets, and repeat the action. For an unrepresentable scalar, change the source type
explicitly or extract the tweak into a `.dsi` instance.

## DSH9110

<!-- generated:begin DSH9110 -->
**Severity** error

**Message**

```
'-{0}={1}' is neither on nor off. A flag takes true, 1, yes or on, or false, 0, no or off, and '-{0}' alone is on; the command did nothing.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderCommandletRunner.cpp:227`
<!-- generated:end DSH9110 -->

**Cause.** a boolean flag of a commandlet verb -- `-Force`, `-All`, `-Check`, `-DryRun`, `-NoBackup`, `-Shaders`,
`-Json`, `-IncludeEphemeral`, `-KeepAssetPath`, `-Readable`, `-Gc` or `-Rebuild` -- was written with a value that is
neither on (`true`, `1`, `yes`, `on`) nor off (`false`, `0`, `no`, `off`), in any case. An empty value counts as
neither: `-Force=` is usually a script variable that was never set. Nothing guesses what was meant. A verb reads all
of its flags before it touches a file, and on this error it runs nothing and exits `1`. In 2.1.0 and earlier a flag
written with any value read as absent, so `migrate -Check=true` migrated and `fmt -Check=true` rewrote.

**Fix.** write the flag bare to turn it on (`-Force`), or with one of the eight words; leave it out, or write
`=false`, to keep it off. Only the flags a verb reads are checked: one it does not take is ignored, whatever its
value.

**See** [Commandlet — Boolean flags](../tools/commandlet.md#boolean-flags)

## DSH9200

<!-- generated:begin DSH9200 -->
**Severity** error

**Message**

```
The Custom Pass slot registry cannot be read: {0}. 'dsc pass-registry -Rebuild' moves it aside and gives every HLSL pass a slot again.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:164`
<!-- generated:end DSH9200 -->

**Cause.** `pass-registry` could not read `Registry.json` to list it: the file cannot be opened, it is
not valid JSON — a merge conflict left in it is the usual reason — or an entry has no slot number,
pipeline or pass. Nothing is listed and the run fails (exit code `1`). A compile of any `.dsp` refuses
the same file (DSH8315).

**Fix.** Run `./dsc.ps1 pass-registry -Rebuild`: it moves the file aside to `Registry.json.unreadable`
and compiles every `.dsp`, which gives every HLSL pass a slot again. Taking either side of the conflict
and compiling the `.dsp` files again works too; never merge the JSON by hand. See
[`pass-registry`](../tools/commandlet.md#pass-registry).

## DSH9201

<!-- generated:begin DSH9201 -->
**Severity** warning

**Message**

```
No .dsp builds '{0}' any more, but the asset is still there and its pass '{1}' points at {2} slot {3}. Delete the asset or restore its source: once the slot is collected and given to another pass, that pass's shader is what this one would run.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:98`
<!-- generated:end DSH9201 -->

**Cause.** No `.dsp` under the source roots builds this slot's pipeline any more — the `.dsp` was deleted
or renamed — and the pipeline asset is still there, loaded or on disk, with its pass pointing at the
slot. `-Gc` frees such a slot, and once a new HLSL pass takes it, the old pipeline dispatches that pass's
shader with its own bindings wherever it still runs (a volume, a component, the project's global
pipelines). Raised by the listing, `-Gc` and `-Rebuild`.

**Fix.** Delete the old pipeline asset and whatever activates it, or restore its `.dsp` and compile it,
which gives the pass a slot of its own again. A renamed `.dsp` builds a new pipeline under its new name;
the asset under the old one is the orphan.

## DSH9202

<!-- generated:begin DSH9202 -->
**Severity** info

**Message**

```
{0} slot {1} was freed: {2}
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:213`
<!-- generated:end DSH9202 -->

**Cause.** Informational. `pass-registry -Gc`, or the collection step of `-Rebuild`, freed a slot: no
`.dsp` builds its pipeline any more, or its pipeline's `.dsp` compiles and no longer runs that pass in
HLSL in that table. Its entry in `Registry.json`, its section in the registry file and its snapshot are
deleted. A commandlet recompiles no shader; the next editor start compiles the changed registry.

**Fix.** Nothing. Commit the changed `.dreampass/` folder.

## DSH9203

<!-- generated:begin DSH9203 -->
**Severity** warning

**Message**

```
Registry.json did not parse and was moved aside to '{0}'; the compiles that follow give every HLSL pass a slot afresh.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:343`
<!-- generated:end DSH9203 -->

**Cause.** `pass-registry -Rebuild` found a `Registry.json` that does not parse — a merge conflict left
in it is the usual reason — moved it aside to `Registry.json.unreadable` and started from an empty
registry. The compiles that follow give every HLSL pass a slot afresh, in path order, so slot numbers
can change, and every pipeline with an HLSL pass is rebuilt and saved with its new slots.

**Fix.** Nothing to repair. Commit the rebuilt `.dreampass/` folder, and the rebuilt pipelines if you
commit generated assets. Nothing reads `Registry.json.unreadable`: look at it to see what the conflict
was, then delete it rather than commit it.

## DSH9204

<!-- generated:begin DSH9204 -->
**Severity** error

**Message**

```
'{0}' did not compile, so its pipeline keeps the slots it had: {1}
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:395`
<!-- generated:end DSH9204 -->

**Cause.** `pass-registry -Rebuild` compiles every `.dsp` under the source roots, forced, and this one
failed; the message ends with the compile's report. Its pipeline keeps the slots the registry has for it
— none, if this rebuild reset an unreadable registry (DSH9203) — and its asset is not updated. The run
fails (exit code `1`) and still finishes its remaining steps.

**Fix.** Fix the `.dsp` from the report and compile it (`./dsc.ps1 compile <file>.dsp`): that plans its
slots against the rebuilt registry and updates its asset. DSH9205 names each of its passes that points
at a slot it no longer owns, and DSH8337 each of its slots left without a snapshot.

## DSH9205

<!-- generated:begin DSH9205 -->
**Severity** warning

**Message**

```
Pass '{1}' of '{0}' points at {2} slot {3}, which the registry {4}. Its source '{5}' did not compile, so the asset was not updated: fix the source and compile it, or that pass runs whatever the slot holds.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:314`
<!-- generated:end DSH9205 -->

**Cause.** After `pass-registry -Rebuild`, a pipeline whose `.dsp` failed in the rebuild (DSH9204) has a
pass that points at a slot the registry now gives to another pass, or leaves free — typically because
the rebuild reset an unreadable registry and gave every slot out afresh. The asset was not updated, so
until its source compiles, that pass dispatches whatever the slot holds: another pass's shader with this
pass's bindings, or the empty stub.

**Fix.** Fix the `.dsp` the message names and compile it; the compile gives the pass a slot of its own
and writes it into the asset. Until then `r.DreamPass.DisablePipelines` keeps the pipeline from running.

## DSH9206

<!-- generated:begin DSH9206 -->
**Severity** info

**Message**

```
No .dsp under the source roots; the registry is rewritten from Registry.json as it stands.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:363`
<!-- generated:end DSH9206 -->

**Cause.** Informational. `pass-registry -Rebuild` found no `.dsp` under the source roots (`Packages`
folders excluded), so there was nothing to compile. The rest of the rebuild still runs: the slots of
pipelines no `.dsp` builds are freed — a slot whose recorded `.dsp` is still on disk is kept — and the
registry files are rewritten from `Registry.json`.

**Fix.** Nothing, when the project has no pipelines. If it has, the source roots do not reach their
`.dsp` files: check the *Source Directory* project setting and the plugins' `DShader` folders.

## DSH9208

<!-- generated:begin DSH9208 -->
**Severity** warning

**Message**

```
The snapshot of {0} slot {1} (pass '{2}' of '{3}') is not on disk, and the registry file includes it: the next start takes the slot's section out of the registry file, and the pass does nothing until its source is compiled again -- unless the registry file is read-only, and then the global shaders fail to compile, which is fatal. Compile '{4}', or run 'dsc pass-registry -Rebuild'; commit the Slots folder with the registry.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:114`
<!-- generated:end DSH9208 -->

**Cause.** `Registry.json` records a snapshot for this slot and its files are not under
`.dreampass/Slots/` — a `Slots` folder that was not committed, or not synced — and the registry file
still includes them. At its next start the DreamShaderPass module takes such a section out of the
registry file (an error in the log), and the pass does nothing until its `.dsp` compiles again. Only a
registry file that is read-only — not checked out — cannot be rewritten that way: it keeps including the
missing file, and the global shaders then fail to compile, which is fatal. Raised by the listing and
`-Gc`; `-Rebuild` reserves such slots itself (DSH8337).

**Fix.** Compile the `.dsp` the message names: that writes the snapshot back, and the section with it.
`./dsc.ps1 pass-registry -Rebuild` does every slot at once. Commit the `Slots` folder with the registry.
If the registry files are read-only, check them out before the editor next starts.

## DSH9209

<!-- generated:begin DSH9209 -->
**Severity** warning

**Message**

```
'{0}' does not compile far enough to tell whether it still runs pass '{1}' in {2} slot {3}, so the slot is kept. Fix the source and compile it.
```

**Raised by** `Source/DreamShaderEditor/Private/Commandlet/DreamShaderPassRegistryCommandlet.cpp:124`
<!-- generated:end DSH9209 -->

**Cause.** The `.dsp` that owns this slot does not compile far enough to tell whether it still runs the
pass in HLSL in that table: its front end fails, its pipeline's asset path does not resolve, or the file
is on disk and did not make it into the product index (a preprocessor error stops the index's parse). A
slot is freed only on evidence, so this one is kept. Raised by the listing, `-Gc` and `-Rebuild`.

**Fix.** Fix the `.dsp` — `./dsc.ps1 check <file>.dsp` says what is wrong — and compile it; the listing
can then judge the slot. If the `.dsp` is meant to go, delete it, and `-Gc` frees its slots.

## DSH9210

<!-- generated:begin DSH9210 -->
**Severity** error

**Message**

```
'{0}' is a pass pipeline, which decompiles to a '.dsp', and '{1}' is not one.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderIRDecompiler.cpp:247`
<!-- generated:end DSH9210 -->

**Cause.** A pipeline decompiles to `.dsp` text, and the output file asked for — `-Out`, or the
request's output path — has another extension. (`-Format=Legacy`, or an `-Out` ending in `.dsm` /
`.dsf`, hands the pipeline to the 1.x decompiler instead, which refuses it: a pipeline has no 1.x text.)

**Fix.** End `-Out` in `.dsp`, or leave it off: the default is
`<SourceDirectory>/Decompiled/Pipelines/<package path>.dsp`.

## DSH9211

<!-- generated:begin DSH9211 -->
**Severity** warning

**Message**

```
The pass '{0}' has neither a material nor a shader, so the text names neither, and it does not build until one is given.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:435`
<!-- generated:end DSH9211 -->

**Cause.** A fullscreen pass of the pipeline has neither a material nor a shader — the material it named
no longer loads, or both were cleared in the pipeline's details panel. A `.dsp` fullscreen pass names
one of the two, so the text names neither and does not build until one is given. `dsc decompile` writes
the warning at the head of the file; Adopt refuses the pipeline and writes nothing.

**Fix.** Give the pass its material, or its shader and entry, in the details panel and decompile or
adopt again — or, after a decompile, write `Material` (or `Shader` and `Entry`) into the text.

## DSH9212

<!-- generated:begin DSH9212 -->
**Severity** warning

**Message**

```
The pass '{0}' has both a material, '{1}', and a shader, '{2}'; a fullscreen pass names one of the two, and the text keeps the material, which is what the pass draws.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:413`
<!-- generated:end DSH9212 -->

**Cause.** A fullscreen pass of the pipeline has both a material and a shader, which only an edit in its
details panel makes: a compile sets one of the two. The pass draws the material, so the text keeps it
and leaves the shader out. Adopt refuses the pipeline rather than drop the shader from the `.dsp`.

**Fix.** Clear the one the pass should not have in the details panel, then decompile or adopt again.

## DSH9214

<!-- generated:begin DSH9214 -->
**Severity** info

**Message**

```
A layer filter of the pass '{0}' kept no spelling of its layers, so they are written as the project's layer table names its bits today: {1}.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:534`
<!-- generated:end DSH9214 -->

**Cause.** Informational. A `Layer(...)` filter term of a mesh pass holds layer bits and none of the
names it was written with — an edit by hand, or an asset compiled before the names were kept beside the
bits. The text names the bits by the project's layer table as it is today.

**Fix.** Check that the names are the layers meant: if the table was reordered since the bits were set,
the same bits name other layers now.

## DSH9215

<!-- generated:begin DSH9215 -->
**Severity** warning

**Message**

```
A layer filter of the pass '{0}' selects layer bit(s) {1}, which the project's layer table has no name for; the text cannot say them and leaves them out.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:541`
<!-- generated:end DSH9215 -->

**Cause.** A `Layer(...)` filter term of a mesh pass selects layer bits that the project's layer table
names nothing at (the table is shorter, or the entry is empty), and no stored name says what they were.
A `.dsp` names layers, so the text leaves those bits out. Adopt refuses the pipeline rather than drop
them.

**Fix.** Name those positions in Project Settings ▸ DreamPlugin ▸ DreamShader Custom Pass ▸ Layer Names
(bit *n* is the *n*-th name, counting from 0), or take the bits out of the filter in the details panel;
then decompile or adopt again.

## DSH9216

<!-- generated:begin DSH9216 -->
**Severity** warning

**Message**

```
'{0}' runs in no kind of view, which a '.dsp' cannot say; the text leaves 'Views' out, and a rebuild runs in {1}.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:834`
<!-- generated:end DSH9216 -->

**Cause.** The pipeline runs in no kind of view: its `Views` is empty, which only an edit in its details
panel makes. A `.dsp` has no spelling for that — leaving `Views` out means `Game | Editor` — so the text
leaves it out, and a pipeline rebuilt from it runs in `Game | Editor`. Adopt refuses the pipeline.

**Fix.** Turn the pipeline off another way — `Enabled` bound to a `bool` uniform (a constant
`Enabled = false` is [`DSH7322`](DSH7xxx.md#dsh7322)), taking it out of what activates it, or
`r.DreamPass.DisablePipelines` for a session — and give it back the views it should run in; then
decompile or adopt again.

## DSH9217

<!-- generated:begin DSH9217 -->
**Severity** warning

**Message**

```
The pass '{0}' checks its override material for no usage flag at all, which a '.dsp' cannot say; the text leaves 'Usage' out, and a rebuild checks the default set ({1}).
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:584`
<!-- generated:end DSH9217 -->

**Cause.** A mesh pass checks its override material for no usage flag at all: its `Usage` is empty, which
only an edit in its details panel makes. A `.dsp` has no spelling for that — leaving `Usage` out means
the default set — so the text leaves it out, and a rebuild checks `StaticMesh`, `InstancedStaticMeshes`
and `SkeletalMesh`. Adopt refuses the pipeline.

**Fix.** Set the flags the override material needs in the details panel (the default set, if nothing
else), then decompile or adopt again.

## DSH9218

<!-- generated:begin DSH9218 -->
**Severity** warning

**Message**

```
The parameter '{0}' defaults to '{1}', which is not a 2D texture. A '.dsp' declares every texture parameter 'Texture2D', and the text does so here too, keeping this default; it builds as it is.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:230`
<!-- generated:end DSH9218 -->

**Cause.** A texture parameter of the pipeline defaults to a texture that is not 2D — a cube, a volume,
an array. A `.dsp` declares every texture parameter `Texture2D`, so the text states a type its default
does not have, and keeps the default. The text builds as it is: a build takes any texture as the
default, so the pipeline rebuilt from it keeps this one.

**Fix.** If the parameter is meant to be 2D, give it a 2D default, in the details panel or in the
`/// @default` of the text.

## DSH9219

<!-- generated:begin DSH9219 -->
**Severity** warning

**Message**

```
The pass '{0}' binds '{1}' to a texture constant, which a 'param' cannot state; the binding is left out of the text.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:333`
<!-- generated:end DSH9219 -->

**Cause.** A pass binds a `param` to a texture constant, which only an edit in its details panel makes.
A `param` takes a uniform, a `static const`, a literal or `DreamPassWeight`, and has no spelling for a
texture constant, so the text leaves the binding out. Adopt refuses the pipeline.

**Fix.** Declare a `Texture2D` uniform with that texture as its `/// @default` and bind the param to it,
in the details panel or in the text; then decompile or adopt again.

## DSH9220

<!-- generated:begin DSH9220 -->
**Severity** warning

**Message**

```
The pass '{0}' tests against its own depth but names no Depth32 buffer for it; the text writes 'Own()' empty, and it does not build until one is named.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:573`
<!-- generated:end DSH9220 -->

**Cause.** A mesh pass tests against its own depth (`Depth = Own`) but names no `Depth32` buffer for it,
which only an edit in its details panel leaves. The text writes `Own()` empty, which does not build
until a buffer is named. Adopt refuses the pipeline.

**Fix.** Name a `Depth32` buffer for it in the details panel — declare one if the pipeline has none — or
choose `TestScene` or `None`; then decompile or adopt again.

## DSH9221

<!-- generated:begin DSH9221 -->
**Severity** warning

**Message**

```
The decompiled pipeline does not parse back: {0}: {1}. It is written as it is; this is a defect of the decompiler.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:951`
<!-- generated:end DSH9221 -->

**Cause.** The `.dsp` text the decompiler printed does not parse back; the message names the first parse
error. The file is written anyway, so nothing is lost. A defect of the decompiler, not of the asset.

**Fix.** Report it with the asset; fixing the named line by hand usually gives a working file.

## DSH9222

<!-- generated:begin DSH9222 -->
**Severity** warning

**Message**

```
The decompiled pipeline parses but does not bind back into a pipeline: {0}: {1}. It is written as it is; either the asset breaks a rule a '.dsp' is checked against (an edit by hand can), or this is a defect of the decompiler.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:967`
<!-- generated:end DSH9222 -->

**Cause.** The printed text parses, and binding it — without the engine: references taken as written,
the checks that need the project skipped — fails or gives no pipeline; the message quotes the first
error. Either the asset breaks a rule every `.dsp` is checked against, which an edit in its details
panel can (two passes or buffers of one name, a pass that reads and writes one buffer, a read before the
first write of a `Clear = None` buffer, a kind at an injection point it cannot run at), or the
decompiler is at fault. The file is written as it is.

**Fix.** Look up the quoted code: it names the rule, at its line in the text. Fix the asset in its
details panel, or the text; if the pipeline was never edited by hand, report it with the asset.

## DSH9223

<!-- generated:begin DSH9223 -->
**Severity** warning

**Message**

```
The decompiled pipeline reads back as a different pipeline ({0} difference(s); the first: {1}). It is written as it is; this is a defect of the decompiler or the printer.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:981`
<!-- generated:end DSH9223 -->

**Cause.** The printed text parses and binds, and the pipeline it binds to differs from the asset — the
message counts the differences and names the first — even with the facts only the engine knows (what a
reference resolves to, a `.usf`'s `[numthreads]`) taken from the asset. A defect of the decompiler or of
the printer; the file is written as it is.

**Fix.** Report it with the asset. Correct the named value in the text before building from it.

## DSH9224

<!-- generated:begin DSH9224 -->
**Severity** warning

**Message**

```
A '.dsp' names its pipeline after its file and has no '/// @name', so '{0}' builds '{1}' where it is written, not '{2}'; move the file to where the pipeline's source belongs to keep its path.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:910`
<!-- generated:end DSH9224 -->

**Cause.** `-KeepAssetPath` asked to keep the pipeline where it is, and a `.dsp` cannot say where its
pipeline goes: it names it after its file and has no `/// @name`, and the file's folder under its source
root gives the package. Where the text is written, it would build the pipeline at the path the message
names, not at the asset's.

**Fix.** Write the file (with `-Out`), or move it, to the folder and name that give the asset's path —
usually where the pipeline's own `.dsp` was. Compiled where it is, it builds a second pipeline.

## DSH9225

<!-- generated:begin DSH9225 -->
**Severity** error

**Message**

```
There is no pass pipeline to decompile.
```

**Raised by** `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:821`, `Source/DreamShaderEditor/Private/Decompiler/DreamShaderPipelineDecompiler.cpp:921`
<!-- generated:end DSH9225 -->

**Cause.** Internal. The pipeline decompiler was handed no pipeline at all, or the payload it read could
not be laid out as a `.dsp` for the printer. Neither is a property of the asset; nothing is written.

**Fix.** Report it with the asset and the command that ran.

## DSH9226

<!-- generated:begin DSH9226 -->
**Severity** error

**Message**

```
DSH9226: '{0}' is built from the pipeline file '{1}' but is not a pass pipeline, so nothing was adopted.
```

**Raised by** `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:459`
<!-- generated:end DSH9226 -->

**Cause.** Adopt was asked for an asset whose source stamp (`DreamShader.SourceFile`) names a `.dsp`, and
the asset is not a `UDreamPassPipeline`. The one other asset a `.dsp` makes is the render target of an
exported buffer, which carries the same stamp and holds nothing a `.dsp` states; anything else with a
`.dsp` stamp had its metadata copied or edited. Nothing was adopted.

**Fix.** Adopt the pipeline itself — from its details panel, the divergence notification or the
Material Content Browser. A render target follows its buffer's declaration: change the buffer in the
`.dsp`.

## DSH9227

<!-- generated:begin DSH9227 -->
**Severity** error

**Message**

```
DSH9227: '{0}' is not a .dsp pipeline file, so the settings of '{1}' cannot be spliced into it.
```

**Raised by** `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:850`
<!-- generated:end DSH9227 -->

**Cause.** The `.dsp` Adopt was handed a file that is not a `.dsp`. Adopt chooses the pipeline route by
the extension of the asset's source stamp, so the editor's own entries always hand it a `.dsp`; this
guards a direct call with another file. Nothing was written.

**Fix.** Adopt from the editor — the pipeline's details panel, the divergence notification, the Material
Content Browser — which uses the `.dsp` the pipeline is stamped with.

## DSH9228

<!-- generated:begin DSH9228 -->
**Severity** error

**Message**

```
DSH9228: '{0}' builds no pass pipeline any more, so '{1}' has nothing to be spliced into.
```

**Raised by** `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:905`
<!-- generated:end DSH9228 -->

**Cause.** The `.dsp` the pipeline is stamped with checks — it parses, binds and lowers — and nothing it
builds is a pass pipeline, so there is nothing to splice the asset's values into. A `.dsp` that checks
always builds exactly one pipeline, named after the file, so this guards a case a file does not normally
reach. Nothing was written.

**Fix.** Run `./dsc.ps1 dump-ir <file>.dsp` to see what the file builds, and report it with the file.

