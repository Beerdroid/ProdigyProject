// BTT_DelayTurn.h
#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTT_DelayTurn.generated.h"

UCLASS()
class PRODIGYPROJECT_API UBTT_DelayTurn : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTT_DelayTurn();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category="Delay")
	float MinDelaySeconds = 0.35f;

	UPROPERTY(EditAnywhere, Category="Delay")
	float MaxDelaySeconds = 0.85f;

	struct FMem { float Remaining = 0.f; };
};