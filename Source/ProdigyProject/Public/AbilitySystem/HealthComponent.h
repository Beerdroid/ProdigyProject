#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChanged, float, NewHealth, float, Delta, AActor*, InstigatorActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDied, AActor*, Killer);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PRODIGYPROJECT_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health")
	float MaxHealth = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
	float CurrentHealth = 100.f;

	UPROPERTY(BlueprintAssignable)
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FOnDied OnDied;

	UFUNCTION(BlueprintCallable, Category="Health")
	void SetHealth(float NewHealth)
	{
		const float Clamped = FMath::Clamp(NewHealth, 0.f, MaxHealth);
		const float Delta = Clamped - CurrentHealth;
		CurrentHealth = Clamped;
		OnHealthChanged.Broadcast(CurrentHealth, Delta, nullptr);
		if (CurrentHealth <= 0.f)
		{
			OnDied.Broadcast(nullptr);
		}
	}

	UFUNCTION(BlueprintCallable, Category="Health")
	bool ApplyDamage(float Amount, AActor* InstigatorActor)
	{
		if (Amount <= 0.f || CurrentHealth <= 0.f) return false;

		const float Old = CurrentHealth;
		CurrentHealth = FMath::Clamp(CurrentHealth - Amount, 0.f, MaxHealth);

		OnHealthChanged.Broadcast(CurrentHealth, CurrentHealth - Old, InstigatorActor);

		if (CurrentHealth <= 0.f)
		{
			OnDied.Broadcast(InstigatorActor);
		}
		return true;
	}
};
