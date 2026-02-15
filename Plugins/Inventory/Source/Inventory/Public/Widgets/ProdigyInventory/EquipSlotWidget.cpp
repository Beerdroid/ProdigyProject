#include "EquipSlotWidget.h"

#include "InvDragVisualWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "ProdigyInventory/InventoryComponent.h"
#include "ProdigyInventory/InvDragDropOp.h" // your drag op

class UInvDragVisualWidget;

void UEquipSlotWidget::SetInventory(UInventoryComponent* InInventory)
{
	Inventory = InInventory;
	if (Inventory.IsValid())
	{
		Inventory->OnItemEquipped.RemoveAll(this);
		Inventory->OnItemUnequipped.RemoveAll(this);

		Inventory->OnItemEquipped.AddDynamic(this, &ThisClass::HandleItemEquipped);
		Inventory->OnItemUnequipped.AddDynamic(this, &ThisClass::HandleItemUnequipped);
	}
	RefreshVisual();
}

void UEquipSlotWidget::HandleItemEquipped(FGameplayTag InEquipSlotTag, FName InItemID)
{
	if (!EquipSlotTag.IsValid()) return;
	if (!InEquipSlotTag.MatchesTagExact(EquipSlotTag)) return;

	RefreshVisual();
}

void UEquipSlotWidget::HandleItemUnequipped(FGameplayTag InEquipSlotTag, FName InItemID)
{
	if (!EquipSlotTag.IsValid()) return;
	if (!InEquipSlotTag.MatchesTagExact(EquipSlotTag)) return;

	RefreshVisual();
}

void UEquipSlotWidget::RefreshVisual()
{
	const bool bHasItemIcon    = IsValid(ItemIcon);
	const bool bHasDefaultIcon = IsValid(DefaultIcon);

	auto SetEmptyState = [&]()
	{
		if (bHasItemIcon)
		{
			ItemIcon->SetBrushFromTexture(nullptr, /*bMatchSize*/ false);
			ItemIcon->SetVisibility(ESlateVisibility::Hidden);
		}
		if (bHasDefaultIcon)
		{
			DefaultIcon->SetVisibility(ESlateVisibility::Visible);
		}
	};

	// invalid setup => empty
	if (!Inventory.IsValid() || !EquipSlotTag.IsValid())
	{
		SetEmptyState();
		return;
	}

	// no equipped item => empty
	FName EquippedItemID = NAME_None;
	if (!Inventory->GetEquippedItem(EquipSlotTag, EquippedItemID) || EquippedItemID.IsNone())
	{
		SetEmptyState();
		return;
	}

	// resolve item row
	FItemRow Row;
	if (!Inventory->TryGetItemDef(EquippedItemID, Row))
	{
		SetEmptyState();
		return;
	}

	// load icon
	UTexture2D* Tex = Row.Icon.LoadSynchronous();
	if (!IsValid(Tex))
	{
		SetEmptyState();
		return;
	}

	// equipped state
	if (bHasItemIcon)
	{
		ItemIcon->SetBrushFromTexture(Tex, /*bMatchSize*/ true);
		ItemIcon->SetVisibility(ESlateVisibility::Visible);
	}

	if (bHasDefaultIcon)
	{
		DefaultIcon->SetVisibility(ESlateVisibility::Hidden);
	}
}

bool UEquipSlotWidget::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	if (!Inventory.IsValid()) return false;
	if (!EquipSlotTag.IsValid()) return false;

	UInvDragDropOp* Op = Cast<UInvDragDropOp>(InOperation);
	if (!IsValid(Op)) return false;

	if (!IsValid(Op->SourceInventory) || Op->SourceIndex == INDEX_NONE) return false;

	TArray<int32> Changed;
	const bool bOk = Op->SourceInventory->EquipFromSlot(Op->SourceIndex, Changed);

	if (bOk)
	{
		// IMPORTANT: mark handled ONLY on success
		Op->bDropHandled = true;

		RefreshVisual();
		return true;
	}

	return false;
}

FReply UEquipSlotWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return FReply::Unhandled();
}

void UEquipSlotWidget::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (!Inventory.IsValid()) return;
	if (!EquipSlotTag.IsValid()) return;

	// Only allow drag if something is actually equipped
	FName EquippedItemID = NAME_None;
	if (!Inventory->GetEquippedItem(EquipSlotTag, EquippedItemID) || EquippedItemID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquipSlot Drag] No equipped item for %s"), *EquipSlotTag.ToString());
		return;
	}

	UInvDragDropOp* Op = NewObject<UInvDragDropOp>(this);
	Op->SourceInventory = Inventory.Get();
	Op->SourceIndex = INDEX_NONE;            // not a normal inventory slot
	Op->Quantity = 1;                        // equip items are single
	Op->bDropHandled = false;

	Op->bFromEquipSlot = true;
	Op->SourceEquipSlotTag = EquipSlotTag;

	Op->Pivot = EDragPivot::MouseDown;

	// Drag visual (must show the equipped item's icon, otherwise it stays default/white)
	if (DragVisualClass)
	{
		UUserWidget* Ghost = CreateWidget<UUserWidget>(GetOwningPlayer(), DragVisualClass);

		if (UInvDragVisualWidget* Typed = Cast<UInvDragVisualWidget>(Ghost))
		{
			FItemRow Row;
			if (Inventory->TryGetItemDef(EquippedItemID, Row))
			{
				FInventorySlotView View;
				View.bEmpty = false;
				View.SlotIndex = INDEX_NONE;
				View.ItemID = EquippedItemID;
				View.Quantity = 1;

				View.DisplayName = Row.DisplayName;
				View.Description = Row.Description;
				View.Icon = Row.Icon;
				View.Category = Row.Category;
				View.Tags = Row.Tags;
				View.Price = Row.SellValue;

				Typed->SetFromSlotView(View);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[EquipSlot Drag] TryGetItemDef failed for ItemID=%s"), *EquippedItemID.ToString());
			}
		}

		// IMPORTANT: ghost widget should be HitTestInvisible in BP (so drop works)
		Op->DefaultDragVisual = Ghost;
	}

	OutOperation = Op;

	UE_LOG(LogTemp, Warning,
		TEXT("[EquipSlot Drag] Start SlotTag=%s ItemID=%s"),
		*EquipSlotTag.ToString(),
		*EquippedItemID.ToString());
}