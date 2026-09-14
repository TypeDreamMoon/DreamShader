// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FIRType: the spelling table, the graph narrowing and the implicit-conversion rules.
//
// Three things live here and nowhere else:
//
//   1. GraphComponentCount() -- the ONE place "what the material graph can carry" is written down.
//      A matrix answers 0, a texture answers 0, a bool4 answers 4. Every refusal downstream
//      (DSH4306, DSH4361) reads this rather than re-deriving it.
//   2. ToString() -- the spelling every diagnostic, dump and manifest uses for a type. The catalog
//      manifest parses it back (IRCatalog.cpp), so it is a wire format, not decoration.
//   3. ClassifyConversion() -- the HLSL implicit-conversion rules, narrowed to what a graph can
//      express. The binder asks it once per argument; nothing else may hold a copy of the rules.

#include "IR/IRTypes.h"

#include "Lang/LangAst.h"

namespace UE::DreamShader::IR
{
	namespace Private
	{
		/** The base spelling of a numeric kind: what goes in front of the width suffix. */
		static const TCHAR* NumericKindSpelling(const EIRTypeKind Kind)
		{
			switch (Kind)
			{
			case EIRTypeKind::Bool:   return TEXT("bool");
			case EIRTypeKind::Int:    return TEXT("int");
			case EIRTypeKind::UInt:   return TEXT("uint");
			case EIRTypeKind::Float:  return TEXT("float");
			case EIRTypeKind::Half:   return TEXT("half");
			case EIRTypeKind::Double: return TEXT("double");
			default:                  return TEXT("float");
			}
		}

		/** `Texture2D`, `TextureCube`...; a texture whose kind was never resolved is plain `Texture`. */
		static const TCHAR* TextureKindSpelling(const Lang::ETextureKind Kind)
		{
			switch (Kind)
			{
			case Lang::ETextureKind::Texture2D:      return TEXT("Texture2D");
			case Lang::ETextureKind::TextureCube:    return TEXT("TextureCube");
			case Lang::ETextureKind::Texture2DArray: return TEXT("Texture2DArray");
			case Lang::ETextureKind::Texture3D:      return TEXT("Texture3D");
			case Lang::ETextureKind::VolumeTexture:  return TEXT("VolumeTexture");
			case Lang::ETextureKind::None:
			default:                                 return TEXT("Texture");
			}
		}

		/** `Lang::EScalarKind` -> the IR kind. A scalar with no kind is malformed, so it types as Error. */
		static EIRTypeKind KindFromScalar(const Lang::EScalarKind Scalar)
		{
			switch (Scalar)
			{
			case Lang::EScalarKind::Float:  return EIRTypeKind::Float;
			case Lang::EScalarKind::Half:   return EIRTypeKind::Half;
			case Lang::EScalarKind::Double: return EIRTypeKind::Double;
			case Lang::EScalarKind::Int:    return EIRTypeKind::Int;
			case Lang::EScalarKind::UInt:   return EIRTypeKind::UInt;
			case Lang::EScalarKind::Bool:   return EIRTypeKind::Bool;
			case Lang::EScalarKind::None:
			default:                        return EIRTypeKind::Error;
			}
		}
	}

	const TCHAR* LexToString(const EIRTypeKind Kind)
	{
		switch (Kind)
		{
		case EIRTypeKind::Void:         return TEXT("Void");
		case EIRTypeKind::Bool:         return TEXT("Bool");
		case EIRTypeKind::Int:          return TEXT("Int");
		case EIRTypeKind::UInt:         return TEXT("UInt");
		case EIRTypeKind::Float:        return TEXT("Float");
		case EIRTypeKind::Half:         return TEXT("Half");
		case EIRTypeKind::Double:       return TEXT("Double");
		case EIRTypeKind::Texture:      return TEXT("Texture");
		case EIRTypeKind::SamplerState: return TEXT("SamplerState");
		case EIRTypeKind::Material:     return TEXT("Material");
		case EIRTypeKind::Substrate:    return TEXT("Substrate");
		case EIRTypeKind::Struct:       return TEXT("Struct");
		case EIRTypeKind::Node:         return TEXT("Node");
		case EIRTypeKind::Error:        return TEXT("Error");
		}

		return TEXT("Error");
	}

	bool FIRType::IsNumeric() const
	{
		// Bool counts. HLSL lets a bool into arithmetic, `bool2` is a vector the parser produces,
		// and the graph carries a bool as a 0/1 float -- so every predicate built on IsNumeric()
		// (IsScalar, IsVector, IsMatrix, GraphComponentCount) has to see it as numeric or a
		// `bool3` stops being a vector halfway down the pipeline.
		switch (Kind)
		{
		case EIRTypeKind::Bool:
		case EIRTypeKind::Int:
		case EIRTypeKind::UInt:
		case EIRTypeKind::Float:
		case EIRTypeKind::Half:
		case EIRTypeKind::Double:
			return true;
		default:
			return false;
		}
	}

	int32 FIRType::GraphComponentCount() const
	{
		if (!IsNumeric())
		{
			return 0;
		}
		if (Cols != 1)
		{
			// A matrix. The graph has no matrix wires at all (decision §2 "Matrices"), so this is
			// 0 rather than Rows*Cols: a caller asking "can the graph carry this" must hear no.
			return 0;
		}
		if (Rows < 1 || Rows > 4)
		{
			return 0;
		}
		return Rows;
	}

	FString FIRType::ToString() const
	{
		switch (Kind)
		{
		case EIRTypeKind::Void:
			return TEXT("void");
		case EIRTypeKind::Error:
			return TEXT("<error>");
		case EIRTypeKind::Texture:
			return Private::TextureKindSpelling(Texture);
		case EIRTypeKind::SamplerState:
			return TEXT("SamplerState");
		case EIRTypeKind::Material:
			return TEXT("material");
		case EIRTypeKind::Substrate:
			return TEXT("Substrate");
		case EIRTypeKind::Struct:
			return FString::Printf(TEXT("struct#%d"), StructIndex);
		case EIRTypeKind::Node:
			return FString::Printf(TEXT("node#%d"), CatalogIndex);
		default:
			break;
		}

		const TCHAR* Base = Private::NumericKindSpelling(Kind);
		if (Cols > 1)
		{
			return FString::Printf(TEXT("%s%dx%d"), Base, Rows, Cols);
		}
		if (Rows > 1)
		{
			return FString::Printf(TEXT("%s%d"), Base, Rows);
		}
		return Base;
	}

	bool FIRType::operator==(const FIRType& Other) const
	{
		if (Kind != Other.Kind)
		{
			return false;
		}

		// Only the fields the kind gives meaning to take part. A Material built by hand with a
		// stray Rows is still the same material; a Texture2D and a TextureCube are not the same
		// texture even though both are Kind == Texture.
		switch (Kind)
		{
		case EIRTypeKind::Bool:
		case EIRTypeKind::Int:
		case EIRTypeKind::UInt:
		case EIRTypeKind::Float:
		case EIRTypeKind::Half:
		case EIRTypeKind::Double:
			return Rows == Other.Rows && Cols == Other.Cols;
		case EIRTypeKind::Texture:
			return Texture == Other.Texture;
		case EIRTypeKind::Struct:
			return StructIndex == Other.StructIndex;
		case EIRTypeKind::Node:
			return CatalogIndex == Other.CatalogIndex;
		default:
			return true;
		}
	}

	FIRType FIRType::Void()
	{
		FIRType Result;
		Result.Kind = EIRTypeKind::Void;
		return Result;
	}

	FIRType FIRType::Scalar(const EIRTypeKind InKind)
	{
		FIRType Result;
		Result.Kind = InKind;
		Result.Rows = 1;
		Result.Cols = 1;
		return Result;
	}

	FIRType FIRType::Vector(const EIRTypeKind InKind, const int32 Components)
	{
		FIRType Result;
		Result.Kind = InKind;
		Result.Rows = Components;
		Result.Cols = 1;
		return Result;
	}

	FIRType FIRType::Matrix(const EIRTypeKind InKind, const int32 InRows, const int32 InCols)
	{
		FIRType Result;
		Result.Kind = InKind;
		Result.Rows = InRows;
		Result.Cols = InCols;
		return Result;
	}

	FIRType FIRType::TextureOf(const Lang::ETextureKind InKind)
	{
		FIRType Result;
		Result.Kind = EIRTypeKind::Texture;
		Result.Texture = InKind;
		return Result;
	}

	FIRType FIRType::Sampler()
	{
		FIRType Result;
		Result.Kind = EIRTypeKind::SamplerState;
		return Result;
	}

	FIRType FIRType::Material()
	{
		FIRType Result;
		Result.Kind = EIRTypeKind::Material;
		return Result;
	}

	FIRType FIRType::Substrate()
	{
		FIRType Result;
		Result.Kind = EIRTypeKind::Substrate;
		return Result;
	}

	FIRType FIRType::Struct(const int32 Index)
	{
		FIRType Result;
		Result.Kind = EIRTypeKind::Struct;
		Result.StructIndex = Index;
		return Result;
	}

	FIRType FIRType::Node(const int32 InCatalogIndex)
	{
		FIRType Result;
		Result.Kind = EIRTypeKind::Node;
		Result.CatalogIndex = InCatalogIndex;
		return Result;
	}

	FIRType FIRType::Error()
	{
		FIRType Result;
		Result.Kind = EIRTypeKind::Error;
		return Result;
	}

	FIRType FIRType::Float(const int32 Components)
	{
		return Vector(EIRTypeKind::Float, Components);
	}

	FIRType FIRType::Bool(const int32 Components)
	{
		return Vector(EIRTypeKind::Bool, Components);
	}

	FIRType TypeFromBuiltinRef(const Lang::FTypeRef& Ref)
	{
		switch (Ref.Category)
		{
		case Lang::ETypeCategory::Void:
			return FIRType::Void();
		case Lang::ETypeCategory::Scalar:
			return FIRType::Scalar(Private::KindFromScalar(Ref.Scalar));
		case Lang::ETypeCategory::Vector:
			return FIRType::Vector(Private::KindFromScalar(Ref.Scalar), Ref.Rows);
		case Lang::ETypeCategory::Matrix:
			return FIRType::Matrix(Private::KindFromScalar(Ref.Scalar), Ref.Rows, Ref.Cols);
		case Lang::ETypeCategory::Texture:
			return FIRType::TextureOf(Ref.Texture);
		case Lang::ETypeCategory::Sampler:
			return FIRType::Sampler();
		case Lang::ETypeCategory::Material:
			return FIRType::Material();
		case Lang::ETypeCategory::Substrate:
			return FIRType::Substrate();
		case Lang::ETypeCategory::Named:
			// Deliberate: a Named type is a user struct or a typo, and only the binder knows which.
			// Answering Error here (rather than guessing) is what keeps the struct table out of
			// this module.
			return FIRType::Error();
		}

		return FIRType::Error();
	}

	EIRConversion ClassifyConversion(const FIRType& From, const FIRType& To)
	{
		if (From == To)
		{
			return EIRConversion::Identity;
		}

		// One bad expression must not cascade: an Error was already reported, so it goes anywhere
		// and takes nothing with it. The same applies the other way round -- a pin the binder could
		// not type must not add a second diagnostic on top of the first.
		if (From.IsError() || To.IsError())
		{
			return EIRConversion::Identity;
		}

		// A multi-output call used as a value reads its default output. The caller is the one that
		// knows what that output's type is -- it passes it as To -- so all this decides is that a
		// Node is allowed to decay at all. Node -> Node of a different class is not a conversion.
		if (From.IsNode())
		{
			return To.IsNode() ? EIRConversion::None : EIRConversion::DefaultOutput;
		}

		// A texture pin that does not name a kind (ECatalogValueType::Texture) accepts any texture.
		// Without this a `Texture2D` argument could not reach a reflected `TextureObject` pin,
		// which the catalog can only describe as "a texture".
		if (From.IsTexture() && To.IsTexture() && To.Texture == Lang::ETextureKind::None)
		{
			return EIRConversion::Identity;
		}

		if (!From.IsNumeric() || !To.IsNumeric())
		{
			// Textures of different kinds, samplers, material, Substrate, structs and void: only
			// their own type, which the identity check above already answered.
			return EIRConversion::None;
		}

		if (From.Cols != 1 || To.Cols != 1)
		{
			// At least one matrix. A matrix converts only to a matrix of the same shape (a kind
			// change, `half3x3` -> `float3x3`); there is no scalar-to-matrix broadcast here,
			// because a matrix only ever exists as a `@custom` signature element.
			return (From.Rows == To.Rows && From.Cols == To.Cols) ? EIRConversion::Numeric : EIRConversion::None;
		}

		if (From.Rows == To.Rows)
		{
			// Same width; the kinds must differ or the identity check would have caught it.
			return EIRConversion::Numeric;
		}

		if (From.Rows == 1 && To.Rows > 1)
		{
			// A scalar spreads across a wider pin. Any kind change rides along with it, which is
			// why this is Broadcast and not Numeric: the emitter has one thing to do, not two.
			return EIRConversion::Broadcast;
		}

		// float4 into a float3 pin, or float2 into a float3 pin. Neither is silent in HLSL and
		// neither is silent here: write the swizzle.
		return EIRConversion::None;
	}
}
