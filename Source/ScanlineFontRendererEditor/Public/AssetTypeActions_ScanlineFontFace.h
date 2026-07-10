// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_ScanlineFontFace : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions interface
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, class FMenuBuilder& MenuBuilder) override;
	// End of IAssetTypeActions interface

private:
	/** Reimport from source UFontFace */
	void ExecuteReimport(TArray<TWeakObjectPtr<class UScanlineFontFace>> Objects);

	/** Check if reimport is available */
	bool CanExecuteReimport(TArray<TWeakObjectPtr<class UScanlineFontFace>> Objects) const;
};
