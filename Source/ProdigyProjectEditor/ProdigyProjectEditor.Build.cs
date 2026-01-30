using UnrealBuildTool;

public class ProdigyProjectEditor : ModuleRules
{
	public ProdigyProjectEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine", "ProdigyProject"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"UnrealEd",
			"Slate",
			"SlateCore",
			"GraphEditor",
			"PropertyEditor",
			"ToolMenus",
			"AssetTools",
			"EditorStyle",

			// to reference runtime dialogue asset/structs
			"ProdigyProject"
		});
	}
}
