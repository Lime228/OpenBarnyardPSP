#include "pch.h"

#include "GUI/AGUIPicture.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TDEFINE_CLASS_INIT_NORUNTIME( AGUIPicture );

AGUIPicture::AGUIPicture()
{
	m_iUnknown1   = 0;
	m_iNumRects1  = 0;
	m_iNumRects2  = 0;
	m_pRectangles = TNULL;
	m_pImages     = TNULL;
	m_bFlag       = TFALSE;
	m_bLoaded     = TFALSE;
	m_bIsVisible  = TTRUE;
	m_fUnknown3   = -10.0f;
}

AGUIPicture::~AGUIPicture()
{
	m_iUnknown1 = -1;
}

void AGUIPicture::InitialiseClass()
{
	const TFLOAT aColumnWidths_PSP[ 3 ] = { 256.0f, 256.0f, 128.0f };
	const TFLOAT aRowHeights_PSP[ 2 ]   = { 256.0f, 224.0f };
	TFLOAT fY = -240.0f;

	for ( TINT iRow = 0; iRow < 2; iRow++ )
	{
		TFLOAT fX = -320.0f;
		for ( TINT iColumn = 0; iColumn < 3; iColumn++ )
		{
			SplitRule& rRule = split_rules[ iRow * 3 + iColumn ];
			rRule.Pos1        = { fX, fX + aColumnWidths_PSP[ iColumn ] };
			rRule.Pos2        = { fY, fY + aRowHeights_PSP[ iRow ] };
			rRule.UV1         = { 0.0f, 1.0f };
			rRule.UV2         = { 0.0f, aRowHeights_PSP[ iRow ] / 256.0f };
			rRule.iImageIndex = iRow * 3 + iColumn;
			rRule.iUnk1       = 0;
			rRule.iUnk2       = 0;
			fX += aColumnWidths_PSP[ iColumn ];
		}
		fY += aRowHeights_PSP[ iRow ];
	}
}

void AGUIPicture::DeinitialiseClass()
{
}

void AGUIPicture::Delete()
{
	Flush();
	delete this;
}

void AGUIPicture::SetFlag()
{
	m_bFlag = TTRUE;
}

void AGUIPicture::Render()
{
}
