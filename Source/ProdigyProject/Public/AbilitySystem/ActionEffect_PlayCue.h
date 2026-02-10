#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/ActionEffect.h"
#include "AbilitySystem/CombatCueSet.h"
#include "ActionEffect_PlayCue.generated.h"

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class PRODIGYPROJECT_API UActionEffect_PlayCue : public UActionEffect
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|Effect")
	FGameplayTag CueTag;

	// Where this effect wants to spawn the cue (unless the cue def overrides it)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|Effect")
	ECombatCueAnchor DefaultAnchor = ECombatCueAnchor::Target;

	virtual bool Apply_Implementation(const FActionContext& Context) const override;
};
