// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DialogueSystem/DialogueSystemTypes.h"
#include "EdGraph/EdGraphNode.h"
#include "DialogueGraphNode.generated.h"

/**
 * 
 */
UCLASS()
class PRODIGYPROJECTEDITOR_API UDialogueGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	// Stable ID for this node in runtime data
	UPROPERTY(EditAnywhere, Category="Dialogue")
	FName NodeID = NAME_None;

	UPROPERTY(EditAnywhere, Category="Dialogue")
	FName SpeakerID = NAME_None;

	UPROPERTY(EditAnywhere, Category="Dialogue", meta=(MultiLine=true))
	FText Text;

	// Node-local outgoing choices. These map to edges.
	UPROPERTY(EditAnywhere, Category="Dialogue")
	TArray<FDialogueChoice> Choices;

	// Pins
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

#if WITH_EDITOR
	void RebuildPinsFromChoices();
#endif
	
#if WITH_EDITOR
	static bool PropertyChainContains(const FPropertyChangedChainEvent& Evt, FName TargetName);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	UEdGraphPin* GetInputPin() const;
	UEdGraphPin* GetChoicePin(int32 ChoiceIndex) const;

	static FName InputPinName();
	static FName ChoicePinName(int32 Index);

	virtual bool CanUserDeleteNode() const override { return true; }
	virtual bool CanDuplicateNode() const override { return true; }
	
};
