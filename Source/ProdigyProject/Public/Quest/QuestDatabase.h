#pragma once

#include "CoreMinimal.h"
#include "QuestTypes.h"
#include "Engine/DataAsset.h"
#include "QuestDatabase.generated.h"
USTRUCT(BlueprintType)
struct FQuestDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName QuestID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Description;

	// --- Story grouping ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	FName StoryID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	int32 StorySequenceIndex = 0;

	// --- Quest content (single stage) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FQuestObjectiveDef> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAutoComplete = false; // means auto-turn-in on complete

	// Rewards (only granted on turn-in)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FQuestItemReward> FinalItemRewards;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 FinalCurrencyReward = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 FinalXPReward = 0;

	// Gating (optional but recommended)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	TArray<FName> PrerequisiteQuestIDs;
};


UCLASS(BlueprintType)
class PRODIGYPROJECT_API UQuestDatabase : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quests")
	TArray<FQuestDefinition> Quests;

	// -------- C++: no-copy fast lookup --------
	const FQuestDefinition* FindQuestPtr(FName QuestID) const;

	// -------- Blueprint: easy usage --------
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Quests")
	bool TryGetQuest(FName QuestID, FQuestDefinition& OutQuest) const;

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;

private:
	// Transient runtime cache: QuestID -> index in Quests array
	UPROPERTY(Transient)
	mutable TMap<FName, int32> QuestIndexById;

	void RebuildCache() const;
};