#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatantInterface.generated.h"

class UActionComponent;
class UTurnResourceComponent;
class UStatusComponent;
class UHealthComponent;

UINTERFACE(BlueprintType)
class PRODIGYPROJECT_API UCombatantInterface : public UInterface
{
	GENERATED_BODY()
};

class PRODIGYPROJECT_API ICombatantInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Combatant")
	UActionComponent* GetActionComponent() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Combatant")
	UStatusComponent* GetStatusComponent() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Combatant")
	UHealthComponent* GetHealthComponent() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Combatant")
	bool IsAlive() const;

	// Optional hook for freeze/unfreeze (combat overlay)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Combatant")
	void OnCombatFreeze(bool bFrozen);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Combatant")
	void OnCombatStateChanged(bool bInCombat);
};
