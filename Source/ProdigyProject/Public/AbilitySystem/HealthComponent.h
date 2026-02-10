#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HealthComponent.generated.h"

class UAttributesComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChanged, float, NewHealth, float, Delta, AActor*, InstigatorActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDied, AActor*, Killer);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PRODIGYPROJECT_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// These are now defaults used to initialize Attr.MaxHealth / Attr.Health if missing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health")
	float MaxHealth = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
	float CurrentHealth = 100.f;

	UPROPERTY(BlueprintAssignable)
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FOnDied OnDied;

	// Set current health (writes Attr.Health). Delta follows New-Old convention.
	UFUNCTION(BlueprintCallable, Category="Health")
	void SetHealth(float NewHealth, AActor* InstigatorActor = nullptr);

	// Damage is applied by modifying Attr.Health negatively. Returns true if applied.
	UFUNCTION(BlueprintCallable, Category="Health")
	bool ApplyDamage(float Amount, AActor* InstigatorActor);

	// Optional convenience
	UFUNCTION(BlueprintCallable, Category="Health")
	float GetHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintCallable, Category="Health")
	float GetMaxHealth() const { return MaxHealth; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAttributesComponent> Attributes = nullptr;

	bool bBoundToAttributes = false;
	bool bHasDied = false;

	void EnsureAttributes();

	UFUNCTION()
	void HandleAttributeChanged(FGameplayTag AttributeTag, float NewValue, float Delta, AActor* InstigatorActor);

	void SyncFromAttributes(); // copies Attr.* into CurrentHealth/MaxHealth
};
