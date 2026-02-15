#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "EquipSlotWidget.generated.h"

class UImage;
class UTextBlock;
class UInventoryComponent;

UCLASS()
class INVENTORY_API UEquipSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Set this per-widget in BP (Helmet, Chest, Weapon, etc.)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Equipment")
	FGameplayTag EquipSlotTag;

	// Optional: if you want the widget to show what is equipped
	UFUNCTION(BlueprintCallable, Category="Inventory|Equipment")
	void SetInventory(UInventoryComponent* InInventory);

	UPROPERTY(EditDefaultsOnly, Category="Inventory|UI")
	TSubclassOf<UUserWidget> DragVisualClass;

	UFUNCTION()
	void HandleItemEquipped(FGameplayTag InEquipSlotTag, FName InItemID);

	UFUNCTION()
	void HandleItemUnequipped(FGameplayTag InEquipSlotTag, FName InItemID);

protected:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> ItemIcon = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> DefaultIcon = nullptr;

	UPROPERTY(Transient)
	bool bDefaultBrushCached = false;

	virtual bool NativeOnDrop(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;

	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

private:
	TWeakObjectPtr<UInventoryComponent> Inventory;

	void RefreshVisual();
};
