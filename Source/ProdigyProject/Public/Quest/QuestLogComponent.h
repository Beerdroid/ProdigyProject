#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "QuestTypes.h"
#include "QuestDatabase.h"
#include "QuestLogComponent.generated.h"

class IQuestKillEventSource;
class IQuestRewardReceiver;
class IQuestInventoryProvider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnQuestsUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveProgressMessage, FName, ObjectiveID, int32, NewProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveCompletedMessage, FName, ObjectiveID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestCompletedMessage, FName, QuestID);

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Quest), meta=(BlueprintSpawnableComponent))
class PRODIGYPROJECT_API UQuestLogComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuestLogComponent();

	void HandleQuestStatesReplicated();

	// -------------------- Replication --------------------
	UPROPERTY(ReplicatedUsing=OnRep_QuestStates)
	FQuestRuntimeArray QuestStates;

	UFUNCTION(BlueprintCallable, Category="Quest")
	void NotifyInventoryChanged();

	UFUNCTION()
	void OnRep_QuestStates();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -------------------- Events for UI --------------------
	UPROPERTY(BlueprintAssignable)
	FOnQuestsUpdated OnQuestsUpdated;

	UPROPERTY(BlueprintAssignable)
	FOnObjectiveProgressMessage OnObjectiveProgressMessage;

	UPROPERTY(BlueprintAssignable)
	FOnObjectiveCompletedMessage OnObjectiveCompletedMessage;

	UPROPERTY(BlueprintAssignable)
	FOnQuestCompletedMessage OnQuestCompleted;

	// -------------------- Database --------------------
	// Assign in BP (Player Character / PlayerState / etc.) or via GameState access in BeginPlay.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Quest")
	TObjectPtr<UQuestDatabase> QuestDatabase;

	// -------------------- Public API --------------------
	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerAddQuest(FName QuestID);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerTurnInQuest(FName QuestID);

	UFUNCTION(Server, Reliable)
	void ServerTrackQuest(FName QuestID, bool bTrack);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerAbandonQuest(FName QuestID);

	bool TryGetQuestDef(FName QuestID, FQuestDefinition& OutDef) const;

	UFUNCTION(BlueprintCallable, Category="Quest")
	void NotifyKillObjectiveTag(FGameplayTag TargetTag);

	void InitializeStageProgress(FQuestRuntimeState& State, const FQuestDefinition& Def);
	bool AreAllObjectivesComplete(const FQuestRuntimeState& State, const FQuestDefinition& Def) const;
	void RecomputeCollectObjectivesForStage(FQuestRuntimeState& State, const FQuestDefinition& Def);
	void ApplyKillProgress(FQuestRuntimeState& State, const FQuestDefinition& Def, const FGameplayTag& TargetTag, int32 Delta);
	void TryAdvanceStageOrComplete(FQuestRuntimeState& State, const FQuestDefinition& Def);
	void GrantStageRewards(const FQuestDefinition& Def, int32 StageIndex);
	void GrantFinalRewards(const FQuestDefinition& Def);

	const FQuestStageDef* GetStageDef(const FQuestDefinition& Def, int32 StageIndex) const;

	UFUNCTION()
	void HandleInventoryDelta(const FInventoryDelta& Delta);

	void RecomputeCollectObjectivesForStage_SingleItem(FQuestRuntimeState& State, const FQuestDefinition& Def, FName ChangedItemID);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Quest|UI")
	TArray<FQuestLogEntryView> GetQuestLogEntries() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Quest|UI")
	TArray<FQuestLogEntryView> BuildQuestOfferViews(const TArray<FName>& QuestIDs) const;
	
protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UActorComponent> CachedIntegrationComponent;

	void NotifyQuestStatesChanged_LocalAuthority();

	void HandleKillTagNative(FGameplayTag TargetTag);

	TScriptInterface<IQuestInventoryProvider> InventoryProvider;
	TScriptInterface<IQuestRewardReceiver> RewardReceiver;
	TScriptInterface<IQuestKillEventSource> KillEventSource;

	bool bIntegrationBound = false;

	void BindIntegration();
	
	const IQuestInventoryProvider* GetInventoryProviderChecked() const;
	IQuestInventoryProvider* GetInventoryProviderCheckedMutable() const;
	IQuestRewardReceiver* GetRewardReceiverChecked() const;

	// Internal helpers
	FQuestRuntimeState* FindQuestStateMutable(FName QuestID);
	const FQuestRuntimeState* FindQuestStateConst(FName QuestID) const;

	int32 GetObjectiveProgress(const FQuestRuntimeState& State, FName ObjectiveID) const;
	void SetObjectiveProgress(FQuestRuntimeState& State, FName ObjectiveID, int32 NewValue);

		// Convenience
	AActor* GetOwnerActorChecked() const;
};
