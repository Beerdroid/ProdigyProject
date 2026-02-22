#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTT_ExecuteCombatAction.generated.h"

UCLASS()
class PRODIGYPROJECT_API UBTT_ExecuteCombatAction : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTT_ExecuteCombatAction();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector ActionTagNameKey;

	UPROPERTY(EditAnywhere, Category="Action")
	FGameplayTag DefaultActionTag;
};