// Copyright Nicholas Chalkley. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionScanlineFont.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionScanlineFont : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** UV coordinates for sampling the glyph index texture. Defaults to vertex texture coordinate 0 if not specified. */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "UV coordinates for glyph. Should be 0-1 across the glyph"))
	FExpressionInput TextureUV;

	/** UV coordinates for sampling the glyph index texture. Defaults to vertex texture coordinate 1 if not specified. */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "UV coordinates for glyph lookup. Defaults to vertex texture coordinates if not specified."))
	FExpressionInput GlyphUV;

	/** Texture object input for the curve data texture (Float16 RGB texture containing quadratic Bezier curves) */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Curve data texture containing quadratic Bezier curves"))
	FExpressionInput CurveTexture;

	/** Texture object input for the glyph index texture (R16G16_UINT texture mapping glyphs to curve ranges) */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Glyph index texture mapping characters to curve ranges"))
	FExpressionInput IndexTexture;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override { return false; }
	virtual void GetIncludeFilePaths(TSet<FString>& OutIncludeFilePaths) const override;
#endif
	//~ End UMaterialExpression Interface
};
