// Copyright Jeenja

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuestGiverComponent.generated.h"


UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PRODIGYPROJECT_API UQuestGiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UQuestGiverComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quests")
	TArray<FName> Quests;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Quests")
	const TArray<FName>& GetOfferedQuests() const { return Quests; }
};
