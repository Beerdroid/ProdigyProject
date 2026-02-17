#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AttributesComponent.generated.h"

DEFINE_LOG_CATEGORY_STATIC(LogAttributes, Log, All);

USTRUCT(BlueprintType)
struct FAttributeEntry
{
	GENERATED_BODY()

	// Identifier, e.g. Attr.Health, Attr.MaxHealth, Attr.AP, Attr.MaxAP
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	FGameplayTag AttributeTag;

	// "Base" or template value (optional for now but useful later)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	float BaseValue = 0.f;

	// Current runtime value
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	float CurrentValue = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnAttributeChanged,
	FGameplayTag, AttributeTag,
	float, NewValue,
	float, Delta,
	AActor*, InstigatorActor
);

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

	// Initial attributes for this actor (set in BP defaults)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	TArray<FAttributeEntry> DefaultAttributes;

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

	// --- Write ---
	UFUNCTION(BlueprintCallable, Category="Attributes")
	bool SetBaseValue(FGameplayTag AttributeTag, float NewBaseValue, AActor* InstigatorActor);

	UFUNCTION(BlueprintCallable, Category="Attributes")
	bool SetCurrentValue(FGameplayTag AttributeTag, float NewCurrentValue, AActor* InstigatorActor);

	UFUNCTION(BlueprintCallable, Category="Attributes")
	bool ModifyCurrentValue(FGameplayTag AttributeTag, float Delta, AActor* InstigatorActor);

	// Convenience: sets CurrentValue of tag A to CurrentValue of tag B (commonly AP <- MaxAP)
	UFUNCTION(BlueprintCallable, Category="Attributes")
	bool CopyCurrentValue(FGameplayTag FromTag, FGameplayTag ToTag, AActor* InstigatorActor);

protected:
	virtual void BeginPlay() override;

private:
	// Internal map for O(1) access at runtime (built from DefaultAttributes)
	UPROPERTY(Transient)
	TMap<FGameplayTag, FAttributeEntry> AttributeMap;

	void BuildMapFromDefaults();

	FAttributeEntry* FindEntryMutable(FGameplayTag AttributeTag);
	const FAttributeEntry* FindEntry(FGameplayTag AttributeTag) const;

	void BroadcastChanged(const FGameplayTag Tag, float OldValue, float NewValue, AActor* InstigatorActor);
};
