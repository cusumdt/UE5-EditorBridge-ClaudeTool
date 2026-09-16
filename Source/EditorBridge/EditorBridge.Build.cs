// Editor-only module. Exposes Blueprint graph / variable / asset tooling to Python and
// Editor Utility Blueprints. Never part of a packaged build.

using UnrealBuildTool;

public class EditorBridge : ModuleRules
{
	public EditorBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",          // FBlueprintEditorUtils, FKismetEditorUtilities
			"BlueprintGraph",    // UK2Node_*
			"Kismet",
			"RenderCore",        // FRayTracingGeometry
			"RHI",               // FRHIRayTracingGeometry::GetSizeInfo
		});
	}
}
