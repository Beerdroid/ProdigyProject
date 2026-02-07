#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProdigyInventory/ItemTypes.h"
#include "InventoryWidgetBase.generated.h"

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



private:
	TWeakObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGridSlotWidget>> SlotWidgets;
};
