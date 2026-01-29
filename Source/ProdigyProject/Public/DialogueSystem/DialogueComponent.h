// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DialogueComponent.generated.h"

class UDialogueDataAsset;
class UDialogueResolverComponent;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueNodeChanged, FName, NodeID);

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Dialogue), meta=(BlueprintSpawnableComponent) )
class PRODIGYPROJECT_API UDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDialogueComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TObjectPtr<UDialogueDataAsset> Dialogue = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	FName StartNodeOverride = NAME_None;

	// Start / reset
	UFUNCTION(BlueprintCallable, Category="Dialogue")
	bool StartDialogue(UDialogueResolverComponent* InResolver);

	// Read current node
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Dialogue")
	bool GetCurrentNode(FDialogueNode& OutNode) const;

	// Get available choices (conditions filtered)
	UFUNCTION(BlueprintCallable, Category="Dialogue")
	TArray<FDialogueChoice> GetAvailableChoices() const;

	// Pick a choice by index in GetAvailableChoices() result
	UFUNCTION(BlueprintCallable, Category="Dialogue")
	bool Choose(int32 AvailableChoiceIndex);

	UPROPERTY(BlueprintAssignable, Category="Dialogue")
	FOnDialogueNodeChanged OnNodeChanged;

private:
	UPROPERTY()
	TObjectPtr<UDialogueResolverComponent> Resolver = nullptr;

	UPROPERTY()
	FName CurrentNodeID = NAME_None;

	bool SetNode(FName NodeID);
		
};
