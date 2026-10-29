#include "ToshiPCH.h"
#include "TRenderMemory_PSP.h"

TOSHI_NAMESPACE_START

TSIZE TRenderMemory_PSP::s_uiMainCurrent  = 0;
TSIZE TRenderMemory_PSP::s_uiMainPeak     = 0;
TSIZE TRenderMemory_PSP::s_uiEDRAMCurrent = 0;

TBOOL TRenderMemory_PSP::ReserveMain( TSIZE a_uiSize )
{
	if ( a_uiSize > MAIN_BUDGET - s_uiMainCurrent )
		return TFALSE;

	s_uiMainCurrent += a_uiSize;
	if ( s_uiMainPeak < s_uiMainCurrent )
		s_uiMainPeak = s_uiMainCurrent;
	return TTRUE;
}

void TRenderMemory_PSP::ReleaseMain( TSIZE a_uiSize )
{
	TASSERT( a_uiSize <= s_uiMainCurrent );
	if ( a_uiSize > s_uiMainCurrent )
		s_uiMainCurrent = 0;
	else
		s_uiMainCurrent -= a_uiSize;
}

TBOOL TRenderMemory_PSP::SetEDRAMUsage( TSIZE a_uiSize )
{
	if ( a_uiSize > EDRAM_BUDGET )
		return TFALSE;

	s_uiEDRAMCurrent = a_uiSize;
	return TTRUE;
}

TRenderMemoryStats_PSP TRenderMemory_PSP::GetStats()
{
	return {
	    s_uiMainCurrent,
	    s_uiMainPeak,
	    MAIN_BUDGET,
	    s_uiEDRAMCurrent,
	    EDRAM_BUDGET,
	};
}

TOSHI_NAMESPACE_END
