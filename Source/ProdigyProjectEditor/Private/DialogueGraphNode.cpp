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

// Add this at the end of the file
#undef LOCTEXT_NAMESPACE


void UDialogueGraphNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDialogueGraphNode, NodeID) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UDialogueGraphNode, SpeakerID) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UDialogueGraphNode, Text) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UDialogueGraphNode, Choices))
	{
		ReconstructNode();
	}
    
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

UEdGraphPin* UDialogueGraphNode::GetInputPin() const
{
	return FindPin(InputPinName(), EGPD_Input);
}

UEdGraphPin* UDialogueGraphNode::GetChoicePin(int32 ChoiceIndex) const
{
	return FindPin(ChoicePinName(ChoiceIndex), EGPD_Output);
}

