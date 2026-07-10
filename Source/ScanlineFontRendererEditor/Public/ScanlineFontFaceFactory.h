// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "ScanlineFontFaceFactory.generated.h"

UCLASS()
class UScanlineFontFaceFactory : public UFactory
{
	GENERATED_BODY()

public:
	UScanlineFontFaceFactory();

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override { return true; }
	// End of UFactory interface

	/** Source font face to import from */
	UPROPERTY()
	TObjectPtr<class UFontFace> SourceFontFace;

	/** Font size to use for import */
	UPROPERTY()
	float FontSize = 72.0f;
};
