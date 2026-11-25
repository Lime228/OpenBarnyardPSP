#pragma once

#include <Math/TVector4.h>

struct ASimStart_PSP
{
	Toshi::TVector4 vPosition;
	Toshi::TVector4 vFacing;
	TINT iArea;
	TFLOAT fGroundY;
};

TBOOL AReadSimStart_PSP( ASimStart_PSP& a_rStart, TUINT& a_ruiPacks, TUINT& a_ruiTriangles );
