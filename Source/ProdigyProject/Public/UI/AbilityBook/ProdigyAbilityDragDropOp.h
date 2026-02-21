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
		UE_LOG(LogTemp, Warning, TEXT("[AbilityDragCancelled] Tag=%s Handled=%d"),
			*AbilityTag.ToString(), bDropHandled ? 1 : 0);
	}
};
