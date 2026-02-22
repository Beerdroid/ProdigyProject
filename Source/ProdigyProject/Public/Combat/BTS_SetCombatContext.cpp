#include "BTS_SetCombatContext.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

UBTS_SetCombatContext::UBTS_SetCombatContext()
{
	bNotifyTick = true;

	TargetActorKey.SelectedKeyName = "TargetActor";
	ActionTagKey.SelectedKeyName = "ActionTag";
}

void UBTS_SetCombatContext::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return;

	APawn* SelfPawn = OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr;
	if (!SelfPawn) return;

	// Minimal: always target player pawn 0
	AActor* PlayerPawn = UGameplayStatics::GetPlayerPawn(SelfPawn->GetWorld(), 0);
	BB->SetValueAsObject(TargetActorKey.SelectedKeyName, PlayerPawn);

	// Minimal: always basic attack
	BB->SetValueAsName(ActionTagKey.SelectedKeyName, DefaultActionTag);
}