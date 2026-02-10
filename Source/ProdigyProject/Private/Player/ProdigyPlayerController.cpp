#include "Player/ProdigyPlayerController.h"

#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/ActionTypes.h"
#include "AbilitySystem/CombatSubsystem.h"
#include "Quest/QuestLogComponent.h"
#include "Quest/Integration/QuestIntegrationComponent.h"
#include "GameFramework/Actor.h"
#include "Interfaces/CombatantInterface.h"
#include "Interfaces/UInv_Interactable.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"

AProdigyPlayerController::AProdigyPlayerController()
{
	// Nothing required here; components can be added in BP.
}

void AProdigyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	CacheQuestComponents();

	UE_LOG(LogTemp, Warning, TEXT("ProdigyPC BeginPlay: %s Local=%d Pawn=%s"),
	*GetNameSafe(this), IsLocalController(), *GetNameSafe(GetPawn()));
}

bool AProdigyPlayerController::HandlePrimaryClickActor(AActor* ClickedActor)
{
	TARGET_LOG(Verbose,
		TEXT("HandlePrimaryClickActor: %s"),
		*GetNameSafe(ClickedActor)
	);

	if (TryLockTarget(ClickedActor))
	{
		TARGET_LOG(Log, TEXT("Click consumed by target lock"));
		return true;
	}

	return false;
}

void AProdigyPlayerController::CacheQuestComponents()
{
	// If you add them as components in BP child, FindComponentByClass will find them.
	if (!QuestLog)
	{
		QuestLog = FindComponentByClass<UQuestLogComponent>();
	}

	if (!QuestIntegration)
	{
		QuestIntegration = FindComponentByClass<UQuestIntegrationComponent>();
	}

	// Optional: log once if missing
	if (!QuestLog)
	{
		UE_LOG(LogTemp, Warning, TEXT("ProdigyPlayerController: QuestLogComponent not found on %s"), *GetNameSafe(this));
	}
}

void AProdigyPlayerController::NotifyQuestsInventoryChanged()
{
	QuestLog->NotifyInventoryChanged();
}

void AProdigyPlayerController::NotifyQuestsKillTag(FGameplayTag TargetTag)
{
	if (!TargetTag.IsValid())
	{
		QuestLog->NotifyKillObjectiveTag(TargetTag);
	}
}

void AProdigyPlayerController::ApplyQuests(FName QuestID)
{
	UE_LOG(LogTemp, Warning, TEXT("AProdigyPlayerController ApplyQuests Owner=%s"), *QuestID.ToString());
	QuestLog->AddQuest(QuestID);
}

void AProdigyPlayerController::PrimaryInteract()
{
	UE_LOG(LogTemp, Warning, TEXT("PrimaryInteract fired"));

	// Use hover (already computed by your trace timer in base)
	AActor* Target = GetHoveredActor();
	if (!IsValid(Target))
	{
		return;
	}

	// 1) If it's a valid combat target, lock it (RMB select)
	if (TryLockTarget(Target))
	{
		TARGET_LOG(Log, TEXT("PrimaryInteract consumed by target lock"));
		return;
	}

	// 2) If it's an item, pick it up (or move to it + pickup)
	if (TryPrimaryPickup(Target))
	{
		return;
	}

	// 3) Otherwise, generic interaction
	if (Target->GetClass()->ImplementsInterface(UInv_Interactable::StaticClass()))
	{
		IInv_Interactable::Execute_Interact(Target, this, GetPawn());
		return;
	}

	if (UActorComponent* InteractableComp = Target->FindComponentByInterface(UInv_Interactable::StaticClass()))
	{
		IInv_Interactable::Execute_Interact(InteractableComp, this, GetPawn());
		return;
	}
}

void AProdigyPlayerController::ClearLockedTarget()
{
	if (!LockedTarget) return;

	TARGET_LOG(Log,
		TEXT("ClearTarget (was %s)"),
		*GetNameSafe(LockedTarget)
	);
	
	LockedTarget = nullptr;
	OnTargetLocked.Broadcast(nullptr);
}

bool AProdigyPlayerController::TryLockTargetUnderCursor()
{
	FHitResult Hit;
	const bool bHit = GetHitResultUnderCursor(
		TargetTraceChannel,
		/*bTraceComplex*/ false,
		Hit
	);

	AActor* Candidate = bHit ? Hit.GetActor() : nullptr;

	TARGET_LOG(Verbose,
		TEXT("TryLockTargetUnderCursor: Hit=%d Actor=%s"),
		bHit ? 1 : 0,
		*GetNameSafe(Candidate)
	);

	if (!IsValid(Candidate))
	{
		ClearLockedTarget();
		return false;
	}

	return TryLockTarget(Candidate);
}

bool AProdigyPlayerController::TryLockTarget(AActor* Candidate)
{
	if (!IsValid(Candidate))
	{
		TARGET_LOG(Verbose, TEXT("TryLockTarget: invalid candidate"));
		return false;
	}

	if (bOnlyLockCombatants &&
		!Candidate->GetClass()->ImplementsInterface(UCombatantInterface::StaticClass()))
	{
		TARGET_LOG(Verbose,
			TEXT("TryLockTarget: rejected %s (not Combatant)"),
			*GetNameSafe(Candidate)
		);
		return false;
	}

	// Toggle off
	if (LockedTarget == Candidate)
	{
		TARGET_LOG(Log,
			TEXT("TryLockTarget: toggle off %s"),
			*GetNameSafe(Candidate)
		);
		ClearLockedTarget();
		return true;
	}

	TARGET_LOG(Log,
		TEXT("TryLockTarget: success -> %s"),
		*GetNameSafe(Candidate)
	);

	SetLockedTarget(Candidate);
	return true;
}


void AProdigyPlayerController::SetLockedTarget(AActor* NewTarget)
{
	if (LockedTarget == NewTarget) return;

	TARGET_LOG(Log,
	TEXT("LockTarget -> %s"),
	*GetNameSafe(NewTarget)
);

	LockedTarget = NewTarget;
	OnTargetLocked.Broadcast(LockedTarget);
}

FActionContext AProdigyPlayerController::BuildActionContextFromLockedTarget() const
{
	FActionContext Ctx;
	Ctx.Instigator = GetPawn();
	Ctx.TargetActor = LockedTarget;
	return Ctx;
}

bool AProdigyPlayerController::IsMyTurn() const
{
	APawn* P = GetPawn();
	if (!IsValid(P)) return false;

	UCombatSubsystem* Combat = GetCombatSubsystem();
	if (!Combat || !Combat->IsInCombat()) return true; // not in combat => allow

	return Combat->GetCurrentTurnActor() == P;
}

bool AProdigyPlayerController::TryUseAbilityOnLockedTarget(FGameplayTag AbilityTag)
{
	APawn* P = GetPawn();
	if (!IsValid(P)) return false;

	UCombatSubsystem* Combat = GetCombatSubsystem();
	if (!Combat) return false;

	// Turn gate (only when in combat)
	if (Combat->IsInCombat())
	{
		AActor* TurnActor = Combat->GetCurrentTurnActor();
		if (TurnActor != P)
		{
			UE_LOG(LogActionExec, Warning,
				TEXT("[PC:%s] Ability blocked: not your turn (Turn=%s)"),
				*GetNameSafe(this), *GetNameSafe(TurnActor));
			return false;
		}
	}

	AActor* Target = LockedTarget.Get();
	if (!IsValid(Target))
	{
		UE_LOG(LogActionExec, Warning, TEXT("[PC:%s] Ability blocked: no LockedTarget"), *GetNameSafe(this));
		return false;
	}

	// Start combat only when you actually cast (not on select)
	if (!Combat->IsInCombat())
	{
		TArray<AActor*> Parts;
		Parts.Add(Target);
		Parts.Add(P);
		Combat->EnterCombat(Parts, /*FirstToAct*/ P);
	}

	UActionComponent* AC = P->FindComponentByClass<UActionComponent>();
	if (!AC) return false;

	FActionContext Ctx;
	Ctx.Instigator = P;
	Ctx.TargetActor = Target;

	return AC->ExecuteAction(AbilityTag, Ctx);
}

void AProdigyPlayerController::EndTurn()
{
	if (!IsMyTurn()) return;

	if (UCombatSubsystem* Combat = GetCombatSubsystem())
	{
		Combat->EndCurrentTurn(GetPawn());
	}
}

UCombatSubsystem* AProdigyPlayerController::GetCombatSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UCombatSubsystem>() : nullptr;
}

AActor* AProdigyPlayerController::GetActorUnderCursorForClick() const
{
	FHitResult Hit;

	// 1) Combat target trace first
	const bool bHitTarget = GetHitResultUnderCursor(TargetTraceChannel, false, Hit);
	if (bHitTarget && Hit.GetActor()) return Hit.GetActor();

	// 2) Fallback to base behavior (interactables/items)
	return Super::GetActorUnderCursorForClick();
}