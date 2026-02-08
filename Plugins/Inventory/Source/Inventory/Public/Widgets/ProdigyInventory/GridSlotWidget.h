#pragma once

#include "CoreMinimal.h"
#include "InventoryWidgetBase.h"
#include "Blueprint/UserWidget.h"
#include "ProdigyInventory/InventoryComponent.h"
#include "ProdigyInventory/ItemTypes.h"
#include "GridSlotWidget.generated.h"


class UTextBlock;
class UImage;

UCLASS()
class INVENTORY_API UGridSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitSlot(UInventoryComponent* InInventory, int32 InIndex, UInventoryWidgetBase* InOwnerMenu);

	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	void SetSlotView(const FInventorySlotView& View);

	UFUNCTION(BlueprintPure, Category="Inventory|UI")
	int32 GetSlotIndex() const { return SlotIndex; }
	
protected:
	// Optional minimal UI
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> ItemIcon = nullptr;

	// --- Drag & Drop ---
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(EditDefaultsOnly, Category="Inventory|UI")
	TSubclassOf<UUserWidget> DragVisualClass;

	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	TWeakObjectPtr<UInventoryComponent> Inventory;
	TWeakObjectPtr<UInventoryWidgetBase> OwnerMenu;
	
	int32 SlotIndex = INDEX_NONE;

	bool bIsEmptyCached = true;

	FInventorySlotView CachedView;
};
