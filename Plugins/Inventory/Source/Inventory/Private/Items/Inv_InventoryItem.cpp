// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/Inv_InventoryItem.h"

#include "Items/Fragments/Inv_ItemFragment.h"
#include "Net/UnrealNetwork.h"

void UInv_InventoryItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UObject::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ItemManifest);
	DOREPLIFETIME(ThisClass, TotalStackCount);
}

void UInv_InventoryItem::SetItemID(FName InItemID)
{
	// Defensive checks
	if (InItemID.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UInv_InventoryItem::SetItemID called with NAME_None"));
		return;
	}

	// Prevent accidental reassignment
	if (!ItemID.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UInv_InventoryItem::SetItemID called twice (old=%s, new=%s)"),
			*ItemID.ToString(),
			*InItemID.ToString());
		return;
	}

	ItemID = InItemID;
}

void UInv_InventoryItem::SetItemManifest(const FInv_ItemManifest& Manifest)
{
	ItemManifest = FInstancedStruct::Make<FInv_ItemManifest>(Manifest);
}

bool UInv_InventoryItem::IsStackable() const
{
	const FInv_StackableFragment* Stackable = GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	return Stackable != nullptr;
}

bool UInv_InventoryItem::IsConsumable() const
{
	return GetItemManifest().GetItemCategory() == EInv_ItemCategory::Consumable;
}
