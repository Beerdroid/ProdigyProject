#include "Player/ProdigyPlayerController.h"

#include "AbilitySystem/ActionTypes.h"
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
	// If we are already on server, call directly; otherwise route to server.
	if (HasAuthority())
	{
		Server_NotifyQuestsInventoryChanged(); // safe even on server
	}
	else
	{
		Server_NotifyQuestsInventoryChanged();
	}
}

void AProdigyPlayerController::NotifyQuestsKillTag(FGameplayTag TargetTag)
{
	if (!TargetTag.IsValid())
	{
		return;
	}

	if (HasAuthority())
	{
		Server_NotifyQuestsKillTag(TargetTag);
	}
	else
	{
		Server_NotifyQuestsKillTag(TargetTag);
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
	AActor* Target = GetHoveredActor(); // getter from base, or use ThisActor.Get() if protected
	if (!IsValid(Target))
	{
		return;
	}

	// 1) Try pickup first (keeps old inventory behavior)
	if (TryPrimaryPickup(Target))
	{
		return;
	}

	// 2) Otherwise do generic interaction
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


void AProdigyPlayerController::Server_NotifyQuestsInventoryChanged_Implementation()
{
	CacheQuestComponents();

	if (!QuestLog)
	{
		return;
	}

	// This should be a server-only function on the QuestLog that recomputes collect objectives and advances stages.
	QuestLog->NotifyInventoryChanged();
}

void AProdigyPlayerController::Server_NotifyQuestsKillTag_Implementation(FGameplayTag TargetTag)
{
	if (!TargetTag.IsValid())
	{
		return;
	}

	CacheQuestComponents();

	if (!QuestLog)
	{
		return;
	}

	// You can expose a BlueprintCallable server function instead, but this is fine if your QuestLog uses this signature.
	QuestLog->NotifyKillObjectiveTag(TargetTag);
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

FActionContext AProdigyPlayerController::BuildActionContextFromLockedTarget() const
{
	FActionContext Ctx;
	Ctx.Instigator = GetPawn();
	Ctx.TargetActor = LockedTarget;
	return Ctx;
}