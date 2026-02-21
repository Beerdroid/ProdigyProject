#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "ProdigyHotbarWidget.generated.h"

class UPanelWidget;
class UProdigyHotbarSlotWidget;

UENUM(BlueprintType)
enum class EProdigyHotbarEntryType : uint8
{
	None,
	Ability,
	Item
};

USTRUCT(BlueprintType)
struct FProdigyHotbarEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EProdigyHotbarEntryType Type = EProdigyHotbarEntryType::None;

	// Ability binding
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag AbilityTag;

	// Item binding
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnabled = true;
};

UCLASS()
class PRODIGYPROJECT_API UProdigyHotbarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Number of slots (you can change later without touching slot widgets)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hotbar")
	int32 NumSlots = 10;

	UPROPERTY(EditDefaultsOnly, Category="Hotbar")
	TSubclassOf<UProdigyHotbarSlotWidget> SlotWidgetClass;

	UFUNCTION(BlueprintCallable, Category="Hotbar")
	void Rebuild();

	UFUNCTION(BlueprintCallable, Category="Hotbar")
	void SetSlotAbility(int32 SlotIndex, FGameplayTag AbilityTag);

	UFUNCTION(BlueprintCallable, Category="Hotbar")
	void SetSlotItem(int32 SlotIndex, FName ItemID);

	UFUNCTION(BlueprintCallable, Category="Hotbar")
	void ClearSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category="Hotbar")
	FProdigyHotbarEntry GetSlotEntry(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category="Hotbar")
	void RefreshAllSlots();

	UFUNCTION(BlueprintCallable, Category="Hotbar")
	void SetEntryEnabled(int32 SlotIndex, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category="Hotbar")
	bool IsEntryEnabled(int32 SlotIndex) const;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UHorizontalBox> SlotRow = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProdigyHotbarSlotWidget>> SlotWidgets;

	UFUNCTION(BlueprintCallable, Category="Hotbar")
	void RebuildSlots();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UPanelWidget> SlotsPanel = nullptr;

	// Persistent hotbar assignments (runtime only)
	UPROPERTY(Transient)
	TArray<FProdigyHotbarEntry> Entries;

	UFUNCTION() void HandleHUDDirty();

	bool IsValidSlotIndex(int32 SlotIndex) const;
};
