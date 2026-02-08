#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quest/QuestTypes.h"
#include "Quest/Interfaces/QuestInventoryProvider.h"
#include "Quest/Interfaces/QuestRewardReceiver.h"
#include "Quest/Interfaces/QuestKillEventSource.h"
#include "QuestIntegrationComponent.generated.h"

class UInventoryComponent;
class UInv_InventoryComponent;
// Dynamic delegate for Blueprints/UI
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestKillTagBP, FGameplayTag, TargetTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestItemChanged, FName, ItemID);

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

	FOnQuestItemChanged OnQuestItemChanged;

	UFUNCTION()
	void HandleItemChanged(FName ItemID);

	// ---- IQuestInventoryProvider ----
	virtual int32 GetTotalQuantityByItemID_Implementation(FName ItemID) const override;
	virtual bool AddItemByID_Implementation(FName ItemID, int32 Quantity, UObject* Context) override;
	virtual void RemoveItemByID_Implementation(FName ItemID, int32 Quantity,  UObject* Context) override;

	// ---- IQuestRewardReceiver ----
	virtual void AddCurrency_Implementation(int32 Amount) override;
	virtual void AddXP_Implementation(int32 Amount) override;

	// ---- IQuestKillEventSource ----
	virtual FOnQuestKillTagNative& GetKillDelegate() override { return OnQuestKillTagNative; }

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
	UPROPERTY()
	TObjectPtr<UInventoryComponent> InventoryComp = nullptr;
	
};
