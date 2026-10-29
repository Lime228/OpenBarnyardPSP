#pragma once

#include "Toshi/Defines.h"

TOSHI_NAMESPACE_START

struct TRenderMemoryStats_PSP
{
	TSIZE uiMainCurrent;
	TSIZE uiMainPeak;
	TSIZE uiMainBudget;
	TSIZE uiEDRAMCurrent;
	TSIZE uiEDRAMBudget;
};

class TRenderMemory_PSP
{
public:
	static constexpr TSIZE MAIN_BUDGET  = 16 * 1024 * 1024;
	static constexpr TSIZE EDRAM_BUDGET = 2 * 1024 * 1024;

public:
	static TBOOL ReserveMain( TSIZE a_uiSize );
	static void  ReleaseMain( TSIZE a_uiSize );
	static TBOOL SetEDRAMUsage( TSIZE a_uiSize );
	static TRenderMemoryStats_PSP GetStats();

private:
	static TSIZE s_uiMainCurrent;
	static TSIZE s_uiMainPeak;
	static TSIZE s_uiEDRAMCurrent;
};

TOSHI_NAMESPACE_END
