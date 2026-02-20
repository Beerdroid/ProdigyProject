#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "GameplayTagContainer.h"
#include "ProdigyAbilityDragDropOp.generated.h"

UCLASS()
class PRODIGYPROJECT_API UProdigyAbilityDragDropOp : public UDragDropOperation
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly)
	FGameplayTag AbilityTag;

	UPROPERTY(BlueprintReadOnly)
	bool bDropHandled = false;

protected:
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override
	{
		Super::DragCancelled_Implementation(PointerEvent);
		// No world-drop behavior for abilities.
	}
};
