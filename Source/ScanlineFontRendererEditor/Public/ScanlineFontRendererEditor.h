// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FScanlineFontRendererEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Register asset actions */
	void RegisterAssetTypeActions();

	/** Unregister asset actions */
	void UnregisterAssetTypeActions();

	/** Register context menu extensions */
	void RegisterMenuExtensions();

	/** Unregister context menu extensions */
	void UnregisterMenuExtensions();

	/** Register details customizations */
	void RegisterDetailsCustomizations();

	/** Unregister details customizations */
	void UnregisterDetailsCustomizations();

	/** Array of registered asset type actions */
	TArray<TSharedPtr<class IAssetTypeActions>> RegisteredAssetTypeActions;

	/** Delegate handle for content browser extender */
	FDelegateHandle ContentBrowserExtenderDelegateHandle;
};
