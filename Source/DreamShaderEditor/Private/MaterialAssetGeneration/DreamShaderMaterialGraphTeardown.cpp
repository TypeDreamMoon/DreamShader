// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader-generated material/function graph teardown: removes previously generated expressions
// and comments before regeneration. Extracted from DreamShaderMaterialGeneratorSupport.cpp; the two
// teardown helpers it calls (EnsureExpressionCanBeDeleted, ClearDreamShaderGeneratedComments) are
// now header-declared (their definitions stay in Support.cpp).

#include "DreamShaderMaterialGeneratorCodeShared.h"

#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialFunction.h"
#include "Misc/ScopedSlowTask.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		constexpr int32 FastClearExpressionThreshold = 1200;
	}

	void ClearMaterialExpressions(UMaterial* Material)
	{
		if (!Material)
		{
			return;
		}

		ClearDreamShaderGeneratedComments(Material, nullptr);

		FScopedSlowTask ClearSlowTask(
			FMath::Max(1.0f, static_cast<float>(Material->GetExpressions().Num())),
			FText::FromString(FString::Printf(TEXT("Clearing Material graph '%s'..."), *Material->GetName()))); /* I18N-EXEMPT: deferred codegen or compatibility path */

		for (int32 MaterialPropertyIndex = 0; MaterialPropertyIndex < MP_MAX; ++MaterialPropertyIndex)
		{
			if (FExpressionInput* ExpressionInput = Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(MaterialPropertyIndex)))
			{
				ExpressionInput->Expression = nullptr;
			}
		}

		if (Material->GetExpressions().Num() >= FastClearExpressionThreshold)
		{
			for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
			{
				EnsureExpressionCanBeDeleted(Expression.Get());
				if (Expression)
				{
					Expression->MarkAsGarbage();
				}
			}

			Material->EditorParameters.Reset();
			Material->GetExpressionCollection().Empty();
			return;
		}

		int32 SafetyCounter = 0;
		while (!Material->GetExpressions().IsEmpty() && SafetyCounter < 64)
		{
			TArray<UMaterialExpression*> ExpressionSnapshot;
			ExpressionSnapshot.Reserve(Material->GetExpressions().Num());
			for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
			{
				if (Expression)
				{
					ExpressionSnapshot.Add(Expression.Get());
				}
			}

			if (ExpressionSnapshot.IsEmpty())
			{
				break;
			}

			for (UMaterialExpression* Expression : ExpressionSnapshot)
			{
				ClearSlowTask.EnterProgressFrame(1.0f, FText::FromString(FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("Deleting old Material node '%s'..."),
					Expression ? *Expression->GetName() : TEXT("<null>"))));
				EnsureExpressionCanBeDeleted(Expression);
				UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expression);
			}

			++SafetyCounter;
		}
	}

	void ClearMaterialFunctionExpressions(UMaterialFunction* MaterialFunction)
	{
		if (!MaterialFunction)
		{
			return;
		}

		ClearDreamShaderGeneratedComments(nullptr, MaterialFunction);

		FScopedSlowTask ClearSlowTask(
			FMath::Max(1.0f, static_cast<float>(MaterialFunction->GetExpressions().Num())),
			FText::FromString(FString::Printf(TEXT("Clearing Material Function graph '%s'..."), *MaterialFunction->GetName()))); /* I18N-EXEMPT: deferred codegen or compatibility path */

		if (MaterialFunction->GetExpressions().Num() >= FastClearExpressionThreshold)
		{
			for (const TObjectPtr<UMaterialExpression>& Expression : MaterialFunction->GetExpressions())
			{
				EnsureExpressionCanBeDeleted(Expression.Get());
				if (Expression)
				{
					Expression->MarkAsGarbage();
				}
			}

			MaterialFunction->GetExpressionCollection().Empty();
		}
		else
		{
			int32 SafetyCounter = 0;
			while (!MaterialFunction->GetExpressions().IsEmpty() && SafetyCounter < 64)
			{
				TArray<UMaterialExpression*> ExpressionSnapshot;
				ExpressionSnapshot.Reserve(MaterialFunction->GetExpressions().Num());
				for (const TObjectPtr<UMaterialExpression>& Expression : MaterialFunction->GetExpressions())
				{
					if (Expression)
					{
						ExpressionSnapshot.Add(Expression.Get());
					}
				}

				if (ExpressionSnapshot.IsEmpty())
				{
					break;
				}

				for (UMaterialExpression* Expression : ExpressionSnapshot)
				{
					ClearSlowTask.EnterProgressFrame(1.0f, FText::FromString(FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
						TEXT("Deleting old Material Function node '%s'..."),
						Expression ? *Expression->GetName() : TEXT("<null>"))));
					EnsureExpressionCanBeDeleted(Expression);
					UMaterialEditingLibrary::DeleteMaterialExpressionInFunction(MaterialFunction, Expression);
				}

				++SafetyCounter;
			}
		}

		// A UMaterialFunction keeps a SECOND, serialized list of its function-call nodes that the
		// expression collection knows nothing about, and `UMaterialFunction::IterateDependentFunctions`
		// walks it with no null check (MaterialExpressions.cpp:14406). Emptying the graph without
		// resetting it leaves raw pointers to the nodes marked garbage above; GC nulls the entries, and
		// the next `ForceRecompileForRendering` -- which iterates every loaded UMaterialInterface, so
		// any later compile of any file reaches it -- dereferences null.
		//
		// The engine refreshes this list from exactly two places, PostLoad and
		// ForceRecompileForRendering, so the window stays open for as long as the function is cleared
		// but not rebuilt: a generation that FAILS after this point (a bad .dsf leaves the function
		// empty) arms the crash for the rest of the session. Resetting here makes a half-cleared
		// function merely empty instead of lethal.
		MaterialFunction->DependentFunctionExpressionCandidates.Reset();
	}

}
