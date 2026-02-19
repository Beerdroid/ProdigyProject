#include "DamageTextComponent.h"

UDamageTextComponent::UDamageTextComponent()
{
	// Floating numbers should be world-anchored so they start from the anchor component position
	SetWidgetSpace(EWidgetSpace::Screen);

	SetBlendMode(EWidgetBlendMode::Transparent);
	SetDrawAtDesiredSize(true);

	SetOnlyOwnerSee(false);
	SetOwnerNoSee(false);
	SetHiddenInGame(false);
	SetVisibility(true, true);
	SetTickWhenOffscreen(true);

	// Optional but helps readability in world space
	SetPivot(FVector2D(0.5f, 0.5f));
}
