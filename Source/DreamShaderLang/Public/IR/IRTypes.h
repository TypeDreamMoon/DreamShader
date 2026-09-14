// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The type a value has in the DreamShader IR.
//
// This is the resolved form of Lang::FTypeRef: the same shape, minus the source span, plus the two
// things a spelling alone cannot carry -- a resolved user struct, and the "node" type a multi-output
// reflected call has before one of its outputs is selected. The binder produces these; the IR
// builder and the emitter consume them.
//
// Two levels of fidelity share this one struct on purpose. While binding, the type is the HLSL
// truth (int, bool, half, a 3x3 matrix) so type errors are honest and a `/// @custom` body gets the
// signature its author wrote. While lowering, the graph's narrower model applies: every numeric
// value is float1..4, a bool is 0/1, an int is a float, a matrix is legal only inside a custom node.
// GraphComponentCount() is that narrowing, written down once.
//
// FROZEN for batch 1 (M2+M3). Add nothing here without telling the orchestrator: three agents
// compile against it.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangAst.h"

namespace UE::DreamShader::IR
{
	enum class EIRTypeKind : uint8
	{
		Void,
		Bool,
		Int,
		UInt,
		Float,
		Half,
		Double,
		/** Texture2D / TextureCube / Texture2DArray / Texture3D / VolumeTexture; Texture says which. */
		Texture,
		SamplerState,
		/** The `material` builtin: a MaterialAttributes value. */
		Material,
		/** A Substrate BSDF / operator result. Opaque: only Substrate pins accept it. */
		Substrate,
		/** A user `struct`; StructIndex says which. Exists only before flattening. */
		Struct,
		/** A multi-output reflected node before an output was selected; CatalogIndex says which class. */
		Node,
		/** The binder could not type this. A diagnostic was already reported; do not report again. */
		Error,
	};

	DREAMSHADERLANG_API const TCHAR* LexToString(EIRTypeKind Kind);

	struct DREAMSHADERLANG_API FIRType
	{
		EIRTypeKind Kind = EIRTypeKind::Error;
		/** Components of a vector, rows of a matrix. 1 for a scalar. */
		int32 Rows = 1;
		/** Columns of a matrix. 1 for scalars and vectors. */
		int32 Cols = 1;
		Lang::ETextureKind Texture = Lang::ETextureKind::None;
		/** Kind == Struct: index into FBoundModule::Structs. */
		int32 StructIndex = INDEX_NONE;
		/** Kind == Node: index into FBuiltinCatalog::Expressions. */
		int32 CatalogIndex = INDEX_NONE;

		bool IsNumeric() const;
		bool IsScalar() const { return IsNumeric() && Rows == 1 && Cols == 1; }
		bool IsVector() const { return IsNumeric() && Rows > 1 && Cols == 1; }
		bool IsMatrix() const { return IsNumeric() && Cols > 1; }
		bool IsBool() const { return Kind == EIRTypeKind::Bool; }
		bool IsIntegral() const { return Kind == EIRTypeKind::Int || Kind == EIRTypeKind::UInt; }
		bool IsTexture() const { return Kind == EIRTypeKind::Texture; }
		bool IsMaterial() const { return Kind == EIRTypeKind::Material; }
		bool IsStruct() const { return Kind == EIRTypeKind::Struct; }
		bool IsNode() const { return Kind == EIRTypeKind::Node; }
		bool IsError() const { return Kind == EIRTypeKind::Error; }
		bool IsVoid() const { return Kind == EIRTypeKind::Void; }
		int32 NumComponents() const { return Rows * Cols; }

		/** 1..4 for a numeric value the graph can carry; 0 for everything else, matrices included. */
		int32 GraphComponentCount() const;

		/** float, float3, float4x4, bool2, Texture2D, SamplerState, material, Substrate, struct#2, node#17, void, <error>. */
		FString ToString() const;

		bool operator==(const FIRType& Other) const;
		bool operator!=(const FIRType& Other) const { return !(*this == Other); }

		static FIRType Void();
		static FIRType Scalar(EIRTypeKind InKind);
		static FIRType Vector(EIRTypeKind InKind, int32 Components);
		static FIRType Matrix(EIRTypeKind InKind, int32 InRows, int32 InCols);
		static FIRType TextureOf(Lang::ETextureKind InKind);
		static FIRType Sampler();
		static FIRType Material();
		static FIRType Substrate();
		static FIRType Struct(int32 Index);
		static FIRType Node(int32 InCatalogIndex);
		static FIRType Error();

		/** float1..4: the graph's own type, what every numeric value becomes after lowering. */
		static FIRType Float(int32 Components = 1);
		static FIRType Bool(int32 Components = 1);
	};

	/**
	 * A spelled type, resolved against nothing: the builtin categories map directly, and a Named
	 * type becomes Error. The binder resolves Named types itself, because it is the one that knows
	 * the structs. So this is the right call for `float3` and the wrong one for `ToonInputs`.
	 */
	DREAMSHADERLANG_API FIRType TypeFromBuiltinRef(const Lang::FTypeRef& Ref);

	/**
	 * Whether a value of From may be passed where To is expected without an explicit cast, and
	 * what it costs. Implements the HLSL rules the graph can honour: a scalar broadcasts to any
	 * vector; int/uint/bool/half/double convert to float; a vector never narrows silently (`float4`
	 * into a `float3` pin is an error -- write `.rgb`); a Node with a default output converts to
	 * that output's type. Textures, samplers, material, Substrate and structs convert only to
	 * themselves. Error converts to everything, so one bad expression does not cascade.
	 */
	enum class EIRConversion : uint8
	{
		Identity,
		/** A scalar used as a vector: the emitter sees a float1 feeding a wider pin, which the graph broadcasts. */
		Broadcast,
		/** Numeric kind change (int -> float, bool -> float); no graph node, floats are all there is. */
		Numeric,
		/** Node -> its default (index 0) output. */
		DefaultOutput,
		None,
	};
	DREAMSHADERLANG_API EIRConversion ClassifyConversion(const FIRType& From, const FIRType& To);
}
