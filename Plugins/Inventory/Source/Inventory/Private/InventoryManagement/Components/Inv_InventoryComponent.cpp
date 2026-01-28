// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryManagement/Components/Inv_InventoryComponent.h"

#include "Items/Components/Inv_ItemComponent.h"
#include "Widgets/Inventory/InventoryBase/Inv_InventoryBase.h"
#include "Net/UnrealNetwork.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Fragments/Inv_FragmentTags.h"
#include "Items/Fragments/Inv_ItemFragment.h"


UInv_InventoryComponent::UInv_InventoryComponent() : InventoryList(this)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
	bInventoryMenuOpen = false;
}

void UInv_InventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, InventoryList);
}

FName UInv_InventoryComponent::ResolveItemIDFromInventoryItem(const UInv_InventoryItem* Item) const
{
	return Item ? Item->GetItemID() : NAME_None;
}

int32 UInv_InventoryComponent::GetTotalQuantityByItemID(FName ItemID) const
{
	return InventoryList.GetTotalQuantityByItemID(ItemID);
}

FInv_ItemView UInv_InventoryComponent::BuildItemViewFromManifest(const FInv_ItemManifest& Manifest) const
{
	FInv_ItemView View;
	View.Category = Manifest.GetItemCategory();
	View.ItemType  = Manifest.GetItemType();

	// DisplayName
	if (const FInv_TextFragment* NameFrag =
		Manifest.GetFragmentOfTypeWithTag<FInv_TextFragment>(FragmentTags::ItemNameFragment))
	{
		View.DisplayName = NameFrag->GetText();
	}

	// Description (use FlavorTextFragment, NOT FragmentTags.UI.Description)
	if (const FInv_TextFragment* DescFrag =
		Manifest.GetFragmentOfTypeWithTag<FInv_TextFragment>(FragmentTags::FlavorTextFragment))
	{
		View.Description = DescFrag->GetText();
	}

	// Icon
	if (const FInv_ImageFragment* IconFrag =
		Manifest.GetFragmentOfTypeWithTag<FInv_ImageFragment>(FragmentTags::IconFragment))
	{
		View.Icon = IconFrag->GetIcon();
	}

	// Stackable
	if (const FInv_StackableFragment* StackFrag =
		Manifest.GetFragmentOfTypeWithTag<FInv_StackableFragment>(FragmentTags::StackableFragment))
	{
		View.bStackable = true;
		View.MaxStack   = StackFrag->GetMaxStackSize();
	}
	else
	{
		View.bStackable = false;
		View.MaxStack   = 1;
	}

	return View;
}


bool UInv_InventoryComponent::TryAddItemByManifest(FName ItemID, const FInv_ItemManifest& Manifest, int32 Quantity, int32& OutRemainder)
{
	UE_LOG(LogTemp, Warning, TEXT("TryAddItemByManifest: Owner=%s Auth=%d MenuValid=%d Item=%s Qty=%d"),
	*GetNameSafe(GetOwner()),
	GetOwner() ? (int32)GetOwner()->HasAuthority() : -1,
	(int32)IsValid(InventoryMenu),
	*ItemID.ToString(),
	Quantity);

		
	OutRemainder = Quantity;
	if (ItemID.IsNone() || Quantity <= 0) return false;

	// If we have UI, use your existing capacity logic
	if (IsValid(InventoryMenu))
	{
		FInv_SlotAvailabilityResult Result = InventoryMenu->HasRoomForItem(Manifest, Quantity);
		UInv_InventoryItem* FoundItem = InventoryList.FindFirstItemByID(ItemID);
		Result.Item = FoundItem;

		if (Result.TotalRoomToFill == 0)
		{
		UE_LOG(LogTemp, Warning, TEXT("TryAddItemByManifest: NO ROOM (TotalRoomToFill=0) for %s"), *ItemID.ToString());
			
			NoRoomInInventory.Broadcast();
			return false;
		}


		OutRemainder = Result.Remainder;

		if (Result.Item.IsValid() && Result.bStackable)
		{
			OnStackChange.Broadcast(Result);
			Server_AddStacksToItemFromManifest(ItemID, Manifest.GetItemType(), Result.TotalRoomToFill);
		}
		else
		{
			Server_AddNewItemFromManifest(ItemID, Manifest, Result.bStackable ? Result.TotalRoomToFill : 0);
		}

		UE_LOG(LogTemp, Warning, TEXT("TryAddItemByManifest UIResult: TotalRoomToFill=%d Remainder=%d bStackable=%d FoundItem=%s"),
	Result.TotalRoomToFill,
	Result.Remainder,
	(int32)Result.bStackable,
	*GetNameSafe(Result.Item.Get()));

		return (Result.TotalRoomToFill > 0);
	}

	// ---- Server-safe fallback (no UI) ----
	// For quests/rewards: just add all. (Capacity rules must be moved out of UI later if you need them.)
	const bool bStackable =
		Manifest.GetFragmentOfTypeWithTag<FInv_StackableFragment>(FragmentTags::StackableFragment) != nullptr;

	OutRemainder = 0;

	if (bStackable)
	{
		// Put whole quantity into your single “total stack count” model
		if (UInv_InventoryItem* Existing = InventoryList.FindFirstItemByID(ItemID))
		{
			Server_AddStacksToItemFromManifest(ItemID, Manifest.GetItemType(), Quantity);
		}
		else
		{
			Server_AddNewItemFromManifest(ItemID, Manifest, Quantity);
		}
	}
	else
	{
		// Non-stackable: add Quantity separate entries
		for (int32 i = 0; i < Quantity; ++i)
		{
			Server_AddNewItemFromManifest(ItemID, Manifest, 1);
		}
	}

	return true;
}

void UInv_InventoryComponent::Server_AddNewItemFromManifest_Implementation(FName ItemID, FInv_ItemManifest Manifest, int32 StackCount)
{
	if (ItemID.IsNone() || StackCount <= 0) return;

	UInv_InventoryItem* NewItem = InventoryList.AddEntryFromManifest(Manifest);
	if (!IsValid(NewItem)) return;

	NewItem->SetItemID(ItemID);               
	NewItem->SetTotalStackCount(StackCount);

	EmitInvDeltaByItemID(ItemID, +StackCount, this);

	if (GetOwner()->GetNetMode() == NM_ListenServer || GetOwner()->GetNetMode() == NM_Standalone)
	{
		OnItemAdded.Broadcast(NewItem);
	}
}

void UInv_InventoryComponent::Server_AddStacksToItemFromManifest_Implementation(FName ItemID, FGameplayTag ItemType, int32 StackCount)
{
	if (ItemID.IsNone() || StackCount <= 0) return;

	UInv_InventoryItem* Item = InventoryList.FindFirstItemByID(ItemID);
	if (!IsValid(Item)) return;

	// Ensure identity is persisted on the entry (defensive for legacy items)
	if (Item->GetItemID().IsNone())
	{
		Item->SetItemID(ItemID);
	}

	Item->SetTotalStackCount(Item->GetTotalStackCount() + StackCount);

	EmitInvDeltaByItemID(ItemID, +StackCount, this);
}

void UInv_InventoryComponent::TryAddItem(UInv_ItemComponent* ItemComponent)
{
	FInv_SlotAvailabilityResult Result = InventoryMenu->HasRoomForItem(ItemComponent);

	UInv_InventoryItem* FoundItem = InventoryList.FindFirstItemByID(ItemComponent->GetItemID());
	Result.Item = FoundItem;

	if (Result.TotalRoomToFill == 0)
	{
		NoRoomInInventory.Broadcast();
		return;
	}
	
	if (Result.Item.IsValid() && Result.bStackable)
	{
		// Add stacks to an item that already exists in the inventory. We only want to update the stack count,
		// not create a new item of this type.
		OnStackChange.Broadcast(Result);
		Server_AddStacksToItem(ItemComponent, Result.TotalRoomToFill, Result.Remainder);
	}
	else if (Result.TotalRoomToFill > 0)
	{
		// This item type doesn't exist in the inventory. Create a new one and update all pertinent slots.
		Server_AddNewItem(ItemComponent, Result.bStackable ? Result.TotalRoomToFill : 0, Result.Remainder);
	}
}

void UInv_InventoryComponent::Server_RemoveItemByID_Implementation(FName ItemID, int32 Quantity, UObject* Context)
{
	if (ItemID.IsNone() || Quantity <= 0) return;
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	const bool bLocalServerUI =
		(GetOwner()->GetNetMode() == NM_ListenServer || GetOwner()->GetNetMode() == NM_Standalone);

	int32 Remaining = Quantity;
	int32 RemovedTotal = 0;

	while (Remaining > 0)
	{
		UInv_InventoryItem* Item = InventoryList.FindFirstItemByID(ItemID);
		if (!IsValid(Item))
		{
			break;
		}

		const int32 CurrentStack = Item->GetTotalStackCount();
		if (CurrentStack <= 0)
		{
			// Broken entry, remove it.
			if (bLocalServerUI)
			{
				OnItemRemoved.Broadcast(Item);
			}

			InventoryList.RemoveEntry(Item);
			continue;
		}

		const int32 Take = FMath::Min(CurrentStack, Remaining);
		const int32 NewStack = CurrentStack - Take;

		if (NewStack <= 0)
		{
			// IMPORTANT: notify local UI before removing (listen/standalone won't get PreReplicatedRemove)
			if (bLocalServerUI)
			{
				OnItemRemoved.Broadcast(Item);
			}

			InventoryList.RemoveEntry(Item);
		}
		else
		{
			Item->SetTotalStackCount(NewStack);

			// If your stack count is NOT replicated via the item subobject, you may also need a “stack changed”
			// notification here. But often UI reads directly from the item and updates automatically on listen server.
		}

		Remaining -= Take;
		RemovedTotal += Take;
	}

	if (RemovedTotal > 0)
	{
		EmitInvDeltaByItemID(ItemID, -RemovedTotal, Context);
	}
}

void UInv_InventoryComponent::Server_AddNewItem_Implementation(UInv_ItemComponent* ItemComponent, int32 StackCount, int32 Remainder)
{
	if (!IsValid(ItemComponent) || StackCount < 0) return;

	UInv_InventoryItem* NewItem = InventoryList.AddEntry(ItemComponent);
	if (!IsValid(NewItem)) return;

	const FName ItemID = ItemComponent->GetItemID();
	NewItem->SetItemID(ItemID);
	NewItem->SetTotalStackCount(StackCount);

	if (StackCount > 0)
	{
		EmitInvDeltaByItemID(ItemID, +StackCount, this);
	}

	if (GetOwner()->GetNetMode() == NM_ListenServer || GetOwner()->GetNetMode() == NM_Standalone)
	{
		OnItemAdded.Broadcast(NewItem);
	}

	if (Remainder == 0)
	{
		ItemComponent->PickedUp();
	}
	else if (FInv_StackableFragment* StackableFragment =
		ItemComponent->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>())
	{
		StackableFragment->SetStackCount(Remainder);
	}
}

void UInv_InventoryComponent::Server_AddStacksToItem_Implementation(UInv_ItemComponent* ItemComponent, int32 StackCount, int32 Remainder)
{
	if (!IsValid(ItemComponent) || StackCount <= 0) return;

	const FGameplayTag ItemType = ItemComponent->GetItemManifest().GetItemType();
	UInv_InventoryItem* Item = InventoryList.FindFirstItemByID(ItemComponent->GetItemID());
	if (!IsValid(Item)) return;

	const FName ItemID = ItemComponent->GetItemID();
	if (Item->GetItemID().IsNone())
	{
		Item->SetItemID(ItemID);
	}

	Item->SetTotalStackCount(Item->GetTotalStackCount() + StackCount);

	EmitInvDeltaByItemID(ItemID, +StackCount, this);

	if (Remainder == 0)
	{
		ItemComponent->PickedUp();
	}
	else if (FInv_StackableFragment* StackableFragment = ItemComponent->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>())
	{
		StackableFragment->SetStackCount(Remainder);
	}
}

void UInv_InventoryComponent::Server_DropItem_Implementation(UInv_InventoryItem* Item, int32 StackCount)
{
	const int32 NewStackCount = Item->GetTotalStackCount() - StackCount;
	if (NewStackCount <= 0)
	{
		InventoryList.RemoveEntry(Item);
	}
	else
	{
		Item->SetTotalStackCount(NewStackCount);
	}

	const FName ItemID = ResolveItemIDFromInventoryItem(Item);
	EmitInvDeltaByItemID(ItemID, -StackCount, this);

	SpawnDroppedItem(Item, StackCount);
}

void UInv_InventoryComponent::SpawnDroppedItem(UInv_InventoryItem* Item, int32 StackCount)
{
	const APawn* OwningPawn = OwningController->GetPawn();
	FVector RotatedForward = OwningPawn->GetActorForwardVector();
	RotatedForward = RotatedForward.RotateAngleAxis(FMath::FRandRange(DropSpawnAngleMin, DropSpawnAngleMax), FVector::UpVector);
	FVector SpawnLocation = OwningPawn->GetActorLocation() + RotatedForward * FMath::FRandRange(DropSpawnDistanceMin, DropSpawnDistanceMax);
	SpawnLocation.Z -= RelativeSpawnElevation;
	const FRotator SpawnRotation = FRotator::ZeroRotator;

	FInv_ItemManifest& ItemManifest = Item->GetItemManifestMutable();
	if (FInv_StackableFragment* StackableFragment = ItemManifest.GetFragmentOfTypeMutable<FInv_StackableFragment>())
	{
		StackableFragment->SetStackCount(StackCount);
	}
	const FName ItemID = Item->GetItemID();
	ItemManifest.SpawnPickupActor(this, ItemID, SpawnLocation, SpawnRotation);
}


void UInv_InventoryComponent::EmitInvDeltaByItemID(FName ItemID, int32 DeltaQty, UObject* Context)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (ItemID.IsNone() || DeltaQty == 0) return;

	OnInvDelta.Broadcast(ItemID, DeltaQty, Context);
}

void UInv_InventoryComponent::Server_ConsumeItem_Implementation(UInv_InventoryItem* Item)
{
	const int32 NewStackCount = Item->GetTotalStackCount() - 1;
	if (NewStackCount <= 0)
	{
		InventoryList.RemoveEntry(Item);
	}
	else
	{
		Item->SetTotalStackCount(NewStackCount);
	}

	const FName ItemID = ResolveItemIDFromInventoryItem(Item);
	EmitInvDeltaByItemID(ItemID, -1, this);

	if (FInv_ConsumableFragment* ConsumableFragment = Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_ConsumableFragment>())
	{
		ConsumableFragment->OnConsume(OwningController.Get());
	}
}

void UInv_InventoryComponent::Server_EquipSlotClicked_Implementation(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip)
{
	Multicast_EquipSlotClicked(ItemToEquip, ItemToUnequip);
}

void UInv_InventoryComponent::Multicast_EquipSlotClicked_Implementation(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip)
{
	// Equipment Component will listen to these delegates
	OnItemEquipped.Broadcast(ItemToEquip);
	OnItemUnequipped.Broadcast(ItemToUnequip);
}

void UInv_InventoryComponent::ToggleInventoryMenu()
{
	if (bInventoryMenuOpen)
	{
		CloseInventoryMenu();
	}
	else
	{
		OpenInventoryMenu();
	}
	OnInventoryMenuToggled.Broadcast(bInventoryMenuOpen);
}

void UInv_InventoryComponent::AddRepSubObj(UObject* SubObj)
{
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && IsValid(SubObj))
	{
		AddReplicatedSubObject(SubObj);
	}
}

void UInv_InventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	
	ConstructInventory();
}

void UInv_InventoryComponent::ConstructInventory()
{
	OwningController = Cast<APlayerController>(GetOwner());
	checkf(OwningController.IsValid(), TEXT("Inventory Component should have a Player Controller as Owner."));
	if (!OwningController->IsLocalController()) return;

	InventoryMenu = CreateWidget<UInv_InventoryBase>(OwningController.Get(), InventoryMenuClass);
	if (!IsValid(InventoryMenu)) return;

	InventoryMenu->AddToViewport();

	// Only UI state; DO NOT set input mode here
	InventoryMenu->SetVisibility(ESlateVisibility::Collapsed);
	InventoryMenu->SetIsEnabled(false);
	bInventoryMenuOpen = false;
}

void UInv_InventoryComponent::OpenInventoryMenu()
{
	if (!IsValid(InventoryMenu) || !OwningController.IsValid()) return;

	InventoryMenu->SetIsEnabled(true);
	InventoryMenu->SetVisibility(ESlateVisibility::Visible);
	bInventoryMenuOpen = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);

	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(true);

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
		GEngine->GameViewport->SetHideCursorDuringCapture(false);
	}
}

void UInv_InventoryComponent::CloseInventoryMenu()
{
	if (!IsValid(InventoryMenu) || !OwningController.IsValid()) return;

	InventoryMenu->SetVisibility(ESlateVisibility::Collapsed);
	InventoryMenu->SetIsEnabled(false);
	bInventoryMenuOpen = false;

	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(false);
	OwningController->SetInputMode(InputMode);

	OwningController->SetShowMouseCursor(true);
	OwningController->bEnableClickEvents = true;
	OwningController->bEnableMouseOverEvents = true;

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
		GEngine->GameViewport->SetHideCursorDuringCapture(false);
	}
}
