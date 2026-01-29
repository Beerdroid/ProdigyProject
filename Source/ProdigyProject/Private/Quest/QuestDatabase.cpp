#include "Quest/QuestDatabase.h"

void UQuestDatabase::PostLoad()
{
	Super::PostLoad();
	RebuildCache();
}

#if WITH_EDITOR
void UQuestDatabase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildCache();
}
#endif

void UQuestDatabase::RebuildCache() const
{
	QuestIndexById.Reset();

	for (int32 i = 0; i < Quests.Num(); ++i)
	{
		const FQuestDefinition& Q = Quests[i];
		if (Q.QuestID.IsNone())
		{
			continue;
		}

		// If duplicates exist, last one wins; you may prefer ensureMsgf + keep first.
		QuestIndexById.Add(Q.QuestID, i);
	}
}

const FQuestDefinition* UQuestDatabase::FindQuestPtr(FName QuestID) const
{
	if (QuestID.IsNone())
	{
		return nullptr;
	}

	// In case cache wasn't built (hot reload / edge cases)
	if (QuestIndexById.Num() == 0 && Quests.Num() > 0)
	{
		RebuildCache();
	}

	const int32* FoundIndex = QuestIndexById.Find(QuestID);
	if (!FoundIndex || !Quests.IsValidIndex(*FoundIndex))
	{
		return nullptr;
	}

	return &Quests[*FoundIndex];
}

bool UQuestDatabase::TryGetQuest(FName QuestID, FQuestDefinition& OutQuest) const
{
	const FQuestDefinition* Ptr = FindQuestPtr(QuestID);
	if (!Ptr)
	{
		OutQuest = FQuestDefinition(); // reset to defaults
		return false;
	}

	OutQuest = *Ptr; // copy for BP consumption
	return true;
}
