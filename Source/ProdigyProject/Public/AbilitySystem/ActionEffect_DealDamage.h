#pragma once

#include "CoreMinimal.h"
#include "ActionEffect.h"
#include "ActionEffect_DealDamage.generated.h"

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class UActionEffect_DealDamage : public UActionEffect
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	float Damage = 10.f;

	virtual bool Apply_Implementation(const FActionContext& Context) const override;
};
