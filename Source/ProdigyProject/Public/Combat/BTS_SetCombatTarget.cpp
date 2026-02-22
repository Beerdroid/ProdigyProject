#include "BTS_SetCombatTarget.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

UBTS_SetCombatTarget::UBTS_SetCombatTarget()
{
	bNotifyTick = true;
	NodeName = TEXT("Set Combat Target");

	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTS_SetCombatTarget, TargetActorKey), AActor::StaticClass());
	ActionTagNameKey.AddNameFilter(this, GET_MEMBER_NAME_CHECKED(UBTS_SetCombatTarget, ActionTagNameKey));

	// If you want default key names (optional, editor can override)
	TargetActorKey.SelectedKeyName = TEXT("TargetActor");
	ActionTagNameKey.SelectedKeyName = TEXT("ActionTagToUse");
}

void UBTS_SetCombatTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return;

	AAIController* AIC = OwnerComp.GetAIOwner();
	APawn* SelfPawn = AIC ? AIC->GetPawn() : nullptr;
	if (!SelfPawn) return;

	AActor* NewTarget = nullptr;

	if (bAlwaysTargetPlayer0)
	{
		NewTarget = UGameplayStatics::GetPlayerPawn(SelfPawn->GetWorld(), 0);
	}

	// If you later add faction/participants-based logic, this is the place to swap it in.
	// For now: player pawn 0 is enough.

	if (IsValid(NewTarget))
	{
		BB->SetValueAsObject(TargetActorKey.SelectedKeyName, NewTarget);
	}

	if (bSetActionTagName)
	{
		BB->SetValueAsName(ActionTagNameKey.SelectedKeyName, DefaultActionTagName);
	}
}