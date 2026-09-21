// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// What the legacy (1.x) front end knows about a source that the 2.0 AST does not say.
//
// The legacy front end lowers `.dsm` / `.dsf` sources, and the `Function` / `GraphFunction` /
// `Namespace` / `VirtualFunction` blocks of a `.dsh`, onto the ordinary node kinds of LangAst.h.
// Everything the tree cannot carry -- the 1.x block words with their `Name=` / `Root=`, the raw
// `Settings.Backend`, output names in 1.x order, unresolved `Path(...)` references, parameter-node
// declarations expanded at their uses, output-selecting calls, the `Namespace` flatten map, body
// renames, synthesized initializers and directives -- is recorded here for `dsc migrate` and for the
// diagnostics that point back at 1.x spellings. It comes back on FLangParseResult::Legacy, and every
// pointer in it refers into that result's Module. GraphFunction hoists live on the tree itself
// (FFunctionDecl::HoistedCalls).
//
// Core-only, like the rest of the module. Design: Plan/m4m5/research-legacy.md sections 3 and 4 (H8).

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangAst.h"
#include "Lang/LangSource.h"

namespace UE::DreamShader::Lang
{
	/** One 1.x block and the declaration it became. */
	struct FLegacyBlock
	{
		/** Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend, Function, GraphFunction, VirtualFunction. */
		FString BlockWord;
		const FDecl* Decl = nullptr;
		FString Name;
		FString Root;
		bool bHasRoot = false;
		FLangSpan HeaderSpan;
		/** Editor-side only: the object path Name/Root resolve to (the front end cannot see plugin mounts). */
		FString ResolvedObjectPath;
		bool bHasBackend = false;
		/** Settings.Backend exactly as written. */
		FString BackendRaw;
		bool bHasUserExposedCaption = false;
		FString UserExposedCaption;
		/** 1.x declaration order. */
		TArray<FString> InputNames;
		/** 1.x declaration order. */
		TArray<FString> OutputNames;
	};

	/** One section of a 1.x block: where its header and its body sat, and the first node it produced. */
	struct FLegacySection
	{
		const FDecl* Block = nullptr;
		FString Name;
		FLangSpan HeaderSpan;
		FLangSpan BodySpan;
		const FNode* FirstNode = nullptr;
	};

	/** A 1.x parameter-node declaration expanded at its uses (StaticSwitchParameter, ChannelMaskParameter, TextureSampleParameter2D, UE.* builtin properties). */
	struct FLegacyParameterDeclaration
	{
		const FDecl* Block = nullptr;
		FString Name;
		/** "StaticSwitchParameter", "ChannelMaskParameter", "UE.CollectionParam", ... */
		FString NodeType;
		/** As written; empty when none. */
		FString DefaultText;
		/** 1.x keys and values as written, in order. */
		TArray<TPair<FString, FString>> Metadata;
		FLangSpan DeclarationSpan;
		/** Every call or read it was expanded into. */
		TArray<FLangSpan> UseSpans;
		/** The synthesized expressions, in the same order as UseSpans. */
		TArray<const FExpr*> Expansions;
	};

	/** A 1.x asset reference the front end carried unresolved (`Path(...)`, a quoted object path); the emitter resolves it. */
	struct FLegacyAssetReference
	{
		enum class EUse : uint8
		{
			VirtualFunctionAsset,
			TextureDefault,
			PropertyArgument,
			CollectionParameter,
		};

		EUse Use = EUse::PropertyArgument;
		FString Text;
		FLangSpan Span;
		const FNode* Node = nullptr;
		/**
		 * TextureDefault: the declaration's type token names no dimension (`TextureObjectParameter`), so the asset is the
		 * only thing that knows which it is (1.x ResolveEffectiveTextureType). The front end cannot load one and reads the
		 * declaration as Texture2D; a host that can retypes it before anything is bound.
		 */
		bool bTypeFromAsset = false;
	};

	/** `F(args, Output = "N")` / `OutputIndex = k`, and `BreakOutFloatN(...)`. */
	struct FLegacyOutputSelection
	{
		/** The FMemberExpr / FIndexExpr the call became (or the swizzle for BreakOutFloatN). */
		const FExpr* Selection = nullptr;
		const FCallExpr* Call = nullptr;
		FString OutputName;
		int32 OutputIndex = INDEX_NONE;
		FLangSpan Span;
		/** Equal callee and argument text share a group (1.x shared one node). */
		int32 Group = INDEX_NONE;
	};

	/** One spelling the legacy front end changed. */
	struct FLegacyRename
	{
		enum class EKind : uint8
		{
			GlslAlias,
			NamespaceQualifier,
			TypeSpelling,
		};

		EKind Kind = EKind::GlslAlias;
		FString From;
		FString To;
		FLangSpan Span;
		const FDecl* Decl = nullptr;
	};

	/** A typed zero initializer written for a 1.x declaration that had none. */
	struct FLegacySynthesizedInitializer
	{
		const FNode* Declaration = nullptr;
		FString Name;
		FLangSpan Span;
	};

	/** A `///` directive synthesized from 1.x state: `@sort 32`, `@root`, `@library` from Settings, ... */
	struct FLegacySynthesizedDirective
	{
		const FDecl* Decl = nullptr;
		FString Key;
		FString Value;
	};

	struct FLegacyMigrationInfo
	{
		TArray<FLegacyBlock> Blocks;
		TArray<FLegacySection> Sections;
		TArray<FLegacyParameterDeclaration> ParameterDeclarations;
		TArray<FLegacyAssetReference> AssetReferences;
		TArray<FLegacyOutputSelection> OutputSelections;
		/** "N::F" -> "N_F". A TArray, not a TMap: FString keys compare case-insensitively (CONTRACT 0.10). */
		TArray<TPair<FString, FString>> NamespaceFlatten;
		/** Function / GraphFunction bodies and type spellings. */
		TArray<FLegacyRename> BodyRenames;
		TArray<FLegacySynthesizedInitializer> SynthesizedInitializers;
		TArray<FLegacySynthesizedDirective> SynthesizedDirectives;
		// GraphFunction hoists: FFunctionDecl::HoistedCalls on the Blocks' declarations.
	};
}
