// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScanlineFontFaceUtils.h"

#include "ScanlineFontFace.h"
#include "Engine/Texture2D.h"

UE_DISABLE_OPTIMIZATION

#if WITH_EDITOR
#include "Engine/FontFace.h"
#include "Fonts/FontFaceInterface.h"
#include "Fonts/CompositeFont.h"

#if WITH_FREETYPE
#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_BBOX_H
#include FT_GLYPH_H

namespace
{
	/**
	 * Structure to hold state during outline decomposition
	 */
	struct FOutlineConverter
	{
		FScanlineFontGlyph* Glyph = nullptr;
		FScanlineFontContour* CurrentContour = nullptr;
		FVector2D LastPoint = FVector2D::ZeroVector;
		FVector2D FirstCubicControl = FVector2D::ZeroVector;
		bool bHasCubicControl = false;

		// FreeType outline decomposition callbacks
		static int MoveToFunc(const FT_Vector* To, void* User);
		static int LineToFunc(const FT_Vector* To, void* User);
		static int ConicToFunc(const FT_Vector* Control, const FT_Vector* To, void* User);
		static int CubicToFunc(const FT_Vector* Control1, const FT_Vector* Control2, const FT_Vector* To, void* User);
	};

	// Convert FreeType 26.6 fixed-point to float
	static float FixedToFloat(FT_Pos Value)
	{
		return static_cast<float>(Value) / 64.0f;
	}

	static FVector2D Vec2FromFT(const FT_Vector* Vec)
	{
		return FVector2D(FixedToFloat(Vec->x), FixedToFloat(Vec->y));
	}

	int FOutlineConverter::MoveToFunc(const FT_Vector* To, void* User)
	{
		FOutlineConverter* Converter = static_cast<FOutlineConverter*>(User);

		// Start a new contour
		FScanlineFontContour NewContour;
		Converter->Glyph->Contours.Add(NewContour);
		Converter->CurrentContour = &Converter->Glyph->Contours.Last();
		Converter->LastPoint = Vec2FromFT(To);
		Converter->bHasCubicControl = false;

		return 0;
	}

	int FOutlineConverter::LineToFunc(const FT_Vector* To, void* User)
	{
		FOutlineConverter* Converter = static_cast<FOutlineConverter*>(User);
		FVector2D ToPoint = Vec2FromFT(To);

		// Create a straight line as a quadratic with control point at midpoint
		FVector2D Control = (Converter->LastPoint + ToPoint) * 0.5f;
		Converter->CurrentContour->Curves.Add(FScanlineQuadraticBezier(Converter->LastPoint, Control, ToPoint));

		Converter->LastPoint = ToPoint;
		Converter->bHasCubicControl = false;
		return 0;
	}

	int FOutlineConverter::ConicToFunc(const FT_Vector* Control, const FT_Vector* To, void* User)
	{
		FOutlineConverter* Converter = static_cast<FOutlineConverter*>(User);
		FVector2D ControlPoint = Vec2FromFT(Control);
		FVector2D ToPoint = Vec2FromFT(To);

		Converter->CurrentContour->Curves.Add(FScanlineQuadraticBezier(Converter->LastPoint, ControlPoint, ToPoint));

		Converter->LastPoint = ToPoint;
		Converter->bHasCubicControl = false;
		return 0;
	}

	int FOutlineConverter::CubicToFunc(const FT_Vector* Control1, const FT_Vector* Control2, const FT_Vector* To, void* User)
	{
		FOutlineConverter* Converter = static_cast<FOutlineConverter*>(User);
		FVector2D C1 = Vec2FromFT(Control1);
		FVector2D C2 = Vec2FromFT(Control2);
		FVector2D ToPoint = Vec2FromFT(To);

		// Convert cubic bezier to quadratic bezier(s)
		// For simplicity, we'll use a midpoint approximation
		// P0 = Converter->LastPoint, C0 = C1, C1 = C2, P1 = ToPoint
		// Approximate with single quadratic: Q = 3/4 * midpoint of cubic controls
		FVector2D CubicMid = (C1 + C2) * 0.5f;
		FVector2D QuadControl = (3.0f / 4.0f) * CubicMid + (1.0f / 8.0f) * (Converter->LastPoint + ToPoint);

		// Check if we need to subdivide for better accuracy
		float Distance1 = FVector2D::Distance(Converter->LastPoint, C1);
		float Distance2 = FVector2D::Distance(C1, C2);
		float Distance3 = FVector2D::Distance(C2, ToPoint);
		float TotalDistance = Distance1 + Distance2 + Distance3;
		float DirectDistance = FVector2D::Distance(Converter->LastPoint, ToPoint);

		// If the curve is very curved (total path >> direct distance), subdivide
		if (TotalDistance > DirectDistance * 1.5f)
		{
			// Subdivide cubic at t=0.5 using De Casteljau
			FVector2D P01 = (Converter->LastPoint + C1) * 0.5f;
			FVector2D P12 = (C1 + C2) * 0.5f;
			FVector2D P23 = (C2 + ToPoint) * 0.5f;
			FVector2D P012 = (P01 + P12) * 0.5f;
			FVector2D P123 = (P12 + P23) * 0.5f;
			FVector2D P0123 = (P012 + P123) * 0.5f;

			// First half: P0 -> P01, P012 -> P0123
			FVector2D Q1 = (3.0f / 4.0f) * (P01 + P012) * 0.5f + (1.0f / 8.0f) * (Converter->LastPoint + P0123);
			Converter->CurrentContour->Curves.Add(FScanlineQuadraticBezier(Converter->LastPoint, Q1, P0123));

			// Second half: P0123 -> P123, P23 -> P1
			FVector2D Q2 = (3.0f / 4.0f) * (P123 + P23) * 0.5f + (1.0f / 8.0f) * (P0123 + ToPoint);
			Converter->CurrentContour->Curves.Add(FScanlineQuadraticBezier(P0123, Q2, ToPoint));
		}
		else
		{
			// Single quadratic approximation is good enough
			Converter->CurrentContour->Curves.Add(FScanlineQuadraticBezier(Converter->LastPoint, QuadControl, ToPoint));
		}

		Converter->LastPoint = ToPoint;
		Converter->bHasCubicControl = false;
		return 0;
	}
}

namespace ScanlineFontFaceUtils
{
	/**
	 * Extract a glyph from FreeType face and convert to quadratic beziers
	 */
	bool ExtractGlyph(const FT_Face& Face, const FT_UInt& GlyphIndex, FScanlineFontGlyph& OutGlyph)
	{
		FT_Error Error = FT_Load_Glyph(Face, GlyphIndex, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING | FT_LOAD_NO_SCALE);
		if (Error != 0)
		{
			return false;
		}

		if (Face->glyph->format != FT_GLYPH_FORMAT_OUTLINE)
		{
			return false;
		}

		// Get metrics (in font units since we used FT_LOAD_NO_SCALE)
		OutGlyph.AdvanceWidth = FixedToFloat(Face->glyph->metrics.horiAdvance);
		OutGlyph.LeftSideBearing = FixedToFloat(Face->glyph->metrics.horiBearingX);

		// Get bounding box
		FT_BBox BBox;
		FT_Outline_Get_BBox(&Face->glyph->outline, &BBox);
		OutGlyph.BoundingBox = FBox2D(
			FVector2D(FixedToFloat(BBox.xMin), FixedToFloat(BBox.yMin)),
			FVector2D(FixedToFloat(BBox.xMax), FixedToFloat(BBox.yMax))
		);

		// Set up outline decomposition
		FT_Outline_Funcs Callbacks;
		Callbacks.move_to = &FOutlineConverter::MoveToFunc;
		Callbacks.line_to = &FOutlineConverter::LineToFunc;
		Callbacks.conic_to = &FOutlineConverter::ConicToFunc;
		Callbacks.cubic_to = &FOutlineConverter::CubicToFunc;
		Callbacks.shift = 0;
		Callbacks.delta = 0;

		FOutlineConverter Converter;
		Converter.Glyph = &OutGlyph;

		Error = FT_Outline_Decompose(&Face->glyph->outline, &Callbacks, &Converter);
		return Error == 0;
	}
}

#endif // WITH_FREETYPE
namespace
{
	struct FFontGlyphTextureData
	{
		int32 StartIndex;
		int32 EndIndexPlusOne;
		FVector2D GlyphSize;
	};

	struct FFontTextureData
	{
		TArray<FFontGlyphTextureData> Glyphs;
		TArray<FScanlineQuadraticBezier> Curves;
	};

	bool CreateCurveDataTexture(const FFontTextureData& TextureData, UScanlineFontFace* FontFace, UTexture2D*& OutCurveDataTexture)
	{
		// Create a texture to hold the curve data
		const int32 CurveCount = TextureData.Curves.Num();
		if (CurveCount == 0)
		{
			return false;
		}

		// 2 pixels per curve
		const auto [TextureWidth, TextureHeight] = ScanlineFontFaceUtils::GetTextureWidthHeight(CurveCount * 2);

		const FName TextureName = MakeUniqueObjectName(FontFace, UTexture2D::StaticClass(), TEXT("CurveData"));
		OutCurveDataTexture = NewObject<UTexture2D>(FontFace, TextureName, RF_Public);
		if (!OutCurveDataTexture)
		{
			return false;
		}

		// Configure texture settings first
		OutCurveDataTexture->SRGB = false;
		OutCurveDataTexture->CompressionSettings = TC_HDR;
		OutCurveDataTexture->MipGenSettings = TMGS_NoMipmaps;
		OutCurveDataTexture->Filter = TF_Nearest;
		OutCurveDataTexture->AddressX = TA_Clamp;
		OutCurveDataTexture->AddressY = TA_Clamp;
		OutCurveDataTexture->NeverStream = true;

		{
			TArray<FFloat16Color> ImageData;
			ImageData.AddDefaulted(CurveCount * 2);

			for (int32 CurveIndex = 0; CurveIndex < CurveCount; ++CurveIndex)
			{
				const FScanlineQuadraticBezier& Curve = TextureData.Curves[CurveIndex];
				// Each curve is stored in 2 pixels
				const int32 PixelIndex = CurveIndex * 2;

				const auto ColorPair = Curve.ToColorPair();
				ImageData.GetData()[PixelIndex] = ColorPair.Key;
				ImageData.GetData()[PixelIndex + 1] = ColorPair.Value;
			}

			// Initialize source data - this is what gets serialized
			OutCurveDataTexture->Source.Init(TextureWidth, TextureHeight, 1, 1, TSF_RGBA16F, (uint8*)ImageData.GetData());
		}

		OutCurveDataTexture->UpdateResource();
		FontFace->MarkPackageDirty();
		return true;
	}

	bool CreateGlyphTexture(const FFontTextureData& TextureData, UScanlineFontFace* FontFace, UTexture2D*& OutGlyphDataTexture)
	{
		// Create a texture to hold the curve data
		const int32 GlyphCount = TextureData.Glyphs.Num();
		if (GlyphCount == 0)
		{
			return false;
		}

		const auto [TextureWidth, TextureHeight] = ScanlineFontFaceUtils::GetTextureWidthHeight(GlyphCount);

		const FName TextureName = MakeUniqueObjectName(FontFace, UTexture2D::StaticClass(), TEXT("GlpyhData"));
		OutGlyphDataTexture = NewObject<UTexture2D>(FontFace, TextureName, RF_Public);
		if (!OutGlyphDataTexture)
		{
			return false;
		}

		// Configure texture settings first
		OutGlyphDataTexture->SRGB = false;
		OutGlyphDataTexture->CompressionSettings = TC_HDR_F32;
		OutGlyphDataTexture->MipGenSettings = TMGS_NoMipmaps;
		OutGlyphDataTexture->Filter = TF_Nearest;
		OutGlyphDataTexture->AddressX = TA_Clamp;
		OutGlyphDataTexture->AddressY = TA_Clamp;
		OutGlyphDataTexture->NeverStream = true;

		{
			TArray<FLinearColor> ImageData;
			ImageData.AddDefaulted(GlyphCount);

			for (int32 GlyphIndex = 0; GlyphIndex < GlyphCount; ++GlyphIndex)
			{
				const FFontGlyphTextureData& Glpyh = TextureData.Glyphs[GlyphIndex];

				ImageData.GetData()[GlyphIndex].R = Glpyh.StartIndex;
				ImageData.GetData()[GlyphIndex].G = Glpyh.EndIndexPlusOne;
				ImageData.GetData()[GlyphIndex].B = Glpyh.GlyphSize.X;
				ImageData.GetData()[GlyphIndex].A = Glpyh.GlyphSize.Y;
			}

			// Initialize source data - this is what gets serialized
			OutGlyphDataTexture->Source.Init(TextureWidth, TextureHeight, 1, 1, TSF_RGBA32F, (uint8*)ImageData.GetData());
		}

		OutGlyphDataTexture->UpdateResource();
		FontFace->MarkPackageDirty();
		return true;
	}
}

namespace ScanlineFontFaceUtils
{
	TPair<int32, int32> GetTextureWidthHeight(const int32 Num)
	{
		const int32 TextureWidth = FMath::RoundUpToPowerOfTwo(FMath::Sqrt((float)Num));
		int32 TextureHeight = FMath::DivideAndRoundUp(Num, TextureWidth);

		return { TextureWidth, TextureHeight };
	}

	bool CreateFontTextures(UScanlineFontFace* FontFace, UTexture2D*& OutCurveDataTexture, UTexture2D*& OutGlyphIndexTexture)
	{
		FFontTextureData TextureData;

		int32 CurrentIndex = 0;
		int32 CurrentCurveIndex = 0;

		const auto AddCurve = [&](const FScanlineQuadraticBezier& Curve)
		{
			TextureData.Curves.Add(Curve);
			CurrentCurveIndex++;
		};

		FontFace->CodePointToTextureIndex.Empty();

		for (const TPair<int32, FScanlineFontGlyph>& Glyph : FontFace->Glyphs)
		{
			// Don't add empty glyphs to the texture
			if (Glyph.Value.Contours.Num() == 0)
			{
				continue;
			}

			FontFace->CodePointToTextureIndex.Add(Glyph.Key, CurrentIndex);
			FFontGlyphTextureData& NewGlpyh = TextureData.Glyphs.AddDefaulted_GetRef();

			NewGlpyh.StartIndex = CurrentCurveIndex;
			for (const FScanlineFontContour& Contour : Glyph.Value.Contours)
			{
				for (const FScanlineQuadraticBezier& Curve : Contour.Curves)
				{
					if (Curve.IsHorizontal())
					{
						continue;
					}

					auto [T1, T2] = Curve.GetTurningPoints();

					if (T1 > T2)
					{
						Swap(T1, T2);
					}

					check(T2 >= T1);

					if (T1 > 0 && T1 < 1)
					{
						UE_LOG(LogTemp, Warning, TEXT("Split at t1: %f"), T1);
					}

					if (T2 > 0 && T2 < 1)
					{
						UE_LOG(LogTemp, Warning, TEXT("Split at t2: %f"), T2);
					}

					if (T1 > 0 && T1 < 1)
					{
						FScanlineQuadraticBezier Curve1, Curve2;
						Curve.Subdivide(T1, Curve1, Curve2);

						AddCurve(Curve1);

						if (T2 > 0 && T2 < 1)
						{
							T2 = (T2 - T1) / (1.f - T1);

							FScanlineQuadraticBezier Curve3, Curve4;
							Curve2.Subdivide(T2, Curve3, Curve4);
							AddCurve(Curve3);
							AddCurve(Curve4);
						}
						else
						{
							AddCurve(Curve2);
						}

						continue;
					}

					if (T2 > 0 && T2 < 1)
					{
						FScanlineQuadraticBezier Curve1, Curve2;
						Curve.Subdivide(T2, Curve1, Curve2);

						AddCurve(Curve1);
						AddCurve(Curve2);

						continue;
					}
					
					AddCurve(Curve);
				}
			}
			NewGlpyh.EndIndexPlusOne = CurrentCurveIndex;
			NewGlpyh.GlyphSize = Glyph.Value.BoundingBox.GetSize();

			CurrentIndex++;
		}

		return CreateCurveDataTexture(TextureData, FontFace, OutCurveDataTexture) 
			&& CreateGlyphTexture(TextureData, FontFace, OutGlyphIndexTexture);
	}
}

#endif 

UE_ENABLE_OPTIMIZATION