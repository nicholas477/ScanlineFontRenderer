// Copyright Nicholas Chalkley. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FScanlineFontRendererModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
