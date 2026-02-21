#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProdigyHotbarDragVisualWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS()
class PRODIGYPROJECT_API UProdigyHotbarDragVisualWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Hotbar|Drag")
	void SetIcon(UTexture2D* InIcon);

	UFUNCTION(BlueprintCallable, Category="Hotbar|Drag")
	void SetQuantity(int32 InQuantity);

protected:
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> IconImage = nullptr;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> QtyText = nullptr;
};