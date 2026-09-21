// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// `dsc migrate`: a 1.x source (`.dsm`, `.dsf`, or the 1.x declarations of a `.dsh`) rewritten as 2.0 text.
//
// The legacy front end already reads a 1.x file into the 2.0 tree, comments included. Migration is that tree
// rewritten until the 2.0 front end reads it the same way (Lang: Migrate/LangMigrate.h), printed, and then proven:
//
//   - every comment of the source is in the result (DSH9092), or nothing is written;
//   - the result parses, binds, lowers and validates as 2.0 source (DSH9097), or nothing is written;
//   - what it lowers to is compared with what the 1.x file lowered to, node order, variable names and source
//     positions aside (IR/IRCompare.h), and every product is checked to land where it did. A difference is DSH9098,
//     a warning: the file is still written, because the usual cause is a 1.x rule the front end reports on its own.
//
// A `.dsm` / `.dsf` becomes `<stem>.dss` beside it and the 1.x file moves to Saved/DreamShader/Migrated/Sources/<root>/,
// mirroring its place under its source root (or is deleted, bNoBackup): two files declaring one asset cannot both stay. A `.dsh`
// is rewritten in place, same name, same rule for the old text. With OutputDirectory the result is written there
// instead and the source is left where it is; BackupFilePath then names the untouched source.
//
// Refused: a source with `#if` conditionals (DSH9090: only the branch taken today would survive), an `import` that
// names a source root (DSH9091), a `.dsh` with nothing 1.x in it (DSH9093), a file that is not a 1.x source or cannot
// be read (DSH9095), an output that already exists or cannot be written (DSH9099).

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangDiagnostic.h"

namespace UE::DreamShader::Editor::Private
{
	struct FDreamShaderMigrateOptions
	{
		/** Verify only: rewrite, re-parse and compare the IR; write nothing. */
		bool bCheck = false;
		/** Produce the text and report what would be written; write nothing. */
		bool bDryRun = false;
		/** Write the `.dss` under this directory, mirroring the source-root-relative path, instead of beside the source. Empty: beside it. */
		FString OutputDirectory;
		/** Delete the 1.x source instead of moving it to Saved/DreamShader/Migrated/. */
		bool bNoBackup = false;
	};

	struct FDreamShaderMigrateResult
	{
		bool bSucceeded = false;
		/** Absolute, normalized. */
		FString SourceFilePath;
		/** The `.dss` written -- or, with bCheck / bDryRun, the one that would be. */
		FString OutputFilePath;
		/** Where the 1.x source went; empty when it was not moved. */
		FString BackupFilePath;
		/** The printed `.dss` text. */
		FString MigratedText;
		/** Every diagnostic of the migration: DSH9090-9099 and the front end's. */
		TArray<UE::DreamShader::Lang::FLangDiagnostic> Diagnostics;
		/** The first error in the located wire form when bSucceeded is false. */
		FString Error;
	};

	/** Migrates exactly SourceFilePath (a `.dsm`, `.dsf` or `.dsh`); no header-set expansion. Game thread. */
	bool MigrateDreamShaderSource(const FString& SourceFilePath, const FDreamShaderMigrateOptions& Options, FDreamShaderMigrateResult& OutResult);

	/**
	 * `migrate { -Source=<file> | -All } [-Check] [-DryRun] [-Out=<dir>] [-NoBackup] [-Root=<name>]`; false when any file failed.
	 *
	 * `-Source` takes the headers the file includes along when nothing else includes them; a header other sources still
	 * include stays as it is, which a `.dss` reads just as well. `-All` is every 1.x file of the writable source roots,
	 * `Packages` aside, headers first; `-Root=<name>` names one root instead, a plugin's included.
	 */
	bool RunDreamShaderMigrateCommandlet(const TArray<FString>& Tokens, const TArray<FString>& Switches, const TMap<FString, FString>& Params);
}
