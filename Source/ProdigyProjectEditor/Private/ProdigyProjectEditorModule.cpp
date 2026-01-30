#include "ProdigyProjectEditorModule.h"

#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

#include "DialogueDataAssetTypeActions.h"
#include "DialogueGraphNodeFactory.h"

IMPLEMENT_MODULE(FProdigyProjectEditorModule, ProdigyProjectEditor)

void FProdigyProjectEditorModule::StartupModule()
{
	UE_LOG(LogTemp, Warning, TEXT("[ProdigyProjectEditor] StartupModule"));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	DialogueAssetActions = MakeShared<FDialogueDataAssetTypeActions>();
	AssetTools.RegisterAssetTypeActions(DialogueAssetActions.ToSharedRef());

	DialogueNodeFactory = MakeShared<FDialogueGraphNodeFactory>();
	FEdGraphUtilities::RegisterVisualNodeFactory(DialogueNodeFactory);
	UE_LOG(LogTemp, Warning, TEXT("[ProdigyProjectEditor] Registered Dialogue node factory"));

	UE_LOG(LogTemp, Warning, TEXT("[ProdigyProjectEditor] Registered DialogueDataAssetTypeActions"));
}

void FProdigyProjectEditorModule::ShutdownModule()
{
	UE_LOG(LogTemp, Warning, TEXT("[ProdigyProjectEditor] ShutdownModule"));

	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		if (DialogueAssetActions.IsValid())
		{
			AssetTools.UnregisterAssetTypeActions(DialogueAssetActions.ToSharedRef());
		}
	}

	if (DialogueNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(DialogueNodeFactory);
		DialogueNodeFactory.Reset();
	}

	DialogueAssetActions.Reset();
}
