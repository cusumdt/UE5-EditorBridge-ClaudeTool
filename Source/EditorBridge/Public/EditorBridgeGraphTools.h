#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EditorBridgeGraphTools.generated.h"

class UBlueprint;

// Editor-only Blueprint GRAPH primitives exposed to Python (unreal.EditorBridgeGraphTools).
// Nodes are addressed by their NodeGuid string (see ListNodes); pins by name.
// Works inside the running editor (EditorBridge inbox) and headless (commandlet).
UCLASS()
class EDITORBRIDGE_API UEditorBridgeGraphTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ─── READ ───────────────────────────────────────────

	// "<graph name> | <kind>"  kind = EventGraph / Function / Macro / Delegate / Sub
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Read")
	static TArray<FString> ListGraphs(UBlueprint* Blueprint);

	// One block per node:
	//   "<guid> | <node class> | <title> | (<x>,<y>)"
	//   "    <in|out> <pin name> : <type> [= <default>] [ORPHAN] -> <guid>.<pin>, ..."
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Read")
	static TArray<FString> ListNodes(UBlueprint* Blueprint, FName GraphName);

	// ─── ADD NODES (return the new node's guid, "" on failure) ──

	// Call to a UFUNCTION of FunctionOwner (C++ class or Blueprint generated class)
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Add")
	static FString AddCallFunctionNode(UBlueprint* Blueprint, FName GraphName, UClass* FunctionOwner, FName FunctionName, int32 X, int32 Y);

	// Get / Set of a variable. VariableOwner = nullptr (or the Blueprint's own class) → self member;
	// any other class → external member (needs a Target connection).
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Add")
	static FString AddVariableGetNode(UBlueprint* Blueprint, FName GraphName, UClass* VariableOwner, FName VariableName, int32 X, int32 Y);

	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Add")
	static FString AddVariableSetNode(UBlueprint* Blueprint, FName GraphName, UClass* VariableOwner, FName VariableName, int32 X, int32 Y);

	// "Cast To <TargetClass>" (impure by default: has exec pins)
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Add")
	static FString AddCastNode(UBlueprint* Blueprint, FName GraphName, UClass* TargetClass, int32 X, int32 Y, bool bPure = false);

	// Override of a BlueprintImplementableEvent / BlueprintNativeEvent declared in EventClass
	// (e.g. AMyActor::OnSomething). Fails if the graph already has that event.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Add")
	static FString AddEventNode(UBlueprint* Blueprint, FName GraphName, UClass* EventClass, FName EventName, int32 X, int32 Y);

	// Custom event whose parameters match the signature of a multicast delegate
	// (e.g. AMyManager::OnThingChanged). Its "OutputDelegate" pin can be wired
	// into a BindDelegate node. Fails if an event with that name already exists.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Add")
	static FString AddCustomEventFromDelegate(UBlueprint* Blueprint, FName GraphName, const FString& EventName, UClass* DelegateOwner, FName DelegateName, int32 X, int32 Y);

	// "Bind Event to <DelegateName>" node. Pins: execute / then / self (Target) / Delegate.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Add")
	static FString AddBindDelegateNode(UBlueprint* Blueprint, FName GraphName, UClass* DelegateOwner, FName DelegateName, int32 X, int32 Y);

	// ─── EDIT ───────────────────────────────────────────

	// Connects <NodeA>.<PinA> (output) to <NodeB>.<PinB> (input) through the schema
	// (type checked). Returns false if the schema refuses the connection.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Edit")
	static bool ConnectPins(UBlueprint* Blueprint, FName GraphName, const FString& NodeA, FName PinA, const FString& NodeB, FName PinB);

	// Breaks every link of a pin
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Edit")
	static bool DisconnectPin(UBlueprint* Blueprint, FName GraphName, const FString& Node, FName Pin);

	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Edit")
	static bool DeleteNode(UBlueprint* Blueprint, FName GraphName, const FString& Node);

	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Edit")
	static bool MoveNode(UBlueprint* Blueprint, FName GraphName, const FString& Node, int32 X, int32 Y);

	// Literal default for a pin ("true", "3.5", "Left", "Hello"...). For object/class
	// pins pass the asset path ("/Game/...") and it is loaded and set as default object.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Edit")
	static bool SetPinDefault(UBlueprint* Blueprint, FName GraphName, const FString& Node, FName Pin, const FString& Value);

	// Changes the type of an input/output pin of a MACRO or FUNCTION graph (its tunnel /
	// entry / result node). Category: object, class, bool, int, float, string, name, struct.
	// SubCategoryObject: the UClass for object/class pins, UScriptStruct for struct pins.
	// Macro instances / call sites in the same Blueprint are refreshed afterwards.
	UFUNCTION(BlueprintCallable, Category = "Editor Bridge Graph | Edit")
	static bool SetGraphPinType(UBlueprint* Blueprint, FName GraphName, FName PinName, const FString& Category, UObject* SubCategoryObject);
};
