#pragma once

#include "CoreMinimal.h"
#include "DreamShaderTypes.h"

// EDreamShaderDigestState, returned by the provenance helpers declared below.
#include "DreamShaderGeneratedAssetDigest.h"

// EBlendMode / EMaterialShadingModel, used by the resolver declarations below. Unity builds happened
// to pull these in through a neighbouring TU; a non-unity compile of this header does not.
#include "Engine/EngineTypes.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "SceneTypes.h"

// FDreamShaderGraphProbe: the per-statement value records the graph builder hands to the preview's
// breakpoint machinery (see FCodeGraphBuilder::TakeProbes).
#include "Preview/DreamShaderGraphDebugInfo.h"

class UMaterial;
class UMaterialFunction;
class UMaterialExpression;
class UMaterialExpressionMaterialFunctionCall;
class UDreamShaderMaterialInstance;
class UClass;
class UEnum;
class FProperty;
struct FScopedSlowTask;

namespace UE::DreamShader::Editor::Private
{
	// Estimated Slate-unit footprint of one material graph node. See EstimateMaterialNodeSize.
	struct FLayoutNodeSize
	{
		int32 Width = 0;
		int32 Height = 0;
	};

	struct FResolvedMaterialProperty
	{
		EMaterialProperty Property = MP_EmissiveColor;
		ECustomMaterialOutputType OutputType = CMOT_Float1;
		bool bIsSubstrateMaterial = false;
	};

	struct FResolvedNamedOutput
	{
		FString Name;
		ECustomMaterialOutputType OutputType = CMOT_Float1;
		bool bIsSubstrateMaterial = false;
	};

	enum class ECodeTokenType : uint8
	{
		Identifier,
		Number,
		String,
		LeftParen,
		RightParen,
		Comma,
		Dot,
		Plus,
		Minus,
		Star,
		Slash,
		Equals,
		ScopeResolution,
		End,
	};

	struct FCodeToken
	{
		ECodeTokenType Type = ECodeTokenType::End;
		FString Text;
	};

	enum class ECodeExpressionKind : uint8
	{
		Name,
		NumberLiteral,
		StringLiteral,
		Call,
		MemberAccess,
		Binary,
		Unary,
	};

	struct FCodeExpression;

	struct FCodeCallArgument
	{
		FString Name;
		TSharedPtr<FCodeExpression> Expression;
		bool bIsNamed = false;
	};

	struct FCodeExpression
	{
		ECodeExpressionKind Kind = ECodeExpressionKind::Name;
		FString Text;
		TSharedPtr<FCodeExpression> Left;
		TSharedPtr<FCodeExpression> Right;
		TArray<FCodeCallArgument> Arguments;
	};

	struct FCodeCondition
	{
		FString Operator;
		TSharedPtr<FCodeExpression> Left;
		TSharedPtr<FCodeExpression> Right;
	};

	struct FCodeStatement
	{
		bool bIsDeclaration = false;
		bool bIsExpressionStatement = false;
		bool bIsIfStatement = false;
		bool bUsesBraceInitializer = false;
		bool bHasSourceLocation = false;
		int32 SourceLine = 1;
		int32 SourceColumn = 1;
		FString DeclaredType;
		FString TargetName;
		FString RegionName;
		FString InitializerText;
		TSharedPtr<FCodeExpression> Expression;
		FCodeCondition Condition;
		TArray<FCodeStatement> ThenStatements;
		TArray<FCodeStatement> ElseStatements;
	};

	struct FCodeValue
	{
		UMaterialExpression* Expression = nullptr;
		int32 OutputIndex = 0;
		int32 ComponentCount = 1;
		bool bHasInputMask = false;
		bool InputMaskR = false;
		bool InputMaskG = false;
		bool InputMaskB = false;
		bool InputMaskA = false;
		bool bIsTextureObject = false;
		ETextShaderTextureType TextureType = ETextShaderTextureType::Texture2D;
		bool bIsMaterialAttributes = false;
		bool bIsSubstrateMaterial = false;
		bool bHasAuthoritativeComponentCount = false;
		bool bIsIntegerType = false;
	};

	// Stable identity token for an FCodeValue, used to dedupe/reuse generated expression nodes.
	// Exposed (was a file-local static in CodeExpressions.cpp) so EvaluateMathBuiltinCall could move
	// to its own TU while other FCodeGraphBuilder members keep calling it.
	FString MakeCodeValueReuseToken(const FCodeValue& Value);

	// CodeCalls function-call helpers, exposed (were a file-local anonymous namespace in
	// CodeCalls.cpp) so the material/virtual-function-call cluster could move to its own TU.
	bool ApplyFunctionCallOutputType(
		UMaterialExpressionMaterialFunctionCall* FunctionCall,
		int32 FunctionOutputIndex,
		int32& InOutComponentCount,
		bool& bInOutIsTextureObject,
		bool& bInOutIsSubstrateMaterial);
	FString BuildFunctionSourceArgumentList(const FTextShaderFunctionDefinition& Function, const TArray<FString>& ResultVariableNames);
	bool IsSubstrateTypeUnsupportedForEngine(const FString& TypeName);

	bool ParseCodeExpression(const FString& InExpression, TSharedPtr<FCodeExpression>& OutExpression, FString& OutError);
	bool ParseCodeStatements(
		const FString& InCode,
		TArray<FCodeStatement>& OutStatements,
		FString& OutError,
		int32* OutErrorLine = nullptr,
		int32* OutErrorColumn = nullptr);
	bool MakeCodeDeclarationStatement(
		const FString& DeclaredType,
		const FString& TargetName,
		const FString& InitializerText,
		FCodeStatement& OutStatement,
		FString& OutError);

	bool ResolveMaterialProperty(const FString& InName, FResolvedMaterialProperty& OutProperty);
	bool TryResolveCustomOutputType(const FString& InTypeName, ECustomMaterialOutputType& OutOutputType);
	bool ParseScalarLiteral(const FString& InText, double& OutValue);
	bool ParseBooleanLiteral(const FString& InText, bool& OutValue);
	bool ParseIntegerLiteral(const FString& InText, int32& OutValue);
	bool ParseUnsignedInteger32Literal(const FString& InText, uint32& OutValue);
	bool ParseVectorLiteral(const FString& InText, TArray<double>& OutValues);
	bool TryResolveWorldPositionShaderOffset(const FString& InValue, EWorldPositionIncludedOffsets& OutValue);
	FString NormalizeEnumLookupKey(const FString& InKey);
	bool TryResolveEnumLiteral(UEnum* Enum, const FString& InValue, int64& OutEnumValue);
	bool ResolveDreamShaderAssetDestination(
		const FString& AssetName,
		const FString& Root,
		FString& OutPackageName,
		FString& OutObjectPath,
		FString& OutAssetLeafName,
		FString& OutError);
	/**
	 * Fills in the `Root` of every block in `Definition` that declared none, when the source file
	 * lives under a plugin source root: the asset then defaults to the plugin that ships the source
	 * instead of to `/Game`. Files under the project root are left alone, as is any block with an
	 * explicit `Root=` — including `Root="/"`, which is how you ask a plugin-root file for `/Game`.
	 *
	 * No-op when the owning plugin cannot host generated content; `OutFallbackReason`, if given,
	 * then explains why the block fell back to `/Game`.
	 *
	 * Must run before ResolveDreamShaderAssetDestination at *every* site that turns a source file
	 * into an asset path, or the Gen page and the preview would name a different asset than the one
	 * generation actually writes.
	 */
	void ApplyDefaultRootFromSourceFile(
		const FString& SourceFilePath,
		FTextShaderDefinition& Definition,
		FString* OutFallbackReason = nullptr);
	bool TryResolveDreamShaderAssetReference(const FString& InText, FString& OutObjectPath, FString& OutError);
	UMaterialExpression* CreateScalarLiteralExpression(UMaterial* Material, double Value, int32 PositionY);
	// Thin wrapper over UMaterialEditingLibrary::CreateMaterialExpressionEx, shared by literal
	// creation, the expression factory, and graph layout (reroute/comment nodes). Exposed (was a
	// file-local static in Support.cpp) so the graph-layout cluster could move to its own TU.
	UMaterialExpression* CreateOwnedMaterialExpression(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		UClass* ExpressionClass,
		int32 PositionX,
		int32 PositionY);
	// Literal / input-value helpers shared by the expression factory (own TU) and staying code.
	// Exposed (were file-local statics in Support.cpp) so the factory cluster could move out.
	bool TryResolvePropertyReference(
		const FString& InReferenceName,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		UMaterialExpression*& OutExpression);
	UMaterialExpression* CreateScalarLiteralExpressionEx(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		double Value,
		int32 PositionY);
	UMaterialExpression* CreateVectorLiteralExpression(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const TArray<double>& Components,
		int32 ExpectedComponentCount,
		int32 PositionY);
	bool ResolveExpressionInputValue(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FString& InValueText,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		int32 ExpectedComponentCount,
		int32 PositionY,
		UMaterialExpression*& OutExpression,
		FString& OutError);
	// UE builtin argument parsing, shared by the expression factory (own TU) and staying code.
	bool TryGetUEBuiltinArgument(const FTextShaderPropertyDefinition& Property, const TCHAR* Key, FString& OutValue);
	bool ValidateUEBuiltinArgumentNames(
		const FTextShaderPropertyDefinition& Property,
		TConstArrayView<const TCHAR*> AllowedArgumentNames,
		FString& OutError);
	bool TryResolvePositionOrigin(const FString& InValue, EPositionOrigin& OutValue);
	FString EnsureTopLevelReturn(const FString& InHLSL);
	// OutEmbeddedIncludePaths receives the hoisted `#include` paths of every Function embedded into the
	// node (SelfContained closure); the caller must put them on the Custom node ahead of any other
	// include, because the embedded bodies reference their symbols.
	bool PrepareCustomNodeCode(
		const FTextShaderDefinition& Definition,
		const FString& SourceCode,
		const TArray<FString>& RequestedEmbeddedFunctionNames,
		const FString& WrapperNameHint,
		FString& OutCode,
		bool& bOutUsesGeneratedInclude,
		TArray<FString>& OutEmbeddedIncludePaths,
		FString& OutError);
	// Rewrite imported-Function call sites in HLSL text to match the generated-include signatures the
	// instance backend references (single-out function -> return value, so an out-param call
	// `Fn(a, b)` becomes `b = Fn(a)`, and the DSL name resolves to the DreamShaderFn_* symbol). The
	// graph backend gets the same reconciliation through PrepareCustomNodeCode / the Custom node path.
	bool IsTextureFunctionParameterType(const FString& InTypeName);
	FString BuildGeneratedFunctionSymbolName(const FTextShaderFunctionDefinition& Function);
	FString BuildGeneratedIncludeVirtualPath(const FString& SourceFilePath);
	bool WriteGeneratedInclude(const FString& SourceFilePath, const FTextShaderDefinition& Definition, FString& OutError);
	// Tearing a graph down is FDreamShaderGraphRollback's job now (DreamShaderGraphRollback.h): it
	// detaches the old nodes instead of destroying them, so a build that fails partway can put them
	// back. The two former ClearMaterial*Expressions helpers were its non-reversible ancestors.
	void EnsureExpressionCanBeDeleted(UMaterialExpression* Expression);
	void ClearDreamShaderGeneratedComments(UMaterial* Material, UMaterialFunction* MaterialFunction);
	FCodeValue CreateOutputRerouteValue(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FCodeValue& SourceValue,
		const FString& RouteName,
		int32 RouteIndex);
	// The footprint the layout pass places a node by. There is no widget to measure at generation
	// time, so this is derived from what SGraphNodeMaterialBase assembles -- title bar, one row per
	// pin, and the expression preview when ShouldShowPreview(). Errs high on purpose: over-estimating
	// only loosens the graph, under-estimating overlaps nodes. Exposed so a layout test can assert
	// against the same measure the placement used.
	FLayoutNodeSize EstimateMaterialNodeSize(UMaterialExpression* Expression);
	void LayoutGeneratedExpressions(UMaterial* Material, UMaterialFunction* MaterialFunction);
	// bQuiet drops the per-node slow-task frames. The in-memory path lays out on every save, where
	// formatting one progress string per node costs more than the placement itself.
	void LayoutGeneratedExpressions(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FTextShaderLayout* Layout,
		const TMap<FString, UMaterialExpression*>* ExpressionsByVariable,
		const TMap<FString, FString>* RegionByVariable,
		bool bQuiet);
	void ResetMaterialToDefaults(UMaterial* Material);
	bool ValidateSettings(const FTextShaderDefinition& Definition, FString& OutError);
	bool ApplySettings(UMaterial* Material, const FTextShaderDefinition& Definition, FString& OutError);
	UMaterialExpression* CreatePropertyExpression(
		UMaterial* Material,
		const FTextShaderPropertyDefinition& Property,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		int32 PositionY,
		FString& OutError);
	UMaterialExpression* CreatePropertyExpression(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FTextShaderPropertyDefinition& Property,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		int32 PositionY,
		FString& OutError);
	bool TryGetComponentCountForOutputType(ECustomMaterialOutputType OutputType, int32& OutComponentCount);
	// Declared texture dimension, or the dimension of the default asset when the type token does not
	// name one (TextureObjectParameter and friends hold any dimension in a single node class).
	ETextShaderTextureType ResolveEffectiveTextureType(const FTextShaderPropertyDefinition& Property);
	bool IsMaterialAttributesType(const FString& InTypeName);
	bool IsSubstrateMaterialType(const FString& InTypeName);
	bool IsSubstrateMaterialTypeSupported();
	bool TryResolveCodeDeclaredType(const FString& InTypeName, int32& OutComponentCount, bool& bOutIsTexture);
	bool TryResolveCodeDeclaredType(const FString& InTypeName, int32& OutComponentCount, bool& bOutIsTexture, ETextShaderTextureType& OutTextureType);
	bool TryResolveCodeDeclaredType(const FString& InTypeName, int32& OutComponentCount, bool& bOutIsTexture, bool& bOutIsSubstrateMaterial);
	bool TryResolveCodeDeclaredType(const FString& InTypeName, int32& OutComponentCount, bool& bOutIsTexture, ETextShaderTextureType& OutTextureType, bool& bOutIsSubstrateMaterial);
	bool TryResolveOutputVariableComponentCount(
		const FTextShaderDefinition& Definition,
		const FString& VariableName,
		int32& OutComponentCount,
		bool& bOutIsTexture);
	bool TryResolveOutputVariableComponentCount(
		const FTextShaderDefinition& Definition,
		const FString& VariableName,
		int32& OutComponentCount,
		bool& bOutIsTexture,
		ETextShaderTextureType& OutTextureType);
	bool TryResolveOutputVariableComponentCount(
		const FTextShaderDefinition& Definition,
		const FString& VariableName,
		int32& OutComponentCount,
		bool& bOutIsTexture,
		ETextShaderTextureType& OutTextureType,
		bool& bOutIsSubstrateMaterial);
	bool CreateOrReuseMaterial(const FTextShaderDefinition& Definition, UMaterial*& OutMaterial, FString& OutError, bool bTransient = false);
	bool CreateOrReuseMaterialFunction(const FTextShaderMaterialFunctionDefinition& Definition, UMaterialFunction*& OutFunction, FString& OutError, bool bTransient = false);
	bool CreateOrReuseInstanceMaterial(const FTextShaderDefinition& Definition, ::UDreamShaderMaterialInstance*& OutInstance, FString& OutError, bool bTransient = false);
	bool TryResolveBlendModeSetting(const FString& InValue, EBlendMode& OutBlendMode);
	bool TryResolveShadingModelSetting(const FString& InValue, EMaterialShadingModel& OutShadingModel);
	bool TryResolveMaterialFunctionParameterType(
		const FString& InTypeName,
		int32& OutComponentCount,
		bool& bOutIsTexture,
		int32& OutFunctionInputTypeValue,
		bool& bOutIsSubstrateMaterial);
	bool ValidateOutputs(
		const FTextShaderDefinition& Definition,
		TArray<FResolvedNamedOutput>& OutNamedOutputs,
		bool& bOutUsesReturn,
		ECustomMaterialOutputType& OutReturnType,
		bool& bOutReturnIsSubstrateMaterial,
		FString& OutError);
	FString BuildSourceHash(const FString& SourceText);
	bool IsGeneratedAssetSourceCurrent(UObject* Asset, const FString& SourceFilePath, const FString& SourceHash);
	bool HasDreamShaderSourceMetadata(UObject* Asset);
	void ApplySourceMetadata(UObject* Asset, const FString& SourceFilePath);
	void ApplySourceMetadata(UObject* Asset, const FString& SourceFilePath, const FString& SourceHash);
	/**
	 * The second rebuild precondition, alongside CheckGeneratedAssetNotDiverged.
	 *
	 * An open asset editor does not edit the asset: FMaterialEditor duplicates it into a transient
	 * UPreviewMaterial and copies that duplicate back on Apply or Save (UpdateOriginalMaterial), and
	 * the material instance editor writes back through a UMaterialEditorInstanceConstant wrapper. So
	 * an editor that was open across a rebuild holds a pre-rebuild copy, and the next Apply silently
	 * reverts everything the rebuild did. Refusing is the only safe answer available: whether that
	 * copy has unsaved edits in it is FMaterialEditor::bMaterialDirty, which is private to the
	 * MaterialEditor module -- so "close it if it is clean" is not a question this plugin can ask, and
	 * closing it blindly would pop a save prompt in the middle of a compile-on-save.
	 */
	bool IsGeneratedAssetOpenInEditor(UObject* Asset);
	bool CheckGeneratedAssetNotOpenInEditor(UObject* Asset, FString& OutError);

	/**
	 * Whether this process is allowed to write generated assets to disk.
	 *
	 * Only one process per project may, and the bridge's ownership lock is what decides which. Two
	 * editors open on the same project would otherwise both rebuild and both SavePackage the same file
	 * -- and a save that loses that race is not a merge, it is a corrupted package or a dead editor.
	 * True by default, because a commandlet has no bridge, no second process, and exactly one job.
	 */
	void SetMayWriteGeneratedAssetsToDisk(bool bMayWrite);
	bool MayWriteGeneratedAssetsToDisk();
	/**
	 * True when a compile that would land on disk has to leave the asset alone because another process
	 * owns writing it. Reported as a skip rather than a failure: a second editor is a legitimate way to
	 * work, its own in-memory materials still compile, and only the shared file is off limits.
	 */
	bool ShouldDeferPersistedAssetToWriteOwner(UObject* Asset, bool bWouldPersist, FString& OutMessage);

	// Whether the asset has a file behind it. The generation paths ask this right after creating or
	// reusing their target and downgrade an in-memory request to a persisted one when it answers yes:
	// where the asset lives decides how it is rebuilt, so there is never an in-memory copy of an
	// on-disk asset disagreeing with the file underneath it.
	bool IsGeneratedAssetPersisted(UObject* Asset);
	// The project-relative source path stamped at generation time -- the asset's answer to "which file
	// am I built from", which the Adopt action needs in order to know what to rewrite.
	FString GetGeneratedAssetSourceFile(UObject* Asset);
	// The stamped source hash. Only a persisted build writes one -- an in-memory build stamps the path
	// alone, deliberately, so the skip check stays off -- which makes this the plainest reading of
	// which of the two paths a compile actually took.
	FString GetGeneratedAssetSourceHash(UObject* Asset);
	// Content fingerprint bookkeeping. See DreamShaderGeneratedAssetDigest.h: the source hash says
	// whether the SOURCE moved, these say whether the ASSET did.
	FString GetOutputDigestMetadata(UObject* Asset);
	void ApplyOutputDigestMetadata(UObject* Asset);
	EDreamShaderDigestState ClassifyGeneratedAsset(UObject* Asset);
	// Detach: drops every DreamShader stamp, which makes the asset Foreign and takes it out of the
	// generator's hands for good (the ownership guard refuses to touch it afterwards).
	void ClearDreamShaderMetadata(UObject* Asset);
	// The divergence gate. Returns false -- with a message naming the three ways out -- when the asset
	// no longer holds what DreamShader last generated into it. Must be called BEFORE anything clears
	// the graph; that ordering is the whole point.
	//
	// Note what it does NOT take: a force flag. See FScopedDreamShaderRevertDiverged.
	bool CheckGeneratedAssetNotDiverged(UObject* Asset, FString& OutError);

	/**
	 * The one thing that lets a compile overwrite a hand-edited asset. Held by the Revert action for
	 * the duration of the rebuild it asked for, and by nothing else.
	 *
	 * Kept separate from bForce on purpose. bForce answers "is the source hash stale", which the
	 * editor's startup sweep asserts unconditionally for every file it regenerates in memory; if the
	 * gate honoured it, every restart would quietly rebuild over saved hand edits -- the exact failure
	 * the gate exists to stop, in the mode the editor spends all its time in.
	 */
	struct FScopedDreamShaderRevertDiverged
	{
		FScopedDreamShaderRevertDiverged();
		~FScopedDreamShaderRevertDiverged();

		FScopedDreamShaderRevertDiverged(const FScopedDreamShaderRevertDiverged&) = delete;
		FScopedDreamShaderRevertDiverged& operator=(const FScopedDreamShaderRevertDiverged&) = delete;
	};
	bool IsRevertingDivergedAssets();
	bool SaveAssetPackage(UObject* Asset, FString& OutError);
	bool SaveAssetPackages(const TArray<UObject*>& Assets, FString& OutError);
	UClass* ResolveMaterialExpressionClass(const FString& ClassSpecifier);
	FProperty* FindMaterialExpressionArgumentProperty(UClass* ExpressionClass, const FString& ArgumentName);
	bool IsMaterialExpressionInputProperty(const FProperty* Property);
	bool SetMaterialExpressionLiteralProperty(UObject* Target, FProperty* Property, const FString& ValueText, FString& OutError);
	bool SetMaterialExpressionLiteralProperty(UObject* Target, FProperty* Property, void* ValuePtr, const FString& ValueText, FString& OutError);
	bool ApplyExpressionMetadata(UMaterialExpression* Expression, const FTextShaderMetadata& Metadata, FString& OutError);

	class FCodeGraphBuilder
	{
	public:
		FCodeGraphBuilder(
			UMaterial* InMaterial,
			UMaterialFunction* InMaterialFunction,
			const FTextShaderDefinition& InDefinition,
			const FString& InSourceFilePath,
			const FString& InIncludeVirtualPath,
			const TArray<FTextShaderPropertyDefinition>* InLocalProperties = nullptr,
			const FString& InCodeSourceFilePath = FString(),
			int32 InCodeStartLine = 1,
			int32 InCodeStartColumn = 1);

		bool Build(
			const TArray<FCodeStatement>& Statements,
			TMap<FString, FCodeValue>& InOutValues,
			FString& OutError);
		bool EvaluateOutputExpression(const FString& ExpressionText, FCodeValue& OutValue, FString& OutError);
		const TMap<FString, UMaterialExpression*>& GetGeneratedExpressionsByVariable() const { return GeneratedExpressionsByVariable; }
		const TMap<FString, FString>& GetRegionByVariable() const { return RegionByVariable; }
		// Every (line, name) -> value binding the build produced, in execution order, with lines
		// already offset into the source file. Moves the array out; the builder keeps nothing.
		TArray<FDreamShaderGraphProbe> TakeProbes() { return MoveTemp(Probes); }

	private:
		UMaterial* Material = nullptr;
		UMaterialFunction* MaterialFunction = nullptr;
		const FTextShaderDefinition& Definition;
		const TArray<FTextShaderPropertyDefinition>* LocalProperties = nullptr;
		FString SourceFilePath;
		FString IncludeVirtualPath;
		FString CodeSourceFilePath;
		int32 CodeStartLine = 1;
		int32 CodeStartColumn = 1;
		TMap<FString, FCodeValue>* Values = nullptr;
		TMap<FString, UMaterialExpression*> GeneratedPropertyExpressions;
		TMap<FString, UMaterialExpression*> GeneratedExpressionsByVariable;
		TMap<FString, FString> RegionByVariable;
		TArray<FDreamShaderGraphProbe> Probes;
		TMap<FString, FCodeValue> ReusableExpressionValues;
		TSet<FString> CreatingPropertyNames;
		int32 NextPropertyNodeY = -620;
		int32 NextNodeY = -120;
		FScopedSlowTask* ActiveBuildSlowTask = nullptr;
		mutable int32 ProgressTickCounter = 0;

		FCodeValue* FindValue(const FString& Name) const;
		void RegisterGeneratedVariable(const FCodeStatement& Statement, const FCodeValue& Value);
		bool TryCreatePropertyValue(const FString& Name, FCodeValue& OutValue, FString& OutError);
		int32 ConsumeNodeY();
		UMaterialExpression* CreateExpression(TSubclassOf<UMaterialExpression> ExpressionClass, int32 PositionX, int32 PositionY) const;
		UMaterialExpression* CreateScalarLiteralNode(double Value, int32 PositionY);
		UMaterialExpression* CreateStaticBoolLiteralNode(bool bValue, int32 PositionY);
		bool CreateMaterialAttributesValue(FCodeValue& OutValue, FString& OutError);
		bool CreateDefaultValue(const FString& DeclaredType, FCodeValue& OutValue, FString& OutError);
		bool CoerceValueToType(const FCodeValue& InValue, int32 ExpectedComponentCount, bool bExpectedTexture, FCodeValue& OutValue, FString& OutError);
		bool CoerceValueToType(const FCodeValue& InValue, int32 ExpectedComponentCount, bool bExpectedTexture, ETextShaderTextureType ExpectedTextureType, FCodeValue& OutValue, FString& OutError);
		bool CoerceValueToType(const FCodeValue& InValue, int32 ExpectedComponentCount, bool bExpectedTexture, ETextShaderTextureType ExpectedTextureType, bool bExpectedSubstrateMaterial, FCodeValue& OutValue, FString& OutError);
		bool EvaluateBraceInitializer(const FString& ConstructorType, const FString& InitializerText, FCodeValue& OutValue, FString& OutError);
		bool ResolveTargetTypeForAssignment(const FCodeStatement& Statement, FString& OutTypeName, FString& OutError) const;
		bool ResolveMaterialAttributesMemberType(const FString& MemberName, int32& OutComponentCount, FString& OutTypeName, FString& OutError) const;
		bool AssignMaterialAttributesMember(const FString& TargetName, const FCodeValue& InValue, FString& OutError);

		static bool TryFlattenQualifiedName(const TSharedPtr<FCodeExpression>& Expression, FString& OutName);
		bool TryExtractTextLiteral(const TSharedPtr<FCodeExpression>& Expression, FString& OutText) const;
		bool TryExtractLiteralText(const TSharedPtr<FCodeExpression>& Expression, FString& OutText) const;
		bool TryExtractAssetReferenceText(const TSharedPtr<FCodeExpression>& Expression, FString& OutText) const;
		bool TryExtractScalarLiteral(const TSharedPtr<FCodeExpression>& Expression, double& OutValue) const;
		bool TryExtractIntegerLiteral(const TSharedPtr<FCodeExpression>& Expression, int32& OutValue) const;
		bool TryExtractBooleanLiteral(const TSharedPtr<FCodeExpression>& Expression, bool& OutValue) const;
		static bool IsDefaultArgument(const TSharedPtr<FCodeExpression>& Expression);
		const FCodeCallArgument* FindNamedArgument(const TArray<FCodeCallArgument>& Arguments, const TCHAR* Name) const;
		const FCodeCallArgument* FindPositionalArgument(const TArray<FCodeCallArgument>& Arguments, int32 PositionIndex) const;
		bool ExecuteExpressionStatement(const TSharedPtr<FCodeExpression>& Expression, FString& OutError);
		// Runs ExecuteStatementInternal and records a probe for every name whose value the statement
		// changed (see RecordProbesForStatement). Nested statements (if bodies) go through here too.
		bool ExecuteStatement(const FCodeStatement& Statement, FString& OutError);
		bool ExecuteStatementInternal(const FCodeStatement& Statement, FString& OutError);
		void RecordProbesForStatement(const FCodeStatement& Statement, const TMap<FString, FCodeValue>& ValuesBefore);
		// Statement (line, column) mapped into the source file -- the same arithmetic
		// FormatStatementError uses for diagnostics, so a probe and an error on one statement agree.
		void ResolveStatementSourceLocation(const FCodeStatement& Statement, int32& OutLine, int32& OutColumn) const;
		FString FormatStatementError(const FCodeStatement& Statement, const FString& Error) const;
		bool ExecuteIfStatement(const FCodeStatement& Statement, FString& OutError);
		bool CreateConditionalValue(
			const FCodeCondition& Condition,
			const FCodeValue& TrueValue,
			const FCodeValue& FalseValue,
			FCodeValue& OutValue,
			FString& OutError);
		bool EvaluateExpression(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError);
		bool EvaluateUnary(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError);
		bool EvaluateBinary(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError);
		bool CreateBinaryOperatorNode(
			const FString& Operator,
			const FCodeValue& LeftValue,
			const FCodeValue& RightValue,
			FCodeValue& OutValue,
			FString& OutError);
		bool EvaluateMathBuiltinCall(const FString& FunctionName, const TArray<FCodeCallArgument>& Arguments, FCodeValue& OutValue, FString& OutError);
		bool TryBuildReusableCallKey(
			const FString& CallKind,
			const FString& FunctionName,
			const TArray<FCodeCallArgument>& Arguments,
			FString& OutKey) const;
		bool TryBuildReusableCallKey(
			const FString& CallKind,
			const FString& FunctionName,
			const TArray<FCodeCallArgument>& Arguments,
			const TSet<FString>& ExcludedNormalizedArgumentNames,
			FString& OutKey) const;
		bool BuildReusableExpressionToken(const TSharedPtr<FCodeExpression>& Expression, FString& OutToken) const;
		bool TryFindReusableExpressionValue(const FString& Key, FCodeValue& OutValue) const;
		void AddReusableExpressionValue(const FString& Key, const FCodeValue& Value);
		bool EvaluateMemberAccess(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError);
		bool CreateSingleChannelMask(
			const FCodeValue& BaseValue,
			int32 ChannelIndex,
			FCodeValue& OutValue,
			FString& OutError);
		bool AppendValues(const TArray<FCodeValue>& Parts, FCodeValue& OutValue, FString& OutError);
		bool CreateSwizzleExpression(
			const FCodeValue& BaseValue,
			const FString& Swizzle,
			FCodeValue& OutValue,
			FString& OutError);
		bool EvaluateCall(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError);
		static bool IsVectorConstructorName(const FString& InName);
		static bool IsIntegerConstructorName(const FString& InName);
		static int32 GetConstructorComponentCount(const FString& InName);
		bool EvaluateVectorConstructor(
			const FString& ConstructorName,
			const TArray<FCodeCallArgument>& Arguments,
			FCodeValue& OutValue,
			FString& OutError);
		const FTextShaderPropertyDefinition* FindPropertyDefinition(const FString& PropertyName) const;
		bool EvaluateStaticSwitchParameterCall(
			const FTextShaderPropertyDefinition& Property,
			const TArray<FCodeCallArgument>& Arguments,
			FCodeValue& OutValue,
			FString& OutError);
		// A declared parameter that owns input pins (channel/component mask, texture samples) can be
		// called with named arguments to wire those pins, e.g. Msk(Input=Col) or TexCube(Coordinates=Dir).
		// Asset slots (Texture/Curve/Font/...) are set via [Prop=Path(...)] metadata, not here.
		bool ParameterTypeAcceptsInputArguments(const FString& ParameterNodeType) const;
		bool EvaluateConfigurableParameterCall(
			const FTextShaderPropertyDefinition& Property,
			const TArray<FCodeCallArgument>& Arguments,
			FCodeValue& OutValue,
			FString& OutError);
		const FTextShaderFunctionDefinition* FindFunctionDefinition(const FString& FunctionName) const;
		const FTextShaderFunctionDefinition* FindGraphFunctionDefinition(const FString& FunctionName) const;
		const FTextShaderMaterialFunctionDefinition* FindMaterialFunctionDefinition(const FString& FunctionName) const;
		const FTextShaderVirtualFunctionDefinition* FindVirtualFunctionDefinition(const FString& FunctionName) const;
		bool EvaluateCustomFunctionCall(
			const FString& FunctionName,
			const TArray<FCodeCallArgument>& Arguments,
			FCodeValue& OutValue,
			FString& OutError);
		bool EvaluateGraphFunctionCall(
			const FTextShaderFunctionDefinition& Function,
			const TArray<FCodeCallArgument>& Arguments,
			FCodeValue& OutValue,
			FString& OutError);
		bool ExecuteCustomFunctionCall(
			const FTextShaderFunctionDefinition& Function,
			const TArray<FCodeCallArgument>& Arguments,
			FString& OutError);
		bool ExecuteGraphFunctionCall(
			const FTextShaderFunctionDefinition& Function,
			const TArray<FCodeCallArgument>& Arguments,
			FString& OutError);
		bool EvaluateMaterialFunctionCall(
			const FTextShaderMaterialFunctionDefinition& Function,
			const TArray<FCodeCallArgument>& Arguments,
			FCodeValue& OutValue,
			FString& OutError);
		bool ExecuteMaterialFunctionCall(
			const FTextShaderMaterialFunctionDefinition& Function,
			const TArray<FCodeCallArgument>& Arguments,
			FString& OutError);
		bool EvaluateVirtualFunctionCall(
			const FTextShaderVirtualFunctionDefinition& Function,
			const TArray<FCodeCallArgument>& Arguments,
			FCodeValue& OutValue,
			FString& OutError);
		bool ExecuteVirtualFunctionCall(
			const FTextShaderVirtualFunctionDefinition& Function,
			const TArray<FCodeCallArgument>& Arguments,
			FString& OutError);
		bool ExecuteMaterialFunctionCallAsset(
			const FString& CallKind,
			const FString& FunctionName,
			const FString& ObjectPath,
			const TArray<FTextShaderFunctionParameter>& Inputs,
			const TArray<FTextShaderFunctionParameter>& Outputs,
			const TArray<FCodeCallArgument>& Arguments,
			FString& OutError);
		bool CreateAndConnectMaterialFunctionCallAsset(
			const FString& CallKind,
			const FString& FunctionName,
			const FString& ObjectPath,
			const TArray<FTextShaderFunctionParameter>& Inputs,
			const TArray<FTextShaderFunctionParameter>& Outputs,
			const TArray<FCodeCallArgument>& InputArguments,
			UMaterialExpressionMaterialFunctionCall*& OutFunctionCall,
			FString& OutError);
		bool EvaluateMaterialFunctionCallAsset(
			const FString& CallKind,
			const FString& FunctionName,
			const FString& ObjectPath,
			const TArray<FTextShaderFunctionParameter>& Inputs,
			const TArray<FTextShaderFunctionParameter>& Outputs,
			const TArray<FCodeCallArgument>& Arguments,
			FCodeValue& OutValue,
			FString& OutError);
		static FString BuildFunctionArgumentList(const FTextShaderFunctionDefinition& Function, const TArray<FString>& ResultVariableNames);
		bool TryResolveVectorTransformBasis(const FString& InText, EMaterialVectorCoordTransformSource& OutSource) const;
		bool TryResolveVectorTransformTarget(const FString& InText, EMaterialVectorCoordTransform& OutTarget) const;
		bool TryResolvePositionTransformBasis(const FString& InText, EMaterialPositionTransformSource& OutBasis) const;
		bool EvaluateUEBuiltinCall(
			const FString& CalleeName,
			const TArray<FCodeCallArgument>& Arguments,
			FCodeValue& OutValue,
			FString& OutError);
	};
}
