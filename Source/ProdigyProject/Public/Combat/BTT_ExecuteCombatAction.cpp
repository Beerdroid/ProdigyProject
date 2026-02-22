#include "BTT_ExecuteCombatAction.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/CombatSubsystem.h"

UBTT_ExecuteCombatAction::UBTT_ExecuteCombatAction()
{
	NodeName = TEXT("Execute Combat Action");

	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTT_ExecuteCombatAction, TargetActorKey), AActor::StaticClass());
	ActionTagNameKey.AddNameFilter(this, GET_MEMBER_NAME_CHECKED(UBTT_ExecuteCombatAction, ActionTagNameKey));

	DefaultActionTag = FGameplayTag::RequestGameplayTag(FName("Action.Attack.Basic"));
}

EBTNodeResult::Type UBTT_ExecuteCombatAction::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIC = OwnerComp.GetAIOwner();
	APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		return EBTNodeResult::Failed;
	}

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!IsValid(Target))
	{
		// No target -> action not executed (EndTurn task will still run after this if your tree continues)
		return EBTNodeResult::Failed;
	}

	if (Target == Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BT] ExecuteCombatAction: Target==Self -> failing"));
		// Let EndTurn task end the turn; do NOT spam action
		return EBTNodeResult::Failed;
	}

	UActionComponent* AC = Pawn->FindComponentByClass<UActionComponent>();
	if (!IsValid(AC))
	{
		return EBTNodeResult::Failed;
	}

	// Resolve ActionTag from BB (Name -> GameplayTag), fallback to default
	FGameplayTag ActionTag = DefaultActionTag;

	const FName TagName = BB->GetValueAsName(ActionTagNameKey.SelectedKeyName);
	if (!TagName.IsNone())
	{
		const FGameplayTag FromBB = FGameplayTag::RequestGameplayTag(TagName, /*ErrorIfNotFound*/ false);
		if (FromBB.IsValid())
		{
			ActionTag = FromBB;
		}
	}

	FActionContext Ctx;
	Ctx.Instigator = Pawn;
	Ctx.TargetActor = Target;

	const bool bOk = AC->ExecuteAction(ActionTag, Ctx);
	return bOk ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}