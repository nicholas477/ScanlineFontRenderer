// Copyright Nicholas Chalkley. All Rights Reserved.

#include "MaterialExpressionScanlineFont.h"
#include "MaterialCompiler.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionScanlineFont"

UMaterialExpressionScanlineFont::UMaterialExpressionScanlineFont(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Font;
		FConstructorStatics()
			: NAME_Font(LOCTEXT("Font", "Font"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Font);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionScanlineFont::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// int32 UMaterialExpressionMaterialXFractal3D::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)

	//float2 TextureUV,
	//float2 GlyphUV,
	//Texture2D CurveTexture,
	//SamplerState CurveSampler,
	//Texture2D IndexTexture,
	//SamplerState IndexSampler

	// Get input coordinates for glyph lookup (defaults to TexCoord 0)
	int32 TextureUVIndex = TextureUV.GetTracedInput().Expression ?
		TextureUV.Compile(Compiler) : Compiler->TextureCoordinate(0, false, false);

	// Get input coordinates for glyph lookup (defaults to TexCoord 1)
	int32 GlyphUVIndex = GlyphUV.GetTracedInput().Expression ? 
		GlyphUV.Compile(Compiler) : Compiler->TextureCoordinate(1, false, false);

	// Get texture object inputs
	int32 CurveTextureIndex = CurveTexture.GetTracedInput().Expression ? 
		CurveTexture.Compile(Compiler) : Compiler->Errorf(TEXT("ScanlineFont: Missing CurveTexture input"));
	int32 IndexTextureIndex = IndexTexture.GetTracedInput().Expression ? 
		IndexTexture.Compile(Compiler) : Compiler->Errorf(TEXT("ScanlineFont: Missing IndexTexture input"));

	if (GlyphUVIndex == INDEX_NONE || CurveTextureIndex == INDEX_NONE || IndexTextureIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	UMaterialExpressionCustom* MaterialExpressionCustom = NewObject<UMaterialExpressionCustom>();

	TSet<FString> Includes;
	GetIncludeFilePaths(Includes);

	MaterialExpressionCustom->IncludeFilePaths.Append(Includes.Array());
	MaterialExpressionCustom->Inputs[0].InputName = TEXT("TextureUV");
	MaterialExpressionCustom->Inputs.Add({ TEXT("GlyphUV") });
	MaterialExpressionCustom->Inputs.Add({ TEXT("CurveTexture") });
	MaterialExpressionCustom->Inputs.Add({ TEXT("IndexTexture") });

	MaterialExpressionCustom->OutputType = ECustomMaterialOutputType::CMOT_Float1; //Noise functions returns only a float
	MaterialExpressionCustom->Code =
		TEXT(R"(return RasterizeScanlineFont(TextureUV, GlyphUV, CurveTexture, CurveTextureSampler, IndexTexture, IndexTextureSampler);)");

	TArray<int32> Inputs{ TextureUVIndex, GlyphUVIndex, CurveTextureIndex, IndexTextureIndex };

	return Compiler->CustomExpression(MaterialExpressionCustom, 0, Inputs);
}

void UMaterialExpressionScanlineFont::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Scanline Font"));
}

void UMaterialExpressionScanlineFont::GetIncludeFilePaths(TSet<FString>& OutIncludeFilePaths) const
{
	// Tell the material compiler to include our shader file
	OutIncludeFilePaths.Add(TEXT("/Plugin/ScanlineFontRenderer/Private/ScanlineFontRasterize.usf"));
}

uint32 UMaterialExpressionScanlineFont::GetInputType(int32 InputIndex)
{
	switch (InputIndex)
	{
	case 0: // TextureUV
		return MCT_Float2;
	case 1: // GlyphUV
		return MCT_Float2;
	case 2: // CurveTexture
		return MCT_Texture2D;
	case 3: // IndexTexture
		return MCT_Texture2D;
	case 4: // PixelPosition
		return MCT_Float2;
	default:
		return MCT_Unknown;
	}
}

uint32 UMaterialExpressionScanlineFont::GetOutputType(int32 OutputIndex)
{
	// Returns a scalar coverage value (0-1)
	return MCT_Float1;
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
