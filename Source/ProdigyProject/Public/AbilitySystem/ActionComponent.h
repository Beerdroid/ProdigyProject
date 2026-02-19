#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ActionTypes.h"
#include "ActionDefinition.h"
#include "ActionComponent.generated.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionExec, Log, All);

#define ACTION_LOG(Verbosity, Fmt, ...) \
UE_LOG(LogActionExec, Verbosity, TEXT("[ActionComp:%s] " Fmt), \
*GetNameSafe(GetOwner()), ##__VA_ARGS__)

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActionExecuted, FGameplayTag, ActionTag, const FActionContext&, Context);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PRODIGYPROJECT_API UActionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActionComponent();
	virtual void OnRegister() override;

	// Registry: tag -> definition
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Action")
	TArray<TObjectPtr<UActionDefinition>> KnownActions;

	// Simple “time model” switch (Combat subsystem will set this)
	UFUNCTION(BlueprintCallable, Category="Action")
	void SetInCombat(bool bNowInCombat);

	UFUNCTION(BlueprintCallable, Category="Action")
	bool IsInCombat() const { return bInCombat; }

	UFUNCTION(BlueprintCallable,Category="Action")
	FActionQueryResult QueryAction(FGameplayTag ActionTag, const FActionContext& Context) const;

	UFUNCTION(Category="Action")
	bool ExecuteAction(FGameplayTag ActionTag, const FActionContext& Context);

	UPROPERTY(BlueprintAssignable)
	FOnActionExecuted OnActionExecuted;

	UFUNCTION(BlueprintCallable, Category="Actions|Combat")
	void OnTurnBegan();

	void OnTurnEnded();

	UFUNCTION(BlueprintCallable, Category="Action|UI")
	bool TryGetActionDefinition(FGameplayTag ActionTag, UActionDefinition*& OutDef) const;

	UFUNCTION(BlueprintCallable, Category="Action|UI")
	TArray<FGameplayTag> GetKnownActionTags() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY() bool bInCombat = false;

	UPROPERTY() TMap<FGameplayTag, TObjectPtr<UActionDefinition>> ActionMap;
	UPROPERTY() TMap<FGameplayTag, FActionCooldownState> Cooldowns;

	const UActionDefinition* FindDef(FGameplayTag Tag) const;
	void BuildMapIfNeeded();

	bool PassesTagGates(AActor* Instigator, const UActionDefinition* Def, EActionFailReason& OutFail) const;
	bool IsTargetValid(const UActionDefinition* Def, const FActionContext& Context) const;

	void StartCooldown(const UActionDefinition* Def);
};

