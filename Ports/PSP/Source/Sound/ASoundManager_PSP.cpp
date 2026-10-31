#include <ToshiPCH.h>
#include "ASoundManager_PSP.h"

#include <Toshi/TUtil.h>

#include <pspaudio.h>

ASoundManager_PSP::ASoundManager_PSP()
    : m_aOutputBuffers{}
    , m_iChannel( -1 )
    , m_uiOutputBuffer( 0 )
{
}

ASoundManager_PSP::~ASoundManager_PSP()
{
	Destroy();
}

TBOOL ASoundManager_PSP::Create()
{
	Destroy();
	m_iChannel = sceAudioChReserve(
	    PSP_AUDIO_NEXT_CHANNEL,
	    BUFFER_SAMPLE_FRAMES,
	    PSP_AUDIO_FORMAT_STEREO
	);
	m_uiOutputBuffer = 0;
	return m_iChannel >= 0;
}

void ASoundManager_PSP::Destroy()
{
	if ( m_iChannel >= 0 )
		sceAudioChRelease( m_iChannel );

	m_iChannel = -1;
	m_uiOutputBuffer = 0;
}

TBOOL ASoundManager_PSP::SubmitStereo( const TINT16* a_pSamples, TUINT a_uiSampleFrames )
{
	if ( !IsCreated() || a_pSamples == TNULL || a_uiSampleFrames != BUFFER_SAMPLE_FRAMES )
		return TFALSE;

	TINT16* pOutput = m_aOutputBuffers[ m_uiOutputBuffer ];
	Toshi::TUtil::MemCopy(
	    pOutput,
	    a_pSamples,
	    BUFFER_SAMPLE_FRAMES * OUTPUT_CHANNELS * sizeof( TINT16 )
	);
	m_uiOutputBuffer = ( m_uiOutputBuffer + 1 ) % TARRAYSIZE( m_aOutputBuffers );
	return sceAudioOutputBlocking( m_iChannel, PSP_AUDIO_VOLUME_MAX, pOutput ) >= 0;
}

TBOOL ASoundManager_PSP::SubmitSilence()
{
	if ( !IsCreated() )
		return TFALSE;

	TINT16* pOutput = m_aOutputBuffers[ m_uiOutputBuffer ];
	Toshi::TUtil::MemClear(
	    pOutput,
	    BUFFER_SAMPLE_FRAMES * OUTPUT_CHANNELS * sizeof( TINT16 )
	);
	m_uiOutputBuffer = ( m_uiOutputBuffer + 1 ) % TARRAYSIZE( m_aOutputBuffers );
	return sceAudioOutputBlocking( m_iChannel, PSP_AUDIO_VOLUME_MAX, pOutput ) >= 0;
}
