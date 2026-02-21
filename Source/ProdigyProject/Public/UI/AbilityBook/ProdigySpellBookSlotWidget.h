#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/ActionDefinition.h"
#include "ProdigySpellBookSlotWidget.generated.h"

class UProdigyAbilityDragVisualWidget;
class UProdigyAbilityTooltipWidget;
class UProdigySpellBookWidget;
class UButton;
class UImage;
class UTextBlock;

UCLASS()
class PRODIGYPROJECT_API UProdigySpellBookSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="SpellBook")
	void Setup(FGameplayTag InAbilityTag);

	UFUNCTION(BlueprintCallable, Category="SpellBook")
	FGameplayTag GetAbilityTag() const { return AbilityTag; }

	UFUNCTION(BlueprintCallable, Category="SpellBook")
	void RefreshFromOwner();

	UFUNCTION(BlueprintCallable, Category="SpellBook")
	void SetSlotEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category="SpellBook")
	bool IsSlotEnabled() const { return bSlotEnabled; }

	// Policy: SlotTag == AbilityTag. This returns the effective tag.
	FGameplayTag GetSlotTag() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SpellBook")
	FGameplayTag SlotTag;

	UPROPERTY()
	UProdigySpellBookWidget* OwnerSpellBook = nullptr;

	void SetAbilityTag(FGameplayTag InAbilityTag);
	void BindOwnerSpellBook(UProdigySpellBookWidget* InOwner);

	UPROPERTY(EditDefaultsOnly, Category="SpellBook|Drag")
	TSubclassOf<UProdigyAbilityDragVisualWidget> DragVisualClass;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(EditDefaultsOnly, Category="SpellBook|Tooltip")
	TSubclassOf<UProdigyAbilityTooltipWidget> AbilityTooltipClass;

	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

private:
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage>  IconImage = nullptr;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText = nullptr;

	UPROPERTY(Transient) bool bSlotEnabled = true;
	UPROPERTY(Transient) FGameplayTag AbilityTag;
	UPROPERTY(Transient) bool bKnown = false;

	const UActionDefinition* ResolveDefinitionFromOwner() const;

	UPROPERTY(Transient)
	TObjectPtr<UProdigyAbilityTooltipWidget> AbilityTooltipWidget = nullptr;

	void EnsureAbilityTooltip();
	void ClearAbilityTooltip();
	void UpdateAbilityTooltip(const UActionDefinition* Def);

	UFUNCTION() void HandleClicked();
	void ApplyKnownState(bool bIsKnown);
};