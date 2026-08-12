// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FDreamShaderGraphDecompiler graph-layout metadata: region assignment, regionized line building,
// layout-comment collection, and finalization. Out-of-line member definitions extracted from
// DreamShaderGraphDecompilerImpl.cpp; declarations stay in DreamShaderGraphDecompilerImpl.h, so all
// call sites are unchanged. Free-helper deps (Escape/FormatDreamShader*) are header-exposed already.

#include "DreamShaderGraphDecompilerImpl.h"

namespace UE::DreamShader::Editor::Private
{
	bool FDreamShaderGraphDecompiler::IsExpressionInsideComment(const UMaterialExpression* Expression, const FDecompiledGraphLayoutComment& Comment)
	{
		if (!Expression)
		{
			return false;
		}

		const int32 X = Expression->MaterialExpressionEditorX;
		const int32 Y = Expression->MaterialExpressionEditorY;
		return X >= Comment.X
			&& Y >= Comment.Y
			&& X <= Comment.X + Comment.W
			&& Y <= Comment.Y + Comment.H;
	}

	FString FDreamShaderGraphDecompiler::FindBestRegionForExpression(const UMaterialExpression* Expression) const
	{
		const FDecompiledGraphLayoutComment* BestComment = nullptr;
		int32 BestArea = MAX_int32;
		for (const FDecompiledGraphLayoutComment& Comment : LayoutComments)
		{
			if (!IsExpressionInsideComment(Expression, Comment))
			{
				continue;
			}

			const int32 Area = FMath::Max(1, Comment.W) * FMath::Max(1, Comment.H);
			if (!BestComment || Area < BestArea)
			{
				BestComment = &Comment;
				BestArea = Area;
			}
		}

		return BestComment ? BestComment->Name : FString();
	}

	void FDreamShaderGraphDecompiler::BuildLayoutLines()
	{
		LayoutLines.Reset();
		for (const FDecompiledGraphLayoutComment& Comment : LayoutComments)
		{
			LayoutLines.Add(FString::Printf( // I18N-EXEMPT: decompiled .dsm layout source text, not a user-facing message
				TEXT("\t\tComment(Name=\"%s\", X=%d, Y=%d, W=%d, H=%d, Color=float4(%s, %s, %s, %s));"),
				*EscapeDreamShaderString(Comment.Name),
				Comment.X,
				Comment.Y,
				Comment.W,
				Comment.H,
				*FormatDreamShaderFloat(Comment.Color.R),
				*FormatDreamShaderFloat(Comment.Color.G),
				*FormatDreamShaderFloat(Comment.Color.B),
				*FormatDreamShaderFloat(Comment.Color.A)));
		}

		TArray<const UMaterialExpression*> SortedExpressions;
		ExpressionNames.GenerateKeyArray(SortedExpressions);
		SortedExpressions.StableSort([this](const UMaterialExpression& Left, const UMaterialExpression& Right)
		{
			const FString LeftName = ExpressionNames.FindRef(&Left);
			const FString RightName = ExpressionNames.FindRef(&Right);
			if (Left.MaterialExpressionEditorX != Right.MaterialExpressionEditorX)
			{
				return Left.MaterialExpressionEditorX < Right.MaterialExpressionEditorX;
			}
			if (Left.MaterialExpressionEditorY != Right.MaterialExpressionEditorY)
			{
				return Left.MaterialExpressionEditorY < Right.MaterialExpressionEditorY;
			}
			return LeftName.Compare(RightName, ESearchCase::CaseSensitive) < 0;
		});

		for (const UMaterialExpression* Expression : SortedExpressions)
		{
			const FString Name = ExpressionNames.FindRef(Expression);
			if (!Expression || Name.TrimStartAndEnd().IsEmpty())
			{
				continue;
			}

			LayoutLines.Add(FString::Printf( // I18N-EXEMPT: decompiled .dsm layout source text, not a user-facing message
				TEXT("\t\tNode(Var=\"%s\", X=%d, Y=%d);"),
				*EscapeDreamShaderString(Name),
				Expression->MaterialExpressionEditorX,
				Expression->MaterialExpressionEditorY));
		}
	}

	TArray<FString> FDreamShaderGraphDecompiler::BuildRegionizedGraphLines(const TArray<FString>& RawGraphLines)
	{
		if (RawGraphLines.IsEmpty() || ExpressionRegionNames.IsEmpty())
		{
			return RawGraphLines;
		}

		TArray<FString> Lines;
		FString ActiveRegion;
		for (const FString& Line : RawGraphLines)
		{
			FString RegionName;
			FString DeclaredName;
			if (TryExtractGraphAssignmentName(Line, DeclaredName))
			{
				if (const FString* FoundRegion = ExpressionRegionNames.Find(DeclaredName))
				{
					RegionName = *FoundRegion;
				}
			}

			if (!RegionName.Equals(ActiveRegion, ESearchCase::CaseSensitive))
			{
				if (!ActiveRegion.IsEmpty())
				{
					Lines.Add(TEXT("\t\t#EndRegion"));
					Lines.Add(TEXT(""));
				}
				if (!RegionName.IsEmpty())
				{
					Lines.Add(FString::Printf(TEXT("\t\t#Region \"%s\""), *EscapeDreamShaderString(RegionName))); // I18N-EXEMPT: decompiled .dsm layout source text
				}
				ActiveRegion = RegionName;
			}

			Lines.Add(Line);
		}

		if (!ActiveRegion.IsEmpty())
		{
			Lines.Add(TEXT("\t\t#EndRegion"));
		}

		return Lines;
	}

	bool FDreamShaderGraphDecompiler::TryExtractGraphAssignmentName(const FString& Line, FString& OutName)
	{
		FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT("#")))
		{
			return false;
		}

		FString Left;
		FString Right;
		if (!Trimmed.Split(TEXT("="), &Left, &Right, ESearchCase::CaseSensitive))
		{
			return false;
		}

		Left.TrimStartAndEndInline();
		TArray<FString> Tokens;
		Left.ParseIntoArrayWS(Tokens);
		if (Tokens.Num() < 2)
		{
			return false;
		}

		OutName = Tokens.Last().TrimStartAndEnd();
		return !OutName.IsEmpty();
	}

	void FDreamShaderGraphDecompiler::CollectLayoutComments(UMaterial* Material)
	{
		LayoutComments.Reset();
		if (!Material)
		{
			return;
		}

		for (const TObjectPtr<UMaterialExpressionComment>& Comment : Material->GetEditorComments())
		{
			if (!Comment)
			{
				continue;
			}
			if (Comment->Text.StartsWith(TEXT("DreamShader: "), ESearchCase::CaseSensitive))
			{
				continue;
			}

			FDecompiledGraphLayoutComment& LayoutComment = LayoutComments.AddDefaulted_GetRef();
			LayoutComment.Name = Comment->Text;
			LayoutComment.X = Comment->MaterialExpressionEditorX;
			LayoutComment.Y = Comment->MaterialExpressionEditorY;
			LayoutComment.W = Comment->SizeX;
			LayoutComment.H = Comment->SizeY;
			LayoutComment.Color = Comment->CommentColor;
		}
	}

	void FDreamShaderGraphDecompiler::CollectLayoutComments(UMaterialFunction* MaterialFunction)
	{
		LayoutComments.Reset();
		if (!MaterialFunction)
		{
			return;
		}

		for (const TObjectPtr<UMaterialExpressionComment>& Comment : MaterialFunction->GetEditorComments())
		{
			if (!Comment)
			{
				continue;
			}
			if (Comment->Text.StartsWith(TEXT("DreamShader: "), ESearchCase::CaseSensitive))
			{
				continue;
			}

			FDecompiledGraphLayoutComment& LayoutComment = LayoutComments.AddDefaulted_GetRef();
			LayoutComment.Name = Comment->Text;
			LayoutComment.X = Comment->MaterialExpressionEditorX;
			LayoutComment.Y = Comment->MaterialExpressionEditorY;
			LayoutComment.W = Comment->SizeX;
			LayoutComment.H = Comment->SizeY;
			LayoutComment.Color = Comment->CommentColor;
		}
	}

	void FDreamShaderGraphDecompiler::FinalizeGraphLayoutMetadata()
	{
		ExpressionRegionNames.Reset();
		for (const TPair<const UMaterialExpression*, FString>& Pair : ExpressionNames)
		{
			const FString RegionName = FindBestRegionForExpression(Pair.Key);
			if (!RegionName.IsEmpty())
			{
				ExpressionRegionNames.Add(Pair.Value, RegionName);
			}
		}

		if (const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>(); !Settings || Settings->bExportDecompiledLayout)
		{
			BuildLayoutLines();
		}
	}
}
