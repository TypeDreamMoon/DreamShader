# Asset rename sync

> [DreamShader](../index.md) » [Tools](index.md) » **Asset rename sync**

Renaming or moving an asset in the editor rewrites the `.dsm` / `.dsf` / `.dsh` files that reference
it, so a source keeps naming the asset rather than the path it used to have.

| | |
| :-- | :-- |
| Kind | editor service, no UI |
| Triggered by | `IAssetRegistry::OnAssetRenamed` — a Content Browser rename, a drag-move, *Fix Up Redirectors*, or any asset-tools rename |
| Writes | every writable project source whose text changed, UTF-8 without BOM; each one backed up to `<file>.bak` first |
| Setting | *Sync Source References On Asset Rename* — `bSyncSourceReferencesOnAssetRename`, category *Editor*, default **on** |
| Since | `1.9.0` |

Before this existed, renaming a texture broke every source that referenced it **silently**. Nothing
went wrong at the moment of the rename; the failure surfaced at the next rebuild, as a texture that
would not load, long after the action that caused it.

## What triggers it

Every rename the asset registry announces is collected into a batch. The batch is flushed **0.5 s
after the last rename in it**, so a burst produces one pass rather than one pass per asset — *Fix Up
Redirectors* on a content folder raises a few hundred events inside a single frame, and handling each
of them on its own would mean a few hundred full scans of the source tree.

| Condition | Behaviour |
| :-- | :-- |
| The setting is off | nothing is collected and nothing is written; read live, so turning it off takes effect on the next rename |
| This editor does not own the [bridge](bridge.md#one-editor-owns-the-bridge) | nothing is written — two editors on one project both hear the rename, and both writing would race on the same file |
| The new path equals the old one | ignored |
| The rename happened inside a commandlet, or `-NoDreamShaderEditorBridge` is set | the service is never started |

## What it rewrites

Every [accepted spelling of an asset reference](../parameters/path.md), plus the two shelled forms
the Content Browser's *Copy Reference* produces.

| Written | After `/Game/Textures/T_X` is renamed to `T_Y` |
| :-- | :-- |
| `Path(Game, "Textures/T_X")` | `Path(Game, "Textures/T_Y")` |
| `Path(Game, "Textures/T_X.T_X")` | `Path(Game, "Textures/T_Y.T_Y")` |
| `Path("/Game/Textures/T_X")` | `Path("/Game/Textures/T_Y")` |
| `"/Game/Textures/T_X"` | `"/Game/Textures/T_Y"` |
| `"Texture2D'/Game/Textures/T_X.T_X'"` | `"Texture2D'/Game/Textures/T_Y.T_Y'"` |
| `/Script/Engine.Texture2D'/Game/Textures/T_X.T_X'` | `/Script/Engine.Texture2D'/Game/Textures/T_Y.T_Y'` |

Two properties of that table are load-bearing.

**The written form is preserved.** A reference spelled as a package path (`/Game/A/T_X`) comes back as
a package path; one spelled as an object path (`/Game/A/T_X.T_X`) keeps the `.Name`, carrying the new
asset's name. A rooted relative reference stays relative, and only its second argument changes —
including the whitespace between the two arguments. The class prefix of a shelled reference is never
touched: it is the author's, and rewriting it would be the plugin inventing a type name.

**A reference is matched by exact equality, never by prefix.** Renaming `/Game/Foo` leaves
`/Game/FooBar` and `/Game/Foo/T_X` alone. A prefix test here would corrupt a neighbouring asset's
references every time somebody renamed something whose name another asset happens to start with.

### When a rooted reference cannot stay relative

`Path(root, "relative")` only keeps its shape while the asset is still under that root. The root's
prefix is recovered by subtracting the relative text from the old path, and the new path is tested
against it:

```c
// The asset moved within the root: only the tail changes.
Texture2D A = Path("Game/Textures", "T_X");   ->   Path("Game/Textures", "T_Y")

// The asset moved out of the root: there is no relative spelling left.
Texture2D B = Path("Game/Textures", "T_X");   ->   Path("/Game/Props/T_Y")
```

The collapsed form is always quoted, because a quoted absolute path is the one spelling **both**
resolvers accept — see [the two resolvers](../parameters/path.md#two-resolvers).

## What it never rewrites

| Region | Why |
| :-- | :-- |
| `Function` and `GraphFunction` bodies | the body is raw HLSL handed to the shader compiler. No asset path can legally live in one, and a false hit would splice a `/Game/...` string into shader code. The scan uses the same brace-, comment- and literal-aware tracker the [preprocessor](../language/preprocessor.md) uses to decide the same question |
| `//` line comments and `/* */` block comments | a reference in a comment is not a reference the compiler reads, and leaving them alone means the service can never be blamed for editing prose |
| Unquoted bare absolute paths outside `Path(…)` and outside a shelled form | accepted by the asset-reference resolver, but indistinguishable from ordinary text; write `"/Game/…"` or `Path("/Game/…")` to have it followed |
| A reference that spans a line break | matching is per line |
| Files under a plugin [source root](../language/source-files.md#source-roots) | a plugin ships its sources as authored; only the project root is writable |
| `DShader/Packages` | third-party package sources, excluded from every editor-side scan |
| Anything, in an editor that does not own the bridge | see the table above |

> [!NOTE]
> A source containing [`#if` and friends](../language/preprocessor.md) **is** rewritten, unlike
> [VirtualFunction sync](virtual-function-tools.md#conditional-sources-are-refused-since-190). That service
> splices at byte offsets and cannot survive a preprocessed string; this one only ever reads and
> writes the raw file, line for line, so a reference inside a branch this build cut is updated along
> with the rest.

## Backups

Every file that changes is copied to `<file>.bak` **before** it is written — the same name and the
same mechanism [*Adopt Into Source*](../generation/divergence.md#adopt-into-source) uses, so there is
one place to look after any DreamShader action has rewritten a source.

| Failure | Result |
| :-- | :-- |
| The backup copy fails | `DSH9020`, and the file is **not** written |
| The write fails after the backup was taken | `DSH9021`; the file as it was is in the `.bak` |

> [!WARNING]
> `<file>.bak` holds **one** generation. A second batch that touches the same file overwrites it, and
> the `.bak` from before the first rename is gone. It is a safety net for the last action, not a
> history.

## Order: text first, rebuild second

Every file in the batch is on disk before anything is compiled. Compiling the first file while the
last one still named the old asset would fail for a reason that no longer exists by the time anyone
reads the error, so the service writes the whole batch first and only then asks the bridge to
rebuild.

The rebuild request is explicit and dispatches exactly like the source-directory watcher — `.dsm`
files are queued, `.dsh` and `.dsf` files queue their dependents through the dependency graph, which
is rebuilt once for the batch — but it does **not** honour *Auto Compile On Save*. That gate exists
for edits you make; a file the plugin itself rewrote is rebuilt regardless, because leaving the asset
and the text it claims to come from in disagreement is the failure this service exists to prevent.
The watcher sees the same writes and queues the same files; queueing is keyed by path, so nothing is
compiled twice.

## The log

One `Display` line per batch, in `LogDreamShader`, listing every file that was rewritten, its backup,
and the old → new pairs behind each change:

```text
LogDreamShader: Display: DreamShader rewrote 2 source file(s) after 3 asset rename(s):
  C:/Projects/MyGame/DShader/M_Cloud.dsm  (2 reference(s), backup 'C:/Projects/MyGame/DShader/M_Cloud.dsm.bak')
      Path(Game, "Texture/Volume/T_Volume_03")  ->  Path(Game, "Texture/Volume/T_Volume_Cloud")
      /Game/Curves/CV_Ramp  ->  /Game/Curves/CV_Falloff
  C:/Projects/MyGame/DShader/MF_Fog.dsf  (1 reference(s), backup 'C:/Projects/MyGame/DShader/MF_Fog.dsf.bak')
      /Game/Textures/T_Noise.T_Noise  ->  /Game/Textures/T_Noise_01.T_Noise_01
```

This is the record to go looking for when your editor tells you a file you had open changed on disk.

## Limits

- **Deletions are out of scope.** A deleted asset has no new path to write, and the compile error it
  produces is the correct outcome — the reference really is broken, and only a person can decide what
  should stand in its place.
- **Only the project source root is written.** Plugin roots are read-only to the editor, so a plugin
  source that references a renamed project asset is reported at its next compile and fixed by hand.
- **A file open in VSCode will prompt to reload.** The contents changed underneath the editor. This is
  expected; the log line above says exactly which files were touched. Unsaved changes in the editor
  are *not* seen by this service — it reads and writes the file on disk — so save before renaming a
  batch of assets, or take the editor's "keep my version" and re-apply the paths by hand.
- **Multi-line references are not matched.** A `Path(` … `)` broken across lines is left alone.
- **The rename is followed, the asset is not verified.** The service writes the path the registry
  reports. Whether the asset at that path loads is settled later, by generation.

## See also

- [`Path(…)` asset references](../parameters/path.md) — every accepted spelling, and the two resolvers
- [Project settings](../settings/project.md) — *Sync Source References On Asset Rename* and its tooltip
- [Bridge](bridge.md) — bridge ownership, the source-directory watcher and the compile queue
- [VirtualFunction tools](virtual-function-tools.md#startup-sync-service) — the other service that rewrites sources in place, and why it refuses conditional files
- [Divergence](../generation/divergence.md#adopt-into-source) — the `.bak` mechanism this reuses
- [Source files](../language/source-files.md#source-roots) — which roots exist and which are writable
- [Diagnostics index](../diagnostics/index.md) — `DSH9020` and `DSH9021`
