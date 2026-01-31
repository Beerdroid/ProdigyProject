// Fill out your copyright notice in the Description page of Project Settings.


#include "DialogueSystem/DialogueFactsComponent.h"

#include "GameplayTagContainer.h"

UDialogueFactsComponent::UDialogueFactsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false); // SP now; can replicate later if moved to PlayerState
}

bool UDialogueFactsComponent::HasTag(FGameplayTag Tag) const
{
	return Facts.Contains(Tag);
}

int32 UDialogueFactsComponent::GetTagValue(FGameplayTag Tag) const
{
	if (const int32* V = Facts.Find(Tag))
	{
		return *V;
	}
	return 0;
}

void UDialogueFactsComponent::AddTag(FGameplayTag Tag)
{
	if (!Tag.IsValid()) return;
	if (!Facts.Contains(Tag))
	{
		Facts.Add(Tag, 1);
		OnFactChanged.Broadcast(Tag, 1);
	}
}

void UDialogueFactsComponent::RemoveTag(FGameplayTag Tag)
{
	if (!Tag.IsValid()) return;
	if (Facts.Remove(Tag) > 0)
	{
		OnFactChanged.Broadcast(Tag, 0);
	}
}

void UDialogueFactsComponent::SetTagValue(FGameplayTag Tag, int32 Value)
{
	if (!Tag.IsValid()) return;
	Facts.Add(Tag, Value);
	OnFactChanged.Broadcast(Tag, Value);
}

void UDialogueFactsComponent::AddTagValue(FGameplayTag Tag, int32 Delta)
{
	if (!Tag.IsValid()) return;
	const int32 NewVal = GetTagValue(Tag) + Delta;
	Facts.Add(Tag, NewVal);
	OnFactChanged.Broadcast(Tag, NewVal);
}