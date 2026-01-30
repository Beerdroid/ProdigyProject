#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkitHost.h"

class UDialogueDataAsset;
class SGraphEditor;
class IDetailsView;

class PRODIGYPROJECTEDITOR_API FDialogueAssetEditorToolkit : public FAssetEditorToolkit, public FGCObject
{
public:
	void InitDialogueEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UDialogueDataAsset* InAsset);

	// FAssetEditorToolkit
	virtual FName GetToolkitFName() const override { return "DialogueAssetEditor"; }
	virtual FText GetBaseToolkitName() const override { return FText::FromString(TEXT("Dialogue Editor")); }
	virtual FString GetWorldCentricTabPrefix() const override { return TEXT("Dialogue"); }
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor(0.15f, 0.15f, 0.2f, 1.f); }

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FDialogueAssetEditorToolkit"); }

	TSharedPtr<FUICommandList> GraphCommands;
	void BindGraphCommands();

	bool CanDeleteSelectedGraphNodes() const;
	void DeleteSelectedGraphNodes();

private:
	// Tabs
	void EnsureGraphEditorWidget();
	TSharedRef<SDockTab> SpawnGraphTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);

	// Graph widgets
	void OnSelectedNodesChanged(const TSet<UObject*>& NewSelection);

	void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);

	// Helpers
	void EnsureGraphExists();

	void CompileDialogue(); 

	TObjectPtr<UDialogueDataAsset> Asset = nullptr;

	TSharedPtr<SGraphEditor> GraphEditor;
	TSharedPtr<IDetailsView> DetailsView;

	static const FName GraphTabId;
	static const FName DetailsTabId;

	void RefreshGraph();

	void ExtendToolbar();

	TSharedPtr<FUICommandList> CommandList;
};


