// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "DreamShaderBrowserUserSettings.generated.h"

UENUM()
enum class EDreamShaderBrowserViewMode : uint8
{
	Sources, // the .dsm/.dsf/.dsh tree, with compile status
	Assets,  // the project's materials, through the engine asset picker
};

UENUM()
enum class EDreamShaderBrowserSortColumn : uint8
{
	Name,
	Status,
	Root,
	Asset,
};

/**
 * Per-user, per-project state of the Material Content Browser: which mode it was in, how the three
 * panes were split, which filters were on, what was selected. Saved so the tab reopens the way it
 * was closed rather than reset to defaults every session. Nothing in here is project content.
 */
UCLASS(Config = EditorPerProjectUserSettings)
class UDreamShaderBrowserUserSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	EDreamShaderBrowserViewMode ViewMode = EDreamShaderBrowserViewMode::Sources;

	/** Navigation tree node that scopes the list: a source directory, or a content path. */
	UPROPERTY(Config)
	FString NavigationScope;

	UPROPERTY(Config)
	float NavigationPaneFraction = 0.18f;

	UPROPERTY(Config)
	float InspectorPaneFraction = 0.32f;

	UPROPERTY(Config)
	bool bErrorsOnly = false;

	UPROPERTY(Config)
	bool bStaleOnly = false;

	UPROPERTY(Config)
	bool bDivergedOnly = false;

	UPROPERTY(Config)
	bool bInMemoryOnly = false;

	UPROPERTY(Config)
	bool bHideLibraries = false;

	UPROPERTY(Config)
	bool bHideUnmanaged = false;

	UPROPERTY(Config)
	bool bTileView = false;

	UPROPERTY(Config)
	EDreamShaderBrowserSortColumn SortColumn = EDreamShaderBrowserSortColumn::Name;

	UPROPERTY(Config)
	bool bSortAscending = true;

	/** The entry key (source path or object path) selected when the tab was last used. */
	UPROPERTY(Config)
	FString LastSelectedKey;

	/** Preview primitive for the inspector: sphere, plane, cube, cylinder, shaderball. */
	UPROPERTY(Config)
	FString PreviewMesh = TEXT("sphere");

	static UDreamShaderBrowserUserSettings* Get();
	void Save();
};
