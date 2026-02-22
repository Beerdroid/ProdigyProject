// BTT_DelayTurn.cpp
#include "BTT_DelayTurn.h"
#include "Math/UnrealMathUtility.h"

UBTT_DelayTurn::UBTT_DelayTurn()
{
	NodeName = TEXT("Delay Turn");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTT_DelayTurn::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FMem* M = (FMem*)NodeMemory;
	M->Remaining = FMath::FRandRange(MinDelaySeconds, MaxDelaySeconds);
	return EBTNodeResult::InProgress;
}

void UBTT_DelayTurn::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FMem* M = (FMem*)NodeMemory;
	M->Remaining -= DeltaSeconds;
	if (M->Remaining <= 0.f)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}