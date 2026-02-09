#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TurnResourceComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PRODIGYPROJECT_API UTurnResourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turn")
	int32 MaxAP = 4;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Turn")
	int32 CurrentAP = 4;

	UFUNCTION(BlueprintCallable, Category="Turn")
	void RefreshForTurn()
	{
		CurrentAP = MaxAP;
	}

	UFUNCTION(BlueprintCallable, Category="Turn")
	bool HasAP(int32 Amount) const
	{
		return Amount <= 0 || CurrentAP >= Amount;
	}

	UFUNCTION(BlueprintCallable, Category="Turn")
	bool SpendAP(int32 Amount)
	{
		if (Amount <= 0) return true;
		if (CurrentAP < Amount) return false;
		CurrentAP -= Amount;
		return true;
	}
};
