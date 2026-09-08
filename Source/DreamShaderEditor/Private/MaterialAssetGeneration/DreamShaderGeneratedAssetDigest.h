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
		// Ours, the generated content still matches -- and the ThinCustom instance carries parameter
		// overrides on top of it. Tuning a generated instance is what the instance is FOR, so this is
		// not divergence and it does not block a rebuild; the rebuild captures the overrides and puts
		// back every name the new base still declares. See DreamShaderThinCustomParameterOverrides.h.
		Tweaked,
		// Ours, and the contents no longer match the stamp: somebody edited the asset by hand.
		Diverged
	};

	// The schema tag a digest is stamped with: the format version, the engine version, and a
	// fingerprint of the reflected layout of every expression class the asset uses. A digest is only
	// ever compared against another digest carrying the same tag, so the format, the engine's own
	// property set, and a source-built engine's changes to an expression class can all move without
	// turning every previously stamped asset into a false divergence report.
	FString MakeDigestSchemaTag(UObject* Asset);

	// The two halves of MakeDigestSchemaTag, for the stamp/check asymmetry: the classes whose layout
	// the tag fingerprints are recorded next to the stamp (DreamShader.OutputDigestClasses) so that a
	// later check fingerprints the classes the stamp was MADE from, not the classes the asset holds
	// now -- a node added by hand must read as Diverged, not as a schema change.
	TArray<FString> CollectDigestClassPathNames(UObject* Asset);
	FString MakeDigestSchemaTagForClasses(const TArray<FString>& ClassPathNames);

	// Deterministic text form of the asset's generated content. Exposed (rather than only the hash)
	// so a test can diff two of them and say WHAT diverged, and so the log can carry the difference
	// when verbose logging is on. Empty for an asset class the digest does not cover.
	FString BuildOutputDigestText(UObject* Asset);
	FString BuildMaterialDigestText(UMaterial* Material);
	FString BuildMaterialFunctionDigestText(UMaterialFunction* MaterialFunction);
	FString BuildMaterialInstanceDigestText(UMaterialInstance* Instance);

	// "<schema>:<crc32>" -- what gets stamped into DreamShader.OutputDigest. Empty when the asset
	// class is not covered, which callers must treat as "cannot judge" rather than "diverged".
	// The one-argument form tags with the asset's current classes (stamp time); the two-argument
	// form takes the tag the check recovered from the stamp.
	FString BuildOutputDigest(UObject* Asset);
	FString BuildOutputDigest(UObject* Asset, const FString& SchemaTag);

	// Whether a generated ThinCustom instance carries any parameter override at all -- the difference
	// between Generated and Tweaked. False for anything that is not a material instance, and false for
	// an instance whose overrides ARE part of its digest (a shape the ThinCustom backend does not
	// produce), so the answer only ever refines a state the digest already called Generated.
	bool GeneratedInstanceHasParameterOverrides(UObject* Asset);

	// Whether this instance is the ThinCustom pair: a material instance whose parent is the hidden
	// base UMaterial generated alongside it -- a subobject in its own package when saved, an object in
	// the transient package when memory-only. It is the shape whose parameter overrides are the user's
	// tuning rather than a hand edit.
	bool IsThinCustomInstancePair(const UMaterialInstance* Instance);

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
