// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InvHighlightable.h"
#include "Components/StaticMeshComponent.h"
#include "InvHighlightableStaticMesh.generated.h"

/**
 * 
 */
UCLASS()
class INVENTORY_API UInvHighlightableStaticMesh : public UStaticMeshComponent, public IInvHighlightable
{
	GENERATED_BODY()
public:
	virtual void Highlight_Implementation() override;
	virtual void UnHighlight_Implementation() override;

private:

	UPROPERTY(EditAnywhere, Category = "Inventory")
	TObjectPtr<UMaterialInterface> HighlightMaterial;
};
