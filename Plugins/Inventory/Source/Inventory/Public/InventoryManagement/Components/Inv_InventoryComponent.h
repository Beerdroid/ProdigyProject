// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryManagement/FastArray/Inv_FastArray.h"
#include "Items/Manifest/Inv_ItemManifest.h"
#include "Inv_InventoryComponent.generated.h"

class UInv_ItemComponent;
class UInv_InventoryItem;
class UInv_InventoryBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryItemChange, UInv_InventoryItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNoRoomInInventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStackChange, const FInv_SlotAvailabilityResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FItemEquipStatusChanged, UInv_InventoryItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryMenuToggled, bool, bOpen);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnInvDeltaNative, FName /*ItemID*/, int32 /*DeltaQty*/, UObject* /*Context*/);

USTRUCT(BlueprintType)
struct FInv_PredefinedItemEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1"))
	int32 Quantity = 1;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class INVENTORY_API UInv_InventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInv_InventoryComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Inventory")
	bool TryAddItemByManifest(FName ItemID, const FInv_ItemManifest& Manifest, int32 Quantity, int32& OutRemainder);

	UFUNCTION(Server, Reliable)
	void Server_AddNewItemFromManifest(FName ItemID, FInv_ItemManifest Manifest, int32 StackCount);

	UFUNCTION(Server, Reliable)
	void Server_AddStacksToItemFromManifest(FName ItemID, FGameplayTag ItemType, int32 StackCount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void TryAddItem(UInv_ItemComponent* ItemComponent);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Inventory")
	bool AddItemByID_ServerAuth(FName ItemID, int32 Quantity, UObject* Context, int32& OutRemainder);

	UFUNCTION(Server, Reliable)
	void Server_AddNewItem(UInv_ItemComponent* ItemComponent, int32 StackCount, int32 Remainder);

	UFUNCTION(Server, Reliable)
	void Server_AddStacksToItem(UInv_ItemComponent* ItemComponent, int32 StackCount, int32 Remainder);

	UFUNCTION(Server, Reliable)
	void Server_DropItem(UInv_InventoryItem* Item, int32 StackCount);

	
	UFUNCTION(Server, Reliable)
	void Server_RemoveItemByID(FName ItemID, int32 Quantity, UObject* Context);

	UFUNCTION(Server, Reliable)
	void Server_ConsumeItem(UInv_InventoryItem* Item);

	UFUNCTION(Server, Reliable)
	void Server_EquipSlotClicked(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EquipSlotClicked(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip);

	void ToggleInventoryMenu();
	void AddRepSubObj(UObject* SubObj);
	void SpawnDroppedItem(UInv_InventoryItem* Item, int32 StackCount);
	UInv_InventoryBase* GetInventoryMenu() const { return InventoryMenu; }
	bool IsMenuOpen() const { return bInventoryMenuOpen; }

	void SetItemID(FName InItemID);
	void EmitInvDeltaByItemID(FName ItemID, int32 DeltaQty, UObject* Context);
	FName ResolveItemIDFromManifest(const FInv_ItemManifest& Manifest) const;
	FName ResolveItemIDFromItemComponent(const UInv_ItemComponent* ItemComponent) const;
	FName ResolveItemIDFromInventoryItem(const UInv_InventoryItem* Item) const;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	int32 GetTotalQuantityByItemID(FName ItemID) const;

	UFUNCTION(BlueprintPure, Category="Inventory|UI")
	FInv_ItemView BuildItemViewFromManifest(const FInv_ItemManifest& Manifest) const;

	FInventoryItemChange OnItemAdded;
	FInventoryItemChange OnItemRemoved;
	FNoRoomInInventory NoRoomInInventory;
	FStackChange OnStackChange;
	FItemEquipStatusChanged OnItemEquipped;
	FItemEquipStatusChanged OnItemUnequipped;
	FInventoryMenuToggled OnInventoryMenuToggled;
	FOnInvDeltaNative OnInvDelta;

	UPROPERTY(Replicated)
	bool bInitialized = false;

	UPROPERTY(Replicated, EditAnywhere, Category="Inventory|Predefined")
	TArray<FInv_PredefinedItemEntry> PredefinedItems;
	
protected:
	virtual void BeginPlay() override;

	virtual void ReadyForReplication() override;

	void SyncInventoryToUI();

private:

	TWeakObjectPtr<APlayerController> OwningController;

	bool ResolveManifestByItemID(FName ItemID, FInv_ItemManifest& OutManifest) const;

	UFUNCTION(Server, Reliable)
	void Server_InitializeFromPredefinedItems();

	bool TryAddItemByManifest_NoUI(FName ItemID, const FInv_ItemManifest& Manifest, int32 Quantity, int32& OutRemainder);
	
	void ConstructInventory();
	
	UFUNCTION(Server, Reliable)
	void Server_InitializeMerchantStock();

	UPROPERTY(Replicated)
	FInv_InventoryFastArray InventoryList;

	UPROPERTY()
	TObjectPtr<UInv_InventoryBase> InventoryMenu;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UInv_InventoryBase> InventoryMenuClass;

	bool bInventoryMenuOpen;
	void OpenInventoryMenu();
	void CloseInventoryMenu();

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DropSpawnAngleMin = -85.f;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DropSpawnAngleMax = 85.f;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DropSpawnDistanceMin = 10.f;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DropSpawnDistanceMax = 50.f;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float RelativeSpawnElevation = 70.f;
};
