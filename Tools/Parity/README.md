# Graph parity comparator

`graph_parity.py` compares `dsc.ps1 dump-graph` output (schema 1) by graph structure. It exists because the 1.x
generator is deleted, so its output survives only as frozen dumps, and the 2.0 compiler (legacy front end included) must
reproduce them.

- Formal baseline of the four DShader roots: `<Project>/Saved/DreamShader/GraphBaseline/v2-6c2e0b6-formal`
  (exclude `*MF_OutlineWidth*`: that asset was hand-edited, see the folder's README).
- Generate corpus: `<Project>/Saved/DreamShader/GraphBaseline/v2-6c2e0b6-generate-corpus` (ThinCustom default backend).

## Why not a text diff

A dump numbers nodes `n0, n1, ...` in a canonical traversal. One node more or less renumbers everything after it. The
tool instead reduces every root (a material property, a function output, a custom output node) to a recursive signature
of the subgraph feeding it, so node numbering never matters.

## Normalisations (applied to both sides)

| Tag | What | Registered delta |
| :-- | :-- | :-- |
| R | Named reroutes are transparent: a usage reads what its declaration reads. Reroute names therefore do not take part. | D6, D12 |
| M | A wire is (source node, base value, channel list). Inline pin masks, masked engine outputs (VectorParameter `RGB`, TextureSample `R`, VertexColor, LocalPosition, Bounds) and ComponentMask nodes fold into the channel list; a list covering the whole base value is dropped. | PD-1, D1 |
| N | `Multiply(constant, Constant(-1))` is the negated constant. | D4 |
| D | Unreachable nodes never enter a signature. | D2 |
| S | Structurally identical nodes have identical signatures, so duplicates merge by construction. | D3 |
| K | Props named by `--filter-keys` are left out (default `Code`, `IncludeFilePaths`, `AdditionalOutputs`; pass `--filter-keys ""` to compare everything). | PD-3, D5, D7 |
| A | A chain of SetMaterialAttributes nodes over a material, or over an empty MakeMaterialAttributes, is one set of attributes over that material. | PD-4 |
| G | BreakMaterialAttributes over a material the graph wrote reads the value written. Works where the dump's attribute names are English; a capture taken under another culture goes through `--allow`. | PD-6 |
| L | The material input of a `BreakMaterialAttributes` / `GetMaterialAttributes` node is `MaterialAttributes`, whatever the capture calls it: the engine names it with translated text, and a dump taken before 2.0 wrote that down (2.0 dumps the stable name, see `Docs/tools/commandlet.md`). | — |
| P | A mask that is exactly the leading components a material attribute (or a known custom-output pin) reads is dropped: 1.x lost such masks on those pins, 2.0 keeps them. | PD-7 |
| B | `AppendVector(x, x)` over one wire is `x`: the engine spreads a scalar where a vector is wanted. | PD-5 |
| C | `AppendVector(constant, constant)` is the constant vector. | D4 |
| T | The `type` a dump gives a function output is left out of the metadata comparison: the 1.x dump inferred it past inline masks. | PD-2 |

The last column is the name a difference was registered under when the 1.x baselines were reviewed; the comparator's
comments and the tests that lean on a normalisation quote it.

Channel widths come from the engine's output layouts (`MaterialExpressions.cpp`), a small table of single-output classes,
`FunctionInput` types and, for `MaterialFunctionCall`, the callee's own dump when it is in the same tree. When a width is
unknown only an `rgba` mask is treated as identity; anything else is kept and will show as a wire difference for review.

## Usage

```
python Tools/Parity/graph_parity.py stats <dump-root>
python Tools/Parity/graph_parity.py compare <baseline-root> <candidate-root> --exclude "*MF_OutlineWidth*" --report parity.md
                                           [--allow Tools/Parity/registered-deltas.json]
python Tools/Parity/graph_parity.py pair <baseline.graph.json> <candidate.graph.json>
```

`--allow` names a JSON file of `{ "<file glob>": { "delta": "PD-6", "lines": ["<regex>", ...] } }`. A file whose every
difference line matches one of its patterns is reported as *equal under a registered delta* and counted apart, so a
difference no normalisation can reach stays visible without failing the run. `registered-deltas.json` holds the one
such file of the sweep (`M_MaterialAttributeRead`, PD-6: its 1.x capture has localised pin names).

Files pair by their path relative to each root. `compare` prints a Markdown report (also written with `--report`), lists
metadata differences (kind, backend, settings, function inputs/outputs, instance block) and, for each differing root, the
first divergences found by walking both signatures from that root. Exit code 0 means every pair is equal.

`stats` runs the normalisations over one tree and prints what each did.

## The sweep

`run_parity_sweep.py` runs the whole comparison, with the editor closed:

```
python Tools/Parity/run_parity_sweep.py [--skip-compile] [--keep-assets] [--legacy-corpus <dir>] [--out <dir>]
```

1. It refuses to start while an editor or commandlet runs.
2. It snapshots and backs up every product asset of the four Content trees and every dirty file of the plugin
   repositories.
3. It runs `dsc.ps1 compile -All -Force` and `dump-graph -All`, then dumps each Legacy corpus fixture the 1.x capture
   covers.
4. It restores everything the runs wrote and checks git status against the snapshot.
5. It compares the roots against `v2-6c2e0b6-formal` (excluding `MF_OutlineWidth`) and the Legacy corpus against
   `v2-6c2e0b6-generate-corpus`, pairing Legacy files by name, with `--allow registered-deltas.json`.

Reports, logs and backups land in `<Project>/Saved/DreamShader/ParitySweep/<stamp>/`. The exit code is 0 only when both
comparisons are equal.

Result of the sweeps (09-18 to 09-20, nine runs): **68 of 68 roots equal, 6 of 6 Legacy fixtures equal** (one under
PD-6), with the other plugin repositories' `git status` identical before and after every run.

## Validation (against independent counts of the same baseline)

| Check on `v2-6c2e0b6-formal` | Tool | Independent count | Note |
| :-- | --: | --: | :-- |
| inline masks in the dump | 203 | 203 | exact |
| unreachable nodes | 22 in 11 files | 24 in 11 files | the research also counted one dead named-reroute pair; reroutes are transparent here. Same classes: AppendVector 11, Constant 4, Constant4Vector 4, MakeMaterialAttributes 2, VectorParameter 1 |
| removable structural duplicates | 13 | groups of 19 nodes | the research counted every node in a group, the tool counts k-1 per group (MoonToonModifier 3, MaterialFunctionCall 2, BreakMaterialAttributes 2, MoonEncodeToonAttributes 1, TextureCoordinate 1 match); the extra Constant 3 and Multiply 1 only become equal after normalisation |
| negative literals folded | 5 | 4 constant operands + 3 others | the fifth reads its constant through a named reroute |
| baseline compared with itself | 69 equal, exit 0 | — | signatures are deterministic |
