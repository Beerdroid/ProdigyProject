#pragma once

#include "CoreMinimal.h"
#include "AttributeModTypes.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AttributesComponent.generated.h"

class UAttributeSetDataAsset;
DEFINE_LOG_CATEGORY_STATIC(LogAttributes, Log, All);

USTRUCT(BlueprintType)
struct FAttributeEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	FGameplayTag AttributeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	float BaseValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	float CurrentValue = 0.f;
};


USTRUCT()
struct FAttrModSource
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAttributeMod> Mods;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnFinalAttributeChanged,
	FGameplayTag, AttributeTag,
	float, NewFinalValue,
	float, Delta,
	UObject*, Source
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnAttributeChanged,
	FGameplayTag, AttributeTag,
	float, NewValue,
	float, Delta,
	AActor*, InstigatorActor
);

USTRUCT(BlueprintType)
struct FPeriodicTurnEffect
{
	GENERATED_BODY()

	// Identity / grouping (Poison, Regen, Bleed...)
	UPROPERTY() FGameplayTag EffectTag;

	// What resource is affected (Attr.Health, Attr.AP, Attr.Mana...)
	UPROPERTY() FGameplayTag AttributeTag;

	// Signed delta applied each tick (negative=DoT, positive=HoT)
	UPROPERTY() float DeltaPerTurn = 0.f;

	// Exact remaining ticks
	UPROPERTY() int32 TurnsRemaining = 0;

	// Who applied it (for logs / later dispel rules)
	UPROPERTY() TWeakObjectPtr<AActor> InstigatorActor;
};

/**
 * Simple tag-addressed attribute storage.
 * - No replication (per your requirement).
 * - No modifier stack/aggregation in v1 (we can add later).
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PRODIGYPROJECT_API UAttributesComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAttributesComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	TObjectPtr<UAttributeSetDataAsset> AttributeSet = nullptr;

	// Fired whenever an attribute changes via Set/Modify
	UPROPERTY(BlueprintAssignable, Category="Attributes")
	FOnAttributeChanged OnAttributeChanged;

	// --- Query ---
	UFUNCTION(BlueprintCallable, Category="Attributes")
	bool HasAttribute(FGameplayTag AttributeTag) const;

	UFUNCTION(BlueprintCallable, Category="Attributes")
	float GetBaseValue(FGameplayTag AttributeTag) const;

	UFUNCTION(BlueprintCallable, Category="Attributes")
	float GetCurrentValue(FGameplayTag AttributeTag) const;

	// NEW: Base + modifiers (equipment, buffs, etc.)
	UFUNCTION(BlueprintCallable, Category="Attributes")
	float GetFinalValue(FGameplayTag AttributeTag) const;

	// --- Write ---
	UFUNCTION(BlueprintCallable, Category="Attributes")
	bool SetBaseValue(FGameplayTag AttributeTag, float NewBaseValue, AActor* InstigatorActor);

	UFUNCTION(BlueprintCallable, Category="Attributes")
	bool SetCurrentValue(FGameplayTag AttributeTag, float NewCurrentValue, AActor* InstigatorActor);

	UFUNCTION(BlueprintCallable, Category="Attributes")
	bool ModifyCurrentValue(FGameplayTag AttributeTag, float Delta, AActor* InstigatorActor);

	UFUNCTION(BlueprintCallable, Category="Attributes")
	bool CopyCurrentValue(FGameplayTag FromTag, FGameplayTag ToTag, AActor* InstigatorActor);

	// --- Modifier Layer API ---
	UFUNCTION(BlueprintCallable, Category="Attributes|Mods")
	void SetModsForSource(UObject* Source, const TArray<FAttributeMod>& Mods, UObject* InstigatorSource);

	UFUNCTION(BlueprintCallable, Category="Attributes|Mods")
	void ClearModsForSource(UObject* Source, UObject* InstigatorSource);

	UFUNCTION(BlueprintCallable, Category="Attributes|Mods")
	bool ApplyItemAttributeModsAsCurrentDeltas(const TArray<FAttributeMod>& ItemMods, AActor* InstigatorActor);

	UFUNCTION(BlueprintCallable, Category="Attributes|TurnEffects")
	bool AddOrRefreshTurnEffect(
		const FGameplayTag& EffectTag,
		const FGameplayTag& AttributeTag,
		float DeltaPerTurn,
		int32 NumTurns,
		AActor* InstigatorActor,
		bool bRefreshDuration = true,
		bool bStackMagnitude = false);
	
	UFUNCTION(BlueprintCallable, Category="Attributes|Periodic")
	void TickTurnEffects(AActor* OwnerTurnActor);

	// Optional utility
	UFUNCTION(BlueprintCallable, Category="Attributes|Periodic")
	void ClearTurnEffect(FGameplayTag EffectTag);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	bool bDefaultsInitialized = false;

	UPROPERTY()
	TArray<FPeriodicTurnEffect> TurnEffects;

	int32 FindTurnEffectIndex(const FGameplayTag& EffectTag, const FGameplayTag& AttributeTag) const;

	// Internal map for O(1) access at runtime (built from DefaultAttributes)
	UPROPERTY(Transient)
	TMap<FGameplayTag, FAttributeEntry> AttributeMap;

	void AppendDefaultsToMap(const TArray<FAttributeEntry>& InDefaults);

	void AppendDefaultsToMap_KeepCurrent(const TArray<FAttributeEntry>& InDefaults);

	// Source -> mods
	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<UObject>, FAttrModSource> ModSources;

	void BuildMapFromDefaults();

	FAttributeEntry* FindEntryMutable(FGameplayTag AttributeTag);
	const FAttributeEntry* FindEntry(FGameplayTag AttributeTag) const;

	void BroadcastChanged(const FGameplayTag Tag, float OldValue, float NewValue, AActor* InstigatorActor);

	// Tag-based policy (no editor setup)
	void ClampResourcesIfNeeded(UObject* InstigatorSource);

	// Helpers
	void ReClampAllRelevantCurrents(UObject* InstigatorSource);

	// stable "source objects" per equip slot tag so each slot has its own modifier source
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UObject>> EquipSlotSources;
};