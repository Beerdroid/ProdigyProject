// Fill out your copyright notice in the Description page of Project Settings.


#include "DialogueSystem/DialogueResolverComponent.h"

#include "DialogueSystem/DialogueFactsComponent.h"
#include "DialogueSystem/DialogueSystemTypes.h"
#include "Quest/QuestLogComponent.h"

UDialogueResolverComponent::UDialogueResolverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UDialogueResolverComponent::BeginPlay()
{
	Super::BeginPlay();
	CacheFromOwner();
}


void UDialogueResolverComponent::CacheFromOwner()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (!Facts)
	{
		Facts = Owner->FindComponentByClass<UDialogueFactsComponent>();
	}

	if (!QuestLog)
	{
		QuestLog = Owner->FindComponentByClass<UQuestLogComponent>();
	}
}

bool UDialogueResolverComponent::EvaluateAllConditions(const TArray<FDialogueCondition>& Conditions) const
{
	for (const FDialogueCondition& C : Conditions)
	{
		if (!EvaluateCondition(C))
		{
			return false;
		}
	}
	return true;
}

bool UDialogueResolverComponent::EvaluateCondition(const FDialogueCondition& C) const
{

	const FName Q = C.QuestID; // use your real QuestID FName
	UE_LOG(LogTemp, Warning, TEXT("QuestState: %s Accepted=%d Completed=%d TurnedIn=%d"),
		*Q.ToString(),
		QuestLog->IsQuestAccepted(Q),
		QuestLog->IsQuestCompleted(Q),
		QuestLog->IsQuestTurnedIn(Q));
	
	switch (C.Op)
	{
	case EDialogueConditionOp::TagExists:
		return Facts && C.Tag.IsValid() ? Facts->HasTag(C.Tag) : false;

	case EDialogueConditionOp::TagNotExists:
		return Facts && C.Tag.IsValid() ? !Facts->HasTag(C.Tag) : true;

	case EDialogueConditionOp::TagValueGE:
		return Facts && C.Tag.IsValid() ? (Facts->GetTagValue(C.Tag) >= C.Value) : false;

	case EDialogueConditionOp::QuestAccepted:
		return QuestLog && !C.QuestID.IsNone() ? QuestLog->IsQuestAccepted(C.QuestID) : false;

	case EDialogueConditionOp::QuestCompleted:
		return QuestLog && !C.QuestID.IsNone() ? QuestLog->IsQuestCompleted(C.QuestID) : false;

	case EDialogueConditionOp::QuestTurnedIn:
		return QuestLog && !C.QuestID.IsNone() ? QuestLog->IsQuestTurnedIn(C.QuestID) : false;

	case EDialogueConditionOp::QuestNotAccepted:
		return QuestLog && !C.QuestID.IsNone() ? !QuestLog->IsQuestAccepted(C.QuestID) : true;

	case EDialogueConditionOp::QuestNotTurnedIn:
		return QuestLog && !C.QuestID.IsNone() ? !QuestLog->IsQuestTurnedIn(C.QuestID) : true;

	default:
		return false;
	}
}

void UDialogueResolverComponent::ApplyEffects(const TArray<FDialogueEffect>& Effects)
{
	CacheFromOwner();

	for (const FDialogueEffect& E : Effects)
	{
		switch (E.Op)
		{
		case EDialogueEffectOp::AddTag:
			if (Facts && E.Tag.IsValid()) Facts->AddTag(E.Tag);
			break;

		case EDialogueEffectOp::RemoveTag:
			if (Facts && E.Tag.IsValid()) Facts->RemoveTag(E.Tag);
			break;

		case EDialogueEffectOp::SetTagValue:
			if (Facts && E.Tag.IsValid()) Facts->SetTagValue(E.Tag, E.Value);
			break;

		case EDialogueEffectOp::AddTagValue:
			if (Facts && E.Tag.IsValid()) Facts->AddTagValue(E.Tag, E.Value);
			break;

		case EDialogueEffectOp::AcceptQuest:
			if (QuestLog && !E.QuestID.IsNone())
			{
				// Call server RPC on QuestLog (safe if executed on client; UE will route it)
				QuestLog->ServerAddQuest(E.QuestID);
			}
			break;

		case EDialogueEffectOp::TurnInQuest:
			if (QuestLog && !E.QuestID.IsNone())
			{
				QuestLog->ServerTurnInQuest(E.QuestID);
			}
			break;

		default:
			break;
		}
	}
}
