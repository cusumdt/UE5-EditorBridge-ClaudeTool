#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EditorBridgeAssetTools.generated.h"

// Editor-only asset / memory diagnostics exposed to Python (unreal.EditorBridgeAssetTools).
UCLASS()
class EDITORBRIDGE_API UEditorBridgeAssetTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Every static mesh currently LOADED in this editor session, sorted by the ray
	// tracing memory it keeps always resident (its last LOD). One line per mesh:
	// "<path> | LODs=<n> | tris=[lod0,lod1,...] | RT=<0/1> | Nanite=<0/1> | resident=<MiB> | alwaysResident=<MiB>"
	// resident = BLAS currently in GPU memory (all LODs), alwaysResident = last LOD only
	// (what r.RayTracing.NumAlwaysResidentLODs=1 never evicts). Meshes without RT geometry
	// report 0. Totals are appended as a final "TOTAL | ..." line.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Memory")
	static TArray<FString> ListLoadedStaticMeshes(bool bOnlyGameContent = true);

};
