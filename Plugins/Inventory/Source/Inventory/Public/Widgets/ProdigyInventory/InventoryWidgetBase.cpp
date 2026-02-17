#include "InventoryWidgetBase.h"

#include "EquipSlotWidget.h"
#include "Components/UniformGridPanel.h"
#include "GridSlotWidget.h"
#include "InvContextMenuWidget.h"
#include "InvItemTooltipWidget.h"
#include "InvSplitCursorWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "ProdigyInventory/InvDragDropOp.h"
#include "ProdigyInventory/InventoryComponent.h"
#include "ProdigyInventory/InvPlayerController.h"

bool UInventoryWidgetBase::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
                                        UDragDropOperation* InOperation)
{
	Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

	UInvDragDropOp* Op = Cast<UInvDragDropOp>(InOperation);
	if (!Op)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MENU DROP] No drag op"));
		return false;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[MENU DROP] Background drop SrcIdx=%d HandledBefore=%d"),
		Op->SourceIndex,
		Op->bDropHandled ? 1 : 0);

	// Consume drop so DragCancelled won't fire
	Op->bDropHandled = true;
	return true;
}

void UInventoryWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UInventoryWidgetBase::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UInventoryWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bSplitMode)
	{
		UpdateSplitCursorPosition();
	}
}

void UInventoryWidgetBase::UpdateSplitCursorPosition()
{
	if (!IsValid(SplitCursorWidget)) return;

	const FVector2D Mouse = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetWorld());
	const FVector2D Offset(16.f, 16.f);

	SplitCursorWidget->SetAlignmentInViewport(FVector2D(0.f, 0.f));
	SplitCursorWidget->SetPositionInViewport(Mouse + Offset, /*bRemoveDPIScale*/ false);
}

void UInventoryWidgetBase::SetInventory(UInventoryComponent* InInventory)
{
	if (Inventory.Get() == InInventory) return;

	Unbind();
	Inventory = InInventory;

	if (!Inventory.IsValid() || !IsValid(Grid) || !SlotWidgetClass)
	{
		return;
	}



	// Bind delegates
	Inventory->OnSlotsChanged.AddDynamic(this, &UInventoryWidgetBase::HandleSlotsChanged);
	Inventory->OnSlotChanged.AddDynamic(this, &UInventoryWidgetBase::HandleSlotChanged);

	AInvPlayerController* PC = GetOwningPlayer<AInvPlayerController>();
	CachedPC = PC;

	InitEquipSlot(EquipSlot_Head,  Inventory.Get());
	InitEquipSlot(EquipSlot_Chest, Inventory.Get());
	InitEquipSlot(EquipSlot_Weapon, Inventory.Get());
	InitEquipSlot(EquipSlot_Cloak, Inventory.Get());
	InitEquipSlot(EquipSlot_Hands, Inventory.Get());
	InitEquipSlot(EquipSlot_Pants, Inventory.Get());
	InitEquipSlot(EquipSlot_Boots, Inventory.Get());

	if (IsValid(PC))
	{
		// Avoid duplicate binds if SetInventory() called multiple times
		PC->OnGoldChanged.RemoveDynamic(this, &UInventoryWidgetBase::HandleGoldChanged);
		PC->OnGoldChanged.AddDynamic(this, &UInventoryWidgetBase::HandleGoldChanged);

		// Force initial sync
		HandleGoldChanged(PC->Gold);
	}

	RebuildAll();
	OnInventoryBound();
}

void UInventoryWidgetBase::HandleGoldChanged(int32 NewGold)
{
	if (IsValid(GoldText))
	{
		GoldText->SetText(FText::AsNumber(NewGold));
	}
}

void UInventoryWidgetBase::Unbind()
{
	if (CachedPC.IsValid())
	{
		CachedPC->OnGoldChanged.RemoveDynamic(this, &UInventoryWidgetBase::HandleGoldChanged);
		CachedPC = nullptr;
	}
	
	if (Inventory.IsValid())
	{
		Inventory->OnSlotsChanged.RemoveAll(this);
		Inventory->OnSlotChanged.RemoveAll(this);
	}

	Inventory = nullptr;

	// Optional: clear UI when unbound
	if (IsValid(Grid))
	{
		Grid->ClearChildren();
	}
	SlotWidgets.Reset();
}

void UInventoryWidgetBase::InitEquipSlot(UEquipSlotWidget* Slot, UInventoryComponent* Inv)
{
	if (!IsValid(Slot)) return;
	if (!IsValid(Inv)) return;

	Slot->SetInventory(Inv);
}

void UInventoryWidgetBase::ShowTooltipForView(const FInventorySlotView& View)
{
	if (bSplitMode)
	{
		return;
	}
	
	if (View.bEmpty || !ItemTooltipWidgetClass) return;

	if (!IsValid(ItemTooltipWidget))
	{
		ItemTooltipWidget = CreateWidget<UInvItemTooltipWidget>(GetOwningPlayer(), ItemTooltipWidgetClass);
		if (!IsValid(ItemTooltipWidget)) return;

		ItemTooltipWidget->AddToViewport(9000);
		ItemTooltipWidget->SetIsEnabled(false);
	}

	ItemTooltipWidget->SetView(View);
	ItemTooltipWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Ensure size is computed if you rely on DesiredSize
	ItemTooltipWidget->ForceLayoutPrepass();
	const FVector2D Size = ItemTooltipWidget->GetDesiredSize();

	const FVector2D Mouse = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetWorld());
	const FVector2D Offset(2.f, 2.f);

	// Place tooltip so its BOTTOM-LEFT is near mouse (this makes it feel "exactly near")
	ItemTooltipWidget->SetAlignmentInViewport(FVector2D(0.f, 1.f));

	// Position is the bottom-left point (only Offset away from cursor)
	const FVector2D Pos = Mouse + FVector2D(+Offset.X, -Offset.Y);

	ItemTooltipWidget->SetPositionInViewport(Pos, /*bRemoveDPIScale*/ false);
}

void UInventoryWidgetBase::HideTooltip()
{
	if (IsValid(ItemTooltipWidget))
	{
		ItemTooltipWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UInventoryWidgetBase::OpenContextMenuForSlot(int32 InSlotIndex)
{
	if (!ContextMenuClass) return;

	CloseContextMenu();

	AInvPlayerController* PC = Cast<AInvPlayerController>(GetOwningPlayer());
	UInventoryComponent* Inv = Inventory.Get();
	if (!IsValid(PC) || !IsValid(Inv)) return;

	ContextMenu = CreateWidget<UInvContextMenuWidget>(PC, ContextMenuClass);
	if (!IsValid(ContextMenu)) return;

	ContextMenu->AddToViewport(9500);
	ContextMenu->SetIsEnabled(true);
	ContextMenu->SetVisibility(ESlateVisibility::Visible);

	ContextMenu->Init(PC, Inv, InSlotIndex, /*bExternal*/ false, this);

	// Position near mouse (no clamp, per your preference)
	const FVector2D Mouse = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetWorld());
	ContextMenu->SetAlignmentInViewport(FVector2D(0.f, 0.f));
	ContextMenu->SetPositionInViewport(Mouse + FVector2D(2.f, 2.f), false);
}

void UInventoryWidgetBase::CloseContextMenu()
{
	if (IsValid(ContextMenu))
	{
		ContextMenu->RemoveFromParent();
	}
	ContextMenu = nullptr;
}

void UInventoryWidgetBase::BeginSplitFrom(int32 FromIndex, int32 Amount)
{
	if (!Inventory.IsValid()) return;

	const FInventorySlot S = Inventory->GetSlot(FromIndex);
	if (S.IsEmpty() || S.Quantity <= 1) return;

	HideTooltip();

	bSplitMode = true;
	SplitFromIndex = FromIndex;
	SplitAmountPending = FMath::Clamp(Amount, 1, S.Quantity - 1);

	// Create cursor widget once
	if (!IsValid(SplitCursorWidget) && SplitCursorWidgetClass)
	{
		SplitCursorWidget = CreateWidget<UInvSplitCursorWidget>(GetOwningPlayer(), SplitCursorWidgetClass);
		if (IsValid(SplitCursorWidget))
		{
			UE_LOG(LogTemp, Warning, TEXT("[INV_MENU] Cursor"));
			SplitCursorWidget->AddToViewport(10000);
			SplitCursorWidget->SetIsEnabled(false);
			SplitCursorWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			SplitCursorWidget->SetAlignmentInViewport(FVector2D(0.f, 0.f));
		}
	}

	// Resolve icon (sync is fine for now; you can async later)
	UTexture2D* IconTex = nullptr;
	if (IsValid(SplitCursorWidget))
	{
		UE_LOG(LogTemp, Warning, TEXT("[INV_MENU] IconTex"));
		FItemRow Row;
		if (Inventory->TryGetItemDef(S.ItemID, Row))
		{
			IconTex = Row.Icon.LoadSynchronous();
		}
		SplitCursorWidget->SetPreview(IconTex, SplitAmountPending);
		SplitCursorWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	UE_LOG(LogTemp, Warning, TEXT("[INV_MENU] SplitMode ON From=%d Amount=%d"), SplitFromIndex, SplitAmountPending);
}

void UInventoryWidgetBase::CancelSplitMode()
{
	if (!bSplitMode) return;

	bSplitMode = false;
	SplitFromIndex = INDEX_NONE;
	SplitAmountPending = 0;

	if (IsValid(SplitCursorWidget))
	{
		SplitCursorWidget->SetVisibility(ESlateVisibility::Hidden);
	}

	UE_LOG(LogTemp, Warning, TEXT("[INV_MENU] SplitMode OFF"));
}

bool UInventoryWidgetBase::HandleClickSlotForSplit(int32 TargetIndex)
{
	if (!bSplitMode) return false;
	if (!Inventory.IsValid()) { CancelSplitMode(); return true; }

	// Only allow drop into EMPTY slot (simple UX)
	const FInventorySlot T = Inventory->GetSlot(TargetIndex);
	if (!T.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[INV_MENU] Split target not empty: %d"), TargetIndex);
		return true; // consume click, keep split mode
	}

	TArray<int32> Changed;
	const bool bOK = Inventory->SplitStack(SplitFromIndex, TargetIndex, SplitAmountPending, Changed);

	UE_LOG(LogTemp, Warning, TEXT("[INV_MENU] SplitStack From=%d To=%d Amount=%d OK=%d"),
		SplitFromIndex, TargetIndex, SplitAmountPending, bOK ? 1 : 0);

	CancelSplitMode();
	return true;
}

bool UInventoryWidgetBase::TryCompleteSplitTo(int32 ToIndex)
{
	if (!bSplitMode) return false;
	bSplitMode = false;

	AInvPlayerController* PC = Cast<AInvPlayerController>(GetOwningPlayer());
	if (!IsValid(PC)) return false;

	// Minimal policy: split half into empty slot
	UInventoryComponent* Inv = Inventory.Get();
	if (!IsValid(Inv)) return false;

	const FInventorySlot S = Inv->GetSlot(SplitFromIndex);
	if (S.IsEmpty() || S.Quantity < 2) return false;

	const int32 SplitAmount = S.Quantity / 2;

	TArray<int32> Changed;
	const bool bOK = PC->Player_SplitStack(SplitFromIndex, ToIndex, SplitAmount, Changed);
	SplitFromIndex = INDEX_NONE;
	return bOK;
}

void UInventoryWidgetBase::RebuildAll()
{
	if (!Inventory.IsValid() || !IsValid(Grid) || !SlotWidgetClass) return;

	Grid->ClearChildren();
	SlotWidgets.Reset();

	const int32 Cap = Inventory->GetCapacity();
	SlotWidgets.SetNum(Cap);

	for (int32 i = 0; i < Cap; ++i)
	{
		UGridSlotWidget* W = CreateWidget<UGridSlotWidget>(GetOwningPlayer(), SlotWidgetClass);
		SlotWidgets[i] = W;

		const int32 Row = Columns > 0 ? (i / Columns) : 0;
		const int32 Col = Columns > 0 ? (i % Columns) : i;

		UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(W, Row, Col);
		(void)GridSlot;

		// Tell slot its index + inventory ref (slot needs it for clicks/drag later)
		W->InitSlot(Inventory.Get(), i, this);

		// Paint initial state
		UpdateSlot(i);
	}
}

void UInventoryWidgetBase::UpdateSlot(int32 SlotIndex)
{
	if (!Inventory.IsValid()) return;
	if (!SlotWidgets.IsValidIndex(SlotIndex)) return;

	UGridSlotWidget* W = SlotWidgets[SlotIndex];
	if (!IsValid(W)) return;

	FInventorySlotView View;
	Inventory->GetSlotView(SlotIndex, View);

	W->SetSlotView(View);
}

void UInventoryWidgetBase::HandleSlotsChanged(const TArray<int32>& ChangedSlots)
{
	for (const int32 Idx : ChangedSlots)
	{
		UpdateSlot(Idx);
	}
}

void UInventoryWidgetBase::HandleSlotChanged(int32 SlotIndex, const FInventorySlot& /*NewValue*/)
{
	UpdateSlot(SlotIndex);
}
