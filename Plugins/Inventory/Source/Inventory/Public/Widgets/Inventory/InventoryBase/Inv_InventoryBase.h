// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/Manifest/Inv_ItemManifest.h"
#include "Types/Inv_GridTypes.h"
#include "Inv_InventoryBase.generated.h"

class UInv_ItemComponent;
class UInv_InventoryItem;
class UInv_HoverItem;

UCLASS()
class INVENTORY_API UInv_InventoryBase : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual FInv_SlotAvailabilityResult HasRoomForItem(UInv_ItemComponent* ItemComponent) const { return FInv_SlotAvailabilityResult(); }
	
	// (quest / manifest / non-world flow)
	virtual FInv_SlotAvailabilityResult HasRoomForItem(const FInv_ItemManifest& Manifest, int32 Quantity) const
	{
		return FInv_SlotAvailabilityResult();
	}
	
	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	UInv_InventoryGrid* GetInventoryGrid() const { return InventoryGrid; }

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UInv_InventoryGrid> InventoryGrid = nullptr;
	
	UFUNCTION(BlueprintCallable, Category="Inventory")
	virtual void SetSourceInventory(class UInv_InventoryComponent* InInventory);

	UPROPERTY()
	TObjectPtr<UInv_InventoryComponent> SourceInventory = nullptr;

	bool bBoundToInventory = false;

	void BindToInventory();
	void UnbindFromInventory();

	UFUNCTION(BlueprintImplementableEvent, Category="Inventory|UI")
	void HandleItemAdded(UInv_InventoryItem* Item);
	
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory|UI")
	void HandleItemRemoved(UInv_InventoryItem* Item);

	UFUNCTION(Category="Inventory|UI")
	void HandleStackChanged(const FInv_SlotAvailabilityResult& Result);
	
	virtual void OnItemHovered(UInv_InventoryItem* Item) {}
	virtual void OnItemUnHovered() {}
	virtual bool HasHoverItem() const { return false; }
	virtual UInv_HoverItem* GetHoverItem() const { return nullptr; }
	virtual float GetTileSize() const { return 0.f; }
};
