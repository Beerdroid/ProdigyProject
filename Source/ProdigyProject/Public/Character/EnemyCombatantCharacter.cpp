#include "Character/EnemyCombatantCharacter.h"

#include "AIController.h"

AEnemyCombatantCharacter::AEnemyCombatantCharacter()
{
	// Ensure enemies get an AI controller automatically
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Set in BP child or here if you want hard binding:
	// AIControllerClass = AEnemyAIController::StaticClass();
}

void AEnemyCombatantCharacter::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning,
		TEXT("[Enemy] BeginPlay Pawn=%s Controller=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetController()));
}
