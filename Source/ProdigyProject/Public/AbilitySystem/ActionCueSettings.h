#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ActionCueSettings.generated.h"

class UActionCueSet;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Action Cues"))
class PRODIGYPROJECT_API UActionCueSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	// Global fallback cues
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Cues")
	TSoftObjectPtr<UActionCueSet> GlobalCueSet;
};
