#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"

#include "AbilitySystem/ActionAgentInterface.h"
#include "AbilitySystem/AttributesComponent.h"
#include "Interfaces/CombatantInterface.h"

#include "CombatantCharacterBase.generated.h"

class UActionComponent;
class UStatusComponent;
class UHealthBarWidgetComponent;

UCLASS()
class PRODIGYPROJECT_API ACombatantCharacterBase : public ACharacter, public IActionAgentInterface, public ICombatantInterface
{
	GENERATED_BODY()

public:
	ACombatantCharacterBase();

	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UActionComponent> ActionComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UStatusComponent> Status = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UAttributesComponent> Attributes = nullptr;

	UPROPERTY(Transient)
	bool bDidRagdoll = false;

	UFUNCTION(BlueprintCallable, Category="Combat|Death")
	void EnableRagdoll();

	UFUNCTION()
	void HandleDeathIfNeeded(FGameplayTag Tag, float NewValue, float Delta, AActor* InstigatorActor);

	UPROPERTY(Transient)
	TObjectPtr<UHealthBarWidgetComponent> WorldHealthBar = nullptr;

	void RemoveHealthBar();

public:
	// ---- ICombatantInterface ----
	virtual UActionComponent* GetActionComponent_Implementation() const override { return ActionComponent; }
	virtual UStatusComponent* GetStatusComponent_Implementation() const override { return Status; }

	virtual void OnCombatFreeze_Implementation(bool bFrozen) override;

	// ---- IActionAgentInterface ----
	virtual void GetOwnedGameplayTags_Implementation(FGameplayTagContainer& OutTags) const override;
	virtual bool AddStatusTag_Implementation(const FGameplayTag& StatusTag, int32 Turns, float Seconds, AActor* InstigatorActor) override;

	virtual bool HasAttribute_Implementation(FGameplayTag AttributeTag) const override;
	virtual float GetAttributeCurrentValue_Implementation(FGameplayTag AttributeTag) const override;
	virtual bool ModifyAttributeCurrentValue_Implementation(FGameplayTag AttributeTag, float Delta, AActor* InstigatorActor) override;
	virtual float GetAttributeFinalValue_Implementation(FGameplayTag AttributeTag) const override;
};
