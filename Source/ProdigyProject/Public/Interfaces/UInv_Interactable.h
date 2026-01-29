// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UInv_Interactable.generated.h"

class APlayerController;
class APawn;

UINTERFACE(BlueprintType)
class PRODIGYPROJECT_API UInv_Interactable : public UInterface
{
	GENERATED_BODY()
};

class PRODIGYPROJECT_API IInv_Interactable
{
	GENERATED_BODY()

public:
	/**
	 * Generic interaction entrypoint for non-item interactables (quest giver, merchant, door, etc.)
	 * Implement this in BP on the actor (recommended) or on a component.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	void Interact(APlayerController* InteractingPC, APawn* InteractingPawn);
};