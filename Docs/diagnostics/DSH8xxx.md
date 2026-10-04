# DSH8xxx --- Asset generation and saving

> The block between the generated markers is written by `.skill/gen-diagnostics.ps1`.
> Everything below a marker is written by hand and survives a regeneration.

## DSH8088

<!-- generated:begin DSH8088 -->
**Severity** error

**Message**

```
%s contains an invalid folder segment.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:42`
<!-- generated:end DSH8088 -->

**Cause.** The asset name a source declares contains a folder segment that cannot be part of a
package path: empty (`A//B`), `.` or `..`, or a character the engine refuses in a path.

**Fix.** Write the name as plain folders and an asset name: `Characters/Hero/M_Hero`.

## DSH8089

<!-- generated:begin DSH8089 -->
**Severity** error

**Message**

```
DreamShader Root '%s' references project plugin '%s', but no enabled plugin with that name was found.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:59`
<!-- generated:end DSH8089 -->

**Cause.** A source says where its asset goes with a root: `Game`, `Engine`, or `Plugin.<Name>` (in
1.x `Root = "..."` on the block; in 2.0 the source root the file lies under, or `/// @name` with an
object path). This root names a plugin no enabled plugin of the project answers to.

**Fix.** Check the plugin's name (the `.uplugin` file name, not its friendly name) and that it is
enabled for this project.

## DSH8090

<!-- generated:begin DSH8090 -->
**Severity** error

**Message**

```
DreamShader Root '%s' must reference a project plugin under '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:66`
<!-- generated:end DSH8090 -->

**Cause.** A source says where its asset goes with a root: `Game`, `Engine`, or `Plugin.<Name>` (in
1.x `Root = "..."` on the block; in 2.0 the source root the file lies under, or `/// @name` with an
object path). This root names a plugin that is not one of the project's own (an engine or
marketplace plugin). DreamShader only writes assets into plugins under the project's `Plugins`
folder.

**Fix.** Target a project plugin, or `Game`.

## DSH8091

<!-- generated:begin DSH8091 -->
**Severity** error

**Message**

```
DreamShader Root '%s' references project plugin '%s', but the plugin is not enabled.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:71`
<!-- generated:end DSH8091 -->

**Cause.** A source says where its asset goes with a root: `Game`, `Engine`, or `Plugin.<Name>` (in
1.x `Root = "..."` on the block; in 2.0 the source root the file lies under, or `/// @name` with an
object path). The plugin exists and is disabled, so its content path is not mounted.

**Fix.** Enable the plugin in the `.uproject` or the Plugins window.

## DSH8092

<!-- generated:begin DSH8092 -->
**Severity** error

**Message**

```
DreamShader Root '%s' references project plugin '%s', but the plugin cannot contain content.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:76`
<!-- generated:end DSH8092 -->

**Cause.** A source says where its asset goes with a root: `Game`, `Engine`, or `Plugin.<Name>` (in
1.x `Root = "..."` on the block; in 2.0 the source root the file lies under, or `/// @name` with an
object path). The plugin is enabled and its descriptor says `CanContainContent: false`.

**Fix.** Set `"CanContainContent": true` in the `.uplugin`.

## DSH8093

<!-- generated:begin DSH8093 -->
**Severity** error

**Message**

```
DreamShader Root '%s' references project plugin '%s', but its Content directory does not exist: '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:82`
<!-- generated:end DSH8093 -->

**Cause.** A source says where its asset goes with a root: `Game`, `Engine`, or `Plugin.<Name>` (in
1.x `Root = "..."` on the block; in 2.0 the source root the file lies under, or `/// @name` with an
object path). The plugin can hold content, and its `Content` directory is not on disk.

**Fix.** Create the folder the message names.

## DSH8094

<!-- generated:begin DSH8094 -->
**Severity** error

**Message**

```
DreamShader Root '%s' references project plugin '%s', but the plugin content is not mounted.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:88`
<!-- generated:end DSH8094 -->

**Cause.** A source says where its asset goes with a root: `Game`, `Engine`, or `Plugin.<Name>` (in
1.x `Root = "..."` on the block; in 2.0 the source root the file lies under, or `/// @name` with an
object path). The plugin's content directory exists and the engine has not mounted it -- typically a
plugin that was enabled or created while the editor was running.

**Fix.** Restart the editor (or the commandlet run) so the mount point is registered.

## DSH8095

<!-- generated:begin DSH8095 -->
**Severity** error

**Message**

```
DreamShader Root '%s' has an invalid plugin name.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:165`
<!-- generated:end DSH8095 -->

**Cause.** A source says where its asset goes with a root: `Game`, `Engine`, or `Plugin.<Name>` (in
1.x `Root = "..."` on the block; in 2.0 the source root the file lies under, or `/// @name` with an
object path). The plugin part of this root is empty or not a name (`Plugin.` with nothing after it).

**Fix.** Write `Plugin.MyPlugin`.

## DSH8096

<!-- generated:begin DSH8096 -->
**Severity** error

**Message**

```
DreamShader Root '%s' has an invalid plugin name.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:179`
<!-- generated:end DSH8096 -->

**Cause.** Same as DSH8095, for the `Plugins.<Name>` spelling of the root.

**Fix.** Write `Plugins.MyPlugin`.

## DSH8097

<!-- generated:begin DSH8097 -->
**Severity** error

**Message**

```
DreamShader Root '%s' has an invalid package root.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:192`
<!-- generated:end DSH8097 -->

**Cause.** A source says where its asset goes with a root: `Game`, `Engine`, or `Plugin.<Name>` (in
1.x `Root = "..."` on the block; in 2.0 the source root the file lies under, or `/// @name` with an
object path). This root is none of those, or resolves to a package root the engine does not know.

**Fix.** Use `Game`, `Engine` or `Plugin.<Name>`.

## DSH8098

<!-- generated:begin DSH8098 -->
**Severity** error

**Message**

```
DreamShader asset name must resolve to a non-empty asset path.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:306`
<!-- generated:end DSH8098 -->

**Cause.** The declared asset name is empty once trimmed.

**Fix.** Give the block a `Name`, or the export a name.

## DSH8099

<!-- generated:begin DSH8099 -->
**Severity** error

**Message**

```
DreamShader asset name must resolve to a non-empty asset path.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:313`
<!-- generated:end DSH8099 -->

**Cause.** The declared asset name consists of separators only (`/`), so no asset name is left.

**Fix.** End the path in an asset name: `Folder/M_Example`.

## DSH8100

<!-- generated:begin DSH8100 -->
**Severity** error

**Message**

```
DreamShader asset name '%s' produced an invalid asset name.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:319`
<!-- generated:end DSH8100 -->

**Cause.** The last segment of the declared asset name is not a valid object name (it contains a
character such as `.`, `:` or a space the engine refuses).

**Fix.** Rename the asset using letters, digits and `_`.

## DSH8101

<!-- generated:begin DSH8101 -->
**Severity** error

**Message**

```
Asset '%s' is open in an asset editor, so it was NOT rebuilt. An open editor works on its own copy of the asset and writes that copy back when you press Apply or Save, 
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:404`
<!-- generated:end DSH8101 -->

**Cause.** The asset is open in an asset editor. An open editor works on a copy of the asset and
writes it back on Apply or Save, which would undo the rebuild; a material editor with unsaved
changes may also have diverged from the source.

**Fix.** Close that asset's editor tab and compile again.

## DSH8102

<!-- generated:begin DSH8102 -->
**Severity** error

**Message**

```
Asset '%s' already exists and is not a Material.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:498`
<!-- generated:end DSH8102 -->

**Cause.** A package already sits where the material would go, and the object in it is not a
UMaterial.

**Fix.** Rename the source's product, or move the existing asset.

## DSH8103

<!-- generated:begin DSH8103 -->
**Severity** error

**Message**

```
Asset '%s' already exists and was not generated by DreamShader. Rename your shader or move/delete the existing asset before regenerating.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:507`
<!-- generated:end DSH8103 -->

**Cause.** A material already sits at the target path and carries no DreamShader provenance, so it
was made by hand. DreamShader never overwrites an asset it did not generate.

**Fix.** Rename the product, move or delete the hand-made asset, or decompile it into a source
first.

## DSH8104

<!-- generated:begin DSH8104 -->
**Severity** error

**Message**

```
Failed to create package '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:516`
<!-- generated:end DSH8104 -->

**Cause.** `CreatePackage` failed for the material's package -- an invalid long package name, or the
engine refusing the mount point.

**Fix.** Check the root and folder of the product; the log line above it has the engine's reason.

## DSH8105

<!-- generated:begin DSH8105 -->
**Severity** error

**Message**

```
Failed to create material '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:532`
<!-- generated:end DSH8105 -->

**Cause.** The UMaterial object could not be created in its package.

**Fix.** Look for an engine error right above it in the log (a name clash with a redirector is the
usual cause); fix up redirectors in that folder.

## DSH8106

<!-- generated:begin DSH8106 -->
**Severity** error

**Message**

```
Asset '%s' already exists and is not a DreamShader instance material. Delete it (or remove Backend="Instance") before switching backends.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:560`
<!-- generated:end DSH8106 -->

**Cause.** `Backend = ThinCustom` builds a DreamShader instance material, and the asset at that path
is of another class (a plain material from an earlier Graph build, usually).

**Fix.** Delete the old asset, or keep the backend it was built with.

## DSH8107

<!-- generated:begin DSH8107 -->
**Severity** error

**Message**

```
Asset '%s' already exists and was not generated by DreamShader. Rename your shader or move/delete the existing asset before regenerating.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:570`
<!-- generated:end DSH8107 -->

**Cause.** Same as DSH8103, for a ThinCustom instance: the existing asset was not generated by
DreamShader.

**Fix.** Rename the product, or move or delete the existing asset.

## DSH8108

<!-- generated:begin DSH8108 -->
**Severity** error

**Message**

```
Failed to create package '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:580`
<!-- generated:end DSH8108 -->

**Cause.** `CreatePackage` failed for a ThinCustom instance's package.

**Fix.** Check the product's root and folder.

## DSH8109

<!-- generated:begin DSH8109 -->
**Severity** error

**Message**

```
Failed to create instance material '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:592`
<!-- generated:end DSH8109 -->

**Cause.** The ThinCustom instance material object could not be created.

**Fix.** Look for the engine's error above it in the log.

## DSH8110

<!-- generated:begin DSH8110 -->
**Severity** error

**Message**

```
Asset '%s' already exists as '%s', but %s generation requires '%s'. Delete or move the existing asset and regenerate it.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:631`
<!-- generated:end DSH8110 -->

**Cause.** A function asset exists at the target path with another class than the source builds: a
plain MaterialFunction where the source is a layer, a layer where it is a blend. The three are
distinct engine classes and an asset cannot change class in place.

**Fix.** Delete or move the existing asset and compile again; references to it have to be re-made,
so prefer a new name when the old asset is in use.

## DSH8111

<!-- generated:begin DSH8111 -->
**Severity** error

**Message**

```
Asset '%s' already exists and is not a MaterialFunction asset.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:637`
<!-- generated:end DSH8111 -->

**Cause.** A package already sits where the function would go, and the object in it is no material
function at all.

**Fix.** Rename the product, or move the existing asset.

## DSH8112

<!-- generated:begin DSH8112 -->
**Severity** error

**Message**

```
Asset '%s' already exists and was not generated by DreamShader. Rename your function or move/delete the existing asset before regenerating.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:644`
<!-- generated:end DSH8112 -->

**Cause.** A material function already sits at the target path and was not generated by DreamShader.

**Fix.** Rename the function, move or delete the existing asset, or decompile it into a source
first.

## DSH8113

<!-- generated:begin DSH8113 -->
**Severity** error

**Message**

```
Failed to create package '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:653`
<!-- generated:end DSH8113 -->

**Cause.** `CreatePackage` failed for a function's package.

**Fix.** Check the product's root and folder.

## DSH8114

<!-- generated:begin DSH8114 -->
**Severity** error

**Message**

```
Failed to create material function '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetFactory.cpp:666`
<!-- generated:end DSH8114 -->

**Cause.** The material function object could not be created.

**Fix.** Look for the engine's error above it in the log.

## DSH8115

<!-- generated:begin DSH8115 -->
**Severity** error

**Message**

```
Asset '%s' was edited by hand since DreamShader generated it from '%s', so it was NOT rebuilt (rebuilding would destroy those edits). %s
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderGeneratedAssetMetadata.cpp:405`
<!-- generated:end DSH8115 -->

**Cause.** the asset no longer matches the output digest stamped at its last generation -- somebody edited it by hand, and a rebuild would destroy that work

**Fix.** answer the notification, or right-click the asset > DreamShader: **Revert to Source** (the source was right), **Adopt Into Source** (the asset was right), **Detach From DreamShader** (neither, stop managing it). `-Force` does not get past this

**See** [Divergence](../generation/divergence.md)

## DSH8116

<!-- generated:begin DSH8116 -->
**Severity** error

**Message**

```
Generated DreamShader asset '%s' could not be saved.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderGeneratedAssetMetadata.cpp:455`
<!-- generated:end DSH8116 -->

**Cause.** A generated asset could not be written to disk: the `.uasset` is read-only or checked in,
another process holds it, or the disk is full.

**Fix.** Check the file out / clear the read-only flag and compile again. Until then the asset
exists in memory only.

## DSH8117

<!-- generated:begin DSH8117 -->
**Severity** error

**Message**

```
Generated DreamShader asset packages could not be saved.%s
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderGeneratedAssetMetadata.cpp:485`
<!-- generated:end DSH8117 -->

**Cause.** Saving the batch of generated packages failed; the list after the message names each
package and why.

**Fix.** Fix the first cause listed (usually a read-only file) and compile again.

## DSH8118

<!-- generated:begin DSH8118 -->
**Severity** error

**Message**

```
Asset Path root '%s' references plugin '%s', but no enabled plugin with that name was found.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:123`
<!-- generated:end DSH8118 -->

**Cause.** `Path(Root, "Folder/Asset")` resolves its first argument to a mount point: `Game`,
`Engine`, or `Plugin.<Name>` / `Plugins.<Name>`. This one names a plugin that no enabled plugin
answers to.

**Fix.** Check the plugin's name and that it is enabled.

## DSH8119

<!-- generated:begin DSH8119 -->
**Severity** error

**Message**

```
Asset Path root '%s' references plugin '%s', but the plugin is not enabled.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:128`
<!-- generated:end DSH8119 -->

**Cause.** `Path(Root, "Folder/Asset")` resolves its first argument to a mount point: `Game`,
`Engine`, or `Plugin.<Name>` / `Plugins.<Name>`. The plugin exists and is disabled.

**Fix.** Enable it.

## DSH8120

<!-- generated:begin DSH8120 -->
**Severity** error

**Message**

```
Asset Path root '%s' references plugin '%s', but the plugin cannot contain content.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:133`
<!-- generated:end DSH8120 -->

**Cause.** `Path(Root, "Folder/Asset")` resolves its first argument to a mount point: `Game`,
`Engine`, or `Plugin.<Name>` / `Plugins.<Name>`. The plugin cannot contain content
(`CanContainContent: false`).

**Fix.** Point the reference at a plugin that holds the asset, or enable content in the `.uplugin`.

## DSH8121

<!-- generated:begin DSH8121 -->
**Severity** error

**Message**

```
Asset Path root '%s' references plugin '%s', but its Content directory does not exist: '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:139`
<!-- generated:end DSH8121 -->

**Cause.** `Path(Root, "Folder/Asset")` resolves its first argument to a mount point: `Game`,
`Engine`, or `Plugin.<Name>` / `Plugins.<Name>`. The plugin's `Content` directory is missing on
disk.

**Fix.** Check the path the message prints.

## DSH8122

<!-- generated:begin DSH8122 -->
**Severity** error

**Message**

```
Asset Path root '%s' references plugin '%s', but the plugin content is not mounted.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:145`
<!-- generated:end DSH8122 -->

**Cause.** `Path(Root, "Folder/Asset")` resolves its first argument to a mount point: `Game`,
`Engine`, or `Plugin.<Name>` / `Plugins.<Name>`. The plugin's content is not mounted in this
session.

**Fix.** Restart the editor so the plugin's mount point is registered.

## DSH8123

<!-- generated:begin DSH8123 -->
**Severity** error

**Message**

```
Relative asset Path(...) references require a root such as Game, Engine, or Plugin.PluginName.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:190`
<!-- generated:end DSH8123 -->

**Cause.** `Path("Folder/Asset")` with one argument has to be an absolute object path (`/Game/...`).
A relative path needs the root as its first argument.

**Fix.** Write `Path(Game, "Folder/Asset")`.

## DSH8124

<!-- generated:begin DSH8124 -->
**Severity** error

**Message**

```
Asset Path root '%s' has an invalid plugin name.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:210`
<!-- generated:end DSH8124 -->

**Cause.** The plugin part of a `Plugin.<Name>` root is empty or not a name.

**Fix.** Write `Path(Plugin.MyPlugin, "Folder/Asset")`.

## DSH8125

<!-- generated:begin DSH8125 -->
**Severity** error

**Message**

```
Asset Path root '%s' has an invalid plugin name.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:223`
<!-- generated:end DSH8125 -->

**Cause.** Same as DSH8124, for the `Plugins.<Name>` spelling.

**Fix.** Write `Path(Plugins.MyPlugin, "Folder/Asset")`.

## DSH8126

<!-- generated:begin DSH8126 -->
**Severity** error

**Message**

```
Unsupported asset Path root '%s'. Use Game, Engine, or Plugin.PluginName.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:233`
<!-- generated:end DSH8126 -->

**Cause.** The first argument of `Path(...)` is none of the roots DreamShader knows.

**Fix.** Use `Game`, `Engine`, or `Plugin.<Name>`.

## DSH8127

<!-- generated:begin DSH8127 -->
**Severity** error

**Message**

```
Asset reference cannot be empty.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:318`
<!-- generated:end DSH8127 -->

**Cause.** An asset reference is empty: `Path()`, an empty string, or a default left blank.

**Fix.** Name the asset, or remove the reference.

## DSH8128

<!-- generated:begin DSH8128 -->
**Severity** error

**Message**

```
Asset Path(...) reference is missing a closing ')'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:327`
<!-- generated:end DSH8128 -->

**Cause.** `Path(` is never closed.

**Fix.** Add the `)`.

## DSH8129

<!-- generated:begin DSH8129 -->
**Severity** error

**Message**

```
Asset Path(...) contains an unterminated string literal.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:334`
<!-- generated:end DSH8129 -->

**Cause.** A string inside `Path(...)` has no closing quote.

**Fix.** Close the string.

## DSH8130

<!-- generated:begin DSH8130 -->
**Severity** error

**Message**

```
Asset Path(...) expects either 1 argument (/Game/... path) or 2 arguments (Game|Engine|Plugin.PluginName, asset path).
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:348`
<!-- generated:end DSH8130 -->

**Cause.** `Path(...)` has no argument or more than two.

**Fix.** Write `Path("/Game/Folder/Asset")` or `Path(Game, "Folder/Asset")`.

## DSH8131

<!-- generated:begin DSH8131 -->
**Severity** error

**Message**

```
Asset reference requires a non-empty path.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:378`
<!-- generated:end DSH8131 -->

**Cause.** The path part of an asset reference is empty (`Path(Game, "")`).

**Fix.** Write the asset's folder and name.

## DSH8132

<!-- generated:begin DSH8132 -->
**Severity** error

**Message**

```
Invalid asset path '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderAssetReferenceResolution.cpp:402`
<!-- generated:end DSH8132 -->

**Cause.** What the reference resolves to is not a valid object path: it has a character the engine
refuses, a doubled `/`, or no asset name.

**Fix.** Copy the reference from the Content Browser (*Copy Reference*).

## DSH8149

<!-- generated:begin DSH8149 -->
**Severity** error

**Message**

```
DSH8149: '{0}' uses conditional compilation, and '{1}' holds only the branch that was taken -- adopting it would write that one branch back over the file and delete the rest. Move the change into the matching branch of the source by hand, or use DreamShader > Detach first if this asset should stop being generated from it.
```

**Raised by** `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:1132`, `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:497`, `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:696`, `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:875`
<!-- generated:end DSH8149 -->

**Cause.** **Adopt Into Source** was asked to rewrite a source that uses conditional compilation (`#if` / `#ifdef` / `#ifndef`). Adopt rebuilds the source text from the asset, and the asset only ever holds the branch that was taken -- so adopting would write that one branch back over the file and silently delete every other branch

**Fix.** move the change into the matching branch of the source by hand (the decompiler's output for the asset is a good starting point), or use **Detach From DreamShader** first if this asset should stop being generated from that source. Revert and Detach are unaffected, since neither writes the source

**See** [Preprocessor](../language/preprocessor.md), [Divergence](../generation/divergence.md)

## DSH8155

<!-- generated:begin DSH8155 -->
**Severity** warning

**Message**

```
rebuilding '%s' from '%s' dropped %d parameter override(s) the rebuilt material no longer declares: %s.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderThinCustomParameterOverrides.cpp:287`
<!-- generated:end DSH8155 -->

**Cause.** a rebuild of a ThinCustom material captured the parameter overrides set on the generated instance and put them back afterwards, but one or more of them named a parameter the rebuilt material no longer declares -- the source renamed it, removed it, or changed its kind -- so those values had nowhere to go and were dropped. Restoration matches by name and kind, because a name is the only thing that survives a graph the generator tears down and rebuilds. The same line reports an override of a parameter kind this build cannot re-apply (a texture collection, for instance)

**Fix.** nothing is broken and the rebuild succeeded; set the value again on the instance under its new name, or move it into the source as a `Properties` default so no override is needed. If the parameter was renamed and you want the value carried across, rename it back, rebuild, then rename once more in the same edit as the override

**See** [Divergence](../generation/divergence.md#parameter-overrides-on-a-generated-thincustom-instance)

## DSH8200

<!-- generated:begin DSH8200 -->
**Severity** error

**Message**

```
'{0}' does not resolve to a valid asset path. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1284`, `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:1104`
<!-- generated:end DSH8200 -->

**Cause.** The product's asset name, or the `/// @name /Game/...` path override, does not resolve
to a package path Unreal will accept. The quoted 1.x message says which rule broke: an empty leaf
after sanitising, a folder segment that sanitises to nothing, a `Root` naming a plugin that is not
enabled or cannot host content, or a path Unreal itself rejects.

**Fix.** Read the quoted message first — it names the offending segment. A `@name` that is a bare
leaf replaces only the asset name and keeps the root rules; a `@name` starting with `/` is a full
path whose last segment is the asset and whose leading segments are the root, so `/Game/X/M_Y`
puts `M_Y` under `/Game/X`. A file under a plugin source root defaults to that plugin's content
mount, so it needs no `/Game` prefix at all.

## DSH8201

<!-- generated:begin DSH8201 -->
**Severity** error

**Message**

```
The material for '{0}' could not be created or reused. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:528`
<!-- generated:end DSH8201 -->

**Cause.** The destination for a material product exists and is not a `UMaterial`, or it exists on
disk without DreamShader's source metadata, or the package could not be created. The last case is
rare and usually means the path is not writable.

**Fix.** For "already exists and was not generated by DreamShader": that guard is the reason a
hand-authored material at the same path is never cleared and overwritten. Rename the shader, or
move or delete the existing asset, then compile again.

## DSH8202

<!-- generated:begin DSH8202 -->
**Severity** error

**Message**

```
The material function for '{0}' could not be created or reused. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:749`
<!-- generated:end DSH8202 -->

**Cause.** As DSH8201, for a material function product — with one extra way to fail: the asset at
that path exists as the wrong function class. A `@layer` needs a `UMaterialFunctionMaterialLayer`,
a `@layerblend` needs a `UMaterialFunctionMaterialLayerBlend`, and a plain export needs a
`UMaterialFunction`. Changing the kind of an exported function changes the class it must become.

**Fix.** Delete or move the existing asset and regenerate it. The class cannot be changed in place:
every material that calls the function holds a typed reference to it.

## DSH8203

<!-- generated:begin DSH8203 -->
**Severity** error

**Message**

```
The ThinCustom instance for '{0}' could not be created or reused. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:604`
<!-- generated:end DSH8203 -->

**Cause.** As DSH8201, for the `UDreamShaderMaterialInstance` a ThinCustom material becomes.

**Fix.** Same as DSH8201. If the asset exists as a plain `UMaterial`, the file used to compile with
`Backend = "Graph"` and now asks for ThinCustom (or the project default changed); delete the old
material or pin the backend back with `#pragma material(Backend = "Graph")`.

## DSH8204

<!-- generated:begin DSH8204 -->
**Severity** error

**Message**

```
The emitter needs the builtin catalog the front end was bound against, but the emit context carries none.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1270`
<!-- generated:end DSH8204 -->

**Cause.** `EmitDreamShaderIRProduct` was called with no `FIREmitContext::Catalog`, or with an
empty one. The catalog is how the emitter turns an attribute name into an `EMaterialProperty` and
how it resolves a reflected class precisely, so it cannot proceed without it.

**Fix.** Internal: the pipeline builds the catalog once per run and must pass the SAME one the
binder used. If this reaches a user, the catalog manifest failed to load or reflection produced
nothing; the reflection filler only logs that case (`BuildBuiltinCatalogFromReflection` runs
before any compile and has no sink to raise into, so it has no code).

## DSH8205

<!-- generated:begin DSH8205 -->
**Severity** error

**Message**

```
Product index {0} does not exist in this module, which has {1}.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1259`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:252`
<!-- generated:end DSH8205 -->

**Cause.** An internal inconsistency: the product index is out of range for the module, or the
graph's topological order names a node the graph does not have.

**Fix.** Not an authoring error. Report it with the source file; `dsc dump-ir` on the same file
shows the graph the emitter was handed.

## DSH8206

<!-- generated:begin DSH8206 -->
**Severity** error

**Message**

```
'{0}' is open in an asset editor, so it was not rebuilt. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:390`
<!-- generated:end DSH8206 -->

**Cause.** The asset is open in an asset editor. An open editor does not edit the asset itself: it
duplicates it and copies the duplicate back on Apply or Save, so an editor open across a rebuild
holds a pre-rebuild copy and the next Apply silently reverts everything the rebuild did.

**Fix.** Close the asset editor and compile again. Refusing is the only safe answer available:
whether that copy has unsaved edits is private to the MaterialEditor module, and closing it blindly
would pop a save prompt in the middle of a compile-on-save.

## DSH8207

<!-- generated:begin DSH8207 -->
**Severity** error

**Message**

```
'{0}' no longer holds what DreamShader generated into it, so it was not rebuilt. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:400`
<!-- generated:end DSH8207 -->

**Cause.** The asset no longer holds what DreamShader last generated into it — somebody edited the
graph by hand. The output digest stamped at the end of the last successful generation no longer
matches the asset's contents.

**Fix.** The quoted message names the three ways out: Revert (throw the hand edits away and rebuild
from source), Adopt (decompile the current asset back into the source file), or Detach (drop
DreamShader's stamps so the asset stops being generated at all). Tuning parameter overrides on a
generated ThinCustom instance is NOT divergence and never raises this — that is what the instance
is for.

## DSH8208

<!-- generated:begin DSH8208 -->
**Severity** warning

**Message**

```
'{0}' could not be snapshotted before rebuilding it, so a failed rebuild will not be rolled back.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:444`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:799`
<!-- generated:end DSH8208 -->

**Cause.** The atomic rollback could not take a snapshot of the asset before the rebuild started,
so a rebuild that fails partway will leave the asset emptied rather than restoring it. The compile
continues; this is a warning about what happens IF something later fails.

**Fix.** Usually a sign the asset is in an unusual state (no editor-only data). If the compile then
succeeds, nothing was lost. If it fails, regenerate with the source fixed — the asset will rebuild
from scratch.

## DSH8209

<!-- generated:begin DSH8209 -->
**Severity** info

**Message**

```
'{0}' was left alone: another editor owns writing this project's generated assets to disk.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:365`
<!-- generated:end DSH8209 -->

**Cause.** Another editor holds this project's DreamShader bridge ownership lock, and only that
process writes generated assets to disk. Two editors saving the same package is not a merge; it is
a corrupted package or a dead editor.

**Fix.** Nothing to fix. Compile from the editor that owns the lock, or close the other one. This
is reported as a skip, not a failure.

## DSH8210

<!-- generated:begin DSH8210 -->
**Severity** warning

**Message**

```
'{0}' has no property named '{1}', so that value was not written.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1761`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:324`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:361`
<!-- generated:end DSH8210 -->

**Cause.** A `/// @key value` on a uniform, or a Group / Desc / SortPriority / slider, named
something the expression class does not reflect. The value was not written; everything else on the
parameter was.

**Fix.** Check the spelling against the node's Details panel — the key is the reflected property
name. Some parameter classes genuinely do not carry the organisation fields (`Group`,
`SortPriority`, `Desc`) because they are not `UMaterialExpressionParameter` subclasses; for those
this warning is expected and harmless. A key meant for a newer engine version is also fine to
leave: it starts working when the property appears.

## DSH8211

<!-- generated:begin DSH8211 -->
**Severity** error

**Message**

```
'{0}' is not a material expression class this engine has.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:509`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:665`
<!-- generated:end DSH8211 -->

**Cause.** A `UE.X(...)` or `UE.Expression(Class = "X")` names a material expression class this
engine build does not have. The binder resolved it against the catalog, so this usually means the
catalog and the running engine disagree — a stale exported catalog, or a class behind a build flag.

**Fix.** Re-export the catalog (`dsc export-catalog`) and compile again. If the class is genuinely
absent (a Substrate node on an engine without Substrate, a plugin class that is not loaded), use a
different node or guard the code with `#if`.

## DSH8212

<!-- generated:begin DSH8212 -->
**Severity** error

**Message**

```
MakeMaterialAttributes has no pin for the attribute '{0}'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:124`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:156`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:220`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1740`
<!-- generated:end DSH8212 -->

**Cause.** A value has no pin to go to on the node that was made. For a reflected node, the named
input matches no `FExpressionInput` on its expression class and no name the live node shows on a
pin; as with DSH8211 this normally means the catalog is stale, because the binder checked the pin
name against it. For the attribute nodes: `MakeMaterialAttributes` has a fixed list of pins, and
what it has none for (`FrontMaterial`, `SurfaceThickness`) is set by a `SetMaterialAttributes` on
top of it -- the code is raised when that Set could not take the attribute either, when the
attribute is the whole-material pseudo-attribute, which a set of attributes cannot hold, or when a
`SetMaterialAttributes` of its own could not take a value.

**Fix.** Re-export the catalog. If the pin was renamed between engine versions, use the new name.
For `MakeMaterialAttributes`, the eight customized UVs are one array pin, written `CustomizedUVs0`
through `CustomizedUVs7`.

## DSH8213

<!-- generated:begin DSH8213 -->
**Severity** error

**Message**

```
'{0}' has no property named '{1}'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1768`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1781`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:315`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:372`
<!-- generated:end DSH8213 -->

**Cause.** A reflected property refused the value: the wrong literal shape for its type. The quoted
message from the literal writer says which — not a number, not a valid enumerator, an object path
that does not load or loads to the wrong class.

**Fix.** For an enum, write the enumerator without its prefix (`Normal`, not `SAMPLERTYPE_Normal`;
either is accepted, the short form is the canonical one). For an asset, write a full object path
starting with `/`. For a number, remove any unit suffix.

## DSH8214

<!-- generated:begin DSH8214 -->
**Severity** error

**Message**

```
Failed to create a FunctionInput node.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:123`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:152`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:281`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:40`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:142`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:173`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:239`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:339`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:75`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1103`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1136`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1175`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1210`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1242`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1279`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1331`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1735`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:359`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:370`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:382`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:397`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:517`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:674`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:743`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:763`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:784`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:805`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:840`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:862`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:881`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:906`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:937`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:985`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:136`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:150`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:170`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:208`
<!-- generated:end DSH8214 -->

**Cause.** `UMaterialEditingLibrary` refused to create an expression of that class in this asset.
The common real cause is a class that is not allowed in the asset kind — several expressions are
material-only and cannot go in a material function, and vice versa.

**Fix.** Check whether the node is legal in a material function. If the graph needs it, put it in
the material that calls the function and pass the value in as a function input.

## DSH8215

<!-- generated:begin DSH8215 -->
**Severity** error

**Message**

```
A material setting on '{0}' was refused. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:460`
<!-- generated:end DSH8215 -->

**Cause.** A `#pragma material(...)` key is not a settable property path on `UMaterial`, or its
value is not valid for that property. The quoted 1.x message says which of the two.

**Fix.** The keys are `UMaterial` property names, plus the four friendly aliases `BlendMode` /
`RenderType`, `ShadingModel`, `MaterialDomain` / `Domain`. An array property needs an explicit
`[index]`. `Backend` is not applied here — it selects the backend and is consumed by the binder.

## DSH8216

<!-- generated:begin DSH8216 -->
**Severity** error

**Message**

```
BreakMaterialAttributes does not publish the attribute '{0}', so it cannot be read from a material that came through a pin.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1568`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1811`
<!-- generated:end DSH8216 -->

**Cause.** One of two things. A material attribute name (`m.SomeThing`, a `MakeMaterialAttributes`
input) is not in this engine's attribute table. Or an attribute was READ from a material that came
through a pin -- a LayerBlend input, a reflected node's material output, a function call's material
result -- and the `BreakMaterialAttributes` node that read becomes has no pin for it:
`MaterialAttributes` itself, `FrontMaterial`, `SurfaceThickness`. The second form is raised at the
read, never on the node: the node carries a slot for every attribute, and a slot nobody reads is not
an error.

**Fix.** For the second form, read the attribute where the material is still being built, before it
crosses the pin, or pass the value alongside the material. For the first, check the spelling. The accepted aliases are `Attributes` for MaterialAttributes,
`Emissive` for EmissiveColor, `AO` for AmbientOcclusion, `WPO` for WorldPositionOffset, `PDO` for
PixelDepthOffset, `ClearCoat` / `ClearCoatRoughness` for CustomData0 / CustomData1, and
`CustomizedUV0`..`7` for CustomizedUVs0..7.

## DSH8217

<!-- generated:begin DSH8217 -->
**Severity** error

**Message**

```
This material has no input for the attribute '{0}'; check the material domain and shading model the file asks for.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:387`
<!-- generated:end DSH8217 -->

**Cause.** The attribute exists, but this material has no input pin for it. A material's available
outputs depend on its domain, blend mode and shading model: a Post Process material has no
Roughness, an Opaque material has no Opacity.

**Fix.** Set the domain / blend mode / shading model the attribute needs in
`#pragma material(...)`, or stop writing that attribute.

## DSH8218

<!-- generated:begin DSH8218 -->
**Severity** error

**Message**

```
The default texture for parameter '{0}' could not be loaded from '{1}'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:236`
<!-- generated:end DSH8218 -->

**Cause.** A texture uniform's default asset path does not load.

**Fix.** Check the path — it must be a full object path (`/Game/Textures/T_X.T_X`, or
`/Game/Textures/T_X`). A texture that was moved or renamed needs the source updated; a redirector
is followed, a deleted asset is not.

## DSH8219

<!-- generated:begin DSH8219 -->
**Severity** error

**Message**

```
This function call names no material function asset.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:247`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:272`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:288`
<!-- generated:end DSH8219 -->

**Cause.** A `FunctionCall` node's material function asset could not be loaded, or the engine
refused to assign it to the call node. The refusal case is a cycle: a material function cannot call
itself, directly or through another function.

**Fix.** For a load failure, check the `@asset` path on the `extern` declaration. For a refusal,
break the cycle — two functions that call each other need the shared part factored into a third.

## DSH8220

<!-- generated:begin DSH8220 -->
**Severity** error

**Message**

```
'{0}' has no input named '{1}'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:331`
<!-- generated:end DSH8220 -->

**Cause.** A call names an input the function asset does not have. Either the function was rebuilt
with a different parameter list, or the `extern` prototype in the source does not match the asset.

**Fix.** Make the `extern` prototype match the asset's actual inputs. `dsc index` lists them; so
does the function's own Details panel.

## DSH8221

<!-- generated:begin DSH8221 -->
**Severity** error

**Message**

```
'{0}' has no output named '{1}'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:385`
<!-- generated:end DSH8221 -->

**Cause.** As DSH8220, for an output. A function's single unnamed return is called `Result`.

**Fix.** Match the `extern` prototype to the asset's outputs.

## DSH8222

<!-- generated:begin DSH8222 -->
**Severity** error

**Message**

```
This call targets product {0} of the same file, but that product has not been emitted yet; the pipeline must compile products in dependency order.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:229`
<!-- generated:end DSH8222 -->

**Cause.** A call to another exported function of the SAME file ran before that function's asset
had been emitted, so its path is not yet known. Internal: the pipeline compiles a file's products
in dependency order precisely so this cannot happen.

**Fix.** Not an authoring error unless the file has a call cycle between its exports, which the
binder should have refused first. Report it with the source file.

## DSH8223

<!-- generated:begin DSH8223 -->
**Severity** error

**Message**

```
The emitter has no rule for the IR operation '{0}'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1028`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:334`
<!-- generated:end DSH8223 -->

**Cause.** The IR carries an operation the emitter has no rule for: a core op whose table entry
names no engine expression class and that has no hand-written lowering.

**Fix.** Not an authoring error — a gap between the core-op table and the emitter. Report the
operation named in the message.

## DSH8224

<!-- generated:begin DSH8224 -->
**Severity** error

**Message**

```
A SetMaterialAttributes node has no MaterialAttributes input; there is nothing for it to modify.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:181`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:245`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1121`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1149`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1191`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1226`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1265`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1626`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:346`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:694`
<!-- generated:end DSH8224 -->

**Cause.** A node's operand or input count does not match its operation — a Select with two
operands, an Append with three, a `SetMaterialAttributes` with no base input. The IR validator
should have caught it.

**Fix.** Not an authoring error. `dsc dump-ir` on the file shows the malformed node.

## DSH8225

<!-- generated:begin DSH8225 -->
**Severity** error

**Message**

```
This node reads node {0}, which has not been emitted; the graph's topological order is inconsistent.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1553`
<!-- generated:end DSH8225 -->

**Cause.** A node reads a value from a node that has not been emitted yet, which means the graph's
topological order does not actually put operands before their users.

**Fix.** Not an authoring error. Report it with the source file; a cycle in the IR is the usual
cause and `dsc dump-ir` shows it.

## DSH8226

<!-- generated:begin DSH8226 -->
**Severity** error

**Message**

```
A Swizzle node carries no Mask property.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1057`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1066`
<!-- generated:end DSH8226 -->

**Cause.** A Swizzle node's `Mask` property is missing, empty, longer than four components, or
contains something other than `x`, `y`, `z`, `w`. The canonical mask is lower-case `xyzw` in order;
`rgba` is rewritten to it at bind time.

**Fix.** Not an authoring error at this stage — a mask a user wrote wrongly is refused earlier, with
a message that quotes it. Report it with the source file.

## DSH8227

<!-- generated:begin DSH8227 -->
**Severity** error

**Message**

```
'{0}' is not a Custom node output type; expected Float1 through Float4 or MaterialAttributes.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1369`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1402`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:1410`
<!-- generated:end DSH8227 -->

**Cause.** A `@custom` function's return type, or one of its `out` parameters, does not map to a
Custom node output type. The engine has exactly five: `Float1`, `Float2`, `Float3`, `Float4` and
`MaterialAttributes`.

**Fix.** Change the type. A matrix has no Custom node output type at all — compute it inside the
body and return a vector. Note that a `material` may be an OUTPUT of a custom node but never an
input.

## DSH8228

<!-- generated:begin DSH8228 -->
**Severity** error

**Message**

```
'{0}' is not a material function input type; write one of Scalar, Vector2, Vector3, Vector4, Texture2D, TextureCube, Texture2DArray, VolumeTexture, StaticBool, Bool, MaterialAttributes or Substrate.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:55`
<!-- generated:end DSH8228 -->

**Cause.** A material function input's type does not map to an `EFunctionInputType`.

**Fix.** The engine's list is Scalar, Vector2, Vector3, Vector4, Texture2D, TextureCube,
Texture2DArray, VolumeTexture, TextureExternal, StaticBool, Bool, MaterialAttributes and Substrate.
A matrix or a user struct cannot be a function input; pass its components separately.

## DSH8229

<!-- generated:begin DSH8229 -->
**Severity** error

**Message**

```
'{0}' was built but could not be saved. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1216`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:571`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:720`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:875`
<!-- generated:end DSH8229 -->

**Cause.** The asset was built successfully but its package could not be written to disk — read-only
file, source control, or a path that no longer exists.

**Fix.** Check out the asset, or make the file writable, and compile again. The graph is already
built in memory, so the next compile will save it without rebuilding.

## DSH8230

<!-- generated:begin DSH8230 -->
**Severity** error

**Message**

```
Cannot create a ThinCustom base material without an instance.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:297`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:314`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:340`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:643`
<!-- generated:end DSH8230 -->

**Cause.** The hidden base `UMaterial` a ThinCustom instance parents to could not be created as a
subobject of the instance.

**Fix.** Not usually an authoring error. If it persists, delete the instance asset and regenerate
it — an instance from an older build that parents to a separate sibling material is not reused, and
a fresh pair is created.

## DSH8232

<!-- generated:begin DSH8232 -->
**Severity** error

**Message**

```
'{0}' has a product kind the emitter does not know how to materialize.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1313`
<!-- generated:end DSH8232 -->

**Cause.** The product kind has no asset shape the emitter knows how to build. Internal.

**Fix.** Report it with the source file.

## DSH8233

<!-- generated:begin DSH8233 -->
**Severity** error

**Message**

```
This graph carries a MaterialSink, which only a material product has; a material function drives FunctionOutput nodes instead.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterMaterial.cpp:372`
<!-- generated:end DSH8233 -->

**Cause.** A graph carries a `MaterialSink` but is being emitted into a material function. Only a
material product has output pins of its own; a function drives `FunctionOutput` nodes.

**Fix.** Not an authoring error. The binder decides the product kind from the function's signature
(`void (inout material)` is the entry); report it with the source file.

## DSH8234

<!-- generated:begin DSH8234 -->
**Severity** error

**Message**

```
Function output '{0}' has {1} operands; it needs exactly the one value it returns.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:179`
<!-- generated:end DSH8234 -->

**Cause.** A `FunctionOutput` node has no operand, so the output pin would drive nothing.

**Fix.** Not an authoring error — an exported function whose output is never assigned is refused
earlier, by name. Report it with the source file.

## DSH8235

<!-- generated:begin DSH8235 -->
**Severity** error

**Message**

```
'{0}' is not a sampler type; write one of the EMaterialSamplerType names, such as Color, Normal or LinearColor.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:278`
<!-- generated:end DSH8235 -->

**Cause.** A texture uniform's `@sampler` names something that is not an `EMaterialSamplerType`.

**Fix.** Write one of the engine's sampler type names without its prefix: `Color`, `Grayscale`,
`Alpha`, `Normal`, `Masks`, `DistanceFieldFont`, `LinearColor`, `LinearGrayscale`, `Data`,
`External`, `VirtualColor` and the rest of the `SAMPLERTYPE_*` list. `Color` is the default.

## DSH8237

<!-- generated:begin DSH8237 -->
**Severity** info

**Message**

```
'{0}' was left alone: its source hash is unchanged since it was last built.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:377`
<!-- generated:end DSH8237 -->

**Cause.** The asset this product compiles to already carries the source hash of this build: the
preprocessed text of the file and of every header it includes, plus the defines it read. The compile
was not forced, so nothing was rebuilt. The compile result says
`Skipped <asset> from <source>; source hash is unchanged (build key <hash>).`, the line the 1.x
generator writes for the same skip, and it does not list the asset as generated.

**Fix.** Nothing to fix; it is a skip, not a failure. Compile with `-Force` (or Recompile) to rebuild
anyway. The skip leaves an asset edited by hand untouched. Once the source changes, the next compile
reaches the divergence gate instead (DSH8207).

## DSH8240

<!-- generated:begin DSH8240 -->
**Severity** error

**Message**

```
The material instance for '{0}' could not be created or reused. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1030`
<!-- generated:end DSH8240 -->

**Cause.** The instance asset of a `.dsi` could not be created or reused; the sentence after it is
the reason (DSH8241, DSH8242 or DSH8253 in words).

**Fix.** Act on that reason.

## DSH8241

<!-- generated:begin DSH8241 -->
**Severity** error

**Message**

```
Asset '%s' already exists as a '%s'; a .dsi builds a plain MaterialInstanceConstant. Rename the instance file or move the existing asset.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:252`
<!-- generated:end DSH8241 -->

**Cause.** An asset of another class already sits where the `.dsi` would build its instance -- a
material, or a ThinCustom instance of a `.dss` with the same name.

**Fix.** Rename the `.dsi` (its file name is the asset's name) or move the existing asset.

## DSH8242

<!-- generated:begin DSH8242 -->
**Severity** error

**Message**

```
Asset '%s' already exists and was not generated by DreamShader. Rename the instance file or move/delete the existing asset before building it.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:261`
<!-- generated:end DSH8242 -->

**Cause.** A material instance already sits at the target path and was not generated by DreamShader.

**Fix.** Rename the `.dsi`, or adopt the existing instance into a source with *DreamShader > Adopt
Into Source* / `dsc decompile`.

## DSH8243

<!-- generated:begin DSH8243 -->
**Severity** error

**Message**

```
The parent '{0}' of '{1}' does not load; compile the source that builds it, or correct the Parent key.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1057`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1082`
<!-- generated:end DSH8243 -->

**Cause.** The parent named by `Parent = ...` could not be loaded, or it exists only in memory -- a
product of this same run -- and could not be saved before the instance that needs it on disk. A bare
name is looked up among the products of the sources under the same root; an object path is loaded as
written.

**Fix.** Compile the source that builds the parent, or correct the key. For the in-memory case the
message carries the save error: free the package file it names.

## DSH8244

<!-- generated:begin DSH8244 -->
**Severity** info

**Message**

```
'{0}' was memory-only, so it was saved to disk first: '{1}' parents to it.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1090`
<!-- generated:end DSH8244 -->

**Cause.** The parent was built into memory only (an Ephemeral build) and an instance on disk cannot
point at an object that is not. The parent was saved first.

**Fix.** Nothing to fix.

## DSH8245

<!-- generated:begin DSH8245 -->
**Severity** error

**Message**

```
'{0}' cannot parent to '{1}': that parent already descends from this instance.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1101`
<!-- generated:end DSH8245 -->

**Cause.** `Parent` names an instance that itself descends from this one, so the parent chain would
be a loop.

**Fix.** Point `Parent` at a material or at an instance further up.

## DSH8246

<!-- generated:begin DSH8246 -->
**Severity** error

**Message**

```
The parent asset '{0}' has no parameter {1} of the kind this instance overrides; the asset is older than its source. Compile the parent source first.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1123`
<!-- generated:end DSH8246 -->

**Cause.** The parent asset on disk lacks a parameter the instance overrides, although the parent's
source declares it: the parent asset is older than its source.

**Fix.** Compile the parent's source, then the instance.

## DSH8247

<!-- generated:begin DSH8247 -->
**Severity** error

**Message**

```
The engine dropped '{0}' as the parent of '{1}' while applying the static overrides; that parent does not allow them.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1193`
<!-- generated:end DSH8247 -->

**Cause.** Applying the static switch overrides made the engine clear the instance's parent. It does
that when the parent may not get new shader permutations: a cooked material (content shipped without
its editor data), for which the engine refuses to compile a permutation of the instance's own.

**Fix.** Parent to an uncooked material, or drop the `/// @static` overrides so the instance shares
its parent's shaders.

## DSH8248

<!-- generated:begin DSH8248 -->
**Severity** error

**Message**

```
The parent '{0}' exists only in memory and no DreamShader source builds it, so '{1}' cannot be saved against it; save the parent first.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1070`
<!-- generated:end DSH8248 -->

**Cause.** The parent exists only in memory and no DreamShader source builds it (a material created
in the editor and never saved), so the instance has nothing on disk to reference.

**Fix.** Save the parent asset first.

## DSH8249

<!-- generated:begin DSH8249 -->
**Severity** error

**Message**

```
'{0}' is not an instance key; a key is a material property an instance can override, such as BlendMode, TwoSided, OpacityMaskClipValue or PhysMaterial.
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderInstanceSettings.cpp:365`
<!-- generated:end DSH8249 -->

**Cause.** A key of `#pragma instance(...)` is not something a material instance can override. The
keys are the fields of the engine's base property overrides (`BlendMode`, `TwoSided`,
`ShadingModel`, `OpacityMaskClipValue`, `DitheredLODTransition`, ...) and a fixed table of instance
properties (`PhysMaterial`, `SubsurfaceProfile`, ...).

**Fix.** Check the spelling, or set the property on the parent material, which is where everything
else lives.

## DSH8250

<!-- generated:begin DSH8250 -->
**Severity** error

**Message**

```
'{0}' is not a valid value for the instance key '{1}'. {2}
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderInstanceSettings.cpp:374`
<!-- generated:end DSH8250 -->

**Cause.** The value of an instance key does not fit its property; the sentence after it says what
was expected.

**Fix.** Enum values go without their prefix (`Translucent`), bools as `true` / `false`, assets as
object paths or `None`.

## DSH8251

<!-- generated:begin DSH8251 -->
**Severity** error

**Message**

```
'{0}' cannot be set from an instance file: {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderInstanceSettings.cpp:341`
<!-- generated:end DSH8251 -->

**Cause.** The key names something an instance file does not set that way; the sentence after it
says which: `Backend` (an instance has none), `UsageFlags` (a bitmask the engine merges with the
parent's), the `BasePropertyOverrides` struct as a whole or one of its `bOverride...` flags (a flag
follows from the key that sets its value), or a `...ParameterValues` array (parameters are `uniform`
overrides).

**Fix.** Write the single key (`BlendMode = Translucent`) or the `uniform` override the message
points to.

## DSH8252

<!-- generated:begin DSH8252 -->
**Severity** error

**Message**

```
The override of '{0}' could not be applied: {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1141`
<!-- generated:end DSH8252 -->

**Cause.** One override could not be written into the instance; the reason follows. Usually the
asset a texture-like override names does not load or is of the wrong class.

**Fix.** Fix the `/// @default` path of that override.

## DSH8253

<!-- generated:begin DSH8253 -->
**Severity** error

**Message**

```
Failed to create package '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:273`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:280`
<!-- generated:end DSH8253 -->

**Cause.** The instance's package, or the `UMaterialInstanceConstant` in it, could not be created:
`CreatePackage` or `NewObject` failed for the path the message names.

**Fix.** Check where the `.dsi` lies: its folder under the source root becomes the package path, and
the file's name the asset's.

## DSH8254

<!-- generated:begin DSH8254 -->
**Severity** error

**Message**

```
The material parameter collection '{0}' has no parameter called '{1}'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:578`
<!-- generated:end DSH8254 -->

**Cause.** A `CollectionParameter` node names a parameter its material parameter collection does not
have. The engine stores the parameter by id and reads the name back from it, so a name without an id
would silently become `None`.

**Fix.** Use a parameter name the collection asset has (scalar or vector), or add it to the
collection.

## DSH8260

<!-- generated:begin DSH8260 -->
**Severity** error

**Message**

```
The parent '{0}' comes from '{1}', which does not compile, so the parameters this instance overrides cannot be checked; compile that source to see why.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:498`, `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:530`, `Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:591`, `Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:620`
<!-- generated:end DSH8260 -->

**Cause.** The parent of a `.dsi` cannot be used. It is built by a DreamShader source that does not
compile, so there is no parameter list to check the overrides against and no asset to build against;
or `Parent` does not resolve to an asset path; or it names nothing: no asset exists there, and no
source under the source roots builds it.

**Fix.** Compile the parent's source and fix what it reports, or correct `Parent`.

## DSH8261

<!-- generated:begin DSH8261 -->
**Severity** error

**Message**

```
No material or instance named '{0}' is built by a source under '{1}'; write the parent's asset path, or check the name.
```

**Raised by** `Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:656`
<!-- generated:end DSH8261 -->

**Cause.** `Parent` is a bare name, and no source under this file's source root builds a material or
instance of that name.

**Fix.** Check the name, or write the parent's object path (`/Game/...`).

## DSH8262

<!-- generated:begin DSH8262 -->
**Severity** error

**Message**

```
'{0}' names more than one product under '{1}' ({2}); write the parent's asset path instead.
```

**Raised by** `Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:670`
<!-- generated:end DSH8262 -->

**Cause.** `Parent` is a bare name that more than one source under the root builds.

**Fix.** Write the parent's object path instead.

## DSH8263

<!-- generated:begin DSH8263 -->
**Severity** error

**Message**

```
'{0}' is its own ancestor: following Parent from it comes back to it ({1}).
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:442`, `Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:602`, `Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:650`
<!-- generated:end DSH8263 -->

**Cause.** An instance is its own ancestor: `Parent` names the instance itself, by path or by name,
or following `Parent` from it leads back to it (the message lists the chain).

**Fix.** Break the cycle: point `Parent` at the material the chain should end in.

## DSH8264

<!-- generated:begin DSH8264 -->
**Severity** info

**Message**

```
'{0}' was missing or older than its source, so '{1}' was compiled first.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:519`
<!-- generated:end DSH8264 -->

**Cause.** The parent's asset was missing or older than its source when the instance was compiled,
so the parent was compiled first.

**Fix.** Nothing to fix.

## DSH8265

<!-- generated:begin DSH8265 -->
**Severity** error

**Message**

```
The Parent chain above '{0}' is more than {1} instances deep; a chain that long is almost always a mistake in a Parent key.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:450`
<!-- generated:end DSH8265 -->

**Cause.** The chain of `Parent` keys above this instance is deeper than the compiler follows. Real
instance chains are a handful deep.

**Fix.** Look for a `Parent` that points the wrong way.

## DSH8270

<!-- generated:begin DSH8270 -->
**Severity** error

**Message**

```
The function reference '{0}' does not resolve to an asset path. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterFunctions.cpp:259`
<!-- generated:end DSH8270 -->

**Cause.** The `/// @asset` of an `extern` function (or the `Asset` of a 1.x VirtualFunction) does
not resolve to an object path; the reason follows (one of DSH8118-DSH8132 in words).

**Fix.** Fix the reference; *Copy Reference* in the Content Browser gives a path that always
resolves.

## DSH8271

<!-- generated:begin DSH8271 -->
**Severity** error

**Message**

```
The default texture reference '{0}' of parameter '{1}' does not resolve to an asset path. {2}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterParameters.cpp:221`
<!-- generated:end DSH8271 -->

**Cause.** The `/// @default` texture of a parameter does not resolve to an object path; the reason
follows.

**Fix.** Fix the reference.

## DSH8290

<!-- generated:begin DSH8290 -->
**Severity** error

**Message**

```
'{0}' could not be read.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:638`
<!-- generated:end DSH8290 -->

**Cause.** The compiler could not open the `.dss` it was asked to compile. The path was normalised
first, so the message shows the absolute path it actually tried. Usual causes: the file was deleted
or renamed between the watcher noticing it and the compile starting, a `-Source=` that named a file
that does not exist (the commandlet resolves a relative path against `DShader/` and then against the
project directory, and falls back to the literal text when neither exists), or a file locked by
another process.

**Fix.** Check the path in the message. If it looks right, check that the file is there and readable;
on Windows a text editor holding an exclusive lock is enough to cause this.

## DSH8291

<!-- generated:begin DSH8291 -->
**Severity** error

**Message**

```
'{0}' failed conditional compilation: {1}: {2}
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:660`, `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:227`
<!-- generated:end DSH8291 -->

**Cause.** The conditional-compilation preprocessor refused the file. The message carries the
preprocessor's own DSH103x code and its message, because those say what was actually wrong — an
unbalanced `#if`, an `#elif` after `#else`, an expression it cannot evaluate, a `#define` of a
reserved `DS_` name. This code adds the one thing the preprocessor could not know: which file of the
compile it was, which matters because a header is preprocessed on its own and its failure is
reported against the `.dss` that included it.

**Fix.** Read the inner code, not this one: `Docs/diagnostics/DSH1xxx.md` has the rule each one
enforces. Remember that `#define` is file-local in DreamShader — a name defined in a `.dsh` is not
visible to the `.dss` that includes it, so a condition that depends on one has to define it in the
file that tests it, or the define belongs in the project settings table.

## DSH8292

<!-- generated:begin DSH8292 -->
**Severity** error

**Message**

```
'{0}', included from '{1}', could not be resolved: {2}.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:173`
<!-- generated:end DSH8292 -->

**Cause.** An `#include` (or `import`) named a file that no source root contains. The resolution
rules, in order: an absolute path that exists is itself; a specifier starting with `/` is
root-anchored and is tried against the including file's own root first and then every other root,
`DShader/` before `DShader/Packages/`; anything else goes through the 1.x import rules — the root
qualifier (`Plugin.MoonToon:Shared/Common.dsh`), then the including file's own directory, then its
root, then its Packages tree. A specifier with no extension gains `.dsh`.

An unqualified include never leaves the including file's root. That is deliberate: two plugins that
ship the same relative path cannot shadow one another, and enabling or disabling a plugin cannot
silently change what another root's include means.

**Fix.** Check the spelling and the extension. To reach another root, qualify it —
`#include "Plugin.MoonToon:Shared/Common.dsh"`. To anchor at the root rather than beside the
including file, start the path with `/`.

## DSH8293

<!-- generated:begin DSH8293 -->
**Severity** error

**Message**

```
'{0}', included from '{1}', resolved but could not be read.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:208`
<!-- generated:end DSH8293 -->

**Cause.** The include resolved to a real path, and opening it failed. Unlike DSH8292 the file was
found, so this is a permissions or locking problem rather than a spelling one.

**Fix.** Check that the file is readable. A `.dsh` under a plugin's `DShader/` that was shipped
read-only is still readable; a file open exclusively in another tool is not.

## DSH8294

<!-- generated:begin DSH8294 -->
**Severity** error

**Message**

```
'{0}', included from '{1}', could not be parsed; its own errors are above.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:257`
<!-- generated:end DSH8294 -->

**Cause.** An included header did not parse. Its own parse errors were reported first, against the
header's own file and line; this code is the "and therefore" line that names which include failed and
which file pulled it in, so the failure is attributable when a header is included from twenty places.

**Fix.** Fix the header's own errors — they are the ones directly above this in the log, and they
carry the header's line numbers. A header is reported once per compile however many files include it.

## DSH8295

<!-- generated:begin DSH8295 -->
**Severity** error

**Message**

```
'{0}' is not a DreamShader header; an include names a '.dsh' (or a '.dss'), not a '{1}' file.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:197`
<!-- generated:end DSH8295 -->

**Cause.** An include resolved to a file that is not a DreamShader header. Only `.dsh` (and, for a
deliberate cross-compilation-unit include, `.dss`) can be included; a `.dsm` or `.dsf` is 1.x source
with a different grammar, and an include of one would be parsed as 2.0 text and fail line by line.

**Fix.** Include the `.dsh` the declarations live in. A 1.x `.dsm`/`.dsf` cannot be shared with a
2.0 source; move the shared declarations into a `.dsh`, which both front ends read.

## DSH8296

<!-- generated:begin DSH8296 -->
**Severity** error

**Message**

```
'{0}' is not a source the compiler builds on its own; it builds '.dss', '.dsi', '.dsp', '.dsm' and '.dsf' files, and a '.dsh' header only through the source that includes it.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:581`
<!-- generated:end DSH8296 -->

**Cause.** The 2.0 pipeline was handed a file whose extension is not `.dss`. `.dsh` answers this too,
on purpose: a header produces no asset and is compiled only as part of the `.dss` that includes it,
so compiling one directly could only ever produce nothing.

**Fix.** Compile the `.dss` that includes the header. `.dsm` and `.dsf` go to `compile`, which routes
them to the 1.x generator.

## DSH8297

<!-- generated:begin DSH8297 -->
**Severity** error

**Message**

```
The builtin expression catalog came back empty, so nothing that names a 'UE.*' node can be bound. Reflection found no UMaterialExpression classes, which normally means the Engine module is not loaded.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:777`
<!-- generated:end DSH8297 -->

**Cause.** The builtin expression catalog — every `UE.*` node, its pins, its properties and the
material attribute table, taken from UE reflection — came back with no entries. Nothing that names a
builtin can be bound against an empty catalog, so the compile stops rather than reporting every
`UE.*` in the file as an unknown name.

In practice this means reflection found no `UMaterialExpression` subclass at all, which happens only
when the Engine module is not loaded — a commandlet started without the editor modules, or a very
early call during module startup.

**Fix.** Not a source problem. Run the compile from the editor or from `-run=DreamShader`, both of
which have the Engine module. If it happens inside an editor, run `dsc export-catalog` and look at
what the manifest holds — an empty manifest confirms the reflection pass, a full one points at the
cached copy and `InvalidateDreamShaderBuiltinCatalog`.

## DSH8298

<!-- generated:begin DSH8298 -->
**Severity** error

**Message**

```
Building '{0}' was cancelled; the asset is as it was before this compile.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:316`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:1165`, `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:620`
<!-- generated:end DSH8298 -->

**Cause.** The user pressed Cancel on the compile's progress dialog. Nothing was written: the emit
is the last stage of the run, and a cancel is checked before every stage and before every product.

**Fix.** Nothing. Recompile when you want the result. A compile that takes long enough to be worth
cancelling is usually one whose shader compilation has started behind it; that half continues in the
engine's own queue and is not cancelled by this.

## DSH8299

<!-- generated:begin DSH8299 -->
**Severity** error

**Message**

```
The exported functions {0} call one another in a cycle, so there is no order in which they can be built; an exported function may call another only in one direction.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:982`
<!-- generated:end DSH8299 -->

**Cause.** Two or more exported functions of one file call one another, directly or through others.
Each exported function becomes its own material function asset, and a call to one binds against the
LIVE asset — the emitter reads the callee's pins off the object, not off the source — so there has to
be an order in which every callee is built before its callers. A cycle has no such order.

The message names every product in the cycle.

**Fix.** Break the cycle. Usually one of the two functions does not need to be exported: a `Helper`
(no `export`) is inlined at its call site and takes part in no dependency at all, so making the
inner one a helper removes the edge. If both really must be assets, factor the shared part into a
third function that neither calls back into.

## DSH8300

<!-- generated:begin DSH8300 -->
**Severity** error

**Message**

```
'{0}' is a Custom Pass pipeline, which needs Unreal Engine 5.8 or later; this engine has the DreamShaderPass asset types but no runtime to run them, so nothing was built.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:999`
<!-- generated:end DSH8300 -->

**Cause.** A `.dsp` was compiled on an engine older than Unreal Engine 5.8. The DreamShaderPass module
builds there with its asset types only (`DREAMSHADER_WITH_CUSTOM_PASS` is `0`): no runtime runs a
pipeline and no global shader holds an HLSL pass, so the emitter stops before it creates or touches
anything. The front half still runs — the file parses, formats and binds, with the checks that need the
runtime skipped (DSH7360) — so `check` passes where `compile` cannot.

**Fix.** Build the pipeline on Unreal Engine 5.8 or later. Nothing needs undoing on the older engine: no
pipeline, render target, slot or snapshot was written.

## DSH8301

<!-- generated:begin DSH8301 -->
**Severity** error

**Message**

```
Failed to create package '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:1009`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:1039`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:341`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:346`
<!-- generated:end DSH8301 -->

**Cause.** The pipeline asset could not be made at the path its `.dsp` resolves to. The message
`The pass pipeline for '…' could not be created or reused.` is followed by the reason with its own code:
DSH8301 again, `Failed to create package '…'` or `Failed to create the pass pipeline '…'`, when
`CreatePackage` or `NewObject` refused the path, which is rare; or DSH8302 / DSH8303 when something
already sits there. A third form, `'…' does not resolve to a valid asset path`, is the destination
check, which a compile makes first and reports as DSH8200.

**Fix.** Act on the inner reason. For a package that cannot be created, check where the `.dsp` lies: a
`.dsp` has no `/// @name`, so its folder under the source root gives the package path and its file name
the asset's.

## DSH8302

<!-- generated:begin DSH8302 -->
**Severity** error

**Message**

```
Asset '%s' already exists as a '%s'; a .dsp builds a DreamPassPipeline. Rename the .dsp or move the existing asset.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:324`
<!-- generated:end DSH8302 -->

**Cause.** An asset of another class already sits where the `.dsp` would build its pipeline — a
material of the same name in the same folder, for instance. A `.dsp` names its pipeline after its file.
Quoted inside DSH8301's message.

**Fix.** Rename the `.dsp` (its file name is the pipeline's name) or move the existing asset.

## DSH8303

<!-- generated:begin DSH8303 -->
**Severity** error

**Message**

```
Asset '%s' already exists and was not generated by DreamShader. Rename the .dsp or move/delete the existing asset before building it.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:331`
<!-- generated:end DSH8303 -->

**Cause.** A `UDreamPassPipeline` is saved at the pipeline's path without DreamShader's source stamp: one
made by hand, or one detached from DreamShader. The ownership guard never takes over an asset it did not
generate. Quoted inside DSH8301's message.

**Fix.** Rename the `.dsp`, or move or delete the existing pipeline, and compile again. To carry a
hand-made pipeline's settings into the `.dsp` first, decompile it (`./dsc.ps1 decompile <asset path>`)
and copy what you need.

## DSH8304

<!-- generated:begin DSH8304 -->
**Severity** error

**Message**

```
'{1}' is not a {0} the Custom Pass runtime knows; the pipeline was not built. This is a compiler gap: the binder should have refused it.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:581`
<!-- generated:end DSH8304 -->

**Cause.** A value of the pipeline reached the emitter in a spelling the DreamShaderPass runtime has no
enumerator for; the message names the kind of value (an injection point, a view kind, a buffer format, a
pass kind, a mesh pass's mode, depth, cull, blend, usage or Nanite policy, a param's source or constant
type) and the spelling. The binder checks every one of these first, so the two disagree and the file is
not at fault. Nothing was built.

**Fix.** Report it with the `.dsp`. Until it is fixed, write the value as
[Custom Pass pipelines](../language-v2/passes.md) spells it.

## DSH8305

<!-- generated:begin DSH8305 -->
**Severity** error

**Message**

```
The material '{0}' of pass '{1}' does not load; compile the source that builds it, or correct the Material key.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:760`
<!-- generated:end DSH8305 -->

**Cause.** When the pipeline was put together, the material a fullscreen or mesh pass names did not load
from the object path its `Material` resolved to. The checks before the build found it, and a compile
builds a material that is missing or older than its source first (DSH8332), so the asset went missing in
between: deleted, renamed without a redirector, or a package that no longer loads.

**Fix.** Compile the source that builds the material, or correct the `Material` key (a bare name, or the
material's object path), then compile the `.dsp` again.

## DSH8306

<!-- generated:begin DSH8306 -->
**Severity** error

**Message**

```
The default of '{0}' could not be applied: {1}.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:642`
<!-- generated:end DSH8306 -->

**Cause.** The `/// @default` of a `Texture2D` uniform could not be written into the pipeline: the
reference does not resolve to an asset path (the reason follows, one of DSH8118-DSH8132 in words), or no
texture loads from the path it resolves to. The binder takes the default as written, so this is the
first time it is loaded.

**Fix.** Correct the path — *Copy Reference* in the Content Browser gives one that always resolves — or
write `/// @default None` (or no `@default`) for a parameter without a texture.

## DSH8307

<!-- generated:begin DSH8307 -->
**Severity** error

**Message**

```
Pass '{0}' selects the layer '{1}', which is not one of the project's pass layers (Project Settings > DreamPlugin > DreamShader Custom Pass > Layer Names).
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:836`
<!-- generated:end DSH8307 -->

**Cause.** A mesh pass's `Filter = Layer(...)` names a layer that has no bit when the pipeline is built.
The binder accepts any name the project's layer table lists (an unlisted one is DSH4412), but only the
first 32 entries of the table have a bit, so a name listed after the 32nd is refused here. Nothing was
built.

**Fix.** Move the layer into the first 32 entries of Project Settings ▸ DreamPlugin ▸ DreamShader Custom
Pass ▸ Layer Names, or select one that is. Moving a layer re-targets every compiled pipeline that names
it until that pipeline is compiled again.

## DSH8308

<!-- generated:begin DSH8308 -->
**Severity** error

**Message**

```
Buffer '{0}' is {1} and cannot be exported: materials, Blueprints and UMG read an exported buffer as a float texture, which an integer or a depth buffer cannot be. Export a float or normalized buffer, or drop Export.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:687`
<!-- generated:end DSH8308 -->

**Cause.** A buffer declared `Export = true` has an integer or depth format (`R32U`, `RG32U`, `Depth32`).
An exported buffer becomes a render target that materials, Blueprints and UMG sample as a float texture,
which neither can be. The binder refuses the same declaration first (DSH7355); this is the emitter's own
check as it maps the format onto a render target format.

**Fix.** Export a float or normalized buffer, or drop `Export`. An id fits `R32F` exactly up to
16777216.

## DSH8309

<!-- generated:begin DSH8309 -->
**Severity** error

**Message**

```
The render target of the exported buffer '{0}' could not be created or reused. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:1144`
<!-- generated:end DSH8309 -->

**Cause.** The render target of an exported buffer — `<Pipeline>_<Buffer>`, in the pipeline's folder —
could not be created or taken over; the sentence after it is the reason, with its code (DSH8313 or
DSH8314). Render targets are found or made before anything is written, so the pipeline, the slot
registry and the snapshots are as they were.

**Fix.** Act on that reason.

## DSH8310

<!-- generated:begin DSH8310 -->
**Severity** warning

**Message**

```
'{0}' was left in place although its buffer is no longer exported: {1}. Delete it by hand once nothing reads it.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:559`
<!-- generated:end DSH8310 -->

**Cause.** A buffer the pipeline used to export is no longer exported — its `Export` dropped, the buffer
renamed or removed — so its render target was to be deleted once the pipeline was saved without it. It
was kept: something else still references it (the message lists the packages, typically a material
whose `UE.DreamPassBuffer` reads that buffer), or, with nobody listed, the delete was refused in this
process. The pipeline itself built and saved.

**Fix.** Point whatever reads the buffer at one the pipeline still exports and rebuild it, then delete
the render target in the Content Browser. If the export was dropped by mistake, put `Export = true` back:
the next compile takes the same render target over again, and its readers stay valid.

## DSH8311

<!-- generated:begin DSH8311 -->
**Severity** error

**Message**

```
'{0}' was built but could not be saved with its render targets; its slots are already in the registry. In this session the pipeline in memory is current, so a plain compile of its source skips it: save it, or compile the source again with -Force. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:1253`
<!-- generated:end DSH8311 -->

**Cause.** The pipeline was built and its render targets configured, and saving the packages failed — a
file that is read-only because it is not checked out, or a path that no longer exists; the asset
layer's reason follows. Everything before the save stands: the slot registry and the snapshots are
written, and in the editor the slot shaders were recompiled and the pipeline in memory already runs the
new version.

**Fix.** Make the packages writable. In a new process — a commandlet, the next editor session —
compiling the `.dsp` rebuilds and saves it. In the same editor session the pipeline in memory already
carries the new source hash, so a plain compile skips it (DSH8237) and saves nothing: save it
(File ▸ Save All), or compile the source again with `-Force` (or Recompile).

## DSH8312

<!-- generated:begin DSH8312 -->
**Severity** info

**Message**

```
'{0}' was deleted: its buffer is no longer exported.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:547`
<!-- generated:end DSH8312 -->

**Cause.** Informational. A buffer the pipeline used to export is no longer exported, and nothing but the
pipeline referenced its render target, so the render target was deleted once the pipeline was saved
without it.

**Fix.** Nothing. If you commit generated assets, commit the deletion with the pipeline.

## DSH8313

<!-- generated:begin DSH8313 -->
**Severity** error

**Message**

```
Asset '%s' already exists as a '%s'; an exported buffer needs a TextureRenderTarget2D there. Move the existing asset or rename the buffer.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:372`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:379`
<!-- generated:end DSH8313 -->

**Cause.** Something other than this pipeline's render target sits where an exported buffer's render
target goes (`<Pipeline>_<Buffer>`, next to the pipeline): an asset of another class, or a
`TextureRenderTarget2D` without DreamShader's source stamp — one made by hand, which is never taken over.
Quoted inside DSH8309's message.

**Fix.** Move or rename the existing asset, or rename the buffer.

## DSH8314

<!-- generated:begin DSH8314 -->
**Severity** error

**Message**

```
Failed to create package '%s'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:389`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterPassPipeline.cpp:394`
<!-- generated:end DSH8314 -->

**Cause.** The package of an exported buffer's render target, or the `TextureRenderTarget2D` in it, could
not be created: `CreatePackage` or `NewObject` failed for the path the message names. Rare; quoted
inside DSH8309's message.

**Fix.** Check that path — the pipeline's folder and `<Pipeline>_<Buffer>` — for something the engine
refuses in a package name.

## DSH8315

<!-- generated:begin DSH8315 -->
**Severity** error

**Message**

```
The Custom Pass slot registry cannot be read: {0}. Nothing was written, because writing over it would lose every slot it records; fix the file, or run 'dsc pass-registry -Rebuild'.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:897`
<!-- generated:end DSH8315 -->

**Cause.** Every `.dsp` compile reads the project's slot registry, `<DShader>/.dreampass/Registry.json` —
to give its HLSL passes their slots and to free the slots of passes it dropped — and the file exists but
cannot be used: it cannot be opened, it is not valid JSON (a merge conflict left in it is the usual
reason), or an entry has no slot number, pipeline or pass. Writing over it would lose every slot it
records, so nothing was written.

**Fix.** Take either side of the conflict and compile the `.dsp` files again — never merge the JSON by
hand — or run `./dsc.ps1 pass-registry -Rebuild`, which moves the file aside and gives every HLSL pass a
slot again ([`pass-registry`](../tools/commandlet.md#pass-registry)).

## DSH8316

<!-- generated:begin DSH8316 -->
**Severity** error

**Message**

```
Pass '{0}' needs a {1} slot and all {2} are taken. Merge passes, run 'dsc pass-registry -Gc' to free the slots of pipelines whose source is gone, or raise {3} in the project's Target.cs.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:980`
<!-- generated:end DSH8316 -->

**Cause.** A new HLSL pass needs a slot — a `compute` pass one of the compute table, a `fullscreen` pass
with `Shader =` one of the pixel table — and every slot of that table is taken in the project's registry
(32 compute and 16 pixel by default). A pass a pipeline drops frees its slot when that pipeline compiles
again; the slots of a `.dsp` that was deleted or renamed stay taken until they are collected.

**Fix.** `./dsc.ps1 pass-registry` lists the slots and counts the garbage; `./dsc.ps1 pass-registry -Gc`
frees it. Otherwise merge passes, or raise `DREAMSHADER_PASS_COMPUTE_SLOTS` /
`DREAMSHADER_PASS_PIXEL_SLOTS` with `GlobalDefinitions` in the project's Target.cs files, editor and
game alike, and rebuild: every slot is compiled for every platform, used or not.

## DSH8317

<!-- generated:begin DSH8317 -->
**Severity** error

**Message**

```
Pass '{0}' cannot be mapped onto its HLSL slot: {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1060`
<!-- generated:end DSH8317 -->

**Cause.** The pass's bindings cannot be written as its slot's section — the `#define`s that map them
onto the slot's fixed parameters; the reason follows. A name that is not an identifier or starts with
`DP_`; a name defined twice (a `read` also defines `<Name>Size` and `<Name>UVRect`, a `write`
`<Name>Size`, and the `Entry` — and `View` at `BeginView` — are names in the slot too); more than 8
reads or 4 writes; params that do not fit the slot's 16 `float4`, or a texture among them. The binder
refuses each of these first (DSH7319, DSH7347), so the two disagree.

**Fix.** Rename or reduce the bindings as the reason says, and report it with the `.dsp`: the binder
should have refused the pass.

## DSH8318

<!-- generated:begin DSH8318 -->
**Severity** error

**Message**

```
[{0}] pass '{1}' runs at BeginView, where the view uniform buffer does not exist yet, and its slot uses 'View' all the same: a function of the file's 'hlsl' block that the pass calls reads it. Give that function what it needs as a parameter, or move the pass to a later injection point.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1310`
<!-- generated:end DSH8318 -->

**Cause.** A pass at `BeginView` whose HLSL is in the `.dsp` compiled, and its slot uses the view uniform buffer,
which does not exist yet at `BeginView`. The pass's own code is guarded -- a `View` there is a compile error at its
line -- so the use comes from a function of the file's `hlsl` block the pass calls.

**Fix.** Give that function what it needs from the view as a parameter and pass a value the pass has at
`BeginView` (`DP_Time` for the time), or move the pass to a later injection point.

## DSH8319

<!-- generated:begin DSH8319 -->
**Severity** error

**Message**

```
The HLSL that pass '{0}' has in its '.dsp' could not be put together for its slot (its entry is not in the file's 'hlsl' block); there is nothing to snapshot.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1005`, `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1014`
<!-- generated:end DSH8319 -->

**Cause.** The `.usf` a pass names in `Shader =` could not be read when its snapshot was built. The
binder found the file (a missing one is DSH4408), so it went missing or became unreadable in between:
deleted or moved, locked by another program, or not readable by this process. For a pass whose HLSL is in the
`.dsp`, the root of its snapshot could not be put together from the `.dsp`'s text: its entry is not in the file's
`hlsl` block, which the binder reports first (DSH4410).

**Fix.** Check that the file is where the reference says — relative to the `.dsp`'s folder, or a virtual
path — and readable, then compile the `.dsp` again.

## DSH8320

<!-- generated:begin DSH8320 -->
**Severity** error

**Message**

```
'{0}' is included by a relative path and names no file, so the snapshot of pass '{1}' cannot be built.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1040`
<!-- generated:end DSH8320 -->

**Cause.** The pass's `.usf`, or a file it includes, has an `#include "<relative path>"` that names no
file relative to the file that holds it. A slot's snapshot copies every file included by a relative
path, so each has to exist. The scan is textual and does not evaluate `#if`: an include inside a branch
that is never compiled counts too. Reported at the line of the include.

**Fix.** Correct the path, create the file, or remove the include. A `//` comment takes it out of the
scan; an `#if 0` around it does not.

## DSH8321

<!-- generated:begin DSH8321 -->
**Severity** warning

**Message**

```
'{0}' is included by a virtual path outside /Engine/, /Plugin/ and /ThirdParty/, so the snapshot of pass '{1}' keeps including the live file: an edit of it later reaches the global shaders without a pre-check. Include it by a relative path to have it copied into the snapshot.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1050`
<!-- generated:end DSH8321 -->

**Cause.** The pass's `.usf`, or a file it includes, includes a file by a virtual path outside
`/Engine/`, `/Plugin/` and `/ThirdParty/` — a project shader directory such as `/Project/...`. A
snapshot copies only what is included by a relative path, so this include keeps pointing at the live
file. Its text is part of the slot's hash, and the pipeline's next compile pre-checks an edit of it; an
editor start or a cook before that compiles the global shaders with the file as it is then, unchecked —
and a global shader that fails to compile is fatal.

**Fix.** Include the file by a path relative to the file that includes it, so that it is copied into the
snapshot. Keep virtual paths for shader code under `/Engine/`, `/Plugin/` and `/ThirdParty/`.

## DSH8322

<!-- generated:begin DSH8322 -->
**Severity** error

**Message**

```
[{0}] pass '{1}' does not compile in its HLSL slot: {2}
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1338`, `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1346`, `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1372`
<!-- generated:end DSH8322 -->

**Cause.** The pre-check compiled the pass in its slot for the shader format in brackets, and the shader
compiler refused it; the rest of the message is the compiler's own. An error in the `.usf`, or in a file
of its snapshot, is reported at that file's line and column; one in HLSL written in the `.dsp`, at its line of the
`.dsp`. An error in the shared code of the file's `hlsl` block fails every pass that calls it the same way, and is
reported once, naming the passes it failed. Code no entry reaches is not compiled: the engine drops it from a slot
before the shader compiler reads it (`r.Shaders.RemoveDeadCode`), so a helper no pass calls yet is not checked, in a
`.dsp` as in a `.usf`. One elsewhere — the slot's section, the
slot shader, an engine header (whose path the message then keeps) — is reported at the pass, with the
note that a binding name may clash: each binding is a `#define` made before the file is included.
Nothing was written: the asset, the registry and the snapshots are as they were, and the previous
version of the pass keeps running.

One false refusal is known: a uniform buffer no slot shader referenced before is declared for the real
compile but not for the pre-check, which then reports it as undeclared.

**Fix.** Fix the line the error points at and save; the editor recompiles the pipelines that use the
file, and `./dsc.ps1 check <file>.dsp -Shaders` pre-checks every HLSL pass again. For an error at the
pass, rename the binding that collides (`InMask`, not `Texture`). Report the uniform-buffer case: it is a
limit of the pre-check, not a mistake in the file.

## DSH8323

<!-- generated:begin DSH8323 -->
**Severity** error

**Message**

```
The {0} slot shader cannot be pre-checked: the global shader type {1} is not registered, or its source no longer includes '{2}'. The DreamShaderPass module is out of step with this compiler; nothing was written.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1263`, `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1382`
<!-- generated:end DSH8323 -->

**Cause.** The pre-check needs the slot shaders as the DreamShaderPass module registers them — the global
shader types `FDreamPassCS` / `FDreamPassPS`, whose `DreamPassCompute.usf` / `DreamPassPixel.usf`
include the registry — and the type is not registered in this process, or its source cannot be read or
no longer includes the registry file. The DreamShaderPass module did not load, or it and the compiler
come from different builds of the plugin. Nothing was written. Below Unreal Engine 5.8 the same code
says that HLSL slots need 5.8; a compile and `check -Shaders` stop at DSH8300 before that.

**Fix.** Rebuild or reinstall the plugin whole, so that the DreamShaderPass module, its `Shaders` folder
and the compiler match, and restart the editor.

## DSH8324

<!-- generated:begin DSH8324 -->
**Severity** warning

**Message**

```
The project targets the shader format {0}, which this machine has no shader compiler for, so the HLSL slots were not pre-checked for it; a cook for that platform compiles them unchecked.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1201`, `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1223`
<!-- generated:end DSH8324 -->

**Cause.** A pre-check compiles a changed slot for every format the project renders and cooks with —
those of the active feature levels, and every format the active target platforms target — and this
machine has no shader compiler for this one (its platform's SDK or shader format is not installed). The
slots were checked for the others; a cook for that platform compiles them unchecked, and a global shader
that fails to compile there fails the cook.

`check -Shaders -Platform <format>` raises it too, for a format it was asked for that this machine has no
compiler for (`The shader format <format> was asked for, ...`). The slots are not checked for that
format, and when no other format was asked for, they are not checked at all: the warning keeps a check
that never ran from reading as one that passed.

**Fix.** Pre-check on a machine that has the compiler — `./dsc.ps1 check <file>.dsp -Shaders -Platform
<format>` — or install it here. `./dsc.ps1 pass-registry` lists, per slot, the formats its snapshot
passed.

## DSH8325

<!-- generated:begin DSH8325 -->
**Severity** error

**Message**

```
There is no shader format to pre-check the HLSL slots with: no active feature level and no target platform has a shader compiler on this machine. Nothing was written, because a slot that never compiled must not reach the global shaders.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1207`
<!-- generated:end DSH8325 -->

**Cause.** A slot had to be pre-checked and no shader format was there to compile it with: the process
renders nothing — `-nullrhi`, which `dsc.ps1` passes to every verb but `check -Shaders` — so no feature
level gives one, and none of the active target platforms has a format this machine can compile. A slot
that never compiled must not reach the global shaders, so nothing was written.

**Fix.** Compile the `.dsp` in the editor, or with `./dsc.ps1 check <file>.dsp -Shaders`, which runs
without `-nullrhi` and builds the pipeline as `compile` does.

## DSH8326

<!-- generated:begin DSH8326 -->
**Severity** error

**Message**

```
'{0}' cannot be written or deleted, so nothing of the slot registry was changed: no snapshot written, no slot deleted, no registry file rewritten. The registry and its snapshots are committed files; a file that is read-only because it is not checked out is the usual reason. Check out the whole .dreampass folder and try again.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1454`, `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1473`, `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1652`
<!-- generated:end DSH8326 -->

**Cause.** A file of the slot registry cannot be written or deleted — a snapshot file under `Slots/`,
`Registry.json`, `RegistryCompute.ush` or `RegistryPixel.ush` — by a `.dsp` compile or by
`pass-registry -Gc` / `-Rebuild`. They are committed files; one that is read-only because it is not
checked out is the usual reason. A compile, `-Gc` and each step of `-Rebuild` check every file they
would write or delete before they touch any, so the message above means nothing was changed: no
snapshot written, no slot deleted, no registry file rewritten. The shorter message, `'<file>' could not be written. ...`, is a write that
failed after that check passed — the file was locked or changed in between — and can leave the step
half done: a compile writes its changed snapshots before the registry files, and `-Gc` deletes the
snapshots of the slots it frees before them.

**Fix.** Make the whole `.dreampass/` folder writable (check it out) and repeat the step: compile the
`.dsp` again, or run the `pass-registry` verb again. After the shorter message, do it before the editor
restarts or a cook runs. When that write was `RegistryCompute.ush` or `RegistryPixel.ush`, after
`Registry.json` was written, compiling the `.dsp` again repairs it too: a compile compares both registry
files with `Registry.json` and rewrites one that differs, whatever else it changes.

## DSH8327

<!-- generated:begin DSH8327 -->
**Severity** info

**Message**

```
{0} slot {1} of pass '{2}' was freed: the pipeline no longer runs that pass in HLSL there.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassSlotRegistry.cpp:1530`
<!-- generated:end DSH8327 -->

**Cause.** Informational. A `.dsp` compile freed a slot its pipeline held: the pass was removed or
renamed, moved to the other table (a `compute` pass made a `fullscreen` one, or the reverse), or no
longer runs HLSL (it draws a `Material` now). Its entry and its section are gone, and so is its snapshot
unless a pass of the same compile took the slot again.

**Fix.** Nothing. Commit the changed `.dreampass/` folder with the `.dsp`.

## DSH8330

<!-- generated:begin DSH8330 -->
**Severity** error

**Message**

```
'{0}' names more than one material under this source root ({1}); write the material's asset path instead.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:564`
<!-- generated:end DSH8330 -->

**Cause.** A pass's `Material = "<Name>"` is a bare name, and more than one source under the `.dsp`'s
source root builds a material or material instance of that name — a `.dss` and a `.dsi`, or two `.dss`
in different folders; the message lists them.

**Fix.** Write the material's object path instead (`/Game/FX/PP_Outline`); *Copy Reference* in the
Content Browser gives one.

## DSH8331

<!-- generated:begin DSH8331 -->
**Severity** error

**Message**

```
The material '{0}' comes from '{1}', which failed to compile, so this pipeline has no material to check its pass against. {2}
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:477`, `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:624`
<!-- generated:end DSH8331 -->

**Cause.** A material a pass names is built by a `.dss` (or a `.dsi`) under the source roots, and that
source does not compile. Two messages, one per kind of run:

- a run that builds assets found the material's asset missing or older than its source, compiled that
  source first (DSH8332), and the compile failed (`... which failed to compile ...`);
- a run that builds nothing — a `check` — found no material asset and read the material's facts from
  its source, which does not compile (`... which does not compile ...`). It is said as that, rather
  than as a material nobody builds (DSH4406).

The pipeline has no material to check the pass against and is not built; the message ends with the
source's first error. When that error is DSH5325, the material reads this pipeline's exported buffer:
the two need each other (DSH8333).

**Fix.** Compile the material's source on its own (`./dsc.ps1 compile <file>.dss`) and fix what it
reports, then compile the `.dsp`. For DSH5325, change the read as DSH8333 says.

## DSH8332

<!-- generated:begin DSH8332 -->
**Severity** info

**Message**

```
'{0}' was missing or older than its source, so '{1}' was compiled first.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:456`
<!-- generated:end DSH8332 -->

**Cause.** Informational. A material a pass names was missing or older than the source that builds it,
so that source was compiled before the pipeline — the rule a `.dsi` applies to its parent (DSH8264).
Only a run that builds assets does this; `check` reads the material's facts from its source instead.

**Fix.** Nothing to fix. When that compile fails, DSH8331 follows.

## DSH8333

<!-- generated:begin DSH8333 -->
**Severity** error

**Message**

```
'{0}' is built by '{1}', which reads this pipeline's exported buffer through UE.DreamPassBuffer (directly, or through a pipeline that needs this one), and a pipeline and its pass material that need each other can be built in no order. Inside its own pipeline a pass binds the buffer with 'read' -- a fullscreen material reads it as a UserSceneTexture input -- rather than the exported copy; a mesh pass's material cannot read its own pipeline's buffers.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:587`
<!-- generated:end DSH8333 -->

**Cause.** A pass of this pipeline draws a material that reads the pipeline's exported buffer through
`UE.DreamPassBuffer` — directly, or through another pipeline that needs this one. The pipeline cannot
be built without its material, and the material cannot be built without the pipeline's asset, so the
two can be built in no order and never bootstrap; the `.dsp` does not compile. It is raised when this
`.dsp` is compiled or read from inside that material's compile, which builds a missing or stale
pipeline first (DSH5320); the material then reports the pipeline as failing (DSH5319). Met from the
pipeline's side, the same cycle is the material's DSH5325, which the pipeline reports as DSH8331.

**Fix.** Inside its own pipeline, read the buffer itself rather than its exported copy: bind it with
`read` in the pass that draws the material, and in the material sample it as a `UE.UserSceneTexture`
input of that name in place of the `UE.DreamPassBuffer` call — `read Blurred;` in the pass,
`UE.UserSceneTexture(UserSceneTexture = "Blurred", Coordinates = UV)` in the material. A mesh pass's
material cannot read its own pipeline's buffers at all: the usual loop is a mesh pass whose override
material samples the same pipeline's exported buffer, and sampling it in a fullscreen pass of the
pipeline, or in a material the pipeline does not draw, avoids it.

## DSH8334

<!-- generated:begin DSH8334 -->
**Severity** error

**Message**

```
The material '{0}' does not resolve to an asset path. {1}
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:557`, `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderPipelineReferences.cpp:604`
<!-- generated:end DSH8334 -->

**Cause.** A pass's `Material` cannot be used as written. Either it is spelled as a path (`/…`,
`Path(...)` or `Class'…'`) that does not resolve to an asset path — the reason follows, with its code —
or it resolves to an asset that is not a material or a material instance; the message then names its
class.

**Fix.** Write the material's object path, or the bare name of a material a `.dss` under the same source
root builds. *Copy Reference* in the Content Browser gives a path that always resolves.

## DSH8335

<!-- generated:begin DSH8335 -->
**Severity** error

**Message**

```
The Custom Pass slot registry cannot be read: {0}. Nothing was changed; 'dsc pass-registry -Rebuild' replaces a registry that does not parse.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:355`, `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:448`
<!-- generated:end DSH8335 -->

**Cause.** `pass-registry -Gc`, or the last two steps of `-Rebuild` (collecting the garbage, rewriting
the registry files), could not read `Registry.json`: it cannot be opened, it is not valid JSON — a merge
conflict left in it is the usual reason — or an entry has no slot number, pipeline or pass. Nothing was
changed. `-Rebuild` replaces an unreadable file before it compiles anything, so from `-Rebuild` this
means another process changed or locked the file while it ran.

**Fix.** Run `./dsc.ps1 pass-registry -Rebuild`. If it came from `-Rebuild`, close whatever else holds
or writes the file — an editor compiling a `.dsp` — and run it again.

## DSH8336

<!-- generated:begin DSH8336 -->
**Severity** error

**Message**

```
The Custom Pass slot registry cannot be read ({0}) and could not be moved aside to '{1}', so nothing was reset. Another process holding one of the two files open is the usual reason; a read-only one is reported before this.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:425`
<!-- generated:end DSH8336 -->

**Cause.** `pass-registry -Rebuild` found `Registry.json` unreadable (the message says why) and could not
move it aside to `Registry.json.unreadable`, so it reset nothing and stopped. A read-only `Registry.json`
or `Registry.json.unreadable` is reported before the move, as [`DSH8326`](#dsh8326); what is left is a
move that fails for another reason, most often another process holding one of the two files open.

**Fix.** Close what holds the file — an editor, a diff tool, a sync in progress — and run
`./dsc.ps1 pass-registry -Rebuild` again.

## DSH8337

<!-- generated:begin DSH8337 -->
**Severity** warning

**Message**

```
{0} slot {1} ({2}, pass '{3}') names snapshot files that are not on disk, so it is now reserved and compiles to the empty stub: a registry that includes a missing file fails the global shader compile. Compile '{4}' to give the pass its snapshot back, and commit the Slots folder with the registry.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:462`
<!-- generated:end DSH8337 -->

**Cause.** At the end of `pass-registry -Rebuild`, `Registry.json` still records a snapshot for this slot
and its files are not under `Slots/`: the `.dsp` that owns the slot did not compile in this rebuild
(DSH9204), so nothing wrote them. A registry file that includes a missing file fails the global shader
compile, so the slot was made a reserved one — it compiles to the empty stub, and its pass does nothing.

**Fix.** Fix the `.dsp` the message names and compile it: that writes the snapshot and gives the slot
its section back. Commit the `Slots` folder with the registry.

## DSH8338

<!-- generated:begin DSH8338 -->
**Severity** error

**Message**

```
HLSL slots need Unreal Engine 5.8 or later; this engine has no Custom Pass runtime to pre-check them for.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:502`, `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:514`, `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:533`
<!-- generated:end DSH8338 -->

**Cause.** `check -Shaders` on a `.dsp` could not get as far as pre-checking its HLSL passes; the message
says which of three things stopped it. The engine is older than Unreal Engine 5.8 and has no slots to
check against — though the build that `check -Shaders` runs first stops at DSH8300 there. Or the file,
read again for the check after it built, did not reach its pipeline and said nothing about why. Or the
pipeline's asset path does not resolve (the reason is in the parentheses).

**Fix.** For the first, check on Unreal Engine 5.8 or later. The other two need the file to fail the
second reading after it passed the first: run the check again, and report it with the file if it
repeats.

## DSH8339

<!-- generated:begin DSH8339 -->
**Severity** info

**Message**

```
'{0}' has no HLSL pass, so there is no slot to pre-check; its materials are checked by the sources that build them.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pass/DreamShaderPassPipelines.cpp:559`
<!-- generated:end DSH8339 -->

**Cause.** Informational. `check -Shaders` ran on a `.dsp` with no HLSL pass — no `compute` pass and no
`fullscreen` pass with `Shader =` — so there is no slot to pre-check. Its passes draw materials, whose
shaders belong to the sources that build them.

**Fix.** Nothing. To check those shaders, run `./dsc.ps1 check <file>.dss -Shaders` on the materials'
sources.

