#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProdigyInventory/ItemTypes.h"
#include "InvDragVisualWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS()
class INVENTORY_API UInvDragVisualWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	void SetFromSlotView(const FInventorySlotView& View);

protected:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> ItemIcon = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText = nullptr;
};
