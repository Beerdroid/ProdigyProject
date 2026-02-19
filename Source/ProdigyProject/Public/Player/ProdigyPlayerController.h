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

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatHUDDirty);

UCLASS()
class PRODIGYPROJECT_API AProdigyPlayerController : public AInvPlayerController
{
	GENERATED_BODY()

public:
	AProdigyPlayerController();

	virtual void OnPossess(APawn* InPawn) override;

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

	UFUNCTION()
	void SetParticipantsWorldHealthBarsVisible(bool bVisible);

	void ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter);

	UPROPERTY(EditDefaultsOnly, Category="FloatingDamage")
	TSubclassOf<class UDamageTextComponent> DamageTextComponentClass;

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

	//UI

	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UUserWidget> CombatHUDClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> CombatHUD;

	UFUNCTION(BlueprintCallable, Category="UI")
	void ShowCombatHUD();

	UFUNCTION(BlueprintCallable, Category="UI")
	void HideCombatHUD();

	// Called by widget buttons
	UFUNCTION(BlueprintCallable, Category="UI")
	void UI_StartFight();

	UFUNCTION(BlueprintCallable, Category="UI")
	void UI_EndTurn();

	// Used by widget to display text
	UFUNCTION(BlueprintCallable, Category="UI")
	float UI_GetHP() const;

	UFUNCTION(BlueprintCallable, Category="UI")
	float UI_GetMaxHP() const;

	UFUNCTION(BlueprintCallable, Category="UI")
	float UI_GetAP() const;

	UFUNCTION(BlueprintCallable, Category="UI")
	float UI_GetMaxAP() const;

	UFUNCTION(BlueprintCallable, Category="UI")
	bool UI_IsInCombat() const;

	UPROPERTY(BlueprintAssignable, Category="UI")
	FOnCombatHUDDirty OnCombatHUDDirty;

	UFUNCTION()
	void HandleCombatStateChanged_FromSubsystem(bool bNowInCombat);

	UFUNCTION()
	void HandleTurnActorChanged_FromSubsystem(AActor* CurrentTurnActor);

	UFUNCTION()
	void HandleParticipantsChanged_FromSubsystem();

	UFUNCTION()
	void ValidateCombatHUDVisibility();

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

	UFUNCTION()
	void HandleAttrChanged_ForHUD(FGameplayTag Tag, float NewValue, float Delta, AActor* InInstigator);

	UFUNCTION()
	void HandleCombatStateChanged_ForHUD(bool bNowInCombat);

	UFUNCTION()
	void HandleTurnChanged_ForHUD();

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
