#include "Quest/QuestTypes.h"

#include "Quest/QuestLogComponent.h"

void FQuestRuntimeArray::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	if (Owner)
	{
		Owner->HandleQuestStatesReplicated();
	}
}

void FQuestRuntimeArray::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	if (Owner)
	{
		Owner->HandleQuestStatesReplicated();
	}
}

void FQuestRuntimeArray::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	if (Owner)
	{
		Owner->HandleQuestStatesReplicated();
	}
}