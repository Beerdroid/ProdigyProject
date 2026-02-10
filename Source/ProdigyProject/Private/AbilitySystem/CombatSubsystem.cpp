
#include "AbilitySystem/CombatSubsystem.h"

#include "AbilitySystem/ActionAgentInterface.h"
#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/AttributesComponent.h"
#include "AbilitySystem/ProdigyAbilityUtils.h"
#include "AbilitySystem/ProdigyGameplayTags.h"

bool UCombatSubsystem::IsValidCombatant(AActor* A) const
{
	if (!IsValid(A)) return false;

	if (!A->FindComponentByClass<UActionComponent>())
	{
		return false;
	}

	if (ProdigyAbilityUtils::IsDeadByAttributes(A)) return false;

	if (!A->GetClass()->ImplementsInterface(UActionAgentInterface::StaticClass()))
	{
		UE_LOG(LogActionExec, Error, TEXT("[Combat] Invalid combatant %s: missing ActionAgentInterface"), *GetNameSafe(A));
		return false;
	}

	const bool bHasHealth =
		IActionAgentInterface::Execute_HasAttribute(A, ProdigyTags::Attr::Health) &&
		IActionAgentInterface::Execute_HasAttribute(A, ProdigyTags::Attr::MaxHealth);

	const bool bHasAP =
		IActionAgentInterface::Execute_HasAttribute(A, ProdigyTags::Attr::AP) &&
		IActionAgentInterface::Execute_HasAttribute(A, ProdigyTags::Attr::MaxAP);

	if (!bHasHealth || !bHasAP)
	{
		UE_LOG(LogActionExec, Error,
			TEXT("[Combat] Invalid combatant %s: missing required Attr tags (need Health/MaxHealth/AP/MaxAP in DefaultAttributes)"),
			*GetNameSafe(A)
		);
		return false;
	}

	return true;
}


void UCombatSubsystem::ExitCombat()
{
	if (!bInCombat) return;

	// Unbind + reset combat flags
	for (TWeakObjectPtr<AActor>& W : Participants)
	{
		AActor* A = W.Get();
		if (!IsValid(A)) continue;

		if (UActionComponent* AC = A->FindComponentByClass<UActionComponent>())
		{
			AC->SetInCombat(false);
			AC->OnActionExecuted.RemoveDynamic(this, &UCombatSubsystem::HandleActionExecuted);
		}
	}

	Participants.Reset();
	TurnIndex = 0;
	bAdvancingTurn = false;
	bInCombat = false;
}

void UCombatSubsystem::EnterCombat(const TArray<AActor*>& InParticipants, AActor* FirstToAct)
{
	// If we're already in combat, ignore
	if (bInCombat) return;

	// Build a filtered list first (do NOT flip bInCombat yet)
	TArray<TWeakObjectPtr<AActor>> NewParticipants;
	NewParticipants.Reserve(InParticipants.Num());

	for (AActor* A : InParticipants)
	{
		if (!IsValidCombatant(A)) continue;
		NewParticipants.Add(A);
	}

	// ✅ No combat if we don't have at least 2 combatants
	if (NewParticipants.Num() < 2)
	{
		UE_LOG(LogActionExec, Warning,
			TEXT("[Combat] EnterCombat ABORTED: ValidParticipants=%d (need >= 2). FirstToAct=%s"),
			NewParticipants.Num(), *GetNameSafe(FirstToAct));
		return;
	}

	// Now we can enter combat
	bInCombat = true;
	bAdvancingTurn = false;

	Participants = MoveTemp(NewParticipants);

	for (TWeakObjectPtr<AActor>& W : Participants)
	{
		AActor* A = W.Get();
		if (!IsValid(A)) continue;

		if (UActionComponent* AC = A->FindComponentByClass<UActionComponent>())
		{
			AC->SetInCombat(true);

			AC->OnActionExecuted.RemoveDynamic(this, &UCombatSubsystem::HandleActionExecuted);
			AC->OnActionExecuted.AddDynamic(this, &UCombatSubsystem::HandleActionExecuted);
		}
	}

	TurnIndex = Participants.IndexOfByKey(FirstToAct);
	if (TurnIndex == INDEX_NONE)
	{
		TurnIndex = 0;
	}

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] EnterCombat: Participants=%d FirstToAct=%s TurnIndex=%d"),
		Participants.Num(), *GetNameSafe(FirstToAct), TurnIndex);

	for (int32 i = 0; i < Participants.Num(); ++i)
	{
		AActor* P = Participants[i].Get();
		UE_LOG(LogActionExec, Warning, TEXT("[Combat]  P[%d]=%s Valid=%d HasActionComp=%d"),
			i,
			*GetNameSafe(P),
			IsValid(P),
			IsValid(P) && P->FindComponentByClass<UActionComponent>() != nullptr);
	}

	BeginTurnForIndex(TurnIndex);
}

void UCombatSubsystem::BeginTurnForIndex(int32 Index)
{
	PruneParticipants();
	if (!bInCombat) return;
	
	if (Participants.Num() == 0) return;
	if (!Participants.IsValidIndex(Index)) return;

	AActor* TurnActor = Participants[Index].Get();
	if (!IsValid(TurnActor)) return;

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] BeginTurn: Index=%d Actor=%s"),
		Index, *GetNameSafe(TurnActor));

	// ✅ only begin-turn work now (AP refresh etc.)
	if (UActionComponent* AC = TurnActor->FindComponentByClass<UActionComponent>())
	{
		AC->OnTurnBegan();
	}

	// Player waits
	APawn* PawnOwner = Cast<APawn>(TurnActor);
	const bool bIsPlayer = PawnOwner && PawnOwner->IsPlayerControlled();
	if (bIsPlayer)
	{
		UE_LOG(LogActionExec, Warning, TEXT("[Combat] Player turn: waiting for input"));
		return;
	}

	// ---- AI TURN ----

	// Choose target (simple 2-participant case)
	AActor* Target = nullptr;
	for (const TWeakObjectPtr<AActor>& W : Participants)
	{
		AActor* A = W.Get();
		if (IsValid(A) && A != TurnActor && !ProdigyAbilityUtils::IsDeadByAttributes(A))
		{
			Target = A;
			break;
		}
	}
	if (!IsValid(Target))
	{
		UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI turn: no valid target -> passing"));
		AdvanceTurn();
		return;
	}

	UActionComponent* AC = TurnActor->FindComponentByClass<UActionComponent>();
	if (!IsValid(AC))
	{
		UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI turn: no ActionComponent -> passing"));
		AdvanceTurn();
		return;
	}

	// Build context
	FActionContext Ctx;
	Ctx.Instigator = TurnActor;
	Ctx.TargetActor = Target;

	// Execute on next tick to avoid re-entrancy during BeginTurn / delegate callbacks
	if (UWorld* World = GetWorld())
	{
		const TWeakObjectPtr<UCombatSubsystem> SelfWeak(this);
		const TWeakObjectPtr<UActionComponent> ACWeak(AC);
		const TWeakObjectPtr<AActor> TurnWeak(TurnActor);
		const TWeakObjectPtr<AActor> TargetWeak(Target);

		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([SelfWeak, ACWeak, TurnWeak, TargetWeak, Ctx]()
		{
			if (!SelfWeak.IsValid()) return;
			if (!ACWeak.IsValid() || !TurnWeak.IsValid() || !TargetWeak.IsValid())
			{
				SelfWeak->AdvanceTurn();
				return;
			}

			// Still must be the current actor when we act
			if (SelfWeak->GetCurrentTurnActor() != TurnWeak.Get())
			{
				return;
			}

			const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(FName("Action.Attack.Basic"));
			const bool bOk = ACWeak->ExecuteAction(AttackTag, Ctx);

			// If blocked (cooldown / AP / invalid), PASS TURN so combat never stalls.
			if (!bOk)
			{
				UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI action failed -> passing turn (%s)"),
					*GetNameSafe(TurnWeak.Get()));
				SelfWeak->AdvanceTurn();
			}
			// If succeeded, HandleActionExecuted will AdvanceTurn() normally.
		}));
	}
	else
	{
		// Fallback: immediate execute, still pass if it fails
		const bool bOk = AC->ExecuteAction(FGameplayTag::RequestGameplayTag(FName("Action.Attack.Basic")), Ctx);
		if (!bOk)
		{
			UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI action failed (no world) -> passing turn"));
			AdvanceTurn();
		}
	}
}

AActor* UCombatSubsystem::GetCurrentTurnActor() const
{
	return Participants.IsValidIndex(TurnIndex) ? Participants[TurnIndex].Get() : nullptr;
}

void UCombatSubsystem::AdvanceTurn()
{
	if (!bInCombat) return;

	PruneParticipants();

	if (!bInCombat) return;
	if (Participants.Num() == 0) return;

	if (bAdvancingTurn)
	{
		UE_LOG(LogActionExec, Verbose, TEXT("[Combat] AdvanceTurn ignored (already advancing)"));
		return;
	}

	bAdvancingTurn = true;

	// ✅ capture current BEFORE changing index
	AActor* CurrentActor = GetCurrentTurnActor();

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] AdvanceTurn: BEFORE TurnIndex=%d Current=%s Participants=%d"),
		TurnIndex, *GetNameSafe(CurrentActor), Participants.Num());

	// now pick next
	TurnIndex = (TurnIndex + 1) % Participants.Num();

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] AdvanceTurn: AFTER  TurnIndex=%d Next=%s"),
		TurnIndex, *GetNameSafe(GetCurrentTurnActor()));

	// begin next on next tick
	if (UWorld* World = GetWorld())
	{
		const int32 IndexToBegin = TurnIndex;
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UCombatSubsystem::BeginTurnForIndex, IndexToBegin)
		);
	}
	else
	{
		BeginTurnForIndex(TurnIndex);
	}

	bAdvancingTurn = false;
}

void UCombatSubsystem::HandleActionExecuted(FGameplayTag ActionTag, const FActionContext& Context)
{
	if (!bInCombat) return;

	AActor* Current = GetCurrentTurnActor();
	if (!IsValid(Current)) return;

	if (Context.Instigator != Current)
	{
		UE_LOG(LogActionExec, Verbose,
			TEXT("[Combat] Ignoring ActionExecuted (not current turn): Inst=%s Current=%s"),
			*GetNameSafe(Context.Instigator), *GetNameSafe(Current));
		return;
	}

	AdvanceTurn(); 
}

bool UCombatSubsystem::EndCurrentTurn(AActor* Instigator)
{
	if (!bInCombat) return false;
	if (!IsValid(Instigator)) return false;

	AActor* Current = GetCurrentTurnActor();
	if (Current != Instigator) return false;

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] EndCurrentTurn: %s"), *GetNameSafe(Instigator));

	AdvanceTurn();
	
	return true;
}

void UCombatSubsystem::PruneParticipants()
{
	Participants.RemoveAll([](const TWeakObjectPtr<AActor>& W)
	{
		AActor* A = W.Get();
		return !IsValid(A) || A->IsActorBeingDestroyed() || ProdigyAbilityUtils::IsDeadByAttributes(A);
	});

	// ✅ End combat when there is nobody to fight (0) OR only one survivor (1)
	if (Participants.Num() <= 1)
	{
		ExitCombat();
		return;
	}

	// Keep index in range
	TurnIndex = TurnIndex % Participants.Num();
}

