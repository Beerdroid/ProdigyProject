// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/Inventory/InventoryBase/Inv_InventoryBase.h"

#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "Items/Inv_InventoryItem.h"

void UInv_InventoryBase::SetSourceInventory(class UInv_InventoryComponent* InInventory)
{
	UE_LOG(LogTemp, Warning, TEXT("SetSourceInventory started"));
	if (SourceInventory == InInventory)
	{
		return;
	}

	if (bBoundToInventory && IsValid(SourceInventory))
	{
		UnbindFromInventory();
	}

	SourceInventory = InInventory;

	if (!IsValid(SourceInventory))
	{
		return;
	}

	BindToInventory();

	// ---- Replay current state into this widget ----
	const TArray<UInv_InventoryItem*> Items = SourceInventory->GetAllItems();
	UE_LOG(LogTemp, Warning, TEXT("[InvUI] Replay Items=%d"), Items.Num());

	for (UInv_InventoryItem* It : Items)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InvUI]  Item=%s ItemID=%s TotalStack=%d"),
			*GetNameSafe(It),
			It ? *It->GetItemID().ToString() : TEXT("<null>"),
			It ? It->GetTotalStackCount() : -1
		);
	}

	for (UInv_InventoryItem* Item : Items)
	{
		if (!IsValid(Item)) continue;
		HandleItemAdded(Item); // <-- call widget handler directly
	}

	UE_LOG(LogTemp, Warning, TEXT("SetSourceInventory completed"));
}

void UInv_InventoryBase::BindToInventory()
{
	if (!IsValid(SourceInventory) || bBoundToInventory) return;

	// NOTE: your delegates in component are NOT UPROPERTY dynamic multicast,
	// but they are regular members. If you want BP binding, they must be dynamic multicast UPROPERTY.
	// If you only bind in C++, regular is fine.

	SourceInventory->OnItemAdded.AddDynamic(this, &ThisClass::HandleItemAdded);
	SourceInventory->OnItemRemoved.AddDynamic(this, &ThisClass::HandleItemRemoved);
	SourceInventory->OnStackChange.AddDynamic(this, &ThisClass::HandleStackChanged);

	bBoundToInventory = true;
}

void UInv_InventoryBase::UnbindFromInventory()
{
	if (!IsValid(SourceInventory) || !bBoundToInventory) return;

	SourceInventory->OnItemAdded.RemoveDynamic(this, &ThisClass::HandleItemAdded);
	SourceInventory->OnItemRemoved.RemoveDynamic(this, &ThisClass::HandleItemRemoved);
	SourceInventory->OnStackChange.RemoveDynamic(this, &ThisClass::HandleStackChanged);

	bBoundToInventory = false;
}

void UInv_InventoryBase::HandleStackChanged(const FInv_SlotAvailabilityResult& Result)
{
}

