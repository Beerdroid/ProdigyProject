#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ProdigyItemDatabase.generated.h"

USTRUCT(BlueprintType)
struct FProdigyItemRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemID = NAME_None;
};

UCLASS(BlueprintType)
class PRODIGYPROJECT_API UProdigyItemDatabase : public UDataAsset
{
	GENERATED_BODY()

public:
	// Authoring-friendly list
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	TArray<FProdigyItemRecord> Items;

	// Optional: call once at startup (or lazily) for O(1) lookup.
	void BuildCacheIfNeeded() const;

private:
	// Cache is runtime-only. Mutable so we can build it from const functions.
	UPROPERTY(Transient)
	mutable bool bCacheBuilt = false;

	UPROPERTY(Transient)
	mutable TMap<FName, int32> IndexByID;
};
