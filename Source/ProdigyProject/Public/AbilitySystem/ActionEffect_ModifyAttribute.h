#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/ActionEffect.h"
#include "ActionEffect_ModifyAttribute.generated.h"

UENUM(BlueprintType)
enum class EActionEffectTarget : uint8
{
	Instigator UMETA(DisplayName="Instigator"),
	Target     UMETA(DisplayName="Target"),
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class PRODIGYPROJECT_API UActionEffect_ModifyAttribute : public UActionEffect
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|Effect")
	FGameplayTag AttributeTag;

	// Signed delta. Negative = damage/spend, Positive = heal/gain.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|Effect")
	float Delta = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|Effect")
	EActionEffectTarget Target = EActionEffectTarget::Target;

	// Explicit clamping (NO magic): you choose per effect.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|Effect|Clamp")
	bool bClampMinZero = false;

	// If true, you must set MaxAttributeTag explicitly.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|Effect|Clamp")
	bool bClampToMaxAttribute = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|Effect|Clamp", meta=(EditCondition="bClampToMaxAttribute"))
	FGameplayTag MaxAttributeTag;

	virtual bool Apply_Implementation(const FActionContext& Context) const override;

private:
	AActor* ResolveTargetActor(const FActionContext& Context) const;
};
