# DreamShader localization baseline

This inventory covers compile-time LOCTEXT/NSLOCTEXT entries that the Localization Dashboard gather step can see.

It spans EVERY source file, not just the ones this script lints. GatherText scans the whole
module and knows nothing about the Scope / Deferred / Allowlisted / Excluded split, so an
inventory narrower than the module would report a count no real gather ever produces. That
includes the automation tests: their LOCTEXT entries (namespace `DreamShaderTests`) are
gathered like any other, so they are listed here rather than quietly dropped.

`-IncludeDeferred` widens which files the R1/R2 literal rules run on; it does not change this count.

## Expected gather count
1503

## Inventory
| Namespace | Key | Source text |
| --- | --- | --- |
| DreamShader.Binder | ArraySizeNotLiteral | An array size must be an integer literal. |
| DreamShader.Binder | ArraySizeOutOfRange | An array size must be between 1 and 4096. |
| DreamShader.Binder | ConstantNeedsInitializer | '{0}' is a compile-time constant and must be initialised where it is declared. |
| DreamShader.Binder | CustomBodyParsed | '@custom' on '{0}' did not make its body opaque; the directive has to sit in the '///' block directly above the declaration. |
| DreamShader.Binder | CustomMaterialParameter | '{0}' is '@custom', so '{1}' becomes an input pin of a Custom node, and a Custom node cannot take a material; pass the fields it needs instead. |
| DreamShader.Binder | DirectiveWrongLinkage | '@{0}' means nothing here; it belongs on {1}. |
| DreamShader.Binder | DuplicateParameter | '{0}' is declared twice in the parameter list of '{1}'. |
| DreamShader.Binder | DuplicateStructField | '{0}' is declared twice in struct '{1}'. |
| DreamShader.Binder | EmptyCatalog | The builtin catalog is empty, so no 'UE.' expression and no material attribute can be resolved; export it with 'dsc export-catalog'. |
| DreamShader.Binder | EntryAndExports | '{0}' is exported from a file whose entry is '{1}'; a file makes a material or it makes functions, not both. Move it to its own file, or drop 'export' to make it a helper. |
| DreamShader.Binder | ExternWithoutAsset | 'extern {0}' has nothing to bind to; add '/// @asset /Game/.../MF_Name' above it. |
| DreamShader.Binder | FunctionShadowsBuiltin | '{0}' is a builtin operation and cannot be redeclared; rename the function. |
| DreamShader.Binder | GlobalStorage | '{0}' is a file-scope variable with no storage class; write 'uniform' for a material parameter or 'static const' for a compile-time constant. |
| DreamShader.Binder | GlobalTypeUnsupported | A file-scope variable of type {0} has no node; a 'uniform' or 'static const' must be numeric, bool, a texture or a sampler. |
| DreamShader.Binder | HoistedCallRecursion | '{0}' reaches itself through the 'UE.' calls lifted out of its body, and each call makes a new custom node, so the graph would never end. |
| DreamShader.Binder | IncludeCycle | Including '{0}' from '{1}' closes a cycle; a header may not include itself, directly or through another header. |
| DreamShader.Binder | IncludesUnavailable | '{0}' cannot be read: this front end was started without an include resolver, so nothing a header declares is visible. |
| DreamShader.Binder | LayerBlendNotExported | '@layerblend' makes '{0}' a material layer blend asset, so it has to be 'export'. |
| DreamShader.Binder | LayerBlendSignature | A '@layerblend' function is written 'export void {0}(material Base, material Top, ..., inout material Result)': at least one 'material' input and a final 'inout material'. |
| DreamShader.Binder | LayerNotExported | '@layer' makes '{0}' a material layer asset, so it has to be 'export'. |
| DreamShader.Binder | LayerSignature | A '@layer' function is written 'export void {0}(inout material m)'. |
| DreamShader.Binder | LegacyBlockNameTaken | The asset of this block and a function this file can call are both named '{0}', which 1.x kept apart; the block is declared as '{1}', and a '.dss' writes that name. |
| DreamShader.Binder | LinkageTargetExport | an exported function |
| DreamShader.Binder | LinkageTargetExtern | an 'extern' prototype |
| DreamShader.Binder | LinkageTargetLegacyExport | a block of a 1.x source that produces an asset |
| DreamShader.Binder | MaterialPragmaWithoutEntry | '#pragma material' configures a material, and this file has no 'export void Name(inout material m)' entry to configure. |
| DreamShader.Binder | MultiDimensionalArray | A multi-dimensional array has no graph form; declare one dimension, or move the code into a '/// @custom' function. |
| DreamShader.Binder | ParamDocUnknown | '@param {0}' does not name a parameter of '{1}'. |
| DreamShader.Binder | PinDocUnknown | '@pin {0}' does not name a parameter of '{1}'. |
| DreamShader.Binder | PrototypeWithoutExtern | '{0}' has no body; a prototype has to be 'extern' and carry '/// @asset'. |
| DreamShader.Binder | RedefinitionAcrossFiles | '{0}' is declared in '{1}' and again in '{2}'; a name included from a header may not be declared a second time. |
| DreamShader.Binder | RedefinitionSameFile | '{0}' is already declared in this file; one name declares one thing. |
| DreamShader.Binder | StaticDocUnknown | '@static {0}' does not name a parameter of '{1}'. |
| DreamShader.Binder | StaticOnNonBoolUniform | '@static' asks for a static switch and is only meaningful on a 'uniform bool'. |
| DreamShader.Binder | StaticParamNotBool | '@static {0}' makes a parameter a static bool pin, which only a 'bool' input can be, and '{0}' is {1}. |
| DreamShader.Binder | StaticParamOut | an 'out' or 'inout' parameter |
| DreamShader.Binder | TextureUniformInitializer | A texture uniform has no HLSL initializer; write its default asset as '/// @default /Game/...'. |
| DreamShader.Binder | TwoEntries | '{0}' is a second material entry; '{1}' above it is already the entry, and one file makes one material. |
| DreamShader.Binder | UniformArray | A 'uniform' array has no parameter node; declare one uniform per element, or make it 'static const'. |
| DreamShader.Binder | UnknownType | '{0}' is not a builtin type and no 'struct' of that name is declared before this line. |
| DreamShader.Binder | UnknownTypeDidYouMean | '{0}' is not a type; did you mean '{1}'? Type names are case-sensitive. |
| DreamShader.Binder | UnsizedArrayNeedsInitializer | An unsized array needs an initializer list to take its length from. |
| DreamShader.Binder | UnsupportedType | '{0}' is not a type this front end can resolve. |
| DreamShader.Binder | VoidParameter | A parameter cannot be 'void'. |
| DreamShader.Binder | VoidStructField | A struct field cannot be 'void'. |
| DreamShader.Binder.Directives | BackendEmpty | 'Backend' has no value; write 'Backend = Graph' or 'Backend = ThinCustom'. An empty value meant Graph in 1.x and means nothing now. |
| DreamShader.Binder.Directives | BackendInstance | 'Backend = Instance' is the old spelling of 'Backend = ThinCustom'; write the new one. |
| DreamShader.Binder.Directives | BackendUnknown | 'Backend = {0}' is not a backend; the backends are 'Graph' and 'ThinCustom'. |
| DreamShader.Binder.Directives | CustomModifier | '@custom {0}' is not a modifier this language knows; the only one is 'selfcontained'. |
| DreamShader.Binder.Directives | DirectiveNeedsValue | '@{0}' needs a value after it. |
| DreamShader.Binder.Directives | DirectiveRepeated | '@{0}' is written twice in this block; the last one wins. |
| DreamShader.Binder.Directives | DirectiveWrongTarget | '@{0}' means nothing here; it belongs on {1}. |
| DreamShader.Binder.Directives | EndRegionWithoutRegion | '#pragma endregion' closes a box that was never opened. |
| DreamShader.Binder.Directives | LayerAndLayerBlend | '@layer' and '@layerblend' make two different assets; a function is one or the other. |
| DreamShader.Binder.Directives | LayoutBadColor | '#pragma layout' expects 'Color' as three or four numbers in quotes, such as "0.1 0.16 0.22 0.35"; '{0}' was ignored. |
| DreamShader.Binder.Directives | LayoutBadValue | '#pragma layout' expects a whole number for '{0}'; '{1}' was ignored. |
| DreamShader.Binder.Directives | LayoutColorOnNode | '#pragma layout(Node, ...)' places a node, and 'Color' colours only a comment box; the colour was ignored. |
| DreamShader.Binder.Directives | LayoutExtraPositional | '#pragma layout' takes one positional selector; '{0}' was ignored. |
| DreamShader.Binder.Directives | LayoutNoKind | '#pragma layout' starts with 'Node' or 'Comment'; the line was ignored. |
| DreamShader.Binder.Directives | LayoutUnknownKey | '#pragma layout' has no '{0}' key; it was ignored. |
| DreamShader.Binder.Directives | LayoutUnknownKind | '#pragma layout({0}, ...)' is neither 'Node' nor 'Comment'; the line was ignored. |
| DreamShader.Binder.Directives | MaterialPragmaDuplicate | '{0}' is set twice by '#pragma material'; it was already set on line {1}. |
| DreamShader.Binder.Directives | MaterialPragmaPositional | '#pragma material' takes 'Key = Value' pairs; '{0}' has no key. |
| DreamShader.Binder.Directives | ParamNeedsName | '@param' is written '@param <ParameterName> <description>'. |
| DreamShader.Binder.Directives | PinNeedsNames | '@pin' is written '@pin <ParameterName> <engine pin name>'. |
| DreamShader.Binder.Directives | RegionNotClosed | '#pragma region {0}' is never closed; add a '#pragma endregion'. |
| DreamShader.Binder.Directives | RegionNotClosedUnnamed | This '#pragma region' is never closed; add a '#pragma endregion'. |
| DreamShader.Binder.Directives | SamplerEmpty | '@sampler' needs a sampler type after it, such as 'Color', 'Normal' or 'LinearColor'. |
| DreamShader.Binder.Directives | SliderMalformed | '@slider' takes two numbers, a minimum and a maximum; '{0}' is not that. |
| DreamShader.Binder.Directives | SliderRange | '@slider' needs its minimum below its maximum. |
| DreamShader.Binder.Directives | SortMalformed | '@sort' takes one whole number; '{0}' is not that. |
| DreamShader.Binder.Directives | StaticNeedsParam | '@static' on a function is written '@static <ParameterName>'. |
| DreamShader.Binder.Directives | TargetExtern | an 'extern' prototype |
| DreamShader.Binder.Directives | TargetFunction | an exported function |
| DreamShader.Binder.Directives | TargetFunction2 | a function |
| DreamShader.Binder.Directives | TargetFunction3 | a function |
| DreamShader.Binder.Directives | TargetFunction4 | an exported function |
| DreamShader.Binder.Directives | TargetFunction5 | an exported function |
| DreamShader.Binder.Directives | TargetFunctionPin | a function |
| DreamShader.Binder.Directives | TargetFunctionRoot | an exported function |
| DreamShader.Binder.Directives | TargetTextureUniform | a texture 'uniform' |
| DreamShader.Binder.Directives | TargetTextureUniform2 | a texture 'uniform' |
| DreamShader.Binder.Directives | TargetUniform | a 'uniform' |
| DreamShader.Binder.Directives | TargetUniform2 | a 'uniform' |
| DreamShader.Binder.Directives | TargetUniform3 | a 'uniform' |
| DreamShader.Binder.Directives | TargetUniformOrFunction | a 'uniform' or an exported function |
| DreamShader.Binder.Expressions | AbstractClass | '{0}' is abstract and cannot be made into a node. |
| DreamShader.Binder.Expressions | ArgumentOf | The '{0}' argument of '{1}' |
| DreamShader.Binder.Expressions | ArgumentTwice | '{0}' is given twice in this call to '{1}'. |
| DreamShader.Binder.Expressions | ArrayElement | This array element |
| DreamShader.Binder.Expressions | ArrayIndexNotConstant | An array index must be a compile-time constant: the graph has no arrays, so every element is read at compile time. |
| DreamShader.Binder.Expressions | ArrayIndexOutOfRange | Element {0} is out of range for an array of {1}. |
| DreamShader.Binder.Expressions | ArrayInitializerCount | This array has {0} elements and its initializer has {1}. |
| DreamShader.Binder.Expressions | ArrayNeedsList | An array is initialised with a list, as in '= { 1.0, 2.0 }'. |
| DreamShader.Binder.Expressions | ArrayNotFolded | This array is not a compile-time constant, and the graph has no arrays; declare it 'static const' with a constant initializer. |
| DreamShader.Binder.Expressions | AssignmentTarget | This assignment |
| DreamShader.Binder.Expressions | AssignToGlobal | A 'uniform' is an input and a 'static const' is a constant; neither can be assigned to. Copy it into a local first. |
| DreamShader.Binder.Expressions | AssignToInParam | An 'in' parameter is a function input pin and cannot be written to; declare it 'out' or 'inout', or copy it into a local. |
| DreamShader.Binder.Expressions | AssignToNonLValue | The left of '=' has to be a variable, a struct field, a material pin or a swizzle of one. |
| DreamShader.Binder.Expressions | BitwiseAssignUnsupported | '{0}=' has no graph form; the graph carries floats, not bit patterns. |
| DreamShader.Binder.Expressions | BitwiseNotUnsupported | '~' has no graph form; the graph has no integers to complement. Move the code into a '/// @custom' function. |
| DreamShader.Binder.Expressions | BitwiseUnsupported | '{0}' has no graph form; the graph carries floats, not bit patterns. Move the code into a '/// @custom' function. |
| DreamShader.Binder.Expressions | CalleeNotCallable | This is not something that can be called; a call names a function, a builtin, a type or 'UE.'/'Substrate.' followed by a node. |
| DreamShader.Binder.Expressions | CallToProduct | '{0}' is a {1} asset, not a function this file may call. |
| DreamShader.Binder.Expressions | CannotIndex | A value of type {0} cannot be indexed. |
| DreamShader.Binder.Expressions | CastFromNonNumeric | A value of type {0} cannot be cast to {1}. |
| DreamShader.Binder.Expressions | CastNarrows | A cast from {0} to {1} drops components; write the swizzle that says which, such as '.xyz'. |
| DreamShader.Binder.Expressions | CastOperand | This cast |
| DreamShader.Binder.Expressions | CastToNonNumeric | A cast to {0} has no meaning here; only numbers and bools can be cast. |
| DreamShader.Binder.Expressions | ChannelViewAcrossOutputs | '{0}.{1}' publishes the channels of '.{2}' on different outputs; read them one output at a time from {3}. |
| DreamShader.Binder.Expressions | ClassNotLiteral | 'Class' takes the expression class as a quoted string. |
| DreamShader.Binder.Expressions | ConditionalBranches | The two halves of '?:' are {0} and {1}; they have to make one value. |
| DreamShader.Binder.Expressions | ConditionalCondition | The condition of '?:' has to be a single true-or-false value, and this is {0}. |
| DreamShader.Binder.Expressions | ConditionalConditionWhat | The condition of '?:' |
| DreamShader.Binder.Expressions | ConditionalFalse | The 'else' half of '?:' |
| DreamShader.Binder.Expressions | ConditionalTrue | The 'then' half of '?:' |
| DreamShader.Binder.Expressions | ConstructorArgumentType | A constructor takes numbers, and this is {0}. |
| DreamShader.Binder.Expressions | ConstructorComponent | This constructor component |
| DreamShader.Binder.Expressions | ConstructorCount | {0} needs {1} components and these arguments supply {2}. |
| DreamShader.Binder.Expressions | ConstructorMatrixArgument | A matrix cannot be a constructor component. |
| DreamShader.Binder.Expressions | ConstructorNamedArguments | A constructor takes its components in order; named arguments belong on 'UE.' nodes and on function calls. |
| DreamShader.Binder.Expressions | ConstructorNoArguments | {0}() has no components; write the value, as in 'float3(0.0)'. |
| DreamShader.Binder.Expressions | CoreOpArgumentTwice | The '{0}' argument of '{1}' is given twice. |
| DreamShader.Binder.Expressions | CoreOpArityExact | '{0}' takes {1} arguments and {2} were given. |
| DreamShader.Binder.Expressions | CoreOpArityRange | '{0}' takes between {1} and {2} arguments, and {3} were given. |
| DreamShader.Binder.Expressions | CoreOpMissingArgument | '{0}' is missing its '{1}' argument. |
| DreamShader.Binder.Expressions | CoreOpNoPins | '{0}' takes its arguments in order and has no argument called '{1}'. |
| DreamShader.Binder.Expressions | CoreOpNoSuchPin | '{0}' has no argument called '{1}'; its arguments are {2}. |
| DreamShader.Binder.Expressions | CoreOpTooManyArguments | '{0}' takes at most {1} arguments. |
| DreamShader.Binder.Expressions | CustomCallOutputIndexRange | This Custom node declares {0} output(s), counted from 0, and this selects output {1}. |
| DreamShader.Binder.Expressions | CustomOutputAsValue | '{0}.{1}' is an output node, not a value: write it as a statement on a line of its own. |
| DreamShader.Binder.Expressions | DynamicInputTwice | '{0}' is connected twice in this call. |
| DreamShader.Binder.Expressions | DynamicInputType | '{0}' becomes an input of the custom node, which carries a number or a texture, and this argument is {1}. |
| DreamShader.Binder.Expressions | EnumNotSpelling | '{0}' is an enumerated property; write one of its values, as in 'SamplerType = Normal'. |
| DreamShader.Binder.Expressions | ExpressionNeedsClass | 'UE.Expression' reaches a node this language has no name for, so it needs 'Class = "MaterialExpressionName"'. |
| DreamShader.Binder.Expressions | GlslAlias | '{0}' is the GLSL spelling; this language is HLSL, so write '{1}'. |
| DreamShader.Binder.Expressions | HyperbolicUnsupported | The material graph has no hyperbolic node, so '{0}' cannot be lowered; write it in a '/// @custom' body, where the shader compiler has it. |
| DreamShader.Binder.Expressions | IncrementNeedsLValue | '++' and '--' write back into what they read, so they need a variable. |
| DreamShader.Binder.Expressions | InitializerElement | This initializer element |
| DreamShader.Binder.Expressions | InitializerListBadTarget | A value of type {0} cannot be written as an initializer list. |
| DreamShader.Binder.Expressions | InitializerListCount | {0} needs {1} components and this list supplies {2}. |
| DreamShader.Binder.Expressions | InitializerListNoTarget | An initializer list only has a meaning against a declared type; it cannot stand on its own. |
| DreamShader.Binder.Expressions | IntegerDivide | Both sides of this '/' are integers, and the material graph has no integer division; write 'float(a) / b' for the fraction, or 'floor(float(a) / b)' for the whole part. |
| DreamShader.Binder.Expressions | LegacyDefaultDropped | '{0}.{1}' has no 'DefaultValue', so the default written for this parameter is dropped, as 1.x dropped it. |
| DreamShader.Binder.Expressions | LegacyDefaultOutput | '{0}' has more than one output and is read as its first, '{1}', which is what 1.x did; a '.dss' names the output. |
| DreamShader.Binder.Expressions | LegacyEnumerator | '{0}' is not spelled like a value of '{1}', and 1.x matched enumerators loosely, so this is '{2}'; a '.dss' writes '{2}'. |
| DreamShader.Binder.Expressions | LegacyGlslAlias | '{0}' is the GLSL spelling of '{1}'; a 1.x source may use it and it is read as '{1}', and a '.dss' writes '{1}'. |
| DreamShader.Binder.Expressions | LegacyImplicitLocal | '{0}' is not declared, and as in 1.x this assignment declares it, as a local of type {1}. |
| DreamShader.Binder.Expressions | LegacyLateBoundPin | '{0}.{1}' lists no pin called '{2}'; it is connected by that name once the node exists, because a node may name its pins after its properties. |
| DreamShader.Binder.Expressions | LegacyRequiredPin | '{0}.{1}' leaves its required '{2}' pin unconnected, which 1.x allowed and the engine reports when the material compiles. |
| DreamShader.Binder.Expressions | LegacySelectOrdinalNotConstant | An output is selected by a whole number the compiler knows, and this index is computed. |
| DreamShader.Binder.Expressions | LegacyTruncation | {0} expects {1}, and this is {2}: its leading components are taken, which is what 1.x did; a '.dss' writes the swizzle. |
| DreamShader.Binder.Expressions | MatrixIndex | A matrix row cannot be read: the graph has no matrices. Move the code into a '/// @custom' function, where the matrix is an input. |
| DreamShader.Binder.Expressions | MissingArgument | '{0}' needs an argument for '{1}'. |
| DreamShader.Binder.Expressions | NamedOnly | '{0}.{1}' takes named arguments: write 'Pin = value'. |
| DreamShader.Binder.Expressions | NamespaceAsValue | '{0}' is a namespace, not a value; write '{0}.SomeNode(...)'. |
| DreamShader.Binder.Expressions | NoConversion | {0} expects {1}, and this is {2}. |
| DreamShader.Binder.Expressions | NodeNeedsNamedOutput | {0} expects {1}, and '{2}' has more than one output; name the one you mean: {3}. |
| DreamShader.Binder.Expressions | NodeNeedsOutput | {0} expects {1}, and this node has more than one output; name the one you mean. |
| DreamShader.Binder.Expressions | NodeOutputIndexNotConstant | An output is selected by a whole number the compiler knows, and this index is computed. |
| DreamShader.Binder.Expressions | NodeOutputIndexRange | '{0}.{1}' has {2} output(s), counted from 0, and this selects output {3}. |
| DreamShader.Binder.Expressions | NoSuchMember | A value of type {0} has no member '{1}'. |
| DreamShader.Binder.Expressions | NoSuchMethod | A value of type {0} has no method '{1}'. |
| DreamShader.Binder.Expressions | NoSuchParameter | '{0}' has no parameter called '{1}'. |
| DreamShader.Binder.Expressions | NoSuchParameterDidYouMean | '{0}' has no parameter called '{1}'; did you mean '{2}'? |
| DreamShader.Binder.Expressions | NoSuchPinOrProperty | '{0}.{1}' has no pin or property called '{2}'. |
| DreamShader.Binder.Expressions | NoSuchPinOrPropertyDidYouMean | '{0}.{1}' has no pin or property called '{2}'; did you mean '{3}'? |
| DreamShader.Binder.Expressions | NoSuchTextureMethod | A texture has no '{0}' method; it has 'Sample' and 'SampleLevel'. |
| DreamShader.Binder.Expressions | NotConstructible | A value of type {0} cannot be constructed; it comes from a declaration or a node. |
| DreamShader.Binder.Expressions | OperandNotNumeric | '{0}' works on numbers, and this is {1}. |
| DreamShader.Binder.Expressions | OperandOf | '{0}' |
| DreamShader.Binder.Expressions | OpNotCallable | '{0}' is not an operation this language spells as a call. |
| DreamShader.Binder.Expressions | OutArgumentNotLValue | '{0}' is an out parameter of '{1}', so its argument has to be a variable. |
| DreamShader.Binder.Expressions | OutArgumentType | '{0}' writes back {1}, and this variable is {2}; an out argument has to match exactly. |
| DreamShader.Binder.Expressions | PinArgumentNotReflectedCall | 'Pin[{0}] = ...' connects a node's input pin by its engine index, and only a 'UE.' or 'Substrate.' node call has one; pass this argument by name or by position. |
| DreamShader.Binder.Expressions | PinIndexOutOfRange | '{0}.{1}' has {2} input pin(s), counted from 0, and this argument connects pin {3}. |
| DreamShader.Binder.Expressions | PinOf | The '{0}' pin of '{1}.{2}' |
| DreamShader.Binder.Expressions | PinTwice | '{0}' is connected twice in this call. |
| DreamShader.Binder.Expressions | PropertyNotConstant | '{0}' is written into the node itself, not connected to it, so its value has to be known at compile time. |
| DreamShader.Binder.Expressions | PropertyNotSpelling | '{0}' takes a name or an asset path; write it as a quoted string. |
| DreamShader.Binder.Expressions | PropertyTwice | '{0}' is set twice in this call. |
| DreamShader.Binder.Expressions | ReflectedNotCalled | '{0}.{1}' is a node and has to be called: write '{0}.{1}(...)'. |
| DreamShader.Binder.Expressions | RequiredPin | '{0}.{1}' leaves its required '{2}' pin unconnected; unless the node reads a default for it, the engine reports it when the material compiles. |
| DreamShader.Binder.Expressions | RequiredPinOrConst | '{0}.{1}' leaves its required '{2}' pin unconnected and '{3}' unset; unless the node reads a default for it, the engine reports it when the material compiles. |
| DreamShader.Binder.Expressions | ResultReceiverNotLValue | '{0}' hands its return value to the argument in this place, so the argument has to be a variable. |
| DreamShader.Binder.Expressions | ResultReceiverType | '{0}' returns {1}, and the variable receiving it is {2}; a receiver has to match exactly. |
| DreamShader.Binder.Expressions | SampleArity | 'Sample' is written 'Tex.Sample(UV)' or 'Tex.Sample(Sampler, UV)'. |
| DreamShader.Binder.Expressions | SampleLevelArgument | The mip level of a texture sample |
| DreamShader.Binder.Expressions | SampleLevelArgument2 | The mip level of a texture sample |
| DreamShader.Binder.Expressions | SampleLevelArity | 'SampleLevel' is written 'Tex.SampleLevel(UV, Level)' or 'Tex.SampleLevel(Sampler, UV, Level)'. |
| DreamShader.Binder.Expressions | SampleNamedArguments | A texture sample takes its arguments in order: an optional sampler, the coordinates, and for 'SampleLevel' the mip level. |
| DreamShader.Binder.Expressions | SampleNotSampler | The second argument of 'Texture2DSample' is the sampler, and this is {0}. |
| DreamShader.Binder.Expressions | SampleNotTexture | The first argument of a texture sample is the texture, and this is {0}. |
| DreamShader.Binder.Expressions | SampleUV | The coordinates of a texture sample |
| DreamShader.Binder.Expressions | SampleUV2 | The coordinates of a texture sample |
| DreamShader.Binder.Expressions | StringInExpression | A string has no value in an expression; it is only ever the value of a reflected property, as in 'UE.Expression(Class = "...")'. |
| DreamShader.Binder.Expressions | StructConstructorCount | '{0}' has {1} fields and this call supplies {2}. |
| DreamShader.Binder.Expressions | StructField2 | This struct field |
| DreamShader.Binder.Expressions | StructFieldArgument | This struct field |
| DreamShader.Binder.Expressions | StructInitializerCount | '{0}' has {1} fields and this list has {2}. |
| DreamShader.Binder.Expressions | SubstrateClass | 'Substrate.' already names the node, so it takes no 'Class' argument. |
| DreamShader.Binder.Expressions | SwizzleLength | '.{0}' is not a swizzle; a swizzle is one to four of 'xyzw' or 'rgba'. |
| DreamShader.Binder.Expressions | SwizzleLetter | '.{0}' is not a swizzle; '{1}' is not one of 'xyzw' or 'rgba'. |
| DreamShader.Binder.Expressions | SwizzleMixedSets | '.{0}' mixes 'xyzw' with 'rgba'; a swizzle picks one family. |
| DreamShader.Binder.Expressions | SwizzleOutOfRange | '.{0}' reads component {1} of a value that has {2}. |
| DreamShader.Binder.Expressions | SwizzleRepeatTarget | '.{0}' names one component twice, so this assignment would write it twice with no order between the two; assign each component on its own line. |
| DreamShader.Binder.Expressions | Texture2DSampleArity | 'Texture2DSample' is written 'Texture2DSample(Tex, TexSampler, UV)'. |
| DreamShader.Binder.Expressions | Texture2DSampleLevelArity | 'Texture2DSampleLevel' is written 'Texture2DSampleLevel(Tex, TexSampler, UV, Level)'. |
| DreamShader.Binder.Expressions | TextureMethodNotCalled | '{0}' on a texture is a call: write 'Tex.{0}(UV)'. |
| DreamShader.Binder.Expressions | TooManyArguments | '{0}' takes {1} arguments and more were given. |
| DreamShader.Binder.Expressions | TooManyPositional | '{0}.{1}' takes {2} arguments in order; name the rest. |
| DreamShader.Binder.Expressions | TypeAsValue | '{0}' is a type, not a value; write '{0}(...)' to construct one. |
| DreamShader.Binder.Expressions | UnaryUnsupported | This operator has no graph form. |
| DreamShader.Binder.Expressions | UnknownAttribute | A material has no '{0}' pin. |
| DreamShader.Binder.Expressions | UnknownAttributeDidYouMean | A material has no '{0}' pin; did you mean '{1}'? Attribute names are case-sensitive. |
| DreamShader.Binder.Expressions | UnknownCallee | '{0}' is not a function, a builtin or a struct. |
| DreamShader.Binder.Expressions | UnknownCalleeDidYouMean | '{0}' is not declared; did you mean '{1}'? Names are case-sensitive. |
| DreamShader.Binder.Expressions | UnknownClass | '{0}' is not a material expression class this engine has. |
| DreamShader.Binder.Expressions | UnknownCustomCallOutput | This Custom node declares no output called '{0}'; its outputs are '{1}'. An output is declared by 'AdditionalOutputs'. |
| DreamShader.Binder.Expressions | UnknownEnumerator | '{0}' is not a value of '{1}' on '{2}'. |
| DreamShader.Binder.Expressions | UnknownEnumeratorDidYouMean | '{0}' is not a value of '{1}'; did you mean '{2}'? |
| DreamShader.Binder.Expressions | UnknownField | '{0}' has no field called '{1}'. |
| DreamShader.Binder.Expressions | UnknownFieldDidYouMean | '{0}' has no field called '{1}'; did you mean '{2}'? |
| DreamShader.Binder.Expressions | UnknownName | '{0}' is not declared in this scope. |
| DreamShader.Binder.Expressions | UnknownNameDidYouMean | '{0}' is not declared; did you mean '{1}'? Names are case-sensitive. |
| DreamShader.Binder.Expressions | UnknownOutput | '{0}.{1}' has no output called '{2}'. |
| DreamShader.Binder.Expressions | UnknownOutputDidYouMean | '{0}.{1}' has no output called '{2}'; did you mean '{3}'? |
| DreamShader.Binder.Expressions | UnknownOutputListed | '{0}.{1}' has no output called '{2}'; its outputs are {3}. |
| DreamShader.Binder.Expressions | UnknownReflected | '{0}.{1}' is not a node this engine has; 'UE.Expression(Class = "...")' reaches one the language has no name for. |
| DreamShader.Binder.Expressions | UnknownReflectedDidYouMean | '{0}.{1}' is not a node; did you mean '{0}.{2}'? Node names are case-sensitive. |
| DreamShader.Binder.Expressions | VectorIndexNotConstant | A component index must be a compile-time constant; write a swizzle such as '.z', or select with 'lerp'. |
| DreamShader.Binder.Expressions | VectorIndexOutOfRange | Component {0} is out of range for a value that has {1}. |
| DreamShader.Binder.Expressions | VoidCallAsValue | '{0}' returns nothing, so its call has no value; its results come back through its out parameters. |
| DreamShader.Binder.Instance | InstanceAssetInitializer | '{0}' overrides a {1} parameter, which takes an asset rather than an HLSL value; write '/// @default /Game/...' (or '/// @default None') above it instead of an initializer. |
| DreamShader.Binder.Instance | InstanceAssetNoDefault | '{0}' overrides a {1} parameter and needs the asset it is set to, as '/// @default /Game/...' or '/// @default None', and it has none. |
| DreamShader.Binder.Instance | InstanceCaseOnlyName | '{0}' matches the parent parameter '{1}' only in case, and DreamShader names are case-sensitive; write '{1}'. |
| DreamShader.Binder.Instance | InstanceDefaultOnValue | '@default' has no effect on '{0}', a {1} override whose value is its initializer; remove it. |
| DreamShader.Binder.Instance | InstanceDirectiveNeedsValue | '@{0}' needs a value after it. |
| DreamShader.Binder.Instance | InstanceDuplicateOverride | '{0}' is overridden a second time, and one instance sets a parameter once; the override on line {1} already sets it. |
| DreamShader.Binder.Instance | InstanceHoldsConstant | A '.dsi' holds a '#pragma instance' and 'uniform' overrides, and '{0}' is not a 'uniform'; an instance only assigns parameters of its parent. |
| DreamShader.Binder.Instance | InstanceHoldsFunction | A '.dsi' holds a '#pragma instance' and 'uniform' overrides, and '{0}' is a function; write it in a '.dss' or a '.dsh'. |
| DreamShader.Binder.Instance | InstanceHoldsInclude | A '.dsi' holds a '#pragma instance' and 'uniform' overrides, and has no code that could use '{0}'; remove the include. |
| DreamShader.Binder.Instance | InstanceHoldsMaterialPragma | A '.dsi' is configured by '#pragma instance', and '#pragma material' configures a material; write these keys in '#pragma instance(...)'. |
| DreamShader.Binder.Instance | InstanceHoldsStruct | A '.dsi' holds a '#pragma instance' and 'uniform' overrides, and 'struct {0}' declares a type; write it in a '.dsh'. |
| DreamShader.Binder.Instance | InstanceIgnoresGraphPragma | '#pragma {0}' boxes or places graph nodes, and a '.dsi' has no graph; the line was ignored. |
| DreamShader.Binder.Instance | InstanceInitializer | The value of the override '{0}' |
| DreamShader.Binder.Instance | InstanceLayerOnlyParameter | '{0}' exists in the parent only as a layer or blend parameter, and a '.dsi' overrides global parameters only. |
| DreamShader.Binder.Instance | InstanceNameNeedsValue | '@{0}' needs a value after it. |
| DreamShader.Binder.Instance | InstanceNoPragma | A '.dsi' needs one '#pragma instance(Parent = "...")' naming the material it is an instance of, and this file has none. |
| DreamShader.Binder.Instance | InstanceNoSchema | The parameters of the parent are not available here, so the names and types of these overrides are checked only for their shape; compile the parent first, or check the file in the editor. |
| DreamShader.Binder.Instance | InstanceOverrideArray | '{0}' is an array, and an instance override assigns one parameter; override each element's parameter on its own. |
| DreamShader.Binder.Instance | InstanceOverrideDirectiveNoEffect | '@{0}' has no effect on an instance override, because an override assigns only the parent parameter's value and its metadata stays on the parent; remove it. |
| DreamShader.Binder.Instance | InstanceOverrideNoKind | '{0}' is declared '{1}', which is no material parameter type; an override is a number, a bool, a texture, or one of RuntimeVirtualTexture, SparseVolumeTexture, TextureCollection, ParameterCollection and Font. |
| DreamShader.Binder.Instance | InstancePageMalformed | '@page' takes one whole number of zero or more, and '{0}' is not that. |
| DreamShader.Binder.Instance | InstancePageNotFont | '@page' picks the page of a Font override, and '{0}' is a {1} override. |
| DreamShader.Binder.Instance | InstanceParentEmpty | 'Parent' in '#pragma instance' names the material this is an instance of, and it is empty. |
| DreamShader.Binder.Instance | InstanceParentMissing | '#pragma instance' needs 'Parent = "/Game/.../M_Parent"' naming the material this is an instance of, and it has none. |
| DreamShader.Binder.Instance | InstancePragmaDirectiveNoEffect | '@{0}' has no effect above '#pragma instance', where only '@name' is read; remove it. |
| DreamShader.Binder.Instance | InstancePragmaDuplicateKey | '{0}' is set twice by '#pragma instance'; it was already set on line {1}. |
| DreamShader.Binder.Instance | InstancePragmaOutsideDsi | '#pragma instance' declares a material instance and belongs in a '.dsi' file of its own, and this line is in '{0}'; move it and its overrides into a '.dsi'. |
| DreamShader.Binder.Instance | InstancePragmaPositional | '#pragma instance' takes 'Key = Value' pairs, and '{0}' has no key. |
| DreamShader.Binder.Instance | InstancePrunedParameter | '{0}' is declared by the parent source but nothing there reads it, so the parent material has no such parameter (DSH4390); read it in the parent, or remove this override. |
| DreamShader.Binder.Instance | InstanceSecondPragma | '#pragma instance' is written a second time, and one '.dsi' is one material instance; the line {0} already declares it. |
| DreamShader.Binder.Instance | InstanceStaticExtra | '{0}' is a {1} parameter the parent sets at run time, and '/// @static' on this override asks for a static one; remove '@static'. |
| DreamShader.Binder.Instance | InstanceStaticMissing | '{0}' is a static {1} parameter in the parent, so the override needs '/// @static', and it has none. |
| DreamShader.Binder.Instance | InstanceStaticOnAsset | '/// @static' on an instance override means a static switch ('uniform bool') or a static component mask ('uniform bool4'), and '{0}' is declared '{1}'. |
| DreamShader.Binder.Instance | InstanceStaticOnValue | '/// @static' on an instance override means a static switch ('uniform bool') or a static component mask ('uniform bool4'), and '{0}' is declared '{1}'. |
| DreamShader.Binder.Instance | InstanceTypeMismatch | '{0}' is a {1} parameter of type '{2}' in the parent, and this override declares '{3}'. |
| DreamShader.Binder.Instance | InstanceUnknownParameter | '{0}' is not a parameter of the parent '{1}'. |
| DreamShader.Binder.Instance | InstanceValueNoInitializer | '{0}' overrides a {1} parameter and needs the value it is set to, as 'uniform {2} {0} = ...;', and it has no initializer. |
| DreamShader.Binder.Instance | InstanceValueNotConstant | '{0}' is set to a value the compiler can fold, a literal or an expression over literals, and this initializer is not one. |
| DreamShader.Binder.Legacy | HoistedCallNoInputValue | '{0}' lifts the call behind its input '{1}', and that call makes {2}, which no custom node input carries. |
| DreamShader.Binder.Legacy | HoistedCallReadsCallerLocal | '{0}' lifts a 'UE.' call out of its body that reads '{1}', which 1.x took from the caller's scope and 2.0 does not; pass '{1}' to '{0}' as a parameter. |
| DreamShader.Binder.Legacy | HoistedCallsNotCustom | '{0}' carries 'UE.' calls lifted out of its body, and only a '@custom' function with a verbatim body lifts calls into its node's inputs. |
| DreamShader.Binder.Legacy | HoistedCallUnknownName | '{0}' lifts a 'UE.' call out of its body that reads '{1}', which is neither a parameter of '{0}' nor declared at file scope. |
| DreamShader.Binder.Legacy | LegacyCaseFallback | '{0}' matches '{1}' only in case; a 1.x source is read ignoring case, so this is '{1}', and a '.dss' needs the exact spelling. |
| DreamShader.Binder.Legacy | LegacyCatalogCaseFallback | '{0}' matches the engine name '{1}' only in case; 1.x matched engine names ignoring case, so this is '{1}', and a '.dss' needs the exact spelling. |
| DreamShader.Binder.Legacy | LegacyImplicitOutLocal | '{0}' is not declared, and as in 1.x it is declared here as a local of type {1} receiving '{2}' of '{3}'. |
| DreamShader.Binder.Legacy | LegacySelectNoOutputs | '{0}' has no output to select, and this call selects '{1}'. |
| DreamShader.Binder.Legacy | LegacySelectOrdinalRange | '{0}' has {1} output(s), counted from 0 with the return value first, and this call selects output {2}. |
| DreamShader.Binder.Legacy | LegacySelectUnknownOutput | '{0}' has no output called '{1}'; its outputs are {2}. |
| DreamShader.Binder.Statements | BreakOutsideLoop | 'break' leaves a loop, and this one is not inside a 'for', 'while' or 'do'. |
| DreamShader.Binder.Statements | ConditionNotScalar | {0} has to be a single true-or-false value, and this is {1}. |
| DreamShader.Binder.Statements | ConstantNotConstant | '{0}' is a compile-time constant, and this initializer is not one; a constant is built from literals and other constants. |
| DreamShader.Binder.Statements | ContinueOutsideLoop | 'continue' starts the next turn of a loop, and this one is not inside a 'for', 'while' or 'do'. |
| DreamShader.Binder.Statements | DiscardOutsideEntry | 'discard' belongs in a material entry or a layer; a material function has no pixel of its own to drop. Write the mask into 'm.OpacityMask' instead. |
| DreamShader.Binder.Statements | DoWhileCondition | The condition of a 'do' |
| DreamShader.Binder.Statements | ForCondition | The condition of a 'for' |
| DreamShader.Binder.Statements | GlobalInitializer | The initializer of '{0}' |
| DreamShader.Binder.Statements | IfCondition | The condition of an 'if' |
| DreamShader.Binder.Statements | LocalConstantNeedsInitializer | '{0}' is a compile-time constant and must be initialised where it is declared. |
| DreamShader.Binder.Statements | LocalConstantNotConstant | '{0}' is a compile-time constant, and this initializer is not one. |
| DreamShader.Binder.Statements | LocalInitializer | The initializer of '{0}' |
| DreamShader.Binder.Statements | LocalRedeclared | '{0}' is already declared in this block. |
| DreamShader.Binder.Statements | LocalShadowsGlobal | '{0}' hides the file-scope declaration of the same name for the rest of this function. |
| DreamShader.Binder.Statements | LocalShadowsLocal | '{0}' hides a variable of the same name from an enclosing block; give one of them another name. |
| DreamShader.Binder.Statements | LocalShadowsParam | '{0}' hides the parameter of the same name; give one of them another name. |
| DreamShader.Binder.Statements | OutParamNeverAssigned | '{0}' is an 'out' parameter of '{1}' but the body never assigns it, so a caller would read a value nothing produced. Assign it before the function returns, or remove the parameter. |
| DreamShader.Binder.Statements | ParameterDefault | The default value of '{0}' |
| DreamShader.Binder.Statements | ReturnValue | The value returned from '{0}' |
| DreamShader.Binder.Statements | ReturnWithoutValue | '{0}' returns {1}, so this 'return' needs a value. |
| DreamShader.Binder.Statements | ReturnWithValue | '{0}' returns nothing, so this 'return' cannot carry a value; extra results are written to 'out' parameters. |
| DreamShader.Binder.Statements | VoidLocal | A variable cannot be 'void'. |
| DreamShader.Binder.Statements | WhileCondition | The condition of a 'while' |
| DreamShader.CommandletRunner | DecompileDiagnosticsOutFailed | The diagnostics JSON could not be written: {0}. |
| DreamShader.CustomHlsl | CustomBadFunctionIndex | Custom node code was asked for function {0}, but the bound module has {1}. |
| DreamShader.CustomHlsl | CustomCallArity | '{0}' takes {1} argument(s) but this call passes {2}; a call that carries a texture cannot be matched up by position otherwise. |
| DreamShader.CustomHlsl | CustomCallCycle | The '@custom' functions {0} call each other in a cycle; HLSL has no recursion, so their bodies cannot be embedded in a custom node. |
| DreamShader.CustomHlsl | CustomCallsGraphFunction | '{0}' is not a '@custom' function and cannot be called from the HLSL body of '{1}'; a custom node sees no graph values, so mark '{0}' '@custom' as well or move the call out of the body. |
| DreamShader.CustomHlsl | CustomCallsHoistingFunction | '{0}' takes the 'UE.' calls lifted out of its body as inputs of its own custom node, so the HLSL body of '{1}' cannot call it; call it from a graph body instead. |
| DreamShader.CustomHlsl | CustomCaseOnlyMatch | '{0}' differs from the function '{1}' only in case; HLSL is case-sensitive, so this call is left for the shader compiler. Did you mean '{1}'? |
| DreamShader.CustomHlsl | CustomDuplicateParameterName | '{0}' declares '{1}' twice in the HLSL it generates; a texture parameter also claims '{1}Sampler', which the engine declares alongside it. |
| DreamShader.CustomHlsl | CustomEmptyInclude | '{0}' has an '#include' with an empty path; write the virtual shader path the header lives at, for example "/Engine/Private/Common.ush". |
| DreamShader.CustomHlsl | CustomHoistedRange | '{0}' lifts the call behind its input '{1}' out of a place its body does not have, so its custom node's code cannot be built. |
| DreamShader.CustomHlsl | CustomInoutParameter | '{0}' declares '{1}' as 'inout', which a custom node cannot carry; split it into an 'in' parameter and an 'out' parameter. |
| DreamShader.CustomHlsl | CustomMaterialInput | '{0}' takes the material '{1}' as an input; a custom node cannot accept material attributes on a pin, so read the fields the body needs and pass them as floats. |
| DreamShader.CustomHlsl | CustomNeverReturns | '{0}' declares a return type but its body never returns a value; the node's first output will be 0. |
| DreamShader.CustomHlsl | CustomNotACustomFunction | '{0}' is not marked '/// @custom', so it has no custom node to build. |
| DreamShader.CustomHlsl | CustomNoVerbatimBody | '{0}' has no verbatim HLSL body, so it cannot become a custom node; only a '/// @custom' function can. |
| DreamShader.CustomHlsl | CustomPrepareFailed | '{0}' could not be prepared for a custom node. |
| DreamShader.CustomHlsl | CustomSelfContainedCall | '{0}' is 'selfcontained', so the '@custom' function '{1}' it calls is not embedded in its node; the call is left for the shader compiler to resolve out of this body's own includes. |
| DreamShader.CustomHlsl | CustomSubstrateParameter | '{0}' uses Substrate on '{1}'; a custom node has no Substrate pins, so build that part of the material out of reflected Substrate nodes. |
| DreamShader.CustomHlsl | CustomSubstrateReturn | '{0}' returns Substrate; a custom node has no Substrate pins, so build that part of the material out of reflected Substrate nodes. |
| DreamShader.CustomHlsl | CustomTextureArgumentNotAName | The texture argument for '{0}' of '{1}' has to be a plain texture name, because the sampler that goes with it is named after it. |
| DreamShader.CustomHlsl | CustomTextureOutput | '{0}' returns a texture through '{1}'; a custom node output carries float1..4 or material attributes, never a texture object. |
| DreamShader.CustomHlsl | CustomTextureReturn | '{0}' returns a texture object; a custom node output carries float1..4 or material attributes, never a texture object. |
| DreamShader.CustomHlsl | CustomVoidBareReturn | '{0}' returns void but its body uses 'return;'; a custom node always returns its first output, so give the function a return type or restructure the body. |
| DreamShader.Decompiler.GraphImport | AttributeOutputUnknown | {0} is read through its output {1}, which is no material attribute the catalog knows; a zero stands in for that read. |
| DreamShader.Decompiler.GraphImport | BoolFunctionInput | The input '{0}' of '{1}' is a dynamic bool pin, which the language has no parameter for; it is written as 'bool' and rebuilds as a scalar pin. |
| DreamShader.Decompiler.GraphImport | BreakWithoutMaterial | {0} reads the attributes of no material; a zero stands in for every value read from it. |
| DreamShader.Decompiler.GraphImport | CallWithoutFunction | {0} calls a material function that is missing; a zero stands in for every value read from it. |
| DreamShader.Decompiler.GraphImport | CaptionDropped | '{0}' has the caption '{1}', which source has no directive for; the rebuilt function has none. |
| DreamShader.Decompiler.GraphImport | ClassNotInCatalog | {0} is of a class the builtin catalog does not list (abstract, deprecated, or from a module loaded after the catalog was built); a zero stands in for every value read from it. |
| DreamShader.Decompiler.GraphImport | CustomDefinesDropped | {0} carries {1} additional define(s), which a '/// @custom' function cannot declare; the rebuilt node has none. Move them into the body as '#define' lines. |
| DreamShader.Decompiler.GraphImport | DescribeExpression | '{0}' ({1}) of '{2}' |
| DreamShader.Decompiler.GraphImport | DescribeNoExpression | an expression |
| DreamShader.Decompiler.GraphImport | DynamicPinsDropped | {0} has {1} wired input(s) the class does not declare as named pins, so a call cannot connect them; the rebuilt node leaves them unconnected. |
| DreamShader.Decompiler.GraphImport | GetAttributesAsBreak | {0} is a GetMaterialAttributes node; reading a material's attributes always rebuilds as a BreakMaterialAttributes. |
| DreamShader.Decompiler.GraphImport | GraphCycle | {0} reads {1}, which reads it back; the graph has a cycle, and a zero stands in for that read. |
| DreamShader.Decompiler.GraphImport | MakeAttributeUnknown | {0} sets '{1}', which is no material attribute the catalog knows; the connection is dropped. |
| DreamShader.Decompiler.GraphImport | OutputPastCatalog | {0} is read through its output {1}, which the node has no slot for; its first output is read instead. |
| DreamShader.Decompiler.GraphImport | ParameterPropertySkipped | {0} changes '{1}', which a '///' directive cannot carry; the rebuilt parameter has the default. |
| DreamShader.Decompiler.GraphImport | PinsIgnoredUnderAttributes | '{0}' reads its attributes as one set, so the {1} individual pin(s) that are also wired are ignored, by the engine and here. |
| DreamShader.Decompiler.GraphImport | PreviewPinConnected | The input '{0}' of '{1}' has its Preview pin wired. Source says that as a default that is an expression ('float {0} = UE.TexCoord(Index = 1).r'), which the decompiler does not write yet; the rebuilt input previews its number. |
| DreamShader.Decompiler.GraphImport | PropertyKindSkipped | {0} changes '{1}', a property of a kind a call argument cannot carry (an array, a map, a delegate); the rebuilt node has the default. |
| DreamShader.Decompiler.GraphImport | RerouteCycle | The reroute {0} feeds itself; the pin that reads it is treated as unconnected. |
| DreamShader.Decompiler.GraphImport | RerouteDangling | The reroute {0} has nothing wired into it; the pin that reads it is treated as unconnected. |
| DreamShader.Decompiler.GraphImport | RerouteUsageWithoutDeclaration | The named reroute {0} has no declaration; the pin that reads it is treated as unconnected. |
| DreamShader.Decompiler.GraphImport | StaticSwitchFalseBranch | False |
| DreamShader.Decompiler.GraphImport | StaticSwitchFolded | {0} has nothing wired to Value, so it is its {1} branch and nothing else; the rebuilt graph has no switch there. |
| DreamShader.Decompiler.GraphImport | StaticSwitchParameterSplit | {0} is a StaticSwitchParameter, which the language writes as a '/// @static' uniform and a static branch; the rebuilt graph has those two nodes in its place. |
| DreamShader.Decompiler.GraphImport | StaticSwitchTrueBranch | True |
| DreamShader.Decompiler.GraphImport | TextureSampleParameterSplit | {0} is a texture sample parameter, which the language writes as a texture uniform and a sample of it; the rebuilt graph has a TextureObjectParameter and a TextureSample in its place. |
| DreamShader.Decompiler.GraphImport | UnreachableExpressions | {0} expression(s) of '{1}' feed no output and are not part of the source; a build would prune them all the same. |
| DreamShader.Decompiler.GraphImport | UnsupportedAsset | '{0}' is neither a material nor a material function, so it has no graph to read. A material instance decompiles to a '.dsi'. |
| DreamShader.Decompiler.Impl | AppendNodeOverflow | Append node '{0}' resolved to {1} + {2} components, which cannot fit a float4; masked its inputs down to {3} + {4}. Review the emitted swizzle. |
| DreamShader.Decompiler.Impl | DecompilingMaterial | Decompiling Material '{0}'... |
| DreamShader.Decompiler.Impl | DecompilingMaterialFunction | Decompiling Material Function '{0}'... |
| DreamShader.Decompiler.Impl | DecompilingNode | Decompiling node {0}: {1} |
| DreamShader.Decompiler.Impl | DuplicateCustomOutputNode | Material has more than one '{0}' node; output targets are de-duplicated by class, so only the first was exported. |
| DreamShader.Decompiler.Impl | EmitVirtualFunctionFailed | Failed to emit VirtualFunction for '{0}': {1} |
| DreamShader.Decompiler.Impl | ExportedAsUEExpression | Exported '{0}' as UE.Expression; review reflected literal properties if the node has editor-only state. |
| DreamShader.Decompiler.Impl | FormattingDSFSource | Formatting DSF source for '{0}'... |
| DreamShader.Decompiler.Impl | FormattingDSMSource | Formatting DSM source for '{0}'... |
| DreamShader.Decompiler.Impl | GetMaterialAttributesMissingOutput | GetMaterialAttributes node '{0}' has no attribute for output {1}; emitted a default literal. |
| DreamShader.Decompiler.Impl | MaterialFunctionCallMissingAsset | A MaterialFunctionCall had no function asset and was exported as a zero literal. |
| DreamShader.Decompiler.Impl | MaterialFunctionCallNotPlain | MaterialFunctionCall '{0}' is not a plain MaterialFunction; it was exported through UE.Expression. |
| DreamShader.Decompiler.Impl | MaterialFunctionHasNoOutputs | MaterialFunction '{0}' does not expose any outputs. |
| DreamShader.Decompiler.Impl | NamedRerouteUsageInvalidDeclaration | Named reroute usage '{0}' has no valid declaration; emitted a default literal. |
| DreamShader.Decompiler.Impl | NamedRerouteUsageMissingDeclaration | Named reroute usage '{0}' has no valid declaration; emitted its default value. |
| DreamShader.Decompiler.Impl | NoMaterialAssetProvided | No Material asset was provided. |
| DreamShader.Decompiler.Impl | NoMaterialFunctionAssetProvided | No MaterialFunction asset was provided. |
| DreamShader.Decompiler.Impl | RecursiveGraphDependency | Detected a recursive graph dependency while decompiling node '{0}'; emitted a default literal to avoid stack overflow. |
| DreamShader.Decompiler.Impl | RecursiveNamedRerouteDependency | Detected a recursive named reroute dependency for '{0}'; emitted a default literal to avoid stack overflow. |
| DreamShader.Decompiler.Impl | RecursiveRerouteDependency | Detected a recursive reroute dependency while decompiling node '{0}'; emitted a default literal to avoid stack overflow. |
| DreamShader.Decompiler.Impl | ScanningFunctionInputsOutputs | Scanning function inputs and outputs for '{0}'... |
| DreamShader.Decompiler.Impl | ScanningMaterialOutputs | Scanning material outputs for '{0}'... |
| DreamShader.Decompiler.Instance | AtlasOverride | '{0}' is a curve atlas row, and '{1}' picks its curve; a '.dsi' can state the row's number and nothing else, so the rebuilt instance loses the curve. |
| DreamShader.Decompiler.Instance | BoolParameterHoldsNumber | '{0}' is declared 'bool' by the parent's source and '{1}' sets it to {2}; the override is written as a 'float'. |
| DreamShader.Decompiler.Instance | InstanceWithoutParent | '{0}' has no parent material, and a '.dsi' is nothing but overrides of one. |
| DreamShader.Decompiler.Instance | LayerOverridesSkipped | '{0}' overrides {1} parameter(s) of its material layers or blends, which a '.dsi' cannot address; they are left out. |
| DreamShader.Decompiler.Instance | OverrideNameSanitized | The parameter '{0}' is not a name a variable can have; it is written under another with '/// @name {0}'. |
| DreamShader.Decompiler.Instance | UnsupportedInstanceState | '{0}' overrides '{1}', which '#pragma instance' has no key for; the rebuilt instance has the parent's. |
| DreamShader.Decompiler.IR | ImportedModuleInvalid | The graph of '{0}' did not read into a valid module; the errors above say where. This is a defect of the decompiler, not of the asset. |
| DreamShader.Decompiler.IR | InstanceNeedsDsi | '{0}' is a material instance, which decompiles to a '.dsi', and '{1}' is not one. |
| DreamShader.Decompiler.IR | PrintedTextDoesNotParse | The decompiled text does not parse back: {0}: {1}. It is written as it is; this is a defect of the decompiler. |
| DreamShader.Decompiler.IR | PrintedTextNoReason | the parser gave no reason |
| DreamShader.Decompiler.IR | SourceProductMissing | '{0}' builds '{1}', and that asset does not exist or is not a material or a material function; build the source first. |
| DreamShader.Decompiler.IR | SourceProductsUnresolved | '{0}' does not resolve to the assets it builds, so there is nothing to decompile for it. |
| DreamShader.Decompiler.IR | UnsupportedAssetClass | '{0}' is a {1}, which no DreamShader source describes; a decompile takes a material, a material function, layer or blend, or a material instance. |
| DreamShader.Decompiler.Service | DecompileDidNotProduceSourceText | Decompile did not produce source text. |
| DreamShader.Decompiler.Service | DecompileDidNotSayWhy | The decompile failed without reporting why. |
| DreamShader.Decompiler.Service | DecompileDssLanguage | 2.0 |
| DreamShader.Decompiler.Service | DecompileDssText | 2.0 |
| DreamShader.Decompiler.Service | DecompileFormatContradictsExtension | '{0}' ends in '.{1}', which is read as {2} source, and the decompile was asked for {3} text; name the file after the text, or leave the format to the extension. |
| DreamShader.Decompiler.Service | DecompileLegacyLanguage | 1.x |
| DreamShader.Decompiler.Service | DecompileLegacyText | 1.x |
| DreamShader.Decompiler.Service | FailedToCreateOutputDirectory | DreamShader failed to create output directory '{0}'. |
| DreamShader.Decompiler.Service | FailedToResolveOutputFilePath | DreamShader failed to resolve an output file path. |
| DreamShader.Decompiler.Service | FailedToWriteDecompiledSource | DreamShader failed to write decompiled source '{0}'. |
| DreamShader.Decompiler.Service | LegacyDecompileNeedsAsset | The 1.x decompiler takes one asset at a time; name the asset rather than its source. |
| DreamShader.Decompiler.Service | NoAssetProvided | No asset was provided. |
| DreamShader.Decompiler.Service | UnsupportedAssetType | DreamShader decompile supports Material and MaterialFunction assets only: {0} |
| DreamShader.EditorDecompileTools | DecompileToolsNoReason | The decompiler reported a failure without saying why. |
| DreamShader.EditorDecompileTools | DecompileToolsSourceNotBuilt | Nothing '{0}' builds exists yet, so there is nothing to decompile; compile it first. |
| DreamShader.EditorDecompileTools | DecompileToolsSourceUnresolved | '{0}' could not be resolved to the assets it builds. |
| DreamShader.Emitter | AndFailed | Failed to create the Multiply node a logical and lowers to. |
| DreamShader.Emitter | AppendArity | An Append node takes two operands; this one has {0}. Three or more parts are chained by the IR builder, not here. |
| DreamShader.Emitter | AppendFailed | Failed to create an AppendVector node. |
| DreamShader.Emitter | AssetDiverged | '{0}' no longer holds what DreamShader generated into it, so it was not rebuilt. {1} |
| DreamShader.Emitter | AssetOpenInEditor | '{0}' is open in an asset editor, so it was not rebuilt. {1} |
| DreamShader.Emitter | BadFunctionInputType | '{0}' is not a material function input type; write one of Scalar, Vector2, Vector3, Vector4, Texture2D, TextureCube, Texture2DArray, VolumeTexture, StaticBool, Bool, MaterialAttributes or Substrate. |
| DreamShader.Emitter | BadNodeIndex | The topological order of '{0}' names node {1}, which the graph does not have. |
| DreamShader.Emitter | BadProductIndex | Product index {0} does not exist in this module, which has {1}. |
| DreamShader.Emitter | BadSamplerType | '{0}' is not a sampler type; write one of the EMaterialSamplerType names, such as Color, Normal or LinearColor. |
| DreamShader.Emitter | BreakAttributesFailed | Failed to create a BreakMaterialAttributes node. |
| DreamShader.Emitter | BreakAttributesSlotNotPublished | BreakMaterialAttributes does not publish the attribute '{0}', so it cannot be read from a material that came through a pin. |
| DreamShader.Emitter | CollectionParameterMissing | The material parameter collection '{0}' has no parameter called '{1}'. |
| DreamShader.Emitter | CompareArity | A Compare node takes five operands (A, B, greater, equal, less); this one has {0}. |
| DreamShader.Emitter | CompareFailed | Failed to create an If node. |
| DreamShader.Emitter | CompareLowerFailed | Failed to create the If node a comparison lowers to. |
| DreamShader.Emitter | ConnectNoExpression | Cannot connect an input on an expression that was not created. |
| DreamShader.Emitter | Constant2Failed | Failed to create a Constant2Vector node. |
| DreamShader.Emitter | Constant3Failed | Failed to create a Constant3Vector node. |
| DreamShader.Emitter | Constant4Failed | Failed to create a Constant4Vector node. |
| DreamShader.Emitter | ConstantFailed | Failed to create a Constant node. |
| DreamShader.Emitter | ConstantNoValue | A Constant node carries no Value property. |
| DreamShader.Emitter | CoreOpClassMissing | The core operation '{0}' maps to expression class '{1}', which this engine does not have. |
| DreamShader.Emitter | CoreOpFailed | Failed to create a '{0}' node. |
| DreamShader.Emitter | CoreOpTooManyOperands | The core operation '{0}' was given {1} operands, but its table names only {2} pins. |
| DreamShader.Emitter | CreateFunctionFailed | The material function for '{0}' could not be created or reused. {1} |
| DreamShader.Emitter | CreateInstanceFailed | The ThinCustom instance for '{0}' could not be created or reused. {1} |
| DreamShader.Emitter | CreateMaterialFailed | The material for '{0}' could not be created or reused. {1} |
| DreamShader.Emitter | CreateMaterialInstanceFailed | The material instance for '{0}' could not be created or reused. {1} |
| DreamShader.Emitter | CreateThinBaseFailed | The hidden base material for '{0}' could not be created. {1} |
| DreamShader.Emitter | CustomBadAdditionalOutput | '{0}' is not a Custom node additional output; the emitter expects Name:Type. |
| DreamShader.Emitter | CustomBadAdditionalOutputType | '{0}' is not a Custom node output type; expected Float1 through Float4 or MaterialAttributes. |
| DreamShader.Emitter | CustomBadOutputType | '{0}' is not a Custom node output type; expected Float1 through Float4 or MaterialAttributes. |
| DreamShader.Emitter | CustomFailed | Failed to create a Custom node. |
| DreamShader.Emitter | DeferredToWriteOwner | '{0}' was left alone: another editor owns writing this project's generated assets to disk. |
| DreamShader.Emitter | DestinationFailed | '{0}' does not resolve to a valid asset path. {1} |
| DreamShader.Emitter | EmitCancelled | Building '{0}' was cancelled; the asset is as it was before this compile. |
| DreamShader.Emitter | FunctionCallAssignFailed | '{0}' could not be assigned to the generated call node; a material function cannot call itself, directly or through another function. |
| DreamShader.Emitter | FunctionCallFailed | Failed to create a MaterialFunctionCall node. |
| DreamShader.Emitter | FunctionCallLoadFailed | The material function asset '{0}' could not be loaded. |
| DreamShader.Emitter | FunctionCallNoPath | This function call names no material function asset. |
| DreamShader.Emitter | FunctionCallUnknownInput | '{0}' has no input named '{1}'. |
| DreamShader.Emitter | FunctionCallUnknownOutput | '{0}' has no output named '{1}'. |
| DreamShader.Emitter | FunctionInputFailed | Failed to create a FunctionInput node. |
| DreamShader.Emitter | FunctionOutputArity | Function output '{0}' has {1} operands; it needs exactly the one value it returns. |
| DreamShader.Emitter | FunctionOutputFailed | Failed to create a FunctionOutput node. |
| DreamShader.Emitter | FunctionReferenceUnresolved | The function reference '{0}' does not resolve to an asset path. {1} |
| DreamShader.Emitter | FwidthFailed | Failed to create the nodes fwidth lowers to. |
| DreamShader.Emitter | InstanceParentCycle | '{0}' cannot parent to '{1}': that parent already descends from this instance. |
| DreamShader.Emitter | InstanceParentDoesNotLoad | The parent '{0}' of '{1}' does not load; compile the source that builds it, or correct the Parent key. |
| DreamShader.Emitter | InstanceParentDrift | The parent asset '{0}' has no parameter {1} of the kind this instance overrides; the asset is older than its source. Compile the parent source first. |
| DreamShader.Emitter | InstanceParentDropped | The engine dropped '{0}' as the parent of '{1}' while applying the static overrides; that parent does not allow them. |
| DreamShader.Emitter | InstanceParentMaterialized | '{0}' was memory-only, so it was saved to disk first: '{1}' parents to it. |
| DreamShader.Emitter | InstanceParentMaterializeFailed | The parent '{0}' of '{1}' exists only in memory and could not be saved first. {2} |
| DreamShader.Emitter | InstanceParentMemoryOnlyForeign | The parent '{0}' exists only in memory and no DreamShader source builds it, so '{1}' cannot be saved against it; save the parent first. |
| DreamShader.Emitter | InstanceValueNotApplied | The override of '{0}' could not be applied: {1} |
| DreamShader.Emitter | LocalFunctionNotEmitted | This call targets product {0} of the same file, but that product has not been emitted yet; the pipeline must compile products in dependency order. |
| DreamShader.Emitter | MakeAttributesFailed | Failed to create a MakeMaterialAttributes node. |
| DreamShader.Emitter | MakeAttributesNoPin | MakeMaterialAttributes has no pin for the attribute '{0}'. |
| DreamShader.Emitter | MissingOperand | This node needs operand {0}, but it has only {1}. |
| DreamShader.Emitter | NegateFailed | Failed to create the Multiply node a negation lowers to. |
| DreamShader.Emitter | NoLowering | The core operation '{0}' has no engine expression class and no lowering in the emitter. |
| DreamShader.Emitter | NoOrganizationField | '{0}' exposes no '{1}' field, so that value was not written. |
| DreamShader.Emitter | NoParameterNameProperty | '{0}' exposes no ParameterName, so the name '{1}' was not written. |
| DreamShader.Emitter | NotFailed | Failed to create the Subtract node a logical not lowers to. |
| DreamShader.Emitter | OrFailed | Failed to create the Max node a logical or lowers to. |
| DreamShader.Emitter | OrganizationFieldFailed | '{0}' could not take '{1}' for its '{2}' field. {3} |
| DreamShader.Emitter | ParameterNameFailed | '{0}' could not take '{1}' as its parameter name. {2} |
| DreamShader.Emitter | PropertyMissing | '{0}' has no property named '{1}'. |
| DreamShader.Emitter | PropertyNotReflected | '{0}' has no property named '{1}', so that value was not written. |
| DreamShader.Emitter | PropertyWriteFailed | '{0}' could not take '{1}' for its '{2}' property. {3} |
| DreamShader.Emitter | RcpFailed | Failed to create the Divide node a reciprocal lowers to. |
| DreamShader.Emitter | ReflectedFailed | Failed to create a '{0}' node. |
| DreamShader.Emitter | ReflectFailed | Failed to create the nodes reflect lowers to. |
| DreamShader.Emitter | RefractFailed | Failed to create the nodes refract lowers to. |
| DreamShader.Emitter | RollbackNotArmed | '{0}' could not be snapshotted before rebuilding it, so a failed rebuild will not be rolled back. |
| DreamShader.Emitter | RollbackNotArmedFunction | '{0}' could not be snapshotted before rebuilding it, so a failed rebuild will not be rolled back. |
| DreamShader.Emitter | RsqrtFailed | Failed to create the nodes an inverse square root lowers to. |
| DreamShader.Emitter | SaveFunctionFailed | '{0}' was built but could not be saved. {1} |
| DreamShader.Emitter | SaveInstanceFailed | '{0}' was built but could not be saved. {1} |
| DreamShader.Emitter | SaveMaterialFailed | '{0}' was built but could not be saved. {1} |
| DreamShader.Emitter | SaveMaterialInstanceFailed | '{0}' was built but could not be saved. {1} |
| DreamShader.Emitter | ScalarParamFailed | Failed to create a ScalarParameter node. |
| DreamShader.Emitter | SelectArity | A Select node takes three operands (condition, then, else); this one has {0}. |
| DreamShader.Emitter | SelectFailed | Failed to create the If node a select lowers to. |
| DreamShader.Emitter | SetAttributesConnectFailed | SetMaterialAttributes could not take a value for the attribute '{0}'. |
| DreamShader.Emitter | SetAttributesFailed | Failed to create a SetMaterialAttributes node. |
| DreamShader.Emitter | SettingsRefused | A material setting on '{0}' was refused. {1} |
| DreamShader.Emitter | SinkNoPropertyInput | This material has no input for the attribute '{0}'; check the material domain and shading model the file asks for. |
| DreamShader.Emitter | SourceHashCurrent | '{0}' was left alone: its source hash is unchanged since it was last built. |
| DreamShader.Emitter | StaticBoolDefaultFailed | Failed to create the StaticBool node that holds a static bool input's default. |
| DreamShader.Emitter | StaticBoolParamFailed | Failed to create a StaticBoolParameter node. |
| DreamShader.Emitter | StaticSwitchArity | A StaticSwitch node takes three operands (condition, then, else); this one has {0}. |
| DreamShader.Emitter | StaticSwitchFailed | Failed to create a StaticSwitch node. |
| DreamShader.Emitter | SwizzleBadMask | '{0}' is not a canonical swizzle mask; the emitter expects a subset of xyzw in order. |
| DreamShader.Emitter | SwizzleFailed | Failed to create a ComponentMask node. |
| DreamShader.Emitter | SwizzleNoMask | A Swizzle node carries no Mask property. |
| DreamShader.Emitter | TextureDefaultMissing | The default texture for parameter '{0}' could not be loaded from '{1}'. |
| DreamShader.Emitter | TextureDefaultReferenceUnresolved | The default texture reference '{0}' of parameter '{1}' does not resolve to an asset path. {2} |
| DreamShader.Emitter | TextureParamFailed | Failed to create a TextureObjectParameter node. |
| DreamShader.Emitter | TextureSampleFailed | Failed to create a TextureSample node. |
| DreamShader.Emitter | UnhandledOp | The emitter has no rule for the IR operation '{0}'. |
| DreamShader.Emitter | UnknownAttribute | '{0}' is not a material attribute this engine has. |
| DreamShader.Emitter | UnknownExpressionClass | '{0}' is not a material expression class this engine has. |
| DreamShader.Emitter | UnknownPin | '{0}' has no input pin named '{1}'. |
| DreamShader.Emitter | UnknownProductKind | '{0}' has a product kind the emitter does not know how to materialize. |
| DreamShader.Emitter | ValueNotEmitted | This node reads node {0}, which has not been emitted; the graph's topological order is inconsistent. |
| DreamShader.Emitter | VectorParamFailed | Failed to create a VectorParameter node. |
| DreamShader.InstanceSettings | BadKeyValue | '{0}' is not a valid value for the instance key '{1}'. {2} |
| DreamShader.InstanceSettings | RefusedBackend | an instance has no backend of its own; it is a plain material instance of its parent. |
| DreamShader.InstanceSettings | RefusedBaseStruct | write the overrides as keys of their own, such as BlendMode or TwoSided. |
| DreamShader.InstanceSettings | RefusedFlag | an override flag follows from the key that sets its value. |
| DreamShader.InstanceSettings | RefusedKey | '{0}' cannot be set from an instance file: {1} |
| DreamShader.InstanceSettings | RefusedParameterArray | parameter values are written as uniform overrides. |
| DreamShader.InstanceSettings | RefusedUsageFlags | usage flags are a bitmask the engine merges with the parent's, which one key value cannot express. |
| DreamShader.InstanceSettings | UnknownKey | '{0}' is not an instance key; a key is a material property an instance can override, such as BlendMode, TwoSided, OpacityMaskClipValue or PhysMaterial. |
| DreamShader.IR.Validator | CallBadLocalProduct | Node {0} calls local product {1}, which this module does not have; it has {2}. |
| DreamShader.IR.Validator | CallNoTarget | Node {0} names neither a function asset path nor a local product; a MaterialFunctionCall needs one of the two. |
| DreamShader.IR.Validator | CallSelfProduct | Node {0} calls the product it belongs to; a material function cannot call itself. |
| DreamShader.IR.Validator | DuplicateDedupeKey | Nodes %{0} and %{1} of product {2} have the same dedupe key, so the dedupe pass should have merged them into one. |
| DreamShader.IR.Validator | DuplicateOutputName | Node {0} names two outputs '{1}'; an output name selects one slot and must be unique. |
| DreamShader.IR.Validator | DuplicatePinName | Node {0} connects pin '{1}' twice; a pin takes one value. |
| DreamShader.IR.Validator | DuplicateProductName | Products {0} and {1} would both be called '{2}'; one file cannot produce two assets of the same name. |
| DreamShader.IR.Validator | DuplicateProperty | Node {0} sets property '{1}' twice. |
| DreamShader.IR.Validator | EmptyPinName | Node {0} has a named input with no pin name. |
| DreamShader.IR.Validator | EmptyPropertyName | Node {0} carries a property with no name. |
| DreamShader.IR.Validator | FunctionHasSink | Product {0} is a {1} and must have no MaterialSink node, but it has {2}. |
| DreamShader.IR.Validator | FunctionListBadIndex | Product {0} lists node %{1} in its {2}, and that node does not exist. |
| DreamShader.IR.Validator | FunctionListWrongOp | Product {0} lists node {1} in its {2}, where only {3} nodes belong. |
| DreamShader.IR.Validator | FunctionNoOutputs | Product {0} is a {1} and produces nothing; a material function needs at least one FunctionOutput. |
| DreamShader.IR.Validator | FunctionRecordsSink | Product {0} is a {1} and records sink node {2}; only a material has a sink. |
| DreamShader.IR.Validator | GraphHasCycle | Product {0} cannot be ordered: these nodes feed themselves, directly or through others -- {1}. |
| DreamShader.IR.Validator | InstanceBackend | Material instance product {0} carries backend {1}, and an instance is a plain material instance with the default backend. |
| DreamShader.IR.Validator | InstanceBesideProducts | This module holds a material instance and {0} other product(s), and a material instance is the only product of its file. |
| DreamShader.IR.Validator | InstanceDuplicateOverride | Product {0} overrides '{1}' twice, and an instance sets each parameter once. |
| DreamShader.IR.Validator | InstanceFontPage | Override '{0}' of product {1} sets font page {2}, and a font page is zero or more. |
| DreamShader.IR.Validator | InstanceHasGraph | Product {0} is a material instance, which assigns its parent's parameters and has no graph, and it carries {1} node(s), a sink or a function signature. |
| DreamShader.IR.Validator | InstanceNoParent | Material instance product {0} needs the material it is an instance of, and its parent reference is empty. |
| DreamShader.IR.Validator | InstanceNotInSchema | Override '{0}' of product {1} names a parameter, and the parent schema the product carries has no such parameter. |
| DreamShader.IR.Validator | InstanceSchemaKind | Override '{0}' of product {1} is a {2} override, and the parent schema lists that parameter as {3}. |
| DreamShader.IR.Validator | InstanceValueMask | a Float4 of four components, each 0 or 1 |
| DreamShader.IR.Validator | InstanceValueObject | an Object path, empty for None |
| DreamShader.IR.Validator | InstanceValueScalar | a Float4 of one component |
| DreamShader.IR.Validator | InstanceValueShape | Override '{0}' of product {1} is a {2} override holding a {3} value, and a {2} override holds {4}. |
| DreamShader.IR.Validator | InstanceValueStaticSwitch | a Bool |
| DreamShader.IR.Validator | InstanceValueVector | a Float4 of four components |
| DreamShader.IR.Validator | LayoutHintKind | Layout hint {0} of product {1} has kind '{2}'; a hint is spelled exactly 'Node' or 'Comment', so this one places nothing. |
| DreamShader.IR.Validator | LayoutHintNodeColor | Layout hint {0} of product {1} places a node and carries a colour, and only a comment box has one, so the colour is ignored. |
| DreamShader.IR.Validator | LayoutHintNoName | Layout hint {0} of product {1} draws a comment box with no title, so it draws nothing. |
| DreamShader.IR.Validator | LayoutHintNoVar | Layout hint {0} of product {1} places a node but names no variable, so it places nothing. |
| DreamShader.IR.Validator | ListInputs | function inputs |
| DreamShader.IR.Validator | ListOutputs | function outputs |
| DreamShader.IR.Validator | MaterialHasFunctionIO | Material product {0} declares function inputs or outputs; a material has parameters and a sink, not a signature. |
| DreamShader.IR.Validator | MaterialSinkCount | Material product {0} has {1} MaterialSink node(s); a material has exactly one. |
| DreamShader.IR.Validator | MaterialSinkIndex | Material product {0} records its sink as {1} but the MaterialSink node is %{2}. |
| DreamShader.IR.Validator | MathOperandNotCarryable | Node {0} does arithmetic on operand {1}, which is a {2}; the graph carries only float1 to float4, so a matrix, a texture or a material cannot reach a math node. |
| DreamShader.IR.Validator | MathResultNotCarryable | Node {0} produces a {1}; a math node's result must be float1 to float4. |
| DreamShader.IR.Validator | MissingProperty | Node {0} needs property '{1}' and does not have it. |
| DreamShader.IR.Validator | NodeBadRegion | Node {0} sits in region {1}, which does not exist; the graph has {2} region(s). |
| DreamShader.IR.Validator | OperandBadNode | Node {0} reads {1} from node %{2}, which does not exist; the graph has {3} nodes. |
| DreamShader.IR.Validator | OperandBadOutput | Node {0} reads output {1} of node {2}, which has {3} output(s). |
| DreamShader.IR.Validator | OperandReadsStatement | Node {0} reads {1} from node {2}, which is a statement and produces no value. |
| DreamShader.IR.Validator | OutputNamesNotParallel | Node {0} has {1} output name(s) for {2} output(s); the two lists are either parallel or the names are omitted. |
| DreamShader.IR.Validator | PartialDedupeKeys | Product {0} has dedupe keys on {1} of its {2} nodes; the key is either computed for the whole graph or for none of it. |
| DreamShader.IR.Validator | ProductNoName | Product {0} has no name; the asset name comes from the exported function or from '/// @name'. |
| DreamShader.IR.Validator | ReflectedClassMismatch | Node {0} names class '{1}' but its catalog entry is '{2}'; the two must agree. |
| DreamShader.IR.Validator | ReflectedNoCatalogIndex | Node {0} is a reflected node with catalog index {1}, which is not an entry of the builtin catalog. |
| DreamShader.IR.Validator | ReflectedUnknownPin | Node {0} connects pin '{1}', which '{2}' does not have. |
| DreamShader.IR.Validator | ReflectedUnknownProperty | Node {0} sets property '{1}', which '{2}' does not have. |
| DreamShader.IR.Validator | RegionBadParent | Region {0} of product {1} names parent {2}, which does not exist. |
| DreamShader.IR.Validator | RegionCycle | Region {0} of product {1} encloses itself; regions nest, they do not loop. |
| DreamShader.IR.Validator | SelectNotScalar | Node {0} selects on a {1}; a condition is one 0/1 component. |
| DreamShader.IR.Validator | SlotOperand | operand {0} |
| DreamShader.IR.Validator | SlotPin | pin '{0}' |
| DreamShader.IR.Validator | StatementHasOutputs | Node {0} is a statement and must have no outputs, but it declares {1}. |
| DreamShader.IR.Validator | StaticSwitchNotBool | Node {0} switches on a {1}; a StaticSwitch condition is a single static bool. |
| DreamShader.IR.Validator | StrayCatalogIndex | Node {0} carries catalog index {1}; only a reflected node has one. |
| DreamShader.IR.Validator | SwizzleMaskCharacter | Node {0} has mask '{1}'; masks are canonical lower-case x, y, z and w. |
| DreamShader.IR.Validator | SwizzleMaskLength | Node {0} has mask '{1}'; a mask is one to four of x, y, z and w. |
| DreamShader.IR.Validator | SwizzleMaskOrder | Node {0} has mask '{1}', which is not in ascending order; a ComponentMask cannot reorder channels, so a reordering swizzle is masks plus an Append. |
| DreamShader.IR.Validator | SwizzleMaskRepeat | Node {0} has mask '{1}', which names the same component twice; a ComponentMask cannot repeat a channel. |
| DreamShader.IR.Validator | SwizzleNoMask | Node {0} needs a string Mask property holding the component letters. |
| DreamShader.IR.Validator | SwizzleTooWide | Node {0} masks '{1}' out of a {2}, which has only {3} component(s). |
| DreamShader.IR.Validator | TakesNamedInputs | named inputs |
| DreamShader.IR.Validator | TakesNothing | no inputs at all |
| DreamShader.IR.Validator | TakesNothing2 | no inputs at all |
| DreamShader.IR.Validator | TakesOperands | positional operands |
| DreamShader.IR.Validator | UnexpectedNamedInputs | Node {0} carries {1} named input(s); this op takes {2}. |
| DreamShader.IR.Validator | UnexpectedOperands | Node {0} carries {1} positional operand(s); this op takes {2}. |
| DreamShader.IR.Validator | UnknownAttributeSetType | Node {0} lists '{1}' in AttributeSetTypes, which is not a material attribute. |
| DreamShader.IR.Validator | UnknownMaterialAttribute | Node {0} writes attribute '{1}', which is not in the engine's material attribute table. |
| DreamShader.IR.Validator | UnlistedFunctionInput | Product {0} has node {1} that is not in its function input list; the list is what gives an input its order. |
| DreamShader.IR.Validator | UnlistedFunctionOutput | Product {0} has node {1} that is not in its function output list; the list is what gives an output its order. |
| DreamShader.IR.Validator | UnsetOperand | Node {0} has nothing connected to {1}; every operand of this op must carry a value. |
| DreamShader.IR.Validator | ValueHasNoOutputs | Node {0} declares no outputs, so nothing can read it. |
| DreamShader.IR.Validator | WrongArityExact | Node {0} has {1} operand(s); this op takes exactly {2}. |
| DreamShader.IR.Validator | WrongArityRange | Node {0} has {1} operand(s); this op takes {2} to {3}. |
| DreamShader.IR.Validator | WrongPropertyKind | Node {0} stores property '{1}' as {2}; it must be {3}. |
| DreamShader.IRBuilder | IRBuilderAppendTooWide | This builds a {0}-component value, but a material graph carries at most four components. |
| DreamShader.IRBuilder | IRBuilderAttributeNotSet | '{0}' is read before anything wrote it; a material attribute has no value until this function assigns one. |
| DreamShader.IRBuilder | IRBuilderCallEntry | '{0}' is this file's material entry and is called by the engine, not by the shader. |
| DreamShader.IRBuilder | IRBuilderFieldOfNonMaterial | The IR builder cannot read '{0}' here: the binder typed what it is read from as a material, but that did not lower to one. |
| DreamShader.IRBuilder | IRBuilderInlineDepth | Inlining '{0}' would go {1} calls deep, past the limit of {2}; flatten the call chain or move part of it into a '/// @custom' function. |
| DreamShader.IRBuilder | IRBuilderJumpInBranch | '{0}' inside an 'if' cannot be unrolled, because both arms of the 'if' become nodes and only one of them may leave the loop; move it to the top of the loop body, or write the loop in a '/// @custom' function. |
| DreamShader.IRBuilder | IRBuilderLoopNotUnrollable | This loop runs a number of times the compiler cannot fix at {0} or fewer, and a material graph has no loops; give it a constant trip count, or move it into a '/// @custom' function. |
| DreamShader.IRBuilder | IRBuilderMaterialIntoParam | A 'material' value was passed where {0} was expected. |
| DreamShader.IRBuilder | IRBuilderMaterialIntoPin | A 'material' value reached a pin that carries {0}; only a MaterialAttributes pin accepts one. |
| DreamShader.IRBuilder | IRBuilderMatrixValue | This expression is {0} and the material graph has no matrices; compute it inside the custom body instead. |
| DreamShader.IRBuilder | IRBuilderNestedAttributeWrite | '{0}' writes into the material held in an attribute, and an attribute takes one whole value, not a write to part of it; assign that attribute a whole material, or set the attribute on the material itself. |
| DreamShader.IRBuilder | IRBuilderNoAttributeEntry | The material attribute table has no entry {0}; the module was bound against a different catalog than this build is reading. |
| DreamShader.IRBuilder | IRBuilderNoBody | '{0}' has no body to inline; give it one, mark it 'extern' with '/// @asset', or '/// @custom'. |
| DreamShader.IRBuilder | IRBuilderNoExpressionEntry | The builtin catalog has no expression entry {0}; the module was bound against a different catalog than this build is reading. |
| DreamShader.IRBuilder | IRBuilderNoInputType | A parameter of type {0} cannot be a material function input; the graph has no pin that carries one. |
| DreamShader.IRBuilder | IRBuilderNoLowering | The IR builder has no lowering for a bound expression of kind {0}. |
| DreamShader.IRBuilder | IRBuilderNoTextureObject | '{0}' is a constant texture, which is a TextureObject node, and this engine has no such material expression; declare it 'uniform'. |
| DreamShader.IRBuilder | IRBuilderOneArmedMaterialWrite | '{0}' is set in only one arm of this 'if' and has no value before it; set it in both arms, or before the 'if'. |
| DreamShader.IRBuilder | IRBuilderOneArmedWrite | '{0}' is assigned in only one arm of this 'if' and has no value before it; assign it in both arms, or give it a value before the 'if'. |
| DreamShader.IRBuilder | IRBuilderOutArgNotLValue | '{0}' is an '{1}' parameter of {2}, so the argument has to be something that can be assigned to; this expression cannot. |
| DreamShader.IRBuilder | IRBuilderPropertyNotLiteral | '{0}' is a property of {1} and needs a literal; this argument is computed at run time. |
| DreamShader.IRBuilder | IRBuilderReadNoAttributeEntry | The material attribute table has no entry {0}; the module was bound against a different catalog than this build is reading. |
| DreamShader.IRBuilder | IRBuilderRecursion | '{0}' calls itself, and an inlined function has no stack to recurse on; rewrite it as a loop with a constant trip count, or as a '/// @custom' function. |
| DreamShader.IRBuilder | IRBuilderRecursionCycle | '{0}' is already being inlined further up this call chain; an inlined function cannot call back into itself. |
| DreamShader.IRBuilder | IRBuilderSubstrateBranch | A branch over {0} values needs a static condition, and this one is decided at run time; make the condition a '/// @static' uniform bool, or mix the two values with a Substrate mixing node. |
| DreamShader.IRBuilder | IRBuilderTextureBranch | A branch can only choose between numbers, bools and static Substrate values, and these are {0} objects, which no material graph node switches; sample each texture first and branch on the samples. |
| DreamShader.IRBuilder | IRBuilderUnsetLocalRead | '{0}' is read here, but nothing gives it a value on any path that reaches this line; assign it first, or give it an initializer where it is declared. |
| DreamShader.IRBuilder | IRBuilderWholeMaterialMergeNamed | '{0}' holds a different whole material in each arm of this 'if', and DreamShader chooses between attribute values, not between whole materials; assign its attributes one at a time in both arms, or mix the two materials with UE.BlendMaterialAttributes. |
| DreamShader.IRPasses | IRPassesUnusedUniform | '{0}' is declared but nothing reads it, so it is not in the generated material. |
| DreamShader.Lang.Declarations | BadStorageCombination | '{0}' cannot be combined with the keywords before it; a declaration is 'uniform', 'static const', 'static', 'const', 'extern' or 'export', not a mix. |
| DreamShader.Lang.Declarations | DefaultOnOutParameter | Parameter '{0}' is 'out' and cannot have a default value; only inputs are optional. |
| DreamShader.Lang.Declarations | ExpectedArrayClose | ']' to close the array dimension |
| DreamShader.Lang.Declarations | ExpectedBodyOpen | '{' to open the function body |
| DreamShader.Lang.Declarations | ExpectedBodyOrSemicolon | Expected '`{' or ';' after the parameter list of '{0}', found {1}. |
| DreamShader.Lang.Declarations | ExpectedDeclarationName | a declaration name |
| DreamShader.Lang.Declarations | ExpectedFieldName | a field name |
| DreamShader.Lang.Declarations | ExpectedNextDeclarator | another name after ',' |
| DreamShader.Lang.Declarations | ExpectedParameterListClose | ')' to close the parameter list |
| DreamShader.Lang.Declarations | ExpectedParameterName | a parameter name |
| DreamShader.Lang.Declarations | ExpectedSemicolonAfterDeclarators | ';' after the declaration |
| DreamShader.Lang.Declarations | ExpectedSemicolonAfterField | ';' after the field |
| DreamShader.Lang.Declarations | ExpectedSemicolonAfterImport | ';' after the import path |
| DreamShader.Lang.Declarations | ExpectedSemicolonAfterStruct | ';' after the closing '}' of the struct |
| DreamShader.Lang.Declarations | ExpectedSemicolonAfterVariable | ';' after the declaration |
| DreamShader.Lang.Declarations | ExpectedStructName | a name after 'struct' |
| DreamShader.Lang.Declarations | ExpectedStructOpen | '{' after the struct name |
| DreamShader.Lang.Declarations | ExpectedTypeName | Expected a type name, found {0}. |
| DreamShader.Lang.Declarations | ExternWithBody | '{0}' is 'extern' and binds to an existing asset, so it cannot have a body; write a prototype ending in ';'. |
| DreamShader.Lang.Declarations | FunctionWithoutBody | '{0}' has no body. Only an 'extern' prototype may end in ';'; a function you define needs '`{...`}'. |
| DreamShader.Lang.Declarations | ImportNeedsPath | Expected a quoted path after 'import', found {0}. |
| DreamShader.Lang.Declarations | LegacyDeclarationInTwoPointZeroFile | Expected a 2.0 declaration, found the 1.x declaration '{0}'; 1.x declarations belong in a .dsh header or in a .dsm or .dsf file. |
| DreamShader.Lang.Declarations | LinkageOnVariable | '{0}' is a variable; 'extern' and 'export' apply to functions only. |
| DreamShader.Lang.Declarations | MalformedDocDirective | A '@' in a '///' line must be followed by a directive name; the text is kept as description. |
| DreamShader.Lang.Declarations | MalformedInclude | '#include' needs a quoted path: #include "/Game/Shared/Common.dsh". |
| DreamShader.Lang.Declarations | MalformedPragma | Malformed '#pragma {0}': {1} |
| DreamShader.Lang.Declarations | OrphanDocBlock | This '///' block is not followed by a declaration and is ignored. |
| DreamShader.Lang.Declarations | OrphanDocBlockInStruct | This '///' block is not followed by a field and is ignored. |
| DreamShader.Lang.Declarations | PragmaExpectedKey | expected a key or a value. |
| DreamShader.Lang.Declarations | PragmaExpectedOpen | expected '(' after the pragma name. |
| DreamShader.Lang.Declarations | PragmaExpectedSeparator | expected ',' or ')'. |
| DreamShader.Lang.Declarations | PragmaExpectedValue | expected a value after '{0} ='. |
| DreamShader.Lang.Declarations | PragmaPositionalInMaterial | '{0}' needs a value: write '{0} = ...'. |
| DreamShader.Lang.Declarations | PragmaQuotedKey | a key cannot be a quoted string. |
| DreamShader.Lang.Declarations | PragmaTrailingText | unexpected text after ')'. |
| DreamShader.Lang.Declarations | PragmaWithoutNameWithInstance | '#pragma' needs a name: material, instance, layout, region or endregion. |
| DreamShader.Lang.Declarations | StorageOnFunction | '{0}' is a function; 'uniform', 'static' and 'const' apply to variables only. |
| DreamShader.Lang.Declarations | StrayDirective | Preprocessor directive '#{0}' reached the parser; only '#pragma' and '#include' belong here, and '#if' / '#define' lines must be resolved by the preprocessor first. |
| DreamShader.Lang.Declarations | UnexpectedAtFileScope | Unexpected {0} at file scope; expected a declaration, '#pragma', '#include' or 'import'. |
| DreamShader.Lang.Declarations | WhileParsingRawBody | a function body |
| DreamShader.Lang.Declarations | WhileParsingStruct | struct '{0}' |
| DreamShader.Lang.Expressions | ArgumentsRightParen | ')' to close an argument list |
| DreamShader.Lang.Expressions | CastRightParen | ')' to close a cast |
| DreamShader.Lang.Expressions | ExpectedColonInConditional | Expected ':' to complete the conditional operator, found {0}. |
| DreamShader.Lang.Expressions | ExpectedExpression | Expected an expression, found {0}. |
| DreamShader.Lang.Expressions | ExpectedInitializerList | Expected an initializer list, found {0}. |
| DreamShader.Lang.Expressions | ExpectedMemberName | Expected a member or swizzle name after '.', found {0}. |
| DreamShader.Lang.Expressions | IndexRightBracket | ']' to close an index |
| DreamShader.Lang.Expressions | InitializerListRightBrace | '}' to close an initializer list |
| DreamShader.Lang.Expressions | ParenRightParen | ')' to close a parenthesized expression |
| DreamShader.Lang.Expressions | WhileParsingArgumentList | an argument list |
| DreamShader.Lang.Expressions | WhileParsingExpression | an expression |
| DreamShader.Lang.Expressions | WhileParsingInitializerList | an initializer list |
| DreamShader.Lang.InstanceSource | NoInstancePragma | Expected a '#pragma instance(...)' line in this .dsi file, found none; the file was left unchanged. |
| DreamShader.Lang.InstanceSource | NoUniformForDefault | Expected a uniform whose parameter name is '{0}' declared in this file, found none; no default was written. |
| DreamShader.Lang.InstanceSource | OverlappingEdits | Expected every change to this file to touch its own stretch of text, found an edit at line {0} that overlaps another or runs past the end; the file was left unchanged. |
| DreamShader.Lang.InstanceSource | SharedStatement | Expected '{0}' to be declared alone to rewrite its value, found it in a declaration shared with other names; split the declaration first. |
| DreamShader.Lang.InstanceSource | UniformKindMismatch | Expected '{0}' to be declared as a {1} parameter to take this default, found a declaration of another kind; no default was written. |
| DreamShader.Lang.LegacyExpressions | CustomOutputType | Expected 'OutputType' of a Custom expression to be float1 to float4 or MaterialAttributes, found '{0}'. |
| DreamShader.Lang.LegacyExpressions | IgnoredNamedArgument | '{0}' is not an argument '{1}' reads; 1.x ignored it, so it is dropped. |
| DreamShader.Lang.LegacyExpressions | IgnoredPositionalArgument | '{0}' takes no positional argument here; 1.x ignored it, so it is dropped. |
| DreamShader.Lang.LegacyExpressions | OutputIndexNotInteger | Expected 'OutputIndex' to be a whole number of zero or more, found '{0}'. |
| DreamShader.Lang.LegacyExpressions | OutputNotName | Expected 'Output' to name an output with a quoted name or an identifier, found '{0}'. |
| DreamShader.Lang.LegacyExpressions | PinCallPositional | Expected every argument of the parameter call '{0}' to name an input pin, found a positional argument. |
| DreamShader.Lang.LegacyExpressions | QualifierNeedsName | Expected a name after '::', found {0}. |
| DreamShader.Lang.LegacyExpressions | SampleTexture2DShape | Expected 'SampleTexture2D' to take exactly two positional arguments, a texture and coordinates, found {0} argument(s). |
| DreamShader.Lang.LegacyExpressions | SceneTextureShape | Expected 'UE.SceneTexture' to take exactly one argument, 'Id = ...', found {0} argument(s). |
| DreamShader.Lang.LegacyExpressions | SelectorBoth | Expected either 'Output' or 'OutputIndex' on this call, found both. |
| DreamShader.Lang.LegacyExpressions | SelectorOnConstructor | Expected an output selector only on a call that has outputs, found one on the constructor '{0}'. |
| DreamShader.Lang.LegacyExpressions | SelectorOnParameterCall | Expected the parameter call '{0}' to take only its inputs, found an output selector. |
| DreamShader.Lang.LegacyExpressions | StaticSwitchInputs | Expected the static switch '{0}' to be called with a 'True = ...' and a 'False = ...' input, found at most one of them. |
| DreamShader.Lang.LegacyExpressions | StaticSwitchSugarDefault | Expected 'Default' of 'UE.StaticSwitchParameter' to be true or false, found '{0}'. |
| DreamShader.Lang.LegacyExpressions | StaticSwitchSugarInputs | Expected the static switch '{0}' to be called with a 'True = ...' and a 'False = ...' input, found at most one of them. |
| DreamShader.Lang.LegacyExpressions | StaticSwitchSugarName | Expected 'UE.StaticSwitchParameter' to name its parameter with 'Name = "..."', found no name. |
| DreamShader.Lang.LegacyExpressions | StaticSwitchSugarSort | Expected 'SortPriority' of 'UE.StaticSwitchParameter' to be a whole number, found '{0}'. |
| DreamShader.Lang.LegacyParser | AssetBlockInHeader | Expected only Function, GraphFunction, Namespace and VirtualFunction blocks in a '.dsh' header, found the asset block '{0}', which belongs in a .dsm or .dsf file. |
| DreamShader.Lang.LegacyParser | AssetBlockInLegacyHeader | Expected only Function, GraphFunction, Namespace and VirtualFunction blocks in a '.dsh' header, found the asset block '{0}', which belongs in a .dsm or .dsf file. |
| DreamShader.Lang.LegacyParser | AttributeEquals | Expected '=' after the attribute '{0}', found {1}. |
| DreamShader.Lang.LegacyParser | AttributeName | Expected an attribute name such as 'Name', found {0}. |
| DreamShader.Lang.LegacyParser | AttributeRepeated | The attribute '{0}' is written twice; the later value wins, as it did in 1.x. |
| DreamShader.Lang.LegacyParser | AttributeSeparator | Expected ',' or ')' in the attribute list, found {0}. |
| DreamShader.Lang.LegacyParser | AttributesMissing | Expected '(' with the block's attributes, such as '(Name = "M_Example")', found {0}. |
| DreamShader.Lang.LegacyParser | AttributeValue | Expected a value after '{0} =', found {1}. |
| DreamShader.Lang.LegacyParser | BareReturn | Expected a value after 'return' in '{0}', which returns '{1}', found a bare 'return;'. |
| DreamShader.Lang.LegacyParser | BlockOpen | Expected '`{' to open the '{0}' block, found {1}. |
| DreamShader.Lang.LegacyParser | BlockWithoutName | Expected a 'Name = "..."' attribute on '{0}', found none. |
| DreamShader.Lang.LegacyParser | CodeSection | Expected 'Graph' as the body section of '{0}', found 'Code', which 1.x accepted only inside a Function. |
| DreamShader.Lang.LegacyParser | ExposeNotBool | Expected 'true' or 'false' for 'ExposeToLibrary', found '{0}'; 1.x ignored the setting and so does this front end. |
| DreamShader.Lang.LegacyParser | FunctionBodyOpen | Expected '`{' to open the body of '{0}', found {1}. |
| DreamShader.Lang.LegacyParser | FunctionName | Expected a function name after '{0}', found {1}. |
| DreamShader.Lang.LegacyParser | FunctionNameOrParen | Expected '(' or a function name after '{0}', found {1}. |
| DreamShader.Lang.LegacyParser | FunctionParamsOpen | Expected '(' to open the parameter list of '{0}', found {1}. |
| DreamShader.Lang.LegacyParser | FunctionSettingIgnored | '{0}' is not a setting of '{1}'; 1.x ignored it and so does this front end. |
| DreamShader.Lang.LegacyParser | GraphModifier | Expected a return type or a name after 'GraphFunction', found the modifier '{0}', which only a Function takes. |
| DreamShader.Lang.LegacyParser | GraphRepeated | The section '{0}' is written twice; the later one wins, as it did in 1.x. |
| DreamShader.Lang.LegacyParser | HoistUnterminated | Expected a ')' to close the 'UE.' call in the body of '{0}', found the end of the body. |
| DreamShader.Lang.LegacyParser | ImportCase | '{0}' is read as 'import'; write it in lower case. |
| DreamShader.Lang.LegacyParser | ImportNotHeader | Expected an import of a '.dsh' header, found '{0}'; a material or function file is compiled on its own, not included. |
| DreamShader.Lang.LegacyParser | ImportPath | Expected a double-quoted path after 'import', found {0}. |
| DreamShader.Lang.LegacyParser | ImportQualified | Expected an import path inside this file's own source root, found the root-qualified '{0}', which the 2.0 include resolver does not read. |
| DreamShader.Lang.LegacyParser | InlineModifier | 'Inline' is the old spelling of 'SelfContained'; the function becomes '@custom selfcontained'. |
| DreamShader.Lang.LegacyParser | LayerInputRenamed | The layer input '{0}' becomes the 'inout material' parameter named after the output '{1}', so the input pin changes its name. |
| DreamShader.Lang.LegacyParser | LayerWithoutMaterial | Expected a MaterialAttributes output on '{0}', found none. |
| DreamShader.Lang.LegacyParser | LayoutRepeated | The section '{0}' is written twice; the later one wins, as it did in 1.x. |
| DreamShader.Lang.LegacyParser | NameAfterModifier | Expected a function name after '{0}', found '('. |
| DreamShader.Lang.LegacyParser | NamespaceMember | Expected only Function and GraphFunction blocks inside the namespace '{0}', found {1}. |
| DreamShader.Lang.LegacyParser | NamespaceNoName | Expected a 'Name = "..."' attribute with a name on 'Namespace', found none. |
| DreamShader.Lang.LegacyParser | NamespaceNotIdentifier | Expected the namespace name to be an identifier, found '{0}'. |
| DreamShader.Lang.LegacyParser | NamespaceOpen | Expected '`{' to open the namespace '{0}', found {1}. |
| DreamShader.Lang.LegacyParser | NoLegacyBlock | Expected a top-level Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend, Function, GraphFunction, Namespace or VirtualFunction block in this 1.x file, found none. |
| DreamShader.Lang.LegacyParser | NoResult | Expected '{0}' to return a value or to have at least one 'out' parameter, found neither. |
| DreamShader.Lang.LegacyParser | OldLayerWord | '{0}' is the old spelling of '{1}'; it still reads the same. |
| DreamShader.Lang.LegacyParser | ParamQualifier | Expected 'in' or 'out' before the parameter '{0}' of '{1}', found '{2}'; a 1.x function has no 'inout'. |
| DreamShader.Lang.LegacyParser | ParamWords | Expected '[in\|out] Type Name' in the parameter list of '{0}', found '{1}'. |
| DreamShader.Lang.LegacyParser | ReturnAndOut | Expected either a return type or 'out' parameters on '{0}', found both. |
| DreamShader.Lang.LegacyParser | ReturnParamName | Expected a parameter name other than '__return', which 1.x reserved, in '{0}'. |
| DreamShader.Lang.LegacyParser | SecondShader | Expected one Shader block in a file, found a second one. |
| DreamShader.Lang.LegacyParser | SectionName | Expected a section name such as 'Properties' or 'Graph' in '{0}', found {1}. |
| DreamShader.Lang.LegacyParser | SectionOpen | Expected '`{' after the section name '{0}', found {1}. |
| DreamShader.Lang.LegacyParser | ShaderWithoutGraph | Expected a Graph section in the Shader '{0}', found none. |
| DreamShader.Lang.LegacyParser | ShaderWithoutOutputs | The Shader '{0}' has no Outputs section, so nothing its Graph computes reaches the material. |
| DreamShader.Lang.LegacyParser | SubstrateNotHoisted | A 'Substrate.' call in the body of '{0}' is not lifted into a node, because no custom node input carries a Substrate value; it reaches the shader compiler as text. |
| DreamShader.Lang.LegacyParser | TwoPointZeroInLegacyFile | Expected a 1.x block in a '.{0}' file, found {1}, which is 2.0 syntax; 2.0 declarations belong in a .dss file or a .dsh header. |
| DreamShader.Lang.LegacyParser | UnknownFunctionSection | Expected a function section (Properties, Inputs, Outputs, Settings, Graph or Layout), found '{0}'. |
| DreamShader.Lang.LegacyParser | UnknownShaderSection | Expected a Shader section (Properties, Settings, Outputs, Graph or Layout), found '{0}'. |
| DreamShader.Lang.LegacyParser | UnknownTopLevel | Expected a top-level Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend, Function, GraphFunction, Namespace, VirtualFunction or import, found {0}. |
| DreamShader.Lang.LegacyParser | UserExposedCaption | 'UserExposedCaption' has no 2.0 spelling and is not applied; its value is kept for migration. |
| DreamShader.Lang.LegacyParser | VirtualBody | Expected no body in the VirtualFunction '{0}', which declares an existing asset, found the section '{1}'. |
| DreamShader.Lang.LegacyParser | VirtualNoAsset | Expected an 'Asset = Path(...)' option on the VirtualFunction '{0}', found none. |
| DreamShader.Lang.LegacyParser | VirtualNoName | Expected a 'Name = "..."' attribute with a name on 'VirtualFunction', found none. |
| DreamShader.Lang.LegacyParser | VirtualNoOutput | Expected at least one output on the VirtualFunction '{0}', found none. |
| DreamShader.Lang.LegacyParser | VirtualNotIdentifier | Expected the VirtualFunction name to be an identifier, found '{0}'. |
| DreamShader.Lang.LegacyParser | VirtualOpen | Expected '`{' to open the VirtualFunction '{0}', found {1}. |
| DreamShader.Lang.LegacyParser | VirtualSectionName | Expected a section name such as 'Inputs' in the VirtualFunction '{0}', found {1}. |
| DreamShader.Lang.LegacyParser | VirtualSectionOpen | Expected '`{' after the section name '{0}', found {1}. |
| DreamShader.Lang.LegacyParser | VirtualUnknownSection | Expected a VirtualFunction section (Inputs, Outputs or Options), found '{0}'. |
| DreamShader.Lang.LegacyParser | WhileBlock | the '{0}' block |
| DreamShader.Lang.LegacyParser | WhileNamespace | the namespace '{0}' |
| DreamShader.Lang.LegacyParser | WhileParams | the parameter list of '{0}' |
| DreamShader.Lang.LegacyParser | WhileVirtual | the VirtualFunction '{0}' |
| DreamShader.Lang.LegacySections | BindingSourceMissing | Expected a source after 'Base.{0} =', found none. |
| DreamShader.Lang.LegacySections | BuiltinUnclosed | Expected ')' to close the arguments of 'UE.{0}', found {1}. |
| DreamShader.Lang.LegacySections | BuiltinWithDefault | Expected the arguments of 'UE.{0}' inside its parentheses, found a default value after '{1}'. |
| DreamShader.Lang.LegacySections | ConstWithoutType | Expected a property type after 'const', found {0}. |
| DreamShader.Lang.LegacySections | DefaultMissing | Expected a default value after '{0} =', found {1}. |
| DreamShader.Lang.LegacySections | DefaultOnOutput | A default on the output '{0}' means nothing; 1.x ignored it, so it is dropped. |
| DreamShader.Lang.LegacySections | GroupEmptyName | Expected a name inside 'Group("...")', found an empty string. |
| DreamShader.Lang.LegacySections | LayoutArgumentEquals | Expected '=' after '{0}' inside '{1}(...)', found {2}. |
| DreamShader.Lang.LegacySections | LayoutArgumentRepeated | Expected the Layout argument '{0}' once, found it again. |
| DreamShader.Lang.LegacySections | LayoutArgumentShape | Expected 'Key = Value' inside '{0}(...)', found {1}. |
| DreamShader.Lang.LegacySections | LayoutArgumentValue | Expected a value after '{0} =' inside '{1}(...)', found {2}. |
| DreamShader.Lang.LegacySections | LayoutClose | Expected ')' to close '{0}(...)', found {1}. |
| DreamShader.Lang.LegacySections | LayoutColor | Expected 'Color' to be a vector literal such as '(0.1, 0.16, 0.22, 0.35)', found '{0}'. |
| DreamShader.Lang.LegacySections | LayoutMissingArgument | Expected the argument '{0}' in '{1}(...)', found none. |
| DreamShader.Lang.LegacySections | LayoutNotInteger | Expected the Layout argument '{0}' to be a whole number, found '{1}'. |
| DreamShader.Lang.LegacySections | LayoutOpen | '{' to open the Layout section |
| DreamShader.Lang.LegacySections | LayoutShape | Expected 'Node(...)' or 'Comment(...)' in the Layout section, found {0}. |
| DreamShader.Lang.LegacySections | LayoutUnknown | Expected 'Node' or 'Comment' in the Layout section, found '{0}'. |
| DreamShader.Lang.LegacySections | MetadataExpectedEquals | Expected '=' after the metadata key '{0}', found {1}. |
| DreamShader.Lang.LegacySections | MetadataExpectedKey | Expected a metadata key such as 'Group', found {0}. |
| DreamShader.Lang.LegacySections | MetadataExpectedValue | Expected a value after '{0} =', found none. |
| DreamShader.Lang.LegacySections | MetadataRepeated | Expected the metadata key '{0}' once, found it again. |
| DreamShader.Lang.LegacySections | MetadataUnclosed | Expected ']' to close the metadata block, found {0}. |
| DreamShader.Lang.LegacySections | OptOnOutput | 'opt' on the output '{0}' means nothing; 1.x ignored it and so does this front end. |
| DreamShader.Lang.LegacySections | OutputDeclarationEnd | Expected ';' after the output declaration '{0}', found {1}. |
| DreamShader.Lang.LegacySections | OutputInitializerMissing | Expected an initializer after '{0} =', found none. |
| DreamShader.Lang.LegacySections | OutputsOpen | '{' to open the Outputs section |
| DreamShader.Lang.LegacySections | OutputsStatementShape | Expected an output declaration, 'Base.<Attribute> = <source>;' or 'Expression(...).Pin[<index>] = <source>;', found {0}. |
| DreamShader.Lang.LegacySections | ParamDefaultMissing | Expected a default value after '{0} =', found {1}. |
| DreamShader.Lang.LegacySections | ParamEnd | Expected ';' after the parameter '{0}', found {1}. |
| DreamShader.Lang.LegacySections | ParamShape | Expected a parameter type and name such as 'float Amount', found {0}. |
| DreamShader.Lang.LegacySections | ParamsOpen | '{' to open the parameter section |
| DreamShader.Lang.LegacySections | ParamSortNotInteger | Expected 'SortPriority' to be a whole number, found '{0}'. |
| DreamShader.Lang.LegacySections | PinBindingShape | Expected 'Pin[<index>] = <source>' for an Expression(...) output target, found {0}. |
| DreamShader.Lang.LegacySections | PinBoundTwice | Expected each pin of Expression(Class = "{0}") to be bound once, found Pin[{1}] bound again. |
| DreamShader.Lang.LegacySections | PinSourceMissing | Expected a source after 'Pin[{0}] =', found none. |
| DreamShader.Lang.LegacySections | PropertiesOpen | '{' to open the Properties section |
| DreamShader.Lang.LegacySections | PropertyExpectedEnd | Expected ';' after the property '{0}', found {1}. |
| DreamShader.Lang.LegacySections | PropertyExpectedName | Expected a property name after the type '{0}', found {1}. |
| DreamShader.Lang.LegacySections | PropertyExpectedType | Expected a property type and a name, found {0}. |
| DreamShader.Lang.LegacySections | PropertyNoForm | Expected a property type with a 2.0 spelling, found '{0}{1}', which has none; move this material to a .dss file and write the node with UE.Expression. |
| DreamShader.Lang.LegacySections | PropertyUnknownType | Expected a property type such as 'float', 'float4', 'Texture2D' or 'ScalarParameter', found '{0}'. |
| DreamShader.Lang.LegacySections | ScalarDefault | Expected a number as the default of '{0}', found '{1}'. |
| DreamShader.Lang.LegacySections | SettingExpectedEquals | Expected '=' after the setting '{0}', found {1}. |
| DreamShader.Lang.LegacySections | SettingExpectedKey | Expected a setting name such as 'BlendMode', found {0}. |
| DreamShader.Lang.LegacySections | SettingExpectedValue | Expected a value after '{0} =', found {1}. |
| DreamShader.Lang.LegacySections | SettingRepeated | The setting '{0}' is written twice; the later value wins, as it did in 1.x. |
| DreamShader.Lang.LegacySections | SettingsOpen | '{' to open the Settings section |
| DreamShader.Lang.LegacySections | SliderMalformed | Expected 'Slider(min, max)' with two numbers, found '{0}'. |
| DreamShader.Lang.LegacySections | SliderRepeated | Expected the slider range once, found 'Slider(...)' together with another slider bound. |
| DreamShader.Lang.LegacySections | SortNotInteger | Expected 'SortPriority' to be a whole number, found '{0}'. |
| DreamShader.Lang.LegacySections | SwitchDefault | Expected 'true' or 'false' as the default of the static switch '{0}', found '{1}'. |
| DreamShader.Lang.LegacySections | TargetArgumentEquals | Expected '=' after '{0}' inside the output target Expression(...), found {1}. |
| DreamShader.Lang.LegacySections | TargetArgumentRepeated | Expected the argument '{0}' once in the output target Expression(...), found it again. |
| DreamShader.Lang.LegacySections | TargetArgumentShape | Expected 'Key = Value' inside the output target Expression(...), found {0}. |
| DreamShader.Lang.LegacySections | TargetArgumentValue | Expected a value after '{0} =' inside the output target Expression(...), found {1}. |
| DreamShader.Lang.LegacySections | TargetBlockEmpty | Expected at least one 'Pin[<index>] = <source>;' in the Expression(...) block, found none. |
| DreamShader.Lang.LegacySections | TargetClose | Expected ')' to close the output target Expression(...), found {0}. |
| DreamShader.Lang.LegacySections | TargetNeedsPin | Expected '.Pin[<index>] = <source>' or a block of pin bindings after the output target Expression(...), found {0}. |
| DreamShader.Lang.LegacySections | TargetWithoutClass | Expected 'Class = "..."' in the output target Expression(...), found no class. |
| DreamShader.Lang.LegacySections | VectorDefault | Expected a vector literal such as 'float4(1, 0, 0, 1)' as the default of '{0}', found '{1}'. |
| DreamShader.Lang.LegacySections | WhileGroup | a Group block |
| DreamShader.Lang.LegacySections | WhileLayout | a Layout section |
| DreamShader.Lang.LegacySections | WhileOutputs | an Outputs section |
| DreamShader.Lang.LegacySections | WhileParams | a parameter section |
| DreamShader.Lang.LegacySections | WhileProperties | a Properties section |
| DreamShader.Lang.LegacySections | WhilePropertiesEnd | a Properties section |
| DreamShader.Lang.LegacySections | WhileSettings | a Settings section |
| DreamShader.Lang.LegacySections | WhileTargetBlock | an Expression(...) block in Outputs |
| DreamShader.Lang.LegacyStatements | ArrayLocal | Expected a single value in a 1.x Graph declaration, found the array declarator '{0}'; move this code to a .dss file. |
| DreamShader.Lang.LegacyStatements | BadAssignmentTarget | Expected a variable or 'variable.member' on the left of a 1.x Graph assignment, found '{0}'. |
| DreamShader.Lang.LegacyStatements | CastInLegacy | Expected a constructor such as 'float3(x)' in a 1.x Graph expression, found the cast '({0})', which 1.x never had; move this code to a .dss file. |
| DreamShader.Lang.LegacyStatements | CompoundAssignment | Expected '=' in a 1.x Graph assignment, found '{0}', which 1.x read as the declaration of a variable named '{1}'; write 'x = x + y' or move this code to a .dss file. |
| DreamShader.Lang.LegacyStatements | ConditionTruncated | Expected at most one comparison in a 1.x Graph 'if' condition, found '{0}' as well, where 1.x silently dropped the rest of the condition; move this code to a .dss file. |
| DreamShader.Lang.LegacyStatements | ElseWithoutBraces | Expected braces around the body of a 1.x Graph 'else', found a single statement. |
| DreamShader.Lang.LegacyStatements | EndRegionWithoutRegion | Expected a '#Region' before this '#EndRegion', found none open. |
| DreamShader.Lang.LegacyStatements | HexLiteral | Expected a decimal number in a 1.x Graph expression, found the hexadecimal literal '{0}', which 1.x read as 0 followed by a name; move this code to a .dss file. |
| DreamShader.Lang.LegacyStatements | IfWithoutBraces | Expected braces around the body of a 1.x Graph 'if', found a single statement. |
| DreamShader.Lang.LegacyStatements | IncrementDecrement | Expected no '++' or '--' in a 1.x Graph expression, found '{0}'; move this code to a .dss file. |
| DreamShader.Lang.LegacyStatements | IndexTruncated | Expected no '[ ]' in a 1.x Graph expression, found '{0}', where 1.x silently dropped the index and everything after it; use a swizzle such as '.r' or move this code to a .dss file. |
| DreamShader.Lang.LegacyStatements | InitializerList | Expected an expression as a 1.x Graph initializer, found an initializer list; use a constructor such as 'float3(a, b, c)'. |
| DreamShader.Lang.LegacyStatements | NestedAssignment | Expected an assignment only as a whole 1.x Graph statement, found one inside an expression; move this code to a .dss file. |
| DreamShader.Lang.LegacyStatements | NestedBlock | Expected no bare block in a 1.x Graph body, found one; 1.x has blocks only after 'if' and 'else'. |
| DreamShader.Lang.LegacyStatements | NoZeroForType | Expected an initializer on the 1.x Graph variable '{0}' of type '{1}', found none, and 1.x had no zero value for that type. |
| DreamShader.Lang.LegacyStatements | OperatorTruncated | Expected only '+', '-', '*', '/', calls and members in a 1.x Graph expression, found '{0}', where 1.x silently dropped the rest of the expression; move this code to a .dss file. |
| DreamShader.Lang.LegacyStatements | OtherDirectiveInGraph | Expected '#Region' or '#EndRegion' as the only '#' line in a 1.x Graph body, found '#{0}'. |
| DreamShader.Lang.LegacyStatements | RegionNotClosed | Expected '#EndRegion' to close the region '{0}' before the end of the Graph body, found none. |
| DreamShader.Lang.LegacyStatements | RegionWithoutName | Expected a name after '#Region', found none. |
| DreamShader.Lang.LegacyStatements | StatementNotInLegacy | Expected a declaration, an assignment, a call or 'if' in a 1.x Graph body, found '{0}', which 1.x did not have; move this code to a .dss file. |
| DreamShader.Lang.LegacyStatements | StorageOnLocal | Expected no storage keyword on a 1.x Graph variable, found '{0}'; move this code to a .dss file. |
| DreamShader.Lang.LegacyStatements | UnusedExpression | Expected a call or an assignment as a 1.x Graph statement, found an expression whose value is never used. |
| DreamShader.Lang.Lexer | HashNotAtLineStart | A '#' directive must be the first thing on its line. |
| DreamShader.Lang.Lexer | MalformedNumber | Malformed number literal '{0}'. |
| DreamShader.Lang.Lexer | UnknownCharacter | Unexpected character '{0}' in source. |
| DreamShader.Lang.Lexer | UnknownStringEscape | Unknown escape sequence '\{0}' in a string literal. |
| DreamShader.Lang.Lexer | UnterminatedBlockComment | Unterminated block comment; expected a closing '*/'. |
| DreamShader.Lang.Lexer | UnterminatedStringLiteral | Unterminated string literal; expected a closing '"'. |
| DreamShader.Lang.Parser | DescribeDirective | directive '#{0}' |
| DreamShader.Lang.Parser | DescribeDocComment | a '///' comment |
| DreamShader.Lang.Parser | DescribeEndOfFile | end of file |
| DreamShader.Lang.Parser | DescribeIdentifier | identifier '{0}' |
| DreamShader.Lang.Parser | DescribeKeyword | keyword '{0}' |
| DreamShader.Lang.Parser | DescribeKind | '{0}' |
| DreamShader.Lang.Parser | DescribeNumber | number '{0}' |
| DreamShader.Lang.Parser | DescribeSpelling | '{0}' |
| DreamShader.Lang.Parser | DescribeString | string '{0}' |
| DreamShader.Lang.Parser | DescribeUnknown | '{0}' |
| DreamShader.Lang.Parser | ExpectedFound | Expected {What}, found {Token}. |
| DreamShader.Lang.Parser | ExpectedFoundIdentifier | Expected {What}, found {Token}. |
| DreamShader.Lang.Parser | ExportInHeader | A '.dsh' header cannot export '{0}'; only a '.dss' file produces assets. |
| DreamShader.Lang.Parser | TrailingTokenAfterExpression | Expected the end of the expression, found {0}. |
| DreamShader.Lang.Parser | UnexpectedEndOfFile | Unexpected end of file while parsing {0}. |
| DreamShader.Lang.Parser | WhileParsingABlock | a block |
| DreamShader.Lang.Statements | BlockLeftBrace | '{' to open a block |
| DreamShader.Lang.Statements | BreakSemicolon | ';' after 'break' |
| DreamShader.Lang.Statements | ContinueSemicolon | ';' after 'continue' |
| DreamShader.Lang.Statements | DirectiveInsideBody | Unsupported statement: the preprocessor line '#{0}' cannot appear inside a function body; mark the function /// @custom to hand its body to the shader compiler. |
| DreamShader.Lang.Statements | DiscardSemicolon | ';' after 'discard' |
| DreamShader.Lang.Statements | DoWhileLeftParen | '(' after 'while' |
| DreamShader.Lang.Statements | DoWhileRightParen | ')' to close the 'while' condition |
| DreamShader.Lang.Statements | DoWhileSemicolon | ';' after a 'do ... while' statement |
| DreamShader.Lang.Statements | ExpectedVariableName | a variable name |
| DreamShader.Lang.Statements | ExpectedWhileAfterDo | Expected 'while' after the body of a 'do' statement, found {0}. |
| DreamShader.Lang.Statements | ExpressionSemicolon | ';' after an expression statement |
| DreamShader.Lang.Statements | ForConditionSemicolon | ';' after the 'for' condition |
| DreamShader.Lang.Statements | ForInitSemicolon | ';' after the 'for' initializer |
| DreamShader.Lang.Statements | ForLeftParen | '(' after 'for' |
| DreamShader.Lang.Statements | ForRightParen | ')' to close the 'for' header |
| DreamShader.Lang.Statements | IfLeftParen | '(' after 'if' |
| DreamShader.Lang.Statements | IfRightParen | ')' to close the 'if' condition |
| DreamShader.Lang.Statements | ReturnSemicolon | ';' after a 'return' statement |
| DreamShader.Lang.Statements | SwitchNotSupported | Unsupported statement '{0}': DreamShaderLang 2.0 has no switch statement, write if / else if instead. |
| DreamShader.Lang.Statements | VarDeclSemicolon | ';' after a variable declaration |
| DreamShader.Lang.Statements | WhileLeftParen | '(' after 'while' |
| DreamShader.Lang.Statements | WhileParsingBlock | a block |
| DreamShader.Lang.Statements | WhileParsingStatement | a statement |
| DreamShader.Lang.Statements | WhileRightParen | ')' to close the 'while' condition |
| DreamShader.LegacyTextureDefaults | TextureReferenceClassNotATexture | Asset reference is written as '{0}', which is not a texture class; a texture default requires {1}. |
| DreamShader.LegacyTextureDefaults | TextureReferenceClassWrongDimension | Asset reference is written as '{0}', but this property is declared as {1}. |
| DreamShader.Migrate | AssetMoves | '{0}' builds '{1}', and its migrated text would build '{2}': a new asset, with the old one left behind. Give the declaration a '/// @name {1}'. |
| DreamShader.Migrate | CommentsLost | {0} comment(s) of '{1}' would not be in the migrated file ({2}), so nothing was written; this is a fault of the migration, not of the source. |
| DreamShader.Migrate | ConditionalSource | '{0}' uses '#if' conditional compilation, and only the branch taken with today's defines would reach the migrated file; migrate it by hand, or remove the conditionals first. |
| DreamShader.Migrate | GraphDiffers | The migrated text of '{0}' does not build the graph the 1.x file builds: {1} |
| DreamShader.Migrate | MigratedTextFails | The migrated text of '{0}' does not build as 2.0 source ({1}), so nothing was written; the text is in '{2}'. |
| DreamShader.Migrate | NothingToMigrate | '{0}' has no 1.x declaration left; there is nothing to migrate. |
| DreamShader.Migrate | NotLegacySource | '{0}' is not a 1.x source; migrate takes '.dsm', '.dsf' and '.dsh' files. |
| DreamShader.Migrate | OutputExists | '{0}' already exists and is not written over; move it away, or migrate into another folder with -Out. |
| DreamShader.Migrate | OutputUnmovable | '{0}' could not be written. |
| DreamShader.Migrate | OutputUnwritable | '{0}' could not be written. |
| DreamShader.Migrate | PreprocessFailed | '{0}' fails conditional compilation ({1}: {2}), and a source with '#if' lines is not migrated in any case. |
| DreamShader.Migrate | SourceUndeletable | '{0}' could not be deleted, so '{1}' was not written: the two would declare the same assets. |
| DreamShader.Migrate | SourceUnmovable | '{0}' could not be moved to '{1}', so '{2}' was not written: the two would declare the same assets. |
| DreamShader.Migrate | SourceUnreadable | '{0}' could not be read. |
| DreamShader.Navigation | OpenCallSiteLabel | Open Call Site |
| DreamShader.Navigation | OpenCallSiteTooltip | Open {File} at line {Line}, column {Column} -- the call that inlined the helper this node came from. |
| DreamShader.Navigation | OpenSourceLineLabel | Open Source Line |
| DreamShader.Navigation | OpenSourceLineTooltip | Open {File} at line {Line}, column {Column} -- the DreamShader source this node was generated from. |
| DreamShader.Navigation | RevealNodeBadRequest | reveal-node needs a non-empty 'file' and a 'line' of 1 or more; got file '{File}' and line {Line}. |
| DreamShader.Navigation | RevealNodeEditorRefused | The material editor would not open for '{Asset}', so the node could not be revealed. |
| DreamShader.Navigation | RevealNodeExpressionMissing | The expression {Guid} recorded for line {Line} is not in '{Asset}' any more, so the node could not be selected; the asset changed since it was generated -- rebuild it from its source. |
| DreamShader.Navigation | RevealNodeNoAsset | No asset loaded in this editor was generated from '{File}', so there is no graph to reveal a node in; compile the file first, or open one of its assets once so the editor knows about it. |
| DreamShader.Navigation | RevealNodeNoSpanOnLine | No node generated from '{File}' has a source span on line {Line}; the line produced no graph node (a declaration, a comment, or a statement that folded away). |
| DreamShader.Navigation | RevealNodeNoSpanTable | The {Count} asset(s) generated from '{File}' carry no DreamShader.SourceSpans metadata, so no node on them can be located; they predate node navigation -- rebuild them with -Force. |
| DreamShader.Navigation | RevealNodeNotMaterialEditor | '{Asset}' opened in an editor that is not the material editor, which has no node graph to select in; only a node of a Material or a Material Function can be revealed. |
| DreamShader.Navigation | RevealNodeOk | Revealed {Count} node(s) for {File}({Line}) in '{Asset}'. |
| DreamShader.Navigation | SourceFileLaunchFailed | No text editor could be launched for '{File}'; VSCode, the OS default editor and Notepad all refused. |
| DreamShader.Navigation | SourceFileMissing | The source file '{File}' this node was generated from is not on disk any more, so it could not be opened; regenerate the asset, or restore the file. |
| DreamShader.Navigation | SourceNavigationMenuLabel | DreamShader |
| DreamShader.Navigation | SourceNavigationMenuTooltip | Jump from this generated node back to the DreamShader source it came from. |
| DreamShader.Navigation | SourceNavigationSectionLabel | Source |
| DreamShader.Navigation | SpanTableBadKey | The DreamShader.SourceSpans entry '{Key}' of '{Asset}' is not an expression GUID and was skipped; that node cannot be navigated to. |
| DreamShader.Navigation | SpanTableBadRow | The DreamShader.SourceSpans entry '{Key}' of '{Asset}' is not an object with file/line/col and was skipped; that node cannot be navigated to. |
| DreamShader.Navigation | SpanTableBadSpan | The DreamShader.SourceSpans entry '{Key}' of '{Asset}' names no file or a line below 1 and was skipped; that node cannot be navigated to. |
| DreamShader.Navigation | SpanTableNotJson | The DreamShader.SourceSpans metadata of '{Asset}' is not a JSON object, so no node on it can be mapped back to a source line; rebuild the asset from its source. |
| DreamShader.Pipeline | CompileCancelled | Compiling '{0}' was cancelled; nothing was written. |
| DreamShader.Pipeline | CompilingLang2Source | Compiling DreamShader source '{0}'... |
| DreamShader.Pipeline | IncludePreprocessFailed | '{0}' failed conditional compilation: {1}: {2} |
| DreamShader.Pipeline | IncludeUnparsable | '{0}', included from '{1}', could not be parsed; its own errors are above. |
| DreamShader.Pipeline | IncludeUnreadable | '{0}', included from '{1}', resolved but could not be read. |
| DreamShader.Pipeline | IncludeUnresolved | '{0}', included from '{1}', could not be resolved: {2}. |
| DreamShader.Pipeline | IncludeWrongKind | '{0}' is not a DreamShader header; an include names a '.dsh' (or a '.dss'), not a '{1}' file. |
| DreamShader.Pipeline | InstanceChainCycle | '{0}' is its own ancestor: following Parent from it comes back to it ({1}). |
| DreamShader.Pipeline | InstanceChainTooDeep | The Parent chain above '{0}' is more than {1} instances deep; a chain that long is almost always a mistake in a Parent key. |
| DreamShader.Pipeline | Lang2Binding | Resolving names in '{0}'... |
| DreamShader.Pipeline | Lang2Emitting | Building the graph for '{0}'... |
| DreamShader.Pipeline | Lang2Lowering | Lowering '{0}' to IR... |
| DreamShader.Pipeline | Lang2Parsing | Parsing '{0}'... |
| DreamShader.Pipeline | Lang2Reading | Reading '{0}'... |
| DreamShader.Pipeline | Lang2Validating | Validating the IR of '{0}'... |
| DreamShader.Pipeline | NotACompilableSource | '{0}' is not a source the compiler builds on its own; it builds '.dss', '.dsi', '.dsm' and '.dsf' files, and a '.dsh' header only through the source that includes it. |
| DreamShader.Pipeline | ParentCompiledFirst | '{0}' was missing or older than its source, so '{1}' was compiled first. |
| DreamShader.Pipeline | ParentCompileFailed | The parent source '{0}' failed to compile, so this instance has no parent to build against. {1} |
| DreamShader.Pipeline | ParentSourceDoesNotBuild | The parent '{0}' comes from '{1}', which does not compile, so the parameters this instance overrides cannot be checked; compile that source to see why. |
| DreamShader.Pipeline | ProductCycle | The exported functions {0} call one another in a cycle, so there is no order in which they can be built; an exported function may call another only in one direction. |
| DreamShader.Pipeline | ResolveDestinationFailed | '{0}' does not resolve to a valid asset path. {1} |
| DreamShader.Pipeline | SourcePreprocessFailed | '{0}' failed conditional compilation: {1}: {2} |
| DreamShader.Pipeline | SourceUnreadable | '{0}' could not be read. |
| DreamShader.Preprocessor | BranchAfterElse | {0}({1}): '#{2}' after the '#else' on line {3}, which already closed this chain. |
| DreamShader.Preprocessor | ConditionalNestingTooDeep | {0}({1}): '#{2}' nesting is deeper than the limit of {3}. |
| DreamShader.Preprocessor | InvalidDefineName | {0}({1}): '#{2}' needs a name made of letters, digits and underscores and not starting with a digit; got '{3}'. |
| DreamShader.Preprocessor | InvalidDefineNameOnDefinition | {0}({1}): '#{2}' needs a name made of letters, digits and underscores and not starting with a digit; got '{3}'. |
| DreamShader.Preprocessor | MissingDefineNameOperand | {0}({1}): '#{2}' requires a define name. |
| DreamShader.Preprocessor | ReservedDefineName | {0}({1}): '{3}' is a read-only built-in constant, so '#{2}' cannot change it. The 'DS_' prefix is reserved by DreamShader. |
| DreamShader.Preprocessor | StrayConditionalBranch | {0}({1}): '#{2}' without a matching '#if'. |
| DreamShader.Preprocessor | StrayEndif | {0}({1}): '#endif' without a matching '#if'. |
| DreamShader.Preprocessor | UnknownDirectiveSuggestCase | Preprocessor directives are lowercase: write '#{0}'. |
| DreamShader.Preprocessor | UnknownDirectiveSuggestImport | '#include' is HLSL: it is recognized inside a Function body and nowhere else. At the declaration level, use import "..." instead. |
| DreamShader.Preprocessor | UnknownDirectiveSuggestList | A '#' line must be #if, #ifdef, #ifndef, #elif, #else, #endif, #define or #undef, or one of the parser's #Region / #EndRegion. |
| DreamShader.Preprocessor | UnknownDirectiveSuggestNearest | Did you mean '#{0}'? |
| DreamShader.Preprocessor | UnknownPreprocessorDirective | {0}({1}): unknown preprocessor directive '#{2}'. {3} |
| DreamShader.Preprocessor | UnterminatedConditional | {0}({1}): this '#if' is never closed; the file ends with {2} conditional block(s) still open. |
| DreamShader.Preprocessor.Expression | BadIntegerLiteral | '{0}' is not a valid integer literal (decimal, or 0x hexadecimal). |
| DreamShader.Preprocessor.Expression | ConditionDivideByZero | {0}({1}): the right operand of '{2}' in this '{3}' condition is zero. |
| DreamShader.Preprocessor.Expression | ConditionTypeMismatch | {0}({1}): type mismatch in '{2}' condition: {3} |
| DreamShader.Preprocessor.Expression | DefinedExpectedCloseParenthesis | expected ')' to close 'defined({0})' but found {1}. |
| DreamShader.Preprocessor.Expression | DefinedNeedsName | 'defined' needs a define name, but found {0}. |
| DreamShader.Preprocessor.Expression | ExpectedCloseParenthesis | expected ')' but found {0}. |
| DreamShader.Preprocessor.Expression | InvalidConditionExpression | {0}({1}): invalid '{2}' condition: {3} |
| DreamShader.Preprocessor.Expression | MissingConditionExpression | {0}({1}): '{2}' requires a condition expression. |
| DreamShader.Preprocessor.Expression | MixedEqualityOperands | '{0}' cannot compare a string with a number. |
| DreamShader.Preprocessor.Expression | StringAsCondition | a condition must be a number, but this one is the string "{0}". Compare it with '==' instead. |
| DreamShader.Preprocessor.Expression | StringAsTruthValue | '{0}' needs a number, but one operand is the string "{1}". |
| DreamShader.Preprocessor.Expression | StringInNumericOperator | '{0}' is only defined for numbers, but one operand is the string "{1}". Strings compare only with '==' and '!='. |
| DreamShader.Preprocessor.Expression | TokenEndOfCondition | the end of the condition |
| DreamShader.Preprocessor.Expression | TokenSpelling | '{0}' |
| DreamShader.Preprocessor.Expression | TrailingTokensAfterDirective | {0}({1}): '{2}' is already complete before '{3}'. Nothing may follow a directive but a '//' comment. |
| DreamShader.Preprocessor.Expression | UnexpectedCharacter | unexpected character '{0}'. |
| DreamShader.Preprocessor.Expression | UnexpectedTokenInCondition | unexpected {0}. |
| DreamShader.Preprocessor.Expression | UnterminatedConditionString | unterminated string literal. |
| DreamShader.Preview | CompiledPreviewMaterial | Compiled preview material for {0}. |
| DreamShader.Preview | CompiledPreviewMaterialWithDetails | Compiled preview material for {0}. {1} |
| DreamShader.Preview | GeneratedMaterialCouldNotBeLoaded | Generated material '{0}' could not be loaded. |
| DreamShader.Preview | PreviewCompilerUnavailable | The DreamShader compiler module is not available, so '{0}' could not be compiled for preview. |
| DreamShader.Preview | PreviewSourceMissing | DreamShader source '{0}' does not exist. |
| DreamShader.Preview | PreviewSupportsOnlyMaterialSources | DreamShader preview renders a material or a material instance, so it takes a .dss, .dsi or .dsm source: '{0}'. |
| DreamShader.Preview | RenderedPreview | Rendered preview for {0}. |
| DreamShader.ProductIndex | ParentIsSelf | '{0}' names this instance itself as its parent; an instance needs a different material to instance. |
| DreamShader.ProductIndex | ParentMissing | The parent '{0}' names no material: nothing exists at '{1}', and no DreamShader source under the source roots builds it. |
| DreamShader.ProductIndex | ParentNameAmbiguous | '{0}' names more than one product under '{1}' ({2}); write the parent's asset path instead. |
| DreamShader.ProductIndex | ParentNameIsSelf | '{0}' is the name of this instance itself; an instance needs a different material to instance. |
| DreamShader.ProductIndex | ParentNameNotFound | No material or instance named '{0}' is built by a source under '{1}'; write the parent's asset path, or check the name. |
| DreamShader.ProductIndex | ParentReferenceUnresolved | The parent '{0}' does not resolve to an asset path. {1} |
| DreamShader.Tools | DiagnosticsOutFailed | The diagnostics JSON could not be written: {0}. |
| DreamShader.Tools | DumpIRWriteFailed | The IR dump could not be written: {0}. |
| DreamShader.Tools | ExportCatalogEmpty | The builtin catalog came back empty, so '{0}' describes no expression at all. Reflection found no UMaterialExpression classes, which normally means the Engine module is not loaded. |
| DreamShader.Tools | ExportCatalogWriteFailed | The builtin catalog manifest could not be written: {0}. |
| DreamShader.Tools | IndexWriteFailed | The symbol index could not be written: {0}. |
| DreamShader.Tools | NoMaterialToCheck | '{0}' produced no material, so there are no shaders to compile; a function library is checked by the material that calls it. |
| DreamShader.Tools | NotALang2SourceForVerb | '{0}' is not a compilable DreamShader source (.dss, .dsi, .dsm or .dsf), so '{1}' has nothing to do with it; a .dsh header is checked through a source that includes it. |
| DreamShader.Tools | ShaderCompileError | [{0} / {1}] {2} |
| DreamShader.Tools | ShaderCompileTimedOut | Shader compilation for '{0}' did not finish within {1} seconds per material. A compile that never finishes is usually a dynamic loop or a texture read whose mip cannot be resolved in a divergent branch; move it into a '@custom' body with an explicit SampleLevel. |
| DreamShader.Tools | UnknownQualityLevel | '{0}' is not a material quality level. Write Low, Medium, High or Epic. |
| DreamShader.Tools | UnknownShaderPlatform | '{0}' is not a shader platform this engine knows. Write SM6, SM5, ES3_1, or a shader format name such as PCD3D_SM6. |
| DreamShader.Tools | VerbCheck | check |
| DreamShader.Tools | VerbDumpIR | dump-ir |
| DreamShader.Tools | VerbIndex | index |
| DreamShader.VirtualFunction | InvalidMaterialFunctionPackagePath | MaterialFunction '{0}' does not have a valid package path. |
| DreamShader.VirtualFunction | MaterialFunctionHasNoOutputs | MaterialFunction '{0}' does not expose any outputs. |
| DreamShader.VirtualFunction | NoMaterialFunctionAssetProvided | No MaterialFunction asset was provided. |
| DreamShader.VirtualFunction | VirtualFunctionHasNoOutputs | VirtualFunction '{0}' does not expose any outputs. |
| DreamShader.VirtualFunction | VirtualFunctionNameCannotBeEmpty | VirtualFunction name cannot be empty. |
| DreamShader.VirtualFunction | VirtualFunctionNoPrototype | The VirtualFunction definition did not read back as an extern prototype. |
| DreamShaderEditor.Settings | SectionDescription | Dream Shader Settings |
| DreamShaderEditor.Settings | SectionText | Dream Shader |
| DreamShaderEditorBridge | DreamShaderAdoptBackupFailed | Could not back up '{0}' to '{1}'; nothing was written. |
| DreamShaderEditorBridge | DreamShaderAdoptConditionalSource | DSH8149: '{0}' uses conditional compilation, and '{1}' holds only the branch that was taken -- adopting it would write that one branch back over the file and delete the rest. Move the change into the matching branch of the source by hand, or use DreamShader > Detach first if this asset should stop being generated from it. |
| DreamShaderEditorBridge | DreamShaderAdoptConfirmDss | Rewrite '{0}' from the current contents of the {1} asset(s) it builds, '{2}' among them?\n\nThe existing source is copied to '{3}' first. The rewritten file is the decompiler's own form, so hand-written comments, helper functions and formatting in it are replaced. |
| DreamShaderEditorBridge | DreamShaderAdoptDecompileFailed | DreamShader could not decompile '{0}', so nothing was written: {1} |
| DreamShaderEditorBridge | DreamShaderAdoptInstanceConditionalSource | DSH8149: '{0}' uses conditional compilation, so the overrides of '{1}' cannot be spliced into it without deleting the branches this build did not take; move the change into the source by hand. |
| DreamShaderEditorBridge | DreamShaderAdoptInstanceConfirm | Write the parameter overrides and instance settings of '{0}' back into '{1}'?\n\nThe existing file is copied to '{2}' first. Only the declarations whose values changed are rewritten, so comments and the order of the file are kept. |
| DreamShaderEditorBridge | DreamShaderAdoptInstanceNotDsi | '{0}' is not a .dsi instance file, so the overrides of '{1}' cannot be spliced into it. |
| DreamShaderEditorBridge | DreamShaderAdoptInstanceNotMic | '{0}' is built from the instance file '{1}' but is not a plain material instance, so nothing was adopted. |
| DreamShaderEditorBridge | DreamShaderAdoptInstanceParentUnresolved | DSH9103: The Parent of '{0}' no longer resolves, so the overrides of '{1}' cannot be written back into it: {2} |
| DreamShaderEditorBridge | DreamShaderAdoptInstanceResult | Wrote the overrides of '{0}' into '{1}' ({2} edit(s), backup: '{3}'). {4} |
| DreamShaderEditorBridge | DreamShaderAdoptInstanceRewriteRefused | The overrides of '{0}' could not be spliced into '{1}', so nothing was written. |
| DreamShaderEditorBridge | DreamShaderAdoptInstanceUnbound | '{0}' does not check, so the overrides of '{1}' cannot be spliced into it; fix the file first. |
| DreamShaderEditorBridge | DreamShaderAdoptInstanceUnchanged | '{0}' already states every override of '{1}', so only the instance was rebuilt. {2} |
| DreamShaderEditorBridge | DreamShaderAdoptInstanceUnreadable | '{0}' could not be read, so nothing was adopted. |
| DreamShaderEditorBridge | DreamShaderAdoptLabel | Adopt Into Source |
| DreamShaderEditorBridge | DreamShaderAdoptMigrationConfirm | Adopt '{0}' as a migration of '{1}' to 2.0?\n\nThe asset is decompiled into '{2}', and the 1.x source is moved to '{3}'. The new file is the decompiler's own form, so comments and formatting of the old file are not carried over. |
| DreamShaderEditorBridge | DreamShaderAdoptMigrationMoveFailed | Could not move '{0}' out of the source tree, so the new '{1}' was removed again and nothing changed. |
| DreamShaderEditorBridge | DreamShaderAdoptMigrationResult | Adopted '{0}' as a migration into '{1}' (the 1.x source's backup: '{2}'). {3} |
| DreamShaderEditorBridge | DreamShaderAdoptMigrationTargetExists | '{0}' already exists, so '{1}' cannot be adopted as a migration into it; move or delete that file first. |
| DreamShaderEditorBridge | DreamShaderAdoptNoBackup | none |
| DreamShaderEditorBridge | DreamShaderAdoptNoProducts | '{0}' declares no asset any more, so '{1}' cannot be adopted into it. |
| DreamShaderEditorBridge | DreamShaderAdoptOpenInEditor | '{0}' is open in an asset editor, whose copy a decompile cannot see, so nothing was adopted; save and close the editor, then adopt again. |
| DreamShaderEditorBridge | DreamShaderAdoptResult | Adopted '{0}' into '{1}' (backup: '{2}'). {3} |
| DreamShaderEditorBridge | DreamShaderAdoptTooltip | Rewrite the DreamShader source file from this asset's current contents, so your hand edits become the source of truth. The previous source is backed up alongside it. |
| DreamShaderEditorBridge | DreamShaderAdoptTweakedInstance | '{0}' still matches its source and only carries parameter overrides, so there is nothing to adopt; use DreamShader > Adopt Tweaks as Source Defaults or Extract Tweaks to .dsi instead. |
| DreamShaderEditorBridge | DreamShaderAdoptTweaksLabel | Adopt Tweaks as Source Defaults |
| DreamShaderEditorBridge | DreamShaderAdoptTweaksTooltip | Write this instance's parameter overrides into the .dss as the defaults of its uniforms, then clear them from the instance. The source is backed up first. |
| DreamShaderEditorBridge | DreamShaderAdoptTweaksUnavailableTooltip | Only a .dss source under a writable root takes tweaks as uniform defaults; use Extract Tweaks to .dsi instead. |
| DreamShaderEditorBridge | DreamShaderAdoptUnresolved | '{0}' could not be resolved to the assets it builds, so nothing was adopted. |
| DreamShaderEditorBridge | DreamShaderCleanGeneratedShadersLabel | Clean Generated Shaders |
| DreamShaderEditorBridge | DreamShaderCleanGeneratedShadersTooltip | Delete Intermediate/DreamShader/GeneratedShaders and queue a full DreamShader recompile. |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionCallLabel | CopyVirtualFunctionCall |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionCallNoAsset | DreamShader could not find the selected Material Function. |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionCallTooltip | Copy a DreamShader Graph call example for this VirtualFunction. |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionLabel | CopyVirtualFunction |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionNoAsset | DreamShader could not find the selected Material Function. |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionPrototypeLabel | Copy extern Prototype (2.0) |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionPrototypeTooltip | Copy the 2.0 extern prototype of this Material Function, with its /// @asset line, for a .dss or a .dsh. |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionReferenceLabel | Copy Virtual Function Reference |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionReferenceNoAsset | DreamShader could not find the selected Material Function. |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionReferenceTooltip | Copy a DreamShader Graph call that references this existing VirtualFunction. |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionTooltip | Copy a complete DreamShader VirtualFunction declaration for this Material Function. |
| DreamShaderEditorBridge | DreamShaderCreateVirtualFunctionLabel | CreateVirtualFunction |
| DreamShaderEditorBridge | DreamShaderCreateVirtualFunctionNoAsset | DreamShader could not find the selected Material Function. |
| DreamShaderEditorBridge | DreamShaderCreateVirtualFunctionPrototypeLabel | Create extern Prototype (2.0) |
| DreamShaderEditorBridge | DreamShaderCreateVirtualFunctionPrototypeTooltip | Create a .dsh file holding the 2.0 extern prototype of this Material Function, for .dss sources to #include. |
| DreamShaderEditorBridge | DreamShaderCreateVirtualFunctionTooltip | Create a .dsh file containing the VirtualFunction declaration. |
| DreamShaderEditorBridge | DreamShaderDecompileActionsSection | Decompiler |
| DreamShaderEditorBridge | DreamShaderDetachConfirm | Stop managing '{0}'?\n\nIt keeps its current contents and becomes an ordinary asset. DreamShader will never rebuild it again, and compiling '{1}' afterwards fails with an ownership error until you move or rename one of them. |
| DreamShaderEditorBridge | DreamShaderDetachLabel | Detach From DreamShader |
| DreamShaderEditorBridge | DreamShaderDetachNoAsset | DreamShader could not find the selected asset. |
| DreamShaderEditorBridge | DreamShaderDetachNotGenerated | '{0}' is not a DreamShader-generated asset. |
| DreamShaderEditorBridge | DreamShaderDetachResult | '{0}' is no longer managed by DreamShader. Save it to keep the change. |
| DreamShaderEditorBridge | DreamShaderDetachTooltip | Keep this asset exactly as it is and stop DreamShader from ever rebuilding it. It becomes an ordinary hand-authored asset. |
| DreamShaderEditorBridge | DreamShaderDivergenceAdopt | Adopt Into Source |
| DreamShaderEditorBridge | DreamShaderDivergenceAdoptTip | Rewrite the DreamShader source file from this asset's current contents, so your hand edits become the source of truth. The previous source is backed up alongside it. |
| DreamShaderEditorBridge | DreamShaderDivergenceDetach | Detach |
| DreamShaderEditorBridge | DreamShaderDivergenceDetachTip | Keep this asset exactly as it is and stop DreamShader from ever rebuilding it. It becomes an ordinary hand-authored asset. |
| DreamShaderEditorBridge | DreamShaderDivergenceDismiss | Dismiss |
| DreamShaderEditorBridge | DreamShaderDivergenceDismissTip | Leave the asset alone for now. The refusal stays in the log, in the diagnostics, and in the Material Content Browser. |
| DreamShaderEditorBridge | DreamShaderDivergenceOpenBrowser | Open Material Browser |
| DreamShaderEditorBridge | DreamShaderDivergenceOpenBrowserTip | Open the DreamShader Material Content Browser, which lists every source and the state of the asset it generated. |
| DreamShaderEditorBridge | DreamShaderDivergenceRevert | Revert to Source |
| DreamShaderEditorBridge | DreamShaderDivergenceRevertTip | Discard the hand edits and rebuild this asset from its DreamShader source. The source file is not modified. |
| DreamShaderEditorBridge | DreamShaderDivergenceShowEphemeral | Show Ephemeral Materials |
| DreamShaderEditorBridge | DreamShaderDivergenceShowEphemeralTip | This asset is Ephemeral -- it has no file on disk -- and is currently hidden. Show Ephemeral DreamShader materials in the Content Browser so you can find and inspect it. |
| DreamShaderEditorBridge | DreamShaderDivergenceSubText | Rebuilding it from {0} would destroy those edits. Decide which copy is right. |
| DreamShaderEditorBridge | DreamShaderDivergenceSummary | {0} generated assets were edited by hand and were not rebuilt. |
| DreamShaderEditorBridge | DreamShaderDivergenceSummaryDismiss | Dismiss |
| DreamShaderEditorBridge | DreamShaderDivergenceSummaryDismissTip | Leave them alone for now. Every refusal is in the log and in the diagnostics. |
| DreamShaderEditorBridge | DreamShaderDivergenceTitle | '{0}' was edited by hand, so it was not rebuilt. |
| DreamShaderEditorBridge | DreamShaderEphemeralMaterialsHidden | Hidden {0} Ephemeral material(s) from the Content Browser and asset pickers. |
| DreamShaderEditorBridge | DreamShaderEphemeralMaterialsShown | Showing {0} Ephemeral material(s) in the Content Browser and asset pickers. |
| DreamShaderEditorBridge | DreamShaderEphemeralShadowed | {0} previously generated asset(s) are still saved on disk and shadow the Ephemeral materials. Run Tools > DreamShader > Make Ephemeral to remove them. |
| DreamShaderEditorBridge | DreamShaderExported | Exported '{0}'. |
| DreamShaderEditorBridge | DreamShaderExportedNotOpened | Exported '{0}' but could not open it. |
| DreamShaderEditorBridge | DreamShaderExportFailed | DreamShader failed to export '{0}': {1} |
| DreamShaderEditorBridge | DreamShaderExportFunctionDssLabel | Export .dss |
| DreamShaderEditorBridge | DreamShaderExportFunctionDssTooltip | Decompile this Material Function graph into a 2.0 DreamShader .dss source file. |
| DreamShaderEditorBridge | DreamShaderExportFunctionLegacyLabel | Export Legacy .dsf |
| DreamShaderEditorBridge | DreamShaderExportFunctionLegacyTooltip | Decompile this Material Function graph into a 1.x .dsf source file, with the 1.x decompiler that is kept through 2.0.x. |
| DreamShaderEditorBridge | DreamShaderExportFunctionNoAsset | DreamShader could not find the selected Material Function. |
| DreamShaderEditorBridge | DreamShaderExportInstanceNoAsset | DreamShader could not find the selected material instance. |
| DreamShaderEditorBridge | DreamShaderExportMaterialDssLabel | Export .dss |
| DreamShaderEditorBridge | DreamShaderExportMaterialDssTooltip | Decompile this Material graph into a 2.0 DreamShader .dss source file. |
| DreamShaderEditorBridge | DreamShaderExportMaterialInstanceDsiLabel | Export .dsi |
| DreamShaderEditorBridge | DreamShaderExportMaterialInstanceDsiTooltip | Decompile this material instance into a .dsi source file: its parent, its instance settings and every parameter that differs from the parent. |
| DreamShaderEditorBridge | DreamShaderExportMaterialLegacyLabel | Export Legacy .dsm |
| DreamShaderEditorBridge | DreamShaderExportMaterialLegacyTooltip | Decompile this Material graph into a 1.x .dsm source file, with the 1.x decompiler that is kept through 2.0.x. |
| DreamShaderEditorBridge | DreamShaderExportMaterialNoAsset | DreamShader could not find the selected Material. |
| DreamShaderEditorBridge | DreamShaderExtractTweaksLabel | Extract Tweaks to .dsi |
| DreamShaderEditorBridge | DreamShaderExtractTweaksTooltip | Write this instance's parameter overrides into a new .dsi whose parent is this material, compile it, and clear the overrides from this instance. |
| DreamShaderEditorBridge | DreamShaderFunctionDecompileActionsSection | Decompiler |
| DreamShaderEditorBridge | DreamShaderFunctionProvenanceActionsSection | Generated Asset |
| DreamShaderEditorBridge | DreamShaderInstanceConstantDecompileActionsSection | Decompiler |
| DreamShaderEditorBridge | DreamShaderInstanceConstantProvenanceActionsSection | Generated Asset |
| DreamShaderEditorBridge | DreamShaderInstanceProvenanceActionsSection | Generated Asset |
| DreamShaderEditorBridge | DreamShaderMakeEphemeralLabel | Make Ephemeral |
| DreamShaderEditorBridge | DreamShaderMakeEphemeralNoneFound | No Materialized DreamShader ThinCustom products found. |
| DreamShaderEditorBridge | DreamShaderMakeEphemeralResult | Made {0} of {1} Materialized product(s) Ephemeral. |
| DreamShaderEditorBridge | DreamShaderMakeEphemeralTooltip | Delete the packages of Materialized ThinCustom products so they go back to being Ephemeral. Shows a confirmation with the full list; source files are untouched and the products are rebuilt in memory. Graph materials and material functions are not listed -- they have no Ephemeral state. |
| DreamShaderEditorBridge | DreamShaderMaterialActionsLabel | DreamShader |
| DreamShaderEditorBridge | DreamShaderMaterialActionsTooltip | DreamShader actions for this Material. |
| DreamShaderEditorBridge | DreamShaderMaterialFunctionActionsLabel | DreamShader |
| DreamShaderEditorBridge | DreamShaderMaterialFunctionActionsTooltip | DreamShader actions for this Material Function. |
| DreamShaderEditorBridge | DreamShaderMaterialFunctionToolbarMenuLabel | DreamShader |
| DreamShaderEditorBridge | DreamShaderMaterialFunctionToolbarMenuTooltip | DreamShader actions for this Material Function. |
| DreamShaderEditorBridge | DreamShaderMaterialInstanceActionsLabel | DreamShader |
| DreamShaderEditorBridge | DreamShaderMaterialInstanceActionsTooltip | DreamShader actions for this generated material. |
| DreamShaderEditorBridge | DreamShaderMaterialInstanceConstantActionsLabel | DreamShader |
| DreamShaderEditorBridge | DreamShaderMaterialInstanceConstantActionsTooltip | DreamShader actions for this material instance. |
| DreamShaderEditorBridge | DreamShaderMaterialToolbarMenuLabel | DreamShader |
| DreamShaderEditorBridge | DreamShaderMaterialToolbarMenuTooltip | DreamShader actions for this Material. |
| DreamShaderEditorBridge | DreamShaderOpenVirtualFunctionLabel | OpenVirtualFunction |
| DreamShaderEditorBridge | DreamShaderOpenVirtualFunctionNoAsset | DreamShader could not find the selected Material Function. |
| DreamShaderEditorBridge | DreamShaderOpenVirtualFunctionTooltip | Open the existing DreamShader VirtualFunction definition in VSCode. |
| DreamShaderEditorBridge | DreamShaderOpenWorkspaceLabel | Open Dream Shader Workspace (VSCode) |
| DreamShaderEditorBridge | DreamShaderOpenWorkspaceSharedLabel | DreamShader Workspace |
| DreamShaderEditorBridge | DreamShaderOpenWorkspaceToolbarTooltip | Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable. |
| DreamShaderEditorBridge | DreamShaderOpenWorkspaceTooltip | Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable. |
| DreamShaderEditorBridge | DreamShaderProvenanceActionsSection | Generated Asset |
| DreamShaderEditorBridge | DreamShaderProvenanceInstanceDecompileNoReason | the decompiler gave no reason |
| DreamShaderEditorBridge | DreamShaderProvenanceInstanceDecompileRefused | '{0}' holds state a .dsi cannot state, so nothing was written: {1} |
| DreamShaderEditorBridge | DreamShaderProvenanceNoAsset | DreamShader could not find the selected asset. |
| DreamShaderEditorBridge | DreamShaderProvenanceWriteFailed | Could not write '{0}'. |
| DreamShaderEditorBridge | DreamShaderRecompileLabel | Recompile DSM |
| DreamShaderEditorBridge | DreamShaderRecompileSharedLabel | Recompile DSM |
| DreamShaderEditorBridge | DreamShaderRecompileSharedTooltip | Recompile all DreamShader .dsm and .dsf source files and refresh diagnostics. Asks first. |
| DreamShaderEditorBridge | DreamShaderRecompileTooltip | Recompile all DreamShader .dsm and .dsf source files and refresh diagnostics. Asks first. |
| DreamShaderEditorBridge | DreamShaderRevertConfirm | Rebuild '{0}' from '{1}'?\n\nEvery hand edit in the asset is discarded. The source file is not modified. |
| DreamShaderEditorBridge | DreamShaderRevertDivergedLabel | Revert to Source (discards your edits) |
| DreamShaderEditorBridge | DreamShaderRevertLabel | Revert to Source |
| DreamShaderEditorBridge | DreamShaderRevertTooltip | Rebuild this asset from the DreamShader source it was generated from, discarding every hand edit in it. The source file is not modified. |
| DreamShaderEditorBridge | DreamShaderSharedSectionLabel | DreamShader |
| DreamShaderEditorBridge | DreamShaderToggleShowEphemeralMaterialsLabel | Show Ephemeral Materials |
| DreamShaderEditorBridge | DreamShaderToggleShowEphemeralMaterialsTooltip | Show Ephemeral ThinCustom/Instance-backend DreamShader materials in the Content Browser and asset pickers — needed when picking one as a material instance Parent or referencing it from a detail panel. Graph-backend materials are plain UMaterials with no Ephemeral state, so this toggle does not affect them. While shown, an explicit Save on one would materialize it to disk (the shadow warning and Make Ephemeral cover recovery). |
| DreamShaderEditorBridge | DreamShaderTweaksAdoptConfirm | Write the parameter overrides of '{0}' into '{1}' as the defaults of its uniforms?\n\nThe existing source is copied to '{2}' first, only the initializers and @default values that change are rewritten, and the overrides are then cleared from the instance. |
| DreamShaderEditorBridge | DreamShaderTweaksAdoptResult | Wrote {0} tweak(s) of '{1}' into '{2}' as uniform defaults and cleared them from the instance (backup: '{3}'). {4} |
| DreamShaderEditorBridge | DreamShaderTweaksConditionalSource | DSH8149: '{0}' uses conditional compilation, so the tweaks of '{1}' cannot be spliced into its uniforms without deleting the branches this build did not take; set the defaults by hand. |
| DreamShaderEditorBridge | DreamShaderTweaksExtractCompileFailed | Wrote '{0}', but it did not compile, so the tweaks stay on '{1}'. {2} |
| DreamShaderEditorBridge | DreamShaderTweaksExtractConfirm | Create '{0}' holding the parameter overrides of '{1}', compile it, and clear the overrides from '{1}'?\n\nMeshes and materials that use '{1}' lose the tuned look until you point them at the new instance. |
| DreamShaderEditorBridge | DreamShaderTweaksExtractResult | Extracted {0} tweak(s) of '{1}' into '{2}' and cleared them from the instance; anything that uses '{1}' shows the untuned material until it is pointed at the new instance.{3} {4} |
| DreamShaderEditorBridge | DreamShaderTweaksExtractSaveFailed |  Clearing the tweaks could not be saved: {0} |
| DreamShaderEditorBridge | DreamShaderTweaksNeedDss | '{0}' is not a .dss source, and tweaks are written into 2.0 uniform declarations; migrate the source first, or use Extract Tweaks to .dsi. |
| DreamShaderEditorBridge | DreamShaderTweaksNone | '{0}' carries no parameter override a source default can state, so nothing was written. |
| DreamShaderEditorBridge | DreamShaderTweaksNotTweaked | '{0}' is not a generated instance that only carries parameter overrides, so there are no tweaks to write back. |
| DreamShaderEditorBridge | DreamShaderTweaksOpenInEditor | '{0}' is open in an asset editor, whose copy is not what a decompile reads, so no tweak was written; save and close the editor, then try again. |
| DreamShaderEditorBridge | DreamShaderTweaksRewriteRefused | The tweaks of '{0}' could not be spliced into the uniforms of '{1}', so nothing was written. |
| DreamShaderEditorBridge | DreamShaderTweaksSourceUnchecked | '{0}' does not check, so the tweaks of '{1}' cannot be spliced into it; fix the source first. |
| DreamShaderEditorBridge | DreamShaderTweaksTargetExists | '{0}' already exists, so the tweaks were not extracted into it. |
| DreamShaderEditorBridge | DreamShaderTweaksTargetNotDsi | '{0}' is not a .dsi file name, so the tweaks were not extracted. |
| DreamShaderEditorBridge | DreamShaderTweaksTargetReadOnly | '{0}' is not under a writable source root, where the watcher would find it, so the tweaks were not extracted. |
| DreamShaderEditorBridge | DreamShaderTweaksUnreadable | '{0}' could not be read, so no tweak was written. |
| DreamShaderEditorBridge | DreamShaderVirtualFunctionActionsSection | VirtualFunction |
| DreamShaderEditorBridge | DreamShaderVirtualFunctionPrototypeCopied | Copied the extern prototype of {0}. |
| DreamShaderEditorBridge | DreamShaderVirtualFunctionPrototypeCreated | Created '{0}'. |
| DreamShaderEditorBridge | DreamShaderVirtualFunctionPrototypeDirectoryFailed | DreamShader failed to create directory '{0}'. |
| DreamShaderEditorBridge | DreamShaderVirtualFunctionPrototypeFailed | DreamShader failed to build the extern prototype of '{0}': {1} |
| DreamShaderEditorBridge | DreamShaderVirtualFunctionPrototypeNotOpened | Created '{0}' but could not open it. |
| DreamShaderEditorBridge | DreamShaderVirtualFunctionPrototypeWriteFailed | DreamShader failed to write '{0}'. |
| DreamShaderEditorBridge | DreamToolsComboLabel | Dream |
| DreamShaderEditorBridge | MaterialCompileErrorHeader | [{0} / {1}] {2} |
| DreamShaderEditorBridge | ProvenanceCompilerUnavailable | The DreamShader compiler module is not available, so nothing was rebuilt. |
| DreamShaderIRToAst | CustomCodeUnreadable | The code of the custom node '{0}' carries DreamShader's markers and does not read as what the compiler writes; it is kept verbatim as the body of one function, the functions it embeds included. |
| DreamShaderIRToAst | CustomPinRenamed | The custom function '{0}' has a pin '{1}' the language cannot declare under that name; it is written as '{2}', and the body still says '{1}'. |
| DreamShaderIRToAst | CustomRenamed | The custom function '{0}' is written as '{1}': the name is taken or is not one the language allows. A custom body that calls it by the old name has to be changed by hand. |
| DreamShaderIRToAst | ExternInferred | '{0}' is called and its interface was not available, so its 'extern' prototype is written from the calls alone: pins no call connects are missing from it, and their order is the order the calls wire them in. |
| DreamShaderIRToAst | InstanceProduct | '{0}' is a material instance, which is written as a '.dsi' (PrintDreamShaderInstance), not as part of a '.dss'. |
| DreamShaderIRToAst | LayerApproximated | '{0}' is a {1}, and {2}. |
| DreamShaderIRToAst | LayerDemoted | '{0}' is a {1}, and its pins are not the ones the language writes that kind with; it is written as a plain exported function, which builds a material function. |
| DreamShaderIRToAst | LayerMaterialRequired | its material input '{0}' has to be connected, and the language makes the materials of a layer and of a blend optional, as the engine's own are: the rebuilt asset lets that input go unconnected |
| DreamShaderIRToAst | LayerOutputRenamed | its output pin '{0}' is written under the input's name '{1}', because a layer's material goes in and out through one parameter |
| DreamShaderIRToAst | LayoutHintsAmbiguous | {0} variable name(s) are placed differently by more than one function of this file, and '#pragma layout' goes by name for the whole file; their positions are not kept. |
| DreamShaderIRToAst | LayoutHintsDropped | {0} node position(s) of '{1}' belong to values the source writes inline, and a position is kept by variable name; those nodes are placed by the layout pass when the file is built. |
| DreamShaderIRToAst | OutputUnconnected | The output '{0}' of '{1}' is not connected to anything; nothing is written to it. |
| DreamShaderIRToAst | OutputWithoutParam | The output '{0}' of '{1}' did not become a parameter; nothing is written to it. |
| DreamShaderIRToAst | UniformDisagrees | The parameter '{0}' appears more than once and its nodes do not agree on its default, group or kind; the uniform is written from the first one, in '{1}'. |
| DreamShaderIRToAst | UniformRenamed | The uniform '{0}' is written as '{1}': the name is already taken in this file. |
| DreamShaderIRToAstExpressions | AttributesWithoutMaterial | it reads the attributes of no material |
| DreamShaderIRToAstExpressions | CallInputUnconnectedSkipped | {0} leaves the required input '{1}' of '{2}' unconnected and nothing can stand in for it; the call is written without it and will not compile as it is. |
| DreamShaderIRToAstExpressions | CallInputUnconnectedZero | {0} leaves the required input '{1}' of '{2}' unconnected; '0.0' is passed for it. |
| DreamShaderIRToAstExpressions | CallInputUndeclared | {0} connects an input '{1}' that '{2}' does not declare; the connection is dropped. |
| DreamShaderIRToAstExpressions | CallOfVoidAsValue | the function it calls returns nothing |
| DreamShaderIRToAstExpressions | CallOutWithoutLocal | the function it calls has 'out' parameters and the call was taken for a plain value |
| DreamShaderIRToAstExpressions | CallWithoutCallee | the function it calls could not be declared |
| DreamShaderIRToAstExpressions | CompareAsReflectedCall | {0} is an If whose branches no comparison selects between; it is written as 'UE.If(...)'. |
| DreamShaderIRToAstExpressions | CoreOpArity | it has the wrong number of operands |
| DreamShaderIRToAstExpressions | CoreOpMissingOperand | one of its operands is not connected |
| DreamShaderIRToAstExpressions | CoreOpNoSpelling | the language has no spelling for this operation |
| DreamShaderIRToAstExpressions | DescribeMissingNode | node {0} of '{1}' |
| DreamShaderIRToAstExpressions | DescribeNode | node {0} ({1}) of '{2}' |
| DreamShaderIRToAstExpressions | ReflectedNoCatalogEntry | its class is not in the builtin catalog |
| DreamShaderIRToAstExpressions | ReflectedPropertySkipped | {0} sets '{1}' to a list, which a call has no argument for; the property is left at its default. |
| DreamShaderIRToAstExpressions | ReflectedPropertyUnnamed | {0} sets a property '{1}' whose name an argument cannot carry; it is left at its default. |
| DreamShaderIRToAstExpressions | ReflectedUnnamedPin | {0} has an input '{1}' that is neither a pin of '{2}.{3}' nor a name an argument can carry; it is left unconnected. |
| DreamShaderIRToAstExpressions | SampleWithoutOperands | a texture sample needs its texture and its coordinates |
| DreamShaderIRToAstExpressions | SwizzleWithoutMask | the component mask is missing |
| DreamShaderIRToAstExpressions | TakeBaseBeforeBuilt | it is read before it was written (the graph has a cycle) |
| DreamShaderIRToAstExpressions | TakeBeforeBuilt | it is read before it was written (the graph has a cycle) |
| DreamShaderIRToAstExpressions | TakeBeforeDeclared | it is read before the statement that declares it (the graph has a cycle) |
| DreamShaderIRToAstExpressions | TakeExtraOutput | a reader names an output past the one this node has in source |
| DreamShaderIRToAstExpressions | TakeInvalidValue | a reader names an output the graph does not have |
| DreamShaderIRToAstExpressions | TakeMissingOutput | a reader names an output this node does not have |
| DreamShaderIRToAstExpressions | TakeStatement | it is a statement and has no value |
| DreamShaderIRToAstExpressions | TakeUnboundInput | the function input is not one of the function's parameters |
| DreamShaderIRToAstExpressions | TakeUndeclaredOutput | the output a reader names is not one the called function declares |
| DreamShaderIRToAstExpressions | TakeUnnamedAttribute | the attribute it reads has no name |
| DreamShaderIRToAstExpressions | TakeUnnamedParameter | the parameter node carries no ParameterName |
| DreamShaderIRToAstExpressions | ValueHasNoSourceForm | {0} cannot be written as source: {1}. '0.0' stands in its place. |
| DreamShaderIRToAstStatements | GroupAttributeUnnamed | {0} writes an attribute '{1}' that is not a name the language has; the write is dropped. |
| DreamShaderIRToAstStatements | InOutInputUnconnected | {0} leaves the 'inout' input '{1}' of '{2}' unconnected; the variable passed for it starts at zero. |
| DreamShaderIRToAstStatements | LocalRenamed | The variable '{0}' of '{1}' is written as '{2}': the name is taken or is not one the language allows. |
| DreamShaderIRToAstStatements | RegionTitleChanged | A region of '{0}' is titled '{1}', which does not fit on a '#pragma region' line; it is written as '{2}'. |
| DreamShaderIRToAstStatements | SinkAttributeUnnamed | '{0}' wires a material attribute '{1}' that is not a name the language has; the connection is dropped. |
| DreamShaderIRToAstStatements | WriteAttributeUnnamed | {0} writes an attribute '{1}' that is not a name the language has; the write is dropped. |
| DreamShaderLangMigrate | DocLineBreaksLost | '@{0}' held a text of several lines, which one '///' line cannot; it is written on one line, its line breaks as blanks. |
| DreamShaderLangMigrate | OptionalWithoutDefault | '{0}' of '{1}' was an optional input, and a {2} has no default the language can write; it is a required input in the migrated file. |
| DreamShaderLangMigrate | RootKept | '{0}' keeps its '/// @root', the 1.x spelling of where its asset goes, because the place the new file would put it could not be worked out; check the asset path the first build reports. |
| DreamShaderLangMigrate | RootQualifiedImport | '{0}' names a source root in front of the path, which '#include' cannot; write the path from the root's own folder (a leading '/') or relative to this file, then migrate again. |
| DreamShaderLangMigrate | SelectedOutputMissing | This reads output {0} of '{1}', which has no variable to be read from; the expression is replaced by '0.0'. |
| DreamShaderMaterialBrowser | AdoptBtn | Adopt Into Source |
| DreamShaderMaterialBrowser | AdoptReadOnlyTip | This asset's source ships with a plugin and is read-only; adopt is not available. |
| DreamShaderMaterialBrowser | AdoptTip | Rewrite the DreamShader source file from this asset's current contents, so your hand edits become the source of truth. The previous source is backed up alongside it. |
| DreamShaderMaterialBrowser | AdoptTweaksBtn | Adopt Tweaks as Source Defaults |
| DreamShaderMaterialBrowser | AdoptTweaksTip | Write this instance's parameter overrides into the .dss as the defaults of its uniforms, then clear them from the instance. The source is backed up first. |
| DreamShaderMaterialBrowser | AdoptTweaksUnavailableTip | Only a .dss source under a writable root takes tweaks as uniform defaults; use Extract Tweaks to .dsi instead. |
| DreamShaderMaterialBrowser | AssetLinkTip | Show this asset in the Content list. |
| DreamShaderMaterialBrowser | AssetPathNoMaterial | {0}: this file builds no material or material instance. |
| DreamShaderMaterialBrowser | AssetPathNoProduct | {0}: this file declares no material, instance or exported function. |
| DreamShaderMaterialBrowser | AssetPathUnresolved | {0}: the source could not be resolved to the assets it builds. |
| DreamShaderMaterialBrowser | AssetRow | Asset |
| DreamShaderMaterialBrowser | BadName | Provide a name and a destination folder. |
| DreamShaderMaterialBrowser | Base | Base |
| DreamShaderMaterialBrowser | BaseNone | - |
| DreamShaderMaterialBrowser | Blend | Blend mode |
| DreamShaderMaterialBrowser | BridgeBusy | bridge: {0} |
| DreamShaderMaterialBrowser | BridgeIdleGuest | bridge: idle (another editor owns writes) |
| DreamShaderMaterialBrowser | BridgeIdleOwner | bridge: idle |
| DreamShaderMaterialBrowser | BridgeOff | bridge: off |
| DreamShaderMaterialBrowser | Browse | Browse... |
| DreamShaderMaterialBrowser | BrowseTip | Pick the destination folder. |
| DreamShaderMaterialBrowser | Cancel | Cancel |
| DreamShaderMaterialBrowser | CBCreateInstance | Create DreamShader instance |
| DreamShaderMaterialBrowser | CBCreateInstanceTipDsi | Create a material instance of this material: a .dsi source file compiled into the instance when DreamShader generated the material, an ordinary instance asset otherwise. |
| DreamShaderMaterialBrowser | CBShowInBrowser | Show in Material Content Browser |
| DreamShaderMaterialBrowser | CBShowInBrowserTip | Open the DreamShader Material Content Browser on this asset: its source, compile status, provenance and inheritance. |
| DreamShaderMaterialBrowser | ChainRowFmt | {0}{1} |
| DreamShaderMaterialBrowser | ChildrenHeader | Child instances ({0}) |
| DreamShaderMaterialBrowser | ColumnAsset | Asset |
| DreamShaderMaterialBrowser | ColumnName | Name |
| DreamShaderMaterialBrowser | ColumnRoot | Root |
| DreamShaderMaterialBrowser | ColumnState | Status |
| DreamShaderMaterialBrowser | CommandContext | Material Content Browser |
| DreamShaderMaterialBrowser | CompiledAll | Compiled {0} source(s), {1} failed |
| DreamShaderMaterialBrowser | CompileFail | Failed to compile {0} |
| DreamShaderMaterialBrowser | CompileMenu | Compile |
| DreamShaderMaterialBrowser | CompileMenuTip | Compile the selection, every stale source, or everything. |
| DreamShaderMaterialBrowser | CompileOk | Compiled {0} |
| DreamShaderMaterialBrowser | CompilerUnavailable | The DreamShader compiler module is not available, so nothing was compiled. |
| DreamShaderMaterialBrowser | CompilingAll | Compiling all DreamShader sources... |
| DreamShaderMaterialBrowser | Create | Create |
| DreamShaderMaterialBrowser | CreateInstanceBtn | Create instance |
| DreamShaderMaterialBrowser | CreateInstanceSourceTitle | Create material instance (.dsi) |
| DreamShaderMaterialBrowser | CreateInstanceTitle | Create material instance |
| DreamShaderMaterialBrowser | DependentsHeader | Used by ({0}) |
| DreamShaderMaterialBrowser | DetachBtn | Detach |
| DreamShaderMaterialBrowser | DetachTip | Keep this asset exactly as it is and stop DreamShader from ever rebuilding it. |
| DreamShaderMaterialBrowser | DiagJumpTip | Open {0} at this line in your editor. |
| DreamShaderMaterialBrowser | DiagLocationCodeFmt | [{2}] L{0}:{1} |
| DreamShaderMaterialBrowser | DiagLocationFmt | L{0}:{1} |
| DreamShaderMaterialBrowser | DiagnosticLineFmt | L{0}:{1} {2} |
| DreamShaderMaterialBrowser | DiagnosticsHeader | Diagnostics ({0}) |
| DreamShaderMaterialBrowser | Domain | Domain |
| DreamShaderMaterialBrowser | EmptyContentScope | No materials here. |
| DreamShaderMaterialBrowser | Ephemeral | Ephemeral (no file on disk) |
| DreamShaderMaterialBrowser | ExportDsiBtn | Export .dsi |
| DreamShaderMaterialBrowser | ExportDsiTip | Decompile this material instance into a .dsi source file: its parent, its instance settings and every parameter that differs from the parent. |
| DreamShaderMaterialBrowser | ExportDssFunctionBtn | Export .dss |
| DreamShaderMaterialBrowser | ExportDssFunctionTip | Decompile this material function into a .dss source file under the project's DShader root. |
| DreamShaderMaterialBrowser | ExportDssMaterialBtn | Export .dss |
| DreamShaderMaterialBrowser | ExportDssMaterialTip | Decompile this material into a .dss source file under the project's DShader root. |
| DreamShaderMaterialBrowser | ExtractTweaksBtn | Extract Tweaks to .dsi |
| DreamShaderMaterialBrowser | ExtractTweaksTip | Write this instance's parameter overrides into a new .dsi whose parent is this material, compile it, and clear the overrides from this instance. |
| DreamShaderMaterialBrowser | FactoryAssetExists | An asset already exists at {0}. |
| DreamShaderMaterialBrowser | FactoryCompilerUnavailable | The DreamShader compiler module is not available, so the instance was not compiled. |
| DreamShaderMaterialBrowser | FactoryCreatePackageFailed | Failed to create package {0}. |
| DreamShaderMaterialBrowser | FactoryInstanceSourceNoAsset | '{0}' compiled, but no material instance was found at the asset path it builds. |
| DreamShaderMaterialBrowser | FactoryMaterializeFailed | Failed to materialize the material to disk: {0} |
| DreamShaderMaterialBrowser | FactoryReloadFailed | Materialized the material but could not reload it at {0}. |
| DreamShaderMaterialBrowser | FunctionDetail | Function library / header. Recompiles the materials that import it. |
| DreamShaderMaterialBrowser | FunctionUsedBy | function · used by {0} material(s) |
| DreamShaderMaterialBrowser | GenPageNoGeneratedAsset | No generated asset at {0} |
| DreamShaderMaterialBrowser | ImportsHeader | Imports ({0}) |
| DreamShaderMaterialBrowser | Inheritance | Inheritance |
| DreamShaderMaterialBrowser | InspectorEmpty | Select a source file or a material to inspect it. |
| DreamShaderMaterialBrowser | InstanceCreated | Created {0} |
| DreamShaderMaterialBrowser | InstanceNeedsCompile | Compile {0} first. |
| DreamShaderMaterialBrowser | InstanceSourceBrowse | Browse... |
| DreamShaderMaterialBrowser | InstanceSourceCompileFailed | Created {0}, but it did not compile: {1} |
| DreamShaderMaterialBrowser | InstanceSourceCreate | Create .dsi |
| DreamShaderMaterialBrowser | InstanceSourceCreated | Created {0} |
| DreamShaderMaterialBrowser | InstanceSourceFolderLabel | Source folder |
| DreamShaderMaterialBrowser | InstanceSourceHint | A .dsi source is written into the folder and compiled now. Its asset lands at the folder's /Game path, and every value you tune there can be adopted back into the file. |
| DreamShaderMaterialBrowser | InstanceSourceNameLabel | Name |
| DreamShaderMaterialBrowser | InstanceSourceOpenAfter | Open the instance after creating |
| DreamShaderMaterialBrowser | InstanceSourceParentLabel | Parent |
| DreamShaderMaterialBrowser | InstanceSourcePickFolder | Choose a source folder |
| DreamShaderMaterialBrowser | InstanceSourceUnmanaged | Unmanaged instance... |
| DreamShaderMaterialBrowser | InstanceSourceUnmanagedTip | Create an ordinary material instance asset instead, with no source file describing it. |
| DreamShaderMaterialBrowser | MaterializeBtn | Materialize |
| DreamShaderMaterialBrowser | Materialized | Materialized {0} to disk |
| DreamShaderMaterialBrowser | MaterializeNoSource | This material is memory-only and has no DreamShader source file to materialize from. |
| DreamShaderMaterialBrowser | MaterializeTip | Write this memory-only material (and its base) to disk. |
| DreamShaderMaterialBrowser | MenuSectionBuild | Build |
| DreamShaderMaterialBrowser | MenuSectionCopy | Copy |
| DreamShaderMaterialBrowser | MenuSectionDecompile | Decompiler |
| DreamShaderMaterialBrowser | MenuSectionOpen | Open |
| DreamShaderMaterialBrowser | MenuSectionProvenance | Generated asset |
| DreamShaderMaterialBrowser | NameLabel | Name |
| DreamShaderMaterialBrowser | NavContent | Content |
| DreamShaderMaterialBrowser | NavSources | Sources |
| DreamShaderMaterialBrowser | NavUnmanaged | Not managed by DreamShader |
| DreamShaderMaterialBrowser | NewFunction | Material function (.dsf) |
| DreamShaderMaterialBrowser | NewFunctionTip | A ShaderFunction block with one input, one optional input and one output. |
| DreamShaderMaterialBrowser | NewFunctionTitle | New material function (.dsf) |
| DreamShaderMaterialBrowser | NewHeader | Header (.dsh) |
| DreamShaderMaterialBrowser | NewHeaderTip | A header with one Function, for materials to import. |
| DreamShaderMaterialBrowser | NewHeaderTitle | New header (.dsh) |
| DreamShaderMaterialBrowser | NewInstance | Instance (.dsi) |
| DreamShaderMaterialBrowser | NewInstanceTip | A material instance source: a #pragma instance naming its parent, and one uniform per parameter to override. |
| DreamShaderMaterialBrowser | NewInstanceTitle | New material instance (.dsi) |
| DreamShaderMaterialBrowser | NewMaterial | Material (.dsm) |
| DreamShaderMaterialBrowser | NewMaterialTip | A Shader block with a base colour and roughness, ready to compile. |
| DreamShaderMaterialBrowser | NewMaterialTitle | New material (.dsm) |
| DreamShaderMaterialBrowser | NewMenu | New |
| DreamShaderMaterialBrowser | NewMenuTip | Create a new source file from a template, in the selected folder. |
| DreamShaderMaterialBrowser | NewObjectFailed | Failed to create the material instance object. |
| DreamShaderMaterialBrowser | NewSourceBadName | The name must be an identifier: letters, digits and underscores, not starting with a digit. |
| DreamShaderMaterialBrowser | NewSourceBrowse | Browse... |
| DreamShaderMaterialBrowser | NewSourceCancel | Cancel |
| DreamShaderMaterialBrowser | NewSourceCreate | Create |
| DreamShaderMaterialBrowser | NewSourceCreated | Created {0} |
| DreamShaderMaterialBrowser | NewSourceExists | '{0}' already exists. |
| DreamShaderMaterialBrowser | NewSourceFolderLabel | Folder |
| DreamShaderMaterialBrowser | NewSourceHint | The file is written from the plugin's template and compiled by the watcher on save. |
| DreamShaderMaterialBrowser | NewSourceInstanceHint | The file is written from the plugin's template and compiled by the watcher on save. Its asset lands at the folder's /Game path; add one uniform per parameter to override. |
| DreamShaderMaterialBrowser | NewSourceNameLabel | Name |
| DreamShaderMaterialBrowser | NewSourceNeedsParent | An instance needs a parent: the asset path of a material, or the name of a product under the same source root. |
| DreamShaderMaterialBrowser | NewSourceParentHint | /Game/Materials/M_Base, or the name of a product |
| DreamShaderMaterialBrowser | NewSourceParentLabel | Parent |
| DreamShaderMaterialBrowser | NewSourcePickFolder | Choose a source folder |
| DreamShaderMaterialBrowser | NewSourceReadOnly | Choose a folder under the project's DShader root. A plugin's sources are read-only. |
| DreamShaderMaterialBrowser | NewSourceWriteFailed | Could not write '{0}'. |
| DreamShaderMaterialBrowser | NoChildrenAnywhere | No child instances. |
| DreamShaderMaterialBrowser | NoParent | No parent material was provided. |
| DreamShaderMaterialBrowser | NothingStale | Nothing is stale. |
| DreamShaderMaterialBrowser | OnDisk | on disk |
| DreamShaderMaterialBrowser | OpenAfter | Open the instance after creating |
| DreamShaderMaterialBrowser | OpenBtn | Open |
| DreamShaderMaterialBrowser | OpenInEditorBadge | open in an asset editor — a rebuild will refuse until it is closed |
| DreamShaderMaterialBrowser | OpenSettingsTip | Open the DreamShader project settings. |
| DreamShaderMaterialBrowser | OpenTabLabel | Material Content Browser |
| DreamShaderMaterialBrowser | OpenTabTooltip | Open the DreamShader Material Content Browser. |
| DreamShaderMaterialBrowser | OpenWorkspaceTip | Open the DreamShader source workspace in VSCode. |
| DreamShaderMaterialBrowser | ParentGone | The parent material is no longer available. |
| DreamShaderMaterialBrowser | ParentLabel | Parent |
| DreamShaderMaterialBrowser | PathLabel | Folder |
| DreamShaderMaterialBrowser | PComp | Compile |
| DreamShaderMaterialBrowser | PCompTip | Force-recompile this source (in memory). |
| DreamShaderMaterialBrowser | PickFolderTitle | Choose a destination folder |
| DreamShaderMaterialBrowser | PInstTipDsi | Write a .dsi instance of this material and compile it (an ordinary material instance when DreamShader did not generate the material). |
| DreamShaderMaterialBrowser | POpenMatTip | Open the generated material asset. |
| DreamShaderMaterialBrowser | POpenSrc | Open source |
| DreamShaderMaterialBrowser | POpenSrcTipAny | Open the source file in your preferred editor. |
| DreamShaderMaterialBrowser | PreviewDragHint | drag to orbit |
| DreamShaderMaterialBrowser | PreviewFunction | function library |
| DreamShaderMaterialBrowser | PreviewMeshLabel | Mesh |
| DreamShaderMaterialBrowser | PreviewMeshTip | The shape the preview renders the material on. |
| DreamShaderMaterialBrowser | PreviewNoMaterial | not compiled yet |
| DreamShaderMaterialBrowser | PreviewRendering | rendering… |
| DreamShaderMaterialBrowser | ProvenanceDiverged | edited by hand since the last build |
| DreamShaderMaterialBrowser | ProvenanceExplainDiverged | The asset was edited by hand since it was generated, so a rebuild is refused to protect those edits. Decide which copy is the truth. |
| DreamShaderMaterialBrowser | ProvenanceExplainForeign | Not generated by DreamShader. Export it to a source file to bring it under DreamShader's management. |
| DreamShaderMaterialBrowser | ProvenanceExplainGenerated | The asset holds exactly what DreamShader last generated into it. A source change rebuilds it freely. |
| DreamShaderMaterialBrowser | ProvenanceExplainTweakedActions | The generated content still matches, and you have set parameter overrides on the instance. Rebuilds go ahead as normal and put your values back; a parameter the source no longer declares is dropped and named in the log. Adopt Tweaks writes the values into the source as defaults, and Extract Tweaks moves them into a .dsi instance. |
| DreamShaderMaterialBrowser | ProvenanceExplainUnstamped | Generated by DreamShader, but carrying no digest this version can compare. The next rebuild restamps it. |
| DreamShaderMaterialBrowser | ProvenanceForeign | not generated by DreamShader |
| DreamShaderMaterialBrowser | ProvenanceGenerated | generated (matches the last build) |
| DreamShaderMaterialBrowser | ProvenanceHeader | Provenance |
| DreamShaderMaterialBrowser | ProvenanceRow | Provenance |
| DreamShaderMaterialBrowser | ProvenanceTip | Whether the asset still holds what DreamShader last generated into it. A hand-edited asset refuses to rebuild until you choose Revert, Adopt or Detach from its Content Browser context menu. Parameter overrides on a generated instance are not a hand edit: they read as 'tweaked' and survive a rebuild. |
| DreamShaderMaterialBrowser | ProvenanceTweaked | generated, with parameter overrides on the instance |
| DreamShaderMaterialBrowser | ProvenanceUnstamped | generated (no comparable digest) |
| DreamShaderMaterialBrowser | QFDiverged | Edited by hand |
| DreamShaderMaterialBrowser | QFDivergedTip | Generated assets that no longer match what DreamShader last wrote into them. |
| DreamShaderMaterialBrowser | QFEphemeral | Ephemeral |
| DreamShaderMaterialBrowser | QFEphemeralTip | Materials that have not been written to disk. |
| DreamShaderMaterialBrowser | QFErrors | Errors |
| DreamShaderMaterialBrowser | QFErrorsTip | Sources whose last compile failed, or that could not be read. |
| DreamShaderMaterialBrowser | QFHideLibraries | Hide functions |
| DreamShaderMaterialBrowser | QFHideLibrariesTip | Drop every .dsf and .dsh from the list. |
| DreamShaderMaterialBrowser | QFHideUnmanaged | Hide unmanaged |
| DreamShaderMaterialBrowser | QFHideUnmanagedTip | Drop the materials DreamShader does not manage from the list. |
| DreamShaderMaterialBrowser | QFStale | Stale |
| DreamShaderMaterialBrowser | QFStaleTip | Sources that changed since their asset was last generated. |
| DreamShaderMaterialBrowser | QuickFilters | Quick filters |
| DreamShaderMaterialBrowser | RevertBtn | Revert to Source |
| DreamShaderMaterialBrowser | RevertDivergedBtn | Revert to Source (discards edits) |
| DreamShaderMaterialBrowser | RevertTip | Rebuild this asset from its DreamShader source, discarding every hand edit in it. The source file is not modified. |
| DreamShaderMaterialBrowser | RootProject | Project |
| DreamShaderMaterialBrowser | RootRow | Root |
| DreamShaderMaterialBrowser | SearchHintV2 | Search name, path, root, asset, error… |
| DreamShaderMaterialBrowser | SelectFirst | Select a material to create an instance of. |
| DreamShaderMaterialBrowser | ShowEphemeral | Show Ephemeral materials |
| DreamShaderMaterialBrowser | ShowEphemeralTip | Show DreamShader's Ephemeral materials here and in the Content Browser (global project setting). |
| DreamShaderMaterialBrowser | SortAscending | Ascending |
| DreamShaderMaterialBrowser | SortAsset | Asset path |
| DreamShaderMaterialBrowser | SortName | Name |
| DreamShaderMaterialBrowser | SortRoot | Root |
| DreamShaderMaterialBrowser | SortStatus | Status |
| DreamShaderMaterialBrowser | SourceLinkTip | Show this file in the Sources list. |
| DreamShaderMaterialBrowser | SourceNone | - |
| DreamShaderMaterialBrowser | SourceRow | Source |
| DreamShaderMaterialBrowser | StatusCountFmt | {0} {1} |
| DreamShaderMaterialBrowser | StatusCountTip | Click to filter the list to these. |
| DreamShaderMaterialBrowser | StatusDivergedCount | edited by hand |
| DreamShaderMaterialBrowser | StatusEphemeralCount | Ephemeral |
| DreamShaderMaterialBrowser | StatusEphemeralUntracked | compiled, Ephemeral |
| DreamShaderMaterialBrowser | StatusError | compile error |
| DreamShaderMaterialBrowser | StatusErrorCount | errors |
| DreamShaderMaterialBrowser | StatusFunction | function / header |
| DreamShaderMaterialBrowser | StatusNever | not compiled |
| DreamShaderMaterialBrowser | StatusOk | ok |
| DreamShaderMaterialBrowser | StatusStale | stale |
| DreamShaderMaterialBrowser | StatusStaleCount | stale |
| DreamShaderMaterialBrowser | StatusTotal | {0} sources |
| DreamShaderMaterialBrowser | StatusUnmanaged | not managed by DreamShader |
| DreamShaderMaterialBrowser | StatusUnmanagedCount | not managed |
| DreamShaderMaterialBrowser | StatusUnmanagedInstance | material instance · not managed by DreamShader |
| DreamShaderMaterialBrowser | StatusUnresolved | unresolved |
| DreamShaderMaterialBrowser | StatusUpToDate | up to date |
| DreamShaderMaterialBrowser | Storage | Storage |
| DreamShaderMaterialBrowser | TabTitle | Material Content Browser |
| DreamShaderMaterialBrowser | TabTooltip | Browse, manage, and create instances of project and DreamShader-generated materials. |
| DreamShaderMaterialBrowser | TemplateMissing | The template '{0}' is missing from the plugin. |
| DreamShaderMaterialBrowser | UnloadedChildFmt | └ {0} (not loaded) |
| DreamShaderMaterialBrowser | UnloadedChildTip | {0} (not loaded — click to load) |
| DreamShaderMaterialBrowser | ViewMenu | View |
| DreamShaderMaterialBrowser | ViewMenuTip | List or tiles, sorting, and what the Content Browser shows. |
| DreamShaderMaterialBrowser | ViewSectionGlobal | Content Browser |
| DreamShaderMaterialBrowser | ViewSectionLayout | Sources list |
| DreamShaderMaterialBrowser | ViewSectionSort | Sort by |
| DreamShaderTests | WireUtils.Float | value {0} |
| DreamShaderTests | WireUtils.Inner | inner {0} |
| DreamShaderTests | WireUtils.Ordered | Unsupported swizzle {0} at line {1}. |
| DreamShaderTests | WireUtils.Outer | outer [{0}] end |
| DreamShaderTests | WireUtils.Plain | Generation aborted. |
| DreamShaderVirtualFunctionSyncService | ConditionalSourceNotSynced | DSH9001: '{0}' uses conditional compilation, and VirtualFunction sync rewrites a source in place at byte offsets taken from the file as written -- it cannot tell which of your branches a definition belongs to, and refuses rather than risk writing one branch over the others. Refresh these definitions by moving them into a source without directives, or edit them by hand. |
| DreamShaderVirtualFunctionSyncService | ExpectedOneVirtualFunctionBlock | Expected exactly one VirtualFunction block. |
| DreamShaderVirtualFunctionSyncService | InvalidAssetReference | VirtualFunction '{0}' asset reference is invalid: {1} |
| DreamShaderVirtualFunctionSyncService | InvalidDeclaration | VirtualFunction declaration is invalid: {0} |
| DreamShaderVirtualFunctionSyncService | MissingClosingBrace | VirtualFunction body is missing a closing '}'. |
| DreamShaderVirtualFunctionSyncService | MissingClosingParenthesis | VirtualFunction attributes are missing a closing ')'. |
| DreamShaderVirtualFunctionSyncService | MissingMaterialFunction | VirtualFunction '{0}' references missing MaterialFunction '{1}'. |
| DreamShaderVirtualFunctionSyncService | ReadSourceFileFailed | DreamShader could not read VirtualFunction source file '{0}'. |
| DreamShaderVirtualFunctionSyncService | RefreshFailed | VirtualFunction '{0}' could not be refreshed from MaterialFunction '{1}': {2} |
| DreamShaderVirtualFunctionSyncService | UpdateSourceFileFailed | DreamShader failed to update VirtualFunction source file '{0}'. |

## Deferred diagnostics inventory
Deferred files: 17
Runtime FText::FromString/FText::FromName / FString::Printf literal call sites in deferred diagnostics: 0
Use -IncludeDeferred to lint MaterialAssetGeneration/ and Decompiler/ in the next phase.

## Auto-gathered metadata
Unreal will add any auto-gathered UPROPERTY metadata (for example DisplayName and ToolTip) on top of this compile-time baseline. This file intentionally counts only LOCTEXT/NSLOCTEXT entries.
