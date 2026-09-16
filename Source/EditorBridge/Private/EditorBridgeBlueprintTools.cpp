#include "EditorBridgeBlueprintTools.h"
#include "EditorBridgeModule.h"

#include "Components/StaticMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Variable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"
#include "UObject/ReferencerFinder.h"
#include "Serialization/FindReferencersArchive.h"


// ─── VARIABLES ────────────────────────────────────────────────────────
int32 UEditorBridgeBlueprintTools::RenameVariableReferences(UBlueprint* Blueprint, UClass* VariableClass, FName OldName, FName NewName)
{
	if (!Blueprint || !VariableClass || OldName.IsNone() || NewName.IsNone())
	{
		UE_LOG(LogEditorBridge, Error, TEXT("RenameVariableReferences: invalid arguments"));
		return 0;
	}

	int32 Renamed = 0;

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node);
			if (!VarNode || VarNode->GetVarName() != OldName) continue;

			// Same rule the engine applies on rename: the node must point at
			// VariableClass or one of its subclasses.
			UClass* NodeRefClass = VarNode->VariableReference.GetMemberParentClass(Blueprint->GeneratedClass);
			if (!NodeRefClass || !NodeRefClass->IsChildOf(VariableClass)) continue;

			VarNode->HandleVariableRenamed(Blueprint, VariableClass, Graph, OldName, NewName);

			if (VarNode->GetVarName() == NewName)
			{
				++Renamed;
				UE_LOG(LogEditorBridge, Log, TEXT("  renamed %s in graph '%s'"),
					*VarNode->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Graph->GetName());
			}
		}
	}

	if (Renamed > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}

	UE_LOG(LogEditorBridge, Log, TEXT("RenameVariableReferences: %d node(s) in '%s' now reference '%s'"),
		Renamed, *Blueprint->GetName(), *NewName.ToString());
	return Renamed;
}

int32 UEditorBridgeBlueprintTools::RetargetMemberReferences(UBlueprint* Blueprint, UClass* FromClass, UClass* ToClass)
{
	if (!Blueprint || !FromClass || !ToClass) return 0;

	int32 Changed = 0;
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node))
			{
				FMemberReference& Ref = VarNode->VariableReference;
				if (Ref.IsSelfContext() || Ref.GetMemberParentClass() != FromClass) continue;
				if (!FindFProperty<FProperty>(ToClass, Ref.GetMemberName())) continue;

				VarNode->Modify();
				Ref.SetExternalMember(Ref.GetMemberName(), ToClass);
				VarNode->ReconstructNode();
				++Changed;
			}
			else if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				FMemberReference& Ref = CallNode->FunctionReference;
				if (Ref.IsSelfContext() || Ref.GetMemberParentClass() != FromClass) continue;
				if (!ToClass->FindFunctionByName(Ref.GetMemberName())) continue;

				CallNode->Modify();
				Ref.SetExternalMember(Ref.GetMemberName(), ToClass);
				CallNode->ReconstructNode();
				++Changed;
			}
		}
	}

	if (Changed > 0) FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogEditorBridge, Log, TEXT("RetargetMemberReferences: %d node(s) in '%s' moved from %s to %s"),
		Changed, *Blueprint->GetName(), *FromClass->GetName(), *ToClass->GetName());
	return Changed;
}

bool UEditorBridgeBlueprintTools::RemoveMemberVariable(UBlueprint* Blueprint, FName VariableName)
{
	if (!Blueprint) return false;

	if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableName) == INDEX_NONE)
	{
		UE_LOG(LogEditorBridge, Warning, TEXT("RemoveMemberVariable: '%s' is not declared by '%s'"),
			*VariableName.ToString(), *Blueprint->GetName());
		return false;
	}

	FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VariableName);
	UE_LOG(LogEditorBridge, Log, TEXT("RemoveMemberVariable: removed '%s' from '%s'"),
		*VariableName.ToString(), *Blueprint->GetName());
	return true;
}

TArray<FName> UEditorBridgeBlueprintTools::ListMemberVariables(UBlueprint* Blueprint)
{
	TArray<FName> Names;
	if (!Blueprint) return Names;

	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		Names.Add(Var.VarName);
	}
	return Names;
}

TArray<FString> UEditorBridgeBlueprintTools::ListMemberVariableTypes(UBlueprint* Blueprint)
{
	TArray<FString> Lines;
	if (!Blueprint) return Lines;

	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		Lines.Add(FString::Printf(TEXT("%s : %s"), *Var.VarName.ToString(), *UEdGraphSchema_K2::TypeToText(Var.VarType).ToString()));
	}
	return Lines;
}

bool UEditorBridgeBlueprintTools::ChangeMemberVariableType(UBlueprint* Blueprint, FName VariableName, const FString& Category, UObject* SubCategoryObject)
{
	if (!Blueprint) return false;
	if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableName) == INDEX_NONE)
	{
		UE_LOG(LogEditorBridge, Warning, TEXT("ChangeMemberVariableType: '%s' is not declared by '%s'"), *VariableName.ToString(), *Blueprint->GetName());
		return false;
	}

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
		UE_LOG(LogEditorBridge, Warning, TEXT("ChangeMemberVariableType: unknown category '%s'"), *Category);
		return false;
	}
	NewType.PinSubCategoryObject = SubCategoryObject;
	if (!FBlueprintEditorUtils::IsPinTypeValid(NewType)) return false;

	FBlueprintEditorUtils::ChangeMemberVariableType(Blueprint, VariableName, NewType);
	UE_LOG(LogEditorBridge, Log, TEXT("ChangeMemberVariableType: '%s'.%s -> %s"),
		*Blueprint->GetName(), *VariableName.ToString(), *UEdGraphSchema_K2::TypeToText(NewType).ToString());
	return true;
}

TArray<FString> UEditorBridgeBlueprintTools::ListLocalVariables(UBlueprint* Blueprint)
{
	TArray<FString> Lines;
	if (!Blueprint) return Lines;

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			{
				for (const FBPVariableDescription& Var : Entry->LocalVariables)
				{
					Lines.Add(FString::Printf(TEXT("%s | %s : %s"), *Graph->GetName(), *Var.VarName.ToString(),
						*UEdGraphSchema_K2::TypeToText(Var.VarType).ToString()));
				}
			}
		}
	}
	return Lines;
}

TArray<FString> UEditorBridgeBlueprintTools::ListVariableReferences(UBlueprint* Blueprint, FName VariableName)
{
	TArray<FString> Lines;
	if (!Blueprint) return Lines;

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node);
			if (!VarNode) continue;
			if (!VariableName.IsNone() && VarNode->GetVarName() != VariableName) continue;

			UClass* NodeRefClass = VarNode->VariableReference.GetMemberParentClass(Blueprint->GeneratedClass);
			const FProperty* Property = VarNode->GetPropertyForVariable();

			Lines.Add(FString::Printf(TEXT("%s | %s | %s::%s%s"),
				*Graph->GetName(),
				*VarNode->GetNodeTitle(ENodeTitleType::ListView).ToString(),
				NodeRefClass ? *NodeRefClass->GetName() : TEXT("<none>"),
				*VarNode->GetVarName().ToString(),
				Property ? TEXT("") : TEXT("  (UNRESOLVED)")));
		}
	}
	return Lines;
}

// ─── NODES / COMPILE ──────────────────────────────────────────────────
void UEditorBridgeBlueprintTools::RefreshAllNodes(UBlueprint* Blueprint)
{
	if (!Blueprint) return;
	FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
	UE_LOG(LogEditorBridge, Log, TEXT("RefreshAllNodes: '%s'"), *Blueprint->GetName());
}

static FString PinTypeName(const UEdGraphPin* Pin)
{
	if (!Pin) return TEXT("?");
	const UObject* SubObj = Pin->PinType.PinSubCategoryObject.Get();
	return SubObj ? SubObj->GetName() : Pin->PinType.PinCategory.ToString();
}

static FString PinDesc(const UEdGraphPin* Pin)
{
	return FString::Printf(TEXT("%s.%s"),
		Pin && Pin->GetOwningNode() ? *Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("?"),
		Pin ? *Pin->PinName.ToString() : TEXT("?"));
}

int32 UEditorBridgeBlueprintTools::RewireOrphanedPins(UBlueprint* Blueprint, TArray<FString>& OutUnfixable)
{
	OutUnfixable.Reset();
	if (!Blueprint) return 0;

	int32 Removed = 0;

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;
		const UEdGraphSchema* Schema = Graph->GetSchema();

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			TArray<UEdGraphPin*> Orphans;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->bOrphanedPin) Orphans.Add(Pin);
			}
			if (Orphans.Num() == 0) continue;

			Node->Modify();

			for (UEdGraphPin* Orphan : Orphans)
			{
				UEdGraphPin* Replacement = nullptr;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin && !Pin->bOrphanedPin && Pin->PinName == Orphan->PinName && Pin->Direction == Orphan->Direction)
					{
						Replacement = Pin;
						break;
					}
				}

				const FString NodeName = FString::Printf(TEXT("%s | %s"), *Graph->GetName(), *PinDesc(Orphan));

				if (!Replacement)
				{
					OutUnfixable.Add(NodeName + TEXT("  -> no live pin with that name, orphan left in place"));
					continue;
				}

				// Move each link to the live pin; the schema decides if the types are compatible
				TArray<UEdGraphPin*> Links = Orphan->LinkedTo;
				for (UEdGraphPin* Other : Links)
				{
					if (!Other) continue;
					Other->Modify();
					Orphan->BreakLinkTo(Other);

					if (!Schema || !Schema->TryCreateConnection(Replacement, Other))
					{
						OutUnfixable.Add(FString::Printf(TEXT("%s  -x->  %s  (%s vs %s)"),
							*NodeName, *PinDesc(Other), *PinTypeName(Replacement), *PinTypeName(Other)));
					}
				}

				Node->RemovePin(Orphan);
				++Removed;
			}
		}
	}

	if (Removed > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}

	UE_LOG(LogEditorBridge, Log, TEXT("RewireOrphanedPins: '%s' -> %d orphan(s) removed, %d link(s) could not be re-created"),
		*Blueprint->GetName(), Removed, OutUnfixable.Num());
	return Removed;
}

TArray<FString> UEditorBridgeBlueprintTools::CompileBlueprintWithLog(UBlueprint* Blueprint, bool& bHasErrors)
{
	TArray<FString> Lines;
	bHasErrors = false;
	if (!Blueprint) return Lines;

	FCompilerResultsLog Results;
	Results.bSilentMode = true;   // we report the messages ourselves

	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);

	for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages)
	{
		const TCHAR* Severity = TEXT("Info");
		switch (Message->GetSeverity())
		{
			case EMessageSeverity::Error:   Severity = TEXT("Error");   break;
			case EMessageSeverity::Warning: Severity = TEXT("Warning"); break;
			default: break;
		}
		Lines.Add(FString::Printf(TEXT("[%s] %s"), Severity, *Message->ToText().ToString()));
	}

	bHasErrors = Results.NumErrors > 0;
	UE_LOG(LogEditorBridge, Log, TEXT("CompileBlueprintWithLog: '%s' -> %d error(s), %d warning(s)"),
		*Blueprint->GetName(), Results.NumErrors, Results.NumWarnings);
	return Lines;
}

// ─── REFERENCES ───────────────────────────────────────────────────────
TArray<FString> UEditorBridgeBlueprintTools::FindReferencers(UBlueprint* Blueprint, const FString& TargetPath)
{
	TArray<FString> Lines;
	if (!Blueprint) return Lines;

	UObject* Root = StaticLoadObject(UObject::StaticClass(), nullptr, *TargetPath);
	if (!Root)
	{
		Lines.Add(TEXT("target not found: ") + TargetPath);
		return Lines;
	}

	TArray<UObject*> Targets = { Root };
	if (UBlueprint* TargetBP = Cast<UBlueprint>(Root))
	{
		if (TargetBP->GeneratedClass)
		{
			Targets.Add(TargetBP->GeneratedClass);
			Targets.Add(TargetBP->GeneratedClass->GetDefaultObject());
		}
		if (TargetBP->SkeletonGeneratedClass) Targets.Add(TargetBP->SkeletonGeneratedClass);
	}
	else if (UClass* TargetClass = Cast<UClass>(Root))
	{
		Targets.Add(TargetClass->GetDefaultObject());
	}

	UPackage* Package = Blueprint->GetOutermost();
	TArray<UObject*> Referencers = FReferencerFinder::GetAllReferencers(Targets, nullptr);

	for (UObject* Referencer : Referencers)
	{
		if (!Referencer || Referencer->GetOutermost() != Package) continue;

		FFindReferencersArchive Ar(Referencer, Targets);
		for (UObject* Target : Targets)
		{
			TArray<FProperty*> Props;
			if (Ar.GetReferenceCount(Target, &Props) == 0) continue;

			FString PropNames;
			for (const FProperty* P : Props)
			{
				if (!PropNames.IsEmpty()) PropNames += TEXT(", ");
				PropNames += P ? P->GetName() : TEXT("?");
			}
			Lines.Add(FString::Printf(TEXT("%s (%s) -> %s | %s"),
				*Referencer->GetPathName(), *Referencer->GetClass()->GetName(), *Target->GetName(),
				PropNames.IsEmpty() ? TEXT("? (script / non-property)") : *PropNames));
		}
	}
	return Lines;
}

// ─── CLASS ────────────────────────────────────────────────────────────
UClass* UEditorBridgeBlueprintTools::GetParentClass(UBlueprint* Blueprint)
{
	return Blueprint ? Blueprint->ParentClass.Get() : nullptr;
}

// ─── COMPONENTS ───────────────────────────────────────────────────────
static FString DescribeTemplate(const UActorComponent* Template)
{
	if (!Template) return TEXT("<null template>");

	FString Line = Template->GetClass()->GetName();
	if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Template))
	{
		const UStaticMesh* Mesh = SMC->GetStaticMesh();
		Line += TEXT(" | ");
		Line += Mesh ? Mesh->GetPathName() : TEXT("None");
	}
	return Line;
}

TArray<FString> UEditorBridgeBlueprintTools::ListComponents(UBlueprint* Blueprint)
{
	TArray<FString> Lines;
	if (!Blueprint) return Lines;

	// Own components
	if (Blueprint->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (!Node) continue;
			Lines.Add(FString::Printf(TEXT("%s | %s"),
				*Node->GetVariableName().ToString(), *DescribeTemplate(Node->ComponentTemplate)));
		}
	}

	// Inherited components (walk the Blueprint parent chain) + this BP's overrides
	const UInheritableComponentHandler* ICH = Blueprint->GetInheritableComponentHandler(false);
	for (UBlueprintGeneratedClass* ParentBPGC = Cast<UBlueprintGeneratedClass>(Blueprint->ParentClass);
		 ParentBPGC; ParentBPGC = Cast<UBlueprintGeneratedClass>(ParentBPGC->GetSuperClass()))
	{
		if (!ParentBPGC->SimpleConstructionScript) continue;

		for (const USCS_Node* Node : ParentBPGC->SimpleConstructionScript->GetAllNodes())
		{
			if (!Node) continue;
			const UActorComponent* Override = ICH ? ICH->GetOverridenComponentTemplate(FComponentKey(Node)) : nullptr;
			Lines.Add(FString::Printf(TEXT("%s | %s | from %s%s"),
				*Node->GetVariableName().ToString(),
				*DescribeTemplate(Override ? Override : Node->ComponentTemplate.Get()),
				*ParentBPGC->GetName(),
				Override ? TEXT(" (overridden here)") : TEXT(" (inherited)")));
		}
	}
	return Lines;
}

bool UEditorBridgeBlueprintTools::SetComponentStaticMesh(UBlueprint* Blueprint, FName ComponentName, UStaticMesh* NewMesh)
{
	if (!Blueprint) return false;

	UStaticMeshComponent* Target = nullptr;

	// 1. Component declared by this Blueprint
	if (Blueprint->SimpleConstructionScript)
	{
		if (USCS_Node* Node = Blueprint->SimpleConstructionScript->FindSCSNode(ComponentName))
		{
			Target = Cast<UStaticMeshComponent>(Node->ComponentTemplate);
		}
	}

	// 2. Component inherited from a parent Blueprint: create/get the override template
	if (!Target)
	{
		for (UBlueprintGeneratedClass* ParentBPGC = Cast<UBlueprintGeneratedClass>(Blueprint->ParentClass);
			 ParentBPGC && !Target; ParentBPGC = Cast<UBlueprintGeneratedClass>(ParentBPGC->GetSuperClass()))
		{
			if (!ParentBPGC->SimpleConstructionScript) continue;
			USCS_Node* Node = ParentBPGC->SimpleConstructionScript->FindSCSNode(ComponentName);
			if (!Node || !Cast<UStaticMeshComponent>(Node->ComponentTemplate)) continue;

			UInheritableComponentHandler* ICH = Blueprint->GetInheritableComponentHandler(true);
			Target = Cast<UStaticMeshComponent>(ICH->CreateOverridenComponentTemplate(FComponentKey(Node)));
		}
	}

	if (!Target)
	{
		UE_LOG(LogEditorBridge, Warning, TEXT("SetComponentStaticMesh: no StaticMeshComponent '%s' in '%s' or its parents"),
			*ComponentName.ToString(), *Blueprint->GetName());
		return false;
	}

	Target->Modify();
	Target->SetStaticMesh(NewMesh);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogEditorBridge, Log, TEXT("SetComponentStaticMesh: '%s'.%s = %s"),
		*Blueprint->GetName(), *ComponentName.ToString(), NewMesh ? *NewMesh->GetPathName() : TEXT("None"));
	return true;
}
