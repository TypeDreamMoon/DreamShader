# Name resolution

> [DreamShader](../index.md) » [Graph](index.md) » **Name resolution**

How a bare identifier and a call target are looked up inside a `Graph` block, in the fixed order the
compiler tries them, and what each surface shadows.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — applies to every expression inside a `Graph { … }` body, an `Outputs` binding expression, and an `Outputs` declaration default |
| Kind | resolution rules |

## Synopsis

Two distinct ladders exist. Which one runs depends purely on whether the name is followed by `(`.

```c
<identifier>                 // bare-identifier ladder
<identifier> ( … )           // call-name ladder
<identifier> :: <identifier> ( … )   // call-name ladder, qualified callee
```

The callee of a call is first flattened back into one string: `.` joins with a dot, `::` joins with
`::`. `UE.TexCoord` and `Common::ApplyTint` are therefore looked up as those exact strings. A callee
that is not a name or a chain of member accesses fails with
`Graph calls must target a named function.`

## Bare identifiers

| Order | Surface | Case | Result |
| :-- | :-- | :-- | :-- |
| 1 | An existing Graph value — a declared variable, an assigned variable, or a parameter already materialized in this scope | insensitive | the stored value |
| 2 | A declared property or parameter — function-local `Properties` first, then the enclosing block's `Properties` | insensitive | the parameter node, created on first read and cached under the property's declared name |
| 3 | The boolean literals `true` and `false` | insensitive | a `StaticBool` node with one component |
| 4 | *(no match)* | — | `Unknown Graph identifier '{Name}'.` |

Step 1 compares the exact spelling first, then falls back to a case-insensitive scan of the value map.
Step 2 is where a parameter becomes a node: the node is created lazily on first read and inserted into
the current value map, so every later read of the same name is served by step 1.

> [!NOTE]
> A `StaticSwitchParameter` property deliberately does **not** resolve as a bare identifier — it needs
> its two branches. Reading one by name fails with `Unknown Graph identifier '{Name}'.` Use the call
> form `Switch(True = …, False = …)`; see [Calls](calls.md#staticswitchparameter).

## Call names

Probed strictly in this order. The first surface that claims the name wins; later surfaces are never
consulted.

| Order | Surface | Case | Notes |
| :-- | :-- | :-- | :-- |
| 1 | Vector/scalar constructor name | insensitive | The 34 names in [Constructors](constructors.md#constructor-names) |
| 2 | `UE.SceneTexture` | insensitive | Desugars to `UE.Expression(Class="SceneTexture", …)` |
| 3 | Any name beginning with `UE.` | insensitive | The [`UE.*` builtin catalogue](../builtins/ue.md) |
| 4 | Any name beginning with `Substrate.` | insensitive | The [`Substrate.*` catalogue](../builtins/substrate.md), UE 5.4+ |
| 5 | Math builtin | insensitive | The 19 spellings in [Math builtins](../builtins/math.md) |
| 6 | `SampleTexture2D` | **sensitive** | Desugars to `UE.Expression(Class="TextureSample", …)` |
| 7 | A declared property whose node type is `StaticSwitchParameter` | insensitive | [Static-switch call form](calls.md#staticswitchparameter) |
| 8 | A declared property whose node type owns input pins | insensitive | [Pin call form](calls.md#input-pin-wiring) |
| 9 | `Function`, `GraphFunction`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `VirtualFunction` | insensitive | Ambiguity is checked here; see below |
| 10 | *(fallback)* `Function` lookup again | insensitive | `Unknown Graph function '{Name}'.` when nothing matched |

Step 5 has one subtlety: the math-builtin handler distinguishes "this is not a math builtin" from
"this **is** a math builtin and the call is malformed". Only the second aborts; the first falls
through to step 6. A malformed `clamp(x)` therefore reports
`Math function 'clamp' expects exactly 3 arguments.` instead of falling through to a user function.

At step 9 all four declaration kinds are looked up. If **more than one** matches, the call fails:

```text
Graph call '{Name}' is ambiguous because multiple definitions use that name: {Kinds}.
```

with the kind names joined by `, `. When exactly one matched, dispatch precedence is
material function → `VirtualFunction` → `GraphFunction`, with `Function` reached through step 10.

### How each declaration kind is matched

| Kind | Matches |
| :-- | :-- |
| `Function`, `GraphFunction` | The declared name — `Name`, or `Namespace::Name` for a namespaced declaration — **or** the generated HLSL symbol `DreamShaderFn_<sanitized name>`. First array entry that matches wins. |
| `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `VirtualFunction` | The full declared `Name`, **or** the last `/`-separated segment of it. A block declared `Name="Functions/F_Tint"` is callable as `F_Tint`. |

There is **no overload resolution**: names are not scoped by arity or by parameter type. Two
declarations of the same name in the same translation unit — imports are inlined into one text before
parsing — are not diagnosed here; a duplicate `Function` is caught later, when the generated include is
written, with `DreamShader Function '{Name}' is declared more than once.`

Namespaced functions are reachable **only** by their fully qualified `Namespace::Name`. There is no
`using`-style import and no unqualified fallback.

## Shadowing

Because the ladders are strictly ordered, "shadowing" always runs one way: an earlier surface hides a
later one, never the reverse.

| Situation | Outcome |
| :-- | :-- |
| A `Function` named `lerp`, `dot`, `pow`, `min`, `max`, `clamp`, `saturate`, `sin`, `cos`, `abs`, `floor`, `ceil`, `frac`, `fract`, `sqrt`, `normalize`, `fmod`, `mod` or `mix` | **Never called.** The math builtin at step 5 always wins. |
| A `Function` named `float`, `float2`, `vec3`, `int4`, `bool2` or any other constructor spelling | **Never called.** The constructor at step 1 always wins. |
| A `Function` named `SampleTexture2D` | **Never called.** A `Function` named `sampletexture2d` or `SampleTexture2d` **is** called — step 6 is the only case-sensitive comparison in the ladder. |
| A `Function` whose name begins with `UE.` or `Substrate.` | Unreachable — the prefix tests at steps 3 and 4 claim it first. Such a name is not a valid identifier anyway. |
| A Graph variable with the same name as a property | The variable wins for bare reads (step 1 precedes step 2), including when only the case differs. |
| A property with the same name as a `Function` | For a **call**, the property is consulted only at steps 7–8; a scalar or vector parameter is not input-bearing, so the call falls through to the `Function`. For a **bare read**, the property is the only candidate. |
| A variable or property named `True` or `False` | Shadows the boolean literal, which is only reached at step 3. |
| A local `Properties` entry with the same name as a block-level property | The function-local entry wins. |

> [!WARNING]
> Shadowing a math builtin or a constructor is silent. `Function float3 lerp(in float3 a, in float3 b, in float3 t)`
> parses, generates its HLSL helper into the include, and is simply never invoked from a `Graph` block —
> every `lerp(…)` call becomes a `LinearInterpolate` node. Rename the function.

## Identifier case

| Element | Case sensitivity |
| :-- | :-- |
| `if`, `else` | **sensitive** |
| Top-level block keywords (`Shader`, `Function`, `GraphFunction`, `Namespace`, `VirtualFunction`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`) | **sensitive** |
| `SampleTexture2D` | **sensitive** |
| Out-target uniqueness within one call | **sensitive** |
| Section names (`Properties`, `Settings`, `Outputs`, `Graph`, `Layout`) | insensitive |
| Type tokens (`float3`, `vec3`, `MaterialAttributes`, `Texture2D`, `Substrate`) | insensitive |
| Constructor names | insensitive |
| Math builtin names | insensitive |
| `UE.*` and `Substrate.*` builtin names | insensitive |
| Swizzle channels (`.RGB`, `.xyZ`) | insensitive |
| `true` / `false` | insensitive |
| Graph variable names | insensitive on lookup |
| Property and parameter names | insensitive |
| `Function` / `GraphFunction` / material-function names | insensitive |
| Named call argument names | insensitive, and surrounding whitespace is trimmed |
| `MaterialAttributes` member names | insensitive |
| `in` / `out` parameter qualifiers | insensitive |
| `default` call argument | insensitive |

> [!NOTE]
> Graph variable *lookup* is case-insensitive, but the value map is keyed by the spelling used at the
> assignment. `float3 Color = …;` followed by `color = …;` writes a **second** entry named `color`
> whose value shadows nothing consistently — later reads take whichever the exact-match test finds, and
> fall back to the first case-insensitive hit otherwise. Keep one spelling per variable.

## Named-argument matching

Argument names are normalized with the same rule used for settings keys: trim, then lower-case. Both
sides of the comparison are normalized, so `Coordinates=`, `coordinates=` and ` COORDINATES = ` are
identical. Positional arguments are indexed among the **unnamed** arguments only, so a positional index
skips over any named argument that precedes it.

Which callees accept named arguments at all is documented in [Calls](calls.md#positional-and-named).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

| Message | Cause |
| :-- | :-- |
| `Unknown Graph identifier '{Name}'.` | A bare name that is not a Graph variable, not a declared property, and not `true`/`false`. Also produced by reading a `StaticSwitchParameter` property by name. |
| `Unknown Graph function '{Name}'.` | A call name that reached the end of the ladder without matching. |
| `Graph calls must target a named function.` | The callee expression is not an identifier or a qualified name — for example a call on a parenthesized expression. |
| `Graph call '{Name}' is ambiguous because multiple definitions use that name: {Kinds}.` | Two or more declaration kinds share the name, in the value form. |
| `Graph expression statement '{Name}' is ambiguous because multiple callable definitions exist.` | The same collision, in the statement form. |
| `Expected function name after '::'.` | `::` not followed by an identifier. |
| `Expected member name after '.'.` | `.` not followed by an identifier. |
| `Property '{Name}' has a recursive UE builtin dependency.` | A `UE.*` property references itself through its arguments, directly or indirectly. |
| `Property '{Name}': {Error}` | The parameter node for a referenced property could not be created. |
| `DreamShader Function '{Name}' is declared more than once.` | Two `Function` declarations share a name, compared case-insensitively, in one translation unit. |
| `DreamShader Function '{Name}' collides with another generated helper symbol '{Symbol}'. Rename the Function or Namespace.` | Two `Function` names sanitize to the same `DreamShaderFn_*` symbol — for example `A::B` and `A_B`. |

## Example

```c
import "Helpers.dsh";

Shader(Name="Docs/M_Resolution")
{
    Properties {
        vec3                  Tint    = vec3(1.0, 0.5, 0.2);
        StaticSwitchParameter UseTint = true;
    }

    Settings { Domain = "UI"; ShadingModel = "Unlit"; }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        // 'Tint' resolves at step 2 the first time, at step 1 afterwards.
        vec3 Base1 = Tint;

        // 'TINT' finds the same variable — lookup ignores case.
        vec3 Base2 = TINT;

        // 'lerp' is claimed by the math builtin, whatever Helpers.dsh declares.
        vec3 Mixed = lerp(Base1, Base2, 0.5);

        // A namespaced Function needs its full name.
        vec3 Lit = Common::ApplyTint(Mixed, Tint);

        // 'UseTint' is a StaticSwitchParameter: callable, not readable.
        Color = UseTint(True = Lit, False = Mixed);
    }
}
```

Resolution trace:

```text
Tint                  -> step 2  declared property        -> VectorParameter node
TINT                  -> step 1  existing Graph value     -> the same node
lerp(...)             -> step 5  math builtin             -> LinearInterpolate node
Common::ApplyTint(..) -> step 9  Function (namespaced)    -> Custom node
UseTint(True=,False=) -> step 7  StaticSwitchParameter    -> StaticSwitchParameter node
UseTint               -> step 4  (bare read) ERROR: Unknown Graph identifier 'UseTint'.
```

## See also

- [Calls](calls.md) — the call forms each surface accepts, and every call diagnostic
- [Expressions](expressions.md) — where identifiers and callees appear in the grammar
- [Declarations](declarations.md) — how a Graph variable enters the value map
- [Constructors](constructors.md) — the 34 reserved constructor spellings
- [Math builtins](../builtins/math.md) — the 19 reserved builtin spellings
- [UE builtins](../builtins/ue.md) — the `UE.*` surface claimed by prefix
- [Substrate builtins](../builtins/substrate.md) — the `Substrate.*` surface
- [Parameters in Graph](../parameters/graph-usage.md) — reading parameters and the pin call form
- [`Namespace`](../language/namespace.md) — declaring `Namespace::Name` and the no-nesting rule
- [Lexical elements](../language/lexical.md) — the full case-sensitivity matrix for the declaration grammar
- [Node reuse](node-reuse.md) — how a resolved value feeds the reuse key
