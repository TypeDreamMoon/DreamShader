#include "DreamPassTypes.h"

#include "Engine/Texture.h"

FDreamPassParameterValue FDreamPassParameterValue::MakeFloat(float InValue)
{
	FDreamPassParameterValue Value;
	Value.Type = EDreamPassParameterType::Float;
	Value.Vector = FVector4f(InValue, 0.0f, 0.0f, 0.0f);
	return Value;
}

FDreamPassParameterValue FDreamPassParameterValue::MakeVector(EDreamPassParameterType InType, const FVector4f& InValue)
{
	FDreamPassParameterValue Value;
	Value.Type = InType;
	Value.Vector = InValue;
	// The components a narrower type does not have are kept at 0, so two equal values compare equal however they
	// were made.
	switch (InType)
	{
	case EDreamPassParameterType::Float:  Value.Vector.Y = 0.0f; [[fallthrough]];
	case EDreamPassParameterType::Float2: Value.Vector.Z = 0.0f; [[fallthrough]];
	case EDreamPassParameterType::Float3: Value.Vector.W = 0.0f; break;
	default: break;
	}
	return Value;
}

FDreamPassParameterValue FDreamPassParameterValue::MakeInt(int32 InValue)
{
	FDreamPassParameterValue Value;
	Value.Type = EDreamPassParameterType::Int;
	Value.Int = InValue;
	return Value;
}

FDreamPassParameterValue FDreamPassParameterValue::MakeBool(bool bInValue)
{
	FDreamPassParameterValue Value;
	Value.Type = EDreamPassParameterType::Bool;
	Value.Bool = bInValue;
	return Value;
}

FDreamPassParameterValue FDreamPassParameterValue::MakeTexture(UTexture* InTexture)
{
	FDreamPassParameterValue Value;
	Value.Type = EDreamPassParameterType::Texture;
	Value.Texture = InTexture;
	return Value;
}

FVector4f FDreamPassParameterValue::AsVector() const
{
	switch (Type)
	{
	case EDreamPassParameterType::Int:     return FVector4f(float(Int), 0.0f, 0.0f, 0.0f);
	case EDreamPassParameterType::Bool:    return FVector4f(Bool ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
	case EDreamPassParameterType::Texture: return FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	default:                               return Vector;
	}
}

FDreamPassParameterValue FDreamPassParameterValue::BlendTowards(const FDreamPassParameterValue& Target, float Weight) const
{
	if (Target.Type != Type || Weight <= 0.0f)
	{
		return *this;
	}
	if (Weight >= 1.0f)
	{
		return Target;
	}

	switch (Type)
	{
	case EDreamPassParameterType::Float:
	case EDreamPassParameterType::Float2:
	case EDreamPassParameterType::Float3:
	case EDreamPassParameterType::Float4:
	{
		FDreamPassParameterValue Result = *this;
		Result.Vector = Vector + (Target.Vector - Vector) * Weight;
		return Result;
	}
	case EDreamPassParameterType::Int:
	{
		// An Int is a count or an index more often than a quantity, so it switches like a Bool rather than lerping
		// through values neither side asked for.
		return Weight >= 0.5f ? Target : *this;
	}
	default:
		return Weight >= 0.5f ? Target : *this;
	}
}

bool FDreamPassParameterValue::Identical(const FDreamPassParameterValue& Other) const
{
	if (Type != Other.Type)
	{
		return false;
	}
	switch (Type)
	{
	case EDreamPassParameterType::Int:     return Int == Other.Int;
	case EDreamPassParameterType::Bool:    return Bool == Other.Bool;
	case EDreamPassParameterType::Texture: return Texture == Other.Texture;
	default:                               return Vector == Other.Vector;
	}
}

bool FDreamPassMeshFilter::UsesStencil() const
{
	for (const FDreamPassFilterClause& Clause : AnyOf)
	{
		for (const FDreamPassFilterTerm& Term : Clause.AllOf)
		{
			if (Term.Kind == EDreamPassFilterKind::Stencil)
			{
				return true;
			}
		}
	}
	return false;
}

bool FDreamPassMeshFilter::MatchesStencilTerms(uint32 Stencil) const
{
	for (const FDreamPassFilterClause& Clause : AnyOf)
	{
		bool bHasStencil = false;
		bool bMatches = true;
		for (const FDreamPassFilterTerm& Term : Clause.AllOf)
		{
			if (Term.Kind == EDreamPassFilterKind::Stencil)
			{
				bHasStencil = true;
				const uint32 Mask = uint32(Term.StencilMask) & 0xFFu;
				if ((Stencil & Mask) != (uint32(Term.StencilValue) & Mask))
				{
					bMatches = false;
					break;
				}
			}
		}
		if (bHasStencil && bMatches)
		{
			return true;
		}
	}
	return false;
}

namespace UE::DreamPass
{
	namespace Private
	{
		struct FInjectionName
		{
			EDreamPassInjection Injection;
			const TCHAR* Name;
		};

		// The `.dsp` spellings. DreamShaderLang has the same table (Lang/Private/Semantic/LangBinderPipeline.cpp);
		// DreamShader.Pass.Logic.InjectionNames fails when they drift apart.
		static const FInjectionName InjectionNames[] =
		{
			{ EDreamPassInjection::BeginView,                        TEXT("BeginView") },
			{ EDreamPassInjection::BeforeBasePass,                   TEXT("BeforeBasePass") },
			{ EDreamPassInjection::AfterBasePass,                    TEXT("AfterBasePass") },
			{ EDreamPassInjection::AfterOpaque,                      TEXT("AfterOpaque") },
			{ EDreamPassInjection::BeforePostProcess,                TEXT("BeforePostProcess") },
			{ EDreamPassInjection::PostProcessBeforeDOF,             TEXT("PostProcess.BeforeDOF") },
			{ EDreamPassInjection::PostProcessAfterDOF,              TEXT("PostProcess.AfterDOF") },
			{ EDreamPassInjection::PostProcessTranslucencyAfterDOF,  TEXT("PostProcess.TranslucencyAfterDOF") },
			{ EDreamPassInjection::PostProcessReplaceTonemapper,     TEXT("PostProcess.ReplaceTonemapper") },
			{ EDreamPassInjection::PostProcessAfterMotionBlur,       TEXT("PostProcess.AfterMotionBlur") },
			{ EDreamPassInjection::PostProcessAfterTonemap,          TEXT("PostProcess.AfterTonemap") },
			{ EDreamPassInjection::PostProcessAfterFXAA,             TEXT("PostProcess.AfterFXAA") },
			{ EDreamPassInjection::EndOfView,                        TEXT("EndOfView") },
		};
		static_assert(UE_ARRAY_COUNT(InjectionNames) == int32(EDreamPassInjection::Count), "Every injection point needs a spelling.");

		struct FFormatName
		{
			EDreamPassBufferFormat Format;
			const TCHAR* Name;
			EPixelFormat PixelFormat;
		};

		static const FFormatName FormatNames[] =
		{
			{ EDreamPassBufferFormat::R8,      TEXT("R8"),      PF_G8 },
			{ EDreamPassBufferFormat::RG8,     TEXT("RG8"),     PF_R8G8 },
			{ EDreamPassBufferFormat::RGBA8,   TEXT("RGBA8"),   PF_R8G8B8A8 },
			{ EDreamPassBufferFormat::R16F,    TEXT("R16F"),    PF_R16F },
			{ EDreamPassBufferFormat::RG16F,   TEXT("RG16F"),   PF_G16R16F },
			{ EDreamPassBufferFormat::RGBA16F, TEXT("RGBA16F"), PF_FloatRGBA },
			{ EDreamPassBufferFormat::R32F,    TEXT("R32F"),    PF_R32_FLOAT },
			{ EDreamPassBufferFormat::RG32F,   TEXT("RG32F"),   PF_G32R32F },
			{ EDreamPassBufferFormat::RGBA32F, TEXT("RGBA32F"), PF_A32B32G32R32F },
			{ EDreamPassBufferFormat::R32U,    TEXT("R32U"),    PF_R32_UINT },
			{ EDreamPassBufferFormat::RG32U,   TEXT("RG32U"),   PF_R32G32_UINT },
			{ EDreamPassBufferFormat::Depth32, TEXT("Depth32"), PF_DepthStencil },
		};
	}

	const TCHAR* LexToString(EDreamPassInjection Injection)
	{
		for (const Private::FInjectionName& Entry : Private::InjectionNames)
		{
			if (Entry.Injection == Injection)
			{
				return Entry.Name;
			}
		}
		return TEXT("?");
	}

	bool LexTryParse(const FString& Text, EDreamPassInjection& OutInjection)
	{
		for (const Private::FInjectionName& Entry : Private::InjectionNames)
		{
			if (Text.Equals(Entry.Name, ESearchCase::CaseSensitive))
			{
				OutInjection = Entry.Injection;
				return true;
			}
		}
		return false;
	}

	bool IsOutputResolutionInjection(EDreamPassInjection Injection)
	{
		// The post-process chain upscales at TSR, which runs after the TranslucencyAfterDOF subscription and before the
		// SSRInput one (R/Private/PostProcess/PostProcessing.cpp:1214-1240); every point after that, and the end of the
		// view, sees the output rect.
		switch (Injection)
		{
		case EDreamPassInjection::PostProcessReplaceTonemapper:
		case EDreamPassInjection::PostProcessAfterMotionBlur:
		case EDreamPassInjection::PostProcessAfterTonemap:
		case EDreamPassInjection::PostProcessAfterFXAA:
		case EDreamPassInjection::EndOfView:
			return true;
		default:
			return false;
		}
	}

	const TCHAR* LexToString(EDreamPassBufferFormat Format)
	{
		for (const Private::FFormatName& Entry : Private::FormatNames)
		{
			if (Entry.Format == Format)
			{
				return Entry.Name;
			}
		}
		return TEXT("?");
	}

	bool LexTryParse(const FString& Text, EDreamPassBufferFormat& OutFormat)
	{
		for (const Private::FFormatName& Entry : Private::FormatNames)
		{
			if (Text.Equals(Entry.Name, ESearchCase::CaseSensitive))
			{
				OutFormat = Entry.Format;
				return true;
			}
		}
		return false;
	}

	EPixelFormat GetPixelFormat(EDreamPassBufferFormat Format)
	{
		for (const Private::FFormatName& Entry : Private::FormatNames)
		{
			if (Entry.Format == Format)
			{
				return Entry.PixelFormat;
			}
		}
		return PF_FloatRGBA;
	}

	bool IsBuiltinBuffer(FName Name)
	{
		using namespace BuiltinBuffers;
		return Name == SceneColor || Name == SceneDepth || Name == CustomDepth || Name == CustomStencil
			|| Name == GBufferA || Name == GBufferB || Name == GBufferC || Name == GBufferD || Name == GBufferE || Name == GBufferF
			|| Name == Velocity || Name == Translucency;
	}
}
