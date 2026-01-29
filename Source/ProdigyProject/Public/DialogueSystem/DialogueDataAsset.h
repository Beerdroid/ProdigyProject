// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DialogueSystemTypes.h"
#include "Engine/DataAsset.h"
#include "DialogueDataAsset.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class PRODIGYPROJECT_API UDialogueDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName StartNodeID = "Start";

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FDialogueNode> Nodes;

	bool TryGetNode(FName NodeID, FDialogueNode& OutNode) const;
};
