#pragma once

#include "CoreMinimal.h"
#include "Math/Vector4.h"
#include "PixelFormat.h"
#include "UObject/ObjectPtr.h"

#include "DreamPassTypes.generated.h"

class UMaterialInterface;
class UTexture;

/**
 * Where in the frame a pass runs. Every pass picks its own; a pipeline only supplies the default. The spelling
 * a `.dsp` uses for each is LexToString below (`PostProcess.AfterTonemap`), and DreamShaderLang keeps the same
 * table for the binder -- DreamShader.Pass.Logic.InjectionNames checks the two agree.
 */
UENUM(BlueprintType)
enum class EDreamPassInjection : uint8
{
	/** PreRenderView_RenderThread: before visibility, no scene texture exists yet. Compute and utility passes. */
	BeginView,
	/** PreRenderBasePass_RenderThread: the depth prepass is done, the base pass has not started. */
	BeforeBasePass,
	/** PostRenderBasePassDeferred_RenderThread: the GBuffer is written, nothing is lit yet. */
	AfterBasePass,
	/** The renderer's post-opaque delegate: lighting, fog and sky are done, translucency is not. */
	AfterOpaque,
	/** PrePostProcessPass_RenderThread: everything but the post-process chain, for every view at once. */
	BeforePostProcess,
	PostProcessBeforeDOF UMETA(DisplayName = "PostProcess.BeforeDOF"),
	PostProcessAfterDOF UMETA(DisplayName = "PostProcess.AfterDOF"),
	PostProcessTranslucencyAfterDOF UMETA(DisplayName = "PostProcess.TranslucencyAfterDOF"),
	/** Replaces the tonemapper. Only one delegate of every extension runs here, so one pipeline at most. */
	PostProcessReplaceTonemapper UMETA(DisplayName = "PostProcess.ReplaceTonemapper"),
	PostProcessAfterMotionBlur UMETA(DisplayName = "PostProcess.AfterMotionBlur"),
	PostProcessAfterTonemap UMETA(DisplayName = "PostProcess.AfterTonemap"),
	PostProcessAfterFXAA UMETA(DisplayName = "PostProcess.AfterFXAA"),
	/** PostRenderView_RenderThread: the view family texture, LDR, output resolution, before the UI. */
	EndOfView,

	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDreamPassKind : uint8
{
	/** A Post Process material, or a `.usf` pixel shader, drawn over the view. */
	Fullscreen,
	/** A `.usf` compute shader through one of the global shader slots. */
	Compute,
	/** Selected primitives drawn again, with an override material or their own. */
	Mesh,
	/** A buffer cleared to a value. */
	Clear,
	/** One texture copied, or drawn scaled, into another. */
	Copy,
};

UENUM(BlueprintType)
enum class EDreamPassBufferFormat : uint8
{
	R8,
	RG8,
	RGBA8,
	R16F,
	RG16F,
	RGBA16F,
	R32F,
	RG32F,
	RGBA32F,
	R32U,
	RG32U,
	/** Depth only: the own depth of a mesh pass (`Depth = Own(Name)`). */
	Depth32,
};

/** What a buffer's size is relative to. */
UENUM(BlueprintType)
enum class EDreamPassBufferResolution : uint8
{
	/** The view rect the renderer draws at, before temporal upscaling. */
	Render,
	/** The view rect after temporal upscaling: what the post-process chain from SSRInput on, and EndOfView, draw at. */
	Output,
	/** `Size = int2(w, h)`, independent of the view. */
	Fixed,
};

/** Which views a pipeline runs in. A mask of these is the pipeline's `Views`. */
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EDreamPassViewFlags : uint8
{
	None = 0 UMETA(Hidden),
	/** Game viewports, PIE included. */
	Game = 1 << 0,
	/** Editor perspective and orthographic viewports. Never hit-proxy views. */
	Editor = 1 << 1,
	/** Scene captures, 2D and cube, whose capture source resolves the scene. */
	SceneCapture = 1 << 2,
	PlanarReflection = 1 << 3,
	ReflectionCapture = 1 << 4,
	Thumbnail = 1 << 5,
};
ENUM_CLASS_FLAGS(EDreamPassViewFlags);

/** What a view must offer for a pipeline to run in it at all. A mask of these is the pipeline's `Requires`. */
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EDreamPassRequirementFlags : uint8
{
	None = 0 UMETA(Hidden),
	/** The view runs the post-process chain (its PostProcess.* points exist). */
	PostProcess = 1 << 0,
	/** The view resolves the scene (BeforePostProcess exists). */
	SceneResolve = 1 << 1,
	/** r.CustomDepth is 3, so CustomStencil holds values. */
	CustomStencil = 1 << 2,
};
ENUM_CLASS_FLAGS(EDreamPassRequirementFlags);

UENUM(BlueprintType)
enum class EDreamPassParameterType : uint8
{
	Float,
	Float2,
	Float3,
	Float4,
	Int,
	Bool,
	Texture,
};

/** A pipeline parameter's value, its default, or the value an activation overrides it with. */
USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassParameterValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	EDreamPassParameterType Type = EDreamPassParameterType::Float;

	/** Float to Float4: the components in use; the rest are 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter", meta = (EditCondition = "Type != EDreamPassParameterType::Int && Type != EDreamPassParameterType::Bool && Type != EDreamPassParameterType::Texture", EditConditionHides))
	FVector4f Vector = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter", meta = (EditCondition = "Type == EDreamPassParameterType::Int", EditConditionHides))
	int32 Int = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter", meta = (EditCondition = "Type == EDreamPassParameterType::Bool", EditConditionHides))
	bool Bool = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter", meta = (EditCondition = "Type == EDreamPassParameterType::Texture", EditConditionHides))
	TObjectPtr<UTexture> Texture = nullptr;

	static FDreamPassParameterValue MakeFloat(float InValue);
	static FDreamPassParameterValue MakeVector(EDreamPassParameterType InType, const FVector4f& InValue);
	static FDreamPassParameterValue MakeInt(int32 InValue);
	static FDreamPassParameterValue MakeBool(bool bInValue);
	static FDreamPassParameterValue MakeTexture(UTexture* InTexture);

	bool IsNumeric() const { return Type != EDreamPassParameterType::Texture; }

	/** The value as a float4, as the HLSL slots and material scalar/vector parameters take it. Int and Bool as their number. */
	FVector4f AsVector() const;

	/** `Weight` of the way from this value to `Target`: numbers lerp, anything else switches at 0.5. Types must match. */
	FDreamPassParameterValue BlendTowards(const FDreamPassParameterValue& Target, float Weight) const;

	bool Identical(const FDreamPassParameterValue& Other) const;
};

/** One parameter a pipeline declares (`uniform` in a `.dsp`). */
USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassParameterDesc
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	FDreamPassParameterValue Default;

	/** `/// @group`, with `|` between nesting levels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	FString Group;

	/** `/// @desc`. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	bool bHasSlider = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter", meta = (EditCondition = "bHasSlider"))
	float SliderMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter", meta = (EditCondition = "bHasSlider"))
	float SliderMax = 1.0f;

	/** `/// @sort`. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	int32 SortPriority = 0;
};

/** A parameter an activation (the project settings, a volume, a component, an API call) sets for its pipeline. */
USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassParameterOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	FDreamPassParameterValue Value;
};

/** One named texture of a pipeline (`buffer` in a `.dsp`). */
USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassBufferDesc
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buffer")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buffer")
	EDreamPassBufferFormat Format = EDreamPassBufferFormat::RGBA16F;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buffer")
	EDreamPassBufferResolution Resolution = EDreamPassBufferResolution::Render;

	/** Relative to Resolution. Ignored for Fixed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buffer", meta = (ClampMin = "0.0625", ClampMax = "4.0", EditCondition = "Resolution != EDreamPassBufferResolution::Fixed"))
	float Scale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buffer", meta = (EditCondition = "Resolution == EDreamPassBufferResolution::Fixed"))
	FIntPoint FixedSize = FIntPoint(256, 256);

	/** Cleared to ClearValue when the frame first writes or reads it. False is `Clear = None`: a read before every write is an error. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buffer")
	bool bClear = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buffer", meta = (EditCondition = "bClear"))
	FLinearColor ClearValue = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buffer", meta = (ClampMin = "1", ClampMax = "14"))
	int32 Mips = 1;

	/** Kept from one frame to the next, per view; `<Name>.Previous` reads last frame's. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buffer")
	bool bHistory = false;

	/** Copied into ExportTarget after its last writer, for ordinary materials, Blueprints, UMG and Niagara to read. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buffer")
	bool bExport = false;

	/** `/// @desc`. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buffer")
	FString Description;
};

/** `read X = B;` / `write Y = B;`: the name a pass knows a texture by, and the buffer it is. */
USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassBufferBinding
{
	GENERATED_BODY()

	/**
	 * What the pass calls it: a UserSceneTexture name in a fullscreen material, an HLSL name in a `.usf`,
	 * Output0..3 in a mesh pass. Empty in a fullscreen material pass's one write.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding")
	FName Slot;

	/** A declared buffer, or a built-in one (SceneColor, SceneDepth, CustomDepth, CustomStencil, GBufferA..F, Velocity, Translucency). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding")
	FName Buffer;

	/** `B.Previous`: last frame's contents of a History buffer. Reads only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding")
	bool bPrevious = false;
};

UENUM(BlueprintType)
enum class EDreamPassParamSource : uint8
{
	/** A pipeline parameter, times Multiplier, plus Offset. */
	Parameter,
	/** A value folded when the `.dsp` compiled. */
	Constant,
	/** The pipeline's total weight in this view, times Multiplier, plus Offset. */
	Weight,
};

/** `param P = E;`: one input of a pass, and where its value comes from each frame. */
USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassParamBinding
{
	GENERATED_BODY()

	/** A material parameter name, or an HLSL parameter name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding")
	FName Target;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding")
	EDreamPassParamSource Source = EDreamPassParamSource::Parameter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding", meta = (EditCondition = "Source == EDreamPassParamSource::Parameter"))
	FName Parameter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding", meta = (EditCondition = "Source == EDreamPassParamSource::Constant"))
	FDreamPassParameterValue Constant;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding", meta = (EditCondition = "Source != EDreamPassParamSource::Constant"))
	float Multiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding", meta = (EditCondition = "Source != EDreamPassParamSource::Constant"))
	float Offset = 0.0f;
};

UENUM(BlueprintType)
enum class EDreamPassFilterKind : uint8
{
	/** Primitives that render CustomDepth with a stencil value matching Value under Mask. */
	Stencil,
	/** Primitives whose pass layer mask (UDreamPassLayerComponent) shares a bit with LayerMask. */
	Layer,
	/** Primitives registered under ListName (UDreamPassSubsystem::AddToList). */
	List,
};

USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassFilterTerm
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter")
	EDreamPassFilterKind Kind = EDreamPassFilterKind::Layer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter", meta = (ClampMin = "0", ClampMax = "255", EditCondition = "Kind == EDreamPassFilterKind::Stencil"))
	int32 StencilValue = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter", meta = (ClampMin = "0", ClampMax = "255", EditCondition = "Kind == EDreamPassFilterKind::Stencil"))
	int32 StencilMask = 255;

	/** Bits are the project settings' layer table: bit i is LayerNames[i]. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter", meta = (EditCondition = "Kind == EDreamPassFilterKind::Layer"))
	int32 LayerMask = 0;

	/** The layer names LayerMask was made from, kept so the decompiler can write them back as they were spelled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter", meta = (EditCondition = "Kind == EDreamPassFilterKind::Layer"))
	TArray<FName> LayerNames;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter", meta = (EditCondition = "Kind == EDreamPassFilterKind::List"))
	FName ListName;
};

/** Terms that must all hold (`a & b`). */
USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassFilterClause
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter")
	TArray<FDreamPassFilterTerm> AllOf;
};

/** A mesh pass's selection, in disjunctive normal form: any clause (`|`), each an `&` of terms. */
USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassMeshFilter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter")
	TArray<FDreamPassFilterClause> AnyOf;

	bool UsesStencil() const;
	bool IsEmpty() const { return AnyOf.IsEmpty(); }
};

UENUM(BlueprintType)
enum class EDreamPassMeshMode : uint8
{
	/** Every selected primitive is drawn with the pass's material. */
	Override,
	/** Every selected primitive is drawn with its own material; one without UE.DreamPassOutput is skipped. */
	Own,
	/** Its own material when that has UE.DreamPassOutput, the pass's material otherwise. */
	OwnOrOverride,
};

UENUM(BlueprintType)
enum class EDreamPassDepthMode : uint8
{
	/** Tested against the scene depth, read only. */
	TestScene,
	/** No depth test. */
	None,
	/** Tested and written against a Depth32 buffer of the pipeline, cleared on its first write. */
	Own,
};

UENUM(BlueprintType)
enum class EDreamPassCullMode : uint8
{
	/** As the primitive's own material and the primitive say, also when the pass draws an override material. */
	Auto,
	Back,
	Front,
	None,
};

UENUM(BlueprintType)
enum class EDreamPassBlendMode : uint8
{
	Replace,
	Add,
	Max,
	Min,
	AlphaBlend,
};

UENUM(BlueprintType)
enum class EDreamPassNanitePolicy : uint8
{
	/** A Nanite primitive is skipped; a warning says so once per pass. */
	Skip,
	/** No mesh is drawn; a fullscreen pass writes NaniteValue where CustomStencil matches the pass's stencil filter. */
	StencilMask,
	/** Nanite primitives of a layer or list get CustomDepth and AssignedStencilValue, then StencilMask. Collides with stencil the project uses itself. */
	AssignStencil,
};

/** The vertex factories a mesh pass expects to draw, which its override material needs usage flags for. */
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EDreamPassMeshUsageFlags : uint8
{
	None = 0 UMETA(Hidden),
	StaticMesh = 1 << 0,
	InstancedStaticMeshes = 1 << 1,
	SkeletalMesh = 1 << 2,
	Landscape = 1 << 3,
	SplineMesh = 1 << 4,
	GeometryCache = 1 << 5,
};
ENUM_CLASS_FLAGS(EDreamPassMeshUsageFlags);

USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassFullscreenSettings
{
	GENERATED_BODY()

	/** The Post Process material a material pass draws (`Material =`). Null for an HLSL pass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fullscreen")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	/**
	 * An HLSL pass's `.usf` as its `.dsp` names it: relative to the `.dsp`'s folder, or a virtual shader path. Kept for the
	 * way back to text and for the editor; what runs is the slot's snapshot of the file. Empty for a material pass.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fullscreen")
	FString ShaderPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fullscreen")
	FString Entry;

	/**
	 * The FDreamPassPS slot the compiler gave an HLSL pass, whatever its output count: a `.usf` always runs in a pixel
	 * shader slot, never through a material. INDEX_NONE for a material pass.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fullscreen")
	int32 PixelSlot = INDEX_NONE;
};

UENUM(BlueprintType)
enum class EDreamPassDispatchMode : uint8
{
	/** One thread per texel of DispatchBuffer, times DispatchScale. */
	Buffer,
	/** DispatchSize threads. */
	Fixed,
};

USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassComputeSettings
{
	GENERATED_BODY()

	/** The `.usf` as the `.dsp` names it: relative to the `.dsp`'s folder, or a virtual shader path. What runs is the slot's snapshot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compute")
	FString ShaderPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compute")
	FString Entry;

	/** The FDreamPassCS slot the compiler gave this shader. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compute")
	int32 Slot = INDEX_NONE;

	/** `[numthreads(x, y, z)]`, read from the source or written as `Threads = uint3(...)`. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compute")
	FIntVector ThreadGroupSize = FIntVector(8, 8, 1);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compute")
	EDreamPassDispatchMode DispatchMode = EDreamPassDispatchMode::Buffer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compute", meta = (EditCondition = "DispatchMode == EDreamPassDispatchMode::Buffer"))
	FName DispatchBuffer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compute", meta = (EditCondition = "DispatchMode == EDreamPassDispatchMode::Buffer"))
	float DispatchScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compute", meta = (EditCondition = "DispatchMode == EDreamPassDispatchMode::Fixed"))
	FIntVector DispatchSize = FIntVector(1, 1, 1);
};

USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassMeshSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	FDreamPassMeshFilter Filter;

	/** A Surface material with UE.DreamPassOutput. Required by Override and OwnOrOverride. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UMaterialInterface> OverrideMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	EDreamPassMeshMode Mode = EDreamPassMeshMode::Override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	EDreamPassDepthMode Depth = EDreamPassDepthMode::TestScene;

	/** The Depth32 buffer of `Depth = Own(Name)`. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (EditCondition = "Depth == EDreamPassDepthMode::Own"))
	FName DepthBuffer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	EDreamPassCullMode Cull = EDreamPassCullMode::Auto;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	EDreamPassBlendMode Blend = EDreamPassBlendMode::Replace;

	/** EDreamPassMeshUsageFlags. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (Bitmask, BitmaskEnum = "/Script/DreamShaderPass.EDreamPassMeshUsageFlags"))
	int32 Usage = int32(EDreamPassMeshUsageFlags::StaticMesh | EDreamPassMeshUsageFlags::InstancedStaticMeshes | EDreamPassMeshUsageFlags::SkeletalMesh);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	EDreamPassNanitePolicy Nanite = EDreamPassNanitePolicy::Skip;

	/** What StencilMask writes where a Nanite primitive is. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	FLinearColor NaniteValue = FLinearColor(1.0f, 0.0f, 0.0f, 0.0f);

	/** The stencil value AssignStencil gives the Nanite members of the filter's layers and lists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (ClampMin = "1", ClampMax = "255", EditCondition = "Nanite == EDreamPassNanitePolicy::AssignStencil"))
	int32 AssignedStencilValue = 255;
};

USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassClearSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clear")
	FLinearColor Value = FLinearColor::Transparent;
};

/** One pass of a pipeline (`pass` in a `.dsp`). Only the settings of its Kind are read. */
USTRUCT(BlueprintType)
struct DREAMSHADERPASS_API FDreamPassDesc
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pass")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pass")
	EDreamPassKind Kind = EDreamPassKind::Fullscreen;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pass")
	EDreamPassInjection Injection = EDreamPassInjection::BeforePostProcess;

	/** A Bool parameter; the pass is skipped in a view where it is false. None: always on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pass")
	FName EnabledParameter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pass")
	TArray<FDreamPassBufferBinding> Reads;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pass")
	TArray<FDreamPassBufferBinding> Writes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pass")
	TArray<FDreamPassParamBinding> Params;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pass", meta = (EditCondition = "Kind == EDreamPassKind::Fullscreen", EditConditionHides))
	FDreamPassFullscreenSettings Fullscreen;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pass", meta = (EditCondition = "Kind == EDreamPassKind::Compute", EditConditionHides))
	FDreamPassComputeSettings Compute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pass", meta = (EditCondition = "Kind == EDreamPassKind::Mesh", EditConditionHides))
	FDreamPassMeshSettings Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pass", meta = (EditCondition = "Kind == EDreamPassKind::Clear", EditConditionHides))
	FDreamPassClearSettings Clear;

	/** `/// @desc`. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pass")
	FString Description;
};

namespace UE::DreamPass
{
	/** The `.dsp` spelling of an injection point (`PostProcess.AfterTonemap`). */
	DREAMSHADERPASS_API const TCHAR* LexToString(EDreamPassInjection Injection);

	/** The inverse of LexToString; false for an unknown spelling. Case-sensitive, as the language is. */
	DREAMSHADERPASS_API bool LexTryParse(const FString& Text, EDreamPassInjection& OutInjection);

	/** Frame order: an earlier point runs before a later one in every view that has both. */
	inline int32 GetInjectionOrder(EDreamPassInjection Injection) { return int32(Injection); }

	/** Whether Injection is one of the post-process chain's subscriptions. */
	inline bool IsPostProcessInjection(EDreamPassInjection Injection)
	{
		return Injection >= EDreamPassInjection::PostProcessBeforeDOF && Injection <= EDreamPassInjection::PostProcessAfterFXAA;
	}

	/** Whether buffers at Injection are sized from the output (post-upscale) view rect rather than the render one. */
	DREAMSHADERPASS_API bool IsOutputResolutionInjection(EDreamPassInjection Injection);

	/** The `.dsp` spelling of a buffer format (`RGBA16F`). */
	DREAMSHADERPASS_API const TCHAR* LexToString(EDreamPassBufferFormat Format);
	DREAMSHADERPASS_API bool LexTryParse(const FString& Text, EDreamPassBufferFormat& OutFormat);

	/** The pixel format a buffer format is created with. */
	DREAMSHADERPASS_API EPixelFormat GetPixelFormat(EDreamPassBufferFormat Format);

	/** Names a pass can bind without declaring them. */
	namespace BuiltinBuffers
	{
		inline const FName SceneColor(TEXT("SceneColor"));
		inline const FName SceneDepth(TEXT("SceneDepth"));
		inline const FName CustomDepth(TEXT("CustomDepth"));
		inline const FName CustomStencil(TEXT("CustomStencil"));
		inline const FName GBufferA(TEXT("GBufferA"));
		inline const FName GBufferB(TEXT("GBufferB"));
		inline const FName GBufferC(TEXT("GBufferC"));
		inline const FName GBufferD(TEXT("GBufferD"));
		inline const FName GBufferE(TEXT("GBufferE"));
		inline const FName GBufferF(TEXT("GBufferF"));
		inline const FName Velocity(TEXT("Velocity"));
		inline const FName Translucency(TEXT("Translucency"));
	}

	DREAMSHADERPASS_API bool IsBuiltinBuffer(FName Name);

	/** The scalar material / HLSL parameter every pass receives: the pipeline's total weight in the view. */
	inline const FName WeightParameterName(TEXT("DreamPassWeight"));

	/** The shader tag UE.DreamPassOutput gives its material; the mesh pass shaders compile only for tagged materials. */
	inline const FName MaterialShaderTag(TEXT("DreamPass"));

	/** Number of UE.DreamPassOutput outputs, and so of a mesh pass's colour targets. */
	inline constexpr int32 MaxMeshOutputs = 4;

	/** Inputs a fullscreen material pass can bind besides SceneColor: the post-process material input slots 1..4. */
	inline constexpr int32 MaxMaterialInputs = 4;

	/** The fixed parameter block of the HLSL slots (Shaders/Pass/DreamPass.ush). */
	inline constexpr int32 MaxSlotInputs = 8;
	inline constexpr int32 MaxSlotOutputs = 4;
	inline constexpr int32 MaxSlotParamVectors = 16;
}
