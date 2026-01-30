#include "SDialogueGraphNode.h"

#include "DialogueGraphNode.h"
#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"

void SDialogueGraphNode::Construct(const FArguments& InArgs, UDialogueGraphNode* InNode)
{
	this->GraphNode = InNode;
	this->SetCursor(EMouseCursor::CardinalCross);
	UpdateGraphNode();
}

TSharedRef<SWidget> SDialogueGraphNode::CreateTitleWidget()
{
	UDialogueGraphNode* N = CastChecked<UDialogueGraphNode>(GraphNode);

	return SNew(STextBlock)
		.Text(N->GetNodeTitle(ENodeTitleType::FullTitle))
		.WrapTextAt(300.f);
}

void SDialogueGraphNode::CreatePinWidgets()
{
	UE_LOG(LogTemp, Warning, TEXT("SDialogueGraphNode::CreatePinWidgets Pins=%d"), GraphNode ? GraphNode->Pins.Num() : -1);

	for (UEdGraphPin* CurPin : GraphNode->Pins)
	{
		if (!CurPin || CurPin->bHidden) continue;

		TSharedRef<SGraphPin> NewPin = SNew(SGraphPin, CurPin);
		AddPin(NewPin);
	}
}

void SDialogueGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner(SharedThis(this));

	if (PinToAdd->GetDirection() == EGPD_Input)
	{
		InputPins.Add(PinToAdd);
		LeftNodeBox->AddSlot()
		.AutoHeight()
		[
			PinToAdd
		];
	}
	else
	{
		OutputPins.Add(PinToAdd);
		RightNodeBox->AddSlot()
		.AutoHeight()
		[
			PinToAdd
		];
	}
}

void SDialogueGraphNode::UpdateGraphNode()
{
	// Reset the base class state as well
	SGraphNode::UpdateGraphNode();

	InputPins.Empty();
	OutputPins.Empty();
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	// Clear existing UI
	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);

	// IMPORTANT: fully replace the center slot content
	this->GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNew(SBorder)
		.Padding(6)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreateTitleWidget()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 6)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(LeftNodeBox, SVerticalBox)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SSpacer)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(RightNodeBox, SVerticalBox)
				]
			]
		]
	];

	CreatePinWidgets();
}
