#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/Manifest/Inv_ItemManifest.h"   // adjust include path to your plugin
#include "ProdigyItemDatabase.generated.h"

USTRUCT(BlueprintType)
struct FProdigyItemRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FInv_ItemManifest Manifest;
};

UCLASS(BlueprintType)
class PRODIGYPROJECT_API UProdigyItemDatabase : public UDataAsset
{
	GENERATED_BODY()

public:
	// Authoring-friendly list
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	TArray<FProdigyItemRecord> Items;

	// C++-friendly lookup
	bool FindManifest(FName ItemID, FInv_ItemManifest& OutManifest) const;

	// Optional: call once at startup (or lazily) for O(1) lookup.
	void BuildCacheIfNeeded() const;

private:
	// Cache is runtime-only. Mutable so we can build it from const functions.
	UPROPERTY(Transient)
	mutable bool bCacheBuilt = false;

	UPROPERTY(Transient)
	mutable TMap<FName, int32> IndexByID;
};
