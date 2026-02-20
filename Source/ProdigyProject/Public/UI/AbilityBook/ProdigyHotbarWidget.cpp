#include "ProdigyHotbarWidget.h"

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
	if (!IsValid(SlotsPanel) || !SlotWidgetClass) return;

	if (NumSlots < 1) NumSlots = 1;

	// Ensure entries exist
	Entries.SetNum(NumSlots);

	SlotsPanel->ClearChildren();
	SlotWidgets.Reset();
	SlotWidgets.Reserve(NumSlots);

	for (int32 i = 0; i < NumSlots; ++i)
	{
		UProdigyHotbarSlotWidget* W = CreateWidget<UProdigyHotbarSlotWidget>(GetOwningPlayer(), SlotWidgetClass);
		if (!W) continue;

		W->InitSlot(this, i);

		SlotsPanel->AddChild(W);
		SlotWidgets.Add(W);
	}

	RefreshAllSlots();
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