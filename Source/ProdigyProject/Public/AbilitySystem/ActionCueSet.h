#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ActionCueTypes.h"
#include "ActionCueSet.generated.h"

UCLASS(BlueprintType)
class PRODIGYPROJECT_API UActionCueSet : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FGameplayTag, FActionCueDef> Cues;

	UFUNCTION(BlueprintCallable)
	bool TryGetCue(const FGameplayTag& CueTag, FActionCueDef& OutDef) const
	{
		if (const FActionCueDef* Found = Cues.Find(CueTag))
		{
			OutDef = *Found;
			return true;
		}
		return false;
	}
};
