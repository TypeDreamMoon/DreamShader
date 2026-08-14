# DreamShader localization baseline

This inventory covers compile-time LOCTEXT/NSLOCTEXT entries that the Localization Dashboard gather step can see.

It spans EVERY source file, not just the ones this script lints. GatherText scans the whole
module and knows nothing about the Scope / Deferred / Allowlisted / Excluded split, so an
inventory narrower than the module would report a count no real gather ever produces. That
includes the automation tests: their LOCTEXT entries (namespace `DreamShaderTests`) are
gathered like any other, so they are listed here rather than quietly dropped.

`-IncludeDeferred` widens which files the R1/R2 literal rules run on; it does not change this count.

## Expected gather count
342

## Inventory
| Namespace | Key | Source text |
| --- | --- | --- |
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
| DreamShader.Generator | BuildingMaterialGraph | Building material graph for '{0}'... |
| DreamShader.Generator | ClearingOldFunctionGraph | Clearing old function graph '{0}'... |
| DreamShader.Generator | CompilingDreamShaderSource | Compiling DreamShader source '{0}'... |
| DreamShader.Generator | CompilingMaterial | Compiling material '{0}'... |
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
| DreamShader.Generator | LayingOutFunction | Laying out '{0}'... |
| DreamShader.Generator | LayingOutMaterialGraph | Laying out material graph '{0}'... |
| DreamShader.Generator | ParsingDreamShaderSource | Parsing DreamShader source '{0}'... |
| DreamShader.Generator | ParsingFunctionGraphBlock | Parsing Graph block for '{0}'... |
| DreamShader.Generator | ParsingMaterialGraphBlock | Parsing Graph block for '{0}'... |
| DreamShader.Generator | ParsingMaterialSource | Parsing material source '{0}'... |
| DreamShader.Generator | PreparingDreamShaderGeneratedAssets | Preparing DreamShader generated assets... |
| DreamShader.Generator | PreparingMaterialAsset | Preparing material asset '{0}'... |
| DreamShader.Generator | ReadingDreamShaderSource | Reading DreamShader source '{0}'... |
| DreamShader.Generator | ReadingMaterialSource | Reading material source '{0}'... |
| DreamShader.Generator | SavingFunction | Saving '{0}'... |
| DreamShader.Generator | SavingMaterial | Saving material '{0}'... |
| DreamShader.Generator | UpdatingFunction | Updating '{0}'... |
| DreamShader.Generator | ValidatingDreamShaderFunction | Validating {0} '{1}'... |
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
| DreamShader.Parser | MaterialFunctionNameRequired | {0}(Name=\"...\") is required. |
| DreamShader.Parser | NamespaceNameEmpty | Namespace name cannot be empty. |
| DreamShader.Parser | NamespaceNameRequired | Namespace(Name=\"...\") is required. |
| DreamShader.Parser | NamespaceNameSIsNotA | Namespace name '{0}' is not a valid identifier. |
| DreamShader.Parser | NamespaceSMayOnlyContainFunction | Namespace '{0}' may only contain Function or GraphFunction blocks. |
| DreamShader.Parser | OnlyOneTopLevelShaderBlock | Only one top-level Shader block is currently supported. |
| DreamShader.Parser | SDeclarationIsMissingAFunction | {0} declaration is missing a function name after the return type '{1}'. |
| DreamShader.Parser | ShaderMustProvideAGraphBlock | Shader must provide a Graph block. |
| DreamShader.Parser | ShaderNameRequired | Shader(Name=\"...\") is required. |
| DreamShader.Parser | SSIsMissingAValid | {0} '{1}' is missing a valid parameter list. {2} |
| DreamShader.Parser | SSIsMissingAValid2 | {0} '{1}' is missing a valid body block. {2} |
| DreamShader.Parser | UnexpectedTokenNearIndex | Unexpected token near index {0}. |
| DreamShader.Parser | UnterminatedCBlock | Unterminated '{0}' block. |
| DreamShader.Parser | VirtualFunctionMustDeclareAtLeastOneOutput | VirtualFunction '{0}' must declare at least one output. |
| DreamShader.Parser | VirtualFunctionMustProvideOptionsAsset | VirtualFunction '{0}' must provide Options = {{ Asset = Path(...); }}. |
| DreamShader.Parser | VirtualFunctionNameCannotBeEmpty | VirtualFunction name cannot be empty. |
| DreamShader.Parser | VirtualFunctionNameRequired | VirtualFunction(Name=\"...\") is required. |
| DreamShader.Parser.Scanner | ExpectedCNearIndexD | Expected '{0}' near index {1}. |
| DreamShader.Parser.Scanner | ExpectedCurlyNearIndex | Expected '{{' near index {0}. |
| DreamShader.Parser.Scanner | ExpectedIdentifierNearIndexD | Expected identifier near index {0}. |
| DreamShader.Parser.Scanner | ExpectedOrNearIndexD | Expected ',' or ')' near index {0}. |
| DreamShader.Parser.Scanner | ExpectedValueNearIndexD | Expected value near index {0}. |
| DreamShader.Parser.Scanner | InvalidTextureAssetPathS | Invalid texture asset path '{0}'. |
| DreamShader.Parser.Scanner | RelativeTexturePathReferencesRequireA | Relative texture Path(...) references require a root such as Game, Engine, or Plugin.PluginName. |
| DreamShader.Parser.Scanner | TextureDefaultsMustUsePath | Texture defaults must use Path(Game\|Engine\|Plugin.PluginName, \"/Folder/Asset\"), Path(\"/Game/Folder/Asset\"), or a bare \"/Game/Folder/Asset\". |
| DreamShader.Parser.Scanner | TexturePathRequiresANonEmpty | Texture Path(...) requires a non-empty asset path. |
| DreamShader.Parser.Scanner | TexturePathRootSHasAn | Texture Path root '{0}' has an invalid plugin name. |
| DreamShader.Parser.Scanner | TexturePathRootSReferencesPlugin | Texture Path root '{0}' references plugin '{1}', but no enabled plugin with that name was found. |
| DreamShader.Parser.Scanner | TexturePathRootSReferencesPlugin2 | Texture Path root '{0}' references plugin '{1}', but the plugin is not enabled. |
| DreamShader.Parser.Scanner | TexturePathRootSReferencesPlugin3 | Texture Path root '{0}' references plugin '{1}', but the plugin cannot contain content. |
| DreamShader.Parser.Scanner | UnexpectedTrailingTokensAfterTexturePath | Unexpected trailing tokens after texture Path(...) reference. |
| DreamShader.Parser.Scanner | UnsupportedTexturePathRootSUse | Unsupported texture Path root '{0}'. Use Game, Engine, or Plugin.PluginName. |
| DreamShader.Parser.Scanner | UnterminatedBlock | Unterminated block. |
| DreamShader.Parser.Scanner | UnterminatedStringLiteral | Unterminated string literal. |
| DreamShader.Parser.Sections | ExpressionOutputTargetArgumentSIs | Expression output target argument '{0}' is declared more than once. |
| DreamShader.Parser.Sections | ExpressionOutputTargetArgumentSMust | Expression output target argument '{0}' must use Key=Value syntax. |
| DreamShader.Parser.Sections | ExpressionOutputTargetSHasAn | Expression output target '{0}' has an invalid pin index. |
| DreamShader.Parser.Sections | ExpressionOutputTargetSMustSelect | Expression output target '{0}' must select a pin with .Pin[index]. |
| DreamShader.Parser.Sections | ExpressionOutputTargetSMustSpecify | Expression output target '{0}' must specify Class=\\\"...\\\". |
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
| DreamShader.Parser.Sections | ParameterNodeTypeSIsRecognized | Parameter node type '{0}' is recognized but not supported as a plain Properties declaration yet. Use UE.{1}(OutputType=\\\"float4\\\", ...) for reflected node creation. |
| DreamShader.Parser.Sections | ShaderGraphCodeDeprecated | Shader graph sections now use Graph = { ... }. Function Code = { ... } is still supported. |
| DreamShader.Parser.Sections | UEBuiltinArgumentSIsDeclared | UE builtin argument '{0}' is declared more than once in '{1}'. |
| DreamShader.Parser.Sections | UEBuiltinArgumentSMustUse | UE builtin argument '{0}' must use named syntax like Key=Value in '{1}'. |
| DreamShader.Parser.Sections | UEBuiltinPropertyDeclarationsMustSpecify | UE builtin property declarations must specify a function name, for example UE.TexCoord UV. |
| DreamShader.Parser.Sections | UEBuiltinPropertySDoesNot | UE builtin property '{0}' does not support inline defaults. Put arguments inside UE.{1}(...). |
| DreamShader.Parser.Sections | UnexpectedCharactersAfterUEBuiltinArgument | Unexpected characters after UE builtin argument list in '{0}'. |
| DreamShader.Parser.Sections | UnexpectedInPropertiesNearSOnly | Unexpected '{{' in Properties near '{0}'. Only Group(\"Name\") {{ ... }} may open a brace here. |
| DreamShader.Parser.Sections | UnexpectedTextAfterLayoutStatementS | Unexpected text after Layout statement '{0}'. |
| DreamShader.Parser.Sections | UnknownLayoutStatementS | Unknown Layout statement '{0}'. |
| DreamShader.Parser.Sections | UnknownMaterialFunctionSectionS | Unknown material function section '{0}'. |
| DreamShader.Parser.Sections | UnknownShaderFunctionSectionS | Unknown shader function section '{0}'. |
| DreamShader.Parser.Sections | UnknownShaderSectionS | Unknown shader section '{0}'. |
| DreamShader.Parser.Sections | UnknownVirtualFunctionSectionS | Unknown VirtualFunction section '{0}'. |
| DreamShader.Parser.Sections | UnsupportedOutputTargetS | Unsupported output target '{0}'. |
| DreamShader.Parser.Sections | UnsupportedPropertyTypeS | Unsupported property type '{0}'. |
| DreamShader.Parser.Sections | UnsupportedUEBuiltinFunctionSUse | Unsupported UE builtin function '{0}'. Use OutputType=\\\"float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture\\\" for generic MaterialExpression calls. |
| DreamShader.Parser.Sections | UnterminatedGroupBlock | Unterminated Group(\"{0}\") {{ ... }} block. |
| DreamShader.Parser.Sections | VirtualFunctionNoGraphOrCode | VirtualFunction declares an existing MaterialFunction asset and does not support Graph or Code sections. |
| DreamShader.Preview | CompiledPreviewMaterial | Compiled preview material for {0}. |
| DreamShader.Preview | CompiledPreviewMaterialWithDetails | Compiled preview material for {0}. {1} |
| DreamShader.Preview | GeneratedMaterialCouldNotBeLoaded | Generated material '{0}' could not be loaded. |
| DreamShader.Preview | PreviewSourceMissing | DreamShader source '{0}' does not exist. |
| DreamShader.Preview | PreviewSupportsOnlyDsm | DreamShader preview only supports .dsm material files: '{0}'. |
| DreamShader.Preview | RenderedPreview | Rendered preview for {0}. |
| DreamShader.VirtualFunction | InvalidMaterialFunctionPackagePath | MaterialFunction '{0}' does not have a valid package path. |
| DreamShader.VirtualFunction | MaterialFunctionHasNoOutputs | MaterialFunction '{0}' does not expose any outputs. |
| DreamShader.VirtualFunction | NoMaterialFunctionAssetProvided | No MaterialFunction asset was provided. |
| DreamShader.VirtualFunction | VirtualFunctionHasNoOutputs | VirtualFunction '{0}' does not expose any outputs. |
| DreamShader.VirtualFunction | VirtualFunctionNameCannotBeEmpty | VirtualFunction name cannot be empty. |
| DreamShaderEditor.Settings | SectionDescription | Dream Shader Settings |
| DreamShaderEditor.Settings | SectionText | Dream Shader |
| DreamShaderEditorBridge | DreamShaderCleanGeneratedShadersLabel | Clean Generated Shaders |
| DreamShaderEditorBridge | DreamShaderCleanGeneratedShadersTooltip | Delete Intermediate/DreamShader/GeneratedShaders and queue a full DreamShader recompile. |
| DreamShaderEditorBridge | DreamShaderCleanPersistedGeneratedAssetsLabel | Clean Persisted Generated Assets |
| DreamShaderEditorBridge | DreamShaderCleanPersistedGeneratedAssetsTooltip | Delete DreamShader-generated material assets that are saved on disk (they shadow in-memory material mode). Shows a confirmation with the full list; source files are untouched and regenerate in memory. |
| DreamShaderEditorBridge | DreamShaderCleanPersistedNoneFound | No persisted DreamShader-generated assets found. |
| DreamShaderEditorBridge | DreamShaderCleanPersistedResult | Deleted {0} of {1} persisted generated asset(s). |
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
| DreamShaderEditorBridge | DreamShaderExportFunctionDSFLabel | Export DSF |
| DreamShaderEditorBridge | DreamShaderExportFunctionDSFTooltip | Export this Material Function graph to a DreamShader .dsf source file. |
| DreamShaderEditorBridge | DreamShaderExportFunctionNoAsset | DreamShader could not find the selected Material Function. |
| DreamShaderEditorBridge | DreamShaderExportMaterialDSMLabel | Export DSM |
| DreamShaderEditorBridge | DreamShaderExportMaterialDSMTooltip | Export this Material graph to a DreamShader .dsm source file. |
| DreamShaderEditorBridge | DreamShaderExportMaterialNoAsset | DreamShader could not find the selected Material. |
| DreamShaderEditorBridge | DreamShaderFunctionDecompileActionsSection | Decompiler |
| DreamShaderEditorBridge | DreamShaderInMemoryMaterialsHidden | Hidden {0} in-memory material(s) from the Content Browser and asset pickers. |
| DreamShaderEditorBridge | DreamShaderInMemoryMaterialsShown | Showing {0} in-memory material(s) in the Content Browser and asset pickers. |
| DreamShaderEditorBridge | DreamShaderInMemoryModeShadowed | {0} previously generated asset(s) are still saved on disk and shadow the in-memory materials. Run Tools > DreamShader > Clean Persisted Generated Assets to remove them. |
| DreamShaderEditorBridge | DreamShaderMaterialActionsLabel | DreamShader |
| DreamShaderEditorBridge | DreamShaderMaterialActionsTooltip | DreamShader actions for this Material. |
| DreamShaderEditorBridge | DreamShaderMaterialFunctionActionsLabel | DreamShader |
| DreamShaderEditorBridge | DreamShaderMaterialFunctionActionsTooltip | DreamShader actions for this Material Function. |
| DreamShaderEditorBridge | DreamShaderMaterialFunctionToolbarMenuLabel | DreamShader |
| DreamShaderEditorBridge | DreamShaderMaterialFunctionToolbarMenuTooltip | DreamShader actions for this Material Function. |
| DreamShaderEditorBridge | DreamShaderMaterialToolbarMenuLabel | DreamShader |
| DreamShaderEditorBridge | DreamShaderMaterialToolbarMenuTooltip | DreamShader actions for this Material. |
| DreamShaderEditorBridge | DreamShaderOpenVirtualFunctionLabel | OpenVirtualFunction |
| DreamShaderEditorBridge | DreamShaderOpenVirtualFunctionNoAsset | DreamShader could not find the selected Material Function. |
| DreamShaderEditorBridge | DreamShaderOpenVirtualFunctionTooltip | Open the existing DreamShader VirtualFunction definition in VSCode. |
| DreamShaderEditorBridge | DreamShaderOpenWorkspaceLabel | Open Dream Shader Workspace (VSCode) |
| DreamShaderEditorBridge | DreamShaderOpenWorkspaceToolbarLabel | Open Dream Shader Workspace (VSCode) |
| DreamShaderEditorBridge | DreamShaderOpenWorkspaceToolbarTooltip | Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable. |
| DreamShaderEditorBridge | DreamShaderOpenWorkspaceTooltip | Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable. |
| DreamShaderEditorBridge | DreamShaderRecompileLabel | Recompile DSM |
| DreamShaderEditorBridge | DreamShaderRecompileToolbarLabel | DSM |
| DreamShaderEditorBridge | DreamShaderRecompileToolbarTooltip | Recompile all DreamShader .dsm and .dsf source files. |
| DreamShaderEditorBridge | DreamShaderRecompileTooltip | Recompile all DreamShader .dsm and .dsf source files and refresh diagnostics. |
| DreamShaderEditorBridge | DreamShaderToggleShowInMemoryMaterialsLabel | Show In-Memory Materials |
| DreamShaderEditorBridge | DreamShaderToggleShowInMemoryMaterialsTooltip | Show memory-only ThinCustom/Instance-backend DreamShader materials in the Content Browser and asset pickers — needed when picking one as a material instance Parent or referencing it from a detail panel. Graph-backend materials are plain UMaterials and are always visible, so this toggle does not affect them. While shown, an explicit Save on one would persist it to disk (the shadow warning and Clean command cover recovery). |
| DreamShaderEditorBridge | DreamShaderVirtualFunctionActionsSection | VirtualFunction |
| DreamShaderEditorBridge | MaterialCompileErrorHeader | [{0} / {1}] {2} |
| DreamShaderMaterialBrowser | AssetPathNoShaderBlock | {0}: this file does not define a top-level Shader block. |
| DreamShaderMaterialBrowser | AssetPathParseError | {0}: {1} |
| DreamShaderMaterialBrowser | AssetPathReadFailed | Failed to read DreamShader source '{0}'. |
| DreamShaderMaterialBrowser | BadName | Provide a name and a destination folder. |
| DreamShaderMaterialBrowser | Base | Base |
| DreamShaderMaterialBrowser | Blend | Blend mode |
| DreamShaderMaterialBrowser | Browse | Browse... |
| DreamShaderMaterialBrowser | BrowseTip | Pick the destination folder. |
| DreamShaderMaterialBrowser | Cancel | Cancel |
| DreamShaderMaterialBrowser | CBCreateInstance | Create DreamShader instance |
| DreamShaderMaterialBrowser | CBCreateInstanceTip | Create a material instance that shares this material's compiled shader map. |
| DreamShaderMaterialBrowser | ChainRowFmt | {0}{1} |
| DreamShaderMaterialBrowser | ChildrenHeader | Child instances ({0}) |
| DreamShaderMaterialBrowser | CompileAll | Compile all |
| DreamShaderMaterialBrowser | CompileAllTip | Force-recompile every .dsm/.dsf source (in memory). |
| DreamShaderMaterialBrowser | CompiledAll | Compiled {0} source(s), {1} failed |
| DreamShaderMaterialBrowser | CompileOk | Compiled {0} |
| DreamShaderMaterialBrowser | CompilingAll | Compiling all DreamShader sources... |
| DreamShaderMaterialBrowser | Create | Create |
| DreamShaderMaterialBrowser | CreateInstance | Create instance |
| DreamShaderMaterialBrowser | CreateInstanceBtn | Create instance |
| DreamShaderMaterialBrowser | CreateInstanceTip | Create a material instance of the selected material (shares its compiled shader map). |
| DreamShaderMaterialBrowser | CreateInstanceTitle | Create material instance |
| DreamShaderMaterialBrowser | DiagnosticLineFmt | L{0}:{1} {2} |
| DreamShaderMaterialBrowser | Domain | Domain |
| DreamShaderMaterialBrowser | EmptyProject | No materials found under /Game. |
| DreamShaderMaterialBrowser | ErrorsOnly | Errors only |
| DreamShaderMaterialBrowser | FactoryAssetExists | An asset already exists at {0}. |
| DreamShaderMaterialBrowser | FactoryCreatePackageFailed | Failed to create package {0}. |
| DreamShaderMaterialBrowser | FactoryMaterializeFailed | Failed to materialize the material to disk: {0} |
| DreamShaderMaterialBrowser | FactoryReloadFailed | Materialized the material but could not reload it at {0}. |
| DreamShaderMaterialBrowser | FunctionDetail | Function library / header. Recompiles the materials that import it. |
| DreamShaderMaterialBrowser | FunctionUsedBy | function · used by {0} material(s) |
| DreamShaderMaterialBrowser | GenPage | Dream Shader Gen |
| DreamShaderMaterialBrowser | GenPageNoGeneratedAsset | No generated asset at {0} |
| DreamShaderMaterialBrowser | HideFunctions | Hide functions |
| DreamShaderMaterialBrowser | Inheritance | Inheritance |
| DreamShaderMaterialBrowser | InstanceCreated | Created {0} |
| DreamShaderMaterialBrowser | InstanceNeedsCompile | Compile {0} first. |
| DreamShaderMaterialBrowser | MaterializeBtn | Materialize |
| DreamShaderMaterialBrowser | Materialized | Materialized {0} to disk |
| DreamShaderMaterialBrowser | MaterializeNoSource | This material is memory-only and has no DreamShader source file to materialize from. |
| DreamShaderMaterialBrowser | MaterializeTip | Write this memory-only material (and its base) to disk. |
| DreamShaderMaterialBrowser | NameLabel | Name |
| DreamShaderMaterialBrowser | NewObjectFailed | Failed to create the material instance object. |
| DreamShaderMaterialBrowser | NoChildren | No loaded child instances. |
| DreamShaderMaterialBrowser | NoneSelected | Select a material, then create an instance. |
| DreamShaderMaterialBrowser | NoParent | No parent material was provided. |
| DreamShaderMaterialBrowser | NoSelection | Select a material to see its inheritance and settings. |
| DreamShaderMaterialBrowser | OpenAfter | Open the instance after creating |
| DreamShaderMaterialBrowser | OpenBtn | Open |
| DreamShaderMaterialBrowser | OpenTabLabel | Material Content Browser |
| DreamShaderMaterialBrowser | OpenTabTooltip | Open the DreamShader Material Content Browser. |
| DreamShaderMaterialBrowser | ParentGone | The parent material is no longer available. |
| DreamShaderMaterialBrowser | ParentLabel | Parent |
| DreamShaderMaterialBrowser | PathLabel | Folder |
| DreamShaderMaterialBrowser | PComp | Compile |
| DreamShaderMaterialBrowser | PickFolderTitle | Choose a destination folder |
| DreamShaderMaterialBrowser | PInst | Create instance |
| DreamShaderMaterialBrowser | PMat | Materialize |
| DreamShaderMaterialBrowser | POpenMat | Open material |
| DreamShaderMaterialBrowser | POpenSrc | Open source |
| DreamShaderMaterialBrowser | PreviewFunction | function library |
| DreamShaderMaterialBrowser | PreviewNone | Select a source file to preview its material. |
| DreamShaderMaterialBrowser | PreviewUsedBy | used by {0} material(s) |
| DreamShaderMaterialBrowser | ProjectPage | Project |
| DreamShaderMaterialBrowser | Refresh | Refresh |
| DreamShaderMaterialBrowser | RefreshTip | Rescan the source directory and recompute status. |
| DreamShaderMaterialBrowser | SearchHint | Search sources |
| DreamShaderMaterialBrowser | SelectedFmt | Selected: {0} |
| DreamShaderMaterialBrowser | SelectFirst | Select a material to create an instance of. |
| DreamShaderMaterialBrowser | ShowInMemory | Show in-memory materials |
| DreamShaderMaterialBrowser | ShowInMemoryTip | Show DreamShader's memory-only materials here and in the Content Browser (global setting). |
| DreamShaderMaterialBrowser | SourceCount | {0} / {1} |
| DreamShaderMaterialBrowser | SourceNone | - |
| DreamShaderMaterialBrowser | SourceRow | Source |
| DreamShaderMaterialBrowser | StatusError | compile error |
| DreamShaderMaterialBrowser | StatusFunction | function / header |
| DreamShaderMaterialBrowser | StatusNever | not compiled |
| DreamShaderMaterialBrowser | StatusStale | stale |
| DreamShaderMaterialBrowser | StatusUnresolved | unresolved |
| DreamShaderMaterialBrowser | StatusUpToDate | up to date |
| DreamShaderMaterialBrowser | Storage | Storage |
| DreamShaderMaterialBrowser | SubLabelWithRoot | {0} · {1} |
| DreamShaderMaterialBrowser | TabTitle | Material Content Browser |
| DreamShaderMaterialBrowser | TabTooltip | Browse, manage, and create instances of project and DreamShader-generated materials. |
| DreamShaderTests | WireUtils.Float | value {0} |
| DreamShaderTests | WireUtils.Inner | inner {0} |
| DreamShaderTests | WireUtils.Ordered | Unsupported swizzle {0} at line {1}. |
| DreamShaderTests | WireUtils.Outer | outer [{0}] end |
| DreamShaderTests | WireUtils.Plain | Generation aborted. |
| DreamShaderVirtualFunctionSyncService | InvalidAssetReference | VirtualFunction '{0}' asset reference is invalid: {1} |
| DreamShaderVirtualFunctionSyncService | InvalidDeclaration | VirtualFunction declaration is invalid: {0} |
| DreamShaderVirtualFunctionSyncService | MissingClosingBrace | VirtualFunction body is missing a closing '}'. |
| DreamShaderVirtualFunctionSyncService | MissingClosingParenthesis | VirtualFunction attributes are missing a closing ')'. |
| DreamShaderVirtualFunctionSyncService | MissingMaterialFunction | VirtualFunction '{0}' references missing MaterialFunction '{1}'. |
| DreamShaderVirtualFunctionSyncService | ReadSourceFileFailed | DreamShader could not read VirtualFunction source file '{0}'. |
| DreamShaderVirtualFunctionSyncService | RefreshFailed | VirtualFunction '{0}' could not be refreshed from MaterialFunction '{1}': {2} |
| DreamShaderVirtualFunctionSyncService | UpdateSourceFileFailed | DreamShader failed to update VirtualFunction source file '{0}'. |

## Deferred diagnostics inventory
Deferred files: 45
Runtime FText::FromString/FText::FromName / FString::Printf literal call sites in deferred diagnostics: 0
Use -IncludeDeferred to lint MaterialAssetGeneration/ and Decompiler/ in the next phase.

## Auto-gathered metadata
Unreal will add any auto-gathered UPROPERTY metadata (for example DisplayName and ToolTip) on top of this compile-time baseline. This file intentionally counts only LOCTEXT/NSLOCTEXT entries.

