#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"

#include "AbilitySystem/ActionAgentInterface.h"
#include "Interfaces/CombatantInterface.h"

#include "CombatantCharacterBase.generated.h"

class UActionComponent;
class UTurnResourceComponent;
class UHealthComponent;
class UStatusComponent;

UCLASS()
class PRODIGYPROJECT_API ACombatantCharacterBase : public ACharacter, public IActionAgentInterface, public ICombatantInterface
{
	GENERATED_BODY()

public:
	ACombatantCharacterBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UActionComponent> ActionComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UHealthComponent> Health = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UStatusComponent> Status = nullptr;

public:
	// ---- ICombatantInterface ----
	virtual UActionComponent* GetActionComponent_Implementation() const override { return ActionComponent; }
	virtual UStatusComponent* GetStatusComponent_Implementation() const override { return Status; }
	virtual UHealthComponent* GetHealthComponent_Implementation() const override { return Health; }
	virtual bool IsAlive_Implementation() const override;

	virtual void OnCombatFreeze_Implementation(bool bFrozen) override;

	// ---- IActionAgentInterface ----
	virtual void GetOwnedGameplayTags_Implementation(FGameplayTagContainer& OutTags) const override;
	virtual bool AddStatusTag_Implementation(const FGameplayTag& StatusTag, int32 Turns, float Seconds, AActor* InstigatorActor) override;
};
