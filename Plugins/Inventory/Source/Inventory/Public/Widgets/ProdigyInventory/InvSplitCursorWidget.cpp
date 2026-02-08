#include "InvSplitCursorWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UInvSplitCursorWidget::SetPreview(UTexture2D* InIcon, int32 InQuantity)
{
	UE_LOG(LogTemp, Warning, TEXT("[SplitCursor] SetPreview Icon=%s Amount=%d ItemIcon=%d QuantityText=%d"),
		*GetNameSafe(InIcon),
		InQuantity,
		IsValid(ItemIcon) ? 1 : 0,
		IsValid(QuantityText) ? 1 : 0);

	if (IsValid(ItemIcon))
	{
		if (InIcon)
		{
			ItemIcon->SetBrushFromTexture(InIcon);
			ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);

			// IMPORTANT: if brush size is 0, force a size (UMG can have 0x0 if not constrained)
			FSlateBrush Brush = ItemIcon->GetBrush();
			if (Brush.ImageSize.IsNearlyZero())
			{
				Brush.ImageSize = FVector2D(48.f, 48.f);
				ItemIcon->SetBrush(Brush);
			}

			UE_LOG(LogTemp, Warning, TEXT("[SplitCursor] Icon brush size=%s"),
				*ItemIcon->GetBrush().ImageSize.ToString());
		}
		else
		{
			ItemIcon->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (IsValid(QuantityText))
	{
		if (InQuantity > 1)
		{
			QuantityText->SetText(FText::AsNumber(InQuantity));
			QuantityText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			QuantityText->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}
