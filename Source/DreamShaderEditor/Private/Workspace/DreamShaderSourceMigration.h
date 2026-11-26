#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor::Private
{
	/**
	 * The deprecation notice for a project still keeping its sources in `DShader/`, and the button that moves them to
	 * `DreamSource/Shader/` (or wherever UDreamShaderSourceSettings points).
	 *
	 * `DShader/` keeps working for good -- this only tells the author it is deprecated and that Dream sources are managed
	 * together under DreamSource/. Logged once per session in every process (LogDeprecatedSourceDirectoriesOnce); the
	 * notification only in the editor that owns the bridge, and not again once the author said so
	 * (EditorPerProjectUserSettings `[DreamSource] bHideShaderMigrationNotice`, this machine only).
	 * Plan/DreamSource/dreamsource-layout-spec.md section 12.5.
	 */
	struct FDreamShaderSourceMigration
	{
		/** Logs the deprecations after engine init; with bInShowNotice, also offers the migration. */
		static void Register(bool bInShowNotice);
		static void Unregister();

		/**
		 * Moves the legacy `DShader/` to the configured directory, rewrites the DreamShader.code-workspace in it so its
		 * relative folders still point where they did, and deletes the generated data the DreamSource layout replaced
		 * (`Saved/DreamShader/Bridge`, `Saved/DreamShader/Index`, `Intermediate/DreamShader`). Refuses when the configured
		 * directory already exists. The running editor keeps reading the old place -- the Custom Pass shader mapping and
		 * the watchers were made at startup -- so the caller asks for a restart. False with OutError on failure; whatever
		 * could not be moved is left where it was.
		 */
		static bool Migrate(FString& OutError);

		/**
		 * The workspace text with every relative folder path that was written against InOldDirectory rewritten against
		 * InNewDirectory. Paths are made absolute against the old directory and relative again against the new one. False
		 * when the text is not a workspace file.
		 */
		static bool RebaseWorkspaceFolders(const FString& InText, const FString& InOldDirectory, const FString& InNewDirectory, FString& OutText);
	};
}
