#include "FDialogueAssetEditorToolkit.h"

#include "DialogueDataAsset_GraphBake.h"
#include "DialogueSystem/DialogueDataAsset.h"        // runtime asset
#include "DialogueEdGraph.h"
#include "DialogueEdGraphSchema.h"
#include "DialogueGraphNode.h"

#include "ToolMenus.h"
#include "EdGraph/EdGraph.h"
#include "GraphEditor.h"
#include "PropertyEditorModule.h"
#include "Framework/Commands/GenericCommands.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"

const FName FDialogueAssetEditorToolkit::GraphTabId(TEXT("DialogueEditor_Graph"));
const FName FDialogueAssetEditorToolkit::DetailsTabId(TEXT("DialogueEditor_Details"));

void FDialogueAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(GraphTabId, FOnSpawnTab::CreateRaw(this, &FDialogueAssetEditorToolkit::SpawnGraphTab))
		.SetDisplayName(FText::FromString(TEXT("Graph")))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateRaw(this, &FDialogueAssetEditorToolkit::SpawnDetailsTab))
		.SetDisplayName(FText::FromString(TEXT("Details")))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FDialogueAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(GraphTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);

	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

void FDialogueAssetEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Asset);
}

void FDialogueAssetEditorToolkit::InitDialogueEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UDialogueDataAsset* InAsset)
{
	Asset = InAsset;
	EnsureGraphExists(); // create EditorGraph if missing

	BindGraphCommands();


	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("DialogueEditor_Layout_v3")
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Horizontal)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.75f)
			->AddTab(GraphTabId, ETabState::OpenedTab)
			->SetHideTabWell(true)
		)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.25f)
			->AddTab(DetailsTabId, ETabState::OpenedTab)
			->SetHideTabWell(true)
		)
	);

	InitAssetEditor(Mode, InitToolkitHost, GetToolkitFName(), Layout, true, true, InAsset);

	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

void FDialogueAssetEditorToolkit::BindGraphCommands()
{
	if (GraphCommands.IsValid())
	{
		return;
	}

	GraphCommands = MakeShared<FUICommandList>();

	GraphCommands->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateRaw(this, &FDialogueAssetEditorToolkit::DeleteSelectedGraphNodes),
		FCanExecuteAction::CreateRaw(this, &FDialogueAssetEditorToolkit::CanDeleteSelectedGraphNodes)
	);
}

bool FDialogueAssetEditorToolkit::CanDeleteSelectedGraphNodes() const
{
	return GraphEditor.IsValid() && GraphEditor->GetSelectedNodes().Num() > 0;
}

void FDialogueAssetEditorToolkit::DeleteSelectedGraphNodes()
{
	if (!GraphEditor.IsValid())
	{
		return;
	}

	UEdGraph* Graph = GraphEditor->GetCurrentGraph();
	if (!Graph)
	{
		return;
	}

	const FScopedTransaction Tx(NSLOCTEXT("DialogueEditor", "DeleteDialogueNodes", "Delete Dialogue Nodes"));
	Graph->Modify();

	// Copy selection because we will mutate the graph
	const TSet<UObject*> Selected = GraphEditor->GetSelectedNodes();

	for (UObject* Obj : Selected)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(Obj);
		if (!Node) continue;

		// Respect per-node rule
		if (!Node->CanUserDeleteNode())
		{
			continue;
		}

		Node->Modify();
		Node->BreakAllNodeLinks();
		Node->DestroyNode();
	}

	GraphEditor->ClearSelectionSet();
	GraphEditor->NotifyGraphChanged();
}

void FDialogueAssetEditorToolkit::EnsureGraphEditorWidget()
{
	if (GraphEditor.IsValid()) return;

	check(Asset);
	check(Asset->EditorGraph);

	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = FText::FromString(TEXT("Dialogue"));

	SGraphEditor::FGraphEditorEvents Events;
	Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateRaw(this, &FDialogueAssetEditorToolkit::OnSelectedNodesChanged);

	GraphEditor = SNew(SGraphEditor)
		.AdditionalCommands(GraphCommands)
		.Appearance(Appearance)
		.IsEditable(true)
		.GraphToEdit(Asset->EditorGraph)
		.GraphEvents(Events)
		.ShowGraphStateOverlay(false);
}

TSharedRef<SDockTab> FDialogueAssetEditorToolkit::SpawnGraphTab(const FSpawnTabArgs& Args)
{
	EnsureGraphEditorWidget();

	return SNew(SDockTab)
		.Label(FText::FromString(TEXT("Graph")))
		[
			GraphEditor.ToSharedRef()
		];
}

TSharedRef<SDockTab> FDialogueAssetEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
	// Create details view
	FPropertyEditorModule& PropModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs ViewArgs;
	ViewArgs.bHideSelectionTip = true;
	ViewArgs.bLockable = false;
	ViewArgs.bUpdatesFromSelection = false;
	ViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	DetailsView = PropModule.CreateDetailView(ViewArgs);
	DetailsView->OnFinishedChangingProperties().AddRaw(this, &FDialogueAssetEditorToolkit::OnFinishedChangingProperties);

	// Default show asset itself
	DetailsView->SetObject(Asset.Get());

	return SNew(SDockTab)
		.Label(FText::FromString(TEXT("Details")))
		[
			DetailsView.ToSharedRef()
		];
}

void FDialogueAssetEditorToolkit::EnsureGraphExists()
{
	if (!Asset) return;

#if WITH_EDITORONLY_DATA
	if (!Asset->EditorGraph)
	{
		Asset->Modify();

		UDialogueEdGraph* Graph = NewObject<UDialogueEdGraph>(Asset.Get(), TEXT("DialogueEditorGraph"), RF_Transactional);
		Graph->Schema = UDialogueEdGraphSchema::StaticClass();

		Asset->EditorGraph = Graph;
		Graph->bAllowDeletion = false;

		// Optional: create one starter node
		UDialogueGraphNode* StartNode = NewObject<UDialogueGraphNode>(Graph);
		StartNode->NodeID = Asset->StartNodeID.IsNone() ? FName("start") : Asset->StartNodeID;
		StartNode->Choices.AddDefaulted();
		Graph->AddNode(StartNode, true, true);
		Graph->bAllowDeletion = true;
		StartNode->NodePosX = 0;
		StartNode->NodePosY = 0;
		StartNode->AllocateDefaultPins();
	}
	else
	{
		UEdGraph* Graph = Asset->EditorGraph;

		// Fix-up schema if asset was created with old code / copied / etc.
		if (!Graph->Schema || Graph->Schema != UDialogueEdGraphSchema::StaticClass())
		{
			Graph->Modify();
			Graph->Schema = UDialogueEdGraphSchema::StaticClass();
		}
	}
#endif
}

void FDialogueAssetEditorToolkit::CompileDialogue()
{
	if (!Asset) return;

	Asset->Modify();

#if WITH_EDITOR
	UE_LOG(LogTemp, Warning, TEXT("CompileDialogue: Asset=%s RuntimeNodes(before)=%d"),
		*GetPathNameSafe(Asset.Get()), Asset->RuntimeNodes.Num());

	FDialogueGraphBakerEditor::Bake(Asset.Get());

	UE_LOG(LogTemp, Warning, TEXT("CompileDialogue: RuntimeNodes(after)=%d"),
		Asset->RuntimeNodes.Num());
#endif

	Asset->MarkPackageDirty();

	// Refresh details panel
	if (DetailsView.IsValid())
	{
		UObject* Current = DetailsView->GetSelectedObjects().Num() > 0 ? DetailsView->GetSelectedObjects()[0].Get() : nullptr;
		if (!Current || Current == Asset.Get())
		{
			DetailsView->SetObject(Asset.Get(), true);
		}
	}
}

void FDialogueAssetEditorToolkit::RefreshGraph()
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->NotifyGraphChanged();
	}
}

void FDialogueAssetEditorToolkit::ExtendToolbar()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus) return;

	// This is the toolbar menu for your toolkit instance
	const FName ToolbarMenuName = GetToolMenuToolbarName();
	UToolMenu* ToolbarMenu = ToolMenus->ExtendMenu(ToolbarMenuName);
	if (!ToolbarMenu) return;

	FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Dialogue");
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(
		"Dialogue_Compile",
		FUIAction(FExecuteAction::CreateRaw(this, &FDialogueAssetEditorToolkit::CompileDialogue)),
		FText::FromString(TEXT("Compile")),
		FText::FromString(TEXT("Bake graph/manual data into RuntimeNodes")),
		FSlateIcon() // you can add an icon later
	));
}

void FDialogueAssetEditorToolkit::OnSelectedNodesChanged(const TSet<UObject*>& NewSelection)
{
	if (!DetailsView.IsValid())
	{
		return;
	}

	// Show selected node properties; if none selected -> show asset
	if (NewSelection.Num() == 1)
	{
		for (UObject* Obj : NewSelection)
		{
			DetailsView->SetObject(Obj);
			return;
		}
	}

	DetailsView->SetObject(Asset.Get());
}

void FDialogueAssetEditorToolkit::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	const int32 Num = PropertyChangedEvent.GetNumObjectsBeingEdited();
	UE_LOG(LogTemp, Warning, TEXT("FinishedChangingProperties: NumObjects=%d Prop=%s Member=%s"),
		Num,
		*PropertyChangedEvent.GetPropertyName().ToString(),
		*PropertyChangedEvent.GetMemberPropertyName().ToString());

	UDialogueGraphNode* NodeToRebuild = nullptr;

	// 1) Try objects from the event
	for (int32 i = 0; i < Num; ++i)
	{
		const UObject* ObjConst = PropertyChangedEvent.GetObjectBeingEdited(i);
		UE_LOG(LogTemp, Warning, TEXT("  EditedObj[%d]=%s (%s)"),
			i, *GetNameSafe(ObjConst), *GetNameSafe(ObjConst ? ObjConst->GetClass() : nullptr));

		if (UDialogueGraphNode* AsNode = Cast<UDialogueGraphNode>(const_cast<UObject*>(ObjConst)))
		{
			NodeToRebuild = AsNode;
			break;
		}
	}

	// 2) Fallback: use currently displayed object in DetailsView (often more reliable)
	if (!NodeToRebuild && DetailsView.IsValid())
	{
		TArray<TWeakObjectPtr<UObject>> Selected = DetailsView->GetSelectedObjects();

		for (const TWeakObjectPtr<UObject>& W : Selected)
		{
			UObject* Obj = W.Get();
			UE_LOG(LogTemp, Warning, TEXT("  DetailsSelected=%s (%s)"),
				*GetNameSafe(Obj), *GetNameSafe(Obj ? Obj->GetClass() : nullptr));

			if (UDialogueGraphNode* AsNode = Cast<UDialogueGraphNode>(Obj))
			{
				NodeToRebuild = AsNode;
				break;
			}
		}
	}

	if (!NodeToRebuild)
	{
		UE_LOG(LogTemp, Warning, TEXT("  No DialogueGraphNode found to rebuild."));
		return;
	}

	UEdGraph* Graph = NodeToRebuild->GetGraph();
	if (!Graph)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Node has no graph."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("  Rebuilding node %s: Choices=%d PinsBefore=%d"),
		*GetNameSafe(NodeToRebuild),
		NodeToRebuild->Choices.Num(),
		NodeToRebuild->Pins.Num());

	Graph->Modify();
	NodeToRebuild->Modify();

	// This MUST cause AllocateDefaultPins to run
	NodeToRebuild->RebuildPinsFromChoices();

	UE_LOG(LogTemp, Warning, TEXT("  After Reconstruct: PinsAfter=%d"), NodeToRebuild->Pins.Num());

	Graph->NotifyGraphChanged();

	if (GraphEditor.IsValid())
	{
		GraphEditor->NotifyGraphChanged();
	}
}
