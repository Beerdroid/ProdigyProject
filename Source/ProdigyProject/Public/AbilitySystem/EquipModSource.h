// EquipModSource.h (main module)
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "EquipModSource.generated.h"

UCLASS()
class PRODIGYPROJECT_API UEquipModSource : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FGameplayTag SlotTag;
};
