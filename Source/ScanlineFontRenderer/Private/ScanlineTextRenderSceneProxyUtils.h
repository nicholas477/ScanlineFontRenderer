// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScanlineFontFace.h"
#include "ScanlineTextRenderSceneProxy.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

UE_DISABLE_OPTIMIZATION

namespace
{
	// Text parsing helper - handles <br> line breaks
	struct FTextIterator
	{
		const TCHAR* SourceString;
		const TCHAR* CurrentPosition;

		FTextIterator(const TCHAR* InSourceString)
			: SourceString(InSourceString)
			, CurrentPosition(InSourceString)
		{
			check(InSourceString);
		}

		bool NextLine()
		{
			check(CurrentPosition);
			return (CurrentPosition[0] != '\0');
		}

		bool NextCharacterInLine(TCHAR& Ch)
		{
			bool bRet = false;
			check(CurrentPosition);

			if (CurrentPosition[0] == '\0')
			{
				// Leave current position on the null terminator
			}
			else if (CurrentPosition[0] == '<' && CurrentPosition[1] == 'b' && CurrentPosition[2] == 'r' && CurrentPosition[3] == '>')
			{
				CurrentPosition += 4;
			}
			else if (CurrentPosition[0] == '\n')
			{
				++CurrentPosition;
			}
			else
			{
				Ch = *CurrentPosition;
				CurrentPosition++;
				bRet = true;
			}

			return bRet;
		}

		bool Peek(TCHAR& Ch)
		{
			check(CurrentPosition);
			if (CurrentPosition[0] == '\0' ||
				(CurrentPosition[0] == '<' && CurrentPosition[1] == 'b' && CurrentPosition[2] == 'r' && CurrentPosition[3] == '>') ||
				(CurrentPosition[0] == '\n'))
			{
				return false;
			}

			Ch = *CurrentPosition;
			return true;
		}
	};

	// @param It must be a valid initialized text iterator
	// @param Font 0 is silently ignored
	FVector2D ComputeTextSize(FTextIterator It, class UScanlineFontFace* Font,
		float XScale, float YScale, float HorizSpacingAdjust, float VertSpacingAdjust)
	{
		FVector2D Ret(0.f, 0.f);

		if (!Font)
		{
			return Ret;
		}

		const float CharIncrement = HorizSpacingAdjust * XScale;

		float LineX = 0.f;

		TCHAR Ch = 0;
		while (It.NextCharacterInLine(Ch))
		{
			FScanlineFontGlyph Glyph;
			if (!Font->GetGlyph(Ch, Glyph))
			{
				continue;
			}

			const float X = LineX;
			//const float Y = Glyph.VerticalOffset * YScale;
			float SizeX = Glyph.AdvanceWidth * XScale;
			const float SizeY = (Font->GetLineHeight() + VertSpacingAdjust) * YScale;//(Glyph.BoundingBox.GetSize().Y + VertSpacingAdjust)* YScale;
			//const float U = Char.StartU * InvTextureSize.X;
			//const float V = Char.StartV * InvTextureSize.Y;
			//const float SizeU = Char.USize * InvTextureSize.X;
			//const float SizeV = Char.VSize * InvTextureSize.Y;

			const float Right = X + SizeX;
			const float Bottom = SizeY;

			Ret.X = FMath::Max(Ret.X, Right);
			Ret.Y = FMath::Max(Ret.Y, Bottom);

			// if we have another non-whitespace character to render, add the font's kerning.
			TCHAR NextCh = 0;
			if (It.Peek(NextCh) && !FChar::IsWhitespace(NextCh))
			{
				SizeX += CharIncrement;
			}

			LineX += SizeX;
		}

		return Ret;
	}

	// compute the left top depending on the alignment
	static float ComputeHorizontalAlignmentOffset(FVector2D Size, EHorizTextAligment HorizontalAlignment)
	{
		float Ret = 0.f;

		switch (HorizontalAlignment)
		{
		case EHTA_Left:
		{
			// X is already 0
			break;
		}

		case EHTA_Center:
		{
			Ret = -Size.X * 0.5f;
			break;
		}

		case EHTA_Right:
		{
			Ret = -Size.X;
			break;
		}

		default:
		{
			// internal error
			check(0);
		}
		}

		return Ret;
	}

	float ComputeVerticalAlignmentOffset(float SizeY, EVerticalTextAligment VerticalAlignment)
	{
		switch (VerticalAlignment)
		{
		case EVRTA_TextBottom:
		{
			return -SizeY;
		}
		case EVRTA_QuadTop:
		case EVRTA_TextTop:
		{
			return 0.f;
		}
		case EVRTA_TextCenter:
		{
			return (-SizeY / 2.0f);
		}
		default:
		{
			check(0);
			return 0.f;
		}
		}
	}

	///**
	// * For the given text info, calculate the vertical offset that needs to be applied to the component
	// * in order to vertically align it to the requested alignment.
	// **/
	//float CalculateVerticalAlignmentOffset(
	//	const TCHAR* Text,
	//	class UScanlineFontFace* Font,
	//	float XScale,
	//	float YScale,
	//	float HorizSpacingAdjust,
	//	float VertSpacingAdjust,
	//	EVerticalTextAligment VerticalAlignment)
	//{
	//	check(Text);

	//	if (!Font)
	//	{
	//		return 0.f;
	//	}

	//	float FirstLineHeight = -1.f; // Only kept around for legacy positioning support
	//	float StartY = 0.f;

	//	FTextIterator It(Text);

	//	while (It.NextLine())
	//	{
	//		FVector2D LineSize = ComputeTextSize(It, Font, XScale, YScale, HorizSpacingAdjust, VertSpacingAdjust);

	//		if (FirstLineHeight < 0.f)
	//		{
	//			FirstLineHeight = LineSize.Y;
	//		}

	//		// Iterate to end of line
	//		TCHAR Ch = 0;
	//		while (It.NextCharacterInLine(Ch)) {}

	//		// Move Y position down to next line. If the current line is empty, move by max char height in font
	//		StartY += LineSize.Y > 0.f ? LineSize.Y : Font->GetLineHeight();
	//	}

	//	// Calculate a vertical translation to create the correct vertical alignment
	//	return -ComputeVerticalAlignmentOffset(StartY, VerticalAlignment);
	//}
}

UE_ENABLE_OPTIMIZATION
