#pragma once

#include "AWave_PSP.h"

class AWaveBank_PSP
{
public:
	static constexpr TUINT MAX_WAVES = 16;
	static constexpr TUINT MAX_WAVE_NAME = 48;

public:
	AWaveBank_PSP();

	TBOOL RegisterWave( const TCHAR* a_pName, AWave_PSP* a_pWave );
	AWave_PSP* FindWave( const TCHAR* a_pName ) const;
	TBOOL DeregisterWave( const TCHAR* a_pName );
	void Clear();

	TUINT GetCount() const { return m_uiCount; }
	TUINT GetCapacity() const { return MAX_WAVES; }

private:
	struct Entry
	{
		TCHAR szName[ MAX_WAVE_NAME ];
		AWave_PSP* pWave;
	};

private:
	Entry m_aEntries[ MAX_WAVES ];
	TUINT m_uiCount;
};
