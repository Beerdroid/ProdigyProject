#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InvSplitCursorWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

UCLASS()
class INVENTORY_API UInvSplitCursorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	void SetPreview(UTexture2D* InIcon, int32 InQuantity);

protected:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> ItemIcon = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText = nullptr;
};
