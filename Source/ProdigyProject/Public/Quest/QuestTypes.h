#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "GameplayTagContainer.h"
#include "QuestTypes.generated.h"

UENUM(BlueprintType)
enum class EQuestObjectiveType : uint8
{
	Collect,
	Kill,
	Talk,
	Interact
};




USTRUCT(BlueprintType)
struct FInventoryDelta
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName ItemID = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	int32 DeltaQuantity = 0;

	// Optional: who/what caused it (bench, container, etc.)
	UPROPERTY(BlueprintReadOnly)
	UObject* Context = nullptr;
};

// -------------------- Quest Definition Data --------------------

USTRUCT(BlueprintType)
struct FQuestItemReward
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName ItemID = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 Quantity = 0;
};

USTRUCT(BlueprintType)
struct FQuestObjectiveDef
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName ObjectiveID = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EQuestObjectiveType Type = EQuestObjectiveType::Collect;

	// Collect
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName ItemID = NAME_None;

	// Kill / Interact / Talk
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag TargetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 RequiredQuantity = 1;
};


// -------------------- Quest Runtime (Replicated) --------------------

USTRUCT(BlueprintType)
struct FQuestObjectiveProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName ObjectiveID = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	int32 Progress = 0;
};

USTRUCT(BlueprintType)
struct FQuestRuntimeState : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName QuestID = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	bool bIsTracked = false;

	UPROPERTY(BlueprintReadOnly)
	bool bIsCompleted = false;

	UPROPERTY(BlueprintReadOnly)
	bool bIsTurnedIn = false;

	UPROPERTY(BlueprintReadOnly)
	TArray<FQuestObjectiveProgress> ObjectiveProgress;
};


USTRUCT(BlueprintType)
struct FQuestLogEntryView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName QuestID = NAME_None;

	// Definition
	UPROPERTY(BlueprintReadOnly) FText Title;
	UPROPERTY(BlueprintReadOnly) FText Description;

	// Runtime
	UPROPERTY(BlueprintReadOnly) bool bIsAccepted = false;
	UPROPERTY(BlueprintReadOnly) int32 CurrentStage = 0;
	UPROPERTY(BlueprintReadOnly) bool bIsTracked = false;
	UPROPERTY(BlueprintReadOnly) bool bIsCompleted = false;
	UPROPERTY(BlueprintReadOnly) bool bIsTurnedIn = false;

	UPROPERTY(BlueprintReadOnly) TArray<FQuestObjectiveDef> Objectives;
	UPROPERTY(BlueprintReadOnly) TArray<FQuestObjectiveProgress> ObjectiveProgress;

	UPROPERTY(BlueprintReadOnly) TArray<FQuestItemReward> FinalItemRewards;
	UPROPERTY(BlueprintReadOnly) int32 FinalCurrencyReward = 0;
	UPROPERTY(BlueprintReadOnly) int32 FinalXPReward = 0;
};

USTRUCT()
struct FQuestRuntimeArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FQuestRuntimeState> Items;

	UPROPERTY(NotReplicated)
	class UQuestLogComponent* Owner = nullptr;

	// --- FastArray callbacks (Inventory-style) ---
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FQuestRuntimeState, FQuestRuntimeArray>(Items, DeltaParams, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FQuestRuntimeArray> : public TStructOpsTypeTraitsBase2<FQuestRuntimeArray>
{
	enum { WithNetDeltaSerializer = true };
};
