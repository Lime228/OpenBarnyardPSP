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
    , m_aOutputSamples{}
{
}

TINT ASoundMixer_PSP::Play( const AWave_PSP* a_pWave, TUINT a_uiVolume )
{
	if ( a_pWave == TNULL || !a_pWave->IsCreated() || a_uiVolume > MAX_VOLUME )
		return INVALID_VOICE;

	for ( TUINT i = 0; i < MAX_VOICES; i++ )
	{
		if ( !m_aVoices[ i ].bActive )
		{
			m_aVoices[ i ] = { a_pWave, 0, a_uiVolume, TTRUE };
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

TBOOL ASoundMixer_PSP::Mix()
{
	TBOOL bMixedVoice = TFALSE;
	Toshi::TUtil::MemClear( m_aOutputSamples, sizeof( m_aOutputSamples ) );

	for ( TUINT uiFrame = 0; uiFrame < AWave_PSP::MAX_SAMPLE_FRAMES; uiFrame++ )
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
				rVoice = {};
				continue;
			}

			const TINT16* pSamples = rVoice.pWave->GetSamples() + rVoice.uiSampleFrame * AWave_PSP::OUTPUT_CHANNELS;
			iLeft += TINT32( pSamples[ 0 ] ) * TINT32( rVoice.uiVolume ) / TINT32( MAX_VOLUME );
			iRight += TINT32( pSamples[ 1 ] ) * TINT32( rVoice.uiVolume ) / TINT32( MAX_VOLUME );
			rVoice.uiSampleFrame++;
			bMixedVoice = TTRUE;
		}

		m_aOutputSamples[ uiFrame * AWave_PSP::OUTPUT_CHANNELS ] = ClampMixedSample_PSP( iLeft );
		m_aOutputSamples[ uiFrame * AWave_PSP::OUTPUT_CHANNELS + 1 ] = ClampMixedSample_PSP( iRight );
	}

	for ( TUINT uiVoice = 0; uiVoice < MAX_VOICES; uiVoice++ )
	{
		Voice& rVoice = m_aVoices[ uiVoice ];
		if ( rVoice.bActive && rVoice.uiSampleFrame >= rVoice.pWave->GetSampleFrames() )
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
