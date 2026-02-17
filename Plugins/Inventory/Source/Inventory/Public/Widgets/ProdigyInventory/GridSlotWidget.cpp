#include "Widgets/ProdigyInventory/GridSlotWidget.h"

#include "InvDragVisualWidget.h"
#include "InventoryWidgetBase.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "ProdigyInventory/InvDragDropOp.h"
#include "ProdigyInventory/InvPlayerController.h"
#include "ProdigyInventory/ItemTypes.h"

void UGridSlotWidget::InitSlot(UInventoryComponent* InInventory, int32 InIndex, UInventoryWidgetBase* InOwnerMenu)
{
	Inventory = InInventory;
	SlotIndex = InIndex;
	OwnerMenu = InOwnerMenu;
}

void UGridSlotWidget::SetSlotView(const FInventorySlotView& View)
{
	UE_LOG(LogTemp, Warning, TEXT("SetSlotView slot=%d empty=%d qty=%d item=%s"),
	       SlotIndex, (int32)View.bEmpty, View.Quantity, *View.ItemID.ToString());

	CachedView = View;

	bIsEmptyCached = View.bEmpty;

	// Item icon
	if (IsValid(ItemIcon))
	{
		if (View.bEmpty)
		{
			ItemIcon->SetBrushFromTexture(nullptr);
			ItemIcon->SetVisibility(ESlateVisibility::Hidden);
		}
		else
		{
			// TSoftObjectPtr<UTexture2D> -> UTexture2D*
			UTexture2D* IconTex = View.Icon.LoadSynchronous(); // loads once, then cached by engine

			if (IconTex)
			{
				ItemIcon->SetBrushFromTexture(IconTex, /*bMatchSize*/ true);
				ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				ItemIcon->SetBrushFromTexture(nullptr);
				ItemIcon->SetVisibility(ESlateVisibility::Hidden);
			}
		}
	}

	// Quantity
	if (IsValid(QuantityText))
	{
		if (!View.bEmpty && View.Quantity > 1)
		{
			QuantityText->SetText(FText::AsNumber(View.Quantity));
			QuantityText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			QuantityText->SetText(FText::GetEmpty());
			QuantityText->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

FReply UGridSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 1) Middle click => context menu
	if (InMouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		// You should have OwnerMenu set in InitSlot(..., OwnerMenu)
		if (OwnerMenu.IsValid())
		{
			OwnerMenu->OpenContextMenuForSlot(SlotIndex);
			return FReply::Handled();
		}

		// Fallback if you didn’t store OwnerMenu
		if (UInventoryWidgetBase* Menu = GetTypedOuter<UInventoryWidgetBase>())
		{
			Menu->OpenContextMenuForSlot(SlotIndex);
			return FReply::Handled();
		}

		return FReply::Handled();
	}

	// 2) Only LMB does drag logic
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			if (OwnerMenu.IsValid() && OwnerMenu->HandleClickSlotForSplit(SlotIndex))
			{
				UE_LOG(LogTemp, Warning, TEXT("Slot=%d consumed by SplitMode"), SlotIndex);
				return FReply::Handled();
			}
		}

		const bool bHasItem =
			Inventory.IsValid() &&
			SlotIndex != INDEX_NONE &&
			!Inventory->GetSlot(SlotIndex).IsEmpty();

		UE_LOG(LogTemp, Warning, TEXT("MouseDown slot=%d hasItem=%d vis=%d"),
		       SlotIndex, bHasItem ? 1 : 0, (int32)GetVisibility());

		if (bHasItem)
		{
			return UWidgetBlueprintLibrary::DetectDragIfPressed(
				InMouseEvent, this, EKeys::LeftMouseButton
			).NativeReply;
		}

		// Optional: if empty slot should still “eat” the click (to stop world movement):
		// return FReply::Handled();

		if (OwnerMenu.IsValid() && OwnerMenu->HandleClickSlotForSplit(SlotIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("Slot=%d consumed by SplitMode"), SlotIndex);
			return FReply::Handled();
		}

		return FReply::Unhandled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		AInvPlayerController* PC = GetOwningPlayer<AInvPlayerController>();
		if (!IsValid(PC)) return FReply::Handled();

		// Slot inventory is THIS widget's Inventory
		UInventoryComponent* ThisInv = Inventory.Get();
		if (!IsValid(ThisInv)) return FReply::Handled();

		PC->EnsurePlayerInventoryResolved();
		UInventoryComponent* PlayerInv = PC->InventoryComponent.Get();
		UInventoryComponent* ExternalInv = PC->ExternalInventory.Get();

		// Decide target by context
		if (ThisInv->InventoryType == EInventoryType::Merchant && IsValid(PlayerInv))
		{
			// BUY: merchant -> player (auto place into player inventory is nicer than targeting a slot)
			TArray<int32> ChangedS, ChangedT;
			// You can call AutoTransferTo_NoSwap if you implement it; otherwise pick a target slot (like first empty).
			const int32 TargetSlot = PlayerInv->FindFirstEmptySlot();
			if (TargetSlot != INDEX_NONE)
			{
				PC->ExecuteInventoryAction(ThisInv, SlotIndex, PlayerInv, TargetSlot, 1, EInvAction::QuickUse, /*swap*/
				                           false, /*full*/false);
			}
			return FReply::Handled();
		}

		if (ThisInv->InventoryType == EInventoryType::Player && IsValid(ExternalInv) && ExternalInv->InventoryType ==
			EInventoryType::Merchant)
		{
			// SELL: player -> merchant
			const int32 TargetSlot = ExternalInv->FindFirstEmptySlot();
			if (TargetSlot != INDEX_NONE)
			{
				PC->ExecuteInventoryAction(ThisInv, SlotIndex, ExternalInv, TargetSlot, 1, EInvAction::QuickUse,
				                           /*swap*/false, /*full*/false);
			}
			return FReply::Handled();
		}

		// Container move RMB behavior (optional): move to the "other side"
		if (ThisInv->InventoryType == EInventoryType::Player && IsValid(ExternalInv))
		{
			const int32 TargetSlot = ExternalInv->FindFirstEmptySlot();
			if (TargetSlot != INDEX_NONE)
			{
				PC->ExecuteInventoryAction(ThisInv, SlotIndex, ExternalInv, TargetSlot, -1, EInvAction::AutoMove,
				                           /*swap*/true, /*full*/false);
			}
			return FReply::Handled();
		}

		if (IsValid(PlayerInv) && ThisInv != PlayerInv)
		{
			const int32 TargetSlot = PlayerInv->FindFirstEmptySlot();
			if (TargetSlot != INDEX_NONE)
			{
				PC->ExecuteInventoryAction(ThisInv, SlotIndex, PlayerInv, TargetSlot, -1, EInvAction::AutoMove, /*swap*/
				                           true, /*full*/false);
			}
			return FReply::Handled();
		}

		return FReply::Handled();
	}

	// 3) Everything else: fallback to parent behavior
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UGridSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
                                           UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (UInventoryWidgetBase* Menu = Cast<UInventoryWidgetBase>(GetTypedOuter<UUserWidget>()))
	{
		Menu->HideTooltip();
	}

	if (!Inventory.IsValid() || SlotIndex == INDEX_NONE) return;

	const FInventorySlot GridSlot = Inventory->GetSlot(SlotIndex);
	if (GridSlot.IsEmpty()) return;

	UInvDragDropOp* Op = NewObject<UInvDragDropOp>(this);
	Op->SourceInventory = Inventory.Get();
	Op->SourceIndex = SlotIndex;
	Op->Quantity = -1;
	Op->Pivot = EDragPivot::MouseDown;

	if (DragVisualClass)
	{
		UUserWidget* Ghost = CreateWidget<UUserWidget>(GetOwningPlayer(), DragVisualClass);

		// If it’s our typed ghost, initialize it from a view
		if (UInvDragVisualWidget* Typed = Cast<UInvDragVisualWidget>(Ghost))
		{
			FInventorySlotView View;
			Inventory->GetSlotView(SlotIndex, View); // your existing view builder
			Typed->SetFromSlotView(View);
		}

		// IMPORTANT: ghost must be HitTestInvisible in BP
		Op->DefaultDragVisual = Ghost;
	}

	OutOperation = Op;
}

bool UGridSlotWidget::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

	UInvDragDropOp* Op = Cast<UInvDragDropOp>(InOperation);
	if (!Op)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SLOT DROP] No drag op"));
		return false;
	}

	UE_LOG(LogTemp, Warning,
	       TEXT("[SLOT DROP] Slot=%d SrcIdx=%d SrcInv=%s FromEquip=%d SrcEquipTag=%s"),
	       SlotIndex,
	       Op->SourceIndex,
	       *GetNameSafe(Op->SourceInventory),
	       Op->bFromEquipSlot ? 1 : 0,
	       *Op->SourceEquipSlotTag.ToString());

	UInventoryComponent* TargetInv = Inventory.Get();
	if (!IsValid(TargetInv))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SLOT DROP] Invalid TargetInv"));
		return false;
	}

	// ===== Equip -> Inventory (unequip) =====
	if (Op->bFromEquipSlot)
	{
		UInventoryComponent* SourceInv = Op->SourceInventory;

		if (!IsValid(SourceInv))
		{
			UE_LOG(LogTemp, Warning, TEXT("[SLOT DROP] EquipDrop: invalid SourceInv"));
			return false;
		}
		if (!Op->SourceEquipSlotTag.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[SLOT DROP] EquipDrop: invalid SourceEquipSlotTag"));
			return false;
		}

		// Mark handled so DragCancelled won't drop to world
		Op->bDropHandled = true;

		// Unequip goes back to inventory (stack/empty-slot rules apply)
		TArray<int32> Changed;
		const bool bOk = SourceInv->UnequipToInventory(Op->SourceEquipSlotTag, Changed);

		UE_LOG(LogTemp, Warning,
		       TEXT("[SLOT DROP] EquipDrop: UnequipToInventory SlotTag=%s Result=%d Changed=%d"),
		       *Op->SourceEquipSlotTag.ToString(),
		       bOk ? 1 : 0,
		       Changed.Num());

		return bOk;
	}

	// ===== Normal Inventory drag/drop =====

	UInventoryComponent* SourceInv = Op->SourceInventory;
	if (!IsValid(SourceInv))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SLOT DROP] Invalid SourceInv"));
		return false;
	}

	bool bOk = false;

	if (SourceInv == TargetInv && Op->SourceIndex == SlotIndex)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SLOT DROP] Same slot -> handled"));
		Op->bDropHandled = true;
		return true;
	}

	if (SourceInv == TargetInv)
	{
		TArray<int32> Changed;
		bOk = SourceInv->MoveOrSwap(Op->SourceIndex, SlotIndex, Changed);
	}
	else
	{
		TArray<int32> ChangedS, ChangedT;
		bOk = SourceInv->TransferTo(
			TargetInv,
			Op->SourceIndex,
			SlotIndex,
			Op->Quantity,
			ChangedS,
			ChangedT);
	}

	UE_LOG(LogTemp, Warning,
	       TEXT("[SLOT DROP] Result=%d"),
	       bOk ? 1 : 0);

	if (bOk)
	{
		Op->bDropHandled = true;
	}

	return bOk;
}

void UGridSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	// UE_LOG(LogTemp, Warning, TEXT("MouseEnter slot=%d empty=%d OwnerMenu=%s vis=%d"),
	//        SlotIndex, CachedView.bEmpty ? 1 : 0, *GetNameSafe(OwnerMenu.Get()), (int32)GetVisibility());

	if (CachedView.bEmpty) return;

	if (OwnerMenu.IsValid())
	{
		OwnerMenu->ShowTooltipForView(CachedView);
	}
}

void UGridSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	// UE_LOG(LogTemp, Warning, TEXT("MouseLeave slot=%d OwnerMenu=%s"), SlotIndex, *GetNameSafe(OwnerMenu.Get()));

	if (OwnerMenu.IsValid())
	{
		OwnerMenu->HideTooltip();
	}
}
