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
Asset '%s' was edited by hand since DreamShader generated it from '%s', so it was NOT rebuilt (rebuilding would destroy those edits). 
```

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderGeneratedAssetMetadata.cpp:395`
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

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderGeneratedAssetMetadata.cpp:445`
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

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderGeneratedAssetMetadata.cpp:475`
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

**Raised by** `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:454`, `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:653`, `Source/DreamShaderEditor/Private/Provenance/DreamShaderProvenanceActions.cpp:918`
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

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderThinCustomParameterOverrides.cpp:240`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1259`, `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:1010`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:520`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:741`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:596`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1245`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1234`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitterNodes.cpp:252`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:382`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:392`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:436`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:791`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:357`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:452`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1208`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:563`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:712`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:867`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:296`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:313`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:339`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:635`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1285`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:369`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1022`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:251`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:260`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1049`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1074`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1082`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1093`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1115`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1185`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1062`
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

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderInstanceSettings.cpp:340`
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

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderInstanceSettings.cpp:349`
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

**Raised by** `Source/DreamShaderCompiler/Private/Assets/DreamShaderInstanceSettings.cpp:316`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:1133`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:272`, `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIRAssets.cpp:279`
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

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:482`, `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:514`, `Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:387`, `Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:416`
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

**Raised by** `Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:452`
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

**Raised by** `Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:466`
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

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:426`, `Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:398`, `Source/DreamShaderCompiler/Private/Sources/DreamShaderProductIndex.cpp:446`
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

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:503`
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

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:434`
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

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:622`
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

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:644`, `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:225`
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

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:171`
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

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:206`
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

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:255`
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

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilerIncludes.cpp:195`
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
'{0}' is not a source the compiler builds on its own; it builds '.dss', '.dsi', '.dsm' and '.dsf' files, and a '.dsh' header only through the source that includes it.
```

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:565`
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

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:737`
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

**Raised by** `Source/DreamShaderCompiler/Private/Emitter/DreamShaderIREmitter.cpp:308`, `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:604`
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

**Raised by** `Source/DreamShaderCompiler/Private/Pipeline/DreamShaderCompilePipeline.cpp:896`
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

