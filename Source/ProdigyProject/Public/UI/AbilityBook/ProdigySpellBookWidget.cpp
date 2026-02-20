#include "ProdigySpellBookWidget.h"

#include "Components/PanelWidget.h"
#include "Player/ProdigyPlayerController.h"
#include "ProdigySpellBookSlotWidget.h"

void UProdigySpellBookWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (AProdigyPlayerController* PC = GetOwningPlayer<AProdigyPlayerController>())
	{
		PC->OnCombatHUDDirty.RemoveAll(this);
		PC->OnCombatHUDDirty.AddDynamic(this, &ThisClass::HandleHUDDirty);
	}

	RebuildSlotList();
	RefreshAllSlots();
}

void UProdigySpellBookWidget::NativeDestruct()
{
	if (AProdigyPlayerController* PC = GetOwningPlayer<AProdigyPlayerController>())
	{
		PC->OnCombatHUDDirty.RemoveAll(this);
	}

	SlotWidgets.Reset();

	Super::NativeDestruct();
}

void UProdigySpellBookWidget::HandleHUDDirty()
{
	RefreshAllSlots();
}

void UProdigySpellBookWidget::RebuildSlotList()
{
	SlotWidgets.Reset();

	if (!IsValid(SlotsPanel))
	{
		return;
	}

	const int32 NumChildren = SlotsPanel->GetChildrenCount();
	SlotWidgets.Reserve(NumChildren);

	for (int32 i = 0; i < NumChildren; ++i)
	{
		if (UWidget* Child = SlotsPanel->GetChildAt(i))
		{
			if (UProdigySpellBookSlotWidget* SlotWidget = Cast<UProdigySpellBookSlotWidget>(Child))
			{
				SlotWidget->BindOwnerSpellBook(this);

				// If designer uses SlotTag as “ability tag”, normalize once.
				// This preserves your policy SlotTag == AbilityTag and “no remap”.
				if (!SlotWidget->GetAbilityTag().IsValid() && SlotWidget->GetSlotTag().IsValid())
				{
					SlotWidget->Setup(SlotWidget->GetSlotTag());
				}
				else
				{
					// Ensure visuals are correct even if Setup() is never called from BP.
					SlotWidget->RefreshFromOwner();
				}

				SlotWidgets.Add(SlotWidget);
			}
		}
	}
}

void UProdigySpellBookWidget::RefreshAllSlots()
{
	// If slots can be added/removed at runtime, keep this:
	// (If you guarantee fixed designer slots, you can remove this call.)
	RebuildSlotList();

	for (UProdigySpellBookSlotWidget* W : SlotWidgets)
	{
		if (IsValid(W))
		{
			W->RefreshFromOwner();
		}
	}
}