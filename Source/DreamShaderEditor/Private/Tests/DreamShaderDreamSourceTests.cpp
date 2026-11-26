// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The DreamSource layout (Plan/DreamSource/dreamsource-layout-spec.md, section 12): shader.db, which replaced the json
// manifests, bridge.db and the symbol index folder, and the migration's rewrite of the workspace file. The database
// is a contract with dreamshader-language-support, so these pin what a reader relies on: the tables, the meta keys,
// and that a database of another schema is rebuilt in place rather than deleted.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderDatabase.h"
#include "DreamShaderModule.h"
#include "Workspace/DreamShaderSourceMigration.h"

#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "SQLiteDatabase.h"
#include "SQLitePreparedStatement.h"

namespace UE::DreamShader::Editor::Private::DreamSourceTests
{
	FString MakeScratchDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShaderTests"),
			TEXT("DreamSource_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	FString ReadMeta(const FString& InDatabasePath, const TCHAR* InKey)
	{
		FString Value;
		FSQLiteDatabase Database;
		if (Database.Open(*InDatabasePath, ESQLiteDatabaseOpenMode::ReadOnly))
		{
			{
				FSQLitePreparedStatement Statement(Database, TEXT("SELECT value FROM meta WHERE key = ?1;"));
				if (Statement.IsValid() && Statement.SetBindingValueByIndex(1, FString(InKey))
					&& Statement.Step() == ESQLitePreparedStatementStepResult::Row)
				{
					Statement.GetColumnValueByIndex(0, Value);
				}
			}
			Database.Close();
		}
		return Value;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDreamSourceDatabaseTest,
	"DreamShader.DreamSource.Database",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDreamSourceDatabaseTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::DreamSourceTests;

	const FString Directory = MakeScratchDirectory();
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Directory, false, true); };
	const FString DatabasePath = FPaths::Combine(Directory, TEXT("shader.db"));

	TestTrue(TEXT("a document is written"),
		FDreamShaderDatabase::WriteDocument(DatabasePath, FDreamShaderDatabase::DocumentPassKeys, TEXT("{\"version\":1}")));
	TestEqual(TEXT("and read back verbatim"),
		FDreamShaderDatabase::ReadDocument(DatabasePath, FDreamShaderDatabase::DocumentPassKeys), FString(TEXT("{\"version\":1}")));
	TestEqual(TEXT("the schema is stamped"), ReadMeta(DatabasePath, TEXT("schema")), FString::FromInt(FDreamShaderDatabase::SchemaVersion));
	TestTrue(TEXT("the producer names the plugin"), ReadMeta(DatabasePath, TEXT("producer")).StartsWith(TEXT("DreamShader@")));
	TestEqual(TEXT("a write bumps its part"), ReadMeta(DatabasePath, TEXT("documents_revision")), FString(TEXT("1")));
	TestEqual(TEXT("and the database"), ReadMeta(DatabasePath, TEXT("revision")), FString(TEXT("1")));
	TestFalse(TEXT("and stamps the part's time"), ReadMeta(DatabasePath, TEXT("documents_updated_utc")).IsEmpty());

	TestTrue(TEXT("an index row is written"),
		FDreamShaderDatabase::WriteSourceIndex(DatabasePath, TEXT("Project"), TEXT("Materials/M_A.dss"), TEXT("{\"symbols\":[]}")));
	TestEqual(TEXT("keyed by root and relative path"),
		FDreamShaderDatabase::ReadSourceIndex(DatabasePath, TEXT("Project"), TEXT("Materials/M_A.dss")), FString(TEXT("{\"symbols\":[]}")));
	TestTrue(TEXT("another root's same path is another row"),
		FDreamShaderDatabase::ReadSourceIndex(DatabasePath, TEXT("MoonToon"), TEXT("Materials/M_A.dss")).IsEmpty());
	TestEqual(TEXT("the index part counts on its own"), ReadMeta(DatabasePath, TEXT("index_revision")), FString(TEXT("1")));
	TestEqual(TEXT("while the database counts every commit"), ReadMeta(DatabasePath, TEXT("revision")), FString(TEXT("2")));

	FDreamShaderDatabase::WriteDocument(DatabasePath, FDreamShaderDatabase::DocumentPreview, TEXT("{\"status\":\"ready\"}"));
	TestTrue(TEXT("a new session starts clean"), FDreamShaderDatabase::ResetSession(DatabasePath));
	TestTrue(TEXT("without the last preview"), FDreamShaderDatabase::ReadDocument(DatabasePath, FDreamShaderDatabase::DocumentPreview).IsEmpty());
	TestFalse(TEXT("but with the manifests"), FDreamShaderDatabase::ReadDocument(DatabasePath, FDreamShaderDatabase::DocumentPassKeys).IsEmpty());
	TestFalse(TEXT("and the symbol index"), FDreamShaderDatabase::ReadSourceIndex(DatabasePath, TEXT("Project"), TEXT("Materials/M_A.dss")).IsEmpty());

	// A database another build wrote, with a schema this one does not know: rebuilt in place, never deleted.
	{
		FSQLiteDatabase Database;
		TestTrue(TEXT("the database opens for tampering"), Database.Open(*DatabasePath, ESQLiteDatabaseOpenMode::ReadWrite));
		Database.Execute(TEXT("UPDATE meta SET value = '999' WHERE key = 'schema';"));
		Database.Close();
	}
	TestTrue(TEXT("a write to a foreign schema succeeds"),
		FDreamShaderDatabase::WriteDocument(DatabasePath, FDreamShaderDatabase::DocumentSettings, TEXT("{}")));
	TestEqual(TEXT("with this build's schema"), ReadMeta(DatabasePath, TEXT("schema")), FString::FromInt(FDreamShaderDatabase::SchemaVersion));
	TestTrue(TEXT("and none of the foreign rows"), FDreamShaderDatabase::ReadDocument(DatabasePath, FDreamShaderDatabase::DocumentPassKeys).IsEmpty());
	TestEqual(TEXT("and its counters started over"), ReadMeta(DatabasePath, TEXT("revision")), FString(TEXT("1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDreamSourceWorkspaceRebaseTest,
	"DreamShader.DreamSource.WorkspaceRebase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDreamSourceWorkspaceRebaseTest::RunTest(const FString& Parameters)
{
	using UE::DreamShader::Editor::Private::FDreamShaderSourceMigration;

	const FString Workspace = TEXT(
		"{\n"
		"\t\"folders\": [\n"
		"\t\t{ \"name\": \"DreamShader Source\", \"path\": \".\" },\n"
		"\t\t{ \"name\": \"Plugin: MoonToon\", \"path\": \"../Plugins/MoonToon/DShader\" },\n"
		"\t\t{ \"name\": \"Elsewhere\", \"path\": \"C:/Shared/Shaders\" }\n"
		"\t],\n"
		"\t\"settings\": { \"files.associations\": { \"*.dss\": \"dreamshader-dss\" } }\n"
		"}\n");

	FString Rebased;
	TestTrue(TEXT("a workspace file is understood"), FDreamShaderSourceMigration::RebaseWorkspaceFolders(
		Workspace, TEXT("C:/Project/DShader"), TEXT("C:/Project/DreamSource/Shader"), Rebased));
	TestTrue(TEXT("a plugin folder now climbs one level more"), Rebased.Contains(TEXT("\"../../Plugins/MoonToon/DShader\"")));
	TestFalse(TEXT("and the old spelling is gone"), Rebased.Contains(TEXT("\"../Plugins/MoonToon/DShader\"")));
	TestTrue(TEXT("the source folder itself stays '.'"), Rebased.Contains(TEXT("\"path\": \".\"")));
	TestTrue(TEXT("an absolute folder is left alone"), Rebased.Contains(TEXT("\"C:/Shared/Shaders\"")));
	TestTrue(TEXT("and the rest of the file is untouched"), Rebased.Contains(TEXT("\"settings\": { \"files.associations\": { \"*.dss\": \"dreamshader-dss\" } }")));

	FString Ignored;
	TestFalse(TEXT("text that is not a workspace is refused"),
		FDreamShaderSourceMigration::RebaseWorkspaceFolders(TEXT("not json"), TEXT("C:/A"), TEXT("C:/B"), Ignored));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDreamSourceLayoutTest,
	"DreamShader.DreamSource.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDreamSourceLayoutTest::RunTest(const FString& Parameters)
{
	const FString Intermediate = UE::DreamShader::NormalizeSourceFilePath(
		FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("DreamSource")));
	TestEqual(TEXT("generated data lives under Intermediate/DreamSource"),
		UE::DreamShader::GetDreamSourceIntermediateDirectory(), Intermediate.LeftChop(Intermediate.EndsWith(TEXT("/")) ? 1 : 0));
	TestTrue(TEXT("the database is shader.db there"),
		UE::DreamShader::GetShaderDatabaseFilePath().EndsWith(TEXT("/Intermediate/DreamSource/shader.db")));
	TestTrue(TEXT("the shader-specific files go under Intermediate/DreamSource/Shader"),
		UE::DreamShader::GetShaderIntermediateDirectory().EndsWith(TEXT("/Intermediate/DreamSource/Shader")));

	const UE::DreamShader::FDreamShaderProjectSourceDirectory Project = UE::DreamShader::ResolveProjectSourceDirectory();
	TestTrue(TEXT("the source directory in use is the configured one or the legacy one"),
		Project.Directory == Project.ConfiguredDirectory || Project.Directory == Project.LegacyDirectory);
	TestEqual(TEXT("the legacy one is <Project>/DShader"), FPaths::GetCleanFilename(Project.LegacyDirectory), FString(TEXT("DShader")));
	TestEqual(TEXT("legacy exactly when the configured one is missing and DShader is there"),
		Project.bUsingLegacy,
		!IFileManager::Get().DirectoryExists(*Project.ConfiguredDirectory) && IFileManager::Get().DirectoryExists(*Project.LegacyDirectory));
	TestEqual(TEXT("and the roots start with it"), UE::DreamShader::GetSourceShaderDirectory(), Project.Directory);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
