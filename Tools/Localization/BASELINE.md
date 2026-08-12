# DreamShader localization baseline

This inventory covers compile-time LOCTEXT/NSLOCTEXT entries that the Localization Dashboard gather step can see.

## Expected gather count
146

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
| DreamShaderEditor.Settings | SectionDescription | Dream Shader Settings |
| DreamShaderEditor.Settings | SectionText | Dream Shader |
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

## Deferred diagnostics inventory
Deferred files: 45
Runtime FText::FromString/FText::FromName / FString::Printf literal call sites in deferred diagnostics: 0
Use -IncludeDeferred to lint MaterialAssetGeneration/ and Decompiler/ in the next phase.

## Auto-gathered metadata
Unreal will add any auto-gathered UPROPERTY metadata (for example DisplayName and ToolTip) on top of this compile-time baseline. This file intentionally counts only LOCTEXT/NSLOCTEXT entries.

