#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ActionEffect.h"
#include "GameplayTagContainer.h"
#include "ActionEffect_DealDamage.generated.h"

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class PRODIGYPROJECT_API UActionEffect_DealDamage : public UActionEffect
{
	GENERATED_BODY()

public:
	// Explicit: which attribute to modify (usually Attr.Health)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage")
	FGameplayTag HealthAttributeTag;

	// Explicit: amount of damage (positive number)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0.0"))
	float Damage = 0.f;

	// Explicit: clamp at 0 (recommended)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage")
	bool bClampMinZero = true;

	virtual bool Apply_Implementation(const FActionContext& Context) const override;
};
