#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

EDITORBRIDGE_API DECLARE_LOG_CATEGORY_EXTERN(LogEditorBridge, Log, All);

class FEditorBridgeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override {}
};
