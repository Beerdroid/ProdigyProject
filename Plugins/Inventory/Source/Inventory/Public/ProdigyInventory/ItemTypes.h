// ItemTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ItemTypes.generated.h"

UENUM(BlueprintType)
enum class EItemCategory : uint8
{
	Consumable,
	Equipment,
	Quest,
	Junk,
	Craft
};

/**
 * Single source of truth row (DataTable).
 * Key this DataTable by ItemID (RowName == ItemID) for simplest lookup.
 */
USTRUCT(BlueprintType)
struct FItemRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ItemID = NAME_None; // optional, but handy
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly) EItemCategory Category = EItemCategory::Junk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bStackable = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 MaxStack = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Icon;

	// Optional: for "drop to world" / "spawn pickup"
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftClassPtr<AActor> WorldPickupClass;

	// Optional: equipment and extra classification
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag EquipSlotTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bNoDrop = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bNoSell = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 SellValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTagContainer Tags;
};

USTRUCT(BlueprintType)
struct FInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemID = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = 0;

	bool IsEmpty() const { return ItemID.IsNone() || Quantity <= 0; }
	void Clear() { ItemID = NAME_None; Quantity = 0; }
};

USTRUCT(BlueprintType)
struct FPredefinedItemEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ItemID = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct FInventorySlotView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bEmpty = true;

	UPROPERTY(BlueprintReadOnly) int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly) FName ItemID = NAME_None;
	UPROPERTY(BlueprintReadOnly) int32 Quantity = 0;

	// resolved presentation
	UPROPERTY(BlueprintReadOnly) FText DisplayName;
	UPROPERTY(BlueprintReadOnly) FText Description;
	UPROPERTY(BlueprintReadOnly) int32 Price;

	// keep soft to avoid forcing sync loads in slot widget
	UPROPERTY(BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Icon;

	// for hover later
	UPROPERTY(BlueprintReadOnly) EItemCategory Category = EItemCategory::Junk;
	UPROPERTY(BlueprintReadOnly) FGameplayTagContainer Tags;
};
