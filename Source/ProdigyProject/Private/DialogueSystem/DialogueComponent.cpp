// Fill out your copyright notice in the Description page of Project Settings.


#include "DialogueSystem/DialogueComponent.h"

#include "DialogueSystem/DialogueDataAsset.h"
#include "DialogueSystem/DialogueResolverComponent.h"
#include "DialogueSystem/DialogueSystemTypes.h"

UDialogueComponent::UDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UDialogueComponent::StartDialogue(UDialogueResolverComponent* InResolver)
{
	Resolver = InResolver;
	if (!Dialogue) return false;

	const FName StartID = !StartNodeOverride.IsNone() ? StartNodeOverride : Dialogue->StartNodeID;
	return SetNode(StartID);
}

bool UDialogueComponent::SetNode(FName NodeID)
{
	if (!Dialogue || NodeID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("SetNode FAIL: Dialogue=null or NodeID=None"));
		return false;
	}

	FDialogueNode N;
	const bool bFound = Dialogue->TryGetNode(NodeID, N);
	UE_LOG(LogTemp, Warning, TEXT("SetNode '%s' found=%d"), *NodeID.ToString(), (int32)bFound);

	if (!bFound) return false;

	CurrentNodeID = NodeID;
	OnNodeChanged.Broadcast(CurrentNodeID);
	return true;
}

bool UDialogueComponent::GetCurrentNode(FDialogueNode& OutNode) const
{
	if (!Dialogue || CurrentNodeID.IsNone()) return false;
	return Dialogue->TryGetNode(CurrentNodeID, OutNode);
}

TArray<FDialogueChoice> UDialogueComponent::GetAvailableChoices() const
{
	TArray<FDialogueChoice> Out;

	
	FDialogueNode Node;
	if (!GetCurrentNode(Node)) return Out;

	Out.Reserve(Node.Choices.Num());

	for (const FDialogueChoice& C : Node.Choices)
	{
		const bool bOk = Resolver ? Resolver->EvaluateAllConditions(C.Conditions) : (C.Conditions.Num() == 0);
		if (bOk)
		{
			Out.Add(C);
		}
	}

	return Out;
}

bool UDialogueComponent::Choose(int32 AvailableChoiceIndex)
{
	if (!Dialogue) return false;

	const TArray<FDialogueChoice> Choices = GetAvailableChoices();
	if (!Choices.IsValidIndex(AvailableChoiceIndex)) return false;

	const FDialogueChoice& Pick = Choices[AvailableChoiceIndex];

	UE_LOG(LogTemp, Warning, TEXT("Choose: idx=%d text='%s' next='%s'"),
	AvailableChoiceIndex,
	*Pick.Text.ToString(),
	*Pick.NextNodeID.ToString());

	// Apply effects
	if (Resolver)
	{
		Resolver->ApplyEffects(Pick.Effects);
	}

	// Move
	if (Pick.NextNodeID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("Choose: NextNodeID is None -> ending"));
		CurrentNodeID = NAME_None;
		OnNodeChanged.Broadcast(CurrentNodeID);
		return true;
	}

	return SetNode(Pick.NextNodeID);
}
