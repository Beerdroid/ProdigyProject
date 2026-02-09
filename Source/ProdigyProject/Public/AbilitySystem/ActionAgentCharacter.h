#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"

#include "ActionAgentInterface.h"
#include "TurnResourceComponent.h"
#include "HealthComponent.h"
#include "StatusComponent.h"

#include "ActionAgentCharacter.generated.h"

UCLASS()
class PRODIGYPROJECT_API AActionAgentCharacter : public ACharacter, public IActionAgentInterface
{
	GENERATED_BODY()

public:
	AActionAgentCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Action|Components")
	TObjectPtr<UTurnResourceComponent> TurnResource;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Action|Components")
	TObjectPtr<UHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Action|Components")
	TObjectPtr<UStatusComponent> Status;

	// IActionAgentInterface
	virtual void GetOwnedGameplayTags_Implementation(FGameplayTagContainer& OutTags) const override;
	virtual bool HasActionPoints_Implementation(int32 Amount) const override;
	virtual bool SpendActionPoints_Implementation(int32 Amount) override;
	virtual bool ApplyDamage_Implementation(float Amount, AActor* InstigatorActor) override;
	virtual bool AddStatusTag_Implementation(const FGameplayTag& StatusTag, int32 Turns, float Seconds, AActor* InstigatorActor) override;
};
