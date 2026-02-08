#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProdigyInventory/ItemTypes.h"
#include "InvItemTooltipWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS()
class INVENTORY_API UInvItemTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	void SetView(const FInventorySlotView& View);

protected:
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> ItemIcon = nullptr;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> NameText = nullptr;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> DescText = nullptr;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> PriceText = nullptr;
};
