#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quest/QuestTypes.h"
#include "Quest/Interfaces/QuestInventoryProvider.h"
#include "Quest/Interfaces/QuestRewardReceiver.h"
#include "Quest/Interfaces/QuestKillEventSource.h"
#include "QuestIntegrationComponent.generated.h"

// Dynamic delegate for Blueprints/UI
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestKillTagBP, FGameplayTag, TargetTag);

// This class is part of the plugin, but DOES NOT know about your project's inventory.
// You subclass it in your project OR implement its Blueprint events.
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Quest), meta=(BlueprintSpawnableComponent))
class PRODIGYPROJECT_API UQuestIntegrationComponent
	: public UActorComponent
	, public IQuestInventoryProvider
	, public IQuestRewardReceiver
	, public IQuestKillEventSource
{
	GENERATED_BODY()

public:
	UQuestIntegrationComponent();

	// ---- IQuestInventoryProvider ----
	virtual int32 GetTotalQuantityByItemID_Implementation(FName ItemID) const override;
	virtual bool AddItemByID_Implementation(FName ItemID, int32 Quantity, UObject* Context) override;
	virtual int32 RemoveItemByID_Implementation(FName ItemID, int32 Quantity,  UObject* Context) override;

	virtual FOnQuestInventoryDelta& GetInventoryDeltaDelegate() override { return OnQuestInventoryDelta; }

	// ---- IQuestRewardReceiver ----
	virtual void AddCurrency_Implementation(int32 Amount) override;
	virtual void AddXP_Implementation(int32 Amount) override;

	// ---- IQuestKillEventSource ----
	virtual FOnQuestKillTagNative& GetKillDelegate() override { return OnQuestKillTagNative; }

	// Delegates the quest system can bind to (Blueprint)
	UPROPERTY(BlueprintAssignable, Category="Quest|Events")
	FOnQuestInventoryDelta OnQuestInventoryDelta;

	// Native delegate (C++ only). NOT a UPROPERTY.
	FOnQuestKillTagNative OnQuestKillTagNative;

	// Blueprint-visible kill event (optional)
	UPROPERTY(BlueprintAssignable, Category="Quest|Events")
	FOnQuestKillTagBP OnQuestKillTagBP;

	// Call this from your game when a kill happens (C++ or BP)
	UFUNCTION(BlueprintCallable, Category="Quest|Events")
	void BroadcastKillTag(FGameplayTag TargetTag)
	{
		OnQuestKillTagNative.Broadcast(TargetTag); // Quest system
		OnQuestKillTagBP.Broadcast(TargetTag);     // Blueprint/UI
	}

protected:
	void BroadcastInventoryDelta(FName ItemID, int32 DeltaQty, UObject* Context);
	
	UFUNCTION(BlueprintImplementableEvent, Category="Quest|Integration")
	int32 BP_GetTotalQuantityByItemID(FName ItemID) const;

	UFUNCTION(BlueprintImplementableEvent, Category="Quest|Integration")
	bool BP_AddItemByID(FName ItemID, int32 Quantity, UObject* Context);

	UFUNCTION(BlueprintImplementableEvent, Category="Quest|Integration")
	int32 BP_RemoveItemByID(FName ItemID, int32 Quantity, UObject* Context);

	UFUNCTION(BlueprintImplementableEvent, Category="Quest|Integration")
	void BP_AddCurrency(int32 Amount);

	UFUNCTION(BlueprintImplementableEvent, Category="Quest|Integration")
	void BP_AddXP(int32 Amount);
};
