#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProdigyAbilityTooltipWidget.generated.h"

enum class EActionTargetingMode : uint8;
class UImage;
class UTextBlock;
class UActionDefinition;

UCLASS()
class PRODIGYPROJECT_API UProdigyAbilityTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Tooltip")
	void SetFromDefinition(const UActionDefinition* Def);

	UFUNCTION(BlueprintCallable, Category="Tooltip")
	void Clear();

protected:
	// Optional bindings in WBP (bind what you want to show)
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> IconImage = nullptr;

	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText = nullptr;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> DescText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TargetingText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> APCostText = nullptr;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> CooldownTurnsText = nullptr;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> CooldownSecondsText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> UsableText = nullptr;

private:
	static FText TargetingModeToText(EActionTargetingMode Mode);
};