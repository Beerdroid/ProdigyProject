// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/ActionTypes.h"
#include "ProdigyInventory/InvPlayerController.h"
#include "Quest/Interfaces/QuestInventoryProvider.h"
#include "ProdigyPlayerController.generated.h"

class UEquipModSource;
class UAttributesComponent;
class UCombatSubsystem;
class UQuestLogComponent;
class UQuestIntegrationComponent;

DEFINE_LOG_CATEGORY_STATIC(LogTargeting, Log, All);

#define TARGET_LOG(Verbosity, Fmt, ...) \
UE_LOG(LogTargeting, Verbosity, TEXT("[PC:%s] " Fmt), *GetNameSafe(this), ##__VA_ARGS__)

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetLocked, AActor*, NewTarget);

UCLASS()
class PRODIGYPROJECT_API AProdigyPlayerController : public AInvPlayerController
{
	GENERATED_BODY()

public:
	AProdigyPlayerController();

protected:
	virtual void BeginPlay() override;
	
	virtual bool HandlePrimaryClickActor(AActor* ClickedActor) override;
	
public:
	// Call this from places where you know inventory has changed (pickup/drop/consume/reward/etc).
	UFUNCTION(BlueprintCallable, Category="Quest")
	void NotifyQuestsInventoryChanged();

	UFUNCTION(BlueprintCallable, Category="Quest")
	void NotifyQuestsKillTag(FGameplayTag TargetTag);

	UFUNCTION(BlueprintCallable, Category="Quest")
	void ApplyQuests(FName QuestID);

	UFUNCTION(BlueprintCallable, Category="Interaction")
	virtual void PrimaryInteract() override;

	// Current locked target (replication not needed in single-player)

	UPROPERTY(BlueprintReadOnly, Category="Targeting")
	TObjectPtr<AActor> LockedTarget = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Targeting")
	TObjectPtr<AActor> PreviousLockedTarget = nullptr;

	UPROPERTY(BlueprintAssignable, Category="Targeting")
	FOnTargetLocked OnTargetLocked;

	// Lock target explicitly (call from click trace or UI)
	UFUNCTION(BlueprintCallable, Category="Targeting")
	void SetLockedTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category="Targeting")
	void ClearLockedTarget();

	UFUNCTION(BlueprintCallable, Category="Targeting")
	AActor* GetLockedTarget() const { return LockedTarget; }

	// Convenience: trace under cursor and lock
	UFUNCTION(BlueprintCallable, Category="Targeting")
	bool TryLockTargetUnderCursor();

	bool TryLockTarget(AActor* Candidate);

	UFUNCTION(BlueprintCallable, Category="Targeting")
	FActionContext BuildActionContextFromLockedTarget() const;

	UFUNCTION(BlueprintCallable, Category="Combat|Turn")
	bool IsMyTurn() const;

	UFUNCTION(BlueprintCallable, Category="Combat|Abilities")
	bool TryUseAbilityOnLockedTarget(FGameplayTag AbilityTag);

	UFUNCTION(BlueprintCallable, Category="Combat|Abilities")
	void EndTurn();

	UCombatSubsystem* GetCombatSubsystem() const;

	virtual AActor* GetActorUnderCursorForClick() const override;

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn);

	// Inventory delegates
	UFUNCTION()
	void HandleItemEquipped(FGameplayTag EquipSlotTag, FName ItemID);

	UFUNCTION()
	void HandleItemUnequipped(FGameplayTag EquipSlotTag, FName ItemID);

	virtual bool ConsumeFromSlot(int32 SlotIndex, TArray<int32>& OutChanged) override;

protected:
	// Add in BP child if you prefer; these will auto-find if present.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest")
	TObjectPtr<UQuestLogComponent> QuestLog = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest")
	TObjectPtr<UQuestIntegrationComponent> QuestIntegration = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Targeting")
	bool bOnlyLockCombatants = true;

	UPROPERTY(EditDefaultsOnly, Category="Targeting")
	TEnumAsByte<ECollisionChannel> TargetTraceChannel = ECC_Visibility;

private:
	void CacheComponents();

	UAttributesComponent* ResolveAttributesFromPawn(APawn* OwnPawn) const;

	void BindInventoryDelegates();
	void ReapplyAllEquipmentMods(); // called on pawn change + initial

	UObject* GetOrCreateEquipSource(FGameplayTag SlotTag);

	// cached refs
	UPROPERTY(Transient)
	TWeakObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAttributesComponent> Attributes;

	UPROPERTY(Transient)
	TWeakObjectPtr<UInvEquipmentComponent> EquipmentComp;

	// per-slot mod sources
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UEquipModSource>> EquipSources;

	void ClearAllEquipSources(UAttributesComponent* Attr);

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UObject>> EquipModSources;
};
