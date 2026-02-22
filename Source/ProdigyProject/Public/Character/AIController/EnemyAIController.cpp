#include "EnemyAIController.h"

#include "Character/EnemyCombatantCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "AbilitySystem/CombatSubsystem.h"
#include "AbilitySystem/WorldCombatEvents.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

AEnemyAIController::AEnemyAIController()
{
	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SetPerceptionComponent(*Perception);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	SightConfig->SightRadius = 900.f;
	SightConfig->LoseSightRadius = 950.f;
	SightConfig->PeripheralVisionAngleDegrees = 75.f;
	SightConfig->SetMaxAge(1.0f);
	SightConfig->AutoSuccessRangeFromLastSeenLocation = 200.f;

	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	Perception->ConfigureSense(*SightConfig);
	Perception->SetDominantSense(SightConfig->GetSenseImplementation());
	Perception->OnTargetPerceptionUpdated.AddDynamic(this, &AEnemyAIController::HandleTargetPerceptionUpdated);
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] OnPossess Controller=%s Pawn=%s"),
		*GetNameSafe(this), *GetNameSafe(InPawn));

	ConfigureSightFromPawn();

	// ✅ Initialize Blackboard ONCE if BT is assigned
	if (CombatBehaviorTree && CombatBehaviorTree->BlackboardAsset)
	{
		UBlackboardComponent* BlackboardComp = nullptr;
		const bool bOk = UseBlackboard(CombatBehaviorTree->BlackboardAsset, BlackboardComp);
		Blackboard = bOk ? BlackboardComp : nullptr;

		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] UseBlackboard %s BB=%s"),
			bOk ? TEXT("OK") : TEXT("FAIL"), *GetNameSafe(Blackboard));
	}

	if (Perception)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] Perception component valid. SightRadius=%.1f"),
			SightConfig ? SightConfig->SightRadius : -1.f);

		Perception->RequestStimuliListenerUpdate();
	}
}
void AEnemyAIController::OnUnPossess()
{
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->StopLogic(TEXT("UnPossess"));
	}

	Super::OnUnPossess();
}

void AEnemyAIController::ConfigureSightFromPawn()
{
	AEnemyCombatantCharacter* Enemy = Cast<AEnemyCombatantCharacter>(GetPawn());
	if (!Enemy || !SightConfig || !Perception) return;

	const float R = FMath::Max(0.f, Enemy->AggroRadius);

	SightConfig->SightRadius = R;
	SightConfig->LoseSightRadius = R + 100.f;

	// Re-apply config so runtime changes take effect
	Perception->RequestStimuliListenerUpdate();
}

void AEnemyAIController::StartCombatTurn()
{
	if (!CombatBehaviorTree)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[EnemyAI] StartCombatTurn: CombatBehaviorTree is NULL on %s"),
			*GetNameSafe(this));
		return;
	}

	// Ensure Blackboard is initialized once
	if (!GetBlackboardComponent() && CombatBehaviorTree->BlackboardAsset)
	{
		UBlackboardComponent* BBInit = nullptr;
		const bool bOk = UseBlackboard(CombatBehaviorTree->BlackboardAsset, BBInit);
		if (!bOk || !BBInit)
		{
			UE_LOG(LogTemp, Error,
				TEXT("[EnemyAI] StartCombatTurn: UseBlackboard failed Controller=%s"),
				*GetNameSafe(this));
			return;
		}
	}

	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[EnemyAI] StartCombatTurn: BlackboardComponent is NULL after init"));
		return;
	}

	// ---- Write turn context ----
	APawn* SelfPawn = GetPawn();


	AActor* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	BB->SetValueAsObject(TEXT("TargetActor"), PlayerPawn);

	BB->SetValueAsName(TEXT("ActionTagToUse"), FName("Action.Attack.Basic"));
	BB->SetValueAsBool(TEXT("bTurnActive"), true);

	// ---- Start / restart BT ----
	const bool bStarted = RunBehaviorTree(CombatBehaviorTree);

	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->RestartLogic();
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[EnemyAI] StartCombatTurn: BT start=%d Controller=%s Pawn=%s"),
		bStarted ? 1 : 0,
		*GetNameSafe(this),
		*GetNameSafe(SelfPawn));
}

AActor* AEnemyAIController::ResolveSinglePlayerPawn() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	// Best practice for “single player pawn”: player 0 pawn
	return UGameplayStatics::GetPlayerPawn(World, 0);
}

void AEnemyAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
		UE_LOG(LogTemp, Warning,
		TEXT("[EnemyAI] PerceptionUpdated Pawn=%s Actor=%s Sensed=%d"),
		*GetNameSafe(GetPawn()),
		*GetNameSafe(Actor),
		Stimulus.WasSuccessfullySensed() ? 1 : 0);
	

	if (!Stimulus.WasSuccessfullySensed())
	{
		return;
	}

	AEnemyCombatantCharacter* Enemy = Cast<AEnemyCombatantCharacter>(GetPawn());
	if (!IsValid(Enemy))
	{
		UE_LOG(LogTemp, Error, TEXT("[EnemyAI] PerceptionUpdated: Pawn is not AEnemyCombatantCharacter"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] Enemy=%s bAggressive=%d"),
		*GetNameSafe(Enemy),
		Enemy->bAggressive ? 1 : 0);

	if (!Enemy->bAggressive)
	{
		return;
	}

	AActor* PlayerPawn = ResolveSinglePlayerPawn();
	UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] PlayerPawn resolved as %s"), *GetNameSafe(PlayerPawn));

	if (!IsValid(PlayerPawn))
	{
		UE_LOG(LogTemp, Error, TEXT("[EnemyAI] ResolveSinglePlayerPawn returned NULL"));
		return;
	}

	// Only aggro the player pawn
	if (Actor != PlayerPawn)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[EnemyAI] Ignored perceived actor (not player)"));
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("[EnemyAI] GetGameInstance is NULL"));
		return;
	}

	UCombatSubsystem* Combat = GI->GetSubsystem<UCombatSubsystem>();
	UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] CombatSubsystem=%s InCombat=%d"),
		*GetNameSafe(Combat),
		Combat && Combat->IsInCombat() ? 1 : 0);

	if (!Combat)
	{
		return;
	}

	if (Combat->IsInCombat())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] Combat already active -> ignore"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[EnemyAI] GetWorld is NULL"));
		return;
	}

	UWorldCombatEvents* Events = World->GetSubsystem<UWorldCombatEvents>();
	UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] WorldCombatEvents=%s"), *GetNameSafe(Events));

	if (!Events)
	{
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] Aggro -> Broadcast OnCombatStartRequested Enemy=%s Player=%s"),
		*GetNameSafe(Enemy),
		*GetNameSafe(PlayerPawn));

	// Player first for now (your current rule)
	Events->OnCombatStartRequested.Broadcast(Enemy, PlayerPawn, PlayerPawn);
}