// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Model/DreamShaderBrowserEntry.h"

namespace UE::DreamShader::Editor::Private
{
	// Creating a new .dss / .dsm / .dsf / .dsh / .dsi / .dsp from the plugin's templates (Resources/Templates). The file
	// lands in a writable source root; the bridge's watcher then lists and compiles it like any
	// other save. Split so the dialog's logic is testable without Slate.

	/**
	 * The language a new Material or Function is written in: the 1.x blocks (`.dsm` / `.dsf`) or DreamShaderLang 2.0
	 * (`.dss`). A header, an instance and a pipeline have their own templates, whatever this says.
	 */
	enum class ENewSourceLanguage : uint8
	{
		Legacy,
		Lang2,
	};

	/**
	 * Which of the three Custom Pass templates a new Pipeline starts from: the chains of the design's appendix A
	 * (DreamShader_Plan), cut down so each builds as soon as it is written. A pipeline names the materials and the shader its passes draw, so the
	 * template writes those too (FNewSourceCompanion), named after the pipeline's stem without its `CP_`.
	 */
	enum class ENewPipelineTemplate : uint8
	{
		/** A fullscreen post-process chain: a `copy` grabs the scene at half size, a fullscreen material pass blends it back; with PP_<Base>.dss. */
		PostProcess,
		/** A mesh mask chain: a mesh pass draws a list's objects into a mask, a fullscreen pass outlines them through walls; with M_<Base>Mask.dss and PP_<Base>Composite.dss. */
		MeshMask,
		/** A compute chain: a compute shader advances an exported field kept from frame to frame; with <Base>.usf. */
		Compute,
	};

	struct FNewSourceRequest
	{
		EBrowserSourceKind Kind = EBrowserSourceKind::Material;
		ENewSourceLanguage Language = ENewSourceLanguage::Legacy;
		FString Directory; // absolute; must be under a writable source root
		FString FileStem;  // without extension
		// Instance only: the `#pragma instance` Parent -- an asset path (`/Game/Materials/M_Base`) or the
		// name of a product under the same source root. Quotes in it are dropped.
		FString ParentReference;
		// Pipeline only: which chain the `.dsp` starts as.
		ENewPipelineTemplate PipelineTemplate = ENewPipelineTemplate::PostProcess;
	};

	/** One more file a template writes beside the source it creates: a pipeline's materials and its compute shader. */
	struct FNewSourceCompanion
	{
		FString FilePath; // absolute, normalized
		FString Text;
	};

	// The template text with {NAME}, {STEM}, {FILENAME}, {ASSETPATH} and {PARENT} filled in. {NAME} is a 1.x block's
	// Name= -- the directory's path relative to its root plus the stem, which is what makes the
	// asset land next to its neighbours' in /Game. {STEM} is the file's stem alone: a `.dss` names its asset by
	// its `export`, and the folder comes from where the file is. {PARENT} is ParentReference (the Instance template
	// has no Name=; its asset path follows the file). A pipeline's templates add {BASE} (the stem without `CP_`, which
	// names the companions), {PIPELINE} (the `.dsp`'s file name) and {SHADERPATH} (the compute shader's path, relative to
	// the `.dsp`: it is written next to it). Fails when the template is missing.
	bool RenderNewSourceTemplate(const FNewSourceRequest& Request, FString& OutText, FString& OutError);

	// The companion files a request writes besides its source, rendered with the same placeholders, each with its own
	// {STEM}, {FILENAME} and {ASSETPATH}. Empty for every kind but Pipeline. Fails when a template is missing.
	bool RenderNewSourceCompanions(const FNewSourceRequest& Request, TArray<FNewSourceCompanion>& OutCompanions, FString& OutError);

	// Writes the rendered template, and its companions first. Refuses an existing file (companions included), a directory
	// outside every writable root, a stem that is not a valid identifier, and an Instance without a parent. OutFilePath is
	// the absolute, normalized source that was asked for.
	bool CreateNewSourceFile(const FNewSourceRequest& Request, FString& OutFilePath, FString& OutError);

	// The modal dialog. OnCreated receives the absolute path of the file that was written. An Instance
	// gets a Parent row, prefilled with DefaultParent; a Pipeline is written from PipelineTemplate.
	void OpenNewSourceDialog(
		EBrowserSourceKind Kind,
		const FString& DefaultDirectory,
		TFunction<void(const FString&)> OnCreated,
		const FString& DefaultParent = FString(),
		ENewSourceLanguage Language = ENewSourceLanguage::Legacy,
		ENewPipelineTemplate PipelineTemplate = ENewPipelineTemplate::PostProcess);

	/** The extension, without its dot, of the file a request of this kind and language writes. */
	const TCHAR* GetSourceKindExtension(EBrowserSourceKind Kind, ENewSourceLanguage Language = ENewSourceLanguage::Legacy);
}
