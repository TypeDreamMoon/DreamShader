#include "DreamShaderMaterialGeneratorCodeShared.h"

#include "Misc/ScopeExit.h"
#include "Misc/ScopedSlowTask.h"

namespace UE::DreamShader::Editor::Private
{
	FCodeGraphBuilder::FCodeGraphBuilder(
		UMaterial* InMaterial,
		UMaterialFunction* InMaterialFunction,
		const FTextShaderDefinition& InDefinition,
		const FString& InSourceFilePath,
		const FString& InIncludeVirtualPath,
		const TArray<FTextShaderPropertyDefinition>* InLocalProperties,
		const FString& InCodeSourceFilePath,
		const int32 InCodeStartLine,
		const int32 InCodeStartColumn)
		: Material(InMaterial)
		, MaterialFunction(InMaterialFunction)
		, Definition(InDefinition)
		, LocalProperties(InLocalProperties)
		, SourceFilePath(InSourceFilePath)
		, IncludeVirtualPath(InIncludeVirtualPath)
		, CodeSourceFilePath(InCodeSourceFilePath.IsEmpty() ? InSourceFilePath : InCodeSourceFilePath)
		, CodeStartLine(FMath::Max(1, InCodeStartLine))
		, CodeStartColumn(FMath::Max(1, InCodeStartColumn))
	{
	}

	bool FCodeGraphBuilder::Build(
		const TArray<FCodeStatement>& Statements,
		TMap<FString, FCodeValue>& InOutValues,
		FString& OutError)
	{
		Values = &InOutValues;

		FScopedSlowTask BuildSlowTask(
			FMath::Max(1, Statements.Num()),
			FText::FromString(FString::Printf(TEXT("Building DreamShader graph nodes (%d statement%s)..."),
				Statements.Num(),
				Statements.Num() == 1 ? TEXT("") : TEXT("s"))));
		ActiveBuildSlowTask = &BuildSlowTask;
		ON_SCOPE_EXIT
		{
			ActiveBuildSlowTask = nullptr;
		};

		int32 StatementIndex = 0;
		for (const FCodeStatement& Statement : Statements)
		{
			const bool bVerboseProgress = Statements.Num() <= 512 || (StatementIndex % 64) == 0;
			BuildSlowTask.EnterProgressFrame(
				1.0f,
				bVerboseProgress
					? FText::FromString(
						Statement.TargetName.IsEmpty()
							? FString::Printf(TEXT("Evaluating DreamShader graph statement %d of %d..."), StatementIndex + 1, Statements.Num())
							: FString::Printf(TEXT("Evaluating DreamShader graph statement %d of %d: '%s'..."), StatementIndex + 1, Statements.Num(), *Statement.TargetName))
					: FText::GetEmpty());
			if (!ExecuteStatement(Statement, OutError))
			{
				OutError = FormatStatementError(Statement, OutError);
				return false;
			}
			++StatementIndex;
		}

		return true;
	}

	static bool LooksLikeLocatedDiagnostic(const FString& Error)
	{
		int32 CloseMarkerIndex = INDEX_NONE;
		if (!Error.FindChar(TCHAR(')'), CloseMarkerIndex))
		{
			return false;
		}

		const int32 OpenMarkerIndex = Error.Find(TEXT("("), ESearchCase::CaseSensitive, ESearchDir::FromEnd, CloseMarkerIndex);
		return OpenMarkerIndex != INDEX_NONE
			&& Error.Left(OpenMarkerIndex).Find(TEXT(": ")) == INDEX_NONE
			&& Error.Find(TEXT(": "), ESearchCase::CaseSensitive, ESearchDir::FromStart, CloseMarkerIndex) != INDEX_NONE;
	}

	static FString AddStatementErrorContext(const FString& Error, const TCHAR* Context)
	{
		const FString ContextPrefix = FString(Context) + TEXT(": ");
		const int32 CloseMarkerIndex = Error.Find(TEXT("): "));
		if (CloseMarkerIndex != INDEX_NONE)
		{
			return Error.Left(CloseMarkerIndex + 3) + ContextPrefix + Error.Mid(CloseMarkerIndex + 3);
		}

		return ContextPrefix + Error;
	}

	FString FCodeGraphBuilder::FormatStatementError(const FCodeStatement& Statement, const FString& Error) const
	{
		if (!Statement.bHasSourceLocation || LooksLikeLocatedDiagnostic(Error))
		{
			return Error;
		}

		const int32 Line = CodeStartLine + FMath::Max(1, Statement.SourceLine) - 1;
		const int32 Column = Statement.SourceLine <= 1
			? CodeStartColumn + FMath::Max(1, Statement.SourceColumn) - 1
			: FMath::Max(1, Statement.SourceColumn);
		return FString::Printf(TEXT("%s(%d,%d): %s"), *CodeSourceFilePath, Line, Column, *Error);
	}

	void FCodeGraphBuilder::RegisterGeneratedVariable(const FCodeStatement& Statement, const FCodeValue& Value)
	{
		if (Statement.TargetName.IsEmpty() || !Value.Expression)
		{
			return;
		}

		GeneratedExpressionsByVariable.Add(Statement.TargetName, Value.Expression);
		if (!Statement.RegionName.IsEmpty())
		{
			RegionByVariable.Add(Statement.TargetName, Statement.RegionName);
		}
	}

	bool FCodeGraphBuilder::ExecuteStatement(const FCodeStatement& Statement, FString& OutError)
	{
		if (Statement.bIsIfStatement)
		{
			return ExecuteIfStatement(Statement, OutError);
		}

		if (!Statement.Expression && !Statement.bIsDeclaration && !Statement.bUsesBraceInitializer)
		{
			OutError = TEXT("Encountered an invalid empty Graph statement.");
			return false;
		}

		if (Statement.bIsExpressionStatement)
		{
			return ExecuteExpressionStatement(Statement.Expression, OutError);
		}

		if (Statement.TargetName.IsEmpty())
		{
			OutError = TEXT("Encountered a Graph assignment without a target variable.");
			return false;
		}

		if (!Statement.bIsDeclaration)
		{
			FString MemberBaseName;
			FString MemberName;
			if (TrySplitMemberTarget(Statement.TargetName, MemberBaseName, MemberName))
			{
				FCodeValue EvaluatedMemberValue;
				if (Statement.bUsesBraceInitializer)
				{
					FString TargetTypeName;
					if (!ResolveTargetTypeForAssignment(Statement, TargetTypeName, OutError)
						|| !EvaluateBraceInitializer(TargetTypeName, Statement.InitializerText, EvaluatedMemberValue, OutError))
					{
						OutError = FString::Printf(TEXT("Failed to evaluate Graph assignment for '%s'. %s"), *Statement.TargetName, *OutError);
						return false;
					}
				}
				else if (Statement.Expression)
				{
					if (!EvaluateExpression(Statement.Expression, EvaluatedMemberValue, OutError))
					{
						OutError = FString::Printf(TEXT("Failed to evaluate Graph assignment for '%s'. %s"), *Statement.TargetName, *OutError);
						return false;
					}
				}
				else
				{
					OutError = FString::Printf(TEXT("MaterialAttributes member assignment '%s' requires a value."), *Statement.TargetName);
					return false;
				}

				if (!AssignMaterialAttributesMember(Statement.TargetName, EvaluatedMemberValue, OutError))
				{
					OutError = FString::Printf(TEXT("Failed to assign Graph member '%s'. %s"), *Statement.TargetName, *OutError);
					return false;
				}

				return true;
			}
		}

		if (Statement.bIsDeclaration && FindValue(Statement.TargetName))
		{
			OutError = FString::Printf(TEXT("Graph variable '%s' is declared more than once."), *Statement.TargetName);
			return false;
		}

		FCodeValue EvaluatedValue;
		if (Statement.bUsesBraceInitializer)
		{
			FString TargetTypeName;
			if (!ResolveTargetTypeForAssignment(Statement, TargetTypeName, OutError)
				|| !EvaluateBraceInitializer(TargetTypeName, Statement.InitializerText, EvaluatedValue, OutError))
			{
				OutError = FString::Printf(TEXT("Failed to evaluate Graph assignment for '%s'. %s"), *Statement.TargetName, *OutError);
				return false;
			}
		}
		else if (Statement.Expression)
		{
			if (!EvaluateExpression(Statement.Expression, EvaluatedValue, OutError))
			{
				OutError = FString::Printf(TEXT("Failed to evaluate Graph assignment for '%s'. %s"), *Statement.TargetName, *OutError);
				return false;
			}
		}
		else if (Statement.bIsDeclaration)
		{
			if (!CreateDefaultValue(Statement.DeclaredType, EvaluatedValue, OutError))
			{
				OutError = FString::Printf(TEXT("Failed to declare Graph variable '%s'. %s"), *Statement.TargetName, *OutError);
				return false;
			}
		}

		if (Statement.bIsDeclaration)
		{
			int32 ExpectedComponentCount = 1;
			bool bExpectedTexture = false;
			bool bExpectedSubstrate = false;
			ETextShaderTextureType ExpectedTextureType = ETextShaderTextureType::Texture2D;
			if (!TryResolveCodeDeclaredType(Statement.DeclaredType, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate))
			{
				if (IsSubstrateMaterialType(Statement.DeclaredType) && !IsSubstrateMaterialTypeSupported())
				{
					OutError = FString::Printf(TEXT("Graph variable '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *Statement.TargetName);
					return false;
				}
				OutError = FString::Printf(TEXT("Unsupported Graph variable type '%s' for '%s'."), *Statement.DeclaredType, *Statement.TargetName);
				return false;
			}

			if (EvaluatedValue.bHasAuthoritativeComponentCount
				&& !EvaluatedValue.bIsTextureObject
				&& !EvaluatedValue.bIsMaterialAttributes
				&& !EvaluatedValue.bIsSubstrateMaterial
				&& !bExpectedTexture
				&& !bExpectedSubstrate
				&& ExpectedComponentCount > 0
				&& EvaluatedValue.ComponentCount != ExpectedComponentCount)
			{
				(*Values).Add(Statement.TargetName, EvaluatedValue);
				RegisterGeneratedVariable(Statement, EvaluatedValue);
				return true;
			}

			FCodeValue CoercedValue;
			if (!CoerceValueToType(EvaluatedValue, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate, CoercedValue, OutError))
			{
				OutError = FString::Printf(
					TEXT("Graph variable '%s' is declared as '%s' but assigned an incompatible value. %s"),
					*Statement.TargetName,
					*Statement.DeclaredType,
					*OutError);
				return false;
			}

			EvaluatedValue = CoercedValue;
		}
		else if (const FCodeValue* ExistingValue = FindValue(Statement.TargetName))
		{
			FCodeValue CoercedValue;
			if (!CoerceValueToType(EvaluatedValue, ExistingValue->ComponentCount, ExistingValue->bIsTextureObject, ExistingValue->TextureType, ExistingValue->bIsSubstrateMaterial, CoercedValue, OutError))
			{
				OutError = FString::Printf(
					TEXT("Graph variable '%s' was previously assigned an incompatible value. %s"),
					*Statement.TargetName,
					*OutError);
				return false;
			}

			EvaluatedValue = CoercedValue;
		}
		else
		{
			int32 OutputComponentCount = 1;
			bool bOutputIsTexture = false;
			if (TryResolveOutputVariableComponentCount(Definition, Statement.TargetName, OutputComponentCount, bOutputIsTexture))
			{
				FCodeValue CoercedValue;
				ETextShaderTextureType OutputTextureType = ETextShaderTextureType::Texture2D;
				bool bOutputIsSubstrate = false;
				(void)TryResolveOutputVariableComponentCount(Definition, Statement.TargetName, OutputComponentCount, bOutputIsTexture, OutputTextureType, bOutputIsSubstrate);
				if (!CoerceValueToType(EvaluatedValue, OutputComponentCount, bOutputIsTexture, OutputTextureType, bOutputIsSubstrate, CoercedValue, OutError))
				{
					OutError = FString::Printf(
						TEXT("Graph output variable '%s' was assigned an incompatible value. %s"),
						*Statement.TargetName,
						*OutError);
					return false;
				}

				EvaluatedValue = CoercedValue;
			}
		}

		(*Values).Add(Statement.TargetName, EvaluatedValue);
		RegisterGeneratedVariable(Statement, EvaluatedValue);
		return true;
	}

	static bool AreCodeValuesEquivalent(const FCodeValue& Left, const FCodeValue& Right)
	{
		return Left.Expression == Right.Expression
			&& Left.OutputIndex == Right.OutputIndex
			&& Left.ComponentCount == Right.ComponentCount
			&& Left.bHasInputMask == Right.bHasInputMask
			&& Left.InputMaskR == Right.InputMaskR
			&& Left.InputMaskG == Right.InputMaskG
			&& Left.InputMaskB == Right.InputMaskB
			&& Left.InputMaskA == Right.InputMaskA
			&& Left.bIsTextureObject == Right.bIsTextureObject
			&& Left.TextureType == Right.TextureType
			&& Left.bIsMaterialAttributes == Right.bIsMaterialAttributes
			&& Left.bIsSubstrateMaterial == Right.bIsSubstrateMaterial;
	}

	static bool IsScalarVectorCompatible(const FCodeValue& LeftValue, const FCodeValue& RightValue)
	{
		return LeftValue.ComponentCount == RightValue.ComponentCount
			|| LeftValue.ComponentCount == 1
			|| RightValue.ComponentCount == 1;
	}

	FString MakeCodeValueReuseToken(const FCodeValue& Value)
	{
		return FString::Printf(
			TEXT("Expr=%s|Out=%d|Comp=%d|Mask=%d%d%d%d%d|Tex=%d|TexType=%d|MA=%d|Sub=%d|Auth=%d|Int=%d"),
			Value.Expression ? *Value.Expression->GetPathName() : TEXT("<null>"),
			Value.OutputIndex,
			Value.ComponentCount,
			Value.bHasInputMask ? 1 : 0,
			Value.InputMaskR ? 1 : 0,
			Value.InputMaskG ? 1 : 0,
			Value.InputMaskB ? 1 : 0,
			Value.InputMaskA ? 1 : 0,
			Value.bIsTextureObject ? 1 : 0,
			static_cast<int32>(Value.TextureType),
			Value.bIsMaterialAttributes ? 1 : 0,
			Value.bIsSubstrateMaterial ? 1 : 0,
			Value.bHasAuthoritativeComponentCount ? 1 : 0,
			Value.bIsIntegerType ? 1 : 0);
	}

	static void CollectChangedValueNames(
		const TMap<FString, FCodeValue>& BaseValues,
		const TMap<FString, FCodeValue>& BranchValues,
		TSet<FString>& OutNames)
	{
		for (const TPair<FString, FCodeValue>& Pair : BranchValues)
		{
			const FCodeValue* BaseValue = BaseValues.Find(Pair.Key);
			if (!BaseValue || !AreCodeValuesEquivalent(*BaseValue, Pair.Value))
			{
				OutNames.Add(Pair.Key);
			}
		}
	}

	bool FCodeGraphBuilder::ExecuteIfStatement(const FCodeStatement& Statement, FString& OutError)
	{
		if (!Values)
		{
			OutError = TEXT("Graph builder is not initialized.");
			return false;
		}

		TMap<FString, FCodeValue>* OuterValues = Values;
		const TMap<FString, FCodeValue> BaseValues = *OuterValues;

		TMap<FString, FCodeValue> ThenValues = BaseValues;
		Values = &ThenValues;
		for (const FCodeStatement& ThenStatement : Statement.ThenStatements)
		{
			if (!ExecuteStatement(ThenStatement, OutError))
			{
				Values = OuterValues;
				OutError = AddStatementErrorContext(FormatStatementError(ThenStatement, OutError), TEXT("In Graph if body"));
				return false;
			}
		}

		TMap<FString, FCodeValue> ElseValues = BaseValues;
		Values = &ElseValues;
		for (const FCodeStatement& ElseStatement : Statement.ElseStatements)
		{
			if (!ExecuteStatement(ElseStatement, OutError))
			{
				Values = OuterValues;
				OutError = AddStatementErrorContext(FormatStatementError(ElseStatement, OutError), TEXT("In Graph else body"));
				return false;
			}
		}

		Values = OuterValues;

		TSet<FString> ChangedNames;
		CollectChangedValueNames(BaseValues, ThenValues, ChangedNames);
		CollectChangedValueNames(BaseValues, ElseValues, ChangedNames);

		for (const FString& Name : ChangedNames)
		{
			// A property/parameter that is only READ inside a branch is lazily materialized into that
			// branch's value map by TryCreatePropertyValue (it calls Values->Add). That is a read
			// side effect, not a conditional assignment: parameters can never be assignment targets,
			// so such a name shows up as "changed" in whichever branch happened to reference it and
			// absent from the other. Reconciling it as a branch output is wrong — it makes any
			// parameter read inside a single branch fail generation ("could not resolve both branch
			// values"). Declared properties are therefore never branch outputs; skip them.
			if (FindPropertyDefinition(Name))
			{
				continue;
			}

			const FCodeValue* ThenValue = ThenValues.Find(Name);
			const FCodeValue* ElseValue = ElseValues.Find(Name);
			if (!ThenValue || !ElseValue)
			{
				OutError = FString::Printf(TEXT("Graph if statement could not resolve both branch values for '%s'."), *Name);
				return false;
			}

			int32 ExpectedComponentCount = ThenValue->ComponentCount;
			bool bExpectedTexture = ThenValue->bIsTextureObject;
			bool bExpectedSubstrate = ThenValue->bIsSubstrateMaterial;
			ETextShaderTextureType ExpectedTextureType = ThenValue->TextureType;
			if (const FCodeValue* BaseValue = BaseValues.Find(Name))
			{
				ExpectedComponentCount = BaseValue->ComponentCount;
				bExpectedTexture = BaseValue->bIsTextureObject;
				bExpectedSubstrate = BaseValue->bIsSubstrateMaterial;
				ExpectedTextureType = BaseValue->TextureType;
			}
			else
			{
				int32 OutputComponentCount = 0;
				bool bOutputIsTexture = false;
				ETextShaderTextureType OutputTextureType = ETextShaderTextureType::Texture2D;
				bool bOutputIsSubstrate = false;
				if (TryResolveOutputVariableComponentCount(Definition, Name, OutputComponentCount, bOutputIsTexture, OutputTextureType, bOutputIsSubstrate))
				{
					ExpectedComponentCount = OutputComponentCount;
					bExpectedTexture = bOutputIsTexture;
					bExpectedSubstrate = bOutputIsSubstrate;
					ExpectedTextureType = OutputTextureType;
				}
				else if (ThenValue->ComponentCount != ElseValue->ComponentCount
					|| ThenValue->bIsTextureObject != ElseValue->bIsTextureObject
					|| ThenValue->bIsSubstrateMaterial != ElseValue->bIsSubstrateMaterial
					|| ThenValue->bIsMaterialAttributes != ElseValue->bIsMaterialAttributes
					|| ThenValue->TextureType != ElseValue->TextureType)
				{
					OutError = FString::Printf(TEXT("Graph if branches assign variable '%s' with inconsistent types"), *Name);
					return false;
				}
			}

			if (bExpectedTexture)
			{
				OutError = FString::Printf(TEXT("Graph if statement cannot select texture value '%s'."), *Name);
				return false;
			}
			if (bExpectedSubstrate)
			{
				OutError = FString::Printf(TEXT("Graph if statement cannot select Substrate value '%s'."), *Name);
				return false;
			}

			FCodeValue CoercedThenValue;
			FCodeValue CoercedElseValue;
			if (!CoerceValueToType(*ThenValue, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate, CoercedThenValue, OutError)
				|| !CoerceValueToType(*ElseValue, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate, CoercedElseValue, OutError))
			{
				OutError = FString::Printf(TEXT("Graph if branches assign incompatible values to '%s'. %s"), *Name, *OutError);
				return false;
			}

			FCodeValue ConditionalValue;
			if (!CreateConditionalValue(Statement.Condition, CoercedThenValue, CoercedElseValue, ConditionalValue, OutError))
			{
				OutError = FString::Printf(TEXT("Graph if statement failed to merge '%s'. %s"), *Name, *OutError);
				return false;
			}

			(*Values).Add(Name, ConditionalValue);
		}

		return true;
	}

	bool FCodeGraphBuilder::CreateConditionalValue(
		const FCodeCondition& Condition,
		const FCodeValue& TrueValue,
		const FCodeValue& FalseValue,
		FCodeValue& OutValue,
		FString& OutError)
	{
		if (TrueValue.bIsTextureObject || FalseValue.bIsTextureObject)
		{
			OutError = TEXT("Texture values cannot be selected by Graph if statements.");
			return false;
		}
		if (TrueValue.bIsSubstrateMaterial || FalseValue.bIsSubstrateMaterial)
		{
			OutError = TEXT("Substrate values cannot be selected by Graph if statements.");
			return false;
		}
		if (TrueValue.bIsMaterialAttributes != FalseValue.bIsMaterialAttributes)
		{
			OutError = TEXT("Graph if branches cannot mix MaterialAttributes and numeric values.");
			return false;
		}

		FCodeValue LeftValue;
		if (!EvaluateExpression(Condition.Left, LeftValue, OutError))
		{
			OutError = FString::Printf(TEXT("Failed to evaluate Graph if condition. %s"), *OutError);
			return false;
		}

		if (LeftValue.bIsTextureObject || LeftValue.bIsMaterialAttributes || LeftValue.bIsSubstrateMaterial || LeftValue.ComponentCount != 1)
		{
			OutError = TEXT("Graph if condition left side must evaluate to a scalar value.");
			return false;
		}

		FCodeValue RightValue;
		if (Condition.Operator == TEXT("truthy"))
		{
			RightValue.Expression = CreateScalarLiteralNode(0.0, ConsumeNodeY());
			if (!RightValue.Expression)
			{
				OutError = TEXT("Failed to create a zero literal for Graph if condition.");
				return false;
			}
			RightValue.ComponentCount = 1;
		}
		else
		{
			if (!EvaluateExpression(Condition.Right, RightValue, OutError))
			{
				OutError = FString::Printf(TEXT("Failed to evaluate Graph if condition. %s"), *OutError);
				return false;
			}
		}

		if (RightValue.bIsTextureObject || RightValue.bIsMaterialAttributes || RightValue.bIsSubstrateMaterial || RightValue.ComponentCount != 1)
		{
			OutError = TEXT("Graph if condition right side must evaluate to a scalar value.");
			return false;
		}

		auto* IfExpression = Cast<UMaterialExpressionIf>(
			CreateExpression(UMaterialExpressionIf::StaticClass(), 520, ConsumeNodeY()));
		if (!IfExpression)
		{
			OutError = TEXT("Failed to create a Material If node.");
			return false;
		}

		ConnectCodeValueToInput(IfExpression->A, LeftValue);
		ConnectCodeValueToInput(IfExpression->B, RightValue);

		const auto ConnectBranches = [&](const FCodeValue& GreaterValue, const FCodeValue& EqualValue, const FCodeValue& LessValue)
		{
			ConnectCodeValueToInput(IfExpression->AGreaterThanB, GreaterValue);
			ConnectCodeValueToInput(IfExpression->AEqualsB, EqualValue);
			ConnectCodeValueToInput(IfExpression->ALessThanB, LessValue);
		};

		if (Condition.Operator == TEXT(">"))
		{
			ConnectBranches(TrueValue, FalseValue, FalseValue);
		}
		else if (Condition.Operator == TEXT("<"))
		{
			ConnectBranches(FalseValue, FalseValue, TrueValue);
		}
		else if (Condition.Operator == TEXT(">="))
		{
			ConnectBranches(TrueValue, TrueValue, FalseValue);
		}
		else if (Condition.Operator == TEXT("<="))
		{
			ConnectBranches(FalseValue, TrueValue, TrueValue);
		}
		else if (Condition.Operator == TEXT("=="))
		{
			ConnectBranches(FalseValue, TrueValue, FalseValue);
		}
		else if (Condition.Operator == TEXT("!=") || Condition.Operator == TEXT("truthy"))
		{
			// `if (x)` is truthy == (x != 0): pick the then-branch for any non-zero x (x > 0 and
			// x < 0), matching HLSL/C semantics and the decompiler's `!= 0` convention.
			ConnectBranches(TrueValue, FalseValue, TrueValue);
		}
		else
		{
			OutError = FString::Printf(TEXT("Unsupported Graph if comparison operator '%s'."), *Condition.Operator);
			return false;
		}

		OutValue.Expression = IfExpression;
		OutValue.OutputIndex = 0;
		OutValue.ComponentCount = TrueValue.ComponentCount;
		OutValue.bIsTextureObject = false;
		OutValue.bIsMaterialAttributes = TrueValue.bIsMaterialAttributes;
		OutValue.bIsSubstrateMaterial = false;
		return true;
	}

	bool FCodeGraphBuilder::EvaluateOutputExpression(const FString& ExpressionText, FCodeValue& OutValue, FString& OutError)
	{
		TSharedPtr<FCodeExpression> ParsedExpression;
		if (!ParseCodeExpression(ExpressionText, ParsedExpression, OutError))
		{
			OutError = FString::Printf(TEXT("In output expression '%s': %s"), *ExpressionText, *OutError);
			return false;
		}

		if (!EvaluateExpression(ParsedExpression, OutValue, OutError))
		{
			OutError = FString::Printf(TEXT("In output expression '%s': %s"), *ExpressionText, *OutError);
			return false;
		}

		return true;
	}

	FCodeValue* FCodeGraphBuilder::FindValue(const FString& Name) const
	{
		if (!Values)
		{
			return nullptr;
		}

		if (FCodeValue* ExactMatch = Values->Find(Name))
		{
			return ExactMatch;
		}

		for (TPair<FString, FCodeValue>& Pair : *Values)
		{
			if (Pair.Key.Equals(Name, ESearchCase::IgnoreCase))
			{
				return &Pair.Value;
			}
		}

		return nullptr;
	}

	int32 FCodeGraphBuilder::ConsumeNodeY()
	{
		const int32 Result = NextNodeY;
		NextNodeY += 180;
		return Result;
	}

	UMaterialExpression* FCodeGraphBuilder::CreateExpression(
		const TSubclassOf<UMaterialExpression> ExpressionClass,
		const int32 PositionX,
		const int32 PositionY) const
	{
		if (ActiveBuildSlowTask && (++ProgressTickCounter % 8) == 0)
		{
			ActiveBuildSlowTask->TickProgress();
		}

		return UMaterialEditingLibrary::CreateMaterialExpressionEx(
			Material,
			MaterialFunction,
			ExpressionClass,
			nullptr,
			PositionX,
			PositionY,
			false);
	}

	UMaterialExpression* FCodeGraphBuilder::CreateScalarLiteralNode(const double Value, const int32 PositionY)
	{
		const FString ReuseKey = FString::Printf(TEXT("literal-node|%.17g"), Value);
		FCodeValue ReusableValue;
		if (TryFindReusableExpressionValue(ReuseKey, ReusableValue))
		{
			return ReusableValue.Expression;
		}

		auto* Expression = Cast<UMaterialExpressionConstant>(
			CreateExpression(UMaterialExpressionConstant::StaticClass(), -1120, PositionY));
		if (Expression)
		{
			Expression->R = static_cast<float>(Value);
			FCodeValue LiteralValue;
			LiteralValue.Expression = Expression;
			LiteralValue.OutputIndex = 0;
			LiteralValue.ComponentCount = 1;
			AddReusableExpressionValue(ReuseKey, LiteralValue);
		}
		return Expression;
	}

	UMaterialExpression* FCodeGraphBuilder::CreateStaticBoolLiteralNode(const bool bValue, const int32 PositionY)
	{
		const FString ReuseKey = FString::Printf(TEXT("static-bool-node|%d"), bValue ? 1 : 0);
		FCodeValue ReusableValue;
		if (TryFindReusableExpressionValue(ReuseKey, ReusableValue))
		{
			return ReusableValue.Expression;
		}

		auto* Expression = Cast<UMaterialExpressionStaticBool>(
			CreateExpression(UMaterialExpressionStaticBool::StaticClass(), -1120, PositionY));
		if (Expression)
		{
			Expression->Value = bValue ? 1U : 0U;
			FCodeValue LiteralValue;
			LiteralValue.Expression = Expression;
			LiteralValue.OutputIndex = 0;
			LiteralValue.ComponentCount = 1;
			AddReusableExpressionValue(ReuseKey, LiteralValue);
		}
		return Expression;
	}

	bool FCodeGraphBuilder::CreateMaterialAttributesValue(FCodeValue& OutValue, FString& OutError)
	{
		auto* Expression = Cast<UMaterialExpressionMakeMaterialAttributes>(
			CreateExpression(UMaterialExpressionMakeMaterialAttributes::StaticClass(), -160, ConsumeNodeY()));
		if (!Expression)
		{
			OutError = TEXT("Failed to create a MakeMaterialAttributes node.");
			return false;
		}

		OutValue = FCodeValue{};
		OutValue.Expression = Expression;
		OutValue.OutputIndex = 0;
		OutValue.ComponentCount = 0;
		OutValue.bIsTextureObject = false;
		OutValue.bIsMaterialAttributes = true;
		return true;
	}

	bool FCodeGraphBuilder::CreateDefaultValue(const FString& DeclaredType, FCodeValue& OutValue, FString& OutError)
	{
		int32 ComponentCount = 1;
		bool bIsTexture = false;
		bool bIsSubstrate = false;
		if (!TryResolveCodeDeclaredType(DeclaredType, ComponentCount, bIsTexture, bIsSubstrate))
		{
			if (IsSubstrateMaterialType(DeclaredType) && !IsSubstrateMaterialTypeSupported())
			{
				OutError = MakeSubstrateRequiresUE54Error();
				return false;
			}
			OutError = FString::Printf(TEXT("Unsupported Graph variable type '%s'."), *DeclaredType);
			return false;
		}

		if (IsMaterialAttributesComponentType(ComponentCount, bIsTexture, bIsSubstrate))
		{
			return CreateMaterialAttributesValue(OutValue, OutError);
		}

		if (bIsSubstrate)
		{
			OutError = FString::Printf(TEXT("Graph variable type '%s' requires an explicit initializer."), *DeclaredType);
			return false;
		}

		if (bIsTexture)
		{
			OutError = FString::Printf(TEXT("Graph variable type '%s' requires an explicit initializer."), *DeclaredType);
			return false;
		}

		FCodeValue ZeroScalar;
		ZeroScalar.Expression = CreateScalarLiteralNode(0.0, ConsumeNodeY());
		if (!ZeroScalar.Expression)
		{
			OutError = TEXT("Failed to create a default literal node.");
			return false;
		}
		ZeroScalar.ComponentCount = 1;

		if (ComponentCount == 1)
		{
			OutValue = ZeroScalar;
			return true;
		}

		TArray<FCodeValue> Parts;
		for (int32 Index = 0; Index < ComponentCount; ++Index)
		{
			Parts.Add(ZeroScalar);
		}

		if (!AppendValues(Parts, OutValue, OutError))
		{
			return false;
		}

		OutValue.ComponentCount = ComponentCount;
		return true;
	}


	bool FCodeGraphBuilder::EvaluateBraceInitializer(
		const FString& ConstructorType,
		const FString& InitializerText,
		FCodeValue& OutValue,
		FString& OutError)
	{
		const FString Trimmed = InitializerText.TrimStartAndEnd();
		if (!Trimmed.StartsWith(TEXT("{")) || !Trimmed.EndsWith(TEXT("}")))
		{
			OutError = FString::Printf(TEXT("Initializer '%s' is not a valid brace initializer."), *InitializerText);
			return false;
		}

		const FString InnerText = Trimmed.Mid(1, Trimmed.Len() - 2).TrimStartAndEnd();
		if (InnerText.IsEmpty())
		{
			return CreateDefaultValue(ConstructorType, OutValue, OutError);
		}

		const FString ConstructorExpression = FString::Printf(TEXT("%s(%s)"), *ConstructorType, *InnerText);
		TSharedPtr<FCodeExpression> ParsedExpression;
		if (!ParseCodeExpression(ConstructorExpression, ParsedExpression, OutError))
		{
			OutError = FString::Printf(TEXT("Invalid brace initializer for type '%s'. %s"), *ConstructorType, *OutError);
			return false;
		}

		return EvaluateExpression(ParsedExpression, OutValue, OutError);
	}

	bool FCodeGraphBuilder::ResolveTargetTypeForAssignment(
		const FCodeStatement& Statement,
		FString& OutTypeName,
		FString& OutError) const
	{
		if (Statement.bIsDeclaration)
		{
			OutTypeName = Statement.DeclaredType;
			return true;
		}

		FString BaseName;
		FString MemberName;
		if (TrySplitMemberTarget(Statement.TargetName, BaseName, MemberName))
		{
			int32 MemberComponentCount = 0;
			return ResolveMaterialAttributesMemberType(MemberName, MemberComponentCount, OutTypeName, OutError);
		}

		if (const FCodeValue* ExistingValue = FindValue(Statement.TargetName))
		{
			if (ExistingValue->bIsTextureObject)
			{
				OutError = FString::Printf(TEXT("Brace initializer assignment is not supported for texture variable '%s'."), *Statement.TargetName);
				return false;
			}
			if (ExistingValue->bIsSubstrateMaterial)
			{
				OutError = FString::Printf(TEXT("Brace initializer assignment is not supported for Substrate variable '%s'."), *Statement.TargetName);
				return false;
			}

			if (ResolveTypeNameForComponentCount(ExistingValue->ComponentCount, OutTypeName))
			{
				return true;
			}
		}

		int32 OutputComponentCount = 1;
		bool bOutputIsTexture = false;
		ETextShaderTextureType OutputTextureType = ETextShaderTextureType::Texture2D;
		bool bOutputIsSubstrate = false;
		if (TryResolveOutputVariableComponentCount(Definition, Statement.TargetName, OutputComponentCount, bOutputIsTexture, OutputTextureType, bOutputIsSubstrate))
		{
			if (bOutputIsTexture)
			{
				OutError = FString::Printf(TEXT("Brace initializer assignment is not supported for texture output '%s'."), *Statement.TargetName);
				return false;
			}
			if (bOutputIsSubstrate)
			{
				OutError = FString::Printf(TEXT("Brace initializer assignment is not supported for Substrate output '%s'."), *Statement.TargetName);
				return false;
			}

			if (ResolveTypeNameForComponentCount(OutputComponentCount, OutTypeName))
			{
				return true;
			}
		}

		OutError = FString::Printf(TEXT("Brace initializer assignment for '%s' requires a declared scalar or vector target type."), *Statement.TargetName);
		return false;
	}

	bool FCodeGraphBuilder::ResolveMaterialAttributesMemberType(
		const FString& MemberName,
		int32& OutComponentCount,
		FString& OutTypeName,
		FString& OutError) const
	{
		FResolvedMaterialProperty ResolvedProperty;
		if (!ResolveMaterialProperty(MemberName, ResolvedProperty)
			|| ResolvedProperty.Property == MP_MaterialAttributes)
		{
			OutError = FString::Printf(TEXT("Unsupported MaterialAttributes member '%s'."), *MemberName);
			return false;
		}

		if (!TryGetComponentCountForOutputType(ResolvedProperty.OutputType, OutComponentCount)
			|| OutComponentCount <= 0
			|| !ResolveTypeNameForComponentCount(OutComponentCount, OutTypeName))
		{
			OutError = FString::Printf(TEXT("MaterialAttributes member '%s' does not have a numeric scalar/vector type."), *MemberName);
			return false;
		}

		return true;
	}

	bool FCodeGraphBuilder::AssignMaterialAttributesMember(const FString& TargetName, const FCodeValue& InValue, FString& OutError)
	{
		FString BaseName;
		FString MemberName;
		if (!TrySplitMemberTarget(TargetName, BaseName, MemberName))
		{
			OutError = FString::Printf(TEXT("Invalid MaterialAttributes member assignment target '%s'."), *TargetName);
			return false;
		}

		FCodeValue* BaseValue = FindValue(BaseName);
		if (!BaseValue)
		{
			OutError = FString::Printf(TEXT("Unknown MaterialAttributes variable '%s'."), *BaseName);
			return false;
		}
		if (!BaseValue->bIsMaterialAttributes)
		{
			OutError = FString::Printf(TEXT("Graph variable '%s' is not a MaterialAttributes value."), *BaseName);
			return false;
		}

		FResolvedMaterialProperty ResolvedProperty;
		if (!ResolveMaterialProperty(MemberName, ResolvedProperty)
			|| ResolvedProperty.Property == MP_MaterialAttributes)
		{
			OutError = FString::Printf(TEXT("Unsupported MaterialAttributes member '%s'."), *MemberName);
			return false;
		}

		int32 ExpectedComponentCount = 0;
		if (!TryGetComponentCountForOutputType(ResolvedProperty.OutputType, ExpectedComponentCount) || ExpectedComponentCount <= 0)
		{
			OutError = FString::Printf(TEXT("MaterialAttributes member '%s' cannot be assigned from Graph code."), *MemberName);
			return false;
		}

		FCodeValue CoercedValue;
		if (!CoerceValueToType(InValue, ExpectedComponentCount, false, CoercedValue, OutError))
		{
			OutError = FString::Printf(
				TEXT("MaterialAttributes member '%s' expects %d component(s). %s"),
				*MemberName,
				ExpectedComponentCount,
				*OutError);
			return false;
		}

		UMaterialExpressionSetMaterialAttributes* SetAttributes = Cast<UMaterialExpressionSetMaterialAttributes>(
			CreateExpression(UMaterialExpressionSetMaterialAttributes::StaticClass(), 240, ConsumeNodeY()));
		if (!SetAttributes)
		{
			OutError = TEXT("Failed to create a SetMaterialAttributes node.");
			return false;
		}

		if (!ConnectDreamShaderSetMaterialAttributeInput(SetAttributes, MP_MaterialAttributes, BaseValue->Expression, BaseValue->OutputIndex))
		{
			OutError = FString::Printf(TEXT("Failed to connect '%s' as the SetMaterialAttributes base value."), *BaseName);
			return false;
		}

		if (!ConnectDreamShaderSetMaterialAttributeInput(SetAttributes, ResolvedProperty.Property, CoercedValue.Expression, CoercedValue.OutputIndex))
		{
			OutError = FString::Printf(TEXT("Failed to connect MaterialAttributes member '%s'."), *MemberName);
			return false;
		}

		BaseValue->Expression = SetAttributes;
		BaseValue->OutputIndex = 0;
		BaseValue->ComponentCount = 0;
		BaseValue->bIsTextureObject = false;
		BaseValue->bIsMaterialAttributes = true;

		return true;
	}

	const FCodeCallArgument* FCodeGraphBuilder::FindNamedArgument(const TArray<FCodeCallArgument>& Arguments, const TCHAR* Name) const
	{
		const FString Normalized = UE::DreamShader::NormalizeSettingKey(Name);
		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (Argument.bIsNamed && UE::DreamShader::NormalizeSettingKey(Argument.Name) == Normalized)
			{
				return &Argument;
			}
		}

		return nullptr;
	}

	const FCodeCallArgument* FCodeGraphBuilder::FindPositionalArgument(const TArray<FCodeCallArgument>& Arguments, const int32 PositionIndex) const
	{
		int32 CurrentIndex = 0;
		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (!Argument.bIsNamed)
			{
				if (CurrentIndex == PositionIndex)
				{
					return &Argument;
				}
				++CurrentIndex;
			}
		}

		return nullptr;
	}

	bool FCodeGraphBuilder::ExecuteExpressionStatement(const TSharedPtr<FCodeExpression>& Expression, FString& OutError)
	{
		if (!Expression || Expression->Kind != ECodeExpressionKind::Call)
		{
			OutError = TEXT("Graph expression statements currently support only Function calls with explicit out arguments.");
			return false;
		}

		FString CalleeName;
		if (!TryFlattenQualifiedName(Expression->Left, CalleeName))
		{
			OutError = TEXT("Graph expression statements must call a named Function.");
			return false;
		}

		const FTextShaderFunctionDefinition* Function = FindFunctionDefinition(CalleeName);
		const FTextShaderFunctionDefinition* GraphFunction = FindGraphFunctionDefinition(CalleeName);
		const FTextShaderMaterialFunctionDefinition* MaterialFunctionDefinition = FindMaterialFunctionDefinition(CalleeName);
		const FTextShaderVirtualFunctionDefinition* VirtualFunction = FindVirtualFunctionDefinition(CalleeName);
		int32 MatchCount = 0;
		MatchCount += Function ? 1 : 0;
		MatchCount += GraphFunction ? 1 : 0;
		MatchCount += MaterialFunctionDefinition ? 1 : 0;
		MatchCount += VirtualFunction ? 1 : 0;
		if (MatchCount == 0)
		{
			OutError = FString::Printf(
				TEXT("Graph expression statement '%s' is unsupported. Only DreamShader Function, GraphFunction, ShaderFunction, ShaderLayer, ShaderLayerBlend, or VirtualFunction calls may use statement syntax."),
				*CalleeName);
			return false;
		}
		if (MatchCount > 1)
		{
			OutError = FString::Printf(TEXT("Graph expression statement '%s' is ambiguous because multiple callable definitions exist."), *CalleeName);
			return false;
		}

		if (Function)
		{
			return ExecuteCustomFunctionCall(*Function, Expression->Arguments, OutError);
		}
		if (GraphFunction)
		{
			return ExecuteGraphFunctionCall(*GraphFunction, Expression->Arguments, OutError);
		}
		if (MaterialFunctionDefinition)
		{
			return ExecuteMaterialFunctionCall(*MaterialFunctionDefinition, Expression->Arguments, OutError);
		}

		return ExecuteVirtualFunctionCall(*VirtualFunction, Expression->Arguments, OutError);
	}

	bool FCodeGraphBuilder::EvaluateExpression(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError)
	{
		if (!Expression)
		{
			OutError = TEXT("Empty Graph expression.");
			return false;
		}

		switch (Expression->Kind)
		{
		case ECodeExpressionKind::Name:
		{
			if (FCodeValue* ExistingValue = FindValue(Expression->Text))
			{
				OutValue = *ExistingValue;
				return true;
			}

			if (TryCreatePropertyValue(Expression->Text, OutValue, OutError))
			{
				return OutError.IsEmpty();
			}

			// `true` / `false` are compile-time bool literals and materialize as StaticBool nodes, the
			// only way to feed a StaticSwitch / StaticBool function input. A StaticBool ShaderFunction
			// input reaches this path through its declared default: UE ignores PreviewValue for
			// static-bool inputs and compiles the node wired to the Preview pin instead.
			bool bBoolLiteralValue = false;
			if (ParseBooleanLiteral(Expression->Text, bBoolLiteralValue))
			{
				OutValue = FCodeValue{};
				OutValue.Expression = CreateStaticBoolLiteralNode(bBoolLiteralValue, ConsumeNodeY());
				OutValue.OutputIndex = 0;
				OutValue.ComponentCount = 1;
				if (!OutValue.Expression)
				{
					OutError = FString::Printf(TEXT("Failed to create a StaticBool node for literal '%s'."), *Expression->Text);
					return false;
				}
				return true;
			}

			OutError = FString::Printf(TEXT("Unknown Graph identifier '%s'."), *Expression->Text);
			return false;
		}

		case ECodeExpressionKind::NumberLiteral:
		{
			double ParsedValue = 0.0;
			if (!ParseScalarLiteral(Expression->Text, ParsedValue))
			{
				OutError = FString::Printf(TEXT("Invalid numeric literal '%s'."), *Expression->Text);
				return false;
			}

			OutValue.Expression = CreateScalarLiteralNode(ParsedValue, ConsumeNodeY());
			OutValue.ComponentCount = 1;
			return OutValue.Expression != nullptr;
		}

		case ECodeExpressionKind::StringLiteral:
			OutError = TEXT("String literals can only be used in named UE builtin arguments.");
			return false;

		case ECodeExpressionKind::Unary:
			return EvaluateUnary(Expression, OutValue, OutError);

		case ECodeExpressionKind::Binary:
			return EvaluateBinary(Expression, OutValue, OutError);

		case ECodeExpressionKind::MemberAccess:
			return EvaluateMemberAccess(Expression, OutValue, OutError);

		case ECodeExpressionKind::Call:
			return EvaluateCall(Expression, OutValue, OutError);

		default:
			OutError = TEXT("Unsupported Graph expression kind.");
			return false;
		}
	}

	bool FCodeGraphBuilder::EvaluateUnary(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError)
	{
		FCodeValue Operand;
		if (!EvaluateExpression(Expression->Left, Operand, OutError))
		{
			return false;
		}

		if (Expression->Text == TEXT("+"))
		{
			OutValue = Operand;
			return true;
		}

		if (Expression->Text == TEXT("-"))
		{
			FCodeValue MinusOne;
			MinusOne.Expression = CreateScalarLiteralNode(-1.0, ConsumeNodeY());
			MinusOne.ComponentCount = 1;
			return CreateBinaryOperatorNode(TEXT("*"), Operand, MinusOne, OutValue, OutError);
		}

		OutError = FString::Printf(TEXT("Unsupported unary operator '%s'."), *Expression->Text);
		return false;
	}

	bool FCodeGraphBuilder::EvaluateBinary(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError)
	{
		FCodeValue LeftValue;
		FCodeValue RightValue;
		if (!EvaluateExpression(Expression->Left, LeftValue, OutError)
			|| !EvaluateExpression(Expression->Right, RightValue, OutError))
		{
			return false;
		}

		return CreateBinaryOperatorNode(Expression->Text, LeftValue, RightValue, OutValue, OutError);
	}

	bool FCodeGraphBuilder::CreateBinaryOperatorNode(
		const FString& Operator,
		const FCodeValue& LeftValue,
		const FCodeValue& RightValue,
		FCodeValue& OutValue,
		FString& OutError)
	{
		FCodeValue LeftOperand = LeftValue;
		FCodeValue RightOperand = RightValue;

		if (LeftOperand.bIsTextureObject || RightOperand.bIsTextureObject)
		{
			OutError = TEXT("Arithmetic operators cannot be applied to texture values.");
			return false;
		}
		if (LeftOperand.bIsMaterialAttributes || RightOperand.bIsMaterialAttributes)
		{
			OutError = TEXT("Arithmetic operators cannot be applied to MaterialAttributes values.");
			return false;
		}
		if (LeftOperand.bIsSubstrateMaterial || RightOperand.bIsSubstrateMaterial)
		{
			OutError = TEXT("Arithmetic operators cannot be applied to Substrate values.");
			return false;
		}

		if (!IsScalarVectorCompatible(LeftOperand, RightOperand))
		{
			auto TryCoerceNonAuthoritativeOperand = [this, &OutError](const FCodeValue& AuthoritativeValue, FCodeValue& OtherValue) -> bool
			{
				if (!AuthoritativeValue.bHasAuthoritativeComponentCount
					|| OtherValue.bHasAuthoritativeComponentCount
					|| AuthoritativeValue.ComponentCount <= 0
					|| OtherValue.ComponentCount > AuthoritativeValue.ComponentCount)
				{
					// Only widen/splat the non-authoritative operand up to the authoritative size.
					// Never narrow it (that would silently drop channels) — let the size-mismatch
					// error below fire instead.
					return false;
				}

				FCodeValue CoercedValue;
				FString CoerceError;
				if (!CoerceValueToType(OtherValue, AuthoritativeValue.ComponentCount, false, CoercedValue, CoerceError))
				{
					return false;
				}

				OtherValue = CoercedValue;
				return true;
			};

			TryCoerceNonAuthoritativeOperand(LeftOperand, RightOperand)
				|| TryCoerceNonAuthoritativeOperand(RightOperand, LeftOperand);
		}

		if (!IsScalarVectorCompatible(LeftOperand, RightOperand))
		{
			OutError = FString::Printf(
				TEXT("Operator '%s' requires matching vector sizes or a scalar/vector pair, got %d and %d component(s)."),
				*Operator,
				LeftOperand.ComponentCount,
				RightOperand.ComponentCount);
			return false;
		}

		FString ReuseKey = FString::Printf(
			TEXT("binary-node|%s|%s|%s"),
			*Operator,
			*MakeCodeValueReuseToken(LeftOperand),
			*MakeCodeValueReuseToken(RightOperand));
		if (TryFindReusableExpressionValue(ReuseKey, OutValue))
		{
			return true;
		}

		UMaterialExpression* Expression = nullptr;
		const int32 PositionY = ConsumeNodeY();

		if (Operator == TEXT("+"))
		{
			auto* AddExpression = Cast<UMaterialExpressionAdd>(
				CreateExpression(UMaterialExpressionAdd::StaticClass(), 160, PositionY));
			if (AddExpression)
			{
				ConnectCodeValueToInput(AddExpression->A, LeftOperand);
				ConnectCodeValueToInput(AddExpression->B, RightOperand);
				Expression = AddExpression;
			}
		}
		else if (Operator == TEXT("-"))
		{
			auto* SubtractExpression = Cast<UMaterialExpressionSubtract>(
				CreateExpression(UMaterialExpressionSubtract::StaticClass(), 160, PositionY));
			if (SubtractExpression)
			{
				ConnectCodeValueToInput(SubtractExpression->A, LeftOperand);
				ConnectCodeValueToInput(SubtractExpression->B, RightOperand);
				Expression = SubtractExpression;
			}
		}
		else if (Operator == TEXT("*"))
		{
			auto* MultiplyExpression = Cast<UMaterialExpressionMultiply>(
				CreateExpression(UMaterialExpressionMultiply::StaticClass(), 160, PositionY));
			if (MultiplyExpression)
			{
				ConnectCodeValueToInput(MultiplyExpression->A, LeftOperand);
				ConnectCodeValueToInput(MultiplyExpression->B, RightOperand);
				Expression = MultiplyExpression;
			}
		}
		else if (Operator == TEXT("/"))
		{
			if (LeftOperand.bIsIntegerType && RightOperand.bIsIntegerType)
			{
				OutError = TEXT("Integer division is not supported by the material graph; use float() or floor(a/b).");
				return false;
			}
			auto* DivideExpression = Cast<UMaterialExpressionDivide>(
				CreateExpression(UMaterialExpressionDivide::StaticClass(), 160, PositionY));
			if (DivideExpression)
			{
				ConnectCodeValueToInput(DivideExpression->A, LeftOperand);
				ConnectCodeValueToInput(DivideExpression->B, RightOperand);
				Expression = DivideExpression;
			}
		}

		if (!Expression)
		{
			OutError = FString::Printf(TEXT("Unsupported or failed binary operator '%s'."), *Operator);
			return false;
		}

		OutValue.Expression = Expression;
		OutValue.ComponentCount = FMath::Max(LeftOperand.ComponentCount, RightOperand.ComponentCount);
		OutValue.bHasAuthoritativeComponentCount =
			LeftOperand.bHasAuthoritativeComponentCount || RightOperand.bHasAuthoritativeComponentCount;
		ClearCodeValueInputMask(OutValue);
		AddReusableExpressionValue(ReuseKey, OutValue);
		return true;
	}

	bool FCodeGraphBuilder::EvaluateMemberAccess(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError)
	{
		FCodeValue BaseValue;
		if (!EvaluateExpression(Expression->Left, BaseValue, OutError))
		{
			return false;
		}

		if (BaseValue.bIsMaterialAttributes)
		{
			FResolvedMaterialProperty ResolvedProperty;
			if (!ResolveMaterialProperty(Expression->Text, ResolvedProperty)
				|| ResolvedProperty.Property == MP_MaterialAttributes)
			{
				OutError = FString::Printf(TEXT("Unsupported MaterialAttributes member '%s'."), *Expression->Text);
				return false;
			}

			int32 OutputComponents = 0;
			if (!TryGetComponentCountForOutputType(ResolvedProperty.OutputType, OutputComponents) || OutputComponents <= 0)
			{
				OutError = FString::Printf(TEXT("MaterialAttributes member '%s' cannot be read as a numeric value."), *Expression->Text);
				return false;
			}

			auto* BreakAttributes = Cast<UMaterialExpressionBreakMaterialAttributes>(
				CreateExpression(UMaterialExpressionBreakMaterialAttributes::StaticClass(), 360, ConsumeNodeY()));
			if (!BreakAttributes)
			{
				OutError = TEXT("Failed to create a BreakMaterialAttributes node.");
				return false;
			}

			ConnectCodeValueToInput(BreakAttributes->MaterialAttributes, BaseValue);
			int32 OutputIndex = INDEX_NONE;

			// Prefer resolving the Break output by display name (mirrors the write side in
			// DreamShaderMaterialGeneratorCodeShared.h). Keeps the read path in sync with the actual
			// node output layout across engine/Substrate builds and covers first-class attributes
			// (e.g. SurfaceThickness) that the legacy literal-index switch can miss.
			const FGuid BreakAttributeId = FMaterialAttributeDefinitionMap::GetID(ResolvedProperty.Property);
			const FString BreakAttributeDisplayName =
				FMaterialAttributeDefinitionMap::GetDisplayNameForMaterial(BreakAttributeId, BreakAttributes->Material).ToString();
			for (int32 CandidateIndex = 0; CandidateIndex < BreakAttributes->Outputs.Num(); ++CandidateIndex)
			{
				if (BreakAttributes->Outputs[CandidateIndex].OutputName.ToString().Equals(BreakAttributeDisplayName, ESearchCase::IgnoreCase))
				{
					OutputIndex = CandidateIndex;
					break;
				}
			}
			if (!BreakAttributes->Outputs.IsValidIndex(OutputIndex))
			{
				// Fall back to the legacy literal-index table for outputs without a matching name.
				if (!TryResolveMaterialAttributesBreakOutputIndex(ResolvedProperty.Property, OutputIndex)
					|| !BreakAttributes->Outputs.IsValidIndex(OutputIndex))
				{
					OutError = FString::Printf(TEXT("BreakMaterialAttributes does not expose member '%s'."), *Expression->Text);
					return false;
				}
			}

			OutValue.Expression = BreakAttributes;
			OutValue.OutputIndex = OutputIndex;
			OutValue.ComponentCount = OutputComponents;
			OutValue.bIsTextureObject = false;
			OutValue.bIsMaterialAttributes = false;
			return true;
		}

		if (BaseValue.bIsTextureObject)
		{
			OutError = TEXT("Texture values do not support swizzle/member access in Code.");
			return false;
		}
		if (BaseValue.bIsSubstrateMaterial)
		{
			OutError = TEXT("Substrate values do not support swizzle/member access in Graph.");
			return false;
		}

		return CreateSwizzleExpression(BaseValue, Expression->Text, OutValue, OutError);
	}

	bool FCodeGraphBuilder::AppendValues(const TArray<FCodeValue>& Parts, FCodeValue& OutValue, FString& OutError)
	{
		if (Parts.IsEmpty())
		{
			OutError = TEXT("Cannot build an empty vector.");
			return false;
		}

		if (Parts.Num() == 1)
		{
			OutValue = Parts[0];
			return true;
		}

		int32 TotalComponentCount = 0;
		for (const FCodeValue& Part : Parts)
		{
			if (Part.bIsTextureObject || Part.bIsMaterialAttributes || Part.bIsSubstrateMaterial)
			{
				OutError = TEXT("AppendVector inputs must be numeric scalar/vector values.");
				return false;
			}
			TotalComponentCount += Part.ComponentCount;
		}

		if (TotalComponentCount > 4)
		{
			OutError = FString::Printf(TEXT("AppendVector cannot build %d components; Unreal material vectors support at most 4."), TotalComponentCount);
			return false;
		}

		TArray<FString> ReuseTokens;
		ReuseTokens.Reserve(Parts.Num());
		for (const FCodeValue& Part : Parts)
		{
			ReuseTokens.Add(MakeCodeValueReuseToken(Part));
		}
		const FString ReuseKey = FString::Printf(TEXT("append|%s"), *FString::Join(ReuseTokens, TEXT("|")));
		if (TryFindReusableExpressionValue(ReuseKey, OutValue))
		{
			return true;
		}

		FCodeValue Current = Parts[0];
		for (int32 Index = 1; Index < Parts.Num(); ++Index)
		{
			auto* AppendExpression = Cast<UMaterialExpressionAppendVector>(
				CreateExpression(UMaterialExpressionAppendVector::StaticClass(), 360, ConsumeNodeY()));
			if (!AppendExpression)
			{
				OutError = TEXT("Failed to create an AppendVector node.");
				return false;
			}

			ConnectCodeValueToInput(AppendExpression->A, Current);
			ConnectCodeValueToInput(AppendExpression->B, Parts[Index]);

			const bool bHasAuthoritativeComponentCount =
				Current.bHasAuthoritativeComponentCount || Parts[Index].bHasAuthoritativeComponentCount;
			Current.Expression = AppendExpression;
			Current.OutputIndex = 0;
			Current.ComponentCount += Parts[Index].ComponentCount;
			Current.bHasAuthoritativeComponentCount = bHasAuthoritativeComponentCount;
			ClearCodeValueInputMask(Current);
		}

		OutValue = Current;
		AddReusableExpressionValue(ReuseKey, OutValue);
		return true;
	}

	bool FCodeGraphBuilder::EvaluateCall(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError)
	{
		FString CalleeName;
		if (!TryFlattenQualifiedName(Expression->Left, CalleeName))
		{
			OutError = TEXT("Graph calls must target a named function.");
			return false;
		}

		if (IsVectorConstructorName(CalleeName))
		{
			if (!EvaluateVectorConstructor(CalleeName, Expression->Arguments, OutValue, OutError))
			{
				return false;
			}
			OutValue.bIsIntegerType = IsIntegerConstructorName(CalleeName);
			return true;
		}

		// UE.SceneTexture(Id="PostProcessInput0") is the DSL's cross-backend scene-read surface (the
		// Instance backend lowers it to a named value-input chunk). In the node-graph evaluator it
		// desugars to the generic node builtin -- UE.Expression(Class="SceneTexture",
		// OutputType="float4", SceneTextureId=<Id>) -- producing a real UMaterialExpressionSceneTexture.
		// The Id string passes through verbatim: the reflected byte-enum writer resolves prefixless
		// entry names ("PostProcessInput0" -> PPI_PostProcessInput0, see TryResolveEnumLiteral).
		if (CalleeName.Equals(TEXT("UE.SceneTexture"), ESearchCase::IgnoreCase))
		{
			const FCodeCallArgument* IdArgument = nullptr;
			for (const FCodeCallArgument& Argument : Expression->Arguments)
			{
				if (Argument.bIsNamed && Argument.Name.Equals(TEXT("Id"), ESearchCase::IgnoreCase))
				{
					IdArgument = &Argument;
					break;
				}
			}
			if (!IdArgument || Expression->Arguments.Num() != 1)
			{
				OutError = TEXT("UE.SceneTexture expects exactly Id=\"...\" (e.g. Id=\"PostProcessInput0\").");
				return false;
			}

			const auto MakeStringLiteral = [](const TCHAR* Text)
			{
				TSharedPtr<FCodeExpression> Literal = MakeShared<FCodeExpression>();
				Literal->Kind = ECodeExpressionKind::StringLiteral;
				Literal->Text = Text;
				return Literal;
			};
			const auto MakeNamedArgument = [](const TCHAR* Name, const TSharedPtr<FCodeExpression>& ArgumentExpression)
			{
				FCodeCallArgument Argument;
				Argument.Name = Name;
				Argument.Expression = ArgumentExpression;
				Argument.bIsNamed = true;
				return Argument;
			};

			TArray<FCodeCallArgument> DesugaredArguments;
			DesugaredArguments.Add(MakeNamedArgument(TEXT("Class"), MakeStringLiteral(TEXT("SceneTexture"))));
			DesugaredArguments.Add(MakeNamedArgument(TEXT("OutputType"), MakeStringLiteral(TEXT("float4"))));
			DesugaredArguments.Add(MakeNamedArgument(TEXT("SceneTextureId"), IdArgument->Expression));
			return EvaluateUEBuiltinCall(TEXT("UE.Expression"), DesugaredArguments, OutValue, OutError);
		}

		if (CalleeName.StartsWith(TEXT("UE."), ESearchCase::IgnoreCase))
		{
			return EvaluateUEBuiltinCall(CalleeName, Expression->Arguments, OutValue, OutError);
		}

		if (CalleeName.StartsWith(TEXT("Substrate."), ESearchCase::IgnoreCase))
		{
			return EvaluateUEBuiltinCall(CalleeName, Expression->Arguments, OutValue, OutError);
		}

		FString MathBuiltinError;
		if (EvaluateMathBuiltinCall(CalleeName, Expression->Arguments, OutValue, MathBuiltinError))
		{
			return true;
		}
		if (!MathBuiltinError.IsEmpty())
		{
			OutError = MathBuiltinError;
			return false;
		}

		// SampleTexture2D(textureObject, uv) is the DSL's cross-backend texture-sampling surface: the
		// Instance backend lowers it to the DS_ HLSL macro, and here in the node-graph evaluator it
		// desugars to the generic node builtin -- exactly
		// UE.Expression(Class="TextureSample", OutputType="float4", TextureObject=..., Coordinates=...).
		// Reserved (checked before user properties/functions) to match the Instance lowering.
		if (CalleeName.Equals(TEXT("SampleTexture2D"), ESearchCase::CaseSensitive))
		{
			if (Expression->Arguments.Num() != 2
				|| Expression->Arguments[0].bIsNamed
				|| Expression->Arguments[1].bIsNamed)
			{
				OutError = TEXT("SampleTexture2D expects exactly two positional arguments: (textureObject, uv).");
				return false;
			}

			const auto MakeStringLiteral = [](const TCHAR* Text)
			{
				TSharedPtr<FCodeExpression> Literal = MakeShared<FCodeExpression>();
				Literal->Kind = ECodeExpressionKind::StringLiteral;
				Literal->Text = Text;
				return Literal;
			};
			const auto MakeNamedArgument = [](const TCHAR* Name, const TSharedPtr<FCodeExpression>& ArgumentExpression)
			{
				FCodeCallArgument Argument;
				Argument.Name = Name;
				Argument.Expression = ArgumentExpression;
				Argument.bIsNamed = true;
				return Argument;
			};

			TArray<FCodeCallArgument> DesugaredArguments;
			DesugaredArguments.Add(MakeNamedArgument(TEXT("Class"), MakeStringLiteral(TEXT("TextureSample"))));
			DesugaredArguments.Add(MakeNamedArgument(TEXT("OutputType"), MakeStringLiteral(TEXT("float4"))));
			DesugaredArguments.Add(MakeNamedArgument(TEXT("TextureObject"), Expression->Arguments[0].Expression));
			DesugaredArguments.Add(MakeNamedArgument(TEXT("Coordinates"), Expression->Arguments[1].Expression));
			return EvaluateUEBuiltinCall(TEXT("UE.Expression"), DesugaredArguments, OutValue, OutError);
		}

		if (const FTextShaderPropertyDefinition* Property = FindPropertyDefinition(CalleeName))
		{
			if (Property->ParameterNodeType.Equals(TEXT("StaticSwitchParameter"), ESearchCase::IgnoreCase))
			{
				return EvaluateStaticSwitchParameterCall(*Property, Expression->Arguments, OutValue, OutError);
			}
			// Parameter node types that own input pins (channel/component mask, texture samples) accept a
			// call form that wires those pins: e.g. Msk(Input=Col), TexCube(Coordinates=Dir).
			if (ParameterTypeAcceptsInputArguments(Property->ParameterNodeType))
			{
				return EvaluateConfigurableParameterCall(*Property, Expression->Arguments, OutValue, OutError);
			}
		}

		const FTextShaderFunctionDefinition* CustomFunction = FindFunctionDefinition(CalleeName);
		const FTextShaderFunctionDefinition* GraphFunction = FindGraphFunctionDefinition(CalleeName);
		const FTextShaderMaterialFunctionDefinition* MaterialFunctionDefinition = FindMaterialFunctionDefinition(CalleeName);
		const FTextShaderVirtualFunctionDefinition* VirtualFunctionDefinition = FindVirtualFunctionDefinition(CalleeName);

		TArray<FString> MatchedKinds;
		if (CustomFunction)
		{
			MatchedKinds.Add(TEXT("Function"));
		}
		if (GraphFunction)
		{
			MatchedKinds.Add(TEXT("GraphFunction"));
		}
		if (MaterialFunctionDefinition)
		{
			MatchedKinds.Add(UE::DreamShader::LexToString(MaterialFunctionDefinition->Kind));
		}
		if (VirtualFunctionDefinition)
		{
			MatchedKinds.Add(TEXT("VirtualFunction"));
		}
		if (MatchedKinds.Num() > 1)
		{
			OutError = FString::Printf(
				TEXT("Graph call '%s' is ambiguous because multiple definitions use that name: %s."),
				*CalleeName,
				*FString::Join(MatchedKinds, TEXT(", ")));
			return false;
		}

		if (MaterialFunctionDefinition)
		{
			return EvaluateMaterialFunctionCall(*MaterialFunctionDefinition, Expression->Arguments, OutValue, OutError);
		}
		if (VirtualFunctionDefinition)
		{
			return EvaluateVirtualFunctionCall(*VirtualFunctionDefinition, Expression->Arguments, OutValue, OutError);
		}
		if (GraphFunction)
		{
			return EvaluateGraphFunctionCall(*GraphFunction, Expression->Arguments, OutValue, OutError);
		}

		return EvaluateCustomFunctionCall(CalleeName, Expression->Arguments, OutValue, OutError);
	}

}
