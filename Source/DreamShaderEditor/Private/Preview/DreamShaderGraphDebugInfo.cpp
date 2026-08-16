#include "Preview/DreamShaderGraphDebugInfo.h"

#include "DreamShaderModule.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"

namespace UE::DreamShader::Editor::Private
{
	const FDreamShaderGraphProbe* FDreamShaderGraphDebugTable::ResolveProbe(const int32 Line, const FString& PreferredName) const
	{
		if (Probes.IsEmpty() || Line <= 0)
		{
			return nullptr;
		}

		// Probes are kept sorted by line, so the first index at or past `Line` is the candidate line.
		int32 FirstIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Probes.Num(); ++Index)
		{
			if (Probes[Index].Line >= Line)
			{
				FirstIndex = Index;
				break;
			}
		}
		if (FirstIndex == INDEX_NONE)
		{
			return nullptr;
		}

		const int32 ResolvedLine = Probes[FirstIndex].Line;
		const FDreamShaderGraphProbe* Target = nullptr;
		const FDreamShaderGraphProbe* Last = nullptr;
		for (int32 Index = FirstIndex; Index < Probes.Num() && Probes[Index].Line == ResolvedLine; ++Index)
		{
			const FDreamShaderGraphProbe& Probe = Probes[Index];
			if (!PreferredName.IsEmpty() && Probe.Name.Equals(PreferredName, ESearchCase::CaseSensitive))
			{
				return &Probe;
			}
			if (Probe.bIsStatementTarget)
			{
				Target = &Probe;
			}
			Last = &Probe;
		}
		return Target ? Target : Last;
	}

	void FDreamShaderGraphDebugTable::GetProbesOnLine(const int32 Line, TArray<const FDreamShaderGraphProbe*>& OutProbes) const
	{
		OutProbes.Reset();
		for (const FDreamShaderGraphProbe& Probe : Probes)
		{
			if (Probe.Line == Line)
			{
				OutProbes.Add(&Probe);
			}
			else if (Probe.Line > Line)
			{
				break;
			}
		}
	}

	FDreamShaderGraphDebugRegistry& FDreamShaderGraphDebugRegistry::Get()
	{
		static FDreamShaderGraphDebugRegistry Registry;
		return Registry;
	}

	void FDreamShaderGraphDebugRegistry::Publish(const FString& SourceFilePath, UMaterial* GraphMaterial, TArray<FDreamShaderGraphProbe>&& Probes)
	{
		check(IsInGameThread());
		const FString Key = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		if (Key.IsEmpty())
		{
			return;
		}

		// Stable sort: the builder appends in execution order, and two probes on one line keep that
		// order as their tie-break, which is what "last binding on the line" relies on.
		Probes.StableSort([](const FDreamShaderGraphProbe& A, const FDreamShaderGraphProbe& B)
		{
			return A.Line != B.Line ? A.Line < B.Line : A.Column < B.Column;
		});

		TSharedPtr<FDreamShaderGraphDebugTable> Table = MakeShared<FDreamShaderGraphDebugTable>();
		Table->SourceFilePath = Key;
		Table->GraphMaterial = GraphMaterial;
		Table->Probes = MoveTemp(Probes);
		Table->Generation = NextGeneration++;
		Tables.Add(Key, Table);

		UE_LOG(LogDreamShader, Verbose, TEXT("DreamShader debug table published for '%s': %d probe(s), generation %llu."), *Key, Table->Probes.Num(), Table->Generation);
		OnTablePublished.Broadcast(*Table);
	}

	void FDreamShaderGraphDebugRegistry::Remove(const FString& SourceFilePath)
	{
		check(IsInGameThread());
		Tables.Remove(UE::DreamShader::NormalizeSourceFilePath(SourceFilePath));
	}

	TSharedPtr<const FDreamShaderGraphDebugTable> FDreamShaderGraphDebugRegistry::Find(const FString& SourceFilePath) const
	{
		const TSharedPtr<FDreamShaderGraphDebugTable>* Found = Tables.Find(UE::DreamShader::NormalizeSourceFilePath(SourceFilePath));
		return Found ? *Found : nullptr;
	}

	TSharedPtr<const FDreamShaderGraphDebugTable> FDreamShaderGraphDebugRegistry::FindByGraphMaterial(const UMaterial* GraphMaterial) const
	{
		if (!GraphMaterial)
		{
			return nullptr;
		}
		for (const TPair<FString, TSharedPtr<FDreamShaderGraphDebugTable>>& Pair : Tables)
		{
			if (Pair.Value.IsValid() && Pair.Value->GraphMaterial.Get() == GraphMaterial)
			{
				return Pair.Value;
			}
		}
		return nullptr;
	}

	void FDreamShaderGraphDebugRegistry::NotifyGraphMaterialAboutToReset(UMaterial* GraphMaterial)
	{
		if (GraphMaterial)
		{
			OnGraphMaterialAboutToReset.Broadcast(GraphMaterial);
		}
	}
}
