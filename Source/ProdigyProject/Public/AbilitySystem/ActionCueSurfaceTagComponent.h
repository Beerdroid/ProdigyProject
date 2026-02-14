#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ActionCueSurfaceTagComponent.generated.h"

UCLASS(ClassGroup=(Cues), meta=(BlueprintSpawnableComponent))
class PRODIGYPROJECT_API UActionCueSurfaceTagComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cues")
	FGameplayTag SurfaceTag;
};
