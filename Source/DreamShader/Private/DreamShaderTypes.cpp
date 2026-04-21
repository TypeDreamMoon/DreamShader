#include "DreamShaderTypes.h"

namespace UE::DreamShader
{
	FString NormalizeSettingKey(const FString& InKey)
	{
		FString Result = InKey;
		Result.TrimStartAndEndInline();
		Result.ToLowerInline();
		return Result;
	}

	bool FTextShaderDefinition::TryGetSetting(const TCHAR* Key, FString& OutValue) const
	{
		if (const FString* Value = Settings.Find(NormalizeSettingKey(Key)))
		{
			OutValue = *Value;
			return true;
		}

		return false;
	}

	FString FTextShaderDefinition::GetSetting(const TCHAR* Key, const TCHAR* DefaultValue) const
	{
		FString Value;
		return TryGetSetting(Key, Value) ? Value : FString(DefaultValue);
	}
}
