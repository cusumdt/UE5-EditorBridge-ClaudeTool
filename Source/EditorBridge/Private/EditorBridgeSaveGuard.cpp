#include "EditorBridgeSaveGuard.h"
#include "EditorBridgeModule.h"

#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

// ─── STATE ─────────────────────────────────────────────────────────────
namespace
{
	bool bActive = false;
	bool bBlocking = false;
	FString RunBackupDir;
	FDelegateHandle PreSaveHandle;
	FDelegateHandle SavedHandle;
	FCoreUObjectDelegates::FIsPackageOKToSaveDelegate PreviousOkToSave;   // the editor's own check, restored in End()
	TMap<FName, FString> Backups;      // package -> backup copy made this run
	TArray<FString> Events;

	// Path of the backup copy for a file inside the project, or empty when it is outside.
	FString BackupPathFor(const FString& ExistingFile)
	{
		FString Relative = ExistingFile;
		if (!FPaths::MakePathRelativeTo(Relative, *FPaths::ProjectDir()) || Relative.StartsWith(TEXT("..")))
		{
			return FString();
		}
		return FPaths::Combine(RunBackupDir, Relative);
	}

	void OnPreSave(UPackage* Package, FObjectPreSaveContext Context)
	{
		if (!Package || Context.IsCooking() || bBlocking || RunBackupDir.IsEmpty() || Backups.Contains(Package->GetFName()))
		{
			return;
		}
		FString Existing;
		if (!FPackageName::DoesPackageExist(Package->GetName(), &Existing))
		{
			return;   // new asset: nothing on disk to protect
		}
		Existing = FPaths::ConvertRelativePathToFull(Existing);
		const FString Dest = BackupPathFor(Existing);
		if (Dest.IsEmpty())
		{
			return;
		}
		if (IFileManager::Get().Copy(*Dest, *Existing, /*Replace*/ true, /*EvenIfReadOnly*/ true) == COPY_OK)
		{
			Backups.Add(Package->GetFName(), Dest);
		}
		else
		{
			UE_LOG(LogEditorBridge, Warning, TEXT("SaveGuard: could not back up %s to %s"), *Existing, *Dest);
		}
	}

	void OnSaved(const FString& Filename, UPackage* Package, FObjectPostSaveContext Context)
	{
		if (!Package || Context.IsCooking())
		{
			return;
		}
		const FString Full = FPaths::ConvertRelativePathToFull(Filename);
		FString Note;
		if (const FString* Backup = Backups.Find(Package->GetFName()))
		{
			Note = FString::Printf(TEXT("backup: %s"), **Backup);
		}
		else if (BackupPathFor(Full).IsEmpty())
		{
			Note = TEXT("no backup: outside project");
		}
		else
		{
			Note = TEXT("no backup: new asset");
		}
		Events.Add(FString::Printf(TEXT("saved: %s -> %s (%s)"), *Package->GetName(), *Full, *Note));
	}

	bool OnIsOkToSave(UPackage* Package, const FString& Filename, FOutputDevice* Error)
	{
		Events.Add(FString::Printf(TEXT("blocked: %s"), Package ? *Package->GetName() : TEXT("?")));
		if (Error)
		{
			Error->Logf(ELogVerbosity::Warning, TEXT("EditorBridge: saving is disabled for this run (nosave)"));
		}
		return false;
	}
}

// ─── BEGIN / END ───────────────────────────────────────────────────────
void UEditorBridgeSaveGuard::Begin(const FString& BackupDir, bool bBlockSaves)
{
	if (bActive)
	{
		End();
	}
	bActive = true;
	bBlocking = bBlockSaves;
	RunBackupDir = BackupDir.IsEmpty() ? FString() : FPaths::ConvertRelativePathToFull(BackupDir);
	Backups.Reset();
	Events.Reset();

	PreSaveHandle = UPackage::PreSavePackageWithContextEvent.AddStatic(&OnPreSave);
	SavedHandle = UPackage::PackageSavedWithContextEvent.AddStatic(&OnSaved);
	if (bBlocking)
	{
		PreviousOkToSave = FCoreUObjectDelegates::IsPackageOKToSaveDelegate;
		FCoreUObjectDelegates::IsPackageOKToSaveDelegate.BindStatic(&OnIsOkToSave);
	}
	UE_LOG(LogEditorBridge, Log, TEXT("SaveGuard: begin (backup=%s, block=%d)"), *RunBackupDir, bBlocking ? 1 : 0);
}

TArray<FString> UEditorBridgeSaveGuard::End()
{
	if (!bActive)
	{
		return {};
	}
	UPackage::PreSavePackageWithContextEvent.Remove(PreSaveHandle);
	UPackage::PackageSavedWithContextEvent.Remove(SavedHandle);
	if (bBlocking)
	{
		FCoreUObjectDelegates::IsPackageOKToSaveDelegate = PreviousOkToSave;
		PreviousOkToSave.Unbind();
	}
	bActive = false;
	bBlocking = false;

	TArray<FString> Result = MoveTemp(Events);
	Events.Reset();
	Backups.Reset();
	UE_LOG(LogEditorBridge, Log, TEXT("SaveGuard: end (%d events)"), Result.Num());
	return Result;
}

bool UEditorBridgeSaveGuard::IsActive()
{
	return bActive;
}

// ─── ROLLBACK ──────────────────────────────────────────────────────────
TArray<FString> UEditorBridgeSaveGuard::RestoreBackups(const FString& BackupDir)
{
	TArray<FString> Result;
	const FString Root = FPaths::ConvertRelativePathToFull(BackupDir);
	if (!IFileManager::Get().DirectoryExists(*Root))
	{
		Result.Add(FString::Printf(TEXT("failed: backup folder not found: %s"), *Root));
		return Result;
	}

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.*"), /*Files*/ true, /*Directories*/ false);

	TArray<UPackage*> ToReload;
	for (const FString& Backup : Files)
	{
		FString Relative = Backup;
		FPaths::MakePathRelativeTo(Relative, *(Root + TEXT("/")));
		const FString Target = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Relative));

		if (IFileManager::Get().Copy(*Target, *Backup, /*Replace*/ true, /*EvenIfReadOnly*/ true) != COPY_OK)
		{
			Result.Add(FString::Printf(TEXT("failed: %s"), *Target));
			continue;
		}
		Result.Add(FString::Printf(TEXT("restored: %s"), *Target));

		FString PackageName;
		if (FPackageName::TryConvertFilenameToLongPackageName(Target, PackageName))
		{
			if (UPackage* Loaded = FindPackage(nullptr, *PackageName))
			{
				ToReload.Add(Loaded);
			}
		}
	}

	if (ToReload.Num())
	{
		FText Error;
		const bool bOk = UPackageTools::ReloadPackages(ToReload, Error, EReloadPackagesInteractionMode::AssumePositive);
		Result.Add(FString::Printf(TEXT("reloaded %d loaded package(s)%s"), ToReload.Num(),
			bOk ? TEXT("") : *FString::Printf(TEXT(" with errors: %s"), *Error.ToString())));
	}
	return Result;
}
