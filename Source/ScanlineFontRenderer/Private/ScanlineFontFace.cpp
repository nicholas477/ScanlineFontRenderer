// Copyright Nicholas Chalkley. All Rights Reserved.

#include "ScanlineFontFace.h"
#include "ScanlineFontFaceUtils.h"
#include "Engine/Texture2D.h"

#if WITH_EDITOR
#include "Engine/FontFace.h"
#include "Fonts/FontFaceInterface.h"
#include "Fonts/CompositeFont.h"

FOnScanlineFontFacePropertyChanged UScanlineFontFace::OnPropertyChanged;

#if WITH_FREETYPE
#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_BBOX_H
#include FT_GLYPH_H
#endif // WITH_FREETYPE
#endif // WITH_EDITOR



FBox2D FScanlineQuadraticBezier::GetBounds() const
{
	FBox2D Bounds(ForceInit);
	Bounds += P0;
	Bounds += P2;

	// Check if the control point creates extrema
	// Derivative = 2 * ((1-t) * (P1-P0) + t * (P2-P1)) = 0
	// Solve for t in each dimension

	// X dimension
	float Numerator = P0.X - P1.X;
	float Denominator = P0.X - 2.0f * P1.X + P2.X;
	if (FMath::Abs(Denominator) > SMALL_NUMBER)
	{
		float T = Numerator / Denominator;
		if (T > 0.0f && T < 1.0f)
		{
			Bounds += Evaluate(T);
		}
	}

	// Y dimension
	Numerator = P0.Y - P1.Y;
	Denominator = P0.Y - 2.0f * P1.Y + P2.Y;
	if (FMath::Abs(Denominator) > SMALL_NUMBER)
	{
		float T = Numerator / Denominator;
		if (T > 0.0f && T < 1.0f)
		{
			Bounds += Evaluate(T);
		}
	}

	return Bounds;
}

void FScanlineQuadraticBezier::Subdivide(float T, FScanlineQuadraticBezier& OutLeft, FScanlineQuadraticBezier& OutRight) const
{
	// De Casteljau's algorithm
	FVector2D P01 = FMath::Lerp(P0, P1, T);
	FVector2D P12 = FMath::Lerp(P1, P2, T);
	FVector2D P012 = FMath::Lerp(P01, P12, T);

	OutLeft = FScanlineQuadraticBezier(P0, P01, P012);
	OutRight = FScanlineQuadraticBezier(P012, P12, P2);
}

// FScanlineFontContour implementation

FBox2D FScanlineFontContour::GetBounds() const
{
	FBox2D Bounds(ForceInit);
	for (const FScanlineQuadraticBezier& Curve : Curves)
	{
		Bounds += Curve.GetBounds();
	}
	return Bounds;
}

bool FScanlineFontContour::IsClosed() const
{
	if (Curves.Num() == 0)
	{
		return false;
	}

	const FVector2D& FirstStart = Curves[0].P0;
	const FVector2D& LastEnd = Curves.Last().P2;
	return FirstStart.Equals(LastEnd, KINDA_SMALL_NUMBER);
}

void FScanlineFontContour::Reverse()
{
	Algo::Reverse(Curves);
	for (FScanlineQuadraticBezier& Curve : Curves)
	{
		Swap(Curve.P0, Curve.P2);
	}
	bClockwise = !bClockwise;
}

int32 FScanlineFontGlyph::GetTotalCurveCount() const
{
	int32 Count = 0;
	for (const FScanlineFontContour& Contour : Contours)
	{
		Count += Contour.Curves.Num();
	}
	return Count;
}

void FScanlineFontGlyph::Transform(const FMatrix2x2& Matrix, const FVector2D& Translation)
{
	for (FScanlineFontContour& Contour : Contours)
	{
		for (FScanlineQuadraticBezier& Curve : Contour.Curves)
		{
			Curve.P0 = Matrix.TransformVector(Curve.P0) + Translation;
			Curve.P1 = Matrix.TransformVector(Curve.P1) + Translation;
			Curve.P2 = Matrix.TransformVector(Curve.P2) + Translation;
		}
	}

	// Transform bounding box
	FVector2D Min = Matrix.TransformVector(BoundingBox.Min) + Translation;
	FVector2D Max = Matrix.TransformVector(BoundingBox.Max) + Translation;
	BoundingBox = FBox2D(Min, Max);

	// Transform metrics
	AdvanceWidth = Matrix.TransformVector(FVector2D(AdvanceWidth, 0.0f)).X;
	LeftSideBearing = Matrix.TransformVector(FVector2D(LeftSideBearing, 0.0f)).X;
}

void FScanlineFontGlyph::Scale(float ScaleFactor)
{
	for (FScanlineFontContour& Contour : Contours)
	{
		for (FScanlineQuadraticBezier& Curve : Contour.Curves)
		{
			Curve.P0 *= ScaleFactor;
			Curve.P1 *= ScaleFactor;
			Curve.P2 *= ScaleFactor;
		}
	}

	BoundingBox.Min *= ScaleFactor;
	BoundingBox.Max *= ScaleFactor;
	AdvanceWidth *= ScaleFactor;
	LeftSideBearing *= ScaleFactor;
}

// UScanlineFontFace implementation

UScanlineFontFace::UScanlineFontFace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	for (int32 CodePoint = 32; CodePoint <= 126; CodePoint++)
	{
		CharactersToImport.AppendChar((TCHAR)CodePoint);
	}
	for (int32 CodePoint = 160; CodePoint <= 255; CodePoint++)
	{
		CharactersToImport.AppendChar((TCHAR)CodePoint);
	}
}

bool UScanlineFontFace::GetGlyph(int32 CodePoint, FScanlineFontGlyph& OutGlyph) const
{
	const FScanlineFontGlyph* Found = Glyphs.Find(CodePoint);
	if (Found)
	{
		OutGlyph = *Found;
		return true;
	}
	return false;
}

int32 UScanlineFontFace::GetIndexOfCodepoint(int32 CodePoint) const
{
	check(Glyphs.Num() == CodePointToIndex.Num());

	if (const int32* Index = CodePointToIndex.Find(CodePoint))
	{
		return *Index;
	}

	return INDEX_NONE;
}

int32 UScanlineFontFace::GetIndexOfTextureCodepoint(int32 CodePoint) const
{
	if (const int32* Index = CodePointToTextureIndex.Find(CodePoint))
	{
		return *Index;
	}

	return INDEX_NONE;
}

bool UScanlineFontFace::GetUVForCodepoint(int32 CodePoint, FVector2D& OutUV) const
{
	int32 Index = GetIndexOfTextureCodepoint(CodePoint);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	// Check if we have a valid glyph index texture
	if (!GlyphIndexTexture)
	{
		return false;
	}

	// Get texture dimensions
	int32 TextureWidth = GlyphIndexTexture->GetSizeX();
	int32 TextureHeight = GlyphIndexTexture->GetSizeY();

	if (TextureWidth <= 0 || TextureHeight <= 0)
	{
		return false;
	}

	// Calculate pixel coordinates from linear index
	int32 PixelX = Index % TextureWidth;
	int32 PixelY = Index / TextureWidth;

	// Convert to UV coordinates (pixel center)
	// Add 0.5 to sample from pixel center
	OutUV.X = (PixelX + 0.5f) / (float)TextureWidth;
	OutUV.Y = (PixelY + 0.5f) / (float)TextureHeight;

	return true;
}

bool UScanlineFontFace::HasGlyph(int32 CodePoint) const
{
	return Glyphs.Contains(CodePoint);
}

void UScanlineFontFace::GetAvailableCodePoints(TArray<int32>& OutCodePoints) const
{
	Glyphs.GetKeys(OutCodePoints);
}

float UScanlineFontFace::GetScaleForPixelHeight(float PixelHeight) const
{
	if (UnitsPerEm <= 0)
	{
		return 1.0f;
	}
	return PixelHeight / static_cast<float>(UnitsPerEm / 32.f);
}

FVector2D UScanlineFontFace::CalculateEmToWorldScale(float ScaleX, float ScaleY, float WorldSize) const
{
	const float WorldScale = (WorldSize / 32.f) / (UnitsPerEm / 1000.f);
	return FVector2D(ScaleX * WorldScale, ScaleY * WorldScale);
}

float UScanlineFontFace::GetLineHeight() const
{
	return Ascender - Descender;
}

float UScanlineFontFace::MeasureText(const FString& Text) const
{
	float TotalWidth = 0.0f;
	for (int32 i = 0; i < Text.Len(); i++)
	{
		int32 CodePoint = Text[i];
		const FScanlineFontGlyph* Glyph = Glyphs.Find(CodePoint);
		if (Glyph)
		{
			TotalWidth += Glyph->AdvanceWidth;
		}
	}
	return TotalWidth;
}

void UScanlineFontFace::Clear()
{
	Glyphs.Empty();
	CodePointToIndex.Empty();
	CodePointToTextureIndex.Empty();
	CurveDataTexture = nullptr;
	GlyphIndexTexture = nullptr;
}

#if WITH_EDITOR
void UScanlineFontFace::RebuildTextureData()
{
	UTexture2D* NewCurveDataTexture = nullptr;
	UTexture2D* NewGlyphIndexTexture = nullptr;

	const bool Res = ScanlineFontFaceUtils::CreateFontTextures(this, NewCurveDataTexture, NewGlyphIndexTexture);
	ensure(Res != false);

	CurveDataTexture = NewCurveDataTexture;
	GlyphIndexTexture = NewGlyphIndexTexture;
}

void UScanlineFontFace::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnPropertyChanged.Broadcast(this, PropertyChangedEvent);
}

#if WITH_FREETYPE

bool UScanlineFontFace::ImportFromFontFace(UFontFace* FontFace, float FontSize)
{
	if (!FontFace)
	{
		UE_LOG(LogTemp, Error, TEXT("UScanlineFontFace::ImportFromFontFace - FontFace is null"));
		return false;
	}

#if WITH_EDITORONLY_DATA
	SourceFontFace = FontFace;
#endif

	// Get font face interface
	IFontFaceInterface* FontFaceInterface = Cast<IFontFaceInterface>(FontFace);
	if (!FontFaceInterface)
	{
		UE_LOG(LogTemp, Error, TEXT("UScanlineFontFace::ImportFromFontFace - FontFace does not implement IFontFaceInterface"));
		return false;
	}

	// Get font data
	FFontFaceDataConstRef FontData = FontFaceInterface->GetFontFaceData();
	if (FontData->GetData().Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("UScanlineFontFace::ImportFromFontFace - Font data is empty"));
		return false;
	}

	// Initialize FreeType
	FT_Library Library = nullptr;
	if (FT_Init_FreeType(&Library) != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("UScanlineFontFace::ImportFromFontFace - Failed to initialize FreeType"));
		return false;
	}

	// Load face from memory
	FT_Face Face = nullptr;
	const TArray<uint8>& FontBytes = FontData->GetData();
	FT_Error Error = FT_New_Memory_Face(Library, FontBytes.GetData(), FontBytes.Num(), 0, &Face);

	if (Error != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("UScanlineFontFace::ImportFromFontFace - Failed to load font face (error %d)"), Error);
		FT_Done_FreeType(Library);
		return false;
	}

	// Store font metrics
	FamilyName = FString(Face->family_name);
	StyleName = FString(Face->style_name);
	UnitsPerEm = Face->units_per_EM;
	Ascender = static_cast<float>(Face->ascender) / 64.f;
	Descender = static_cast<float>(Face->descender) / 64.f;
	Height = static_cast<float>(Face->height) / 64.f;
	MaxAdvanceHeight = static_cast<float>(Face->max_advance_height) / 64.f;
	MaxAdvanceWidth = static_cast<float>(Face->max_advance_width) / 64.f;
	LineGap = static_cast<float>(Face->height - Face->ascender + Face->descender);

	// Clear existing glyphs
	Clear();

	TArray<int32> CodePointsToImport;
	for (const TCHAR Char : CharactersToImport)
	{
		CodePointsToImport.AddUnique(static_cast<int32>(Char));
	}

	int32 ImportedCount = 0;
	for (int32 CodePoint : CodePointsToImport)
	{
		FT_UInt GlyphIndex = FT_Get_Char_Index(Face, CodePoint);
		if (GlyphIndex == 0)
		{
			continue; // Glyph not found
		}

		FScanlineFontGlyph NewGlyph;
		NewGlyph.CodePoint = CodePoint;

		if (ScanlineFontFaceUtils::ExtractGlyph(Face, GlyphIndex, NewGlyph))
		{
			//if (!NewGlyph.IsEmpty())
			{
				check(NewGlyph.CodePoint == CodePoint);
				Glyphs.Add(CodePoint, NewGlyph);
				CodePointToIndex.Add(CodePoint, Glyphs.Num() - 1);
				ImportedCount++;
			}
		}
	}

	// Clean up
	FT_Done_Face(Face);
	FT_Done_FreeType(Library);

	UE_LOG(LogTemp, Log, TEXT("UScanlineFontFace::ImportFromFontFace - Imported %d/%d glyphs from '%s %s'"), 
		ImportedCount, CodePointsToImport.Num(), *FamilyName, *StyleName);

	// Build curve data texture
	if (ImportedCount > 0)
	{
		RebuildTextureData();
	}

	return ImportedCount > 0;
}

int32 UScanlineFontFace::ImportCodePointsFromFontFace(UFontFace* FontFace, const TArray<int32>& CodePoints, float FontSize)
{
	if (!FontFace || CodePoints.Num() == 0)
	{
		return 0;
	}

#if WITH_EDITORONLY_DATA
	SourceFontFace = FontFace;
#endif

	// Get font face interface
	IFontFaceInterface* FontFaceInterface = Cast<IFontFaceInterface>(FontFace);
	if (!FontFaceInterface)
	{
		return 0;
	}

	// Get font data
	FFontFaceDataConstRef FontData = FontFaceInterface->GetFontFaceData();
	if (FontData->GetData().Num() == 0)
	{
		return 0;
	}

	// Initialize FreeType
	FT_Library Library = nullptr;
	if (FT_Init_FreeType(&Library) != 0)
	{
		return 0;
	}

	// Load face
	FT_Face Face = nullptr;
	const TArray<uint8>& FontBytes = FontData->GetData();
	FT_Error Error = FT_New_Memory_Face(Library, FontBytes.GetData(), FontBytes.Num(), 0, &Face);

	if (Error != 0)
	{
		FT_Done_FreeType(Library);
		return 0;
	}

	// Import requested code points
	int32 ImportedCount = 0;
	for (int32 CodePoint : CodePoints)
	{
		FT_UInt GlyphIndex = FT_Get_Char_Index(Face, CodePoint);
		if (GlyphIndex == 0)
		{
			continue;
		}

		FScanlineFontGlyph NewGlyph;
		NewGlyph.CodePoint = CodePoint;

		if (ScanlineFontFaceUtils::ExtractGlyph(Face, GlyphIndex, NewGlyph))
		{
			if (!NewGlyph.IsEmpty())
			{
				check(NewGlyph.CodePoint == CodePoint);
				Glyphs.Add(CodePoint, NewGlyph);
				CodePointToIndex.Add(CodePoint, Glyphs.Num() - 1);
				ImportedCount++;
			}
		}
	}

	// Clean up
	FT_Done_Face(Face);
	FT_Done_FreeType(Library);

	// Build curve data texture
	if (ImportedCount > 0)
	{
		RebuildTextureData();
	}

	return ImportedCount;
}

#else // !WITH_FREETYPE

bool UScanlineFontFace::ImportFromFontFace(UFontFace* FontFace, float FontSize)
{
	UE_LOG(LogTemp, Error, TEXT("UScanlineFontFace::ImportFromFontFace - FreeType support not compiled in"));
	return false;
}

int32 UScanlineFontFace::ImportCodePointsFromFontFace(UFontFace* FontFace, const TArray<int32>& CodePoints, float FontSize)
{
	UE_LOG(LogTemp, Error, TEXT("UScanlineFontFace::ImportCodePointsFromFontFace - FreeType support not compiled in"));
	return 0;
}

#endif // WITH_FREETYPE
#endif // WITH_EDITOR


