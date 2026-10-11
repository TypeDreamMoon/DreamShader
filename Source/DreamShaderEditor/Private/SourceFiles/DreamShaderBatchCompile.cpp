// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "SourceFiles/DreamShaderBatchCompile.h"

#include "DreamShaderModule.h"

namespace UE::DreamShader::Editor::Private
{
	void SortDreamShaderSourcesForBatch(TArray<FString>& InOutSourceFiles)
	{
		InOutSourceFiles.Sort([](const FString& Left, const FString& Right)
		{
			const int32 LeftRank = UE::DreamShader::IsDreamShaderFunctionFile(Left) ? 0 : 1;
			const int32 RightRank = UE::DreamShader::IsDreamShaderFunctionFile(Right) ? 0 : 1;
			if (LeftRank != RightRank)
			{
				return LeftRank < RightRank;
			}

			return Left.Compare(Right, ESearchCase::IgnoreCase) < 0;
		});
	}

	int32 CompileDreamShaderSourcesInBatch(
		IDreamShaderCompiler& Compiler,
		const TArray<FString>& SourceFiles,
		const FDreamShaderCompileRequest& RequestTemplate,
		TFunctionRef<void(const FDreamShaderBatchCompileEntry&)> OnFinal)
	{
		TArray<FDreamShaderBatchCompileEntry> Pending;
		Pending.Reserve(SourceFiles.Num());
		for (const FString& SourceFile : SourceFiles)
		{
			FDreamShaderBatchCompileEntry& Entry = Pending.AddDefaulted_GetRef();
			Entry.SourceFile = SourceFile;
		}

		// Each pass either builds at least one more source or ends the loop, so it runs at most
		// SourceFiles.Num() + 1 times; in practice a project without cross-file calls needs one, and a
		// failing source costs one extra attempt.
		while (!Pending.IsEmpty())
		{
			TArray<FDreamShaderBatchCompileEntry> Failed;
			int32 SucceededThisPass = 0;
			for (FDreamShaderBatchCompileEntry& Entry : Pending)
			{
				FDreamShaderCompileRequest Request = RequestTemplate;
				Request.SourceFilePath = Entry.SourceFile;
				Entry.Result = Compiler.CompileAssets(Request);
				++Entry.Attempts;

				if (Entry.Result.bSucceeded)
				{
					++SucceededThisPass;
					OnFinal(Entry);
				}
				else
				{
					Failed.Add(MoveTemp(Entry));
				}
			}

			if (Failed.IsEmpty() || SucceededThisPass == 0)
			{
				// Nothing new on disk: another pass would compile the same failures against the same
				// assets. What is left is final.
				for (const FDreamShaderBatchCompileEntry& Entry : Failed)
				{
					OnFinal(Entry);
				}
				return Failed.Num();
			}

			UE_LOG(LogDreamShader, Display,
				TEXT("DreamShader: retrying %d source file(s) that failed before %d other(s) in the same pass were built; they may depend on them."),
				Failed.Num(), SucceededThisPass);
			Pending = MoveTemp(Failed);
		}

		return 0;
	}
}
