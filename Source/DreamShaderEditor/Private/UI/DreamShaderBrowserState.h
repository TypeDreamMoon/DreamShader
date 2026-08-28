// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/DreamShaderBrowserUserSettings.h"
#include "UI/Model/DreamShaderBrowserModel.h"

namespace UE::DreamShader::Editor::Private
{
	// Where the navigation tree points the list: a source directory (Sources mode) or a content path
	// (Assets mode). Empty directory = every root; empty content path = /Game.
	struct FBrowserScope
	{
		EDreamShaderBrowserViewMode Mode = EDreamShaderBrowserViewMode::Sources;
		FString SourceDirectory; // absolute, normalized, no trailing slash
		FString ContentPath;     // "/Game/Materials"

		// Round-trips through the user settings.
		FString ToKey() const;
		static FBrowserScope FromKey(const FString& Key);

		bool operator==(const FBrowserScope& Other) const
		{
			return Mode == Other.Mode && SourceDirectory == Other.SourceDirectory && ContentPath == Other.ContentPath;
		}
	};

	// The state every pane of the browser shares: the filter the toolbar and the quick-filter
	// checkboxes edit, the scope the navigation tree picks, and a change signal for both. The shell
	// owns it; the panes hold a TSharedPtr.
	struct FBrowserSharedState
	{
		FBrowserFilter Filter;
		FBrowserScope Scope;
		FSimpleMulticastDelegate OnFilterChanged;
		FSimpleMulticastDelegate OnScopeChanged;

		void SetScope(const FBrowserScope& NewScope)
		{
			if (!(Scope == NewScope))
			{
				Scope = NewScope;
				Filter.SourceDirectoryScope = Scope.Mode == EDreamShaderBrowserViewMode::Sources ? Scope.SourceDirectory : FString();
				OnScopeChanged.Broadcast();
				OnFilterChanged.Broadcast();
			}
		}

		void NotifyFilterChanged()
		{
			OnFilterChanged.Broadcast();
		}
	};

	inline FString FBrowserScope::ToKey() const
	{
		return Mode == EDreamShaderBrowserViewMode::Sources
			? FString::Printf(TEXT("src:%s"), *SourceDirectory)
			: FString::Printf(TEXT("content:%s"), *ContentPath);
	}

	inline FBrowserScope FBrowserScope::FromKey(const FString& Key)
	{
		FBrowserScope Scope;
		if (Key.StartsWith(TEXT("content:")))
		{
			Scope.Mode = EDreamShaderBrowserViewMode::Assets;
			Scope.ContentPath = Key.Mid(8);
		}
		else if (Key.StartsWith(TEXT("src:")))
		{
			Scope.Mode = EDreamShaderBrowserViewMode::Sources;
			Scope.SourceDirectory = Key.Mid(4);
		}
		return Scope;
	}
}
