#pragma once

#include "CoreMinimal.h"
#include "CombatantCharacterBase.h"
#include "GameplayTagContainer.h"


#include "ProdigyCharacterBase.generated.h"

class UAIPerceptionStimuliSourceComponent;
class UTurnResourceComponent;
class UHealthComponent;
class UStatusComponent;
class UActionComponent;

UCLASS()
class PRODIGYPROJECT_API AProdigyCharacterBase : public ACombatantCharacterBase
{
	GENERATED_BODY()
public:
	
	AProdigyCharacterBase();

	// Registers this pawn as detectable by AI perception (Sight)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI|Perception")
	TObjectPtr<UAIPerceptionStimuliSourceComponent> StimuliSource;
};
