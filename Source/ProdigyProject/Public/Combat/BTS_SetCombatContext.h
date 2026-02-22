#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTS_SetCombatContext.generated.h"

UCLASS()
class PRODIGYPROJECT_API UBTS_SetCombatContext : public UBTService
{
	GENERATED_BODY()

public:
	UBTS_SetCombatContext();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector ActionTagKey;

	UPROPERTY(EditAnywhere, Category="AI")
	FName DefaultActionTag = FName("Action.Attack.Basic");
};