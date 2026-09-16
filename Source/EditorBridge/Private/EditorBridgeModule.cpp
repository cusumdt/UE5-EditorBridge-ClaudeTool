#include "EditorBridgeModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogEditorBridge);

void FEditorBridgeModule::StartupModule()
{
	UE_LOG(LogEditorBridge, Log, TEXT("EditorBridge module loaded: unreal.EditorBridgeBlueprintTools / EditorBridgeGraphTools / EditorBridgeAssetTools available to Python"));
}

IMPLEMENT_MODULE(FEditorBridgeModule, EditorBridge);
