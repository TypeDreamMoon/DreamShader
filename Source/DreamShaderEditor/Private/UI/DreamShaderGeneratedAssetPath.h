#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor::Private
{
	/**
	 * The generated asset a source file stands for, and the build key its products are stamped with, found without
	 * building anything. Product resolution (ResolveDreamShaderSourceProducts) answers it for every compilable kind
	 * (`.dss`, `.dsi`, `.dsp`, `.dsm`, `.dsf`) with the front end, binder, IR builder, destination rules and define set a
	 * compile uses, so the path is the one a build writes and the key the one it stamps.
	 *
	 * "The asset" is the material product when there is one, else the instance. Unless bMaterialOnly is set (the
	 * preview, which renders a material), a source with neither answers its pass pipeline (a `.dsp`), else its first
	 * product -- a function file's first export. Returns false with OutError, in the wire form of the first diagnostic,
	 * when the source does not get as far as its products or declares none that qualifies.
	 */
	bool ResolveGeneratedAssetProduct(
		const FString& SourceFilePath,
		bool bMaterialOnly,
		FString& OutObjectPath,
		FString& OutSourceHash,
		FText& OutError);
}
