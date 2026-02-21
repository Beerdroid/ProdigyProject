#include "Character/ProdigyCharacterBase.h"

#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"


AProdigyCharacterBase::AProdigyCharacterBase()
{
	StimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSource"));

	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	StimuliSource->bAutoRegister = true;

	UE_LOG(LogTemp, Warning,
		TEXT("[Player] StimuliSource created and registered for Sight"));
}
