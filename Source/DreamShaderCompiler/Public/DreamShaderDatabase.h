#pragma once

#include "CoreMinimal.h"
#include "SQLiteDatabase.h"

/**
 * `Intermediate/DreamSource/shader.db`: what this editor tells tools that are not this editor -- the manifests a
 * language service binds against, the diagnostics of the last compiles, and the symbol index of every source. It
 * replaces `Saved/DreamShader/Bridge/bridge.db`, the JSON manifests beside it and `Saved/DreamShader/Index/`.
 *
 * The schema is a contract with dreamshader-language-support (Plan/DreamSource/dreamsource-layout-spec.md, section 12.3):
 *
 *   meta(key, value)              schema, producer, revision, <part>_revision, <part>_updated_utc
 *   settings_mappings(...)        material enum aliases, one row each          part settings_mappings
 *   material_expressions(...)     reflected UMaterialExpression classes        part material_expressions
 *   substrate_builtins(...)       Substrate builtins                           part substrate_builtins
 *   diagnostics(path, json, updated_at_utc)    one source file's records       part diagnostics
 *   documents(name, json, updated_utc)         a manifest that used to be a whole JSON file, verbatim   part documents
 *   source_index(root, path, json, updated_utc)   one source's symbol index   part index
 *
 * Opened for one write and closed again, never held: closing the last connection folds the WAL back into the file. A
 * schema this build does not know is dropped and recreated in place, never deleted -- on Windows a reader holding the
 * file open makes a delete fail, and everything in here is rebuilt by the editor anyway.
 */
class DREAMSHADERCOMPILER_API FDreamShaderDatabase
{
public:
	/** The schema this build writes. Bumped whenever a table changes shape; readers compare against meta.schema. */
	static constexpr int32 SchemaVersion = 1;

	/** The `documents` names. */
	static const TCHAR* const DocumentMaterialExpressions;
	static const TCHAR* const DocumentSettings;
	static const TCHAR* const DocumentSubstrateBuiltins;
	static const TCHAR* const DocumentPassKeys;
	static const TCHAR* const DocumentPreprocessorDefines;
	static const TCHAR* const DocumentBuiltinCatalog;
	static const TCHAR* const DocumentPreview;

	FDreamShaderDatabase() = default;
	~FDreamShaderDatabase();

	FDreamShaderDatabase(const FDreamShaderDatabase&) = delete;
	FDreamShaderDatabase& operator=(const FDreamShaderDatabase&) = delete;

	/** Opens (creating the directory and file), sets the pragmas, makes sure the schema is current. False, logged, on failure. */
	bool Open(const FString& InPath);
	void Close();

	FSQLiteDatabase& Get() { return Database; }

	bool Begin();
	bool Commit();
	void Rollback();

	/** Empty when the key is not there. */
	FString GetMeta(const TCHAR* InKey);
	bool SetMeta(const TCHAR* InKey, const FString& InValue);

	/** Adds one to `<InPart>_revision` and to `revision`, and stamps `<InPart>_updated_utc` with now. Inside the caller's transaction. */
	bool Bump(const TCHAR* InPart);

	/** Statement + error, for a log line. */
	FString DescribeError(const TCHAR* InWhat) const;

	/** Replaces one `documents` row, in its own transaction. False, logged, on failure. */
	static bool WriteDocument(const FString& InDatabasePath, const TCHAR* InName, const FString& InJson);

	/** Upserts one `source_index` row, in its own transaction. False, logged, on failure. */
	static bool WriteSourceIndex(const FString& InDatabasePath, const FString& InRoot, const FString& InPath, const FString& InJson);

	/** One `source_index` row's json; empty when there is none. */
	static FString ReadSourceIndex(const FString& InDatabasePath, const FString& InRoot, const FString& InPath);

	/** One `documents` row's json; empty when there is none. */
	static FString ReadDocument(const FString& InDatabasePath, const TCHAR* InName);

	/**
	 * What a new editor session starts without: the previous session's diagnostics and preview result. The rest stays --
	 * the manifests are rewritten at startup anyway, and the symbol index is per source and still true.
	 */
	static bool ResetSession(const FString& InDatabasePath);

private:
	bool EnsureSchema();

	FSQLiteDatabase Database;
	FString Path;
};
