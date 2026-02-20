#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "ProdigyHotbarWidget.h"
#include "ProdigyHotbarSlotWidget.generated.h"

class UProdigyAbilityTooltipWidget;
class UActionDefinition;
class UButton;
class UImage;
class UTextBlock;
class UProdigyHotbarWidget;

UCLASS()
class PRODIGYPROJECT_API UProdigyHotbarSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitSlot(UProdigyHotbarWidget* InOwner, int32 InSlotIndex);

	UFUNCTION(BlueprintCallable, Category="Hotbar")
	void Refresh();
	
	UPROPERTY(EditDefaultsOnly, Category="Hotbar|Tooltip")
	TSubclassOf<UProdigyAbilityTooltipWidget> AbilityTooltipClass;

protected:
	virtual void NativeConstruct() override;

	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> SlotButton = nullptr;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage>  IconImage = nullptr;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> CooldownText = nullptr;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> QtyText = nullptr;

	UPROPERTY(Transient) TWeakObjectPtr<UProdigyHotbarWidget> OwnerHotbar;
	UPROPERTY(Transient) int32 SlotIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UProdigyAbilityTooltipWidget> AbilityTooltipWidget = nullptr;

	void EnsureAbilityTooltip();
	void ClearAbilityTooltip();
	void UpdateAbilityTooltip(const UActionDefinition* Def);

	// Cached quantity for item bindings
	UPROPERTY(Transient) int32 CachedQuantity = 0;

	UFUNCTION() void HandleClicked();

	void ApplyVisuals(bool bEnabled, const FText& InCooldownText);

	// Finds first slot with this ItemID, returns INDEX_NONE if not found
	int32 FindFirstItemSlotByID(class UInventoryComponent* Inv, FName ItemID) const;
};
