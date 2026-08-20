// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Output digest: a fingerprint of what a generated asset actually CONTAINS, as opposed to
// DreamShader.SourceHash, which fingerprints the source it was generated FROM.
//
// The two answer different questions. The source hash answers "does this source still need
// compiling"; it says nothing about whether the asset it produced is still the asset we produced.
// Hand-edit a generated material and the source hash is still current -- which is exactly the state
// in which regeneration used to clear the graph and destroy the edit without a word.
//
// The digest closes that gap: it is computed at the end of every successful generation and stamped
// next to the source hash, so the next compile can compare "what the asset holds now" against "what
// we last wrote into it" and refuse to clear a graph somebody has been working in.

#pragma once

#include "CoreMinimal.h"

class UMaterial;
class UMaterialFunction;
class UMaterialInstance;
class UObject;
class UScriptStruct;
class FProperty;

namespace UE::DreamShader::Editor::Private
{
	// What a stamped digest says about the asset it was read from.
	enum class EDreamShaderDigestState : uint8
	{
		// No DreamShader.SourceFile metadata: the asset is not ours. The ownership guard, not the
		// divergence gate, is what speaks about these.
		Foreign,
		// Ours, and carrying no usable digest -- either generated before digests existed, or stamped
		// by a different digest schema (see MakeDigestSchemaTag). Regeneration proceeds and restamps;
		// treating an unreadable digest as divergence would flag every asset in the project the first
		// time the schema moves.
		Unstamped,
		// Ours, and the asset still holds exactly what we last generated into it.
		Generated,
		// Ours, and the contents no longer match the stamp: somebody edited the asset by hand.
		Diverged
	};

	// The schema tag a digest is stamped with. A digest is only ever compared against another digest
	// carrying the same tag, so both the format below and the engine's own property set can change
	// without turning every previously stamped asset into a false divergence report.
	FString MakeDigestSchemaTag();

	// Deterministic text form of the asset's generated content. Exposed (rather than only the hash)
	// so a test can diff two of them and say WHAT diverged, and so the log can carry the difference
	// when verbose logging is on. Empty for an asset class the digest does not cover.
	FString BuildOutputDigestText(UObject* Asset);
	FString BuildMaterialDigestText(UMaterial* Material);
	FString BuildMaterialFunctionDigestText(UMaterialFunction* MaterialFunction);
	FString BuildMaterialInstanceDigestText(UMaterialInstance* Instance);

	// "<schema>:<crc32>" -- what gets stamped into DreamShader.OutputDigest. Empty when the asset
	// class is not covered, which callers must treat as "cannot judge" rather than "diverged".
	FString BuildOutputDigest(UObject* Asset);

	// Whether a struct's contents can go into a digest verbatim. Rejects anything that reaches an
	// FExpressionInput (connections are digested structurally, by node index, so that moving or
	// renaming the package does not read as an edit) or an FGuid (pin ids and named-reroute variable
	// ids are deliberately carried across regenerations, so they are not content).
	bool IsDigestSafeStruct(const UScriptStruct* Struct);

	// Whether a property contributes to the digest at all. Excludes transient/deprecated/non-editable
	// state, expression inputs, and the purely cosmetic node properties a user is free to change --
	// node position, comment-bubble visibility, and the like -- which regeneration does not preserve
	// and which nobody means as an edit to the material.
	bool IsDigestProperty(const FProperty* Property);
}
