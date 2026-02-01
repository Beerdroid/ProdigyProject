// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/Inv_PlayerController.h"
#include "GameplayTagContainer.h"
#include "Quest/Interfaces/QuestInventoryProvider.h"
#include "ProdigyPlayerController.generated.h"

class UQuestLogComponent;
class UQuestIntegrationComponent;


UCLASS()
class PRODIGYPROJECT_API AProdigyPlayerController : public AInv_PlayerController, public IQuestInventoryProvider
{
	GENERATED_BODY()

public:
	AProdigyPlayerController();

protected:
	virtual void BeginPlay() override;

public:
	// Call this from places where you know inventory has changed (pickup/drop/consume/reward/etc).
	UFUNCTION(BlueprintCallable, Category="Quest")
	void NotifyQuestsInventoryChanged();

	// Call this when your game credits a kill to this player (server).
	UFUNCTION(BlueprintCallable, Category="Quest")
	void NotifyQuestsKillTag(FGameplayTag TargetTag);

	// Server entrypoint for BP (safe even if called on client).
	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Quest")
	void Server_NotifyQuestsInventoryChanged();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Quest")
	void Server_NotifyQuestsKillTag(FGameplayTag TargetTag);

	UFUNCTION(BlueprintCallable, Category="Quest")
	void ApplyQuests(FName QuestID);

	UFUNCTION(BlueprintCallable, Category="Interaction")
	virtual void PrimaryInteract() override;

	virtual bool AddItemByID_Implementation(FName ItemID, int32 Quantity, UObject* Context) override;

	virtual FOnQuestInventoryDelta& GetInventoryDeltaDelegate() override;


protected:
	// Add in BP child if you prefer; these will auto-find if present.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest")
	TObjectPtr<UQuestLogComponent> QuestLog = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest")
	TObjectPtr<UQuestIntegrationComponent> QuestIntegration = nullptr;

private:
	void CacheQuestComponents();
	
	FOnQuestInventoryDelta DummyInventoryDelta;
};
