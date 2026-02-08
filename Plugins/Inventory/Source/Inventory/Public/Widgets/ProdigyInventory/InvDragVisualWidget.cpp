#include "InvDragVisualWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UInvDragVisualWidget::SetFromSlotView(const FInventorySlotView& View)
{
	if (IsValid(ItemIcon))
	{
		UTexture2D* Tex = View.Icon.LoadSynchronous();
		ItemIcon->SetBrushFromTexture(Tex, true);
		ItemIcon->SetVisibility(Tex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	if (IsValid(QuantityText))
	{
		if (!View.bEmpty && View.Quantity > 1)
		{
			QuantityText->SetText(FText::AsNumber(View.Quantity));
			QuantityText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			QuantityText->SetText(FText::GetEmpty());
			QuantityText->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}
