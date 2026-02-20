#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "DamageTextComponent.generated.h"

UCLASS(ClassGroup=(UI), BlueprintType, Blueprintable,  meta=(BlueprintSpawnableComponent))
class PRODIGYPROJECT_API UDamageTextComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UDamageTextComponent();

	// Called by code after spawn; you implement in the widget BP via the component
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="Floating")
	void SetDamageText(float Damage);

	// NEW: heal flow (keeps existing damage flow intact)
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="Floating")
	void SetHealText(float Heal);

};
