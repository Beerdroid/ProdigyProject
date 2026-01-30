// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DialogueSystemTypes.h"
#include "Engine/DataAsset.h"
#include "DialogueDataAsset.generated.h"

class UEdGraph;

UENUM(BlueprintType)
enum class EDialogueAuthoringMode : uint8
{
	ManualNodes,
	Graph
};

UCLASS(BlueprintType)
class PRODIGYPROJECT_API UDialogueDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// Entry node id used by DialogueComponent when starting
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	FName StartNodeID = "Start";

	// Manual authoring (optional). Used only when AuthoringMode == ManualNodes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(EditCondition="AuthoringMode==EDialogueAuthoringMode::ManualNodes"))
	TArray<FDialogueNode> Nodes;

	// Derived runtime data (ALWAYS used by runtime lookup).
	// In editor it is rebuilt from either Nodes (manual) or EditorGraph (graph).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dialogue")
	TMap<FName, FDialogueNode> RuntimeNodes;

	// Which authoring source to bake from
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	EDialogueAuthoringMode AuthoringMode = EDialogueAuthoringMode::Graph;

	// Runtime lookup API used by UDialogueComponent
	UFUNCTION(BlueprintCallable, Category="Dialogue")
	bool TryGetNode(FName NodeID, FDialogueNode& OutNode) const;

	UFUNCTION(CallInEditor, Category="Dialogue|Editor")
	void Editor_Compile();

#if WITH_EDITORONLY_DATA
	// Editor graph (authored with SGraphEditor). Stripped in cooked builds.
	UPROPERTY()
	TObjectPtr<UEdGraph> EditorGraph = nullptr;
#endif

#if WITH_EDITOR
	// Rebuild RuntimeNodes from selected authoring mode.
	void RebuildRuntimeData();

	// Manual: Nodes array -> RuntimeNodes map.
	void RebuildRuntimeDataFromManualNodes();

	// Graph: EditorGraph -> RuntimeNodes map.
	// NOTE: Implementation MUST live in the Editor module (ProdigyProjectEditor),
	// because it depends on UDialogueGraphNode and GraphEditor types.
	void RebuildRuntimeDataFromGraph();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
#endif
};