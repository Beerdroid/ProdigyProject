#include "ProdigyCombatHUDWidget.h"

#include "ProdigyAbilityButtonWidget.h"
#include "AbilitySystem/ActionComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"

#include "Player/ProdigyPlayerController.h"
#include "AbilitySystem/CombatSubsystem.h"
#include "Blueprint/WidgetTree.h"

void UProdigyCombatHUDWidget::HandleAbilityClicked(FGameplayTag AbilityTag)
{
	AProdigyPlayerController* PC = GetProdigyPC();
	if (!PC) return;

	PC->TryUseAbilityOnLockedTarget(AbilityTag);
}

void UProdigyCombatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (StartCombatButton)
	{
		StartCombatButton->OnClicked.RemoveAll(this);
		StartCombatButton->OnClicked.AddDynamic(this, &UProdigyCombatHUDWidget::HandleStartCombatClicked);
	}

	if (EndTurnButton)
	{
		EndTurnButton->OnClicked.RemoveAll(this);
		EndTurnButton->OnClicked.AddDynamic(this, &UProdigyCombatHUDWidget::HandleEndTurnClicked);
	}

	if (AProdigyPlayerController* PC = GetProdigyPC())
	{
		PC->OnCombatHUDDirty.RemoveAll(this);
		PC->OnCombatHUDDirty.AddDynamic(this, &UProdigyCombatHUDWidget::Refresh);
	}

	RebuildAbilityButtons();
	Refresh();
}

void UProdigyCombatHUDWidget::NativeDestruct()
{
	if (AProdigyPlayerController* PC = GetProdigyPC())
	{
		PC->OnCombatHUDDirty.RemoveAll(this);
	}
	Super::NativeDestruct();
}

AProdigyPlayerController* UProdigyCombatHUDWidget::GetProdigyPC() const
{
	return Cast<AProdigyPlayerController>(GetOwningPlayer());
}

void UProdigyCombatHUDWidget::RebuildAbilityButtons()
{
	if (!AbilityRow) return;
	if (!AbilityButtonClass) return;

	AbilityRow->ClearChildren();
	AbilityButtonWidgets.Reset();

	AProdigyPlayerController* PC = GetProdigyPC();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	UActionComponent* AC = P ? P->FindComponentByClass<UActionComponent>() : nullptr;

	for (int32 i = 0; i < AbilityTags.Num(); ++i)
	{
		const FGameplayTag Tag = AbilityTags[i];
		if (!Tag.IsValid()) continue;

		FText Title = FText::FromString(Tag.ToString());
		UTexture2D* Icon = nullptr;

		if (AC)
		{
			UActionDefinition* Def = nullptr;
			if (AC->TryGetActionDefinition(Tag, Def) && Def)
			{
				if (!Def->DisplayName.IsEmpty()) Title = Def->DisplayName;
				Icon = Def->Icon;
			}
		}

		UProdigyAbilityButtonWidget* W = CreateWidget<UProdigyAbilityButtonWidget>(GetOwningPlayer(), AbilityButtonClass);
		if (!W) continue;

		W->Setup(Tag, Title, Icon);

		W->OnAbilityClicked.RemoveAll(this);
		W->OnAbilityClicked.AddDynamic(this, &UProdigyCombatHUDWidget::HandleAbilityClicked);

		AbilityRow->AddChildToHorizontalBox(W);

		AbilityButtonWidgets.Add(W);
	}
}

void UProdigyCombatHUDWidget::Refresh()
{
	AProdigyPlayerController* PC = GetProdigyPC();
	if (!PC) return;

	const float HP = PC->UI_GetHP();
	const float MaxHP = PC->UI_GetMaxHP();
	const float AP = PC->UI_GetAP();
	const float MaxAP = PC->UI_GetMaxAP();

	if (HPText) HPText->SetText(FText::FromString(FString::Printf(TEXT("HP : %.0f / %.0f"), HP, MaxHP)));
	if (APText) APText->SetText(FText::FromString(FString::Printf(TEXT("AP : %.0f / %.0f"), AP, MaxAP)));

	UCombatSubsystem* Combat = PC->GetCombatSubsystem();
	const bool bInCombat = Combat ? Combat->IsInCombat() : false;

	AActor* TurnActor = Combat ? Combat->GetCurrentTurnActor() : nullptr;
	const bool bMyTurn = PC->IsMyTurn();

	if (TurnText)
	{
		FString TurnStr = TEXT("Turn: -");

		if (bInCombat)
		{
			APawn* TurnPawn = Cast<APawn>(TurnActor);
			const bool bTurnIsPlayer = TurnPawn && TurnPawn->IsPlayerControlled();
			TurnStr = bTurnIsPlayer ? TEXT("Your turn") : TEXT("Enemy turn");
		}

		TurnText->SetText(FText::FromString(TurnStr));
	}

	const bool bHasTarget = (PC->GetLockedTarget() != nullptr);

	// Start Combat enabled only if: not in combat AND you have a locked target
	if (StartCombatButton) StartCombatButton->SetIsEnabled(!bInCombat && bHasTarget);

	// End Turn enabled only if: in combat AND my turn
	if (EndTurnButton) EndTurnButton->SetIsEnabled(bInCombat && bMyTurn);

	// --- Ability buttons: gate by turn FIRST, then disable on turn-based cooldown ---
	APawn* P = PC->GetPawn();
	UActionComponent* AC = IsValid(P) ? P->FindComponentByClass<UActionComponent>() : nullptr;

	FActionContext Ctx;
	Ctx.Instigator = P;
	Ctx.TargetActor = PC->GetLockedTarget();

	const bool bCanEvaluateAbilities = bInCombat && bMyTurn && IsValid(AC) && IsValid(P);

	// We use AbilityTags as the source of truth for which ability each slot represents.
	// If your widget stores the tag internally, you can swap Tag = AbilityTags[i] -> W->GetAbilityTag().
	const int32 Num = FMath::Min(AbilityButtonWidgets.Num(), AbilityTags.Num());

	for (int32 i = 0; i < Num; ++i)
	{
		UProdigyAbilityButtonWidget* W = AbilityButtonWidgets[i];
		if (!W) continue;

		bool bEnable = bCanEvaluateAbilities;

		if (bEnable)
		{
			const FGameplayTag Tag = AbilityTags[i];

			const FActionQueryResult Q = AC->QueryAction(Tag, Ctx);

			// Disable ONLY if the ability is on turn-based cooldown
			if (Q.FailReason == EActionFailReason::OnCooldown && Q.CooldownTurns > 0)
			{
				bEnable = false;
			}
		}

		W->SetAbilityEnabled(bEnable);
	}

	// Disable any extra widgets that exist without matching tags
	for (int32 i = Num; i < AbilityButtonWidgets.Num(); ++i)
	{
		if (UProdigyAbilityButtonWidget* W = AbilityButtonWidgets[i])
		{
			W->SetAbilityEnabled(false);
		}
	}
}



void UProdigyCombatHUDWidget::HandleStartCombatClicked()
{
	if (AProdigyPlayerController* PC = GetProdigyPC())
	{
		PC->UI_StartFight();
	}
}

void UProdigyCombatHUDWidget::HandleEndTurnClicked()
{
	if (AProdigyPlayerController* PC = GetProdigyPC())
	{
		PC->UI_EndTurn();
	}
}

void UProdigyAbilityClickProxy::HandleClicked()
{
	if (!OwnerWidget.IsValid()) return;

	AProdigyPlayerController* PC = OwnerWidget->GetProdigyPC();
	if (!PC) return;

	PC->TryUseAbilityOnLockedTarget(AbilityTag);
}
