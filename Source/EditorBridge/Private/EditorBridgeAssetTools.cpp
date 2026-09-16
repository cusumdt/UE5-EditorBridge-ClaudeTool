#include "EditorBridgeAssetTools.h"
#include "EditorBridgeModule.h"

#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "RayTracingGeometry.h"
#include "RHIResources.h"
#include "UObject/UObjectIterator.h"

// ─── MEMORY / RAY TRACING ─────────────────────────────────────────────
TArray<FString> UEditorBridgeAssetTools::ListLoadedStaticMeshes(bool bOnlyGameContent)
{
	struct FEntry { FString Line; uint64 AlwaysResident = 0; uint64 Resident = 0; };
	TArray<FEntry> Entries;
	uint64 TotalResident = 0, TotalAlwaysResident = 0;

	for (TObjectIterator<UStaticMesh> It; It; ++It)
	{
		UStaticMesh* Mesh = *It;
		if (!Mesh || Mesh->IsTemplate()) continue;
		const FString Path = Mesh->GetPathName();
		if (bOnlyGameContent && !Path.StartsWith(TEXT("/Game/"))) continue;

		const FStaticMeshRenderData* RD = Mesh->GetRenderData();
		const int32 NumLODs = RD ? RD->LODResources.Num() : 0;

		FString Tris;
		for (int32 i = 0; i < NumLODs; ++i)
		{
			if (i) Tris += TEXT(",");
			Tris += FString::FromInt(RD->LODResources[i].GetNumTriangles());
		}

		FEntry E;
#if RHI_RAYTRACING
		if (RD && RD->RayTracingProxy)
		{
			const int32 NumRTLODs = RD->RayTracingProxy->LODs.Num();
			for (int32 i = 0; i < NumRTLODs; ++i)
			{
				const FRayTracingGeometry* Geo = RD->RayTracingProxy->LODs[i].RayTracingGeometry;
				const FRHIRayTracingGeometry* RHI = Geo ? Geo->GetRHI() : nullptr;
				const uint64 Size = RHI ? RHI->GetSizeInfo().ResultSize : 0;
				E.Resident += Size;
				if (i == NumRTLODs - 1) E.AlwaysResident = Size;
			}
		}
#endif
		TotalResident += E.Resident;
		TotalAlwaysResident += E.AlwaysResident;

		E.Line = FString::Printf(TEXT("%s | LODs=%d | tris=[%s] | RT=%d | Nanite=%d | resident=%.1f | alwaysResident=%.1f"),
			*Path, NumLODs, *Tris, Mesh->bSupportRayTracing ? 1 : 0, Mesh->IsNaniteEnabled() ? 1 : 0,
			E.Resident / 1048576.0, E.AlwaysResident / 1048576.0);
		Entries.Add(MoveTemp(E));
	}

	Entries.Sort([](const FEntry& A, const FEntry& B)
	{
		return A.AlwaysResident != B.AlwaysResident ? A.AlwaysResident > B.AlwaysResident : A.Resident > B.Resident;
	});

	TArray<FString> Lines;
	for (const FEntry& E : Entries) Lines.Add(E.Line);
	Lines.Add(FString::Printf(TEXT("TOTAL | meshes=%d | resident=%.1f MiB | alwaysResident=%.1f MiB"),
		Entries.Num(), TotalResident / 1048576.0, TotalAlwaysResident / 1048576.0));
	return Lines;
}

