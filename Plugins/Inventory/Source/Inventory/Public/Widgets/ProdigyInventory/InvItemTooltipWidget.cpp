#include "Widgets/ProdigyInventory/InvItemTooltipWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UInvItemTooltipWidget::SetView(const FInventorySlotView& View)
{
	// Icon (soft)
	if (IsValid(ItemIcon))
	{
		if (View.bEmpty)
		{
			ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			UTexture2D* Tex = View.Icon.IsValid() ? View.Icon.Get() : View.Icon.LoadSynchronous();
			ItemIcon->SetBrushFromTexture(Tex);
			ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	if (IsValid(NameText))
	{
		NameText->SetText(View.bEmpty ? FText::GetEmpty() : View.DisplayName);
	}

	if (IsValid(DescText))
	{
		DescText->SetText(View.bEmpty ? FText::GetEmpty() : View.Description);
	}
}
