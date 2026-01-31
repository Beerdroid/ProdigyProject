// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DialogueResolverComponent.generated.h"


class UQuestLogComponent;
class UDialogueFactsComponent;

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Dialogue), meta=(BlueprintSpawnableComponent) )
class PRODIGYPROJECT_API UDialogueResolverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDialogueResolverComponent();

	// Evaluate one condition
	UFUNCTION(BlueprintCallable, Category="Dialogue|Resolver")
	bool EvaluateCondition(const FDialogueCondition& C) const;

	// Evaluate all (AND)
	UFUNCTION(BlueprintCallable, Category="Dialogue|Resolver")
	bool EvaluateAllConditions(const TArray<FDialogueCondition>& Conditions) const;

	// Apply effects in order
	UFUNCTION(BlueprintCallable, Category="Dialogue|Resolver")
	void ApplyEffects(const TArray<FDialogueEffect>& Effects);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UDialogueFactsComponent> Facts = nullptr;

	UPROPERTY()
	TObjectPtr<UQuestLogComponent> QuestLog = nullptr;

	void CacheFromOwner();


		
};
