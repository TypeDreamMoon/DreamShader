# DreamShader localization baseline

This inventory covers compile-time LOCTEXT/NSLOCTEXT entries that the Localization Dashboard gather step can see.

It spans EVERY source file, not just the ones this script lints. GatherText scans the whole
module and knows nothing about the Scope / Deferred / Allowlisted / Excluded split, so an
inventory narrower than the module would report a count no real gather ever produces. That
includes the automation tests: their LOCTEXT entries (namespace `DreamShaderTests`) are
gathered like any other, so they are listed here rather than quietly dropped.

`-IncludeDeferred` widens which files the R1/R2 literal rules run on; it does not change this count.

## Expected gather count
1128

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
| DreamShader.Binder | IncludeCycle | Including '{0}' from '{1}' closes a cycle; a header may not include itself, directly or through another header. |
| DreamShader.Binder | IncludesUnavailable | '{0}' cannot be read: this front end was started without an include resolver, so nothing a header declares is visible. |
| DreamShader.Binder | LayerBlendNotExported | '@layerblend' makes '{0}' a material layer blend asset, so it has to be 'export'. |
| DreamShader.Binder | LayerBlendSignature | A '@layerblend' function is written 'export void {0}(material Base, material Top, ..., inout material Result)': at least one 'material' input and a final 'inout material'. |
| DreamShader.Binder | LayerNotExported | '@layer' makes '{0}' a material layer asset, so it has to be 'export'. |
| DreamShader.Binder | LayerSignature | A '@layer' function is written 'export void {0}(inout material m)'. |
| DreamShader.Binder | LinkageTargetExport | an exported function |
| DreamShader.Binder | LinkageTargetExtern | an 'extern' prototype |
| DreamShader.Binder | MaterialPragmaWithoutEntry | '#pragma material' configures a material, and this file has no 'export void Name(inout material m)' entry to configure. |
| DreamShader.Binder | MultiDimensionalArray | A multi-dimensional array has no graph form; declare one dimension, or move the code into a '/// @custom' function. |
| DreamShader.Binder | ParamDocUnknown | '@param {0}' does not name a parameter of '{1}'. |
| DreamShader.Binder | PrototypeWithoutExtern | '{0}' has no body; a prototype has to be 'extern' and carry '/// @asset'. |
| DreamShader.Binder | RedefinitionAcrossFiles | '{0}' is declared in '{1}' and again in '{2}'; a name included from a header may not be declared a second time. |
| DreamShader.Binder | RedefinitionSameFile | '{0}' is already declared in this file; one name declares one thing. |
| DreamShader.Binder | StaticOnNonBoolUniform | '@static' asks for a static switch and is only meaningful on a 'uniform bool'. |
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
| DreamShader.Binder.Directives | LayoutBadValue | '#pragma layout' expects a whole number for '{0}'; '{1}' was ignored. |
| DreamShader.Binder.Directives | LayoutExtraPositional | '#pragma layout' takes one positional selector; '{0}' was ignored. |
| DreamShader.Binder.Directives | LayoutNoKind | '#pragma layout' starts with 'Node' or 'Comment'; the line was ignored. |
| DreamShader.Binder.Directives | LayoutUnknownKey | '#pragma layout' has no '{0}' key; it was ignored. |
| DreamShader.Binder.Directives | LayoutUnknownKind | '#pragma layout({0}, ...)' is neither 'Node' nor 'Comment'; the line was ignored. |
| DreamShader.Binder.Directives | MaterialPragmaDuplicate | '{0}' is set twice by '#pragma material'; it was already set on line {1}. |
| DreamShader.Binder.Directives | MaterialPragmaPositional | '#pragma material' takes 'Key = Value' pairs; '{0}' has no key. |
| DreamShader.Binder.Directives | ParamNeedsName | '@param' is written '@param <ParameterName> <description>'. |
| DreamShader.Binder.Directives | RegionNotClosed | '#pragma region {0}' is never closed; add a '#pragma endregion'. |
| DreamShader.Binder.Directives | RegionNotClosedUnnamed | This '#pragma region' is never closed; add a '#pragma endregion'. |
| DreamShader.Binder.Directives | SamplerEmpty | '@sampler' needs a sampler type after it, such as 'Color', 'Normal' or 'LinearColor'. |
| DreamShader.Binder.Directives | SliderMalformed | '@slider' takes two numbers, a minimum and a maximum; '{0}' is not that. |
| DreamShader.Binder.Directives | SliderRange | '@slider' needs its minimum below its maximum. |
| DreamShader.Binder.Directives | SortMalformed | '@sort' takes one whole number; '{0}' is not that. |
| DreamShader.Binder.Directives | TargetExtern | an 'extern' prototype |
| DreamShader.Binder.Directives | TargetFunction | an exported function |
| DreamShader.Binder.Directives | TargetFunction2 | a function |
| DreamShader.Binder.Directives | TargetFunction3 | a function |
| DreamShader.Binder.Directives | TargetFunction4 | an exported function |
| DreamShader.Binder.Directives | TargetFunction5 | an exported function |
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
| DreamShader.Binder.Expressions | CustomOutputAsValue | '{0}.{1}' is an output node, not a value: write it as a statement on a line of its own. |
| DreamShader.Binder.Expressions | EnumNotSpelling | '{0}' is an enumerated property; write one of its values, as in 'SamplerType = Normal'. |
| DreamShader.Binder.Expressions | ExpressionNeedsClass | 'UE.Expression' reaches a node this language has no name for, so it needs 'Class = "MaterialExpressionName"'. |
| DreamShader.Binder.Expressions | GlslAlias | '{0}' is the GLSL spelling; this language is HLSL, so write '{1}'. |
| DreamShader.Binder.Expressions | HyperbolicUnsupported | The material graph has no hyperbolic node, so '{0}' cannot be lowered; write it in a '/// @custom' body, where the shader compiler has it. |
| DreamShader.Binder.Expressions | IncrementNeedsLValue | '++' and '--' write back into what they read, so they need a variable. |
| DreamShader.Binder.Expressions | InitializerElement | This initializer element |
| DreamShader.Binder.Expressions | InitializerListBadTarget | A value of type {0} cannot be written as an initializer list. |
| DreamShader.Binder.Expressions | InitializerListCount | {0} needs {1} components and this list supplies {2}. |
| DreamShader.Binder.Expressions | InitializerListNoTarget | An initializer list only has a meaning against a declared type; it cannot stand on its own. |
| DreamShader.Binder.Expressions | MatrixIndex | A matrix row cannot be read: the graph has no matrices. Move the code into a '/// @custom' function, where the matrix is an input. |
| DreamShader.Binder.Expressions | MissingArgument | '{0}' needs an argument for '{1}'. |
| DreamShader.Binder.Expressions | NamedOnly | '{0}.{1}' takes named arguments: write 'Pin = value'. |
| DreamShader.Binder.Expressions | NamespaceAsValue | '{0}' is a namespace, not a value; write '{0}.SomeNode(...)'. |
| DreamShader.Binder.Expressions | NoConversion | {0} expects {1}, and this is {2}. |
| DreamShader.Binder.Expressions | NodeNeedsNamedOutput | {0} expects {1}, and '{2}' has more than one output; name the one you mean: {3}. |
| DreamShader.Binder.Expressions | NodeNeedsOutput | {0} expects {1}, and this node has more than one output; name the one you mean. |
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
| DreamShader.Binder.Expressions | PinOf | The '{0}' pin of '{1}.{2}' |
| DreamShader.Binder.Expressions | PinTwice | '{0}' is connected twice in this call. |
| DreamShader.Binder.Expressions | PropertyNotConstant | '{0}' is written into the node itself, not connected to it, so its value has to be known at compile time. |
| DreamShader.Binder.Expressions | PropertyNotSpelling | '{0}' takes a name or an asset path; write it as a quoted string. |
| DreamShader.Binder.Expressions | PropertyTwice | '{0}' is set twice in this call. |
| DreamShader.Binder.Expressions | ReflectedNotCalled | '{0}.{1}' is a node and has to be called: write '{0}.{1}(...)'. |
| DreamShader.Binder.Expressions | RequiredPin | '{0}.{1}' needs its '{2}' pin connected. |
| DreamShader.Binder.Expressions | RequiredPinOrConst | '{0}.{1}' needs its '{2}' pin connected, or '{3}' set to a literal. |
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
| DreamShader.CustomHlsl | CustomBadFunctionIndex | Custom node code was asked for function {0}, but the bound module has {1}. |
| DreamShader.CustomHlsl | CustomCallArity | '{0}' takes {1} argument(s) but this call passes {2}; a call that carries a texture cannot be matched up by position otherwise. |
| DreamShader.CustomHlsl | CustomCallCycle | The '@custom' functions {0} call each other in a cycle; HLSL has no recursion, so their bodies cannot be embedded in a custom node. |
| DreamShader.CustomHlsl | CustomCallsGraphFunction | '{0}' is not a '@custom' function and cannot be called from the HLSL body of '{1}'; a custom node sees no graph values, so mark '{0}' '@custom' as well or move the call out of the body. |
| DreamShader.CustomHlsl | CustomCaseOnlyMatch | '{0}' differs from the function '{1}' only in case; HLSL is case-sensitive, so this call is left for the shader compiler. Did you mean '{1}'? |
| DreamShader.CustomHlsl | CustomDuplicateParameterName | '{0}' declares '{1}' twice in the HLSL it generates; a texture parameter also claims '{1}Sampler', which the engine declares alongside it. |
| DreamShader.CustomHlsl | CustomEmptyInclude | '{0}' has an '#include' with an empty path; write the virtual shader path the header lives at, for example "/Engine/Private/Common.ush". |
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
| DreamShader.Decompiler.Service | DecompileDidNotProduceSourceText | Decompile did not produce source text. |
| DreamShader.Decompiler.Service | FailedToCreateOutputDirectory | DreamShader failed to create output directory '{0}'. |
| DreamShader.Decompiler.Service | FailedToResolveOutputFilePath | DreamShader failed to resolve an output file path. |
| DreamShader.Decompiler.Service | FailedToWriteDecompiledSource | DreamShader failed to write decompiled source '{0}'. |
| DreamShader.Decompiler.Service | NoAssetProvided | No asset was provided. |
| DreamShader.Decompiler.Service | UnsupportedAssetType | DreamShader decompile supports Material and MaterialFunction assets only: {0} |
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
| DreamShader.Emitter | CreateThinBaseFailed | The hidden base material for '{0}' could not be created. {1} |
| DreamShader.Emitter | CustomBadAdditionalOutput | '{0}' is not a Custom node additional output; the emitter expects Name:Type. |
| DreamShader.Emitter | CustomBadAdditionalOutputType | '{0}' is not a Custom node output type; expected Float1 through Float4 or MaterialAttributes. |
| DreamShader.Emitter | CustomBadOutputType | '{0}' is not a Custom node output type; expected Float1 through Float4 or MaterialAttributes. |
| DreamShader.Emitter | CustomFailed | Failed to create a Custom node. |
| DreamShader.Emitter | DeferredToWriteOwner | '{0}' was left alone: another editor owns writing this project's generated assets to disk. |
| DreamShader.Emitter | DestinationFailed | '{0}' does not resolve to a valid asset path. {1} |
| DreamShader.Emitter | FunctionCallAssignFailed | '{0}' could not be assigned to the generated call node; a material function cannot call itself, directly or through another function. |
| DreamShader.Emitter | FunctionCallFailed | Failed to create a MaterialFunctionCall node. |
| DreamShader.Emitter | FunctionCallLoadFailed | The material function asset '{0}' could not be loaded. |
| DreamShader.Emitter | FunctionCallNoPath | This function call names no material function asset. |
| DreamShader.Emitter | FunctionCallUnknownInput | '{0}' has no input named '{1}'. |
| DreamShader.Emitter | FunctionCallUnknownOutput | '{0}' has no output named '{1}'. |
| DreamShader.Emitter | FunctionInputFailed | Failed to create a FunctionInput node. |
| DreamShader.Emitter | FunctionOutputArity | Function output '{0}' has {1} operands; it needs exactly the one value it returns. |
| DreamShader.Emitter | FunctionOutputFailed | Failed to create a FunctionOutput node. |
| DreamShader.Emitter | FwidthFailed | Failed to create the nodes fwidth lowers to. |
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
| DreamShader.Emitter | ScalarParamFailed | Failed to create a ScalarParameter node. |
| DreamShader.Emitter | SelectArity | A Select node takes three operands (condition, then, else); this one has {0}. |
| DreamShader.Emitter | SelectFailed | Failed to create the If node a select lowers to. |
| DreamShader.Emitter | SetAttributesConnectFailed | SetMaterialAttributes could not take a value for the attribute '{0}'. |
| DreamShader.Emitter | SetAttributesFailed | Failed to create a SetMaterialAttributes node. |
| DreamShader.Emitter | SettingsRefused | A material setting on '{0}' was refused. {1} |
| DreamShader.Emitter | SinkNoPropertyInput | This material has no input for the attribute '{0}'; check the material domain and shading model the file asks for. |
| DreamShader.Emitter | SourceHashCurrent | '{0}' was left alone: its source hash is unchanged since it was last built. |
| DreamShader.Emitter | StaticBoolParamFailed | Failed to create a StaticBoolParameter node. |
| DreamShader.Emitter | StaticSwitchArity | A StaticSwitch node takes three operands (condition, then, else); this one has {0}. |
| DreamShader.Emitter | StaticSwitchFailed | Failed to create a StaticSwitch node. |
| DreamShader.Emitter | SwizzleBadMask | '{0}' is not a canonical swizzle mask; the emitter expects a subset of xyzw in order. |
| DreamShader.Emitter | SwizzleFailed | Failed to create a ComponentMask node. |
| DreamShader.Emitter | SwizzleNoMask | A Swizzle node carries no Mask property. |
| DreamShader.Emitter | TextureDefaultMissing | The default texture for parameter '{0}' could not be loaded from '{1}'. |
| DreamShader.Emitter | TextureParamFailed | Failed to create a TextureObjectParameter node. |
| DreamShader.Emitter | TextureSampleFailed | Failed to create a TextureSample node. |
| DreamShader.Emitter | UnhandledOp | The emitter has no rule for the IR operation '{0}'. |
| DreamShader.Emitter | UnknownAttribute | '{0}' is not a material attribute this engine has. |
| DreamShader.Emitter | UnknownExpressionClass | '{0}' is not a material expression class this engine has. |
| DreamShader.Emitter | UnknownPin | '{0}' has no input pin named '{1}'. |
| DreamShader.Emitter | UnknownProductKind | '{0}' has a product kind the emitter does not know how to materialize. |
| DreamShader.Emitter | ValueNotEmitted | This node reads node {0}, which has not been emitted; the graph's topological order is inconsistent. |
| DreamShader.Emitter | VectorParamFailed | Failed to create a VectorParameter node. |
| DreamShader.Generator | BuildingMaterialGraph | Building material graph for '{0}'... |
| DreamShader.Generator | BuildingThinCustomGraph | Building the base material graph for '{0}'... |
| DreamShader.Generator | ClearingOldFunctionGraph | Clearing old function graph '{0}'... |
| DreamShader.Generator | CompilingDreamShaderSource | Compiling DreamShader source '{0}'... |
| DreamShader.Generator | CompilingMaterial | Compiling material '{0}'... |
| DreamShader.Generator | CompilingThinCustomShaders | Compiling shaders for '{0}'... |
| DreamShader.Generator | ConnectingFunctionOutputs | Connecting outputs for '{0}'... |
| DreamShader.Generator | ConnectingMaterialOutputs | Connecting material outputs for '{0}'... |
| DreamShader.Generator | CreatingFunctionGraphNodes | Creating Graph nodes for '{0}'... |
| DreamShader.Generator | CreatingInputsForFunction | Creating inputs for '{0}'... |
| DreamShader.Generator | CreatingMaterialCustomNode | Creating Custom node for '{0}'... |
| DreamShader.Generator | CreatingMaterialGraphNodes | Creating Graph nodes for '{0}'... |
| DreamShader.Generator | FinishingDreamShaderCompile | Finishing DreamShader compile... |
| DreamShader.Generator | GeneratingDreamShaderFunction | Generating DreamShader function '{0}'... |
| DreamShader.Generator | GeneratingDreamShaderFunctionAssets | Generating {0} DreamShader function asset{1}... |
| DreamShader.Generator | GeneratingDreamShaderMaterial | Generating DreamShader material '{0}'... |
| DreamShader.Generator | GeneratingDreamShaderMaterialFromSource | Generating DreamShader material from '{0}'... |
| DreamShader.Generator | GeneratingThinCustomMaterial | Generating thin-custom material for '{0}'... |
| DreamShader.Generator | GeneratingThinCustomStages | Emitting thin-custom material '{0}'... |
| DreamShader.Generator | LayingOutFunction | Laying out '{0}'... |
| DreamShader.Generator | LayingOutMaterialGraph | Laying out material graph '{0}'... |
| DreamShader.Generator | ParsingDreamShaderSource | Parsing DreamShader source '{0}'... |
| DreamShader.Generator | ParsingFunctionGraphBlock | Parsing Graph block for '{0}'... |
| DreamShader.Generator | ParsingMaterialGraphBlock | Parsing Graph block for '{0}'... |
| DreamShader.Generator | ParsingMaterialSource | Parsing material source '{0}'... |
| DreamShader.Generator | PreparingDreamShaderGeneratedAssets | Preparing DreamShader generated assets... |
| DreamShader.Generator | PreparingMaterialAsset | Preparing material asset '{0}'... |
| DreamShader.Generator | ProgressStageGraph | Step 1 of 2, building the graph: {0} |
| DreamShader.Generator | ProgressStageShaders | Step 2 of 2, compiling shaders (this can take minutes): {0} |
| DreamShader.Generator | ReadingDreamShaderSource | Reading DreamShader source '{0}'... |
| DreamShader.Generator | ReadingMaterialSource | Reading material source '{0}'... |
| DreamShader.Generator | SavingFunction | Saving '{0}'... |
| DreamShader.Generator | SavingMaterial | Saving material '{0}'... |
| DreamShader.Generator | UpdatingFunction | Updating '{0}'... |
| DreamShader.Generator | ValidatingDreamShaderFunction | Validating {0} '{1}'... |
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
| DreamShader.IR.Validator | LayoutHintKind | Layout hint {0} of product {1} has kind '{2}'; a hint is spelled exactly 'Node' or 'Comment', so this one places nothing. |
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
| DreamShader.IRBuilder | IRBuilderOneArmedMaterialWrite | '{0}' is set in only one arm of this 'if' and has no value before it; set it in both arms, or before the 'if'. |
| DreamShader.IRBuilder | IRBuilderOneArmedWrite | '{0}' is assigned in only one arm of this 'if' and has no value before it; assign it in both arms, or give it a value before the 'if'. |
| DreamShader.IRBuilder | IRBuilderOutArgNotLValue | '{0}' is an '{1}' parameter of {2}, so the argument has to be something that can be assigned to; this expression cannot. |
| DreamShader.IRBuilder | IRBuilderPropertyNotLiteral | '{0}' is a property of {1} and needs a literal; this argument is computed at run time. |
| DreamShader.IRBuilder | IRBuilderReadNoAttributeEntry | The material attribute table has no entry {0}; the module was bound against a different catalog than this build is reading. |
| DreamShader.IRBuilder | IRBuilderRecursion | '{0}' calls itself, and an inlined function has no stack to recurse on; rewrite it as a loop with a constant trip count, or as a '/// @custom' function. |
| DreamShader.IRBuilder | IRBuilderRecursionCycle | '{0}' is already being inlined further up this call chain; an inlined function cannot call back into itself. |
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
| DreamShader.Lang.Declarations | LegacyDeclarationNotYet | '{0}' is a 1.x declaration; the 2.0 front end does not parse it yet. Keep it in a .dsm/.dsf/.dsh compiled by the 1.x front end. |
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
| DreamShader.Lang.Declarations | PragmaWithoutName | '#pragma' needs a name: material, layout, region or endregion. |
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
| DreamShader.Lang.Parser | LegacyFrontendUnavailable | The 1.x front end is not available in this build. |
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
| DreamShader.Parser | AFunctionWithAReturnType | A function with a return type cannot use a bare 'return;'. Return a value, e.g. 'return expr;'. |
| DreamShader.Parser | ATopLevelShaderFunctionGraphFunction | A top-level Shader, Function, GraphFunction, Namespace, ShaderFunction, ShaderLayer, ShaderLayerBlend, or VirtualFunction block was not found. |
| DreamShader.Parser | ExpectedCNearIndexD | Expected '{0}' near index {1}. |
| DreamShader.Parser | FunctionMissingName | Function declaration is missing a valid function name. |
| DreamShader.Parser | FunctionMissingNameAfterSelfContained | Function declaration is missing a valid function name after SelfContained. |
| DreamShader.Parser | FunctionSHasAnInvalidParameter | Function '{0}' has an invalid parameter declaration '{1}'. |
| DreamShader.Parser | FunctionSHasAnInvalidParameter2 | Function '{0}' has an invalid parameter declaration '{1}'. |
| DreamShader.Parser | FunctionSHasAnInvalidReturn | Function '{0}' has an invalid return type '{1}'. |
| DreamShader.Parser | FunctionSHasAReturnType | Function '{0}' has a return type and cannot also declare out parameters. Use out parameters without a return type for multiple outputs. |
| DreamShader.Parser | FunctionSMustDeclareAtLeast | Function '{0}' must declare at least one out parameter. |
| DreamShader.Parser | FunctionSParameterNameReturnIs | Function '{0}' parameter name '__return' is reserved for return-type lowering. |
| DreamShader.Parser | FunctionSParameterSUsesUnsupported | Function '{0}' parameter '{1}' uses unsupported qualifier '{2}'. Supported qualifiers are in and out. |
| DreamShader.Parser | GraphFunctionMissingName | GraphFunction declaration is missing a valid function name. |
| DreamShader.Parser | MaterialFunctionNameRequired | {0}(Name="...") is required. |
| DreamShader.Parser | NamespaceNameEmpty | Namespace name cannot be empty. |
| DreamShader.Parser | NamespaceNameRequired | Namespace(Name="...") is required. |
| DreamShader.Parser | NamespaceNameSIsNotA | Namespace name '{0}' is not a valid identifier. |
| DreamShader.Parser | NamespaceSMayOnlyContainFunction | Namespace '{0}' may only contain Function or GraphFunction blocks. |
| DreamShader.Parser | OnlyOneTopLevelShaderBlock | Only one top-level Shader block is currently supported. |
| DreamShader.Parser | SDeclarationIsMissingAFunction | {0} declaration is missing a function name after the return type '{1}'. |
| DreamShader.Parser | ShaderMustProvideAGraphBlock | Shader must provide a Graph block. |
| DreamShader.Parser | ShaderNameRequired | Shader(Name="...") is required. |
| DreamShader.Parser | SSIsMissingAValid | {0} '{1}' is missing a valid parameter list. {2} |
| DreamShader.Parser | SSIsMissingAValid2 | {0} '{1}' is missing a valid body block. {2} |
| DreamShader.Parser | UnexpectedTokenNearIndex | Unexpected token near index {0}. |
| DreamShader.Parser | UnterminatedCBlock | Unterminated '{0}' block. |
| DreamShader.Parser | VirtualFunctionMustDeclareAtLeastOneOutput | VirtualFunction '{0}' must declare at least one output. |
| DreamShader.Parser | VirtualFunctionMustProvideOptionsAsset | VirtualFunction '{0}' must provide Options = {{ Asset = Path(...); }}. |
| DreamShader.Parser | VirtualFunctionNameCannotBeEmpty | VirtualFunction name cannot be empty. |
| DreamShader.Parser | VirtualFunctionNameRequired | VirtualFunction(Name="...") is required. |
| DreamShader.Parser.Scanner | ExpectedCNearIndexD | Expected '{0}' near index {1}. |
| DreamShader.Parser.Scanner | ExpectedCurlyNearIndex | Expected '{{' near index {0}. |
| DreamShader.Parser.Scanner | ExpectedIdentifierNearIndexD | Expected identifier near index {0}. |
| DreamShader.Parser.Scanner | ExpectedOrNearIndexD | Expected ',' or ')' near index {0}. |
| DreamShader.Parser.Scanner | ExpectedValueNearIndexD | Expected value near index {0}. |
| DreamShader.Parser.Scanner | InvalidTextureAssetPathS | Invalid texture asset path '{0}'. |
| DreamShader.Parser.Scanner | RelativeTexturePathReferencesRequireA | Relative texture Path(...) references require a root such as Game, Engine, or Plugin.PluginName. |
| DreamShader.Parser.Scanner | TextureDefaultsMustUsePath | Texture defaults must use Path(Game\|Engine\|Plugin.PluginName, "Folder/Asset"), Path("/Game/Folder/Asset"), a bare "/Game/Folder/Asset", or a Class'/Game/Folder/Asset.Asset' reference. |
| DreamShader.Parser.Scanner | TexturePathRequiresANonEmpty | Texture Path(...) requires a non-empty asset path. |
| DreamShader.Parser.Scanner | TexturePathRootSHasAn | Texture Path root '{0}' has an invalid plugin name. |
| DreamShader.Parser.Scanner | TexturePathRootSReferencesPlugin | Texture Path root '{0}' references plugin '{1}', but no enabled plugin with that name was found. |
| DreamShader.Parser.Scanner | TexturePathRootSReferencesPlugin2 | Texture Path root '{0}' references plugin '{1}', but the plugin is not enabled. |
| DreamShader.Parser.Scanner | TexturePathRootSReferencesPlugin3 | Texture Path root '{0}' references plugin '{1}', but the plugin cannot contain content. |
| DreamShader.Parser.Scanner | TextureReferenceClassNotATexture | Asset reference is written as '{0}', which is not a texture class; a texture default requires {1}. |
| DreamShader.Parser.Scanner | TextureReferenceClassWrongDimension | Asset reference is written as '{0}', but this property is declared as {1}. |
| DreamShader.Parser.Scanner | UnexpectedTrailingTokensAfterTexturePath | Unexpected trailing tokens after texture Path(...) reference. |
| DreamShader.Parser.Scanner | UnsupportedTexturePathRootSUse | Unsupported texture Path root '{0}'. Use Game, Engine, or Plugin.PluginName. |
| DreamShader.Parser.Scanner | UnterminatedBlock | Unterminated block. |
| DreamShader.Parser.Scanner | UnterminatedStringLiteral | Unterminated string literal. |
| DreamShader.Parser.Sections | ExpressionBlockBindsNoPin | The Expression(...) block for '{0}' binds no pin. Write at least one Pin[index] = <source>; inside it, or delete the block. |
| DreamShader.Parser.Sections | ExpressionOutputTargetArgumentSIs | Expression output target argument '{0}' is declared more than once. |
| DreamShader.Parser.Sections | ExpressionOutputTargetArgumentSMust | Expression output target argument '{0}' must use Key=Value syntax. |
| DreamShader.Parser.Sections | ExpressionOutputTargetSHasAn | Expression output target '{0}' has an invalid pin index. |
| DreamShader.Parser.Sections | ExpressionOutputTargetSMustSelect | Expression output target '{0}' must select a pin with .Pin[index]. |
| DreamShader.Parser.Sections | ExpressionOutputTargetSMustSpecify | Expression output target '{0}' must specify Class=\"...\". |
| DreamShader.Parser.Sections | ExpressionOutputTargetSMustUse | Expression output target '{0}' must use .Pin[index] syntax. |
| DreamShader.Parser.Sections | GraphEndRegionOnLineDHas | Graph #EndRegion on line {0} has no matching #Region. |
| DreamShader.Parser.Sections | GraphRegionOnLineDMust | Graph #Region on line {0} must include a name. |
| DreamShader.Parser.Sections | GraphRegionSIsMissingEndRegion | Graph #Region '{0}' is missing #EndRegion. |
| DreamShader.Parser.Sections | GroupRequiresANonEmptyName | Group(...) requires a non-empty name. |
| DreamShader.Parser.Sections | InvalidBooleanDefaultValueSFor | Invalid boolean default value '{0}' for property '{1}'. |
| DreamShader.Parser.Sections | InvalidEmptySettingKeyInS | Invalid empty setting key in '{0}'. |
| DreamShader.Parser.Sections | InvalidExpressionOutputTargetArgumentS | Invalid expression output target argument '{0}'. |
| DreamShader.Parser.Sections | InvalidLayoutArgumentS | Invalid Layout argument '{0}'. |
| DreamShader.Parser.Sections | InvalidLayoutCommentStatementSS | Invalid Layout Comment statement '{0}'. {1} |
| DreamShader.Parser.Sections | InvalidLayoutNodeStatementSS | Invalid Layout Node statement '{0}'. {1} |
| DreamShader.Parser.Sections | InvalidLayoutStatementNameInS | Invalid Layout statement name in '{0}'. |
| DreamShader.Parser.Sections | InvalidLayoutStatementS | Invalid Layout statement '{0}'. |
| DreamShader.Parser.Sections | InvalidMetadataEntryS | Invalid metadata entry '{0}'. |
| DreamShader.Parser.Sections | InvalidOutputBindingS | Invalid output binding '{0}'. |
| DreamShader.Parser.Sections | InvalidOutputDeclarationInitializerS | Invalid output declaration initializer '{0}'. |
| DreamShader.Parser.Sections | InvalidOutputExpressionTargetS | Invalid output expression target '{0}'. |
| DreamShader.Parser.Sections | InvalidPropertyDeclarationS | Invalid property declaration '{0}'. |
| DreamShader.Parser.Sections | InvalidScalarDefaultValueSFor | Invalid scalar default value '{0}' for property '{1}'. |
| DreamShader.Parser.Sections | InvalidScalarDefaultValueSFor2 | Invalid scalar default value '{0}' for property '{1}'. |
| DreamShader.Parser.Sections | InvalidSettingDeclarationS | Invalid setting declaration '{0}'. |
| DreamShader.Parser.Sections | InvalidStatementInExpressionBlock | Invalid statement '{0}' inside an Expression(...) block. Only Pin[index] = <source>; is allowed there. |
| DreamShader.Parser.Sections | InvalidTextureDefaultValueSFor | Invalid texture default value '{0}' for property '{1}'. {2} |
| DreamShader.Parser.Sections | InvalidTextureDefaultValueSFor2 | Invalid texture default value '{0}' for property '{1}'. {2} |
| DreamShader.Parser.Sections | InvalidTextureSampleDefaultValueS | Invalid texture sample default value '{0}' for property '{1}'. {2} |
| DreamShader.Parser.Sections | InvalidTypedDeclarationS | Invalid typed declaration '{0}'. |
| DreamShader.Parser.Sections | InvalidTypedDeclarationS2 | Invalid typed declaration '{0}'. |
| DreamShader.Parser.Sections | InvalidTypedDeclarationS3 | Invalid typed declaration '{0}'. |
| DreamShader.Parser.Sections | InvalidUEBuiltinArgumentSIn | Invalid UE builtin argument '{0}' in '{1}'. |
| DreamShader.Parser.Sections | InvalidUEBuiltinDeclarationS | Invalid UE builtin declaration '{0}'. |
| DreamShader.Parser.Sections | InvalidUEBuiltinDeclarationS2 | Invalid UE builtin declaration '{0}'. |
| DreamShader.Parser.Sections | InvalidVectorDefaultValueSFor | Invalid vector default value '{0}' for property '{1}'. |
| DreamShader.Parser.Sections | InvalidVectorDefaultValueSFor2 | Invalid vector default value '{0}' for property '{1}'. |
| DreamShader.Parser.Sections | LayoutArgumentSIsDeclaredMore | Layout argument '{0}' is declared more than once. |
| DreamShader.Parser.Sections | LayoutArgumentSIsRequired | Layout argument '{0}' is required. |
| DreamShader.Parser.Sections | LayoutArgumentSMustBeAn | Layout argument '{0}' must be an integer. |
| DreamShader.Parser.Sections | LayoutArgumentSMustUseKey | Layout argument '{0}' must use Key=Value syntax. |
| DreamShader.Parser.Sections | LayoutCommentColorMustBeA | Layout Comment Color must be a float4 literal in '{0}'. |
| DreamShader.Parser.Sections | MaterialFunctionGraphCodeDeprecated | ShaderFunction, ShaderLayer, and ShaderLayerBlend graph sections now use Graph = { ... }. Function Code = { ... } is still supported. |
| DreamShader.Parser.Sections | MetadataEntrySMustUseKey | Metadata entry '{0}' must use Key=Value syntax. |
| DreamShader.Parser.Sections | MetadataKeySIsDeclaredMore | Metadata key '{0}' is declared more than once. |
| DreamShader.Parser.Sections | MetadataMustFollowADeclaration | Metadata must follow a declaration. |
| DreamShader.Parser.Sections | MetadataSliderMinMaxRequiresExactly | Metadata 'Slider(min, max)' requires exactly two numeric bounds: '{0}'. |
| DreamShader.Parser.Sections | MetadataSliderMinSliderMaxIsDeclaredMore | Metadata SliderMin/SliderMax is declared more than once (entry '{0}'). |
| DreamShader.Parser.Sections | MetadataSortPriorityValueSIsNot | Metadata SortPriority value '{0}' is not an integer. |
| DreamShader.Parser.Sections | MissingPropertyNameInDeclarationS | Missing property name in declaration '{0}'. |
| DreamShader.Parser.Sections | MissingPropertyTypeAfterConstIn | Missing property type after const in declaration '{0}'. |
| DreamShader.Parser.Sections | OutputBindingTargetCannotBeEmpty | Output binding target cannot be empty. |
| DreamShader.Parser.Sections | OutputBindingTargetSIsEmpty | Output binding target '{0}' is empty. |
| DreamShader.Parser.Sections | OutputBindingTargetSMustStart | Output binding target '{0}' must start with Base. for material outputs or Expression(...) for output nodes. |
| DreamShader.Parser.Sections | OutputTargetPinBoundMoreThanOnce | Output target pin '{0}' is bound more than once: first to '{1}', then to '{2}'. An Expression(...) block and an Expression(...).Pin[i] statement that share a class and argument list describe one node, so their pins share one namespace. |
| DreamShader.Parser.Sections | ParameterNodeTypeSIsRecognized | Parameter node type '{0}' is recognized but not supported as a plain Properties declaration yet. Use UE.{1}(OutputType=\"float4\", ...) for reflected node creation. |
| DreamShader.Parser.Sections | ShaderGraphCodeDeprecated | Shader graph sections now use Graph = { ... }. Function Code = { ... } is still supported. |
| DreamShader.Parser.Sections | UEBuiltinArgumentSIsDeclared | UE builtin argument '{0}' is declared more than once in '{1}'. |
| DreamShader.Parser.Sections | UEBuiltinArgumentSMustUse | UE builtin argument '{0}' must use named syntax like Key=Value in '{1}'. |
| DreamShader.Parser.Sections | UEBuiltinPropertyDeclarationsMustSpecify | UE builtin property declarations must specify a function name, for example UE.TexCoord UV. |
| DreamShader.Parser.Sections | UEBuiltinPropertySDoesNot | UE builtin property '{0}' does not support inline defaults. Put arguments inside UE.{1}(...). |
| DreamShader.Parser.Sections | UnexpectedBraceBlockInOutputs | Unexpected brace block in Outputs near '{0}'. Only Expression(Class="...") opens a brace block here; every other Outputs statement ends with ';'. |
| DreamShader.Parser.Sections | UnexpectedCharactersAfterUEBuiltinArgument | Unexpected characters after UE builtin argument list in '{0}'. |
| DreamShader.Parser.Sections | UnexpectedInPropertiesNearSOnly | Unexpected '`{' in Properties near '{0}'. Only Group("Name") `{ ... `} may open a brace here. |
| DreamShader.Parser.Sections | UnexpectedTextAfterLayoutStatementS | Unexpected text after Layout statement '{0}'. |
| DreamShader.Parser.Sections | UnknownLayoutStatementS | Unknown Layout statement '{0}'. |
| DreamShader.Parser.Sections | UnknownMaterialFunctionSectionS | Unknown material function section '{0}'. |
| DreamShader.Parser.Sections | UnknownShaderFunctionSectionS | Unknown shader function section '{0}'. |
| DreamShader.Parser.Sections | UnknownShaderSectionS | Unknown shader section '{0}'. |
| DreamShader.Parser.Sections | UnknownVirtualFunctionSectionS | Unknown VirtualFunction section '{0}'. |
| DreamShader.Parser.Sections | UnsupportedOutputTargetS | Unsupported output target '{0}'. |
| DreamShader.Parser.Sections | UnsupportedPropertyTypeS | Unsupported property type '{0}'. |
| DreamShader.Parser.Sections | UnsupportedUEBuiltinFunctionSUse | Unsupported UE builtin function '{0}'. Use OutputType=\"float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture\" for generic MaterialExpression calls. |
| DreamShader.Parser.Sections | UnterminatedGroupBlock | Unterminated Group("{0}") `{ ... `} block. |
| DreamShader.Parser.Sections | UnterminatedOutputsExpressionBlock | Unterminated Expression(...) block in Outputs after '{0}'. |
| DreamShader.Parser.Sections | VirtualFunctionNoGraphOrCode | VirtualFunction declares an existing MaterialFunction asset and does not support Graph or Code sections. |
| DreamShader.Pipeline | CompileCancelled | Compiling '{0}' was cancelled; nothing was written. |
| DreamShader.Pipeline | CompilingLang2Source | Compiling DreamShader source '{0}'... |
| DreamShader.Pipeline | IncludePreprocessFailed | '{0}' failed conditional compilation: {1}: {2} |
| DreamShader.Pipeline | IncludeUnparsable | '{0}', included from '{1}', could not be parsed; its own errors are above. |
| DreamShader.Pipeline | IncludeUnreadable | '{0}', included from '{1}', resolved but could not be read. |
| DreamShader.Pipeline | IncludeUnresolved | '{0}', included from '{1}', could not be resolved: {2}. |
| DreamShader.Pipeline | IncludeWrongKind | '{0}' is not a DreamShader header; an include names a '.dsh' (or a '.dss'), not a '{1}' file. |
| DreamShader.Pipeline | Lang2Binding | Resolving names in '{0}'... |
| DreamShader.Pipeline | Lang2Emitting | Building the graph for '{0}'... |
| DreamShader.Pipeline | Lang2Lowering | Lowering '{0}' to IR... |
| DreamShader.Pipeline | Lang2Parsing | Parsing '{0}'... |
| DreamShader.Pipeline | Lang2Reading | Reading '{0}'... |
| DreamShader.Pipeline | Lang2Validating | Validating the IR of '{0}'... |
| DreamShader.Pipeline | NotALang2Source | '{0}' is not a 2.0 source; the 2.0 pipeline compiles '.dss' files, and a '.dsh' header is compiled only through the '.dss' that includes it. |
| DreamShader.Pipeline | ProductCycle | The exported functions {0} call one another in a cycle, so there is no order in which they can be built; an exported function may call another only in one direction. |
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
| DreamShader.Preview | PreviewSourceMissing | DreamShader source '{0}' does not exist. |
| DreamShader.Preview | PreviewSupportsOnlyDsm | DreamShader preview only supports .dsm material files: '{0}'. |
| DreamShader.Preview | RenderedPreview | Rendered preview for {0}. |
| DreamShader.Tools | DiagnosticsOutFailed | The diagnostics JSON could not be written: {0}. |
| DreamShader.Tools | DumpIRWriteFailed | The IR dump could not be written: {0}. |
| DreamShader.Tools | ExportCatalogEmpty | The builtin catalog came back empty, so '{0}' describes no expression at all. Reflection found no UMaterialExpression classes, which normally means the Engine module is not loaded. |
| DreamShader.Tools | ExportCatalogWriteFailed | The builtin catalog manifest could not be written: {0}. |
| DreamShader.Tools | IndexWriteFailed | The symbol index could not be written: {0}. |
| DreamShader.Tools | NoMaterialToCheck | '{0}' produced no material, so there are no shaders to compile; a function library is checked by the material that calls it. |
| DreamShader.Tools | NotALang2SourceForVerb | '{0}' is not a 2.0 source, so '{1}' has nothing to do with it; '.dsm' and '.dsf' stay on 'compile'. |
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
| DreamShaderEditor.Settings | SectionDescription | Dream Shader Settings |
| DreamShaderEditor.Settings | SectionText | Dream Shader |
| DreamShaderEditorBridge | DreamShaderAdoptBackupFailed | Could not back up '{0}' to '{1}'; nothing was written. |
| DreamShaderEditorBridge | DreamShaderAdoptConditionalSource | DSH8149: '{0}' uses conditional compilation, and '{1}' holds only the branch that was taken -- adopting it would write that one branch back over the file and delete the rest. Move the change into the matching branch of the source by hand, or use DreamShader > Detach first if this asset should stop being generated from it. |
| DreamShaderEditorBridge | DreamShaderAdoptConfirm | Rewrite '{0}' from the current contents of '{1}'?\n\nThe existing source is copied to '{2}' first. The rewritten file is the decompiler's own form, so hand-written comments, imports and formatting in it are replaced. |
| DreamShaderEditorBridge | DreamShaderAdoptInstanceNoBase | '{0}' has no base material to decompile. |
| DreamShaderEditorBridge | DreamShaderAdoptInstanceOverrides | '{0}' has parameter overrides set on the generated instance, and those cannot be written back into '{1}' -- adopting would drop them. Move the values into the source as Properties defaults (or override them on a child material instance instead), then Revert. |
| DreamShaderEditorBridge | DreamShaderAdoptLabel | Adopt Into Source |
| DreamShaderEditorBridge | DreamShaderAdoptMultiAsset | '{0}' declares {1} assets, so adopting one of them would overwrite the others. Use DreamShader > Export DSM and merge the result by hand. |
| DreamShaderEditorBridge | DreamShaderAdoptResult | Adopted '{0}' into '{1}' (backup: '{2}'). {3} |
| DreamShaderEditorBridge | DreamShaderAdoptSourceUnreadable | Could not read '{0}'. |
| DreamShaderEditorBridge | DreamShaderAdoptTooltip | Rewrite the DreamShader source file from this asset's current contents, so your hand edits become the source of truth. The previous source is backed up alongside it. |
| DreamShaderEditorBridge | DreamShaderCleanGeneratedShadersLabel | Clean Generated Shaders |
| DreamShaderEditorBridge | DreamShaderCleanGeneratedShadersTooltip | Delete Intermediate/DreamShader/GeneratedShaders and queue a full DreamShader recompile. |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionCallLabel | CopyVirtualFunctionCall |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionCallNoAsset | DreamShader could not find the selected Material Function. |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionCallTooltip | Copy a DreamShader Graph call example for this VirtualFunction. |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionLabel | CopyVirtualFunction |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionNoAsset | DreamShader could not find the selected Material Function. |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionReferenceLabel | Copy Virtual Function Reference |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionReferenceNoAsset | DreamShader could not find the selected Material Function. |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionReferenceTooltip | Copy a DreamShader Graph call that references this existing VirtualFunction. |
| DreamShaderEditorBridge | DreamShaderCopyVirtualFunctionTooltip | Copy a complete DreamShader VirtualFunction declaration for this Material Function. |
| DreamShaderEditorBridge | DreamShaderCreateVirtualFunctionLabel | CreateVirtualFunction |
| DreamShaderEditorBridge | DreamShaderCreateVirtualFunctionNoAsset | DreamShader could not find the selected Material Function. |
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
| DreamShaderEditorBridge | DreamShaderExportFunctionDSFLabel | Export DSF |
| DreamShaderEditorBridge | DreamShaderExportFunctionDSFTooltip | Export this Material Function graph to a DreamShader .dsf source file. |
| DreamShaderEditorBridge | DreamShaderExportFunctionNoAsset | DreamShader could not find the selected Material Function. |
| DreamShaderEditorBridge | DreamShaderExportMaterialDSMLabel | Export DSM |
| DreamShaderEditorBridge | DreamShaderExportMaterialDSMTooltip | Export this Material graph to a DreamShader .dsm source file. |
| DreamShaderEditorBridge | DreamShaderExportMaterialNoAsset | DreamShader could not find the selected Material. |
| DreamShaderEditorBridge | DreamShaderFunctionDecompileActionsSection | Decompiler |
| DreamShaderEditorBridge | DreamShaderFunctionProvenanceActionsSection | Generated Asset |
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
| DreamShaderEditorBridge | DreamShaderProvenanceNoAsset | DreamShader could not find the selected asset. |
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
| DreamShaderEditorBridge | DreamShaderVirtualFunctionActionsSection | VirtualFunction |
| DreamShaderEditorBridge | DreamToolsComboLabel | Dream |
| DreamShaderEditorBridge | MaterialCompileErrorHeader | [{0} / {1}] {2} |
| DreamShaderMaterialBrowser | AdoptBtn | Adopt Into Source |
| DreamShaderMaterialBrowser | AdoptReadOnlyTip | This asset's source ships with a plugin and is read-only; adopt is not available. |
| DreamShaderMaterialBrowser | AdoptTip | Rewrite the DreamShader source file from this asset's current contents, so your hand edits become the source of truth. The previous source is backed up alongside it. |
| DreamShaderMaterialBrowser | AssetLinkTip | Show this asset in the Content list. |
| DreamShaderMaterialBrowser | AssetPathNoShaderBlock | {0}: this file does not define a top-level Shader block. |
| DreamShaderMaterialBrowser | AssetPathParseError | {0}: {1} |
| DreamShaderMaterialBrowser | AssetPathReadFailed | Failed to read DreamShader source '{0}'. |
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
| DreamShaderMaterialBrowser | CBCreateInstanceTip | Create a material instance that shares this material's compiled shader map. |
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
| DreamShaderMaterialBrowser | CompilingAll | Compiling all DreamShader sources... |
| DreamShaderMaterialBrowser | Create | Create |
| DreamShaderMaterialBrowser | CreateInstanceBtn | Create instance |
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
| DreamShaderMaterialBrowser | ExportDsfBtn | Export DSF |
| DreamShaderMaterialBrowser | ExportDsfTip | Decompile this material function into a .dsf source file under the project's DShader root. |
| DreamShaderMaterialBrowser | ExportDsmBtn | Export DSM |
| DreamShaderMaterialBrowser | ExportDsmTip | Decompile this material into a .dsm source file under the project's DShader root. |
| DreamShaderMaterialBrowser | FactoryAssetExists | An asset already exists at {0}. |
| DreamShaderMaterialBrowser | FactoryCreatePackageFailed | Failed to create package {0}. |
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
| DreamShaderMaterialBrowser | NewSourceNameLabel | Name |
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
| DreamShaderMaterialBrowser | PInstTip | Create a material instance of this material. |
| DreamShaderMaterialBrowser | POpenMatTip | Open the generated material asset. |
| DreamShaderMaterialBrowser | POpenSrc | Open source |
| DreamShaderMaterialBrowser | POpenSrcTip | Open the .dsm/.dsf in your preferred editor. |
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
| DreamShaderMaterialBrowser | ProvenanceExplainTweaked | The generated content still matches, and you have set parameter overrides on the instance. Rebuilds go ahead as normal and put your values back; a parameter the source no longer declares is dropped and named in the log. |
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
| DreamShaderVirtualFunctionSyncService | InvalidAssetReference | VirtualFunction '{0}' asset reference is invalid: {1} |
| DreamShaderVirtualFunctionSyncService | InvalidDeclaration | VirtualFunction declaration is invalid: {0} |
| DreamShaderVirtualFunctionSyncService | MissingClosingBrace | VirtualFunction body is missing a closing '}'. |
| DreamShaderVirtualFunctionSyncService | MissingClosingParenthesis | VirtualFunction attributes are missing a closing ')'. |
| DreamShaderVirtualFunctionSyncService | MissingMaterialFunction | VirtualFunction '{0}' references missing MaterialFunction '{1}'. |
| DreamShaderVirtualFunctionSyncService | ReadSourceFileFailed | DreamShader could not read VirtualFunction source file '{0}'. |
| DreamShaderVirtualFunctionSyncService | RefreshFailed | VirtualFunction '{0}' could not be refreshed from MaterialFunction '{1}': {2} |
| DreamShaderVirtualFunctionSyncService | UpdateSourceFileFailed | DreamShader failed to update VirtualFunction source file '{0}'. |

## Deferred diagnostics inventory
Deferred files: 51
Runtime FText::FromString/FText::FromName / FString::Printf literal call sites in deferred diagnostics: 0
Use -IncludeDeferred to lint MaterialAssetGeneration/ and Decompiler/ in the next phase.

## Auto-gathered metadata
Unreal will add any auto-gathered UPROPERTY metadata (for example DisplayName and ToolTip) on top of this compile-time baseline. This file intentionally counts only LOCTEXT/NSLOCTEXT entries.
