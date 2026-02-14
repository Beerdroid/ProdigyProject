#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActionCueProviderComponent.generated.h"

class UActionCueSet;

UCLASS(ClassGroup=(Cues), meta=(BlueprintSpawnableComponent))
class PRODIGYPROJECT_API UActionCueProviderComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cues")
	TObjectPtr<UActionCueSet> CueSet = nullptr;
};
