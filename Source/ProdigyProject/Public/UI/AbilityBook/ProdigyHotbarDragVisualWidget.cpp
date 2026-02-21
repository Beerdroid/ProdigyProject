#include "ProdigyHotbarDragVisualWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UProdigyHotbarDragVisualWidget::SetIcon(UTexture2D* InIcon)
{
	if (IconImage)
	{
		IconImage->SetBrushFromTexture(InIcon, true);
	}
}

void UProdigyHotbarDragVisualWidget::SetQuantity(int32 InQuantity)
{
	if (!QtyText) return;

	if (InQuantity > 1)
	{
		QtyText->SetText(FText::AsNumber(InQuantity));
		QtyText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		QtyText->SetText(FText::GetEmpty());
		QtyText->SetVisibility(ESlateVisibility::Hidden);
	}
}