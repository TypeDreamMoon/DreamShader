#include "DreamShaderDatabase.h"

#include "DreamShaderModule.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "SQLitePreparedStatement.h"

const TCHAR* const FDreamShaderDatabase::DocumentMaterialExpressions = TEXT("material-expressions");
const TCHAR* const FDreamShaderDatabase::DocumentSettings = TEXT("settings");
const TCHAR* const FDreamShaderDatabase::DocumentSubstrateBuiltins = TEXT("substrate-builtins");
const TCHAR* const FDreamShaderDatabase::DocumentPassKeys = TEXT("pass-keys");
const TCHAR* const FDreamShaderDatabase::DocumentPreprocessorDefines = TEXT("preprocessor-defines");
const TCHAR* const FDreamShaderDatabase::DocumentBuiltinCatalog = TEXT("builtin-catalog");
const TCHAR* const FDreamShaderDatabase::DocumentPreview = TEXT("preview");

namespace DreamShaderDatabaseLocal
{
	FString Producer()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader"));
		return FString::Printf(TEXT("DreamShader@%s"), Plugin.IsValid() ? *Plugin->GetDescriptor().VersionName : TEXT("unknown"));
	}

	const TCHAR* const Tables[] =
	{
		TEXT("settings_mappings"),
		TEXT("material_expressions"),
		TEXT("substrate_builtins"),
		TEXT("diagnostics"),
		TEXT("documents"),
		TEXT("source_index"),
	};
}

FDreamShaderDatabase::~FDreamShaderDatabase()
{
	Close();
}

bool FDreamShaderDatabase::Open(const FString& InPath)
{
	Close();
	Path = InPath;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(InPath), /*Tree*/true);
	if (!Database.Open(*InPath, ESQLiteDatabaseOpenMode::ReadWriteCreate))
	{
		UE_LOG(LogDreamShader, Warning, TEXT("DreamShader could not open '%s': %s"), *InPath, *Database.GetLastError());
		return false;
	}
	// busy_timeout first: a language server may be mid-read, and every statement after this one should wait it out
	// rather than fail on SQLITE_BUSY.
	Database.Execute(TEXT("PRAGMA busy_timeout=5000;"));
	Database.Execute(TEXT("PRAGMA journal_mode=WAL;"));
	Database.Execute(TEXT("PRAGMA synchronous=NORMAL;"));
	if (!EnsureSchema())
	{
		UE_LOG(LogDreamShader, Warning, TEXT("DreamShader: %s"), *DescribeError(TEXT("creating the schema")));
		Close();
		return false;
	}
	return true;
}

void FDreamShaderDatabase::Close()
{
	if (Database.IsValid())
	{
		Database.Close();
	}
}

bool FDreamShaderDatabase::EnsureSchema()
{
	if (!Database.Execute(TEXT("CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT NOT NULL);")))
	{
		return false;
	}
	const FString Expected = FString::FromInt(SchemaVersion);
	const FString Existing = GetMeta(TEXT("schema"));
	if (!Existing.IsEmpty() && Existing != Expected)
	{
		UE_LOG(LogDreamShader, Display, TEXT("DreamShader: '%s' has schema %s; this build writes %d. Rebuilding it."),
			*Path, *Existing, SchemaVersion);
		for (const TCHAR* Table : DreamShaderDatabaseLocal::Tables)
		{
			Database.Execute(*FString::Printf(TEXT("DROP TABLE IF EXISTS %s;"), Table));
		}
		Database.Execute(TEXT("DELETE FROM meta;"));
	}

	// The first four are bridge.db's tables, unchanged: dreamshader-language-support already queries them.
	const bool bCreated =
		Database.Execute(TEXT("CREATE TABLE IF NOT EXISTS settings_mappings(kind TEXT NOT NULL, alias TEXT NOT NULL, normalized_alias TEXT NOT NULL, ")
			TEXT("value INTEGER NOT NULL, name TEXT, display_name TEXT, source TEXT NOT NULL, PRIMARY KEY(kind, normalized_alias));"))
		&& Database.Execute(TEXT("CREATE INDEX IF NOT EXISTS idx_settings_mappings_kind_alias ON settings_mappings(kind, alias);"))
		&& Database.Execute(TEXT("CREATE TABLE IF NOT EXISTS material_expressions(name TEXT PRIMARY KEY, class_name TEXT NOT NULL, ")
			TEXT("path_name TEXT NOT NULL, default_output_type TEXT NOT NULL, json TEXT NOT NULL);"))
		&& Database.Execute(TEXT("CREATE TABLE IF NOT EXISTS substrate_builtins(name TEXT PRIMARY KEY, qualified_name TEXT NOT NULL, ")
			TEXT("class_name TEXT NOT NULL, output_type TEXT NOT NULL, is_substrate_output INTEGER NOT NULL, detail TEXT, example TEXT, ")
			TEXT("snippet TEXT, json TEXT NOT NULL);"))
		&& Database.Execute(TEXT("CREATE TABLE IF NOT EXISTS diagnostics(path TEXT PRIMARY KEY, json TEXT NOT NULL, updated_at_utc TEXT NOT NULL);"))
		&& Database.Execute(TEXT("CREATE TABLE IF NOT EXISTS documents(name TEXT PRIMARY KEY, json TEXT NOT NULL, updated_utc TEXT NOT NULL);"))
		&& Database.Execute(TEXT("CREATE TABLE IF NOT EXISTS source_index(root TEXT NOT NULL, path TEXT NOT NULL, json TEXT NOT NULL, ")
			TEXT("updated_utc TEXT NOT NULL, PRIMARY KEY(root, path)) WITHOUT ROWID;"));
	if (!bCreated)
	{
		return false;
	}
	if (Existing != Expected)
	{
		SetMeta(TEXT("schema"), Expected);
	}
	const FString Producer = DreamShaderDatabaseLocal::Producer();
	if (GetMeta(TEXT("producer")) != Producer)
	{
		SetMeta(TEXT("producer"), Producer);
	}
	return true;
}

bool FDreamShaderDatabase::Begin()
{
	return Database.Execute(TEXT("BEGIN IMMEDIATE;"));
}

bool FDreamShaderDatabase::Commit()
{
	return Database.Execute(TEXT("COMMIT;"));
}

void FDreamShaderDatabase::Rollback()
{
	Database.Execute(TEXT("ROLLBACK;"));
}

FString FDreamShaderDatabase::GetMeta(const TCHAR* InKey)
{
	FString Value;
	FSQLitePreparedStatement Statement(Database, TEXT("SELECT value FROM meta WHERE key = ?1;"));
	if (Statement.IsValid() && Statement.SetBindingValueByIndex(1, FString(InKey))
		&& Statement.Step() == ESQLitePreparedStatementStepResult::Row)
	{
		Statement.GetColumnValueByIndex(0, Value);
	}
	return Value;
}

bool FDreamShaderDatabase::SetMeta(const TCHAR* InKey, const FString& InValue)
{
	FSQLitePreparedStatement Statement(Database, TEXT("INSERT OR REPLACE INTO meta(key, value) VALUES(?1, ?2);"));
	return Statement.IsValid()
		&& Statement.SetBindingValueByIndex(1, FString(InKey))
		&& Statement.SetBindingValueByIndex(2, InValue)
		&& Statement.Step() == ESQLitePreparedStatementStepResult::Done;
}

bool FDreamShaderDatabase::Bump(const TCHAR* InPart)
{
	const auto Next = [this](const FString& InName)
	{
		const int64 Current = FCString::Atoi64(*GetMeta(*InName));
		return SetMeta(*InName, LexToString(Current + 1));
	};
	return Next(FString::Printf(TEXT("%s_revision"), InPart))
		&& Next(TEXT("revision"))
		&& SetMeta(*FString::Printf(TEXT("%s_updated_utc"), InPart), FDateTime::UtcNow().ToIso8601());
}

FString FDreamShaderDatabase::DescribeError(const TCHAR* InWhat) const
{
	return FString::Printf(TEXT("'%s': %s failed: %s"), *Path, InWhat, *Database.GetLastError());
}

namespace DreamShaderDatabaseLocal
{
	/** Opens, runs InWrite inside one transaction, bumps InPart, commits; logs and rolls back on any failure. */
	template <typename FWrite>
	bool WriteInTransaction(const FString& InDatabasePath, const TCHAR* InPart, const TCHAR* InWhat, FWrite&& InWrite)
	{
		FDreamShaderDatabase Database;
		if (!Database.Open(InDatabasePath))
		{
			return false;
		}
		if (!Database.Begin())
		{
			UE_LOG(LogDreamShader, Warning, TEXT("DreamShader: %s"), *Database.DescribeError(TEXT("BEGIN")));
			return false;
		}
		if (!InWrite(Database) || !Database.Bump(InPart) || !Database.Commit())
		{
			UE_LOG(LogDreamShader, Warning, TEXT("DreamShader: %s; external tools now read stale data."), *Database.DescribeError(InWhat));
			Database.Rollback();
			return false;
		}
		return true;
	}

	FString ReadOne(const FString& InDatabasePath, const TCHAR* InSql, TFunctionRef<bool(FSQLitePreparedStatement&)> InBind)
	{
		if (!IFileManager::Get().FileExists(*InDatabasePath))
		{
			return FString();
		}
		FSQLiteDatabase Database;
		if (!Database.Open(*InDatabasePath, ESQLiteDatabaseOpenMode::ReadOnly))
		{
			return FString();
		}
		Database.Execute(TEXT("PRAGMA busy_timeout=5000;"));
		FString Value;
		{
			FSQLitePreparedStatement Statement(Database, InSql);
			if (Statement.IsValid() && InBind(Statement) && Statement.Step() == ESQLitePreparedStatementStepResult::Row)
			{
				Statement.GetColumnValueByIndex(0, Value);
			}
		}
		Database.Close();
		return Value;
	}
}

bool FDreamShaderDatabase::WriteDocument(const FString& InDatabasePath, const TCHAR* InName, const FString& InJson)
{
	return DreamShaderDatabaseLocal::WriteInTransaction(InDatabasePath, TEXT("documents"), TEXT("writing a document"),
		[InName, &InJson](FDreamShaderDatabase& Database)
		{
			FSQLitePreparedStatement Statement(Database.Get(),
				TEXT("INSERT OR REPLACE INTO documents(name, json, updated_utc) VALUES(?1, ?2, ?3);"));
			return Statement.IsValid()
				&& Statement.SetBindingValueByIndex(1, FString(InName))
				&& Statement.SetBindingValueByIndex(2, InJson)
				&& Statement.SetBindingValueByIndex(3, FDateTime::UtcNow().ToIso8601())
				&& Statement.Step() == ESQLitePreparedStatementStepResult::Done;
		});
}

bool FDreamShaderDatabase::WriteSourceIndex(const FString& InDatabasePath, const FString& InRoot, const FString& InPath, const FString& InJson)
{
	return DreamShaderDatabaseLocal::WriteInTransaction(InDatabasePath, TEXT("index"), TEXT("writing a symbol index"),
		[&InRoot, &InPath, &InJson](FDreamShaderDatabase& Database)
		{
			FSQLitePreparedStatement Statement(Database.Get(),
				TEXT("INSERT OR REPLACE INTO source_index(root, path, json, updated_utc) VALUES(?1, ?2, ?3, ?4);"));
			return Statement.IsValid()
				&& Statement.SetBindingValueByIndex(1, InRoot)
				&& Statement.SetBindingValueByIndex(2, InPath)
				&& Statement.SetBindingValueByIndex(3, InJson)
				&& Statement.SetBindingValueByIndex(4, FDateTime::UtcNow().ToIso8601())
				&& Statement.Step() == ESQLitePreparedStatementStepResult::Done;
		});
}

FString FDreamShaderDatabase::ReadSourceIndex(const FString& InDatabasePath, const FString& InRoot, const FString& InPath)
{
	return DreamShaderDatabaseLocal::ReadOne(InDatabasePath, TEXT("SELECT json FROM source_index WHERE root = ?1 AND path = ?2;"),
		[&InRoot, &InPath](FSQLitePreparedStatement& Statement)
		{
			return Statement.SetBindingValueByIndex(1, InRoot) && Statement.SetBindingValueByIndex(2, InPath);
		});
}

FString FDreamShaderDatabase::ReadDocument(const FString& InDatabasePath, const TCHAR* InName)
{
	return DreamShaderDatabaseLocal::ReadOne(InDatabasePath, TEXT("SELECT json FROM documents WHERE name = ?1;"),
		[InName](FSQLitePreparedStatement& Statement)
		{
			return Statement.SetBindingValueByIndex(1, FString(InName));
		});
}

bool FDreamShaderDatabase::ResetSession(const FString& InDatabasePath)
{
	FDreamShaderDatabase Database;
	if (!Database.Open(InDatabasePath) || !Database.Begin())
	{
		return false;
	}
	FSQLitePreparedStatement DeletePreview(Database.Get(), TEXT("DELETE FROM documents WHERE name = ?1;"));
	const bool bOk = Database.Get().Execute(TEXT("DELETE FROM diagnostics;"))
		&& DeletePreview.IsValid()
		&& DeletePreview.SetBindingValueByIndex(1, FString(DocumentPreview))
		&& DeletePreview.Step() == ESQLitePreparedStatementStepResult::Done
		&& Database.Bump(TEXT("diagnostics"))
		&& Database.Bump(TEXT("documents"));
	if (!bOk || !Database.Commit())
	{
		UE_LOG(LogDreamShader, Warning, TEXT("DreamShader: %s"), *Database.DescribeError(TEXT("resetting the session")));
		Database.Rollback();
		return false;
	}
	return true;
}
