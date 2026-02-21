#include "ProdigyAbilityDragVisualWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "AbilitySystem/ActionDefinition.h"

void UProdigyAbilityDragVisualWidget::SetFromDefinition(const UActionDefinition* Def)
{
	if (!Def)
	{
		if (IconImage)
		{
			IconImage->SetBrushFromTexture(nullptr, true);
			IconImage->SetVisibility(ESlateVisibility::Hidden);
		}
		if (TitleText)
		{
			TitleText->SetText(FText::GetEmpty());
			TitleText->SetVisibility(ESlateVisibility::Hidden);
		}
		return;
	}

	if (IconImage)
	{
		IconImage->SetBrushFromTexture(Def->Icon, true);
		IconImage->SetVisibility(Def->Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	if (TitleText)
	{
		const FText Title = !Def->DisplayName.IsEmpty()
			? Def->DisplayName
			: FText::FromString(Def->ActionTag.IsValid() ? Def->ActionTag.ToString() : FString(TEXT("Ability")));

		TitleText->SetText(Title);
		TitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}