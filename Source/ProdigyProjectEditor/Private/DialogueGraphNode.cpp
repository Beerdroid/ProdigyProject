// Fill out your copyright notice in the Description page of Project Settings.


#include "DialogueGraphNode.h"

#include "DialogueEdGraphSchema.h"

FName UDialogueGraphNode::InputPinName()
{
	return TEXT("In");
}

FName UDialogueGraphNode::ChoicePinName(int32 Index)
{
	return *FString::Printf(TEXT("Choice %d"), Index);
}

void UDialogueGraphNode::AllocateDefaultPins()
{
	UE_LOG(LogTemp, Warning, TEXT("AllocateDefaultPins: Choices=%d"), Choices.Num());
	
	// Create the input pin
	FEdGraphPinType PinType;
	PinType.PinCategory = UDialogueEdGraphSchema::PC_Dialogue;
	PinType.PinSubCategory = NAME_None;
	PinType.PinSubCategoryObject = nullptr;
	PinType.ContainerType = EPinContainerType::None;
	PinType.bIsReference = false;
	PinType.bIsConst = false;
	PinType.bIsWeakPointer = false;
    
	// Input pin
	CreatePin(EGPD_Input, PinType, InputPinName());

	// Output pins for each choice
	for (int32 i = 0; i < Choices.Num(); ++i)
	{
		CreatePin(EGPD_Output, PinType, ChoicePinName(i));
	}

	for (UEdGraphPin* P : Pins)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pin %s Dir=%d Hidden=%d"),
			*P->PinName.ToString(), (int32)P->Direction, (int32)P->bHidden);
	}
}

// Add this at the top of the file, after the includes
#define LOCTEXT_NAMESPACE "DialogueGraphNode"

FText UDialogueGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FString DisplayName;
    
	if (!NodeID.IsNone())
	{
		DisplayName = NodeID.ToString();
        
		// Add speaker if available
		if (!SpeakerID.IsNone())
		{
			DisplayName += FString::Printf(TEXT("\nSpeaker: %s"), *SpeakerID.ToString());
		}
        
		// Add preview of text if available
		if (!Text.IsEmpty())
		{
			FString TextPreview = Text.ToString();
			if (TextPreview.Len() > 32)
			{
				TextPreview = TextPreview.Left(32) + TEXT("...");
			}
			DisplayName += FString::Printf(TEXT("\n%s"), *TextPreview);
		}
        
		return FText::FromString(DisplayName);
	}
    
	return LOCTEXT("UntitledNode", "Untitled Node");
}

#if WITH_EDITOR

void UDialogueGraphNode::RebuildPinsFromChoices()
{
	UEdGraph* Graph = GetGraph();
	if (!Graph)
	{
		return;
	}

	Modify();
	Graph->Modify();

	// 🔴 THIS is the missing step
	// ReconstructNode() alone does nothing in your branch
	UE_LOG(LogTemp, Warning, TEXT("RebuildPinsFromChoices: Choices=%d PinsBefore=%d"), Choices.Num(), Pins.Num());
	Pins.Reset();
	AllocateDefaultPins();
	UE_LOG(LogTemp, Warning, TEXT("RebuildPinsFromChoices: PinsAfter=%d"), Pins.Num());

	// Tell editor + slate to refresh
	Graph->NotifyGraphChanged();
}

#endif

// Add this at the end of the file
#undef LOCTEXT_NAMESPACE

#if WITH_EDITOR

bool UDialogueGraphNode::PropertyChainContains(const FPropertyChangedChainEvent& Evt, FName TargetName)
{
	for (FEditPropertyChain::TDoubleLinkedListNode* Node = Evt.PropertyChain.GetHead();
		Node;
		Node = Node->GetNextNode())
	{
		if (const FProperty* P = Node->GetValue())
		{
			if (P->GetFName() == TargetName)
			{
				return true;
			}
		}
	}
	return false;
}

void UDialogueGraphNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// IMPORTANT: let the property apply first
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName   = PropertyChangedEvent.GetPropertyName();
	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	const bool bChoicesTouched =
		(PropName == GET_MEMBER_NAME_CHECKED(UDialogueGraphNode, Choices)) ||
		(MemberName == GET_MEMBER_NAME_CHECKED(UDialogueGraphNode, Choices));

	if (bChoicesTouched)
	{
		UE_LOG(LogTemp, Warning, TEXT("PostEditChangeProperty: Choices touched, Choices=%d"), Choices.Num());

		Modify();
		ReconstructNode();

		if (UEdGraph* Graph = GetGraph())
		{
			Graph->NotifyGraphChanged();
		}
	}
}

void UDialogueGraphNode::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	// Array add/remove/edit reliably shows up here
	const bool bChoicesTouched = PropertyChainContains(PropertyChangedEvent, GET_MEMBER_NAME_CHECKED(UDialogueGraphNode, Choices));

	if (bChoicesTouched)
	{
		UE_LOG(LogTemp, Warning, TEXT("PostEditChangeChainProperty: Choices touched, Choices=%d"), Choices.Num());

		RebuildPinsFromChoices();
	}
}

#endif

UEdGraphPin* UDialogueGraphNode::GetInputPin() const
{
	return FindPin(InputPinName(), EGPD_Input);
}

UEdGraphPin* UDialogueGraphNode::GetChoicePin(int32 ChoiceIndex) const
{
	return FindPin(ChoicePinName(ChoiceIndex), EGPD_Output);
}

