#include "ProdigyHotbarSlotWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

#include "Player/ProdigyPlayerController.h"

#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/ActionDefinition.h"
#include "AbilitySystem/ActionTypes.h"
#include "AbilitySystem/ProdigyAbilityUtils.h"

#include "ProdigyAbilityDragDropOp.h"
#include "ProdigyAbilityTooltipWidget.h"
#include "ProdigyInventory/InvDragDropOp.h"

void UProdigyHotbarSlotWidget::InitSlot(UProdigyHotbarWidget* InOwner, int32 InSlotIndex)
{
	OwnerHotbar = InOwner;
	SlotIndex = InSlotIndex;
}

void UProdigyHotbarSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SlotButton)
	{
		SlotButton->OnClicked.RemoveAll(this);
		SlotButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
	}

	Refresh();
}


void UProdigyHotbarSlotWidget::HandleClicked()
{
	AProdigyPlayerController* PC = GetOwningPlayer<AProdigyPlayerController>();
	if (!PC) return;

	if (!OwnerHotbar.IsValid() || SlotIndex == INDEX_NONE) return;

	const FProdigyHotbarEntry E = OwnerHotbar->GetSlotEntry(SlotIndex);
	if (!E.bEnabled)
	{
		// Show visuals if you want (icon/qty/cd), but disable interaction
		ApplyVisuals(false, FText::GetEmpty());
		return;
	}

	if (E.Type == EProdigyHotbarEntryType::Ability)
	{
		if (E.AbilityTag.IsValid())
		{
			PC->TryUseAbilityOnLockedTarget(E.AbilityTag);
		}
		return;
	}

	if (E.Type == EProdigyHotbarEntryType::Item)
	{
		if (E.ItemID.IsNone())
		{
			return;
		}

		PC->EnsurePlayerInventoryResolved();
		UInventoryComponent* Inv = PC->InventoryComponent.Get();
		if (!IsValid(Inv))
		{
			return;
		}

		const int32 FromSlot = FindFirstItemSlotByID(Inv, E.ItemID);
		if (FromSlot == INDEX_NONE)
		{
			return;
		}

		TArray<int32> Changed;
		PC->ConsumeFromSlot(FromSlot, Changed);

		// You already broadcast HUD dirty in Attr change / etc, but inventory may change too:
		PC->OnCombatHUDDirty.Broadcast();
		return;
	}
}

int32 UProdigyHotbarSlotWidget::FindFirstItemSlotByID(UInventoryComponent* Inv, FName ItemID) const
{
	if (!IsValid(Inv) || ItemID.IsNone()) return INDEX_NONE;

	const int32 Cap = Inv->GetCapacity();
	for (int32 i = 0; i < Cap; ++i)
	{
		const FInventorySlot S = Inv->GetSlot(i);
		if (!S.IsEmpty() && S.ItemID == ItemID && S.Quantity > 0)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void UProdigyHotbarSlotWidget::ApplyVisuals(bool bEnabled, const FText& InCooldownText)
{
	if (SlotButton)
	{
		SlotButton->SetIsEnabled(bEnabled);
	}

	if (CooldownText)
	{
		if (InCooldownText.IsEmpty())
		{
			CooldownText->SetText(FText::GetEmpty());
			CooldownText->SetVisibility(ESlateVisibility::Hidden);
		}
		else
		{
			CooldownText->SetText(InCooldownText);
			CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

bool UProdigyHotbarSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

	if (!OwnerHotbar.IsValid() || SlotIndex == INDEX_NONE)
	{
		return false;
	}

	// ===== Ability drag =====
	if (UProdigyAbilityDragDropOp* AOp = Cast<UProdigyAbilityDragDropOp>(InOperation))
	{
		if (!AOp->AbilityTag.IsValid())
		{
			return false;
		}

		OwnerHotbar->SetSlotAbility(SlotIndex, AOp->AbilityTag);
		AOp->bDropHandled = true;
		return true;
	}

	// ===== Inventory drag =====
	if (UInvDragDropOp* IOp = Cast<UInvDragDropOp>(InOperation))
	{
		UInventoryComponent* SourceInv = IOp->SourceInventory;
		if (!IsValid(SourceInv) || !SourceInv->IsValidIndex(IOp->SourceIndex))
		{
			return false;
		}

		const FInventorySlot S = SourceInv->GetSlot(IOp->SourceIndex);
		if (S.IsEmpty() || S.ItemID.IsNone())
		{
			return false;
		}

		// Optional: if you want to allow ONLY consumables on hotbar, enable this block:
		/*
		FItemRow Row;
		if (SourceInv->TryGetItemDef(S.ItemID, Row))
		{
			if (Row.Category != EItemCategory::Consumable)
			{
				return false;
			}
		}
		*/

		OwnerHotbar->SetSlotItem(SlotIndex, S.ItemID);
		IOp->bDropHandled = true;
		return true;
	}

	return false;
}

void UProdigyHotbarSlotWidget::Refresh()
{
	AProdigyPlayerController* PC = GetOwningPlayer<AProdigyPlayerController>();

	FText CdText = FText::GetEmpty();
	CachedQuantity = 0;

	if (!OwnerHotbar.IsValid() || SlotIndex == INDEX_NONE)
	{
		ApplyVisuals(false, CdText);
		return;
	}

	const FProdigyHotbarEntry E = OwnerHotbar->GetSlotEntry(SlotIndex);
	if (!E.bEnabled) return;

	// Empty slot: disable
	if (E.Type == EProdigyHotbarEntryType::None)
	{
		if (IconImage) IconImage->SetBrushFromTexture(nullptr, true);
		if (QtyText)
		{
			QtyText->SetText(FText::GetEmpty());
			QtyText->SetVisibility(ESlateVisibility::Hidden);
		}
		ApplyVisuals(false, CdText);
		return;
	}

	// Common turn gate
	const bool bInCombat = PC ? PC->UI_IsInCombat() : false;
	const bool bMyTurn   = PC ? PC->IsMyTurn() : false;

	if (bInCombat && !bMyTurn)
	{
		ApplyVisuals(false, CdText);
		return;
	}

	APawn* P = PC ? PC->GetPawn() : nullptr;

	// ===== Ability =====
	if (E.Type == EProdigyHotbarEntryType::Ability)
	{
		if (!PC || !P || !E.AbilityTag.IsValid())
		{
			ApplyVisuals(false, CdText);
			return;
		}

		UActionComponent* AC = P->FindComponentByClass<UActionComponent>();
		if (!IsValid(AC))
		{
			ApplyVisuals(false, CdText);
			return;
		}

		UActionDefinition* Def = nullptr;
		if (!AC->TryGetActionDefinition(E.AbilityTag, Def) || !Def)
		{
			ClearAbilityTooltip();
			ApplyVisuals(false, CdText);
			return;
		}

		// Icon + tooltip widget
		if (IconImage) IconImage->SetBrushFromTexture(Def->Icon, true);
		UpdateAbilityTooltip(Def);

		// Avoid invalid-target cue spam: pre-check target validity for Unit/Self
		FActionContext Ctx;
		Ctx.Instigator  = P;
		Ctx.TargetActor = PC->GetLockedTarget();

		switch (Def->TargetingMode)
		{
		case EActionTargetingMode::Unit:
		{
			AActor* T = Ctx.TargetActor;
			if (!IsValid(T) || ProdigyAbilityUtils::IsDeadByAttributes(T))
			{
				ApplyVisuals(false, CdText);
				return;
			}
			break;
		}
		case EActionTargetingMode::Self:
			if (!IsValid(Ctx.Instigator))
			{
				ApplyVisuals(false, CdText);
				return;
			}
			break;
		case EActionTargetingMode::None:
			break;
		case EActionTargetingMode::Point:
			ApplyVisuals(false, CdText);
			return;
		default:
			ApplyVisuals(false, CdText);
			return;
		}

		const FActionQueryResult Q = AC->QueryAction(E.AbilityTag, Ctx);

		bool bEnabled = Q.bCanExecute;

		if (!bEnabled && Q.FailReason == EActionFailReason::OnCooldown)
		{
			if (bInCombat && Q.CooldownTurns > 0)
			{
				CdText = FText::AsNumber(Q.CooldownTurns);
			}
			else if (!bInCombat && Q.CooldownSeconds > 0.f)
			{
				const int32 Sec = FMath::CeilToInt(Q.CooldownSeconds);
				CdText = FText::AsNumber(Sec);
			}
		}

		ApplyVisuals(bEnabled, CdText);
		return;
	}

	// ===== Item =====
	if (E.Type == EProdigyHotbarEntryType::Item)
	{
		if (!PC || !P || E.ItemID.IsNone())
		{
			ApplyVisuals(false, CdText);
			return;
		}

		PC->EnsurePlayerInventoryResolved();
		UInventoryComponent* Inv = PC->InventoryComponent.Get();
		if (!IsValid(Inv))
		{
			ApplyVisuals(false, CdText);
			return;
		}

		// Icon + tooltip from DB
		FItemRow Row;
		if (Inv->TryGetItemDef(E.ItemID, Row))
		{
			UTexture2D* Icon = Row.Icon.LoadSynchronous();
			if (IconImage) IconImage->SetBrushFromTexture(Icon, true);

			if (!Row.DisplayName.IsEmpty())
			{
				SetToolTipText(Row.DisplayName);
			}
			else
			{
				SetToolTipText(FText::FromString(E.ItemID.ToString()));
			}
		}
		else
		{
			if (IconImage) IconImage->SetBrushFromTexture(nullptr, true);
			SetToolTipText(FText::FromString(E.ItemID.ToString()));
		}

		CachedQuantity = Inv->GetTotalQuantityByItemID(E.ItemID);
		const bool bEnabled = (CachedQuantity > 0);

		if (QtyText)
		{
			if (CachedQuantity > 1)
			{
				QtyText->SetText(FText::AsNumber(CachedQuantity));
				QtyText->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				QtyText->SetText(FText::GetEmpty());
				QtyText->SetVisibility(ESlateVisibility::Hidden);
			}
		}

		// Item cooldown not implemented yet (you said not critical)
		ApplyVisuals(bEnabled, CdText);
		return;
	}

	ApplyVisuals(false, CdText);
}

void UProdigyHotbarSlotWidget::EnsureAbilityTooltip()
{
	if (IsValid(AbilityTooltipWidget)) return;
	if (!AbilityTooltipClass) return;

	AbilityTooltipWidget = CreateWidget<UProdigyAbilityTooltipWidget>(GetOwningPlayer(), AbilityTooltipClass);
	if (!IsValid(AbilityTooltipWidget)) return;

	SetToolTip(AbilityTooltipWidget);
}

void UProdigyHotbarSlotWidget::ClearAbilityTooltip()
{
	SetToolTip(nullptr);
	AbilityTooltipWidget = nullptr;
}

void UProdigyHotbarSlotWidget::UpdateAbilityTooltip(const UActionDefinition* Def)
{
	if (!Def)
	{
		ClearAbilityTooltip();
		return;
	}

	EnsureAbilityTooltip();
	if (!IsValid(AbilityTooltipWidget)) return;

	AbilityTooltipWidget->SetFromDefinition(Def);
}
