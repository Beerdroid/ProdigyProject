#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quest/QuestTypes.h"
#include "Quest/Interfaces/QuestInventoryProvider.h"
#include "Quest/Interfaces/QuestRewardReceiver.h"
#include "Quest/Interfaces/QuestKillEventSource.h"
#include "QuestIntegrationComponent.generated.h"

class UInv_InventoryComponent;
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

	virtual void BeginPlay() override;

	void HandleInvDelta(FName ItemID, int32 DeltaQty, UObject* Context);

	// ---- IQuestInventoryProvider ----
	virtual int32 GetTotalQuantityByItemID_Implementation(FName ItemID) const override;
	virtual bool AddItemByID_Implementation(FName ItemID, int32 Quantity, UObject* Context) override;
	virtual void RemoveItemByID_Implementation(FName ItemID, int32 Quantity,  UObject* Context) override;

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
	
	virtual bool GetItemViewByID_Implementation(FName ItemID, FInv_ItemView& OutView) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Quest|Items")
	bool TryGetItemManifest(FName ItemID, FInv_ItemManifest& OutManifest) const;

protected:
	UPROPERTY()
	TObjectPtr<UInv_InventoryComponent> InventoryComp = nullptr;
	
	void BroadcastInventoryDelta(FName ItemID, int32 DeltaQty, UObject* Context);
};
