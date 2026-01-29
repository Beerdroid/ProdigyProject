// Fill out your copyright notice in the Description page of Project Settings.


#include "DialogueSystem/DialogueDataAsset.h"

bool UDialogueDataAsset::TryGetNode(FName NodeID, FDialogueNode& OutNode) const
{
	if (NodeID.IsNone()) return false;

	for (const FDialogueNode& N : Nodes)
	{
		if (N.NodeID == NodeID)
		{
			OutNode = N;
			return true;
		}
	}
	return false;
}
