#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;

// ✅ add forward declares
class UBehaviorTree;
class UBlackboardComponent;

namespace ProdigyBB
{
	static const FName TargetActor = TEXT("TargetActor");
	static const FName ActionTag   = TEXT("ActionTag");   // optional
}

UCLASS()
class PRODIGYPROJECT_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

	// Called by CombatSubsystem when this pawn becomes the current turn actor
	void StartCombatTurn();


protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	AActor* ResolveSinglePlayerPawn() const;
	void ConfigureSightFromPawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI|Perception")
	TObjectPtr<UAIPerceptionComponent> Perception = nullptr;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="AI|Combat")
	TObjectPtr<UBehaviorTree> CombatBehaviorTree = nullptr;

};