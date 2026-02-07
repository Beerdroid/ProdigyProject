#include "InventoryWidgetBase.h"

#include "Components/UniformGridPanel.h"
#include "GridSlotWidget.h"
#include "InvItemTooltipWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "ProdigyInventory/InvDragDropOp.h"
#include "ProdigyInventory/InventoryComponent.h"

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

	RebuildAll();
	OnInventoryBound();
}

void UInventoryWidgetBase::Unbind()
{
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

void UInventoryWidgetBase::ShowTooltipForView(const FInventorySlotView& View)
{
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
