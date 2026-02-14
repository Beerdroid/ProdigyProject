#pragma once

#include "CoreMinimal.h"
#include "ActionEffect.h"
#include "GameplayTagContainer.h"
#include "ActionEffect_ApplyStatus.generated.h"

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class UActionEffect_ApplyStatus : public UActionEffect
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	FGameplayTag StatusTag;

	// Dual duration support; you can set either/both in the asset.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	int32 Turns = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	float Seconds = 0.f;

	virtual bool Apply_Implementation(const FActionContext& Context) const override;
};
