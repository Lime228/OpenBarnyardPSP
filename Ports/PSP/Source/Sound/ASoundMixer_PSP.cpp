#include <ToshiPCH.h>
#include "ASoundMixer_PSP.h"

#include <Toshi/TUtil.h>

static TINT16 ClampMixedSample_PSP( TINT32 a_iSample )
{
	if ( a_iSample < -32768 )
		return -32768;
	if ( a_iSample > 32767 )
		return 32767;
	return TINT16( a_iSample );
}

ASoundMixer_PSP::ASoundMixer_PSP()
    : m_aVoices{}
    , m_auiVoiceGenerations{}
{
}

ASoundVoiceHandle_PSP ASoundMixer_PSP::Play(
    const AWave_PSP* a_pWave,
    TUINT a_uiVolume,
    TINT a_iPan,
    TBOOL a_bLoop
)
{
	if ( a_pWave == TNULL || !a_pWave->IsCreated() || a_uiVolume > MAX_VOLUME ||
	     a_iPan < -MAX_PAN || a_iPan > MAX_PAN )
		return INVALID_VOICE;

	for ( TUINT i = 0; i < MAX_VOICES; i++ )
	{
		if ( !m_aVoices[ i ].bActive )
		{
			TUINT32 uiGeneration = ( m_auiVoiceGenerations[ i ] + 1 ) & 0x00ffffff;
			if ( uiGeneration == 0 )
				uiGeneration = 1;
			m_auiVoiceGenerations[ i ] = uiGeneration;
			m_aVoices[ i ] = { a_pWave, 0, a_uiVolume, a_iPan, 0, 0, TTRUE, a_bLoop };
			UpdateVoiceVolumes( m_aVoices[ i ] );
			return ( uiGeneration << 8 ) | i;
		}
	}

	return INVALID_VOICE;
}

TBOOL ASoundMixer_PSP::Stop( ASoundVoiceHandle_PSP a_hVoice )
{
	const TINT iVoice = ResolveVoice( a_hVoice );
	if ( iVoice < 0 )
		return TFALSE;

	m_aVoices[ iVoice ] = {};
	return TTRUE;
}

void ASoundMixer_PSP::StopAll()
{
	Toshi::TUtil::MemClear( m_aVoices, sizeof( m_aVoices ) );
}

TBOOL ASoundMixer_PSP::SetVolume( ASoundVoiceHandle_PSP a_hVoice, TUINT a_uiVolume )
{
	const TINT iVoice = ResolveVoice( a_hVoice );
	if ( iVoice < 0 || a_uiVolume > MAX_VOLUME )
		return TFALSE;

	m_aVoices[ iVoice ].uiVolume = a_uiVolume;
	UpdateVoiceVolumes( m_aVoices[ iVoice ] );
	return TTRUE;
}

TBOOL ASoundMixer_PSP::SetPan( ASoundVoiceHandle_PSP a_hVoice, TINT a_iPan )
{
	const TINT iVoice = ResolveVoice( a_hVoice );
	if ( iVoice < 0 || a_iPan < -MAX_PAN || a_iPan > MAX_PAN )
		return TFALSE;

	m_aVoices[ iVoice ].iPan = a_iPan;
	UpdateVoiceVolumes( m_aVoices[ iVoice ] );
	return TTRUE;
}

TBOOL ASoundMixer_PSP::Mix( TINT16* a_pOutputSamples, TUINT a_uiSampleFrames )
{
	if ( a_pOutputSamples == TNULL || a_uiSampleFrames != AWave_PSP::MAX_SAMPLE_FRAMES )
		return TFALSE;

	TBOOL bMixedVoice = TFALSE;
	Toshi::TUtil::MemClear(
	    a_pOutputSamples,
	    a_uiSampleFrames * AWave_PSP::OUTPUT_CHANNELS * sizeof( TINT16 )
	);

	for ( TUINT uiFrame = 0; uiFrame < a_uiSampleFrames; uiFrame++ )
	{
		TINT32 iLeft = 0;
		TINT32 iRight = 0;
		for ( TUINT uiVoice = 0; uiVoice < MAX_VOICES; uiVoice++ )
		{
			Voice& rVoice = m_aVoices[ uiVoice ];
			if ( !rVoice.bActive )
				continue;

			if ( rVoice.uiSampleFrame >= rVoice.pWave->GetSampleFrames() )
			{
				if ( rVoice.bLoop )
					rVoice.uiSampleFrame = 0;
				else
				{
					rVoice = {};
					continue;
				}
			}

			const TINT16* pSamples = rVoice.pWave->GetSamples() + rVoice.uiSampleFrame * AWave_PSP::OUTPUT_CHANNELS;
			iLeft += TINT32( pSamples[ 0 ] ) * TINT32( rVoice.uiLeftVolume ) / TINT32( MAX_VOLUME );
			iRight += TINT32( pSamples[ 1 ] ) * TINT32( rVoice.uiRightVolume ) / TINT32( MAX_VOLUME );
			rVoice.uiSampleFrame++;
			bMixedVoice = TTRUE;
		}

		a_pOutputSamples[ uiFrame * AWave_PSP::OUTPUT_CHANNELS ] = ClampMixedSample_PSP( iLeft );
		a_pOutputSamples[ uiFrame * AWave_PSP::OUTPUT_CHANNELS + 1 ] = ClampMixedSample_PSP( iRight );
	}

	for ( TUINT uiVoice = 0; uiVoice < MAX_VOICES; uiVoice++ )
	{
		Voice& rVoice = m_aVoices[ uiVoice ];
		if ( rVoice.bActive && !rVoice.bLoop && rVoice.uiSampleFrame >= rVoice.pWave->GetSampleFrames() )
			rVoice = {};
	}

	return bMixedVoice;
}

TUINT ASoundMixer_PSP::GetActiveVoiceCount() const
{
	TUINT uiCount = 0;
	for ( TUINT i = 0; i < MAX_VOICES; i++ )
	{
		if ( m_aVoices[ i ].bActive )
			uiCount++;
	}
	return uiCount;
}

TBOOL ASoundMixer_PSP::IsVoiceActive( ASoundVoiceHandle_PSP a_hVoice ) const
{
	return ResolveVoice( a_hVoice ) >= 0;
}

TINT ASoundMixer_PSP::ResolveVoice( ASoundVoiceHandle_PSP a_hVoice ) const
{
	if ( a_hVoice == INVALID_VOICE )
		return -1;

	const TUINT uiVoice = a_hVoice & 0xff;
	const TUINT32 uiGeneration = a_hVoice >> 8;
	return uiVoice < MAX_VOICES && m_aVoices[ uiVoice ].bActive &&
	       m_auiVoiceGenerations[ uiVoice ] == uiGeneration ? TINT( uiVoice ) : -1;
}

void ASoundMixer_PSP::UpdateVoiceVolumes( Voice& a_rVoice )
{
	const TUINT uiLeftPan = a_rVoice.iPan > 0 ? TUINT( MAX_PAN - a_rVoice.iPan ) : TUINT( MAX_PAN );
	const TUINT uiRightPan = a_rVoice.iPan < 0 ? TUINT( MAX_PAN + a_rVoice.iPan ) : TUINT( MAX_PAN );
	a_rVoice.uiLeftVolume = a_rVoice.uiVolume * uiLeftPan / TUINT( MAX_PAN );
	a_rVoice.uiRightVolume = a_rVoice.uiVolume * uiRightPan / TUINT( MAX_PAN );
}
