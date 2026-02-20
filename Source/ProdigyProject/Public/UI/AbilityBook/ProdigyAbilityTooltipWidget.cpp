#include "ProdigyAbilityTooltipWidget.h"

#include "AbilitySystem/ActionDefinition.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

static void SetOptionalText(UTextBlock* TB, const FText& T, bool bCollapseWhenEmpty = true)
{
	if (!TB) return;

	TB->SetText(T);

	if (bCollapseWhenEmpty)
	{
		TB->SetVisibility(T.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

static void SetOptionalInt(UTextBlock* TB, int32 V)
{
	if (!TB) return;

	if (V <= 0)
	{
		TB->SetText(FText::GetEmpty());
		TB->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	TB->SetText(FText::AsNumber(V));
	TB->SetVisibility(ESlateVisibility::Visible);
}

static void SetOptionalFloatSeconds(UTextBlock* TB, float Seconds)
{
	if (!TB) return;

	if (Seconds <= 0.f)
	{
		TB->SetText(FText::GetEmpty());
		TB->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Show 1 decimal if < 10s else integer seconds
	if (Seconds < 10.f)
	{
		TB->SetText(FText::AsNumber(FMath::RoundToFloat(Seconds * 10.f) / 10.f));
	}
	else
	{
		TB->SetText(FText::AsNumber(FMath::RoundToInt(Seconds)));
	}
	TB->SetVisibility(ESlateVisibility::Visible);
}

FText UProdigyAbilityTooltipWidget::TargetingModeToText(EActionTargetingMode Mode)
{
	switch (Mode)
	{
	case EActionTargetingMode::None:  return FText::FromString(TEXT("None"));
	case EActionTargetingMode::Self:  return FText::FromString(TEXT("Self"));
	case EActionTargetingMode::Unit:  return FText::FromString(TEXT("Unit"));
	case EActionTargetingMode::Point: return FText::FromString(TEXT("Point"));
	default:                          return FText::FromString(TEXT("Unknown"));
	}
}

void UProdigyAbilityTooltipWidget::SetFromDefinition(const UActionDefinition* Def)
{
	if (!Def)
	{
		Clear();
		return;
	}

	// Icon
	if (IconImage)
	{
		IconImage->SetBrushFromTexture(Def->Icon, /*bMatchSize*/ true);
		IconImage->SetVisibility(Def->Icon ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Title / Desc
	const FText Title = !Def->DisplayName.IsEmpty()
		? Def->DisplayName
		: (Def->ActionTag.IsValid() ? FText::FromString(Def->ActionTag.ToString()) : FText::FromString(TEXT("Ability")));

	SetOptionalText(TitleText, Title, /*collapse*/ false);
	SetOptionalText(DescText, Def->Description, /*collapse*/ true);

	// Targeting
	SetOptionalText(TargetingText, TargetingModeToText(Def->TargetingMode), /*collapse*/ true);

	// Costs / cooldowns (base values from Def)
	SetOptionalInt(APCostText, Def->Combat.APCost);
	SetOptionalInt(CooldownTurnsText, Def->Combat.CooldownTurns);
	SetOptionalFloatSeconds(CooldownSecondsText, Def->Exploration.CooldownSeconds);

	// Usability
	if (UsableText)
	{
		FString S;
		S += Def->bUsableInCombat ? TEXT("Combat") : TEXT("");
		if (Def->bUsableInCombat && Def->bUsableInExploration) S += TEXT(" / ");
		S += Def->bUsableInExploration ? TEXT("Exploration") : TEXT("");

		UsableText->SetText(FText::FromString(S));
		UsableText->SetVisibility(S.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void UProdigyAbilityTooltipWidget::Clear()
{
	if (IconImage)
	{
		IconImage->SetBrushFromTexture(nullptr, true);
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	SetOptionalText(TitleText, FText::GetEmpty(), false);
	SetOptionalText(DescText, FText::GetEmpty(), true);
	SetOptionalText(TargetingText, FText::GetEmpty(), true);

	SetOptionalInt(APCostText, 0);
	SetOptionalInt(CooldownTurnsText, 0);
	SetOptionalFloatSeconds(CooldownSecondsText, 0.f);

	SetOptionalText(UsableText, FText::GetEmpty(), true);
}