#include "CombatSubsystem.h"

#include "ActionComponent.h"
#include "Interfaces/CombatantInterface.h"

bool UCombatSubsystem::IsValidCombatant(AActor* A) const
{
	if (!IsValid(A)) return false;

	// Optional: require combatant interface
	if (!A->GetClass()->ImplementsInterface(UCombatantInterface::StaticClass()))
	{
		return false;
	}

	// Optional: alive gate (if you have it)
	// if (!ICombatantInterface::Execute_IsAlive(A)) return false;

	return true;
}

void UCombatSubsystem::ExitCombat()
{
}

void UCombatSubsystem::EnterCombat(const TArray<AActor*>& InParticipants, AActor* FirstToAct)
{
	if (bInCombat) return;

	bInCombat = true;
	Participants.Reset();

	for (AActor* A : InParticipants)
	{
		if (!IsValidCombatant(A)) continue;

		Participants.Add(A);

		if (UActionComponent* AC = A->FindComponentByClass<UActionComponent>())
		{
			AC->SetInCombat(true);

			// IMPORTANT: avoid duplicate binds
			AC->OnActionExecuted.RemoveDynamic(this, &UCombatSubsystem::HandleActionExecuted);
			AC->OnActionExecuted.AddDynamic(this, &UCombatSubsystem::HandleActionExecuted);
		}
	}

	// Pick who starts (usually the instigator/player who triggered combat)
	TurnIndex = Participants.IndexOfByKey(FirstToAct);
	if (TurnIndex == INDEX_NONE)
	{
		TurnIndex = 0;
	}

	// Begin the current actor's turn (NOT AdvanceTurn, which usually increments)
	BeginTurnForIndex(TurnIndex);
}

void UCombatSubsystem::BeginTurnForIndex(int32 Index)
{
	if (!Participants.IsValidIndex(Index)) return;

	AActor* A = Participants[Index].Get();
	if (!IsValid(A)) return;

	if (UActionComponent* AC = A->FindComponentByClass<UActionComponent>())
	{
		AC->OnTurnBegan();
	}
}

AActor* UCombatSubsystem::GetCurrentTurnActor() const
{
	return Participants.IsValidIndex(TurnIndex) ? Participants[TurnIndex].Get() : nullptr;
}

void UCombatSubsystem::AdvanceTurn()
{
	if (Participants.Num() == 0) return;

	TurnIndex = (TurnIndex + 1) % Participants.Num();
	BeginTurnForIndex(TurnIndex);
}

void UCombatSubsystem::HandleActionExecuted(FGameplayTag ActionTag, const FActionContext& Context)
{
	if (!bInCombat) return;

	AActor* Current = GetCurrentTurnActor();
	if (!IsValid(Current)) return;

	// Only the current turn actor can advance the turn
	if (Context.Instigator != Current)
	{
		return;
	}

	// Move to next actor, then begin their turn
	AdvanceTurn();
}