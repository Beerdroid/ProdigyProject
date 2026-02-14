#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ActionTypes.h"
#include "ActionEffect.generated.h"

UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class PRODIGYPROJECT_API UActionEffect : public UObject
{
	GENERATED_BODY()

public:
	// Return true if effect applied successfully (useful for analytics/logging)
	UFUNCTION(BlueprintNativeEvent, Category="Action|Effect")
	bool Apply(const FActionContext& Context) const;
};
