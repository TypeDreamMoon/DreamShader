# Custom Pass demo level

Seven Custom Pass pipelines on one level of the test host, each activated the way a project would activate it,
and a script that renders the level in a game window and takes a screenshot of every pipeline on its own. The
automation suite renders passes into 64x64 scene captures it builds in C++; this is the other half: `.dsp` sources
compiled by DreamShader, materials loaded from disk, a real game viewport, real exports read by real materials.

```powershell
pwsh -NoProfile -File Tools\TestHost\PassDemo\Build-PassDemo.ps1          # sources, compile, level
pwsh -NoProfile -File Tools\TestHost\PassDemo\Build-PassDemo.ps1 -Shots   # ... and the screenshots
```

The host must be built (`Tools/Tests/Invoke-DreamShaderTests.ps1` builds it) and no editor may have it open. The
level is `/Game/PassDemo/L_PassDemo`; logs and screenshots land in `<host>/Saved/PassDemo/`. The pass layers the
level uses, `Highlight` and `XRay`, are in the template's `Config/DefaultEngine.ini`.

## What runs

| Pipeline | What it exercises | Activated by | What the shot shows |
| :-- | :-- | :-- | :-- |
| `CP_Highlight` | a mesh pass by layer with an override material and `Depth = None`, a compute blur, a fullscreen material composite, an export read by an opaque material (`M_DemoGround`) | an unbound volume | an outline around the orange cube, and around the whole blue sphere, half of which is behind the wall |
| `CP_XRay` | a mesh pass by layer and by list with its own depth, a composite against the scene depth | a `UDreamPassComponent` on the camera, `Scope = ViewTarget` | the red cylinder and cone behind the wall as cyan silhouettes; the cone joins list `Enemies` through the API |
| `CP_Tagged` | a mesh pass by custom stencil in `Mode = Own`: the gold cube's own material writes the data | an unbound volume | moving green stripes on the gold cube |
| `CP_Scanner` | a pixel HLSL pass at `BeforePostProcess` that reads the scene depth through `View` | an unbound volume with a `PulseColor` override | a magenta ring sweeping out from the camera (cyan without the override) |
| `CP_Retro` | a pixel HLSL pass at `PostProcess.AfterTonemap`, blended by `DP_Weight` | an unbound volume, `BlendWeight` 0.5 | scanlines, a vignette and colour fringes, at half strength |
| `CP_UIBackdrop` | a `copy` grab after the tonemapper, a compute downsample, an export read by an unlit material | the API (`AddPipeline`) | the blurred frame on the monitor at the left |
| `CP_WindField` | a compute pass at `BeginView` on a fixed-size history buffer, an export read in a vertex shader | the project settings, with a `WindDirection` override, from the command line | the grass leaning with the wind; the field itself on the panel at the right |

`TakeShots.py` turns the others off with `r.DreamPass.DisablePipelines` for each shot, shows a buffer with
`r.DreamPass.Visualize` in three of them, exports two render targets, and prints `DreamPass.Dump` into the game log
at the end. `r.CustomDepth` is set to 3 for `CP_Tagged`: the host does not set it.

## Things the shots also show

- A pipeline that is off leaves no picture behind: its exports are cleared, so the monitor goes black and the
  ground loses its glow once `CP_UIBackdrop` and `CP_Highlight` stop running.
- The camera looks down a little, so the upright grass blades lean outward at the edge of the picture; the wind
  bends them back.
- The first run of a fresh host compiles the demo's materials and the HLSL slots; the driver waits for every
  shader before the first shot.
