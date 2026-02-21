#include "ProdigySpellBookSlotWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

#include "Player/ProdigyPlayerController.h"
#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/ActionDefinition.h"

#include "ProdigyAbilityDragDropOp.h"
#include "ProdigyAbilityDragVisualWidget.h"
#include "ProdigyAbilityTooltipWidget.h"

void UProdigySpellBookSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Normalize once if designer set SlotTag but no one called Setup()
	if (!AbilityTag.IsValid() && SlotTag.IsValid())
	{
		AbilityTag = SlotTag;
	}

	RefreshFromOwner();
}

void UProdigySpellBookSlotWidget::Setup(FGameplayTag InAbilityTag)
{
	AbilityTag = InAbilityTag;

	// Keep SlotTag consistent with policy SlotTag == AbilityTag
	SlotTag = InAbilityTag;

	RefreshFromOwner();
}

void UProdigySpellBookSlotWidget::RefreshFromOwner()
{
	bKnown = false;

	AProdigyPlayerController* PC = GetOwningPlayer<AProdigyPlayerController>();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	UActionComponent* AC = P ? P->FindComponentByClass<UActionComponent>() : nullptr;

	FText Title = FText::FromString(AbilityTag.IsValid() ? AbilityTag.ToString() : FString(TEXT("None")));
	UTexture2D* Icon = nullptr;
	const UActionDefinition* ResolvedDef = nullptr;

	if (IsValid(AC) && AbilityTag.IsValid())
	{
		UActionDefinition* Def = nullptr;
		if (AC->TryGetActionDefinition(AbilityTag, Def) && Def)
		{
			bKnown = true;
			ResolvedDef = Def;

			if (!Def->DisplayName.IsEmpty()) Title = Def->DisplayName;
			Icon = Def->Icon;
		}
	}

	// Visuals
	if (TitleText) TitleText->SetText(Title);

	if (IconImage)
	{
		IconImage->SetBrushFromTexture(Icon, /*bMatchSize*/ true);
		IconImage->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	else
	{
		// If your icon is "missing", this is usually because the WBP variable isn't named IconImage or isn't flagged as variable.
		UE_LOG(LogTemp, Warning, TEXT("[SpellBookSlot:%s] IconImage is null (check WBP binding name)"), *GetNameSafe(this));
	}

	// Tooltip widget (full info)
	if (bKnown && AbilityTag.IsValid() && ResolvedDef)
	{
		UpdateAbilityTooltip(ResolvedDef);
	}
	else
	{
		ClearAbilityTooltip();
	}

	ApplyKnownState(bKnown);
}

void UProdigySpellBookSlotWidget::SetSlotEnabled(bool bEnabled)
{
	bSlotEnabled = bEnabled;
	ApplyKnownState(bKnown);
}

void UProdigySpellBookSlotWidget::ApplyKnownState(bool bIsKnown)
{
	const bool bCanInteract = bSlotEnabled && bIsKnown && AbilityTag.IsValid();

}


void UProdigySpellBookSlotWidget::HandleClicked()
{
	// SpellBook click can be used for details panel later.
}

FReply UProdigySpellBookSlotWidget::NativeOnPreviewMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bSlotEnabled)
	{
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (bKnown && AbilityTag.IsValid())
		{
			return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
		}
		return FReply::Handled();
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void UProdigySpellBookSlotWidget::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	OutOperation = nullptr;

	if (!bSlotEnabled || !bKnown || !AbilityTag.IsValid())
	{
		return;
	}

	UProdigyAbilityDragDropOp* Op = NewObject<UProdigyAbilityDragDropOp>(this);
	if (!Op)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[SpellBook DragDetected] Widget=%s Tag=%s"),
	*GetNameSafe(this),
	*AbilityTag.ToString());

	Op->AbilityTag = AbilityTag;
	Op->Pivot = EDragPivot::MouseDown;

	UE_LOG(LogTemp, Warning, TEXT("OpClass=%s"), *GetNameSafe(Op->GetClass()));

	// Typed drag visual
	if (DragVisualClass)
	{
		UUserWidget* RawWidget = CreateWidget(GetOwningPlayer(), DragVisualClass);
		UProdigyAbilityDragVisualWidget* Ghost = Cast<UProdigyAbilityDragVisualWidget>(RawWidget);

		if (IsValid(Ghost))
		{
			const UActionDefinition* Def = ResolveDefinitionFromOwner();
			Ghost->SetFromDefinition(Def);

			// IMPORTANT: design the drag visual BP root as HitTestInvisible
			Op->DefaultDragVisual = Ghost;
		}
	}

	OutOperation = Op;
}

void UProdigySpellBookSlotWidget::BindOwnerSpellBook(UProdigySpellBookWidget* InOwner)
{
	OwnerSpellBook = InOwner;
}

FGameplayTag UProdigySpellBookSlotWidget::GetSlotTag() const
{
	// Policy: SlotTag == AbilityTag.
	// Return effective tag even if one of them is left unset in designer.
	return AbilityTag.IsValid() ? AbilityTag : SlotTag;
}

void UProdigySpellBookSlotWidget::SetAbilityTag(FGameplayTag InAbilityTag)
{
	AbilityTag = InAbilityTag;
	SlotTag = InAbilityTag;
}

const UActionDefinition* UProdigySpellBookSlotWidget::ResolveDefinitionFromOwner() const
{
	AProdigyPlayerController* PC = GetOwningPlayer<AProdigyPlayerController>();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	UActionComponent* AC = P ? P->FindComponentByClass<UActionComponent>() : nullptr;

	if (!IsValid(AC) || !AbilityTag.IsValid())
	{
		return nullptr;
	}

	UActionDefinition* Def = nullptr;
	if (AC->TryGetActionDefinition(AbilityTag, Def) && Def)
	{
		return Def;
	}

	return nullptr;
}

void UProdigySpellBookSlotWidget::EnsureAbilityTooltip()
{
	if (IsValid(AbilityTooltipWidget)) return;
	if (!AbilityTooltipClass) return;

	AbilityTooltipWidget = CreateWidget<UProdigyAbilityTooltipWidget>(GetOwningPlayer(), AbilityTooltipClass);
	if (!IsValid(AbilityTooltipWidget)) return;

	SetToolTip(AbilityTooltipWidget);
}

void UProdigySpellBookSlotWidget::ClearAbilityTooltip()
{
	SetToolTip(nullptr);
	AbilityTooltipWidget = nullptr;
}

void UProdigySpellBookSlotWidget::UpdateAbilityTooltip(const UActionDefinition* Def)
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

