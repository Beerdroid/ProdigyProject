// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "DialogueFactsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueFactChanged, FGameplayTag, Tag, int32, NewValue);


UCLASS(BlueprintType, Blueprintable, ClassGroup=(Dialogue), meta=(BlueprintSpawnableComponent))
class PRODIGYPROJECT_API UDialogueFactsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDialogueFactsComponent();

	// Tag exists => present in map (even value 0 is "exists" if stored)
	UFUNCTION(BlueprintCallable, Category="Dialogue|Facts")
	bool HasTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category="Dialogue|Facts")
	int32 GetTagValue(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category="Dialogue|Facts")
	void AddTag(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category="Dialogue|Facts")
	void RemoveTag(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category="Dialogue|Facts")
	void SetTagValue(FGameplayTag Tag, int32 Value);

	UFUNCTION(BlueprintCallable, Category="Dialogue|Facts")
	void AddTagValue(FGameplayTag Tag, int32 Delta);

	UPROPERTY(BlueprintAssignable, Category="Dialogue|Facts")
	FOnDialogueFactChanged OnFactChanged;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue|Facts")
	TMap<FGameplayTag, int32> Facts;

		
};
