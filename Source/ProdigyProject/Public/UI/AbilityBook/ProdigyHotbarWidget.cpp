#include "ProdigyHotbarWidget.h"

#include "ProdigyHotbarDragDropOp.h"
#include "Components/PanelWidget.h"

#include "ProdigyHotbarSlotWidget.h"
#include "Components/HorizontalBox.h"
#include "Player/ProdigyPlayerController.h"


void UProdigyHotbarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (AProdigyPlayerController* PC = GetOwningPlayer<AProdigyPlayerController>())
	{
		PC->OnCombatHUDDirty.RemoveAll(this);
		PC->OnCombatHUDDirty.AddDynamic(this, &ThisClass::HandleHUDDirty);
	}

	Rebuild();
}

void UProdigyHotbarWidget::NativeDestruct()
{
	if (AProdigyPlayerController* PC = GetOwningPlayer<AProdigyPlayerController>())
	{
		PC->OnCombatHUDDirty.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UProdigyHotbarWidget::HandleHUDDirty()
{
	RefreshAllSlots();
}

bool UProdigyHotbarWidget::IsValidSlotIndex(int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < Entries.Num();
}

void UProdigyHotbarWidget::Rebuild()
{
	if (NumSlots < 1) NumSlots = 1;
	if (!SlotWidgetClass) return;

	// Ensure entries exist
	Entries.SetNum(NumSlots);

	// Prefer SlotRow (your hotbar is a horizontal bar)
	if (IsValid(SlotRow))
	{
		SlotRow->ClearChildren();
		SlotWidgets.Reset();
		SlotWidgets.Reserve(NumSlots);

		for (int32 i = 0; i < NumSlots; ++i)
		{
			UProdigyHotbarSlotWidget* W = CreateWidget<UProdigyHotbarSlotWidget>(GetOwningPlayer(), SlotWidgetClass);
			if (!IsValid(W)) continue;

			W->InitSlot(this, i);
			SlotRow->AddChildToHorizontalBox(W);
			SlotWidgets.Add(W);
		}

		RefreshAllSlots();
		return;
	}

	// Fallback to SlotsPanel if you ever use UniformGrid / WrapBox in another BP
	if (IsValid(SlotsPanel))
	{
		SlotsPanel->ClearChildren();
		SlotWidgets.Reset();
		SlotWidgets.Reserve(NumSlots);

		for (int32 i = 0; i < NumSlots; ++i)
		{
			UProdigyHotbarSlotWidget* W = CreateWidget<UProdigyHotbarSlotWidget>(GetOwningPlayer(), SlotWidgetClass);
			if (!IsValid(W)) continue;

			W->InitSlot(this, i);
			SlotsPanel->AddChild(W);
			SlotWidgets.Add(W);
		}

		RefreshAllSlots();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("[Hotbar] Rebuild: SlotRow and SlotsPanel are both null. Check WBP bindings."));
}

void UProdigyHotbarWidget::SetSlotAbility(int32 SlotIndex, FGameplayTag AbilityTag)
{
	if (!IsValidSlotIndex(SlotIndex)) return;

	FProdigyHotbarEntry& E = Entries[SlotIndex];
	E.Type = EProdigyHotbarEntryType::Ability;
	E.AbilityTag = AbilityTag;
	E.ItemID = NAME_None;

	if (SlotWidgets.IsValidIndex(SlotIndex) && IsValid(SlotWidgets[SlotIndex]))
	{
		SlotWidgets[SlotIndex]->Refresh();
	}
}

void UProdigyHotbarWidget::SetSlotItem(int32 SlotIndex, FName ItemID)
{
	if (!IsValidSlotIndex(SlotIndex)) return;

	FProdigyHotbarEntry& E = Entries[SlotIndex];
	E.Type = EProdigyHotbarEntryType::Item;
	E.ItemID = ItemID;
	E.AbilityTag = FGameplayTag();

	if (SlotWidgets.IsValidIndex(SlotIndex) && IsValid(SlotWidgets[SlotIndex]))
	{
		SlotWidgets[SlotIndex]->Refresh();
	}
}

void UProdigyHotbarWidget::ClearSlot(int32 SlotIndex)
{
	if (!IsValidSlotIndex(SlotIndex)) return;

	Entries[SlotIndex] = FProdigyHotbarEntry();

	if (SlotWidgets.IsValidIndex(SlotIndex) && IsValid(SlotWidgets[SlotIndex]))
	{
		SlotWidgets[SlotIndex]->Refresh();
	}
}

FProdigyHotbarEntry UProdigyHotbarWidget::GetSlotEntry(int32 SlotIndex) const
{
	if (!IsValidSlotIndex(SlotIndex)) return FProdigyHotbarEntry();
	return Entries[SlotIndex];
}

void UProdigyHotbarWidget::RefreshAllSlots()
{
	for (UProdigyHotbarSlotWidget* W : SlotWidgets)
	{
		if (IsValid(W))
		{
			W->Refresh();
		}
	}
}

void UProdigyHotbarWidget::SetEntryEnabled(int32 SlotIndex, bool bEnabled)
{
	if (!IsValidSlotIndex(SlotIndex)) return;

	Entries[SlotIndex].bEnabled = bEnabled;

	if (SlotWidgets.IsValidIndex(SlotIndex) && IsValid(SlotWidgets[SlotIndex]))
	{
		SlotWidgets[SlotIndex]->Refresh();
	}
}

bool UProdigyHotbarWidget::IsEntryEnabled(int32 SlotIndex) const
{
	return IsValidSlotIndex(SlotIndex) ? Entries[SlotIndex].bEnabled : true;
}

void UProdigyHotbarWidget::RebuildSlots()
{
	if (!IsValid(SlotRow) || !SlotWidgetClass) return;

	SlotRow->ClearChildren();
	SlotWidgets.Reset();

	// Ensure Entries matches slot count
	if (Entries.Num() != NumSlots)
	{
		Entries.SetNum(NumSlots);
	}

	for (int32 i = 0; i < NumSlots; ++i)
	{
		UProdigyHotbarSlotWidget* W =
			CreateWidget<UProdigyHotbarSlotWidget>(GetOwningPlayer(), SlotWidgetClass);

		if (!W) continue;

		W->InitSlot(this, i);
		W->Refresh();

		SlotRow->AddChildToHorizontalBox(W);
		SlotWidgets.Add(W);
	}
}

void UProdigyHotbarWidget::SwapSlots(int32 A, int32 B)
{
	if (!IsValidSlotIndex(A) || !IsValidSlotIndex(B)) return;
	if (A == B) return;

	UE_LOG(LogTemp, Warning, TEXT("[Hotbar] SwapSlots A=%d B=%d"), A, B);

	Entries.Swap(A, B);

	// Refresh visuals immediately
	RefreshAllSlots();
}

bool UProdigyHotbarWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	// If you drop inside the hotbar bounds (but not on a slot),
	// treat it as handled so DragCancelled will NOT clear.
	if (UProdigyHotbarDragDropOp* HOp = Cast<UProdigyHotbarDragDropOp>(InOperation))
	{
		HOp->bDropHandled = true;
		UE_LOG(LogTemp, Warning, TEXT("[Hotbar Root Drop] handled inside hotbar (no slot) Src=%d"), HOp->SourceIndex);
		return true;
	}

	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}
