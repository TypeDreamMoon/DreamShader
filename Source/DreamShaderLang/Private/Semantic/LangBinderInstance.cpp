// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Instance mode: the binder for a `.dsi`.
//
// A `.dsi` is one `#pragma instance(Parent = "...", Key = Value, ...)` and a list of `uniform` overrides,
// and it becomes one UMaterialInstanceConstant. Nothing in it lowers to a node: an override is an
// assignment to a parameter of the parent, and this pass says which parameter, of which engine kind,
// with which value, and whether the parent has it. The parent's parameters arrive as plain data
// (FBindOptions::ParentSchema) produced by the host -- from the parent source's IR
// (BuildParameterSchemaFromIR) or from the loaded parent asset. Without one, only the shape of each
// override is checked and DSH7263 says so, once.
//
// The instance keys other than Parent are carried through as written: they name engine properties that
// the Compiler layer resolves and checks (DSH8249 and friends), never this module.
//
// Diagnostics owned by this file: DSH7250-DSH7270.

#include "LangBinderInternal.h"

#include "IR/IR.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/Char.h"
#include "Misc/CString.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "DreamShader.Binder.Instance"

namespace UE::DreamShader::Lang::Private
{
	// A named namespace, not an anonymous one: the module builds as one unity blob.
	namespace LangBinderInstancePrivate
	{
		/** The parameter kinds a `.dsi` spells with a type name that has no FIRType. False for every other spelling. */
		bool TryGetInstanceOnlyParameterKind(const FTypeRef& Type, IR::EIRParameterKind& OutKind)
		{
			if (Type.Category != ETypeCategory::Named)
			{
				return false;
			}

			struct FInstanceOnlyType
			{
				const TCHAR* Name;
				IR::EIRParameterKind Kind;
			};
			static const FInstanceOnlyType Types[] =
			{
				{ TEXT("RuntimeVirtualTexture"), IR::EIRParameterKind::RuntimeVirtualTexture },
				{ TEXT("SparseVolumeTexture"), IR::EIRParameterKind::SparseVolumeTexture },
				{ TEXT("TextureCollection"), IR::EIRParameterKind::TextureCollection },
				{ TEXT("ParameterCollection"), IR::EIRParameterKind::ParameterCollection },
				{ TEXT("Font"), IR::EIRParameterKind::Font },
			};
			for (const FInstanceOnlyType& Entry : Types)
			{
				if (Type.Name.Equals(Entry.Name, ESearchCase::CaseSensitive))
				{
					OutKind = Entry.Kind;
					return true;
				}
			}
			return false;
		}

		/** The kinds whose value is an asset reference (`/// @default <path|None>`), not an HLSL value. */
		bool IsAssetParameterKind(const IR::EIRParameterKind Kind)
		{
			switch (Kind)
			{
			case IR::EIRParameterKind::Texture:
			case IR::EIRParameterKind::TextureCollection:
			case IR::EIRParameterKind::Font:
			case IR::EIRParameterKind::RuntimeVirtualTexture:
			case IR::EIRParameterKind::SparseVolumeTexture:
			case IR::EIRParameterKind::ParameterCollection:
				return true;
			case IR::EIRParameterKind::Scalar:
			case IR::EIRParameterKind::Vector:
			case IR::EIRParameterKind::DoubleVector:
			case IR::EIRParameterKind::StaticSwitch:
			case IR::EIRParameterKind::StaticComponentMask:
				return false;
			}
			return false;
		}

		bool IsStaticParameterKind(const IR::EIRParameterKind Kind)
		{
			return Kind == IR::EIRParameterKind::StaticSwitch || Kind == IR::EIRParameterKind::StaticComponentMask;
		}

		/** `VolumeTexture` is the 1.x spelling of a Texture3D; the two are one dimension. */
		ETextureKind NormaliseInstanceTextureKind(const ETextureKind Kind)
		{
			return Kind == ETextureKind::VolumeTexture ? ETextureKind::Texture3D : Kind;
		}

		/** `@page 3`: a whole number of zero or more. */
		bool TryParseInstanceFontPage(const FString& Text, int32& OutPage)
		{
			const FString Trimmed = Text.TrimStartAndEnd();
			if (Trimmed.IsEmpty() || Trimmed.Len() > 9)
			{
				return false;
			}
			for (const TCHAR Char : Trimmed)
			{
				if (!FChar::IsDigit(Char))
				{
					return false;
				}
			}
			OutPage = FCString::Atoi(*Trimmed);
			return true;
		}

		/** How a parent parameter reads in a message: its declared type when the parent is DreamShader source, else its kind. */
		FString DescribeSchemaEntryType(const IR::FIRParameterSchemaEntry& Entry)
		{
			return Entry.DeclaredType.IsError() ? FString(IR::LexToString(Entry.Kind)) : Entry.DeclaredType.ToString();
		}

		/**
		 * Whether an override spelled as Type (of kind Kind) can assign the parent parameter Entry. The rules of
		 * A scalar takes any scalar spelling; a vector
		 * takes the parent's declared width or float4 (float3 or float4 for a foreign parent); a DoubleVector
		 * takes a four-component vector; a texture's dimension must match when both sides know it; every other
		 * kind must be exactly that kind. `@static` has been checked before this is asked.
		 */
		bool IsInstanceOverrideCompatible(const IR::FIRType& Type, const IR::EIRParameterKind Kind, const IR::FIRParameterSchemaEntry& Entry)
		{
			switch (Entry.Kind)
			{
			case IR::EIRParameterKind::Scalar:
				return Kind == IR::EIRParameterKind::Scalar;
			case IR::EIRParameterKind::Vector:
				if (Kind != IR::EIRParameterKind::Vector)
				{
					return false;
				}
				if (Type.Rows == 4)
				{
					return true;
				}
				return Entry.DeclaredType.IsError() ? Type.Rows == 3 : Type.Rows == Entry.DeclaredType.Rows;
			case IR::EIRParameterKind::DoubleVector:
				return (Kind == IR::EIRParameterKind::DoubleVector || Kind == IR::EIRParameterKind::Vector) && Type.Rows == 4;
			case IR::EIRParameterKind::StaticSwitch:
			case IR::EIRParameterKind::StaticComponentMask:
				return Kind == Entry.Kind;
			case IR::EIRParameterKind::Texture:
				if (Kind != IR::EIRParameterKind::Texture)
				{
					return false;
				}
				if (Entry.TextureKind == ETextureKind::None || !Type.IsTexture())
				{
					return true;
				}
				return NormaliseInstanceTextureKind(Entry.TextureKind) == NormaliseInstanceTextureKind(Type.Texture);
			case IR::EIRParameterKind::TextureCollection:
			case IR::EIRParameterKind::Font:
			case IR::EIRParameterKind::RuntimeVirtualTexture:
			case IR::EIRParameterKind::SparseVolumeTexture:
			case IR::EIRParameterKind::ParameterCollection:
				return Kind == Entry.Kind;
			}
			return false;
		}
	}

	// ---------------------------------------------------------------------------------------------
	// The run
	// ---------------------------------------------------------------------------------------------

	void FLangBinder::BindInstanceModule()
	{
		Bound.Instance.bIsInstance = true;
		Bound.ParentSchema = Options.ParentSchema;
		CurrentFile = RootModule.FilePath;

		for (const FDeclPtr& DeclPtr : RootModule.Declarations)
		{
			const FDecl* Decl = DeclPtr.Get();
			if (!Decl)
			{
				continue;
			}

			// Keyed like the declare pass keys every file-scope declaration; a `.dsi` has no regions.
			Bound.StatementRegions.Add(Decl, INDEX_NONE);

			switch (Decl->Kind)
			{
			case ENodeKind::PragmaDecl:
			{
				const FPragmaDecl& Pragma = *static_cast<const FPragmaDecl*>(Decl);
				switch (Pragma.PragmaKind)
				{
				case EPragmaKind::Instance:
					BindInstancePragma(Pragma);
					break;
				case EPragmaKind::Material:
					Diagnostics.Error(
						TEXT("DSH7254"),
						CurrentFile,
						Pragma.Span,
						LOCTEXT("InstanceHoldsMaterialPragma", "A '.dsi' is configured by '#pragma instance', and '#pragma material' configures a material; write these keys in '#pragma instance(...)'."));
					break;
				case EPragmaKind::Layout:
				case EPragmaKind::Region:
				case EPragmaKind::EndRegion:
					Diagnostics.Warning(
						TEXT("DSH7267"),
						CurrentFile,
						Pragma.Span,
						FText::Format(
							LOCTEXT("InstanceIgnoresGraphPragma", "'#pragma {0}' boxes or places graph nodes, and a '.dsi' has no graph; the line was ignored."),
							FText::FromString(Pragma.Name)));
					break;
				case EPragmaKind::Pipeline:
					ReportPipelinePragmaOutsideDsp(Pragma);
					break;
				case EPragmaKind::Unknown:
					break;
				}
				break;
			}

			case ENodeKind::BufferDecl:
			case ENodeKind::PassDecl:
				ReportPipelineDeclarationOutsideDsp(*Decl);
				break;

			case ENodeKind::VariableDecl:
			{
				const FVariableDecl& Variable = *static_cast<const FVariableDecl*>(Decl);
				if (Variable.Storage == EStorageClass::Uniform)
				{
					DeclareInstanceOverride(Variable);
				}
				else
				{
					Diagnostics.Error(
						TEXT("DSH7254"),
						CurrentFile,
						Variable.Declarator.NameSpan,
						FText::Format(
							LOCTEXT("InstanceHoldsConstant", "A '.dsi' holds a '#pragma instance' and 'uniform' overrides, and '{0}' is not a 'uniform'; an instance only assigns parameters of its parent."),
							FText::FromString(Variable.Declarator.Name)));
				}
				break;
			}

			case ENodeKind::FunctionDecl:
			{
				const FFunctionDecl& Function = *static_cast<const FFunctionDecl*>(Decl);
				Diagnostics.Error(
					TEXT("DSH7254"),
					CurrentFile,
					Function.NameSpan,
					FText::Format(
						LOCTEXT("InstanceHoldsFunction", "A '.dsi' holds a '#pragma instance' and 'uniform' overrides, and '{0}' is a function; write it in a '.dss' or a '.dsh'."),
						FText::FromString(Function.Name)));
				break;
			}

			case ENodeKind::StructDecl:
			{
				const FStructDecl& Struct = *static_cast<const FStructDecl*>(Decl);
				Diagnostics.Error(
					TEXT("DSH7254"),
					CurrentFile,
					Struct.NameSpan,
					FText::Format(
						LOCTEXT("InstanceHoldsStruct", "A '.dsi' holds a '#pragma instance' and 'uniform' overrides, and 'struct {0}' declares a type; write it in a '.dsh'."),
						FText::FromString(Struct.Name)));
				break;
			}

			case ENodeKind::IncludeDecl:
			{
				const FIncludeDecl& Include = *static_cast<const FIncludeDecl*>(Decl);
				Diagnostics.Error(
					TEXT("DSH7254"),
					CurrentFile,
					Include.PathSpan,
					FText::Format(
						LOCTEXT("InstanceHoldsInclude", "A '.dsi' holds a '#pragma instance' and 'uniform' overrides, and has no code that could use '{0}'; remove the include."),
						FText::FromString(Include.Path)));
				break;
			}

			default:
				break;
			}
		}

		if (!Bound.Instance.Pragma)
		{
			Diagnostics.Error(
				TEXT("DSH7250"),
				CurrentFile,
				FLangSpan(),
				LOCTEXT("InstanceNoPragma", "A '.dsi' needs one '#pragma instance(Parent = \"...\")' naming the material it is an instance of, and this file has none."));
		}

		BindInstanceInitializers();
		CheckInstanceOverrides();
		BuildInstanceProduct();
	}

	void FLangBinder::ReportInstancePragmaOutsideDsi(const FPragmaDecl& Pragma)
	{
		Diagnostics.Error(
			TEXT("DSH7255"),
			CurrentFile,
			Pragma.Span,
			FText::Format(
				LOCTEXT("InstancePragmaOutsideDsi", "'#pragma instance' declares a material instance and belongs in a '.dsi' file of its own, and this line is in '{0}'; move it and its overrides into a '.dsi'."),
				FText::FromString(FPaths::GetCleanFilename(CurrentFile))));
	}

	// ---------------------------------------------------------------------------------------------
	// `#pragma instance`
	// ---------------------------------------------------------------------------------------------

	void FLangBinder::BindInstancePragma(const FPragmaDecl& Pragma)
	{
		if (Bound.Instance.Pragma)
		{
			Diagnostics.Error(
				TEXT("DSH7251"),
				CurrentFile,
				Pragma.Span,
				FText::Format(
					LOCTEXT("InstanceSecondPragma", "'#pragma instance' is written a second time, and one '.dsi' is one material instance; the line {0} already declares it."),
					FText::AsNumber(Bound.Instance.Pragma->Span.Line)));
			return;
		}
		Bound.Instance.Pragma = &Pragma;

		// The `///` block above the pragma names the asset; nothing else is read there.
		for (const FDocDirective& Entry : Pragma.Doc.Directives)
		{
			if (Entry.Key.Equals(Directive::Name, ESearchCase::CaseSensitive))
			{
				const FString Value = Entry.Value.TrimStartAndEnd();
				if (Value.IsEmpty())
				{
					Diagnostics.Error(
						TEXT("DSH7227"),
						CurrentFile,
						Entry.Span,
						FText::Format(
							LOCTEXT("InstanceNameNeedsValue", "'@{0}' needs a value after it."),
							FText::FromString(Entry.Key)));
				}
				else
				{
					InstanceAssetName = Value;
				}
				continue;
			}
			Diagnostics.Warning(
				TEXT("DSH7262"),
				CurrentFile,
				Entry.Span,
				FText::Format(
					LOCTEXT("InstancePragmaDirectiveNoEffect", "'@{0}' has no effect above '#pragma instance', where only '@name' is read; remove it."),
					FText::FromString(Entry.Key)));
		}

		// Keys compare the way the engine properties they name compare: ignoring case.
		TArray<const FPragmaArgument*> SeenKeys;
		bool bHasParent = false;
		for (const FPragmaArgument& Argument : Pragma.Arguments)
		{
			if (Argument.Key.IsEmpty())
			{
				Diagnostics.Error(
					TEXT("DSH7270"),
					CurrentFile,
					Argument.Span,
					FText::Format(
						LOCTEXT("InstancePragmaPositional", "'#pragma instance' takes 'Key = Value' pairs, and '{0}' has no key."),
						FText::FromString(Argument.Value)));
				continue;
			}

			const FPragmaArgument* const* First = SeenKeys.FindByPredicate([&Argument](const FPragmaArgument* Seen)
			{
				return Seen->Key.Equals(Argument.Key, ESearchCase::IgnoreCase);
			});
			if (First)
			{
				Diagnostics.Error(
					TEXT("DSH7253"),
					CurrentFile,
					Argument.Span,
					FText::Format(
						LOCTEXT("InstancePragmaDuplicateKey", "'{0}' is set twice by '#pragma instance'; it was already set on line {1}."),
						FText::FromString(Argument.Key),
						FText::AsNumber((*First)->Span.Line)));
				continue;
			}
			SeenKeys.Add(&Argument);

			if (Argument.Key.Equals(TEXT("Parent"), ESearchCase::IgnoreCase))
			{
				bHasParent = true;
				Bound.Instance.ParentReference = Argument.Value.TrimStartAndEnd();
				Bound.Instance.ParentSpan = Argument.Span;
				if (Bound.Instance.ParentReference.IsEmpty())
				{
					Diagnostics.Error(
						TEXT("DSH7252"),
						CurrentFile,
						Argument.Span,
						LOCTEXT("InstanceParentEmpty", "'Parent' in '#pragma instance' names the material this is an instance of, and it is empty."));
				}
				continue;
			}

			Bound.Instance.Settings.Emplace(Argument.Key, Argument.Value);
			Bound.Instance.SettingSpans.Add(Argument.Span);
		}

		if (!bHasParent)
		{
			Diagnostics.Error(
				TEXT("DSH7252"),
				CurrentFile,
				Pragma.Span,
				LOCTEXT("InstanceParentMissing", "'#pragma instance' needs 'Parent = \"/Game/.../M_Parent\"' naming the material this is an instance of, and it has none."));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Overrides
	// ---------------------------------------------------------------------------------------------

	FBoundDirectives FLangBinder::BindInstanceOverrideDirectives(const FDocBlock& Doc, int32& OutFontPage, bool& bOutHasPage)
	{
		using namespace LangBinderInstancePrivate;

		FBoundDirectives Out;
		Out.FreeText = FString::Join(Doc.FreeText, TEXT("\n")).TrimStartAndEnd();
		OutFontPage = 0;
		bOutHasPage = false;

		for (const FDocDirective& Entry : Doc.Directives)
		{
			const FString Value = Entry.Value.TrimStartAndEnd();

			if (Entry.Key.Equals(Directive::Name, ESearchCase::CaseSensitive)
				|| Entry.Key.Equals(Directive::Default, ESearchCase::CaseSensitive))
			{
				if (Value.IsEmpty())
				{
					Diagnostics.Error(
						TEXT("DSH7227"),
						CurrentFile,
						Entry.Span,
						FText::Format(
							LOCTEXT("InstanceDirectiveNeedsValue", "'@{0}' needs a value after it."),
							FText::FromString(Entry.Key)));
				}
				else if (Entry.Key.Equals(Directive::Name, ESearchCase::CaseSensitive))
				{
					Out.Name = Value;
				}
				else
				{
					Out.DefaultAsset = Value;
				}
			}
			else if (Entry.Key.Equals(Directive::Static, ESearchCase::CaseSensitive))
			{
				Out.bStatic = true;
			}
			else if (Entry.Key.Equals(Directive::Page, ESearchCase::CaseSensitive))
			{
				int32 Page = 0;
				if (TryParseInstanceFontPage(Value, Page))
				{
					OutFontPage = Page;
					bOutHasPage = true;
				}
				else
				{
					Diagnostics.Error(
						TEXT("DSH7266"),
						CurrentFile,
						Entry.Span,
						FText::Format(
							LOCTEXT("InstancePageMalformed", "'@page' takes one whole number of zero or more, and '{0}' is not that."),
							FText::FromString(Value)));
				}
			}
			else
			{
				// Group, description, slider, sort and every pass-through key describe the parameter node,
				// which lives on the parent; an instance can only change the value.
				Diagnostics.Warning(
					TEXT("DSH7262"),
					CurrentFile,
					Entry.Span,
					FText::Format(
						LOCTEXT("InstanceOverrideDirectiveNoEffect", "'@{0}' has no effect on an instance override, because an override assigns only the parent parameter's value and its metadata stays on the parent; remove it."),
						FText::FromString(Entry.Key)));
			}
		}

		Out.Desc = Out.FreeText;
		return Out;
	}

	void FLangBinder::DeclareInstanceOverride(const FVariableDecl& Decl)
	{
		using namespace LangBinderInstancePrivate;

		const FDeclarator& Declarator = Decl.Declarator;
		if (!CheckNameAvailable(Declarator.Name, Declarator.NameSpan, CurrentFile))
		{
			return;
		}

		FBoundGlobal Global;
		Global.Name = Declarator.Name;
		Global.Decl = &Decl;
		Global.Storage = Decl.Storage;
		Global.File = CurrentFile;
		Global.bIsParameter = true;

		IR::EIRParameterKind Kind = IR::EIRParameterKind::Scalar;
		bool bHasKind = false;
		if (TryGetInstanceOnlyParameterKind(Decl.Type, Kind))
		{
			// No FIRType spells these; the kind carries everything the instance needs.
			Global.Type = IR::FIRType::Error();
			bHasKind = true;
		}
		else if (!ResolveTypeRef(Decl.Type, Global.Type))
		{
			Global.Type = IR::FIRType::Error();
		}

		int32 FontPage = 0;
		bool bHasPage = false;
		Global.Directives = BindInstanceOverrideDirectives(Decl.Doc, FontPage, bHasPage);
		const bool bStatic = Global.Directives.bStatic;

		ResolveArrayCount(Declarator.ArrayDimensions, Declarator.Initializer.Get(), Declarator.Span, Global.ArrayCount);

		if (Global.ArrayCount > 0)
		{
			Diagnostics.Error(
				TEXT("DSH7269"),
				CurrentFile,
				Declarator.Span,
				FText::Format(
					LOCTEXT("InstanceOverrideArray", "'{0}' is an array, and an instance override assigns one parameter; override each element's parameter on its own."),
					FText::FromString(Declarator.Name)));
			bHasKind = false;
		}
		else if (bHasKind)
		{
			if (bStatic)
			{
				Diagnostics.Error(
					TEXT("DSH7260"),
					CurrentFile,
					Decl.Doc.Span,
					FText::Format(
						LOCTEXT("InstanceStaticOnAsset", "'/// @static' on an instance override means a static switch ('uniform bool') or a static component mask ('uniform bool4'), and '{0}' is declared '{1}'."),
						FText::FromString(Declarator.Name),
						FText::FromString(Decl.Type.Name)));
			}
		}
		else if (!Global.Type.IsError())
		{
			const IR::FIRType& Type = Global.Type;
			if (Type.IsTexture())
			{
				Kind = IR::EIRParameterKind::Texture;
				bHasKind = true;
			}
			else if (bStatic && Type.IsBool() && Type.Cols == 1 && (Type.Rows == 1 || Type.Rows == 4))
			{
				Kind = Type.Rows == 1 ? IR::EIRParameterKind::StaticSwitch : IR::EIRParameterKind::StaticComponentMask;
				bHasKind = true;
			}
			else if ((Type.IsNumeric() || Type.IsBool()) && Type.Cols == 1)
			{
				Kind = Type.Rows == 1
					? IR::EIRParameterKind::Scalar
					: ((Type.Kind == IR::EIRTypeKind::Double && Type.Rows == 4) ? IR::EIRParameterKind::DoubleVector : IR::EIRParameterKind::Vector);
				bHasKind = true;
				if (bStatic)
				{
					Diagnostics.Error(
						TEXT("DSH7260"),
						CurrentFile,
						Decl.Doc.Span,
						FText::Format(
							LOCTEXT("InstanceStaticOnValue", "'/// @static' on an instance override means a static switch ('uniform bool') or a static component mask ('uniform bool4'), and '{0}' is declared '{1}'."),
							FText::FromString(Declarator.Name),
							FText::FromString(Decl.Type.Name)));
				}
			}
			else
			{
				Diagnostics.Error(
					TEXT("DSH7269"),
					CurrentFile,
					Decl.Type.Span,
					FText::Format(
						LOCTEXT("InstanceOverrideNoKind", "'{0}' is declared '{1}', which is no material parameter type; an override is a number, a bool, a texture, or one of RuntimeVirtualTexture, SparseVolumeTexture, TextureCollection, ParameterCollection and Font."),
						FText::FromString(Declarator.Name),
						FText::FromString(Decl.Type.Name)));
			}
		}

		if (bHasKind)
		{
			if (IsAssetParameterKind(Kind))
			{
				if (Declarator.Initializer)
				{
					Diagnostics.Error(
						TEXT("DSH7257"),
						CurrentFile,
						Declarator.Initializer->Span,
						FText::Format(
							LOCTEXT("InstanceAssetInitializer", "'{0}' overrides a {1} parameter, which takes an asset rather than an HLSL value; write '/// @default /Game/...' (or '/// @default None') above it instead of an initializer."),
							FText::FromString(Declarator.Name),
							FText::FromString(IR::LexToString(Kind))));
				}
				else if (Global.Directives.DefaultAsset.IsEmpty())
				{
					Diagnostics.Error(
						TEXT("DSH7257"),
						CurrentFile,
						Declarator.NameSpan,
						FText::Format(
							LOCTEXT("InstanceAssetNoDefault", "'{0}' overrides a {1} parameter and needs the asset it is set to, as '/// @default /Game/...' or '/// @default None', and it has none."),
							FText::FromString(Declarator.Name),
							FText::FromString(IR::LexToString(Kind))));
				}
			}
			else
			{
				if (!Declarator.Initializer)
				{
					Diagnostics.Error(
						TEXT("DSH7256"),
						CurrentFile,
						Declarator.NameSpan,
						FText::Format(
							LOCTEXT("InstanceValueNoInitializer", "'{0}' overrides a {1} parameter and needs the value it is set to, as 'uniform {2} {0} = ...;', and it has no initializer."),
							FText::FromString(Declarator.Name),
							FText::FromString(IR::LexToString(Kind)),
							FText::FromString(Decl.Type.Name)));
				}
				if (!Global.Directives.DefaultAsset.IsEmpty())
				{
					Diagnostics.Warning(
						TEXT("DSH7262"),
						CurrentFile,
						Decl.Doc.Span,
						FText::Format(
							LOCTEXT("InstanceDefaultOnValue", "'@default' has no effect on '{0}', a {1} override whose value is its initializer; remove it."),
							FText::FromString(Declarator.Name),
							FText::FromString(IR::LexToString(Kind))));
				}
			}

			if (bHasPage && Kind != IR::EIRParameterKind::Font)
			{
				Diagnostics.Error(
					TEXT("DSH7266"),
					CurrentFile,
					Decl.Doc.Span,
					FText::Format(
						LOCTEXT("InstancePageNotFont", "'@page' picks the page of a Font override, and '{0}' is a {1} override."),
						FText::FromString(Declarator.Name),
						FText::FromString(IR::LexToString(Kind))));
			}
		}

		const int32 GlobalIndex = Bound.Globals.Num();
		const FString ParameterName = Global.Directives.Name.IsEmpty() ? Global.Name : Global.Directives.Name;
		Bound.Globals.Add(MoveTemp(Global));
		GlobalArrayValues.AddDefaulted();

		if (bHasKind)
		{
			FBoundInstanceOverride Override;
			Override.GlobalIndex = GlobalIndex;
			Override.ParameterName = ParameterName;
			Override.Kind = Kind;
			Override.FontPage = FontPage;
			Bound.Instance.Overrides.Add(MoveTemp(Override));
		}
	}

	void FLangBinder::BindInstanceInitializers()
	{
		using namespace LangBinderInstancePrivate;

		// The state BindGlobals would set: no function, no scope, file scope.
		CurrentFunctionIndex = INDEX_NONE;
		CurrentFunction = nullptr;
		Scopes.Reset();
		LocalArrayValues.Reset();
		LocalWrites.Reset();
		ParamWrites.Reset();
		CurrentRegion = INDEX_NONE;
		OuterRegion = INDEX_NONE;
		CurrentFile = RootModule.FilePath;

		for (const FBoundInstanceOverride& Override : Bound.Instance.Overrides)
		{
			if (!Bound.Globals.IsValidIndex(Override.GlobalIndex) || IsAssetParameterKind(Override.Kind))
			{
				// An asset override with an initializer already said DSH7257; binding the text would say more.
				continue;
			}
			const FBoundGlobal& Global = Bound.Globals[Override.GlobalIndex];
			const FExpr* Initializer = Global.Decl ? Global.Decl->Declarator.Initializer.Get() : nullptr;
			if (!Initializer)
			{
				continue;
			}

			const IR::FIRType DeclaredType = Global.Type;
			const FString Name = Global.Name;
			BindExpr(*Initializer, &DeclaredType);
			Convert(
				*Initializer,
				DeclaredType,
				EConversionSite::Assignment,
				FText::Format(
					LOCTEXT("InstanceInitializer", "The value of the override '{0}'"),
					FText::FromString(Name)));
		}
	}

	void FLangBinder::CheckInstanceOverrides()
	{
		using namespace LangBinderInstancePrivate;

		for (const FBoundInstanceOverride& Override : Bound.Instance.Overrides)
		{
			if (!Bound.Globals.IsValidIndex(Override.GlobalIndex) || IsAssetParameterKind(Override.Kind))
			{
				continue;
			}
			const FBoundGlobal& Global = Bound.Globals[Override.GlobalIndex];
			const FExpr* Initializer = Global.Decl ? Global.Decl->Declarator.Initializer.Get() : nullptr;
			if (Initializer && !TypeOf(*Initializer).IsError() && !IsConstantExpr(*Initializer))
			{
				Diagnostics.Error(
					TEXT("DSH7265"),
					CurrentFile,
					Initializer->Span,
					FText::Format(
						LOCTEXT("InstanceValueNotConstant", "'{0}' is set to a value the compiler can fold, a literal or an expression over literals, and this initializer is not one."),
						FText::FromString(Global.Name)));
			}
		}

		const IR::FIRParameterSchema* Schema = Options.ParentSchema;
		if (!Schema || !Schema->bValid)
		{
			Diagnostics.Info(
				TEXT("DSH7263"),
				CurrentFile,
				Bound.Instance.Pragma ? Bound.Instance.Pragma->Span : FLangSpan(),
				LOCTEXT("InstanceNoSchema", "The parameters of the parent are not available here, so the names and types of these overrides are checked only for their shape; compile the parent first, or check the file in the editor."));
			return;
		}

		const FString ParentLabel = !Schema->ParentObjectPath.IsEmpty() ? Schema->ParentObjectPath : Bound.Instance.ParentReference;
		TArray<int32> OverrideOfEntry;
		OverrideOfEntry.Init(INDEX_NONE, Schema->Parameters.Num());

		for (int32 Index = 0; Index < Bound.Instance.Overrides.Num(); ++Index)
		{
			FBoundInstanceOverride& Override = Bound.Instance.Overrides[Index];
			if (!Bound.Globals.IsValidIndex(Override.GlobalIndex))
			{
				continue;
			}
			const FBoundGlobal& Global = Bound.Globals[Override.GlobalIndex];
			const FLangSpan Span = Global.Decl ? Global.Decl->Declarator.NameSpan : FLangSpan();

			const int32 EntryIndex = Schema->Find(Override.ParameterName, IR::EIRParameterAssociation::Global, INDEX_NONE);
			if (EntryIndex == INDEX_NONE)
			{
				bool bLayerOnly = false;
				for (const IR::FIRParameterSchemaEntry& Entry : Schema->Parameters)
				{
					if (Entry.Association != IR::EIRParameterAssociation::Global
						&& Entry.Name.Equals(Override.ParameterName, ESearchCase::CaseSensitive))
					{
						bLayerOnly = true;
						break;
					}
				}
				if (bLayerOnly)
				{
					Diagnostics.Error(
						TEXT("DSH7268"),
						CurrentFile,
						Span,
						FText::Format(
							LOCTEXT("InstanceLayerOnlyParameter", "'{0}' exists in the parent only as a layer or blend parameter, and a '.dsi' overrides global parameters only."),
							FText::FromString(Override.ParameterName)));
					continue;
				}

				const int32 CaseIndex = Schema->FindIgnoreCase(Override.ParameterName);
				if (CaseIndex != INDEX_NONE
					&& Schema->Parameters[CaseIndex].Association == IR::EIRParameterAssociation::Global
					&& !Schema->Parameters[CaseIndex].bPruned)
				{
					// The engine compares parameter names ignoring case, so `intensity` would quietly work there;
					// the language does not (risk 2).
					Diagnostics.Error(
						TEXT("DSH7264"),
						CurrentFile,
						Span,
						FText::Format(
							LOCTEXT("InstanceCaseOnlyName", "'{0}' matches the parent parameter '{1}' only in case, and DreamShader names are case-sensitive; write '{1}'."),
							FText::FromString(Override.ParameterName),
							FText::FromString(Schema->Parameters[CaseIndex].Name)));
					continue;
				}

				Diagnostics.Error(
					TEXT("DSH7258"),
					CurrentFile,
					Span,
					FText::Format(
						LOCTEXT("InstanceUnknownParameter", "'{0}' is not a parameter of the parent '{1}'."),
						FText::FromString(Override.ParameterName),
						FText::FromString(ParentLabel)));
				continue;
			}

			const IR::FIRParameterSchemaEntry& Entry = Schema->Parameters[EntryIndex];
			if (Entry.bPruned)
			{
				Diagnostics.Error(
					TEXT("DSH7258"),
					CurrentFile,
					Span,
					FText::Format(
						LOCTEXT("InstancePrunedParameter", "'{0}' is declared by the parent source but nothing there reads it, so the parent material has no such parameter (DSH4390); read it in the parent, or remove this override."),
						FText::FromString(Override.ParameterName)));
				continue;
			}

			if (OverrideOfEntry[EntryIndex] != INDEX_NONE)
			{
				const int32 FirstGlobal = Bound.Instance.Overrides[OverrideOfEntry[EntryIndex]].GlobalIndex;
				const int32 FirstLine = (Bound.Globals.IsValidIndex(FirstGlobal) && Bound.Globals[FirstGlobal].Decl)
					? Bound.Globals[FirstGlobal].Decl->Declarator.NameSpan.Line
					: 0;
				Diagnostics.Error(
					TEXT("DSH7261"),
					CurrentFile,
					Span,
					FText::Format(
						LOCTEXT("InstanceDuplicateOverride", "'{0}' is overridden a second time, and one instance sets a parameter once; the override on line {1} already sets it."),
						FText::FromString(Override.ParameterName),
						FText::AsNumber(FirstLine)));
				continue;
			}
			OverrideOfEntry[EntryIndex] = Index;

			const bool bEntryStatic = IsStaticParameterKind(Entry.Kind);
			if (bEntryStatic != Global.Directives.bStatic)
			{
				if (bEntryStatic)
				{
					Diagnostics.Error(
						TEXT("DSH7260"),
						CurrentFile,
						Span,
						FText::Format(
							LOCTEXT("InstanceStaticMissing", "'{0}' is a static {1} parameter in the parent, so the override needs '/// @static', and it has none."),
							FText::FromString(Override.ParameterName),
							FText::FromString(IR::LexToString(Entry.Kind))));
				}
				else
				{
					Diagnostics.Error(
						TEXT("DSH7260"),
						CurrentFile,
						Span,
						FText::Format(
							LOCTEXT("InstanceStaticExtra", "'{0}' is a {1} parameter the parent sets at run time, and '/// @static' on this override asks for a static one; remove '@static'."),
							FText::FromString(Override.ParameterName),
							FText::FromString(IR::LexToString(Entry.Kind))));
				}
				continue;
			}

			if (!IsInstanceOverrideCompatible(Global.Type, Override.Kind, Entry))
			{
				const FString Written = Global.Decl ? Global.Decl->Type.Name : Global.Type.ToString();
				Diagnostics.Error(
					TEXT("DSH7259"),
					CurrentFile,
					Global.Decl ? Global.Decl->Type.Span : Span,
					FText::Format(
						LOCTEXT("InstanceTypeMismatch", "'{0}' is a {1} parameter of type '{2}' in the parent, and this override declares '{3}'."),
						FText::FromString(Override.ParameterName),
						FText::FromString(IR::LexToString(Entry.Kind)),
						FText::FromString(DescribeSchemaEntryType(Entry)),
						FText::FromString(Written)));
				continue;
			}

			// A float4 written for a DoubleVector is that DoubleVector.
			if (Entry.Kind == IR::EIRParameterKind::DoubleVector)
			{
				Override.Kind = IR::EIRParameterKind::DoubleVector;
			}
			Override.SchemaIndex = EntryIndex;
		}
	}

	void FLangBinder::BuildInstanceProduct()
	{
		FBoundProduct Product;
		Product.Kind = IR::EIRProductKind::MaterialInstance;
		Product.FunctionIndex = INDEX_NONE;
		// An instance has no backend of its own (DSH4335 otherwise): it is a plain MaterialInstanceConstant.
		Product.Backend = IR::EIRBackend::Graph;
		Product.AssetName = FPaths::GetBaseFilename(RootModule.FilePath);

		// The `.dss` rule (LangBinder.cpp BuildProducts): a full path overrides the destination, a bare name
		// replaces the leaf and lets the root rules place it.
		if (!InstanceAssetName.IsEmpty())
		{
			if (InstanceAssetName.StartsWith(TEXT("/"), ESearchCase::CaseSensitive))
			{
				Product.AssetPathOverride = InstanceAssetName;
				FString Leaf = InstanceAssetName;
				int32 Slash = INDEX_NONE;
				if (Leaf.FindLastChar(TEXT('/'), Slash) && Slash + 1 < Leaf.Len())
				{
					Leaf = Leaf.RightChop(Slash + 1);
				}
				if (!Leaf.IsEmpty())
				{
					Product.AssetName = Leaf;
				}
			}
			else
			{
				Product.AssetName = InstanceAssetName;
			}
		}

		Bound.Products.Add(MoveTemp(Product));
	}
}

#undef LOCTEXT_NAMESPACE
