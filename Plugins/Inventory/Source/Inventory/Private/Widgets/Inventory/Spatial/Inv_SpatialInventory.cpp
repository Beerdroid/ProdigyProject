// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/Inventory/Spatial/Inv_SpatialInventory.h"

#include "Inventory.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "Widgets/Inventory/Spatial/Inv_InventoryGrid.h"
#include "Inventory.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Items/Inv_InventoryItem.h"
#include "Widgets/ItemDescription/Inv_ItemDescription.h"
#include "Blueprint/WidgetTree.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Widgets/Inventory/GridSlots/Inv_EquippedGridSlot.h"
#include "Widgets/Inventory/HoverItem/Inv_HoverItem.h"
#include "Widgets/Inventory/SlottedItems/Inv_EquippedSlottedItem.h"

static FVector2D ClampToViewport(const FVector2D& ViewportSize, const FVector2D& WidgetSize, FVector2D DesiredPos)
{
	DesiredPos.X = FMath::Clamp(DesiredPos.X, 0.f, FMath::Max(0.f, ViewportSize.X - WidgetSize.X));
	DesiredPos.Y = FMath::Clamp(DesiredPos.Y, 0.f, FMath::Max(0.f, ViewportSize.Y - WidgetSize.Y));
	return DesiredPos;
}

void UInv_SpatialInventory::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Buttons (guarded)
	if (IsValid(Button_Equippables))
	{
		Button_Equippables->OnClicked.RemoveDynamic(this, &ThisClass::ShowEquippables);
		Button_Equippables->OnClicked.AddDynamic(this, &ThisClass::ShowEquippables);
	}

	if (IsValid(Button_Consumables))
	{
		Button_Consumables->OnClicked.RemoveDynamic(this, &ThisClass::ShowConsumables);
		Button_Consumables->OnClicked.AddDynamic(this, &ThisClass::ShowConsumables);
	}

	if (IsValid(Button_Craftables))
	{
		Button_Craftables->OnClicked.RemoveDynamic(this, &ThisClass::ShowCraftables);
		Button_Craftables->OnClicked.AddDynamic(this, &ThisClass::ShowCraftables);
	}

	// No more canvas dependency:
	// Grid_Equippables->SetOwningCanvas(CanvasPanel);
	// Grid_Consumables->SetOwningCanvas(CanvasPanel);
	// Grid_Craftables->SetOwningCanvas(CanvasPanel);

	// Default tab
	if (IsValid(Grid_Equippables) && IsValid(Switcher) && IsValid(Button_Equippables))
	{
		ShowEquippables();
	}
	else
	{
		// Fallback: try any grid that exists
		if (IsValid(Grid_Consumables) && IsValid(Button_Consumables)) ShowConsumables();
		else if (IsValid(Grid_Craftables) && IsValid(Button_Craftables)) ShowCraftables();
	}

	// Cache equipped slots + bind click
	EquippedGridSlots.Reset();

	if (IsValid(WidgetTree))
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (UInv_EquippedGridSlot* EquippedGridSlot = Cast<UInv_EquippedGridSlot>(Widget))
			{
				EquippedGridSlots.Add(EquippedGridSlot);

				EquippedGridSlot->EquippedGridSlotClicked.RemoveDynamic(this, &ThisClass::EquippedGridSlotClicked);
				EquippedGridSlot->EquippedGridSlotClicked.AddDynamic(this, &ThisClass::EquippedGridSlotClicked);
			}
		});
	}
}

void UInv_SpatialInventory::EquippedGridSlotClicked(UInv_EquippedGridSlot* EquippedGridSlot, const FGameplayTag& EquipmentTypeTag)
{
	// Check to see if we can equip the Hover Item
	if (!CanEquipHoverItem(EquippedGridSlot, EquipmentTypeTag)) return;

	UInv_HoverItem* HoverItem = GetHoverItem();
	
	// Create an Equipped Slotted Item and add it to the Equipped Grid Slot (call EquippedGridSlot->OnItemEquipped())
	const float TileSize = UInv_InventoryStatics::GetInventoryWidget(GetOwningPlayer())->GetTileSize();
	UInv_EquippedSlottedItem* EquippedSlottedItem = EquippedGridSlot->OnItemEquipped(
		HoverItem->GetInventoryItem(),
		EquipmentTypeTag,
		TileSize
	);
	EquippedSlottedItem->OnEquippedSlottedItemClicked.AddDynamic(this, &ThisClass::EquippedSlottedItemClicked);
	
	// Inform the server that we've equipped an item (potentially unequipping an item as well)
	UInv_InventoryComponent* InventoryComponent = UInv_InventoryStatics::GetInventoryComponent(GetOwningPlayer());
	check(IsValid(InventoryComponent)); 

	InventoryComponent->Server_EquipSlotClicked(HoverItem->GetInventoryItem(), nullptr);
	
	// Clear the Hover Item
	Grid_Equippables->ClearHoverItem();
}

void UInv_SpatialInventory::EquippedSlottedItemClicked(UInv_EquippedSlottedItem* EquippedSlottedItem)
{
	// Remove the Item Description
	UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());

	if (IsValid(GetHoverItem()) && GetHoverItem()->IsStackable()) return;
	
	// Get Item to Equip
	UInv_InventoryItem* ItemToEquip = IsValid(GetHoverItem()) ? GetHoverItem()->GetInventoryItem() : nullptr;
	
	// Get Item to Unequip
	UInv_InventoryItem* ItemToUnequip = EquippedSlottedItem->GetInventoryItem();
	
	// Get the Equipped Grid Slot holding this item
	UInv_EquippedGridSlot* EquippedGridSlot = FindSlotWithEquippedItem(ItemToUnequip);
	
	// Clear the equipped grid slot of this item (set its inventory item to nullptr)
	ClearSlotOfItem(EquippedGridSlot);

	// Assign previously equipped item as the hover item
	Grid_Equippables->AssignHoverItem(ItemToUnequip);
	
	// Remove of the equipped slotted item from the equipped grid slot
	RemoveEquippedSlottedItem(EquippedSlottedItem);
	
	// Make a new equipped slotted item (for the item we held in HoverItem)
	MakeEquippedSlottedItem(EquippedSlottedItem, EquippedGridSlot, ItemToEquip);
	
	// Broadcast delegates for OnItemEquipped/OnItemUnequipped (from the IC)
	BroadcastSlotClickedDelegates(ItemToEquip, ItemToUnequip);
}

FReply UInv_SpatialInventory::NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ActiveGrid->DropItem();
	return FReply::Handled();
}

void UInv_SpatialInventory::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsValid(ItemDescription)) return;

	if (ItemDescription->IsVisible())
	{
		SetItemDescriptionSizeAndPosition(ItemDescription);
	}

	if (IsValid(EquippedItemDescription) && EquippedItemDescription->IsVisible())
	{
		SetEquippedItemDescriptionSizeAndPosition(ItemDescription, EquippedItemDescription);
	}
}

void UInv_SpatialInventory::SetItemDescriptionSizeAndPosition(UInv_ItemDescription* Description) const
{
	if (!IsValid(Description)) return;

	// Size
	const FVector2D WidgetSize = Description->GetBoxSize();
	Description->SetDesiredSizeInViewport(WidgetSize);

	// Viewport clamp
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(GetOwningPlayer());

	// Mouse position in viewport space
	const FVector2D MousePos = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());

	FVector2D Pos = ClampToViewport(ViewportSize, WidgetSize, MousePos);
	Description->SetPositionInViewport(Pos, false);
}

void UInv_SpatialInventory::SetEquippedItemDescriptionSizeAndPosition(UInv_ItemDescription* Description, UInv_ItemDescription* EquippedDescription) const
{
	if (!IsValid(Description) || !IsValid(EquippedDescription)) return;

	const FVector2D MainSize = Description->GetBoxSize();
	const FVector2D EquippedSize = EquippedDescription->GetBoxSize();

	EquippedDescription->SetDesiredSizeInViewport(EquippedSize);

	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(GetOwningPlayer());
	const FVector2D MousePos = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());

	// Place the main tooltip clamped first
	FVector2D MainPos = ClampToViewport(ViewportSize, MainSize, MousePos);

	// Place equipped tooltip to the LEFT of main tooltip
	FVector2D EquippedPos = MainPos;
	EquippedPos.X -= EquippedSize.X;

	EquippedPos = ClampToViewport(ViewportSize, EquippedSize, EquippedPos);

	EquippedDescription->SetPositionInViewport(EquippedPos, false);
}

bool UInv_SpatialInventory::CanEquipHoverItem(UInv_EquippedGridSlot* EquippedGridSlot, const FGameplayTag& EquipmentTypeTag) const
{
	if (!IsValid(EquippedGridSlot) || EquippedGridSlot->GetInventoryItem().IsValid()) return false;

	UInv_HoverItem* HoverItem = GetHoverItem();
	if (!IsValid(HoverItem)) return false;

	UInv_InventoryItem* HeldItem = HoverItem->GetInventoryItem();

	return HasHoverItem() && IsValid(HeldItem) &&
		!HoverItem->IsStackable() &&
			HeldItem->GetItemManifest().GetItemCategory() == EInv_ItemCategory::Equippable &&
				HeldItem->GetItemManifest().GetItemType().MatchesTag(EquipmentTypeTag);
}

UInv_EquippedGridSlot* UInv_SpatialInventory::FindSlotWithEquippedItem(UInv_InventoryItem* EquippedItem) const
{
	auto* FoundEquippedGridSlot = EquippedGridSlots.FindByPredicate([EquippedItem](const UInv_EquippedGridSlot* GridSlot)
	{
		return GridSlot->GetInventoryItem() == EquippedItem;
	});
	return FoundEquippedGridSlot ? *FoundEquippedGridSlot : nullptr;
}

void UInv_SpatialInventory::ClearSlotOfItem(UInv_EquippedGridSlot* EquippedGridSlot)
{
	if (IsValid(EquippedGridSlot))
	{
		EquippedGridSlot->SetEquippedSlottedItem(nullptr);
		EquippedGridSlot->SetInventoryItem(nullptr);
	}
}

void UInv_SpatialInventory::RemoveEquippedSlottedItem(UInv_EquippedSlottedItem* EquippedSlottedItem)
{
	if (!IsValid(EquippedSlottedItem)) return;

	if (EquippedSlottedItem->OnEquippedSlottedItemClicked.IsAlreadyBound(this, &ThisClass::EquippedSlottedItemClicked))
	{
		EquippedSlottedItem->OnEquippedSlottedItemClicked.RemoveDynamic(this, &ThisClass::EquippedSlottedItemClicked);
	}
	EquippedSlottedItem->RemoveFromParent();
}

void UInv_SpatialInventory::MakeEquippedSlottedItem(UInv_EquippedSlottedItem* EquippedSlottedItem, UInv_EquippedGridSlot* EquippedGridSlot, UInv_InventoryItem* ItemToEquip)
{
	if (!IsValid(EquippedGridSlot)) return;

	UInv_EquippedSlottedItem* SlottedItem = EquippedGridSlot->OnItemEquipped(
		ItemToEquip,
		EquippedSlottedItem->GetEquipmentTypeTag(),
		UInv_InventoryStatics::GetInventoryWidget(GetOwningPlayer())->GetTileSize());
	if (IsValid(SlottedItem)) SlottedItem->OnEquippedSlottedItemClicked.AddDynamic(this, &ThisClass::EquippedSlottedItemClicked);

	EquippedGridSlot->SetEquippedSlottedItem(SlottedItem);
}

void UInv_SpatialInventory::BroadcastSlotClickedDelegates(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip) const
{
	UInv_InventoryComponent* InventoryComponent = UInv_InventoryStatics::GetInventoryComponent(GetOwningPlayer());
	check(IsValid(InventoryComponent));
	InventoryComponent->Server_EquipSlotClicked(ItemToEquip, ItemToUnequip);
}

FInv_SlotAvailabilityResult UInv_SpatialInventory::HasRoomForItem(UInv_ItemComponent* ItemComponent) const
{
	UE_LOG(LogTemp, Warning, TEXT("HasRoomForItem(Component) Cat=%d ItemID=%s"),
	(int32)UInv_InventoryStatics::GetItemCategoryFromItemComp(ItemComponent),
	*ItemComponent->GetItemID().ToString());
	
	switch (UInv_InventoryStatics::GetItemCategoryFromItemComp(ItemComponent))
	{
		case EInv_ItemCategory::Equippable:
			return Grid_Equippables->HasRoomForItem(ItemComponent);
		case EInv_ItemCategory::Consumable:
			return Grid_Consumables->HasRoomForItem(ItemComponent);
		case EInv_ItemCategory::Craftable:
			return Grid_Craftables->HasRoomForItem(ItemComponent);
		default:
			UE_LOG(LogInventory, Error, TEXT("ItemComponent doesn't have a valid Item Category."))
			return FInv_SlotAvailabilityResult();
	}
}

FInv_SlotAvailabilityResult UInv_SpatialInventory::HasRoomForItem(FName ItemID, const FInv_ItemManifest& Manifest,
	int32 Quantity) const
{
	UE_LOG(LogTemp, Warning, TEXT("HasRoomForItem(Manifest) Cat=%d ItemID=%s Qty=%d"),
	(int32)Manifest.GetItemCategory(),
	*ItemID.ToString(),
	Quantity);
	
	if (ItemID.IsNone() || Quantity <= 0)
	{
		return FInv_SlotAvailabilityResult();
	}

	switch (Manifest.GetItemCategory())
	{
	case EInv_ItemCategory::Equippable:
		return Grid_Equippables ? Grid_Equippables->HasRoomForItem(ItemID, Manifest, Quantity) : FInv_SlotAvailabilityResult();

	case EInv_ItemCategory::Consumable:
		return Grid_Consumables ? Grid_Consumables->HasRoomForItem(ItemID, Manifest, Quantity) : FInv_SlotAvailabilityResult();

	case EInv_ItemCategory::Craftable:
		return Grid_Craftables ? Grid_Craftables->HasRoomForItem(ItemID, Manifest, Quantity) : FInv_SlotAvailabilityResult();

	default:
		UE_LOG(LogInventory, Error, TEXT("Manifest doesn't have a valid Item Category (ItemID=%s)"), *ItemID.ToString());
		return FInv_SlotAvailabilityResult();
	}
}

void UInv_SpatialInventory::OnItemHovered(UInv_InventoryItem* Item)
{
	const auto& Manifest = Item->GetItemManifest();
	UInv_ItemDescription* DescriptionWidget = GetItemDescription();
	DescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);

	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(DescriptionTimer);
	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(EquippedDescriptionTimer);

	FTimerDelegate DescriptionTimerDelegate;
	DescriptionTimerDelegate.BindLambda([this, Item, &Manifest, DescriptionWidget]()
	{
		GetItemDescription()->SetVisibility(ESlateVisibility::HitTestInvisible);
		Manifest.AssimilateInventoryFragments(DescriptionWidget);
		
		// For the second item description, showing the equipped item of this type.
		FTimerDelegate EquippedDescriptionTimerDelegate;
		EquippedDescriptionTimerDelegate.BindUObject(this, &ThisClass::ShowEquippedItemDescription, Item);
		GetOwningPlayer()->GetWorldTimerManager().SetTimer(EquippedDescriptionTimer, EquippedDescriptionTimerDelegate, EquippedDescriptionTimerDelay, false);
	});

	GetOwningPlayer()->GetWorldTimerManager().SetTimer(DescriptionTimer, DescriptionTimerDelegate, DescriptionTimerDelay, false);
}

void UInv_SpatialInventory::OnItemUnHovered()
{
	GetItemDescription()->SetVisibility(ESlateVisibility::Collapsed);
	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(DescriptionTimer);
	GetEquippedItemDescription()->SetVisibility(ESlateVisibility::Collapsed);
	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(EquippedDescriptionTimer);
}

bool UInv_SpatialInventory::HasHoverItem() const
{
	if (Grid_Equippables->HasHoverItem()) return true;
	if (Grid_Consumables->HasHoverItem()) return true;
	if (Grid_Craftables->HasHoverItem()) return true;
	return false;
}

UInv_HoverItem* UInv_SpatialInventory::GetHoverItem() const
{
	if (!ActiveGrid.IsValid()) return nullptr;
	return ActiveGrid->GetHoverItem();
}

float UInv_SpatialInventory::GetTileSize() const
{
	return Grid_Equippables->GetTileSize();
}

void UInv_SpatialInventory::SetSourceInventory(UInv_InventoryComponent* InComp)
{
	UE_LOG(LogTemp, Warning, TEXT("SetSourceInventory start"));
	Super::SetSourceInventory(InComp);

	if (!IsValid(SourceInventory)) return;

	if (IsValid(Grid_Equippables))  Grid_Equippables->SetInventoryComponent(SourceInventory);
	if (IsValid(Grid_Consumables))  Grid_Consumables->SetInventoryComponent(SourceInventory);
	if (IsValid(Grid_Craftables))   Grid_Craftables->SetInventoryComponent(SourceInventory);

	// Optional: ensure ActiveGrid points at the currently visible grid
	// ActiveGrid = Grid_Equippables; (or whatever Switcher index is)
	UE_LOG(LogTemp, Warning, TEXT("SetSourceInventory end"));
	
}

void UInv_SpatialInventory::ShowEquippedItemDescription(UInv_InventoryItem* Item)
{
	const auto& Manifest = Item->GetItemManifest();
	const FInv_EquipmentFragment* EquipmentFragment = Manifest.GetFragmentOfType<FInv_EquipmentFragment>();
	if (!EquipmentFragment) return;

	const FGameplayTag HoveredEquipmentType = EquipmentFragment->GetEquipmentType();
	
	auto EquippedGridSlot = EquippedGridSlots.FindByPredicate([Item](const UInv_EquippedGridSlot* GridSlot)
	{
		return GridSlot->GetInventoryItem() == Item;
	});
	if (EquippedGridSlot != nullptr) return; // The hovered item is already equipped, we're already showing its Item Description

	// It's not equipped, so find the equipped item with the same equipment type
	auto FoundEquippedSlot = EquippedGridSlots.FindByPredicate([HoveredEquipmentType](const UInv_EquippedGridSlot* GridSlot)
	{
		UInv_InventoryItem* InventoryItem = GridSlot->GetInventoryItem().Get();
		return IsValid(InventoryItem) ? InventoryItem->GetItemManifest().GetFragmentOfType<FInv_EquipmentFragment>()->GetEquipmentType() == HoveredEquipmentType : false ;
	});
	UInv_EquippedGridSlot* EquippedSlot = FoundEquippedSlot ? *FoundEquippedSlot : nullptr;
	if (!IsValid(EquippedSlot)) return; // No equipped item with the same equipment type

	UInv_InventoryItem* EquippedItem = EquippedSlot->GetInventoryItem().Get();
	if (!IsValid(EquippedItem)) return;

	const auto& EquippedItemManifest = EquippedItem->GetItemManifest();
	UInv_ItemDescription* DescriptionWidget = GetEquippedItemDescription();

	auto EquippedDescriptionWidget = GetEquippedItemDescription();
	
	EquippedDescriptionWidget->Collapse();
	DescriptionWidget->SetVisibility(ESlateVisibility::HitTestInvisible);	
	EquippedItemManifest.AssimilateInventoryFragments(EquippedDescriptionWidget);
}

UInv_ItemDescription* UInv_SpatialInventory::GetItemDescription()
{
	if (!IsValid(ItemDescription))
	{
		ItemDescription = CreateWidget<UInv_ItemDescription>(GetOwningPlayer(), ItemDescriptionClass);
		if (!IsValid(ItemDescription)) return nullptr;

		// High Z so it draws above both inventories
		ItemDescription->AddToViewport(999);

		// Make sure tooltip never blocks clicks
		ItemDescription->SetVisibility(ESlateVisibility::Collapsed);
		ItemDescription->SetIsEnabled(false); // optional (visibility is the main thing)
	}
	return ItemDescription;
}

UInv_ItemDescription* UInv_SpatialInventory::GetEquippedItemDescription()
{
	if (!IsValid(EquippedItemDescription))
	{
		EquippedItemDescription = CreateWidget<UInv_ItemDescription>(GetOwningPlayer(), EquippedItemDescriptionClass);
		if (!IsValid(EquippedItemDescription)) return nullptr;

		EquippedItemDescription->AddToViewport(998); // just under main if you want
		EquippedItemDescription->SetVisibility(ESlateVisibility::Collapsed);
		EquippedItemDescription->SetIsEnabled(false);
	}
	return EquippedItemDescription;
}

void UInv_SpatialInventory::ShowEquippables()
{
	SetActiveGrid(Grid_Equippables, Button_Equippables);
}

void UInv_SpatialInventory::ShowConsumables()
{
	SetActiveGrid(Grid_Consumables, Button_Consumables);
}

void UInv_SpatialInventory::ShowCraftables()
{
	SetActiveGrid(Grid_Craftables, Button_Craftables);
}

void UInv_SpatialInventory::DisableButton(UButton* Button)
{
	Button_Equippables->SetIsEnabled(true);
	Button_Consumables->SetIsEnabled(true);
	Button_Craftables->SetIsEnabled(true);
	Button->SetIsEnabled(false);
}

void UInv_SpatialInventory::SetActiveGrid(UInv_InventoryGrid* Grid, UButton* Button)
{
	if (ActiveGrid.IsValid())
	{
		ActiveGrid->HideCursor();
		ActiveGrid->OnHide();
	}
	ActiveGrid = Grid;
	if (ActiveGrid.IsValid()) ActiveGrid->ShowCursor();
	DisableButton(Button);
	Switcher->SetActiveWidget(Grid);
}

