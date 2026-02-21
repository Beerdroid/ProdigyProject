#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProdigyAbilityDragVisualWidget.generated.h"

class UImage;
class UTextBlock;
class UActionDefinition;

UCLASS()
class PRODIGYPROJECT_API UProdigyAbilityDragVisualWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="AbilityDragVisual")
	void SetFromDefinition(const UActionDefinition* Def);

protected:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> IconImage = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText = nullptr;
};