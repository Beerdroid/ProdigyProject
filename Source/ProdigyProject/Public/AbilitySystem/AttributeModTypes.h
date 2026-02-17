#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AttributeModTypes.generated.h"

UENUM(BlueprintType)
enum class EAttrModOp : uint8
{
	Add      UMETA(DisplayName="Add"),
	Multiply UMETA(DisplayName="Multiply"),
	Override UMETA(DisplayName="Override")
};

USTRUCT(BlueprintType)
struct FAttributeMod
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	FGameplayTag AttributeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	EAttrModOp Op = EAttrModOp::Add;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	float Magnitude = 0.f;
};
