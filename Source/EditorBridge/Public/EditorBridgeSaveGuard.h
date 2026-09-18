#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EditorBridgeSaveGuard.generated.h"

// Tracks, backs up and optionally blocks every package save that happens while a script
// runs (unreal.EditorBridgeSaveGuard). The editor's undo transaction restores objects in
// memory; this is what makes a save to disk recoverable as well.
UCLASS()
class EDITORBRIDGE_API UEditorBridgeSaveGuard : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Starts tracking. Every package saved from now on is recorded; if BackupDir is not
	// empty, the file that is about to be overwritten is copied first to
	// <BackupDir>/<path relative to the project dir> (one copy per package per run).
	// bBlockSaves makes every save fail with a clear message instead ("nosave" mode).
	// Calling Begin while active ends the previous run first.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Saves")
	static void Begin(const FString& BackupDir, bool bBlockSaves);

	// Stops tracking, restores the editor's own save check, and returns one line per event:
	//   "saved: <package> -> <file> (backup: <copy>)"
	//   "saved: <package> -> <file> (no backup: new asset | outside project)"
	//   "blocked: <package>"
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Saves")
	static TArray<FString> End();

	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Saves")
	static bool IsActive();

	// Copies every file under BackupDir back over its original (project-relative) path and
	// reloads the packages that are currently loaded, so the editor shows the disk state.
	// Returns one line per file ("restored: <file>" or "failed: <file>").
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Saves")
	static TArray<FString> RestoreBackups(const FString& BackupDir);
};
