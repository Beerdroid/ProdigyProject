#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "QuestEventComponent.generated.h"

/**
 * Attach ONLY to quest-relevant actors/volumes.
 * Provides a semantic tag that QuestLog can match against objective TargetTag.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Quest), meta=(BlueprintSpawnableComponent))
class PRODIGYPROJECT_API UQuestEventComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuestEventComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FName QuestID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FName ObjectiveID = NAME_None;

	/** Semantic quest target tag, e.g. QuestTarget.Interact.Blacksmith or QuestTarget.Location.TownSquare */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FGameplayTag QuestTargetTag;

	FORCEINLINE bool HasAnyQuestTags() const { return QuestTargetTag.IsValid(); }
	FORCEINLINE FGameplayTag GetPrimaryQuestTag() const { return QuestTargetTag; }
};
