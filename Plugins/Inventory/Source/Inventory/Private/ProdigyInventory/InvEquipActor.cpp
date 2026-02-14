#include "ProdigyInventory/InvEquipActor.h"

AInvEquipActor::AInvEquipActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // single player
}
