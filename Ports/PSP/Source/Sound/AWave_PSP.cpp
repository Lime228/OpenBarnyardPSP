#include <ToshiPCH.h>
#include "AWave_PSP.h"
#include "ASoundManager_PSP.h"

#include <File/TFile.h>
#include <Toshi/T2Allocator.h>
#include <Toshi/TUtil.h>

AWave_PSP::AWave_PSP()
    : m_oHeader{}
    , m_pSamples( TNULL )
{
}

AWave_PSP::~AWave_PSP()
{
	Destroy();
}

TBOOL AWave_PSP::Create( const void* a_pData, TUINT a_uiDataSize )
{
	Destroy();
	if ( a_pData == TNULL || a_uiDataSize < sizeof( AWaveDataHeader_PSP ) )
		return TFALSE;

	AWaveDataHeader_PSP oHeader;
	Toshi::TUtil::MemCopy( &oHeader, a_pData, sizeof( oHeader ) );
	if ( !AllocateSamples( oHeader, a_uiDataSize ) )
		return TFALSE;

	Toshi::TUtil::MemCopy( m_pSamples, TSTATICCAST( const TUINT8*, a_pData ) + sizeof( oHeader ),
	                       a_uiDataSize - sizeof( oHeader ) );
	return TTRUE;
}

TBOOL AWave_PSP::Create( Toshi::TFile& a_rFile )
{
	Destroy();
	AWaveDataHeader_PSP oHeader;
	const TSIZE uiFileSize = a_rFile.GetSize();
	if ( uiFileSize < sizeof( oHeader ) || a_rFile.Read( &oHeader, sizeof( oHeader ) ) != sizeof( oHeader ) ||
	     !AllocateSamples( oHeader, uiFileSize ) )
		return TFALSE;

	const TSIZE uiSampleBytes = uiFileSize - sizeof( oHeader );
	if ( a_rFile.Read( m_pSamples, uiSampleBytes ) == uiSampleBytes )
		return TTRUE;

	Destroy();
	return TFALSE;
}

TBOOL AWave_PSP::AllocateSamples( const AWaveDataHeader_PSP& a_rcHeader, TSIZE a_uiDataSize )
{
	if ( a_rcHeader.uiMagic != AWaveDataHeader_PSP::MAGIC ||
	     a_rcHeader.uiVersion != AWaveDataHeader_PSP::VERSION ||
	     a_rcHeader.uiSampleRate != ASoundManager_PSP::SAMPLE_RATE ||
	     a_rcHeader.uiChannels != OUTPUT_CHANNELS ||
	     a_rcHeader.uiSampleFrames == 0 || a_rcHeader.uiSampleFrames > MAX_SAMPLE_FRAMES )
	{
		return TFALSE;
	}

	const TSIZE uiSampleBytes = a_rcHeader.uiSampleFrames * OUTPUT_CHANNELS * sizeof( TINT16 );
	if ( a_uiDataSize != sizeof( a_rcHeader ) + uiSampleBytes )
		return TFALSE;

	m_pSamples = TSTATICCAST( TINT16*, Toshi::GetGlobalAllocator()->Malloc( uiSampleBytes, 64 ) );
	if ( m_pSamples == TNULL )
		return TFALSE;

	m_oHeader = a_rcHeader;
	return TTRUE;
}

void AWave_PSP::Destroy()
{
	if ( m_pSamples != TNULL )
		Toshi::GetGlobalAllocator()->Free( m_pSamples );

	m_pSamples = TNULL;
	m_oHeader = {};
}
