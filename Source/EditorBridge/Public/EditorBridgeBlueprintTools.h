#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EditorBridgeBlueprintTools.generated.h"

class UBlueprint;
class UStaticMesh;

// Editor-only helpers exposed to Python (unreal.EditorBridgeBlueprintTools) and Editor
// Utility Blueprints. They wrap FBlueprintEditorUtils operations that the stock
// BlueprintEditorLibrary does not expose, so Blueprint refactors can be scripted
// either inside the running editor (EditorBridge inbox) or headless:
//   UnrealEditor-Cmd.exe <project> -run=pythonscript -script=<file.py>
UCLASS()
class EDITORBRIDGE_API UEditorBridgeBlueprintTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ─── VARIABLES ──────────────────────────────────────

	// Renames every variable node in Blueprint that references
	// VariableClass::OldName (or any subclass of VariableClass) to NewName.
	// Unlike BlueprintEditorLibrary.ReplaceVariableReferences this does not
	// depend on the dependency cache and works on external references, e.g.
	// BP_Controller nodes that point at BP_Vehicle_C::"SomeVar" after the variable moved.
	// Returns the number of nodes changed.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Variables")
	static int32 RenameVariableReferences(UBlueprint* Blueprint, UClass* VariableClass, FName OldName, FName NewName);

	// Re-points variable / function-call nodes whose member parent is FromClass to
	// ToClass, when ToClass also declares that member (e.g. after moving a variable
	// from a Blueprint to its C++ parent: the nodes still hard-reference the BP
	// class through FMemberReference::MemberParent). Returns nodes changed.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Variables")
	static int32 RetargetMemberReferences(UBlueprint* Blueprint, UClass* FromClass, UClass* ToClass);

	// Removes a member variable declared by the Blueprint itself (not inherited).
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Variables")
	static bool RemoveMemberVariable(UBlueprint* Blueprint, FName VariableName);

	// Names of the member variables declared by the Blueprint itself.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Variables")
	static TArray<FName> ListMemberVariables(UBlueprint* Blueprint);

	// "<name> : <type>" for every member variable declared by the Blueprint itself.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Variables")
	static TArray<FString> ListMemberVariableTypes(UBlueprint* Blueprint);

	// Changes the type of a member variable declared by the Blueprint. Category as in
	// EditorBridgeGraphTools.SetGraphPinType (object, class, bool, int, float, string, name, struct).
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Variables")
	static bool ChangeMemberVariableType(UBlueprint* Blueprint, FName VariableName, const FString& Category, UObject* SubCategoryObject);

	// "<function> | <local var> : <type>" for every local variable of every function graph.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Variables")
	static TArray<FString> ListLocalVariables(UBlueprint* Blueprint);

	// One line per variable node in the Blueprint: "<graph> | <node title> | <member parent class>::<var name>".
	// Pass NAME_None to list every variable node.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Variables")
	static TArray<FString> ListVariableReferences(UBlueprint* Blueprint, FName VariableName);

	// ─── NODES / COMPILE ────────────────────────────────

	// Equivalent to File > Refresh All Nodes: reconstructs every node so stale
	// pins ("In use pin X no longer exists on node") are rebuilt.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Nodes")
	static void RefreshAllNodes(UBlueprint* Blueprint);

	// Fixes "In use pin X no longer exists on node Y": for every orphaned pin
	// (left behind when a variable/pin changed type) moves its links to the
	// live pin with the same name and removes the orphan. Links the schema
	// rejects (incompatible types) are reported in OutUnfixable and dropped.
	// Returns the number of orphaned pins removed.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Nodes")
	static int32 RewireOrphanedPins(UBlueprint* Blueprint, TArray<FString>& OutUnfixable);

	// Compiles the Blueprint and returns the compiler messages (errors + warnings).
	// bHasErrors is true if at least one error was reported.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Nodes")
	static TArray<FString> CompileBlueprintWithLog(UBlueprint* Blueprint, bool& bHasErrors);

	// ─── REFERENCES ─────────────────────────────────────

	// Every object inside the Blueprint's package that references Target, with the
	// property holding the reference when it can be determined. Use it to find the
	// last hard reference to a class/asset ("<object path> | <property or ?>").
	// TargetPath: an object path ("/Game/X/BP_Y.BP_Y" or "/Game/X/BP_Y.BP_Y_C"). If it is a
	// Blueprint asset, its generated class and CDO are checked as well.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | References")
	static TArray<FString> FindReferencers(UBlueprint* Blueprint, const FString& TargetPath);

	// ─── CLASS ──────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Class")
	static UClass* GetParentClass(UBlueprint* Blueprint);

	// ─── COMPONENTS ─────────────────────────────────────

	// One line per component owned by the Blueprint's construction script:
	// "<component name> | <class> | <static mesh path or None>".
	// Inherited components are listed with their override (if any) or "(inherited)".
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Components")
	static TArray<FString> ListComponents(UBlueprint* Blueprint);

	// Sets (or clears, with nullptr) the Static Mesh of a StaticMeshComponent
	// declared in this Blueprint's construction script or overridden from a parent.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge | Components")
	static bool SetComponentStaticMesh(UBlueprint* Blueprint, FName ComponentName, UStaticMesh* NewMesh);
};
