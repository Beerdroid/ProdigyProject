#pragma once

#include "Modules/ModuleManager.h"

class FDialogueGraphNodeFactory;
class IAssetTypeActions;

class FProdigyProjectEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<IAssetTypeActions> DialogueAssetActions;
	TSharedPtr<FDialogueGraphNodeFactory> DialogueNodeFactory;
};
