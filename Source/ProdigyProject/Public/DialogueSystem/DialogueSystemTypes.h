// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DialogueSystemTypes.generated.h"

UENUM(BlueprintType)
enum class EDialogueConditionOp : uint8
{
	TagExists      UMETA(DisplayName="Tag Exists"),
	TagNotExists   UMETA(DisplayName="Tag Not Exists"),
	TagValueGE     UMETA(DisplayName="Tag Value >= "),
	QuestAccepted  UMETA(DisplayName="Quest Accepted"),
	QuestCompleted UMETA(DisplayName="Quest Completed"),
	QuestTurnedIn  UMETA(DisplayName="Quest Turned In"),
	QuestNotAccepted UMETA(DisplayName="Quest Not Accepted"),
	QuestNotTurnedIn UMETA(DisplayName="Quest Not Turned In"),
};

UENUM(BlueprintType)
enum class EDialogueEffectOp : uint8
{
	AddTag         UMETA(DisplayName="Add Tag"),
	RemoveTag      UMETA(DisplayName="Remove Tag"),
	SetTagValue    UMETA(DisplayName="Set Tag Value"),
	AddTagValue    UMETA(DisplayName="Add Tag Value"),
	AcceptQuest    UMETA(DisplayName="Accept Quest"),
	TurnInQuest    UMETA(DisplayName="Turn In Quest"),
};

USTRUCT(BlueprintType)
struct FDialogueCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EDialogueConditionOp Op = EDialogueConditionOp::TagExists;

	// Used by tag-based ops
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag Tag;

	// Used by TagValueGE / SetTagValue / AddTagValue
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Value = 0;

	// Used by quest ops
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName QuestID = NAME_None;
};

USTRUCT(BlueprintType)
struct FDialogueEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EDialogueEffectOp Op = EDialogueEffectOp::AddTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Value = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName QuestID = NAME_None;
};

USTRUCT(BlueprintType)
struct FDialogueChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName NextNodeID = NAME_None; // NAME_None => end

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FDialogueCondition> Conditions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FDialogueEffect> Effects;
};

USTRUCT(BlueprintType)
struct FDialogueNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName NodeID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SpeakerID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(MultiLine=true))
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(TitleProperty = "Text"))
	TArray<FDialogueChoice> Choices;
};