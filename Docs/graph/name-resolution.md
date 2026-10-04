# Name resolution

> [DreamShader](../index.md) » [Graph](index.md) » **Name resolution**

How a bare identifier and a call target are looked up inside a `Graph` block, in the fixed order the
compiler tries them, and what each surface shadows.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — applies to every expression inside a `Graph { … }` body, an `Outputs` binding expression, and an `Outputs` declaration default |
| Kind | resolution rules |

A name is looked up in two stages *(since 2.0.0)*. The legacy front end reads a few 1.x spellings
while it parses — the boolean literals, type names, `N::F`, the 1.x call shorthands and the properties
that become nodes — and the 2.0 binder resolves everything else, the same way it does for a `.dss`.
The binder compares names **case-sensitively**; a 1.x source still builds when a name matches only in
another case, through legacy rule L19, which says so with a warning.

## Synopsis

Two distinct ladders exist. Which one runs depends purely on whether the name is followed by `(`.

```c
<identifier>                 // bare-identifier ladder
<identifier> ( … )           // call-name ladder
<identifier> :: <identifier> ( … )   // call-name ladder, qualified callee
```

`Common::ApplyTint` is read as the single identifier `Common_ApplyTint`, the name a namespaced function
is declared under. `UE.TexCoord` and `Substrate.Slab` are node calls. A callee that is none of these —
a call on a parenthesized expression, say — is [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208).

## Bare identifiers

| Order | Stage | Surface | Case | Result |
| :-- | :-- | :-- | :-- | :-- |
| 1 | parse | The boolean literals `true` and `false` | insensitive | a bool value with one component |
| 2 | parse | A type name — `float3`, `vec3`, `half`, `Texture2D`, … | insensitive | a type, which is no value: [`DSH4204`](../diagnostics/DSH4xxx.md#dsh4204) |
| 3 | parse | A property whose type is a parameter node (`TextureSampleParameter2D`, `ChannelMaskParameter`, …), except `StaticSwitchParameter` | insensitive | the property's node, written out at this read |
| 4 | bind | A Graph variable — a declaration of the body, an `Outputs` declaration of a `Shader`, or a `UE.*` property the body reads — innermost block first | sensitive | the variable |
| 5 | bind | A parameter of the function: an input or output of a `ShaderFunction`, or `Base`, the material a `Shader` builds | sensitive | the parameter |
| 6 | bind | A file-scope declaration: a scalar, vector, texture or static-bool property | sensitive | the material parameter |
| 7 | bind | Exactly one of 4–6 that matches when case is ignored | insensitive | that one, with the warning [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275) (legacy rule L19) |
| 8 | — | *(no match)* | — | [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200); `UE` or `Substrate` alone is [`DSH4203`](../diagnostics/DSH4xxx.md#dsh4203) |

Steps 1–3 run before any variable is known, so they win over a variable of the same name. A property
read at step 6 is the parameter node each time; equal nodes are one node after the
[dedupe pass](node-reuse.md). Step 7 needs a unique match: when two declarations match ignoring case,
neither is chosen and the name is `DSH4200`.

> [!NOTE]
> A `StaticSwitchParameter` property deliberately does **not** resolve as a bare identifier — it needs
> its two branches. Reading one by name fails with `DSH4200`. Use the call form
> `Switch(True = …, False = …)`; see [Calls](calls.md#staticswitchparameter).

## Call names

Probed strictly in this order. The first surface that claims the name wins; later surfaces are never
consulted.

| Order | Stage | Surface | Case | Notes |
| :-- | :-- | :-- | :-- | :-- |
| 1 | parse | `Path(…)` | insensitive | an asset reference, not a call |
| 2 | parse | A type name | insensitive | a constructor: [Constructors](constructors.md#constructor-names) |
| 3 | parse | `N::F` | — | read as the identifier `N_F`, then looked up from step 9 on |
| 4 | parse | A property whose type is a parameter node | insensitive | `StaticSwitchParameter`: the [static-switch call](calls.md#staticswitchparameter); the others: the [pin call](calls.md#input-pin-wiring) |
| 5 | parse | `SampleTexture2D` | **sensitive** | `UE.Expression(Class="TextureSample", …)`; anything but two positional arguments is [`DSH5256`](../diagnostics/DSH5xxx.md#dsh5256) |
| 6 | parse | `BreakOutFloat2Components` … `BreakOutFloat4Components` with an output selector | insensitive | a swizzle of the first argument; see [Calls](calls.md#breakoutfloatncomponents) |
| 7 | parse | The 1.x `UE.*` shorthands — `UE.TexCoord`, `UE.Time`, `UE.SceneTexture`, `UE.StaticSwitchParameter`, … | insensitive | their arguments are read the 1.x way, and an argument the shorthand never read is dropped with the warning [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254); `UE.SceneTexture` and `UE.StaticSwitchParameter` become `UE.Expression` calls. See [UE builtins](../builtins/ue.md) |
| 8 | bind | `UE.<Node>` / `Substrate.<Node>` | node name: exact, then insensitive with [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276) | the [`UE.*` catalogue](../builtins/ue.md) and the [`Substrate.*` catalogue](../builtins/substrate.md), UE 5.4+ |
| 9 | bind | Math builtin | sensitive | the HLSL spellings in [Math builtins](../builtins/math.md) |
| 10 | bind | `mix`, `fract`, `mod`, `inversesqrt` | sensitive | the GLSL spelling of a builtin: that builtin, with the warning [`DSH5277`](../diagnostics/DSH5xxx.md#dsh5277) |
| 11 | bind | `Function`, `GraphFunction`, `ShaderFunction`, `VirtualFunction` — of this file or an imported header | sensitive | see below |
| 12 | bind | Exactly one builtin, else exactly one function, that matches when case is ignored | insensitive | that one, with `DSH5275` (legacy rule L19) |
| 13 | — | *(no match)* | — | `DSH4208` |

The binder compares the `UE` in front of a node exactly *(since 2.0.0)*: `ue.TexCoord()` reads `ue` as
an undeclared name, `DSH4200`.

A builtin claims its name outright. A malformed `clamp(x)` is the builtin's arity error,
[`DSH4224`](../diagnostics/DSH4xxx.md#dsh4224); there is no user function of that name to fall through
to, because declaring one is [`DSH6206`](../diagnostics/DSH6xxx.md#dsh6206).

There is no ambiguity to resolve at step 11: one name declares one thing, and the second declaration
of a name is [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210) where it is declared *(since 2.0.0)*.

### How each declaration kind is matched

| Kind | Declared under |
| :-- | :-- |
| `Function`, `GraphFunction` | Its name. Inside `Namespace(Name="N")`, the name `N_F`, which the spelling `N::F` is read as. |
| `ShaderFunction` | The last `/`-separated segment of its `Name`, with every character an identifier cannot hold replaced by `_`: a block declared `Name="Functions/F_Tint"` is callable as `F_Tint`. When a function declared before it already has that name, `<Name>_Asset` (info [`DSH5290`](../diagnostics/DSH5xxx.md#dsh5290)). |
| `ShaderLayer`, `ShaderLayerBlend` | As `ShaderFunction`, but a call is [`DSH6208`](../diagnostics/DSH6xxx.md#dsh6208) *(since 2.0.0)*. |
| `VirtualFunction` | Its `Name`, which has to be an identifier ([`DSH6311`](../diagnostics/DSH6xxx.md#dsh6311)). |

The generated HLSL symbol `DreamShaderFn_<name>` is no longer a name a call can use *(since 2.0.0)*.

There is **no overload resolution**: names are not scoped by arity or by parameter type. An imported
header is declared into the file, not pasted into it as text, and a name it declares may not be
declared again: `DSH4210` names both files. Names are compared case-sensitively here too, so `Foo` and
`foo` are two functions *(since 2.0.0; 1.x reported them as declared twice)*, and `N::F` and a
function named `N_F` are one name, `DSH4210`.

Namespaced functions are reachable only by `N::F` or by its reading `N_F`. There is no `using`-style
import and no unqualified fallback.

## Shadowing

Because the ladders are strictly ordered, "shadowing" always runs one way: an earlier surface hides a
later one, never the reverse.

| Situation | Outcome |
| :-- | :-- |
| A `Function` named `lerp`, `dot`, `pow`, `saturate`, `frac`, `fmod`, … or `mix`, `fract`, `mod` | [`DSH6206`](../diagnostics/DSH6xxx.md#dsh6206) *(since 2.0.0)*. The function is not declared, and every call is the builtin. |
| A `Function` named `float`, `float2`, `vec3`, `int4`, `bool2` or any other type name | **Never called.** The name is read as a type while the call is parsed, so every call builds a constructor. |
| A `Function` named `SampleTexture2D` | **Never called.** Every `SampleTexture2D(…)` is rewritten while it is parsed. A function named `sampletexture2d` **is** called by that spelling: the rewrite compares exactly. |
| A `Function` whose name begins with `UE.` or `Substrate.` | Not a name: `UE.X(…)` is a node call. |
| A Graph variable with the same name as a scalar, vector, texture or static-bool property | The variable wins from its declaration on, and the declaration says so with the warning [`DSH4251`](../diagnostics/DSH4xxx.md#dsh4251). |
| A Graph variable with the same name as a parameter-node property | The property wins, in any case: steps 3 and 4 above run while the line is parsed. |
| A property with the same name as a `Function` | [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210): a property is a file-scope declaration and shares one namespace with the functions *(since 2.0.0)*. A parameter-node property is no declaration; it is written out where it is used, so a call by its name is the property's pin call. |
| The same property name in two blocks of one file | `DSH4210`: every block's scalar, vector, texture and static-bool properties are declared at file scope *(since 2.0.0)*. Parameter-node properties belong to their own block. |
| A variable or property named `True` or `False`, in any case | Unreadable: `true` and `false` are read as the literal before any name *(since 2.0.0; 1.x let the name shadow the literal)*. |
| A variable named like a type, such as `Half` or `Float3` | Unreadable: the name is read as a type, `DSH4204` *(since 2.0.0)*. |

> [!WARNING]
> Shadowing a constructor is silent. `Function vec3(in float x, out float3 r)` parses, is declared, and
> is simply never invoked from a `Graph` block — every `vec3(…)` call builds a constructor. Rename the
> function.

## Identifier case

| Element | Case sensitivity |
| :-- | :-- |
| `if`, `else` | **sensitive** |
| Top-level block keywords (`Shader`, `Function`, `GraphFunction`, `Namespace`, `VirtualFunction`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`) | **sensitive** |
| `SampleTexture2D` | **sensitive** |
| The `UE` in front of a node | **sensitive** *(since 2.0.0)* |
| Section names (`Properties`, `Settings`, `Outputs`, `Graph`, `Layout`) | insensitive |
| Type tokens (`float3`, `vec3`, `MaterialAttributes`, `Texture2D`, `Substrate`) | insensitive |
| Constructor names | insensitive |
| `true` / `false` | insensitive |
| `in` / `out` parameter qualifiers | insensitive |
| Swizzle channels (`.RGB`, `.xyZ`) | insensitive |
| Output selectors and static-switch branch names (`Output`, `OutputName`, `OutputIndex`, `True`, `False`, `A`, `B`) | insensitive |
| Math builtin names | **sensitive** *(since 2.0.0)*; another case resolves with `DSH5275` |
| Graph variable, property and parameter names | **sensitive** *(since 2.0.0)*; another case resolves with `DSH5275` when exactly one declaration matches |
| Parameter-node property names | insensitive — matched while the line is parsed |
| `Function` / `GraphFunction` / material-function names | **sensitive** *(since 2.0.0)*; another case resolves with `DSH5275` |
| Named call argument names | a function's parameters: as names, `DSH5275`; a node's pins and properties: `DSH5276`; a builtin's arguments (`A`, `B`, `Alpha`): **sensitive**, [`DSH4216`](../diagnostics/DSH4xxx.md#dsh4216) |
| `UE.*` / `Substrate.*` node, pin, property and output names | the engine's spelling; another case resolves with `DSH5276` |
| `MaterialAttributes` member names | the engine's spelling; another case resolves with `DSH5276` |
| `default` call argument | **sensitive** *(since 2.0.0)* |

> [!NOTE]
> A name written in another case than its declaration **is** that declaration, with `DSH5275`.
> `float3 Color = …;` followed by `color = …;` writes `Color` *(since 2.0.0; 1.x created a second entry
> named `color`)*. Only a name that matches nothing in any case is declared by an assignment (legacy
> rule L26, info [`DSH5292`](../diagnostics/DSH5xxx.md#dsh5292)). `dsc migrate` respells every such
> name the way it is declared.

## Named-argument matching

Argument names are compared exactly. A function's parameter names fall back to a unique
case-insensitive match with `DSH5275`, a node's pin and property names with `DSH5276`; a builtin's
argument names have no fallback. The 1.x pseudo-arguments listed above are matched ignoring case.
Positional arguments come first and fill the parameters in order; a positional argument after a named
one is [`DSH2158`](../diagnostics/DSH2xxx.md#dsh2158) *(since 2.0.0)*.

Which callees accept named arguments at all is documented in [Calls](calls.md#positional-and-named).

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH4200` | A bare name that is no Graph variable, parameter or file-scope declaration, or that matches several of them ignoring case. Also a `StaticSwitchParameter` property read without the call. |
| `DSH4203` | `UE` or `Substrate` read as a value. |
| `DSH4204` | A type name read as a value. |
| `DSH4208` | A call name that reached the end of the ladder, or a callee that is not a name. |
| `DSH5275` | *(warning)* A name that matches its declaration only when case is ignored. |
| `DSH5276` | *(warning)* An engine name — node, pin, property, output, material attribute — that matches only when case is ignored. |
| `DSH5277` | *(warning)* The GLSL spelling of a builtin. |
| `DSH4210` | One name declared twice, in the file or across an imported header — including `N::F` against a function `N_F`. |
| `DSH6206` | A function named like a builtin. |
| `DSH6208` | A call to the `Shader`, a `ShaderLayer` or a `ShaderLayerBlend`. |
| `DSH4251` | *(warning)* A Graph variable hides a property of the same name. |
| [`DSH5260`](../diagnostics/DSH5xxx.md#dsh5260) | `::` not followed by a name. |
| [`DSH2161`](../diagnostics/DSH2xxx.md#dsh2161) | `.` not followed by a name. |

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
        // 'Tint' is the file-scope property: a VectorParameter node.
        vec3 Base1 = Tint;

        // 'TINT' matches 'Tint' only ignoring case: the same parameter, warning DSH5275.
        vec3 Base2 = TINT;

        // 'lerp' is the builtin; a function of that name in Helpers.dsh would be DSH6206.
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
Tint                  -> bare 6   file-scope property               -> VectorParameter node
TINT                  -> bare 7   'Tint', ignoring case (DSH5275)   -> the same node
lerp(...)             -> call 9   math builtin                      -> LinearInterpolate node
Common::ApplyTint(..) -> call 3   read as Common_ApplyTint
                         call 11  Function (namespaced)             -> Custom node
UseTint(True=,False=) -> call 4   StaticSwitchParameter             -> StaticSwitchParameter node
UseTint               -> (bare read) ERROR: DSH4200
```

## See also

- [Calls](calls.md) — the call forms each surface accepts, and every call diagnostic
- [Expressions](expressions.md) — where identifiers and callees appear in the grammar
- [Declarations](declarations.md) — how a Graph variable is declared
- [Constructors](constructors.md) — the reserved constructor spellings
- [Math builtins](../builtins/math.md) — the reserved builtin spellings
- [UE builtins](../builtins/ue.md) — the `UE.*` surface claimed by prefix
- [Substrate builtins](../builtins/substrate.md) — the `Substrate.*` surface
- [Parameters in Graph](../parameters/graph-usage.md) — reading parameters and the pin call form
- [`Namespace`](../language/namespace.md) — declaring `Namespace::Name` and the no-nesting rule
- [Lexical elements](../language/lexical.md) — the full case-sensitivity matrix for the declaration grammar
- [Node reuse](node-reuse.md) — how equal nodes are merged
- [`dsc migrate`](../tools/migrate.md) — the `.dss` spelling of every legacy rule on this page
