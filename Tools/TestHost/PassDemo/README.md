# Custom Pass demo level

Twelve Custom Pass pipelines on one level of the test host, each activated the way a project would activate it,
and a script that renders the level in a game window and takes a screenshot of every pipeline on its own -- of the
five showcase pipelines also bursts of frames, for animations. The
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
| `CP_Highlight` | a mesh pass by layer with an override material and `Depth = None`, a compute blur written as the statements of its entry, a fullscreen material composite, an export read by an opaque material (`M_DemoGround`) | an unbound volume | an outline around the orange cube, and around the whole blue sphere, half of which is behind the wall |
| `CP_XRay` | a mesh pass by layer and by list with its own depth, a composite against the scene depth | a `UDreamPassComponent` on the camera, `Scope = ViewTarget` | the red cylinder and cone behind the wall as cyan silhouettes; the cone joins list `Enemies` through the API |
| `CP_Tagged` | a mesh pass by custom stencil in `Mode = Own`: the gold cube's own material writes the data | an unbound volume | moving green stripes on the gold cube |
| `CP_Scanner` | a pixel HLSL pass at `BeforePostProcess`, written as the statements of its entry, that reads the scene depth through `View` | an unbound volume with a `PulseColor` override | a magenta ring sweeping out from the camera (cyan without the override) |
| `CP_Retro` | a pixel HLSL pass at `PostProcess.AfterTonemap` whose block holds a whole function (`Main`), blended by `DP_Weight` | an unbound volume, `BlendWeight` 0.5 | scanlines, a vignette and colour fringes, at half strength |
| `CP_UIBackdrop` | a `copy` grab after the tonemapper, a compute downsample whose entry is in the file's `hlsl` block with a helper, an export read by an unlit material | the API (`AddPipeline`) | the blurred frame on the monitor at the left |
| `CP_WindField` | a compute pass at `BeginView` on a fixed-size history buffer, its statements under the BeginView `View` guard, an export read in a vertex shader | the project settings, with a `WindDirection` override, from the command line | the grass leaning with the wind; the field itself on the panel at the right |

The showcase: effects for the eye, each written with HLSL in its `.dsp`.

| Pipeline | What it exercises | Activated by | What the shot shows |
| :-- | :-- | :-- | :-- |
| `CP_LivingWall` | a Gray-Scott reaction-diffusion field: four compute passes at `BeginView` running one entry of the file's `hlsl` block, each with its own buffers, the entry doing four steps in `groupshared` memory; a history buffer, ping-pong buffers, an export read by the wall's material (`M_DemoLivingWall`) | an unbound volume | a glowing maze grown over the wall from ten spores; with the pipeline off, the plain wall |
| `CP_Shockwave` | a pixel pass whose statements reconstruct the surface from the depth: a sphere that bends the picture behind its front with a colour split, and an HDR ring on the surfaces it crosses, for the bloom; `Radius` driven from script with `SetFloatParameter` | the API | the blast at radius 360 around the cubes and up the wall; the burst grows it to 1340 |
| `CP_Comic` | a depth and colour edge pass before post processing into an `RG8` buffer of ink and sky, read after the tonemapper by a pass that posterizes, prints halftone dots and inks; shared HSV helpers in the file's block | the API | the scene as a comic book page |
| `CP_Matrix` | an outline pass before post processing and a rain of procedural glyphs (strokes of a sixteen-segment display) after the tonemapper, lit by the scene's outlines and brightness | the API | the scene as falling code; the burst runs at 6% time |
| `CP_GodRays` | two half-resolution compute passes -- the sky around the sun, then 64 taps toward it -- and an additive composite; the sun found from `View.AtmosphereLightDirection` with a shared helper | the API | light shafts through a colonnade, with the sun lowered behind the wall (`16_sunset` is the same frame without them) |

`TakeShots.py` turns the others off with `r.DreamPass.DisablePipelines` for each shot, shows a buffer with
`r.DreamPass.Visualize` in three of them, exports two render targets, and prints `DreamPass.Dump` into the game log
at the end. `r.CustomDepth` is set to 3 for `CP_Tagged`: the host does not set it. A scenario may have hooks -- the
shockwave's radius, the frame rate the living wall grows at, the world time the rain falls in, the sunset with its
colonnade -- and a burst, whose frames land in `Shots/frames/`.

## Things the shots also show

- A pipeline that is off leaves no picture behind: its exports are cleared, so the monitor goes black and the
  ground loses its glow once `CP_UIBackdrop` and `CP_Highlight` stop running.
- The camera looks down a little, so the upright grass blades lean outward at the edge of the picture; the wind
  bends them back.
- The first run of a fresh host compiles the demo's materials and the HLSL slots; the driver waits for every
  shader before the first shot.
