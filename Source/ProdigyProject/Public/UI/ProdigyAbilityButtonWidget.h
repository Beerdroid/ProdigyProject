#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "ProdigyAbilityButtonWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityButtonClicked, FGameplayTag, AbilityTag);

UCLASS()
class PRODIGYPROJECT_API UProdigyAbilityButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Set by HUD when building the bar
	UFUNCTION(BlueprintCallable, Category="AbilityButton")
	void Setup(FGameplayTag InAbilityTag, const FText& InTitle, UTexture2D* InIcon);

	UFUNCTION(BlueprintCallable, Category="AbilityButton")
	void SetAbilityEnabled(bool bEnabled);

	UPROPERTY(BlueprintAssignable, Category="AbilityButton")
	FOnAbilityButtonClicked OnAbilityClicked;

	// Exposed for HUD if you want
	UFUNCTION(BlueprintCallable, Category="AbilityButton")
	FGameplayTag GetAbilityTag() const { return AbilityTag; }

protected:
	virtual void NativeConstruct() override;

private:
	// Required bindings in WBP (same names)
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> AbilityButton = nullptr;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> AbilityIcon = nullptr;

	UPROPERTY(Transient) FGameplayTag AbilityTag;

	UFUNCTION() void HandleClicked();
};
