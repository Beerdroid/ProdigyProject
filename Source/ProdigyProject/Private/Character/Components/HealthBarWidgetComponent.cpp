#include "HealthBarWidgetComponent.h"

#include "ProdigyWorldHealthBarWidget.h"
#include "AbilitySystem/AttributesComponent.h"
#include "AbilitySystem/ProdigyGameplayTags.h"

class UProdigyWorldHealthBarWidget;

UHealthBarWidgetComponent::UHealthBarWidgetComponent()
{
	HealthTag    = ProdigyTags::Attr::Health;
	MaxHealthTag = ProdigyTags::Attr::MaxHealth;
}

void UHealthBarWidgetComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveAttributes();
	BindAttributes();
	Refresh();
}

void UHealthBarWidgetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Force last visual update before the owner disappears/unregisters.
	Refresh();

	UnbindAttributes();

	Super::EndPlay(EndPlayReason);
}

void UHealthBarWidgetComponent::BindAttributes()
{
	if (!IsValid(Attributes)) return;

	Attributes->OnAttributeChanged.RemoveDynamic(this, &UHealthBarWidgetComponent::HandleAttrChanged);
	Attributes->OnAttributeChanged.AddDynamic(this, &UHealthBarWidgetComponent::HandleAttrChanged);
}

void UHealthBarWidgetComponent::UnbindAttributes()
{
	if (!IsValid(Attributes)) return;

	Attributes->OnAttributeChanged.RemoveDynamic(this, &UHealthBarWidgetComponent::HandleAttrChanged);
}

void UHealthBarWidgetComponent::ResolveAttributes()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		Attributes = nullptr;
		return;
	}

	Attributes = Owner->FindComponentByClass<UAttributesComponent>();
}


void UHealthBarWidgetComponent::HandleAttrChanged(FGameplayTag Tag, float NewValue, float Delta, AActor* InstigatorActor)
{
	if (Tag == HealthTag || Tag == MaxHealthTag)
	{
		Refresh();
	}
}

void UHealthBarWidgetComponent::Refresh()
{
	UProdigyWorldHealthBarWidget* W =
		Cast<UProdigyWorldHealthBarWidget>(GetUserWidgetObject());

	if (!IsValid(W) || !IsValid(Attributes)) return;

	const float Max = Attributes->GetFinalValue(MaxHealthTag);
	const float Cur = Attributes->GetCurrentValue(HealthTag);

	W->SetHP(Cur, Max > 0.f ? Max : 1.f);
}