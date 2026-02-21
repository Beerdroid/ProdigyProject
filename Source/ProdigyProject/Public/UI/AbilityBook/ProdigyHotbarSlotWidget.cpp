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
#include "ProdigyHotbarDragDropOp.h"
#include "ProdigyHotbarDragVisualWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
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
	UE_LOG(LogTemp, Warning, TEXT("[HotbarClick] Slot=%d Widget=%s PC=%s"),
		SlotIndex,
		*GetNameSafe(this),
		*GetNameSafe(PC));

	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HotbarClick] ABORT: PC null"));
		return;
	}

	if (!OwnerHotbar.IsValid() || SlotIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HotbarClick] ABORT: OwnerHotbarValid=%d SlotIndex=%d"),
			OwnerHotbar.IsValid() ? 1 : 0,
			SlotIndex);
		return;
	}

	const FProdigyHotbarEntry E = OwnerHotbar->GetSlotEntry(SlotIndex);

	UE_LOG(LogTemp, Warning,
		TEXT("[HotbarClick] Entry Slot=%d Type=%d Enabled=%d AbilityTag=%s ItemID=%s"),
		SlotIndex,
		(int32)E.Type,
		E.bEnabled ? 1 : 0,
		E.AbilityTag.IsValid() ? *E.AbilityTag.ToString() : TEXT("<INVALID>"),
		E.ItemID.IsNone() ? TEXT("<NONE>") : *E.ItemID.ToString());

	if (!E.bEnabled)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HotbarClick] ABORT: Entry disabled"));
		ApplyVisuals(false, FText::GetEmpty());
		return;
	}

	if (E.Type == EProdigyHotbarEntryType::Ability)
	{
		if (!E.AbilityTag.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[HotbarClick] ABORT: Ability entry but tag invalid"));
			return;
		}

		AActor* Locked = PC->GetLockedTarget();
		UE_LOG(LogTemp, Warning, TEXT("[HotbarClick] Ability: Tag=%s LockedTarget=%s"),
			*E.AbilityTag.ToString(),
			*GetNameSafe(Locked));

		const bool bOk = PC->TryUseAbilityOnLockedTarget(E.AbilityTag);

		UE_LOG(LogTemp, Warning, TEXT("[HotbarClick] Ability ExecuteResult=%d Tag=%s"),
			bOk ? 1 : 0,
			*E.AbilityTag.ToString());

		return;
	}

	if (E.Type == EProdigyHotbarEntryType::Item)
	{
		if (E.ItemID.IsNone())
		{
			UE_LOG(LogTemp, Warning, TEXT("[HotbarClick] ABORT: Item entry but ItemID None"));
			return;
		}

		PC->EnsurePlayerInventoryResolved();
		UInventoryComponent* Inv = PC->InventoryComponent.Get();

		UE_LOG(LogTemp, Warning, TEXT("[HotbarClick] Item: ItemID=%s Inv=%s"),
			*E.ItemID.ToString(),
			*GetNameSafe(Inv));

		if (!IsValid(Inv))
		{
			UE_LOG(LogTemp, Warning, TEXT("[HotbarClick] ABORT: Inventory invalid"));
			return;
		}

		const int32 FromSlot = FindFirstItemSlotByID(Inv, E.ItemID);

		UE_LOG(LogTemp, Warning, TEXT("[HotbarClick] Item: FirstSlot=%d"), FromSlot);

		if (FromSlot == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HotbarClick] ABORT: Item not found in inventory"));
			return;
		}

		TArray<int32> Changed;
		const bool bConsumed = PC->ConsumeFromSlot(FromSlot, Changed);

		UE_LOG(LogTemp, Warning, TEXT("[HotbarClick] Item ConsumeResult=%d ChangedSlots=%d"),
			bConsumed ? 1 : 0,
			Changed.Num());

		PC->OnCombatHUDDirty.Broadcast();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[HotbarClick] ABORT: Unknown Entry.Type=%d"), (int32)E.Type);
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

bool UProdigyHotbarSlotWidget::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	// IMPORTANT: don't call Super first; it may mark handled / do nothing but you lose clarity.
	UE_LOG(LogTemp, Warning, TEXT("[HotbarSlot Drop] RECEIVED Slot=%d Op=%s"),
	SlotIndex, *GetNameSafe(InOperation));

	UE_LOG(LogTemp, Warning, TEXT("NativeOnDrop OpClass=%s"), *GetNameSafe(InOperation->GetClass()));

	UE_LOG(LogTemp, Warning, TEXT("[HotbarSlot Drop] Slot=%d Op=%s"),
	SlotIndex,
	*GetNameSafe(InOperation));

	UE_LOG(LogTemp, Warning, TEXT("  AbilityOp=%d InvOp=%d"),
		InOperation->IsA(UProdigyAbilityDragDropOp::StaticClass()),
		InOperation->IsA(UInvDragDropOp::StaticClass()));

	if (!OwnerHotbar.IsValid() || SlotIndex == INDEX_NONE || !IsValid(InOperation))
	{
		UE_LOG(LogTemp, Warning, TEXT("!OwnerHotbar.IsValid() || SlotIndex == INDEX_NONE || !IsValid(InOperation)"));
		return false;
	}

	// ===== Hotbar slot drag (swap) =====
	if (UProdigyHotbarDragDropOp* HOp = Cast<UProdigyHotbarDragDropOp>(InOperation))
	{
		UProdigyHotbarWidget* SrcHB = HOp->SourceHotbar.Get();
		if (!IsValid(SrcHB) || !OwnerHotbar.IsValid() || SlotIndex == INDEX_NONE)
		{
			return false;
		}

		// Only allow swapping within the same hotbar instance
		if (SrcHB != OwnerHotbar.Get())
		{
			return false;
		}

		const int32 Src = HOp->SourceIndex;
		const int32 Dst = SlotIndex;

		UE_LOG(LogTemp, Warning, TEXT("[HotbarSlot Drop] HotbarSwap Src=%d Dst=%d"), Src, Dst);

		// Drop on itself => count as handled (prevents drag-cancel clearing)
		if (Src == INDEX_NONE || Src == Dst)
		{
			HOp->bDropHandled = true;
			return true;
		}

		OwnerHotbar->SwapSlots(Src, Dst);

		HOp->bDropHandled = true;
		return true;
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
		if (!IsValid(IOp->SourceInventory) || IOp->SourceIndex == INDEX_NONE)
		{
			return false;
		}

		const FInventorySlot S = IOp->SourceInventory->GetSlot(IOp->SourceIndex);
		if (S.IsEmpty() || S.ItemID.IsNone())
		{
			return false;
		}

		OwnerHotbar->SetSlotItem(SlotIndex, S.ItemID);
		IOp->bDropHandled = true;
		return true;
	}

	return false;
}

FReply UProdigyHotbarSlotWidget::NativeOnPreviewMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	// Let clicks still work via SlotButton, but detect drag on preview.
	// Preview gets the event before children (Button), so this is reliable.

	if (!OwnerHotbar.IsValid() || SlotIndex == INDEX_NONE)
	{
		return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
	}

	const FProdigyHotbarEntry E = OwnerHotbar->GetSlotEntry(SlotIndex);

	// Drag only if this slot actually has something assigned and is enabled
	if (!E.bEnabled || E.Type == EProdigyHotbarEntryType::None)
	{
		return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
	}

	UE_LOG(LogTemp, Warning, TEXT("[HotbarSlot] DetectDragIfPressed Slot=%d Type=%d"),
		SlotIndex, (int32)E.Type);

	return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
}

void UProdigyHotbarSlotWidget::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
OutOperation = nullptr;

	if (!OwnerHotbar.IsValid() || SlotIndex == INDEX_NONE) return;

	const FProdigyHotbarEntry E = OwnerHotbar->GetSlotEntry(SlotIndex);
	if (!E.bEnabled || E.Type == EProdigyHotbarEntryType::None) return;

	UProdigyHotbarDragDropOp* Op = NewObject<UProdigyHotbarDragDropOp>(this);
	if (!Op) return;

	Op->SourceHotbar = OwnerHotbar;
	Op->SourceIndex  = SlotIndex;
	Op->DraggedEntry = E;
	Op->Pivot        = EDragPivot::MouseDown;

	UProdigyHotbarDragVisualWidget* Ghost = nullptr;

	if (HotbarDragVisualClass)
	{
		Ghost = CreateWidget<UProdigyHotbarDragVisualWidget>(GetOwningPlayer(), HotbarDragVisualClass);
		if (Ghost)
		{
			// Fill icon/qty based on entry type
			if (E.Type == EProdigyHotbarEntryType::Ability && E.AbilityTag.IsValid())
			{
				AProdigyPlayerController* PC = GetOwningPlayer<AProdigyPlayerController>();
				APawn* P = PC ? PC->GetPawn() : nullptr;
				UActionComponent* AC = P ? P->FindComponentByClass<UActionComponent>() : nullptr;

				UActionDefinition* Def = nullptr;
				if (AC && AC->TryGetActionDefinition(E.AbilityTag, Def) && Def)
				{
					Ghost->SetIcon(Def->Icon);
				}

				Ghost->SetQuantity(0); // hide qty
			}
			else if (E.Type == EProdigyHotbarEntryType::Item && !E.ItemID.IsNone())
			{
				AProdigyPlayerController* PC = GetOwningPlayer<AProdigyPlayerController>();
				if (PC)
				{
					PC->EnsurePlayerInventoryResolved();
					UInventoryComponent* Inv = PC->InventoryComponent.Get();

					if (Inv)
					{
						FItemRow Row;
						if (Inv->TryGetItemDef(E.ItemID, Row))
						{
							UTexture2D* Icon = Row.Icon.LoadSynchronous();
							Ghost->SetIcon(Icon);
						}

						const int32 TotalQty = Inv->GetTotalQuantityByItemID(E.ItemID);
						Ghost->SetQuantity(TotalQty);
					}
				}
			}

			Op->DefaultDragVisual = Ghost;
		}
	}

	OutOperation = Op;

	UE_LOG(LogTemp, Warning, TEXT("[HotbarSlot DragDetected] Src=%d Type=%d Tag=%s Item=%s"),
		SlotIndex,
		(int32)E.Type,
		E.AbilityTag.IsValid() ? *E.AbilityTag.ToString() : TEXT("<INVALID>"),
		E.ItemID.IsNone() ? TEXT("<NONE>") : *E.ItemID.ToString());
}

void UProdigyHotbarSlotWidget::Refresh()
{
	AProdigyPlayerController* PC = GetOwningPlayer<AProdigyPlayerController>();

	FText CdText = FText::GetEmpty();
	CachedQuantity = 0;

	// ALWAYS reset per-refresh transient visuals first
	if (CooldownText)
	{
		CooldownText->SetText(FText::GetEmpty());
		CooldownText->SetVisibility(ESlateVisibility::Hidden);
	}

	if (QtyText)
	{
		QtyText->SetText(FText::GetEmpty());
		QtyText->SetVisibility(ESlateVisibility::Hidden);
	}

	// Tooltips: ability tooltip is a widget, item tooltip is SetToolTipText
	// We only keep ability tooltip when showing an ability.
	ClearAbilityTooltip();
	SetToolTipText(FText::GetEmpty());

	if (!OwnerHotbar.IsValid() || SlotIndex == INDEX_NONE)
	{
		if (IconImage) IconImage->SetBrushFromTexture(nullptr, true);
		ApplyVisuals(false, CdText);
		return;
	}

	const FProdigyHotbarEntry E = OwnerHotbar->GetSlotEntry(SlotIndex);

	if (!E.bEnabled)
	{
		if (IconImage) IconImage->SetBrushFromTexture(nullptr, true);
		ApplyVisuals(false, FText::GetEmpty());
		return;
	}

	// Empty slot
	if (E.Type == EProdigyHotbarEntryType::None)
	{
		if (IconImage) IconImage->SetBrushFromTexture(nullptr, true);
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
			if (IconImage) IconImage->SetBrushFromTexture(nullptr, true);
			ApplyVisuals(false, CdText);
			return;
		}

		UActionComponent* AC = P->FindComponentByClass<UActionComponent>();
		if (!IsValid(AC))
		{
			if (IconImage) IconImage->SetBrushFromTexture(nullptr, true);
			ApplyVisuals(false, CdText);
			return;
		}

		UActionDefinition* Def = nullptr;
		if (!AC->TryGetActionDefinition(E.AbilityTag, Def) || !Def)
		{
			if (IconImage) IconImage->SetBrushFromTexture(nullptr, true);
			ApplyVisuals(false, CdText);
			return;
		}

		// Icon + ability tooltip
		if (IconImage) IconImage->SetBrushFromTexture(Def->Icon, true);
		UpdateAbilityTooltip(Def);

		// Ensure Qty stays hidden for abilities (critical bug fix)
		if (QtyText)
		{
			QtyText->SetText(FText::GetEmpty());
			QtyText->SetVisibility(ESlateVisibility::Hidden);
		}

		// Avoid invalid-target cue spam
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
			if (IconImage) IconImage->SetBrushFromTexture(nullptr, true);
			ApplyVisuals(false, CdText);
			return;
		}

		PC->EnsurePlayerInventoryResolved();
		UInventoryComponent* Inv = PC->InventoryComponent.Get();
		if (!IsValid(Inv))
		{
			if (IconImage) IconImage->SetBrushFromTexture(nullptr, true);
			ApplyVisuals(false, CdText);
			return;
		}

		// Icon + tooltip from DB
		FItemRow Row;
		if (Inv->TryGetItemDef(E.ItemID, Row))
		{
			UTexture2D* Icon = Row.Icon.LoadSynchronous();
			if (IconImage) IconImage->SetBrushFromTexture(Icon, true);

			SetToolTipText(!Row.DisplayName.IsEmpty()
				? Row.DisplayName
				: FText::FromString(E.ItemID.ToString()));
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

		ApplyVisuals(bEnabled, CdText);
		return;
	}

	if (IconImage) IconImage->SetBrushFromTexture(nullptr, true);
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
