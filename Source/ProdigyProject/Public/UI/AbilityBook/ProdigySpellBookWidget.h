#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProdigySpellBookWidget.generated.h"

class UPanelWidget;
class UProdigySpellBookSlotWidget;

UCLASS()
class PRODIGYPROJECT_API UProdigySpellBookWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category="SpellBook")
	void RefreshAllSlots();
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProdigySpellBookSlotWidget>> SlotWidgets;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> SlotsPanel = nullptr;

	UFUNCTION() void HandleHUDDirty();

	void RebuildSlotList();
};