// Fill out your copyright notice in the Description page of Project Settings.


#include "DialogueEdGraph.h"

#include "DialogueSystem/DialogueDataAsset.h"

void UDialogueEdGraph::NotifyGraphChanged()
{
	Super::NotifyGraphChanged();

	// Mark owner asset as modified when graph changes
	UDialogueDataAsset* DialogueAsset = Cast<UDialogueDataAsset>(GetOuter());
	if (DialogueAsset)
	{
		DialogueAsset->Modify();
	}

}
