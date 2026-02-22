#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTT_EndCombatTurn.generated.h"

UCLASS()
class PRODIGYPROJECT_API UBTT_EndCombatTurn : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTT_EndCombatTurn();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};