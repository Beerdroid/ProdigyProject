// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "DialogueEdGraph.generated.h"

/**
 * 
 */
UCLASS()
class PRODIGYPROJECTEDITOR_API UDialogueEdGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	virtual void NotifyGraphChanged() override;



};
