#pragma once

#include "AWave_PSP.h"
#include "AStreamWave_PSP.h"

enum AWaveBankEntryType_PSP : TUINT32
{
	AWaveBankEntryType_PSP_Resident = 0,
	AWaveBankEntryType_PSP_Stream = 1,
};

class AWaveBank_PSP
{
public:
	static constexpr TUINT MAX_WAVES = 16;
	static constexpr TUINT MAX_WAVE_NAME = 48;

public:
	AWaveBank_PSP();

	TBOOL RegisterWave( const TCHAR* a_pName, AWave_PSP* a_pWave );
	TBOOL RegisterStream( const TCHAR* a_pName, AStreamWave_PSP* a_pStream );
	AWave_PSP* FindWave( const TCHAR* a_pName ) const;
	AStreamWave_PSP* FindStream( const TCHAR* a_pName ) const;
	TBOOL DeregisterWave( const TCHAR* a_pName );
	void Clear();

	TUINT GetCount() const { return m_uiCount; }
	TUINT GetCapacity() const { return MAX_WAVES; }

private:
	struct Entry
	{
		TCHAR szName[ MAX_WAVE_NAME ];
		AWaveBankEntryType_PSP eType;
		AWave_PSP* pWave;
		AStreamWave_PSP* pStream;
	};

	TBOOL CanRegister( const TCHAR* a_pName ) const;

private:
	Entry m_aEntries[ MAX_WAVES ];
	TUINT m_uiCount;
};
