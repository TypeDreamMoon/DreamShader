# Source control

> [DreamShader](../index.md) » [Generation](index.md) » **Source control**

What to commit, what to ignore, and how to keep a team on one answer.

| | |
| :-- | :-- |
| Always commit | every `.dss`, `.dsi`, `.dsp`, `.dsh`, `.dsm` and `.dsf` — the sources are the material — and `DShader/.dreampass/` when a `.dsp` has an HLSL pass |
| Never commit | `Intermediate/DreamShader/`, `Saved/DreamShader/` |
| Your choice | the generated `.uasset` files — this page is about that choice |
| Tool | [`dsc list-generated`](../tools/commandlet.md#list-generated) — every asset the sources build, without building any |
| Since | `2.0.0` |

## Why there is a choice to make

Through 1.x the interactive editor kept generated assets in memory, so most projects never saw a
generated `.uasset` outside a cook. Since `2.0.0` that is only true of one kind of product:

| Product | Source | On disk |
| :-- | :-- | :-- |
| `Graph`-backend material | `.dss` / `.dsm` | **always** — every successful generation saves it |
| material function, layer, layer blend | `.dss` / `.dsf` | **always** |
| material instance | `.dsi` | **always** |
| Custom Pass pipeline (`UDreamPassPipeline`) | `.dsp` | **always** |
| an exported buffer's render target (`<Pipeline>_<Buffer>`) | `.dsp` | **always** |
| `ThinCustom`-backend material | `.dss` / `.dsm` | only once [Materialized](ephemeral.md); **Ephemeral** by default, and then it has no file at all |

A `UMaterial` and a `UMaterialFunction` are stock engine classes with no way to stay out of asset
enumeration, so they are saved the moment they are built. The files appear in `Content/` next to
the assets people made by hand, and source control will offer to add them. Decide once, as a
project, which of the two recipes below you follow — a repository where half the generated assets
are committed and half are ignored is the one arrangement that does not work.

## How a generated asset is recognised

A generated asset carries its provenance in **package metadata**, not in an asset registry tag:

| Key | Value |
| :-- | :-- |
| `DreamShader.SourceFile` | the source that built it, project-relative |
| `DreamShader.SourceHash` | the [build key](caching.md) it was built under |
| `DreamShader.OutputDigest` | what the generator wrote, for [divergence](divergence.md) detection |

> [!NOTE]
> Stock classes cannot be given registry tags by a plugin, so nothing outside the editor can ask the
> asset registry "which assets are DreamShader's". Inside the editor the
> [Material Content Browser](../tools/material-browser.md) filters by provenance; outside it, ask the
> **sources** — which is what `list-generated` does.

## Listing the generated assets

```powershell
# long package names, one per line
./dsc.ps1 list-generated -All

# files, relative to the project directory
./dsc.ps1 list-generated -All -ListAs Files -Out Saved/DreamShader/generated-files.txt

# a ready .gitignore block (anchored paths, with a header comment)
./dsc.ps1 list-generated -All -ListAs GitIgnore -Out Saved/DreamShader/generated.gitignore

# everything: source, kind, backend, package, file, onDisk, persistent
./dsc.ps1 list-generated -All -ListAs Json -Out Saved/DreamShader/generated.json
```

The list is computed from the sources — front end, binder, destination rules — and nothing is built,
loaded or saved, so it is safe in CI; what it costs is one start of the commandlet. Without `-Out` the
list goes to the log, which is for reading; a script should always pass `-Out` and read the file. A
relative `-Out` is taken from the directory `dsc.ps1` was started in. What the list answers:

| Field (`Json`) | Meaning |
| :-- | :-- |
| `source` | the source file that declares the product |
| `kind` | `Material`, `MaterialFunction`, `MaterialLayer`, `MaterialLayerBlend`, `MaterialInstance`; for a `.dsp`, `PassPipeline` and, per exported buffer, `PassExportTarget` — its render target *(since 2.1.0)* |
| `backend` | `ThinCustom` for a material on that backend; `Graph` for everything else — a function, a layer, a blend and an instance are built the one way, whatever backend their file names |
| `package` / `objectPath` | `/Game/FX/M_Glow` / `/Game/FX/M_Glow.M_Glow` |
| `file` / `projectRelativeFile` | where that package is, or would be, on disk |
| `onDisk` | the file exists right now |
| `persistent` | `false` only for a ThinCustom material that is Ephemeral right now |

An Ephemeral material is left out of every list unless `-IncludeEphemeral` asks for it: it has no file
to ignore. A source that fails to compile still contributes whatever products were established before
the failure, and the run ends `RESULT=FAILED`, so a list is never silently short.

> [!NOTE]
> `-ListAs Files` and `GitIgnore` write paths relative to the project directory. A generated asset
> outside it — the content of an **engine**-level plugin — has no such path; the run says how many
> were left out ([`DSH9049`](../diagnostics/DSH9xxx.md)) and `Packages` or `Json` lists them.

## Recipe A — sources in, assets out

Generated assets are build output. Nobody commits them; every machine builds its own.

**`.gitignore`.** Keep a generated block between two markers and refresh it from the list:

```powershell
# Tools/Update-GeneratedIgnore.ps1
$begin = '# >>> DreamShader generated assets'
$end   = '# <<< DreamShader generated assets'
$list  = Join-Path ([IO.Path]::GetTempPath()) 'dreamshader-generated.gitignore'
& ./Plugins/DreamShader/.skill/dsc.ps1 list-generated -All -ListAs GitIgnore -Out $list
if ($LASTEXITCODE -ne 0) { throw 'list-generated failed; .gitignore was left as it is.' }
$block = @(Get-Content $list)

$lines = @(Get-Content .gitignore)
$from = [Array]::IndexOf($lines, $begin)
$to   = [Array]::IndexOf($lines, $end)
$head = if ($from -ge 0) { @($lines | Select-Object -First $from) } else { $lines }
$tail = if ($to -ge 0) { @($lines | Select-Object -Skip ($to + 1)) } else { @() }
@($head) + $begin + $block + $end + @($tail) | Set-Content .gitignore
```

Simpler, when the project keeps generated assets in folders of their own — and
[asset paths](asset-paths.md) make that easy, since a product lands under its source's folder — is one
line per folder: `/Content/DreamShader/`. Use the list to check that the folders hold nothing else.

**Perforce.** Add the same paths to `.p4ignore`; nothing else is needed, because the files are never
added.

**Building.** A fresh checkout has no generated assets, and anything that references one — a level, a
Blueprint, a material instance made by hand — resolves only after they exist:

| Moment | What builds them |
| :-- | :-- |
| after a sync or checkout | `./dsc.ps1 compile -All` — make it a post-checkout hook or a sync step |
| opening the editor | sources compile on startup and on save; the first open of a big project pays for all of them |
| a cook | the cook director compiles and saves every project source before it cooks |
| CI | `./dsc.ps1 check -All` gates the sources; `compile -All` before anything that loads content |

**Costs.** The first editor start after a sync that touched shared headers rebuilds what includes them.
A hand edit of a generated asset exists on one machine only — which is the point of the recipe; use
[Adopt](divergence.md) to move such an edit into the source before it is lost.

## Recipe B — commit the generated assets

Generated assets are derived binaries that happen to be versioned, the way a project versions a
lightmap.

**Git.** Treat them as any other `.uasset` (LFS, `binary`). No ignore rules.

**Perforce.** Give them a writable type so that a compile can overwrite them without a checkout —
otherwise every save of a source fails on a read-only file:

```powershell
# one typemap line per generated file; paste under `TypeMap:` in `p4 typemap`
$list = Join-Path ([IO.Path]::GetTempPath()) 'dreamshader-generated-files.txt'
./dsc.ps1 list-generated -All -ListAs Files -Out $list
Get-Content $list | ForEach-Object { "`tbinary+w //depot/MyProject/$_" }
```

`+w` keeps the file writable in the workspace; do **not** add `+l` — an exclusive lock on a file a tool
rewrites for everyone is a queue. With generated assets in folders of their own this is one line:
`binary+w //depot/MyProject/Content/DreamShader/...`.

**Keeping them honest.** A committed asset can fall behind its source. Make CI prove it has not:

```powershell
./dsc.ps1 compile -All          # unchanged sources are skipped by build key: no rewrite, no diff
git status --porcelain -- Content   # must print nothing
```

A compile skips every source whose [build key](caching.md) still matches, so an up-to-date tree comes
out byte-identical. The key also covers the plugin version, the engine version and the defines a source
read — so **upgrading DreamShader or the engine regenerates everything**. Do that in a commit of its
own, with nothing else in it; the diff is large, binary, and expected.

**Costs.** Binary churn in history, and merge conflicts that are resolved by recompiling, never by
picking a side: take either version, run `compile` on the source, commit the result.

## Which one

| | Recipe A — not committed | Recipe B — committed |
| :-- | :-- | :-- |
| Repository size | sources only | grows with every regeneration |
| Fresh checkout opens | after a compile step | immediately |
| Artists without the plugin's toolchain | cannot build — not viable | fine |
| A stale asset | impossible: it is always built from what is checked out | possible; the CI check above catches it |
| Plugin or engine upgrade | nothing to commit | one large regeneration commit |
| Hand edits of generated assets | local until adopted | versioned, and [flagged as diverged](divergence.md) |

Recipe A fits a team where everyone runs the editor with the plugin; Recipe B fits one where some
people only consume the content. The ThinCustom backend narrows the question either way: an Ephemeral
material has no file, so only its functions, its instances and whatever was Materialized are left to
decide about.

## Notes

- **`DShader/.dreampass/` is source, under either recipe.** It holds the snapshots of the `.usf` files
  the `.dsp` pipelines' HLSL passes use, and the slot registry the global shaders are built from
  ([HLSL passes](../runtime/hlsl.md#how-the-hlsl-gets-into-the-engine)). Those shaders are compiled from
  the snapshots, never from the files being edited, so the snapshots are what a teammate's editor and
  the cook run: a checkout without them runs the empty stub of every slot until its `.dsp` files are
  compiled again. Commit the folder with the `.dsp` and `.usf` change that produced it; a conflict in
  it is resolved by taking either side and compiling the `.dsp` files again, never by hand.
- **`Intermediate/DreamShader/GeneratedShaders/*.ush`** is rebuilt by every compile and must never be
  committed; Unreal's stock ignore rules already cover `Intermediate/`.
- **`Saved/DreamShader/`** holds dumps, indices, backups of migrated sources and the bridge's state.
  Stock ignore rules cover `Saved/` too; `Saved/DreamShader/Migrated/` is the one folder worth a look
  before a clean, since it is where [`dsc migrate`](../tools/migrate.md) moves the 1.x originals.
- **A plugin that ships DreamShader sources** has the same choice for its own `Content/`. Shipping the
  generated assets (Recipe B) is what lets a project use the plugin without compiling it; `-All` covers
  plugin source roots too, so one list serves both.
- **Renaming a source moves its assets.** Under Recipe B commit the rename, the new assets and the
  deletion of the old ones together; [asset rename sync](../tools/asset-rename-sync.md) keeps references
  pointed at the new path.
- **`list-generated` lists what the sources build, not what is lying around.** A generated asset whose
  source was deleted is an orphan and is not in the list; the Material Content Browser shows those.

## See also

- [Asset paths](asset-paths.md) — where a product lands, which is what decides how many ignore lines you need
- [Ephemeral materials](ephemeral.md) — the one kind of product that may have no file
- [Caching](caching.md) — the build key that makes an unchanged compile a no-op
- [Divergence](divergence.md) — hand edits of generated assets, and Adopt
- [Commandlet](../tools/commandlet.md) — `list-generated`, `compile`, `check`
