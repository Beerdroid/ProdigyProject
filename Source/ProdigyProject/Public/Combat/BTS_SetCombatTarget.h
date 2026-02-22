#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTS_SetCombatTarget.generated.h"

UCLASS()
class PRODIGYPROJECT_API UBTS_SetCombatTarget : public UBTService
{
	GENERATED_BODY()

public:
	UBTS_SetCombatTarget();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector TargetActorKey;

	// Optional: if you also want service to set the action tag name.
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector ActionTagNameKey;

	UPROPERTY(EditAnywhere, Category="AI")
	bool bSetActionTagName = true;

	UPROPERTY(EditAnywhere, Category="AI")
	FName DefaultActionTagName = FName("Action.Attack.Basic");

	// Minimal target selection: player pawn (single player).
	UPROPERTY(EditAnywhere, Category="AI")
	bool bAlwaysTargetPlayer0 = true;
};