// InvContextMenuWidget.h
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InvContextMenuWidget.generated.h"

class UInventoryWidgetBase;
class UButton;
class UTextBlock;
class UInventoryComponent;
class AInvPlayerController;

UCLASS()
class INVENTORY_API UInvContextMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Called by inventory menu when opening
	void Init(AInvPlayerController* InPC, UInventoryComponent* InInv, int32 InSlotIndex, bool bInExternal, UInventoryWidgetBase* InOwnerMenu);

	// Let BP decide what buttons to show (split/consume etc.)
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory|UI")
	void ConfigureForSlot(bool bHasItem, bool bStackable, int32 Quantity);

	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	// Optional: close hook
	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	
	void Close();
	// Button handlers you bind in BP (or C++)
	UFUNCTION(BlueprintCallable) void Action_ConsumeOne();
	UFUNCTION(BlueprintCallable) void Action_DropOne();
	UFUNCTION(BlueprintCallable) void Action_DropAll();
	UFUNCTION(BlueprintCallable) void Action_BeginSplit(); // enter “split mode”

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION() void OnSplitClicked();
	UFUNCTION() void OnDropOneClicked();
	UFUNCTION() void OnDropAllClicked();
	UFUNCTION() void OnConsumeClicked();
	
	UPROPERTY(BlueprintReadOnly, Category="Inventory|UI")
	TObjectPtr<AInvPlayerController> PC = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Inventory|UI")
	TObjectPtr<UInventoryComponent> Inventory = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Inventory|UI")
	TObjectPtr<UInventoryWidgetBase> OwnerMenu = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Inventory|UI")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Inventory|UI")
	bool bExternal = false;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class USlider> SplitSlider = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SplitAmount = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_Split = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_DropOne = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_DropAll = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_Consume = nullptr;

	void RefreshSplitAmountText();

	UPROPERTY(BlueprintReadOnly, Category="Inventory|UI")
	int32 SplitAmount = 1;

	UPROPERTY(BlueprintReadOnly, Category="Inventory|UI")
	int32 SplitMax = 1;

	UFUNCTION()
	void OnSplitSliderChanged(float Value01);

	UFUNCTION()
	void OnSplitSpinChanged(float NewValue);


};
