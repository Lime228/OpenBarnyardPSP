#pragma once

#include <Math/TVector4.h>

struct ASimStart_PSP
{
	Toshi::TVector4 vPosition;
	Toshi::TVector4 vFacing;
	TINT iArea;
	TFLOAT fGroundY;
};

class ATerrainCollision_PSP;
TBOOL AReadSimGoal_PSP( ASimStart_PSP& a_rStart, const TCHAR* a_szGoal );
TBOOL AReadSimStart_PSP( ASimStart_PSP& a_rStart, ATerrainCollision_PSP& a_rTerrain );
