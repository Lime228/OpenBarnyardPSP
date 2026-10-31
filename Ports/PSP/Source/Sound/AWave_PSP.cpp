#include <ToshiPCH.h>
#include "AWave_PSP.h"
#include "ASoundManager_PSP.h"

#include <Toshi/TUtil.h>

AWave_PSP::AWave_PSP()
    : m_oHeader{}
    , m_aSamples{}
{
}

TBOOL AWave_PSP::Create( const void* a_pData, TUINT a_uiDataSize )
{
	Destroy();
	if ( a_pData == TNULL || a_uiDataSize < sizeof( AWaveDataHeader_PSP ) )
		return TFALSE;

	const AWaveDataHeader_PSP* pHeader = static_cast<const AWaveDataHeader_PSP*>( a_pData );
	if ( pHeader->uiMagic != AWaveDataHeader_PSP::MAGIC ||
	     pHeader->uiVersion != AWaveDataHeader_PSP::VERSION ||
	     pHeader->uiSampleRate != ASoundManager_PSP::SAMPLE_RATE ||
	     pHeader->uiChannels != OUTPUT_CHANNELS ||
	     pHeader->uiSampleFrames == 0 || pHeader->uiSampleFrames > MAX_SAMPLE_FRAMES )
	{
		return TFALSE;
	}

	const TSIZE uiSampleBytes = pHeader->uiSampleFrames * OUTPUT_CHANNELS * sizeof( TINT16 );
	if ( a_uiDataSize != sizeof( AWaveDataHeader_PSP ) + uiSampleBytes )
		return TFALSE;

	m_oHeader = *pHeader;
	Toshi::TUtil::MemCopy( m_aSamples, pHeader + 1, uiSampleBytes );
	return TTRUE;
}

void AWave_PSP::Destroy()
{
	m_oHeader = {};
	Toshi::TUtil::MemClear( m_aSamples, sizeof( m_aSamples ) );
}
