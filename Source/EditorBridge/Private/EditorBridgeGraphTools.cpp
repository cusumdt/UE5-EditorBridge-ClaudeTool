#include "EditorBridgeGraphTools.h"
#include "EditorBridgeModule.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_Event.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"


// ─── HELPERS ──────────────────────────────────────────────────────────
namespace
{
	UEdGraph* FindGraph(UBlueprint* Blueprint, FName GraphName)
	{
		if (!Blueprint) return nullptr;
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && Graph->GetFName() == GraphName) return Graph;
		}
		UE_LOG(LogEditorBridge, Warning, TEXT("Graph '%s' not found in '%s'"), *GraphName.ToString(), *GetNameSafe(Blueprint));
		return nullptr;
	}

	UEdGraphNode* FindNode(UEdGraph* Graph, const FString& GuidStr)
	{
		FGuid Guid;
		if (!Graph || !FGuid::Parse(GuidStr, Guid))
		{
			UE_LOG(LogEditorBridge, Warning, TEXT("Invalid node guid '%s'"), *GuidStr);
			return nullptr;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == Guid) return Node;
		}
		UE_LOG(LogEditorBridge, Warning, TEXT("Node %s not found in graph '%s'"), *GuidStr, *Graph->GetName());
		return nullptr;
	}

	// Prefers the requested direction, falls back to any direction with that name
	UEdGraphPin* FindPin(UEdGraphNode* Node, FName PinName, EEdGraphPinDirection Preferred)
	{
		if (!Node) return nullptr;
		if (UEdGraphPin* Pin = Node->FindPin(PinName, Preferred)) return Pin;
		if (UEdGraphPin* Pin = Node->FindPin(PinName)) return Pin;
		UE_LOG(LogEditorBridge, Warning, TEXT("Pin '%s' not found on node '%s'"),
			*PinName.ToString(), *Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
		return nullptr;
	}

	FString NodeLine(const UEdGraphNode* Node)
	{
		return FString::Printf(TEXT("%s | %s | %s | (%d,%d)"),
			*Node->NodeGuid.ToString(),
			*Node->GetClass()->GetName(),
			*Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"), TEXT(" ")),
			Node->NodePosX, Node->NodePosY);
	}

	FString PinLine(const UEdGraphPin* Pin)
	{
		FString Links;
		for (const UEdGraphPin* Other : Pin->LinkedTo)
		{
			if (!Other || !Other->GetOwningNode()) continue;
			if (!Links.IsEmpty()) Links += TEXT(", ");
			Links += Other->GetOwningNode()->NodeGuid.ToString() + TEXT(".") + Other->PinName.ToString();
		}

		FString Default;
		if (Pin->DefaultObject)             Default = TEXT(" = ") + Pin->DefaultObject->GetPathName();
		else if (!Pin->DefaultValue.IsEmpty()) Default = TEXT(" = ") + Pin->DefaultValue;

		return FString::Printf(TEXT("    %s %s : %s%s%s%s"),
			Pin->Direction == EGPD_Input ? TEXT("in ") : TEXT("out"),
			*Pin->PinName.ToString(),
			*UEdGraphSchema_K2::TypeToText(Pin->PinType).ToString(),
			*Default,
			Pin->bOrphanedPin ? TEXT(" [ORPHAN]") : TEXT(""),
			Links.IsEmpty() ? TEXT("") : *(TEXT(" -> ") + Links));
	}

	FString Finalize(UBlueprint* Blueprint, UEdGraphNode* Node, const TCHAR* What)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		UE_LOG(LogEditorBridge, Log, TEXT("%s: added %s"), What, *NodeLine(Node));
		return Node->NodeGuid.ToString();
	}
}

// ─── READ ─────────────────────────────────────────────────────────────
TArray<FString> UEditorBridgeGraphTools::ListGraphs(UBlueprint* Blueprint)
{
	TArray<FString> Lines;
	if (!Blueprint) return Lines;

	auto Kind = [Blueprint](UEdGraph* G) -> const TCHAR*
	{
		if (Blueprint->UbergraphPages.Contains(G))          return TEXT("EventGraph");
		if (Blueprint->FunctionGraphs.Contains(G))          return TEXT("Function");
		if (Blueprint->MacroGraphs.Contains(G))             return TEXT("Macro");
		if (Blueprint->DelegateSignatureGraphs.Contains(G)) return TEXT("Delegate");
		return TEXT("Sub");
	};

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph) Lines.Add(FString::Printf(TEXT("%s | %s"), *Graph->GetName(), Kind(Graph)));
	}
	return Lines;
}

TArray<FString> UEditorBridgeGraphTools::ListNodes(UBlueprint* Blueprint, FName GraphName)
{
	TArray<FString> Lines;
	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph) return Lines;

	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;
		Lines.Add(NodeLine(Node));
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin) Lines.Add(PinLine(Pin) + (Pin->bHidden ? TEXT("  (hidden)") : TEXT("")));
		}
	}
	return Lines;
}

// ─── ADD NODES ────────────────────────────────────────────────────────
FString UEditorBridgeGraphTools::AddCallFunctionNode(UBlueprint* Blueprint, FName GraphName, UClass* FunctionOwner, FName FunctionName, int32 X, int32 Y)
{
	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph || !FunctionOwner) return TEXT("");

	UFunction* Function = FunctionOwner->FindFunctionByName(FunctionName);
	if (!Function)
	{
		UE_LOG(LogEditorBridge, Warning, TEXT("Function '%s' not found in '%s'"), *FunctionName.ToString(), *FunctionOwner->GetName());
		return TEXT("");
	}

	FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
	UK2Node_CallFunction* Node = Creator.CreateNode();
	Node->SetFromFunction(Function);
	Node->NodePosX = X;
	Node->NodePosY = Y;
	Creator.Finalize();

	return Finalize(Blueprint, Node, TEXT("AddCallFunctionNode"));
}

template <typename TNode>
static FString AddVariableNode(UBlueprint* Blueprint, FName GraphName, UClass* VariableOwner, FName VariableName, int32 X, int32 Y, const TCHAR* What)
{
	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph) return TEXT("");

	UClass* SelfClass = Blueprint->SkeletonGeneratedClass ? Blueprint->SkeletonGeneratedClass.Get() : Blueprint->GeneratedClass.Get();
	const bool bSelf = (VariableOwner == nullptr) || (SelfClass && SelfClass->IsChildOf(VariableOwner));
	UClass* LookupClass = bSelf ? SelfClass : VariableOwner;

	if (!LookupClass || !FindFProperty<FProperty>(LookupClass, VariableName))
	{
		UE_LOG(LogEditorBridge, Warning, TEXT("Variable '%s' not found in '%s'"), *VariableName.ToString(), *GetNameSafe(LookupClass));
		return TEXT("");
	}

	FGraphNodeCreator<TNode> Creator(*Graph);
	TNode* Node = Creator.CreateNode();
	if (bSelf) Node->VariableReference.SetSelfMember(VariableName);
	else       Node->VariableReference.SetExternalMember(VariableName, VariableOwner);
	Node->NodePosX = X;
	Node->NodePosY = Y;
	Creator.Finalize();

	return Finalize(Blueprint, Node, What);
}

FString UEditorBridgeGraphTools::AddVariableGetNode(UBlueprint* Blueprint, FName GraphName, UClass* VariableOwner, FName VariableName, int32 X, int32 Y)
{
	return AddVariableNode<UK2Node_VariableGet>(Blueprint, GraphName, VariableOwner, VariableName, X, Y, TEXT("AddVariableGetNode"));
}

FString UEditorBridgeGraphTools::AddVariableSetNode(UBlueprint* Blueprint, FName GraphName, UClass* VariableOwner, FName VariableName, int32 X, int32 Y)
{
	return AddVariableNode<UK2Node_VariableSet>(Blueprint, GraphName, VariableOwner, VariableName, X, Y, TEXT("AddVariableSetNode"));
}

FString UEditorBridgeGraphTools::AddCastNode(UBlueprint* Blueprint, FName GraphName, UClass* TargetClass, int32 X, int32 Y, bool bPure)
{
	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph || !TargetClass) return TEXT("");

	FGraphNodeCreator<UK2Node_DynamicCast> Creator(*Graph);
	UK2Node_DynamicCast* Node = Creator.CreateNode();
	Node->TargetType = TargetClass;
	Node->SetPurity(bPure);
	Node->NodePosX = X;
	Node->NodePosY = Y;
	Creator.Finalize();

	return Finalize(Blueprint, Node, TEXT("AddCastNode"));
}

FString UEditorBridgeGraphTools::AddEventNode(UBlueprint* Blueprint, FName GraphName, UClass* EventClass, FName EventName, int32 X, int32 Y)
{
	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph || !EventClass) return TEXT("");

	UFunction* Function = EventClass->FindFunctionByName(EventName);
	if (!Function || !Function->HasAnyFunctionFlags(FUNC_BlueprintEvent))
	{
		UE_LOG(LogEditorBridge, Warning, TEXT("'%s' is not a Blueprint event of '%s'"), *EventName.ToString(), *EventClass->GetName());
		return TEXT("");
	}
	if (UK2Node_Event* Existing = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, EventClass, EventName))
	{
		UE_LOG(LogEditorBridge, Warning, TEXT("Event '%s' already exists in '%s' (%s)"),
			*EventName.ToString(), *Blueprint->GetName(), *Existing->NodeGuid.ToString());
		return TEXT("");
	}

	FGraphNodeCreator<UK2Node_Event> Creator(*Graph);
	UK2Node_Event* Node = Creator.CreateNode();
	Node->EventReference.SetExternalMember(EventName, EventClass);
	Node->bOverrideFunction = true;
	Node->NodePosX = X;
	Node->NodePosY = Y;
	Creator.Finalize();

	return Finalize(Blueprint, Node, TEXT("AddEventNode"));
}

FString UEditorBridgeGraphTools::AddCustomEventFromDelegate(UBlueprint* Blueprint, FName GraphName, const FString& EventName, UClass* DelegateOwner, FName DelegateName, int32 X, int32 Y)
{
	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph || !DelegateOwner) return TEXT("");

	const FMulticastDelegateProperty* Delegate = FindFProperty<FMulticastDelegateProperty>(DelegateOwner, DelegateName);
	if (!Delegate || !Delegate->SignatureFunction)
	{
		UE_LOG(LogEditorBridge, Warning, TEXT("Delegate '%s' not found in '%s'"), *DelegateName.ToString(), *DelegateOwner->GetName());
		return TEXT("");
	}
	// CreateFromFunction uniquifies the name if an event called EventName already exists.
	UK2Node_CustomEvent* Node = UK2Node_CustomEvent::CreateFromFunction(FVector2D(X, Y), Graph, EventName, Delegate->SignatureFunction, false);
	if (!Node) return TEXT("");

	return Finalize(Blueprint, Node, TEXT("AddCustomEventFromDelegate"));
}

FString UEditorBridgeGraphTools::AddBindDelegateNode(UBlueprint* Blueprint, FName GraphName, UClass* DelegateOwner, FName DelegateName, int32 X, int32 Y)
{
	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph || !DelegateOwner) return TEXT("");

	const FMulticastDelegateProperty* Delegate = FindFProperty<FMulticastDelegateProperty>(DelegateOwner, DelegateName);
	if (!Delegate)
	{
		UE_LOG(LogEditorBridge, Warning, TEXT("Delegate '%s' not found in '%s'"), *DelegateName.ToString(), *DelegateOwner->GetName());
		return TEXT("");
	}

	UClass* SelfClass = Blueprint->SkeletonGeneratedClass ? Blueprint->SkeletonGeneratedClass.Get() : Blueprint->GeneratedClass.Get();
	const bool bSelf = SelfClass && SelfClass->IsChildOf(DelegateOwner);

	FGraphNodeCreator<UK2Node_AddDelegate> Creator(*Graph);
	UK2Node_AddDelegate* Node = Creator.CreateNode();
	Node->SetFromProperty(Delegate, bSelf, DelegateOwner);
	Node->NodePosX = X;
	Node->NodePosY = Y;
	Creator.Finalize();

	return Finalize(Blueprint, Node, TEXT("AddBindDelegateNode"));
}

// ─── EDIT ─────────────────────────────────────────────────────────────
bool UEditorBridgeGraphTools::ConnectPins(UBlueprint* Blueprint, FName GraphName, const FString& NodeA, FName PinA, const FString& NodeB, FName PinB)
{
	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph) return false;

	UEdGraphPin* A = FindPin(FindNode(Graph, NodeA), PinA, EGPD_Output);
	UEdGraphPin* B = FindPin(FindNode(Graph, NodeB), PinB, EGPD_Input);
	if (!A || !B) return false;

	const UEdGraphSchema* Schema = Graph->GetSchema();
	const FPinConnectionResponse Response = Schema->CanCreateConnection(A, B);
	if (!Schema->TryCreateConnection(A, B))
	{
		UE_LOG(LogEditorBridge, Warning, TEXT("ConnectPins: %s -> %s refused: %s"),
			*PinA.ToString(), *PinB.ToString(), *Response.Message.ToString());
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return true;
}

bool UEditorBridgeGraphTools::DisconnectPin(UBlueprint* Blueprint, FName GraphName, const FString& NodeGuid, FName PinName)
{
	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	UEdGraphPin* Pin = FindPin(FindNode(Graph, NodeGuid), PinName, EGPD_MAX);
	if (!Pin) return false;

	Graph->GetSchema()->BreakPinLinks(*Pin, true);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return true;
}

bool UEditorBridgeGraphTools::DeleteNode(UBlueprint* Blueprint, FName GraphName, const FString& NodeGuid)
{
	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	UEdGraphNode* Node = FindNode(Graph, NodeGuid);
	if (!Node) return false;

	UE_LOG(LogEditorBridge, Log, TEXT("DeleteNode: %s"), *NodeLine(Node));
	FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
	return true;
}

bool UEditorBridgeGraphTools::MoveNode(UBlueprint* Blueprint, FName GraphName, const FString& NodeGuid, int32 X, int32 Y)
{
	UEdGraphNode* Node = FindNode(FindGraph(Blueprint, GraphName), NodeGuid);
	if (!Node) return false;

	Node->Modify();
	Node->NodePosX = X;
	Node->NodePosY = Y;
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return true;
}

bool UEditorBridgeGraphTools::SetPinDefault(UBlueprint* Blueprint, FName GraphName, const FString& NodeGuid, FName PinName, const FString& Value)
{
	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	UEdGraphPin* Pin = FindPin(FindNode(Graph, NodeGuid), PinName, EGPD_Input);
	if (!Pin) return false;

	const UEdGraphSchema* Schema = Graph->GetSchema();
	const FName Category = Pin->PinType.PinCategory;

	if (Category == UEdGraphSchema_K2::PC_Object || Category == UEdGraphSchema_K2::PC_Class ||
		Category == UEdGraphSchema_K2::PC_SoftObject || Category == UEdGraphSchema_K2::PC_SoftClass)
	{
		UObject* Object = Value.IsEmpty() ? nullptr : LoadObject<UObject>(nullptr, *Value);
		if (!Value.IsEmpty() && !Object)
		{
			UE_LOG(LogEditorBridge, Warning, TEXT("SetPinDefault: could not load object '%s'"), *Value);
			return false;
		}
		Schema->TrySetDefaultObject(*Pin, Object);
	}
	else
	{
		Schema->TrySetDefaultValue(*Pin, Value);
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return true;
}

bool UEditorBridgeGraphTools::SetGraphPinType(UBlueprint* Blueprint, FName GraphName, FName PinName, const FString& Category, UObject* SubCategoryObject)
{
	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph) return false;

	// ── Build the pin type ──
	FEdGraphPinType NewType;
	const FString Cat = Category.ToLower();
	if      (Cat == TEXT("object")) NewType.PinCategory = UEdGraphSchema_K2::PC_Object;
	else if (Cat == TEXT("class"))  NewType.PinCategory = UEdGraphSchema_K2::PC_Class;
	else if (Cat == TEXT("bool"))   NewType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	else if (Cat == TEXT("int"))    NewType.PinCategory = UEdGraphSchema_K2::PC_Int;
	else if (Cat == TEXT("float"))  { NewType.PinCategory = UEdGraphSchema_K2::PC_Real; NewType.PinSubCategory = UEdGraphSchema_K2::PC_Double; }
	else if (Cat == TEXT("string")) NewType.PinCategory = UEdGraphSchema_K2::PC_String;
	else if (Cat == TEXT("name"))   NewType.PinCategory = UEdGraphSchema_K2::PC_Name;
	else if (Cat == TEXT("struct")) NewType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	else
	{
		UE_LOG(LogEditorBridge, Warning, TEXT("SetGraphPinType: unknown category '%s'"), *Category);
		return false;
	}
	NewType.PinSubCategoryObject = SubCategoryObject;

	if ((Cat == TEXT("object") || Cat == TEXT("class") || Cat == TEXT("struct")) && !SubCategoryObject)
	{
		UE_LOG(LogEditorBridge, Warning, TEXT("SetGraphPinType: category '%s' needs a SubCategoryObject"), *Category);
		return false;
	}
	if (!FBlueprintEditorUtils::IsPinTypeValid(NewType))
	{
		UE_LOG(LogEditorBridge, Warning, TEXT("SetGraphPinType: invalid pin type"));
		return false;
	}

	// ── Update every tunnel / entry / result node that declares the pin ──
	int32 Changed = 0;
	for (UEdGraphNode* GraphNode : Graph->Nodes)
	{
		UK2Node_EditablePinBase* Node = Cast<UK2Node_EditablePinBase>(GraphNode);
		if (!Node) continue;

		TSharedPtr<FUserPinInfo>* PinInfo = Node->UserDefinedPins.FindByPredicate(
			[PinName](const TSharedPtr<FUserPinInfo>& P) { return P.IsValid() && P->PinName == PinName; });
		if (!PinInfo) continue;

		// Same steps the details panel performs (FBlueprintGraphArgumentLayout::PinInfoChanged)
		Node->Modify();
		(*PinInfo)->PinType = NewType;
		if (!NewType.bIsConst && Node->ShouldUseConstRefParams())
		{
			(*PinInfo)->PinType.bIsConst = NewType.IsArray() || NewType.bIsReference;
		}
		(*PinInfo)->PinDefaultValue.Reset();

		const bool bSaved = Node->bDisableOrphanPinSaving;
		Node->bDisableOrphanPinSaving = true;
		Node->ReconstructNode();
		Node->bDisableOrphanPinSaving = bSaved;

		GetDefault<UEdGraphSchema_K2>()->HandleParameterDefaultValueChanged(Node);
		++Changed;
	}

	if (Changed == 0)
	{
		UE_LOG(LogEditorBridge, Warning, TEXT("SetGraphPinType: no input/output named '%s' in graph '%s'"), *PinName.ToString(), *GraphName.ToString());
		return false;
	}

	// ── Refresh macro instances of this graph so their pins pick up the new type ──
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Other : Graphs)
	{
		if (!Other) continue;
		for (UEdGraphNode* N : Other->Nodes)
		{
			if (UK2Node_MacroInstance* Instance = Cast<UK2Node_MacroInstance>(N))
			{
				if (Instance->GetMacroGraph() == Graph) Instance->ReconstructNode();
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogEditorBridge, Log, TEXT("SetGraphPinType: '%s'.%s -> %s (%d node(s))"),
		*GraphName.ToString(), *PinName.ToString(), *UEdGraphSchema_K2::TypeToText(NewType).ToString(), Changed);
	return true;
}
