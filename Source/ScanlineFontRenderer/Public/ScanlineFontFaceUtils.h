// Copyright Nicholas Chalkley. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FScanlineFontGlyph;
class UScanlineFontFace;
class UTexture2D;

#if WITH_EDITOR
#if WITH_FREETYPE
#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_BBOX_H
#include FT_GLYPH_H

namespace ScanlineFontFaceUtils
{
	/**
	 * Extract a glyph from FreeType face and convert to quadratic beziers
	 */
	bool ExtractGlyph(const FT_Face& Face, const FT_UInt& GlyphIndex, FScanlineFontGlyph& OutGlyph);
}

#endif // WITH_FREETYPE

namespace ScanlineFontFaceUtils
{
	TPair<int32, int32> GetTextureWidthHeight(const int32 Num);

	/**
	 * Create a curve data + glyph index texture from a font face.
	 * 
	 * The curve data texture is just a list of quadratic bezier curves. Each 2 pixels is one curve.
	 * The glyph index texture stores the start and end indicies of each glyph's curves in the curve data texture.
	 * The end index is exclusive (i.e. the last curve of a glyph is at index EndIndex - 1).
	 */
	bool CreateFontTextures(UScanlineFontFace* FontFace, UTexture2D*& OutCurveDataTexture, UTexture2D*& OutGlyphIndexTexture);
}

#endif // WITH_EDITOR