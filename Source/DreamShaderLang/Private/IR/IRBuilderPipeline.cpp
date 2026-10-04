// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The PassPipeline product of a `.dsp`.
//
// A pipeline is data, not a graph: the binder (Semantic/LangBinderPipeline.cpp) built the payload -- canonical, every
// default applied, every engine fact the host resolved copied in -- and nothing in it lowers to a node. What this adds is
// what every product gets here: its own source reference, and every file a source reference names written the way an
// asset may carry it (StampFile), so the payload is the same on every machine and in every checkout.
//
// Diagnostics owned by this file: none. Everything a pipeline can get wrong was reported by the binder.

#include "IRBuilderInternal.h"

namespace UE::DreamShader::IR::Private
{
	void FIRBuilder::BuildPipelineProduct(const FBoundProduct& BoundProduct, FIRProduct& OutProduct)
	{
		const FBoundPipeline& Pipeline = BoundModule.Pipeline;

		OutProduct.Graph = FIRGraph();
		OutProduct.Backend = BoundProduct.Backend;
		OutProduct.Source.File = StampFile(SourceFile);
		OutProduct.Source.Span = Pipeline.Pragma ? Pipeline.Pragma->Span : Lang::FLangSpan();

		FIRPassPipeline& Out = OutProduct.PassPipeline;
		Out = Pipeline.Payload;

		const auto Stamp = [this](FIRSourceRef& Source)
		{
			Source.File = StampFile(Source.File.IsEmpty() ? SourceFile : Source.File);
		};
		for (FIRPassParameter& Parameter : Out.Parameters)
		{
			Stamp(Parameter.Source);
		}
		for (FIRPassBuffer& Buffer : Out.Buffers)
		{
			Stamp(Buffer.Source);
		}
		for (FIRPass& Pass : Out.Passes)
		{
			Stamp(Pass.Source);
			for (FIRPassBinding& Binding : Pass.Reads)
			{
				Stamp(Binding.Source);
			}
			for (FIRPassBinding& Binding : Pass.Writes)
			{
				Stamp(Binding.Source);
			}
			for (FIRPassParam& Param : Pass.Params)
			{
				Stamp(Param.Source);
			}
		}
	}
}
