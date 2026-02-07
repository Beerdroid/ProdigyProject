// Fill out your copyright notice in the Description page of Project Settings.


#include "ProdigyInventory/InvHighlightableStaticMesh.h"

void UInvHighlightableStaticMesh::Highlight_Implementation()
{
	SetOverlayMaterial(HighlightMaterial);
}

void UInvHighlightableStaticMesh::UnHighlight_Implementation()
{
	SetOverlayMaterial(nullptr);
}
