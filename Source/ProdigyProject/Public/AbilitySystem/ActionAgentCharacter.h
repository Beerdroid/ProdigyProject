#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"

#include "ActionAgentInterface.h"
#include "HealthComponent.h"
#include "StatusComponent.h"
#include "AbilitySystem/AttributesComponent.h"

#include "ActionAgentCharacter.generated.h"

UCLASS()
class PRODIGYPROJECT_API AActionAgentCharacter : public ACharacter, public IActionAgentInterface
{
	GENERATED_BODY()

public:
	AActionAgentCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Action|Components")
	TObjectPtr<UHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Action|Components")
	TObjectPtr<UStatusComponent> Status;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Action|Components")
	TObjectPtr<UAttributesComponent> Attributes;

	// IActionAgentInterface
	virtual void GetOwnedGameplayTags_Implementation(FGameplayTagContainer& OutTags) const override;

	// Generic attributes (new)
	virtual bool HasAttribute_Implementation(FGameplayTag AttributeTag) const override;
	virtual float GetAttributeCurrentValue_Implementation(FGameplayTag AttributeTag) const override;
	virtual bool ModifyAttributeCurrentValue_Implementation(FGameplayTag AttributeTag, float Delta, AActor* InstigatorActor) override;

	// Existing hooks
	virtual bool AddStatusTag_Implementation(const FGameplayTag& StatusTag, int32 Turns, float Seconds, AActor* InstigatorActor) override;
};
