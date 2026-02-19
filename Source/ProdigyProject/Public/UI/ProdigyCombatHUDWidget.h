#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "ProdigyCombatHUDWidget.generated.h"

class UProdigyAbilityButtonWidget;
class UTextBlock;
class UButton;
class UHorizontalBox;
class AProdigyPlayerController;

UCLASS()
class PRODIGYPROJECT_API UProdigyCombatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CombatHUD")
	TArray<FGameplayTag> AbilityTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CombatHUD")
	TArray<FText> AbilityLabels;

	UFUNCTION(BlueprintCallable, Category="CombatHUD")
	void Refresh();

	AProdigyPlayerController* GetProdigyPC() const;

	// Ability button widget class (create a WBP child for visuals)
	UPROPERTY(EditDefaultsOnly, Category="CombatHUD")
	TSubclassOf<UProdigyAbilityButtonWidget> AbilityButtonClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProdigyAbilityButtonWidget>> AbilityButtonWidgets;

	UFUNCTION()
	void HandleAbilityClicked(FGameplayTag AbilityTag);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// --- REQUIRED WBP ELEMENTS (must exist in the widget blueprint) ---

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> HPText = nullptr;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> APText = nullptr;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> TurnText = nullptr;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> StartCombatButton = nullptr;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> EndTurnButton = nullptr;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UHorizontalBox> AbilityRow = nullptr;

	// --- Runtime ---
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> AbilityButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UProdigyAbilityClickProxy>> AbilityClickProxies;

	void RebuildAbilityButtons();

	UFUNCTION() void HandleStartCombatClicked();
	UFUNCTION() void HandleEndTurnClicked();
};

UCLASS()
class PRODIGYPROJECT_API UProdigyAbilityClickProxy : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TWeakObjectPtr<UProdigyCombatHUDWidget> OwnerWidget;
	UPROPERTY() FGameplayTag AbilityTag;

	UFUNCTION() void HandleClicked();
};
