// InventoryComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemTypes.h"
#include "InventoryComponent.generated.h"

/** BP-friendly delegates */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnSlotChanged,
	int32, SlotIndex,
	const FInventorySlot&, NewValue
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnSlotsChanged,
	const TArray<int32>&, ChangedSlots
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemIDChanged, FName, ItemID);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnItemEquipped,
	FGameplayTag, EquipSlotTag,
	FName, ItemID
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnItemUnequipped,
	FGameplayTag, EquipSlotTag,
	FName, ItemID
);


UENUM(BlueprintType)
enum class EInventoryType : uint8
{
	Player     UMETA(DisplayName="Player"),
	Container  UMETA(DisplayName="Container"),
	Merchant   UMETA(DisplayName="Merchant"),
	Loot       UMETA(DisplayName="Loot"),
};

UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent), Blueprintable)
class INVENTORY_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	// ===== Config =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	int32 Capacity = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	EInventoryType InventoryType = EInventoryType::Container;

	/** If true, PredefinedItems are added in BeginPlay */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	bool bInitializeOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	TArray<FPredefinedItemEntry> PredefinedItems;

	// ===== Runtime =====
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	TArray<FInventorySlot> Slots;

	UPROPERTY(BlueprintAssignable, Category="Inventory|Events")
	FOnInventoryChanged OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category="Inventory|Events")
	FOnSlotChanged OnSlotChanged;

	UPROPERTY(BlueprintAssignable, Category="Inventory|Events")
	FOnSlotsChanged OnSlotsChanged;

	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnItemIDChanged OnItemIDChanged;

	void BroadcastChanged();
	void BroadcastSlotsChanged(const TArray<int32>& ChangedSlots);

	// ===== Public API (BlueprintCallable) =====
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void InitializeSlots();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory")
	int32 GetCapacity() const { return Capacity; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory")
	bool IsValidIndex(int32 SlotIndex) const;

	UFUNCTION()
	bool TryGetItemDef(FName ItemID, FItemRow& OutRow) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory")
	FInventorySlot GetSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool SetSlot(int32 SlotIndex, const FInventorySlot& NewValue);

	/** Add items by ItemID. Fills partial stacks then empties. Returns remainder. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool AddItem(FName ItemID, int32 Quantity, int32& OutRemainder, TArray<int32>& OutChangedSlots);

	int32 GetEmptySlotCount() const;

	/** Remove quantity from a slot (consuming/dropping/selling). */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool RemoveFromSlot(int32 SlotIndex, int32 Quantity, TArray<int32>& OutChangedSlots);

	/** Move, swap, or merge between two slots in this inventory. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool MoveOrSwap(int32 FromIndex, int32 ToIndex, TArray<int32>& OutChangedSlots);

	/** Split stack: move SplitAmount to an empty slot. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool SplitStack(int32 FromIndex, int32 ToIndex, int32 SplitAmount, TArray<int32>& OutChangedSlots);

	/** Transfer between inventories (explicit target slot). Quantity <=0 means "all". */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool TransferTo(UInventoryComponent* Target, int32 FromIndex, int32 ToIndex, int32 Quantity, TArray<int32>& OutChangedSource, TArray<int32>& OutChangedTarget);

	/** Auto transfer to target inventory (fills partial stacks then empties). Quantity <=0 means "all". */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool AutoTransferTo(UInventoryComponent* Target, int32 FromIndex, int32 Quantity, TArray<int32>& OutChangedSource, TArray<int32>& OutChangedTarget);

	/**
	 * Drop from slot: spawns WorldPickupClass from DB (if set) and configures its UInv_ItemComponent.
	 * Quantity <=0 means "all".
	 */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool DropFromSlot(int32 SlotIndex, int32 Quantity, FVector WorldLocation, FRotator WorldRotation, TArray<int32>& OutChangedSlots);

	/** Simple helper: checks if we can fit Quantity (fully) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory")
	bool CanFullyAdd(FName ItemID, int32 Quantity) const;

	UFUNCTION(BlueprintCallable, Category="Inventory|Pickup")
	bool TryPickupFromItemComponent_FullOnly(UItemComponent* ItemComp);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory")
	int32 GetTotalQuantityByItemID(FName ItemID) const;

	UFUNCTION(BlueprintCallable, Category="Inventory|Quest")
	bool AddByItemID(
		FName ItemID,
		int32 Quantity,
		int32& OutRemainder,
		TArray<int32>& OutChangedSlots
	);

	UFUNCTION(BlueprintCallable, Category="Inventory|Quest")
	bool RemoveByItemID(
		FName ItemID,
		int32 Quantity,
		int32& OutRemoved,
		TArray<int32>& OutChangedSlots
	);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory|Quest")
	bool CanRemoveByItemID(FName ItemID, int32 Quantity) const;

	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	bool GetSlotView(int32 SlotIndex, FInventorySlotView& OutView) const;

	int32 FindFirstEmptySlot() const;

	// ===== Equipment Events =====
	UPROPERTY(BlueprintAssignable, Category="Inventory|Equipment")
	FOnItemEquipped OnItemEquipped;

	UPROPERTY(BlueprintAssignable, Category="Inventory|Equipment")
	FOnItemUnequipped OnItemUnequipped;

	// ===== Equipment API =====
	UFUNCTION(BlueprintCallable, Category="Inventory|Equipment")
	bool EquipFromSlot(int32 SlotIndex, TArray<int32>& OutChangedSlots);

	UFUNCTION(BlueprintCallable, Category="Inventory|Equipment")
	bool UnequipToInventory(FGameplayTag EquipSlotTag, TArray<int32>& OutChangedSlots);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory|Equipment")
	bool GetEquippedItem(FGameplayTag EquipSlotTag, FName& OutItemID) const;

protected:
	virtual void BeginPlay() override;

private:
	// Internal helpers

	void FindPartialStacks(FName ItemID, TArray<int32>& OutIndices) const;
	int32 GetMaxStack(FName ItemID) const;
	bool IsStackable(FName ItemID) const;

	void MarkSlotChanged(int32 SlotIndex, TArray<int32>& InOutChangedSlots);

	UPROPERTY()
	TArray<FEquippedItemEntry> EquippedItems;

	int32 FindEquippedIndex(FGameplayTag EquipSlotTag) const;
};
