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
{
}

TINT ASoundMixer_PSP::Play(
    const AWave_PSP* a_pWave,
    TUINT a_uiVolume,
    TINT a_iPan,
    TBOOL a_bLoop
)
{
	if ( a_pWave == TNULL || !a_pWave->IsCreated() || a_uiVolume > MAX_VOLUME ||
	     a_iPan < -MAX_PAN || a_iPan > MAX_PAN )
		return INVALID_VOICE;

	const TUINT uiLeftPan = a_iPan > 0 ? TUINT( MAX_PAN - a_iPan ) : TUINT( MAX_PAN );
	const TUINT uiRightPan = a_iPan < 0 ? TUINT( MAX_PAN + a_iPan ) : TUINT( MAX_PAN );
	const TUINT uiLeftVolume = a_uiVolume * uiLeftPan / TUINT( MAX_PAN );
	const TUINT uiRightVolume = a_uiVolume * uiRightPan / TUINT( MAX_PAN );

	for ( TUINT i = 0; i < MAX_VOICES; i++ )
	{
		if ( !m_aVoices[ i ].bActive )
		{
			m_aVoices[ i ] = { a_pWave, 0, uiLeftVolume, uiRightVolume, TTRUE, a_bLoop };
			return TINT( i );
		}
	}

	return INVALID_VOICE;
}

void ASoundMixer_PSP::Stop( TINT a_iVoice )
{
	if ( a_iVoice >= 0 && a_iVoice < TINT( MAX_VOICES ) )
		m_aVoices[ a_iVoice ] = {};
}

void ASoundMixer_PSP::StopAll()
{
	Toshi::TUtil::MemClear( m_aVoices, sizeof( m_aVoices ) );
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

TBOOL ASoundMixer_PSP::IsVoiceActive( TINT a_iVoice ) const
{
	return a_iVoice >= 0 && a_iVoice < TINT( MAX_VOICES ) && m_aVoices[ a_iVoice ].bActive;
}
