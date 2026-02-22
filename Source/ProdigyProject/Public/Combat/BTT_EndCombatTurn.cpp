#include "BTT_EndCombatTurn.h"

#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystem/CombatSubsystem.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTT_EndCombatTurn::UBTT_EndCombatTurn()
{
	NodeName = TEXT("End Combat Turn");
}

EBTNodeResult::Type UBTT_EndCombatTurn::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (BB)
	{
		BB->SetValueAsBool(TEXT("bTurnActive"), false);
	}

	AAIController* AIC = OwnerComp.GetAIOwner();
	APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;

	if (Pawn)
	{
		if (UGameInstance* GI = Pawn->GetGameInstance())
		{
			if (UCombatSubsystem* Combat = GI->GetSubsystem<UCombatSubsystem>())
			{
				Combat->EndCurrentTurn(Pawn);
			}
		}
	}

	// ✅ stop BT so it doesn't loop immediately
	OwnerComp.StopTree(EBTStopMode::Safe);

	return EBTNodeResult::Succeeded;
}