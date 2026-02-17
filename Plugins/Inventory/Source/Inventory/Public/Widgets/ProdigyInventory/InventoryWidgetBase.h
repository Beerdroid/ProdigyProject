#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProdigyInventory/ItemTypes.h"
#include "InventoryWidgetBase.generated.h"

class UEquipSlotWidget;
class AInvPlayerController;
class UTextBlock;
class UInvSplitCursorWidget;
class UInvContextMenuWidget;
class UGridSlotWidget;
class UUniformGridPanel;
class UInventoryComponent;
class UInv_GridSlotWidget;
class UInvItemTooltipWidget;


UCLASS()
class INVENTORY_API UInventoryWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void UpdateSplitCursorPosition();

	UPROPERTY()
	TWeakObjectPtr<AInvPlayerController> CachedPC;
	
	// Called by PlayerController
	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	void SetInventory(UInventoryComponent* InInventory);

	// Optional convenience
	UFUNCTION(BlueprintPure, Category="Inventory|UI")
	UInventoryComponent* GetInventory() const { return Inventory.Get(); }

	virtual bool NativeOnDrop(const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation) override;

	UPROPERTY(EditDefaultsOnly, Category="Inventory|UI")
	TSubclassOf<UInvItemTooltipWidget> ItemTooltipWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UInvItemTooltipWidget> ItemTooltipWidget = nullptr;

	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	void ShowTooltipForView(const FInventorySlotView& View);

	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	void HideTooltip();

	UPROPERTY(EditDefaultsOnly, Category="Inventory|UI")
	TSubclassOf<UInvContextMenuWidget> ContextMenuClass;

	UPROPERTY(Transient)
	TObjectPtr<UInvContextMenuWidget> ContextMenu = nullptr;

	UFUNCTION()
	void OpenContextMenuForSlot(int32 InSlotIndex);

	UFUNCTION()
	void CloseContextMenu();
	
	void BeginSplitFrom(int32 FromIndex, int32 Amount);
	
	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	void CancelSplitMode();

	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	bool HandleClickSlotForSplit(int32 TargetIndex); // returns true if it consumed the click
	
	bool TryCompleteSplitTo(int32 ToIndex);



	UPROPERTY(EditDefaultsOnly, Category="Inventory|UI")
	TSubclassOf<UInvSplitCursorWidget> SplitCursorWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UInvSplitCursorWidget> SplitCursorWidget = nullptr;

	bool IsInSplitMode() const { return bSplitMode; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	// Called after binding (BP can override for extra UI)
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory|UI")
	void OnInventoryBound();

	// Rebuild all slots from inventory snapshot
	void RebuildAll();

	// Update a single slot widget
	void UpdateSlot(int32 SlotIndex);

	// Delegate handlers
	UFUNCTION()
	void HandleSlotsChanged(const TArray<int32>& ChangedSlots);

	UFUNCTION()
	void HandleSlotChanged(int32 SlotIndex, const FInventorySlot& NewValue);

	// Unbind from current inventory
	void Unbind();

	// --- Widgets ---
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UUniformGridPanel> Grid = nullptr;

	// Slot widget class (BP child recommended)
	UPROPERTY(EditDefaultsOnly, Category="Inventory|UI")
	TSubclassOf<UGridSlotWidget> SlotWidgetClass;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UUserWidget> EmptySlotWidgetClass;

	// Grid layout
	UPROPERTY(EditDefaultsOnly, Category="Inventory|UI")
	int32 Columns = 6;

	static void InitEquipSlot(UEquipSlotWidget* Slot, UInventoryComponent* Inv);

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEquipSlotWidget> EquipSlot_Head = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEquipSlotWidget> EquipSlot_Chest = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEquipSlotWidget> EquipSlot_Cloak = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEquipSlotWidget> EquipSlot_Hands = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEquipSlotWidget> EquipSlot_Pants = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEquipSlotWidget> EquipSlot_Boots = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEquipSlotWidget> EquipSlot_Weapon = nullptr;

private:
	TWeakObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGridSlotWidget>> SlotWidgets;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> GoldText = nullptr;

	UFUNCTION()
	void HandleGoldChanged(int32 NewGold);

	UPROPERTY(Transient)
	bool bSplitMode = false;
	
	int32 SplitFromIndex = INDEX_NONE;
	int32 SplitAmountPending = 0;
};
