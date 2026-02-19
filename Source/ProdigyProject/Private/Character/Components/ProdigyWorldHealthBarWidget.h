#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProdigyWorldHealthBarWidget.generated.h"

UCLASS()
class PRODIGYPROJECT_API UProdigyWorldHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintImplementableEvent, Category="WorldStatus")
	void SetHP(float Current, float Max);
};
