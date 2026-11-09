using UnrealBuildTool;

public class HoldfastTarget : TargetRules
{
	public HoldfastTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Holdfast");
	}
}
