#include "DreamShaderSourceSettings.h"

UDreamShaderSourceSettings::UDreamShaderSourceSettings()
{
	SourceDirectory.Path = GetDefaultSourceDirectory();
}

const TCHAR* UDreamShaderSourceSettings::GetDefaultSourceDirectory()
{
	return TEXT("DreamSource/Shader");
}
