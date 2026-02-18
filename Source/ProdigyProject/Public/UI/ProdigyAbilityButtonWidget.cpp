#include "UI/ProdigyAbilityButtonWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"

void UProdigyAbilityButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (AbilityButton)
	{
		AbilityButton->OnClicked.RemoveAll(this);
		AbilityButton->OnClicked.AddDynamic(this, &UProdigyAbilityButtonWidget::HandleClicked);
	}
}

void UProdigyAbilityButtonWidget::Setup(FGameplayTag InAbilityTag, const FText& InTitle, UTexture2D* InIcon)
{
	AbilityTag = InAbilityTag;

	if (AbilityIcon)  AbilityIcon->SetBrushFromTexture(InIcon, /*bMatchSize*/ true);
	SetToolTipText(InTitle);
}

void UProdigyAbilityButtonWidget::SetAbilityEnabled(bool bEnabled)
{
	if (AbilityButton)
	{
		AbilityButton->SetIsEnabled(bEnabled);
	}
}

void UProdigyAbilityButtonWidget::HandleClicked()
{
	if (!AbilityTag.IsValid()) return;
	OnAbilityClicked.Broadcast(AbilityTag);
}
