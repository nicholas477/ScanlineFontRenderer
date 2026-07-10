// Copyright Nicholas Chalkley. All Rights Reserved.

using UnrealBuildTool;

public class ScanlineFontRendererEditor : ModuleRules
{
    public ScanlineFontRendererEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
            );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "UnrealEd",
                "AssetTools",
                "ContentBrowser",
                "ToolMenus",
                "ScanlineFontRenderer",
                "PropertyEditor",
            }
            );
    }
}
