// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "UI/DreamShaderBrowserUserSettings.h"

UDreamShaderBrowserUserSettings* UDreamShaderBrowserUserSettings::Get()
{
	return GetMutableDefault<UDreamShaderBrowserUserSettings>();
}

void UDreamShaderBrowserUserSettings::Save()
{
	SaveConfig();
}
