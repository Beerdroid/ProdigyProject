#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InvEquipmentComponent.generated.h"

class UEquipSlotWidget;
class AInvEquipActor;
class UInventoryComponent;
class USkeletalMeshComponent;

UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent), Blueprintable)
class INVENTORY_API UInvEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInvEquipmentComponent();

	UFUNCTION(BlueprintCallable, Category="Inventory|Equipment")
	void SetOwningSkeletalMesh(USkeletalMeshComponent* OwningMesh);

	UFUNCTION()
	void OnItemEquipped(FGameplayTag EquipSlotTag, FName ItemID);

	UFUNCTION()
	void OnItemUnequipped(FGameplayTag EquipSlotTag, FName ItemID);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory|Equipment")
	TArray<AInvEquipActor*> GetEquippedActorsCopy() const;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TWeakObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshComponent> OwningSkeletalMesh;

	UPROPERTY()
	TArray<TObjectPtr<AInvEquipActor>> EquippedActors;

	// Bind to inventory delegates
	void InitInventoryComponent();

	AInvEquipActor* FindEquippedActor(const FGameplayTag& EquipSlotTag) const;
	void RemoveEquippedActor(const FGameplayTag& EquipSlotTag);

	// Single-player: resolve the InventoryComponent from the first player controller
	UInventoryComponent* ResolvePlayerInventory() const;

	TWeakObjectPtr<APlayerController> OwningPC;

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	void ResolveMeshFromPawn(APawn* Pawn);

	void RebuildAllEquippedVisuals(); // optional but recommended
};
