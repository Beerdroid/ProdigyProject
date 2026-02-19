#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/WidgetComponent.h"
#include "HealthBarWidgetComponent.generated.h"

class UAttributesComponent;

UCLASS(ClassGroup=(UI), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class PRODIGYPROJECT_API UHealthBarWidgetComponent: public UWidgetComponent
{

	GENERATED_BODY()
public:

	UHealthBarWidgetComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(Transient)
	TObjectPtr<UAttributesComponent> Attributes = nullptr;
	
	void BindAttributes();
	void UnbindAttributes();
	void ResolveAttributes();

	UFUNCTION()
	void HandleAttrChanged(FGameplayTag Tag, float NewValue, float Delta, AActor* InstigatorActor);

	void Refresh();

	UPROPERTY(EditAnywhere, Category="WorldStatus")
	FGameplayTag HealthTag;

	UPROPERTY(EditAnywhere, Category="WorldStatus")
	FGameplayTag MaxHealthTag;
};
