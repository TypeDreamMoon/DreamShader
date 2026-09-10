using UnrealBuildTool;

// The DreamShaderLang front end: lexer, preprocessor, the two parsers, the AST, the printer and
// the diagnostic types. It depends on Core and on nothing else -- no UObject, no Engine -- so it
// can be exercised without an editor, reused by a language service, and reasoned about as a
// language rather than as a plugin. Engine facts (the DS_* preprocessor constants, asset roots,
// expression class names) enter as plain data supplied by the host; nothing in this module can
// look them up.
public class DreamShaderLang : ModuleRules
{
	public DreamShaderLang(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core"
			});
	}
}
