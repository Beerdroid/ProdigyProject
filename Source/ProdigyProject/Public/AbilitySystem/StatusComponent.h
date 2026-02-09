#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "StatusComponent.generated.h"

USTRUCT(BlueprintType)
struct FStatusEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TurnsRemaining = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SecondsRemaining = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Stacks = 1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStatusChanged, FGameplayTag, StatusTag, bool, bAdded);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PRODIGYPROJECT_API UStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStatusComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
	}

	UPROPERTY(BlueprintAssignable)
	FOnStatusChanged OnStatusChanged;

	// Active statuses
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status")
	TArray<FStatusEntry> Statuses;

	// Quick view for gating
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Status")
	FGameplayTagContainer OwnedTags;

	UFUNCTION(BlueprintCallable, Category="Status")
	void GetOwnedTags(FGameplayTagContainer& Out) const
	{
		Out = OwnedTags;
	}

	UFUNCTION(BlueprintCallable, Category="Status")
	bool HasTag(FGameplayTag Tag) const
	{
		return Tag.IsValid() && OwnedTags.HasTag(Tag);
	}

	// Add/refresh status. If the status already exists, we refresh durations to max.
	UFUNCTION(BlueprintCallable, Category="Status")
	bool AddStatusTag(FGameplayTag Tag, int32 Turns, float Seconds)
	{
		if (!Tag.IsValid()) return false;

		for (FStatusEntry& E : Statuses)
		{
			if (E.Tag == Tag)
			{
				E.TurnsRemaining = FMath::Max(E.TurnsRemaining, Turns);
				E.SecondsRemaining = FMath::Max(E.SecondsRemaining, Seconds);
				return true;
			}
		}

		FStatusEntry NewE;
		NewE.Tag = Tag;
		NewE.TurnsRemaining = FMath::Max(0, Turns);
		NewE.SecondsRemaining = FMath::Max(0.f, Seconds);

		Statuses.Add(NewE);
		OwnedTags.AddTag(Tag);
		OnStatusChanged.Broadcast(Tag, true);
		return true;
	}

	UFUNCTION(BlueprintCallable, Category="Status")
	bool RemoveStatusTag(FGameplayTag Tag)
	{
		if (!Tag.IsValid()) return false;

		const int32 Removed = Statuses.RemoveAll([&](const FStatusEntry& E){ return E.Tag == Tag; });
		if (Removed > 0)
		{
			OwnedTags.RemoveTag(Tag);
			OnStatusChanged.Broadcast(Tag, false);
			return true;
		}
		return false;
	}

	// Call this from your turn manager at BeginTurn(owner)
	UFUNCTION(BlueprintCallable, Category="Status")
	void TickStartOfTurn()
	{
		bool bChanged = false;

		for (int32 i = Statuses.Num() - 1; i >= 0; --i)
		{
			FStatusEntry& E = Statuses[i];
			if (E.TurnsRemaining > 0) E.TurnsRemaining--;

			if (E.TurnsRemaining <= 0 && E.SecondsRemaining <= 0.f)
			{
				const FGameplayTag Tag = E.Tag;
				Statuses.RemoveAtSwap(i);
				OwnedTags.RemoveTag(Tag);
				OnStatusChanged.Broadcast(Tag, false);
				bChanged = true;
			}
		}

		(void)bChanged;
	}

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		// Exploration-style seconds ticking (only matters when you’re not in combat)
		for (int32 i = Statuses.Num() - 1; i >= 0; --i)
		{
			FStatusEntry& E = Statuses[i];
			if (E.SecondsRemaining > 0.f)
			{
				E.SecondsRemaining = FMath::Max(0.f, E.SecondsRemaining - DeltaTime);
			}

			if (E.TurnsRemaining <= 0 && E.SecondsRemaining <= 0.f)
			{
				const FGameplayTag Tag = E.Tag;
				Statuses.RemoveAtSwap(i);
				OwnedTags.RemoveTag(Tag);
				OnStatusChanged.Broadcast(Tag, false);
			}
		}
	}
};
