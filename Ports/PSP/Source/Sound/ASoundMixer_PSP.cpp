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

static TUINT AbsPositionComponent_PSP( TINT a_iValue )
{
	return a_iValue < 0 ? TUINT( -a_iValue ) : TUINT( a_iValue );
}

ASoundMixer_PSP::ASoundMixer_PSP()
    : m_aVoices{}
    , m_auiVoiceGenerations{}
    , m_oListener{ 0, 0, 0, LISTENER_AXIS_SCALE, 0, 0, LISTENER_AXIS_SCALE }
{
}

ASoundVoiceHandle_PSP ASoundMixer_PSP::Play(
    const AWave_PSP* a_pWave,
    TUINT a_uiVolume,
    TINT a_iPan,
    TBOOL a_bLoop,
    TUINT a_uiFrequency
)
{
	if ( a_pWave == TNULL || !a_pWave->IsCreated() || a_uiVolume > MAX_VOLUME ||
	     a_iPan < -MAX_PAN || a_iPan > MAX_PAN ||
	     a_uiFrequency < MIN_FREQUENCY || a_uiFrequency > MAX_FREQUENCY )
		return INVALID_VOICE;

	for ( TUINT i = 0; i < MAX_VOICES; i++ )
	{
		if ( !m_aVoices[ i ].bActive )
		{
			TUINT32 uiGeneration = ( m_auiVoiceGenerations[ i ] + 1 ) & 0x00ffffff;
			if ( uiGeneration == 0 )
				uiGeneration = 1;
			m_auiVoiceGenerations[ i ] = uiGeneration;
			m_aVoices[ i ] = { a_pWave, TNULL, 0, 0, a_uiFrequency, a_uiVolume, MAX_VOLUME, a_iPan, 0, 0, TTRUE, a_bLoop, TFALSE };
			UpdateVoiceFrequency( m_aVoices[ i ], a_uiFrequency );
			UpdateVoiceVolumes( m_aVoices[ i ] );
			return ( uiGeneration << 8 ) | i;
		}
	}

	return INVALID_VOICE;
}

ASoundVoiceHandle_PSP ASoundMixer_PSP::PlayStream(
    AStreamWave_PSP* a_pStreamWave,
    TUINT a_uiVolume,
    TINT a_iPan,
    TBOOL a_bLoop
)
{
	if ( a_pStreamWave == TNULL || !a_pStreamWave->IsCreated() ||
	     a_uiVolume > MAX_VOLUME || a_iPan < -MAX_PAN || a_iPan > MAX_PAN ||
	     ( a_bLoop && !a_pStreamWave->IsLoopAligned() ) )
	{
		return INVALID_VOICE;
	}

	for ( TUINT i = 0; i < MAX_VOICES; i++ )
	{
		if ( m_aVoices[ i ].bActive && m_aVoices[ i ].pStreamWave == a_pStreamWave )
			return INVALID_VOICE;
	}

	for ( TUINT i = 0; i < MAX_VOICES; i++ )
	{
		if ( !m_aVoices[ i ].bActive )
		{
			TUINT32 uiGeneration = ( m_auiVoiceGenerations[ i ] + 1 ) & 0x00ffffff;
			if ( uiGeneration == 0 ) uiGeneration = 1;
			m_auiVoiceGenerations[ i ] = uiGeneration;
			m_aVoices[ i ] = {
				TNULL, a_pStreamWave, 0, 1 << 16, DEFAULT_FREQUENCY,
				a_uiVolume, MAX_VOLUME, a_iPan, 0, 0, TTRUE, a_bLoop, TFALSE,
			};
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

TBOOL ASoundMixer_PSP::SetFrequency( ASoundVoiceHandle_PSP a_hVoice, TUINT a_uiFrequency )
{
	const TINT iVoice = ResolveVoice( a_hVoice );
	if ( iVoice < 0 ||
	     ( m_aVoices[ iVoice ].pStreamWave != TNULL && a_uiFrequency != DEFAULT_FREQUENCY ) )
		return TFALSE;

	return UpdateVoiceFrequency( m_aVoices[ iVoice ], a_uiFrequency );
}

TBOOL ASoundMixer_PSP::SetPaused( ASoundVoiceHandle_PSP a_hVoice, TBOOL a_bPaused )
{
	const TINT iVoice = ResolveVoice( a_hVoice );
	if ( iVoice < 0 )
		return TFALSE;

	m_aVoices[ iVoice ].bPaused = a_bPaused;
	return TTRUE;
}

TBOOL ASoundMixer_PSP::SetPosition(
    ASoundVoiceHandle_PSP a_hVoice,
    const ASoundPosition_PSP& a_rPosition
)
{
	const TINT iVoice = ResolveVoice( a_hVoice );
	if ( iVoice < 0 || a_rPosition.uiMaxDistance == 0 ||
	     a_rPosition.uiMaxDistance > TUINT( MAX_POSITION ) ||
	     a_rPosition.iX < -MAX_POSITION || a_rPosition.iX > MAX_POSITION ||
	     a_rPosition.iY < -MAX_POSITION || a_rPosition.iY > MAX_POSITION ||
	     a_rPosition.iZ < -MAX_POSITION || a_rPosition.iZ > MAX_POSITION )
	{
		return TFALSE;
	}

	Voice& rVoice = m_aVoices[ iVoice ];
	const TUINT uiX = AbsPositionComponent_PSP( a_rPosition.iX );
	const TUINT uiY = AbsPositionComponent_PSP( a_rPosition.iY );
	const TUINT uiZ = AbsPositionComponent_PSP( a_rPosition.iZ );
	const TUINT uiHorizontalMax = uiX > uiZ ? uiX : uiZ;
	const TUINT uiHorizontalMin = uiX > uiZ ? uiZ : uiX;
	const TUINT uiDistance = uiHorizontalMax + uiHorizontalMin / 2 + uiY / 2;
	const TINT64 iPan = TINT64( a_rPosition.iX ) * TINT64( MAX_PAN ) /
	                    TINT64( a_rPosition.uiMaxDistance );
	rVoice.iPan = iPan < -MAX_PAN ? -MAX_PAN : iPan > MAX_PAN ? MAX_PAN : TINT( iPan );
	rVoice.uiAttenuation = uiDistance >= a_rPosition.uiMaxDistance ? 0 :
	                       ( a_rPosition.uiMaxDistance - uiDistance ) * MAX_VOLUME /
	                           a_rPosition.uiMaxDistance;
	UpdateVoiceVolumes( rVoice );
	return TTRUE;
}

TBOOL ASoundMixer_PSP::SetListener( const ASoundListener_PSP& a_rListener )
{
	if ( !IsListenerValid( a_rListener ) )
		return TFALSE;

	m_oListener = a_rListener;
	return TTRUE;
}

TBOOL ASoundMixer_PSP::SetWorldPosition(
    ASoundVoiceHandle_PSP a_hVoice,
    const ASoundPosition_PSP& a_rPosition
)
{
	if ( a_rPosition.iX < -MAX_POSITION || a_rPosition.iX > MAX_POSITION ||
	     a_rPosition.iY < -MAX_POSITION || a_rPosition.iY > MAX_POSITION ||
	     a_rPosition.iZ < -MAX_POSITION || a_rPosition.iZ > MAX_POSITION )
	{
		return TFALSE;
	}

	const TINT64 iRelativeX = TINT64( a_rPosition.iX ) - TINT64( m_oListener.iX );
	const TINT64 iRelativeY = TINT64( a_rPosition.iY ) - TINT64( m_oListener.iY );
	const TINT64 iRelativeZ = TINT64( a_rPosition.iZ ) - TINT64( m_oListener.iZ );
	const TINT64 iLocalX = ( iRelativeX * m_oListener.iRightX +
	                         iRelativeZ * m_oListener.iRightZ ) / LISTENER_AXIS_SCALE;
	const TINT64 iLocalZ = ( iRelativeX * m_oListener.iForwardX +
	                         iRelativeZ * m_oListener.iForwardZ ) / LISTENER_AXIS_SCALE;
	if ( iLocalX < -MAX_POSITION || iLocalX > MAX_POSITION ||
	     iRelativeY < -MAX_POSITION || iRelativeY > MAX_POSITION ||
	     iLocalZ < -MAX_POSITION || iLocalZ > MAX_POSITION )
	{
		return TFALSE;
	}

	const ASoundPosition_PSP oListenerPosition = {
		TINT( iLocalX ),
		TINT( iRelativeY ),
		TINT( iLocalZ ),
		a_rPosition.uiMaxDistance,
	};
	return SetPosition( a_hVoice, oListenerPosition );
}

TBOOL ASoundMixer_PSP::GetSpatialState(
    ASoundVoiceHandle_PSP a_hVoice,
    TINT& a_rPan,
    TUINT& a_rAttenuation
) const
{
	const TINT iVoice = ResolveVoice( a_hVoice );
	if ( iVoice < 0 )
		return TFALSE;

	a_rPan = m_aVoices[ iVoice ].iPan;
	a_rAttenuation = m_aVoices[ iVoice ].uiAttenuation;
	return TTRUE;
}

TBOOL ASoundMixer_PSP::GetFrequency(
    ASoundVoiceHandle_PSP a_hVoice,
    TUINT& a_rFrequency
) const
{
	const TINT iVoice = ResolveVoice( a_hVoice );
	if ( iVoice < 0 )
		return TFALSE;

	a_rFrequency = m_aVoices[ iVoice ].uiFrequency;
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

	for ( TUINT uiVoice = 0; uiVoice < MAX_VOICES; uiVoice++ )
	{
		Voice& rVoice = m_aVoices[ uiVoice ];
		if ( rVoice.bActive && rVoice.pStreamWave != TNULL )
		{
			const TUINT64 uiLoopStart =
			    TUINT64( rVoice.pStreamWave->GetLoopStartFrame() ) << 16;
			const TUINT64 uiLoopEnd =
			    TUINT64( rVoice.pStreamWave->GetLoopEndFrame() ) << 16;
			const TUINT64 uiStreamEnd =
			    TUINT64( rVoice.pStreamWave->GetSampleFrames() ) << 16;
			if ( rVoice.bLoop && rVoice.uiSamplePosition >= uiLoopEnd )
			{
				const TUINT64 uiLoopLength = uiLoopEnd - uiLoopStart;
				rVoice.uiSamplePosition = uiLoopStart +
				    ( rVoice.uiSamplePosition - uiLoopEnd ) % uiLoopLength;
			}
			else if ( !rVoice.bLoop && rVoice.uiSamplePosition >= uiStreamEnd )
			{
				rVoice = {};
				continue;
			}

			const TUINT uiSampleFrame = TUINT( rVoice.uiSamplePosition >> 16 );
			if ( !rVoice.pStreamWave->Prepare( uiSampleFrame, rVoice.bLoop ) )
				rVoice = {};
		}
	}

	for ( TUINT uiFrame = 0; uiFrame < a_uiSampleFrames; uiFrame++ )
	{
		TINT32 iLeft = 0;
		TINT32 iRight = 0;
		for ( TUINT uiVoice = 0; uiVoice < MAX_VOICES; uiVoice++ )
		{
			Voice& rVoice = m_aVoices[ uiVoice ];
			if ( !rVoice.bActive || rVoice.bPaused )
				continue;

			const TUINT uiSourceFrames = rVoice.pStreamWave != TNULL && rVoice.bLoop ?
			                                 rVoice.pStreamWave->GetLoopEndFrame() :
			                             rVoice.pStreamWave != TNULL ?
			                                 rVoice.pStreamWave->GetSampleFrames() :
			                                 rVoice.pWave->GetSampleFrames();
			const TUINT64 uiWaveLength = TUINT64( uiSourceFrames ) << 16;
			if ( rVoice.uiSamplePosition >= uiWaveLength )
			{
				if ( rVoice.bLoop )
					rVoice.uiSamplePosition %= uiWaveLength;
				else
				{
					rVoice = {};
					continue;
				}
			}

			const TUINT uiSampleFrame = TUINT( rVoice.uiSamplePosition >> 16 );
			const TINT16* pSamples = rVoice.pStreamWave != TNULL ?
			                            rVoice.pStreamWave->GetFrame( uiSampleFrame ) :
			                            rVoice.pWave->GetSamples() +
			                                uiSampleFrame * AWave_PSP::OUTPUT_CHANNELS;
			if ( pSamples == TNULL )
			{
				rVoice = {};
				continue;
			}
			iLeft += TINT32( pSamples[ 0 ] ) * TINT32( rVoice.uiLeftVolume ) / TINT32( MAX_VOLUME );
			iRight += TINT32( pSamples[ 1 ] ) * TINT32( rVoice.uiRightVolume ) / TINT32( MAX_VOLUME );
			rVoice.uiSamplePosition += rVoice.uiSampleStep;
			bMixedVoice = TTRUE;
		}

		a_pOutputSamples[ uiFrame * AWave_PSP::OUTPUT_CHANNELS ] = ClampMixedSample_PSP( iLeft );
		a_pOutputSamples[ uiFrame * AWave_PSP::OUTPUT_CHANNELS + 1 ] = ClampMixedSample_PSP( iRight );
	}

	for ( TUINT uiVoice = 0; uiVoice < MAX_VOICES; uiVoice++ )
	{
		Voice& rVoice = m_aVoices[ uiVoice ];
		if ( !rVoice.bActive )
			continue;
		const TUINT uiSourceFrames = rVoice.pStreamWave != TNULL ?
		                                 rVoice.pStreamWave->GetSampleFrames() :
		                                 rVoice.pWave->GetSampleFrames();
		if ( rVoice.bActive && !rVoice.bLoop &&
		     rVoice.uiSamplePosition >= ( TUINT64( uiSourceFrames ) << 16 ) )
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

TBOOL ASoundMixer_PSP::IsVoicePaused( ASoundVoiceHandle_PSP a_hVoice ) const
{
	const TINT iVoice = ResolveVoice( a_hVoice );
	return iVoice >= 0 && m_aVoices[ iVoice ].bPaused;
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
	const TUINT uiSpatialVolume = a_rVoice.uiVolume * a_rVoice.uiAttenuation / MAX_VOLUME;
	const TUINT uiLeftPan = a_rVoice.iPan > 0 ? TUINT( MAX_PAN - a_rVoice.iPan ) : TUINT( MAX_PAN );
	const TUINT uiRightPan = a_rVoice.iPan < 0 ? TUINT( MAX_PAN + a_rVoice.iPan ) : TUINT( MAX_PAN );
	a_rVoice.uiLeftVolume = uiSpatialVolume * uiLeftPan / TUINT( MAX_PAN );
	a_rVoice.uiRightVolume = uiSpatialVolume * uiRightPan / TUINT( MAX_PAN );
}

TBOOL ASoundMixer_PSP::UpdateVoiceFrequency( Voice& a_rVoice, TUINT a_uiFrequency )
{
	if ( a_uiFrequency < MIN_FREQUENCY || a_uiFrequency > MAX_FREQUENCY )
		return TFALSE;

	a_rVoice.uiFrequency = a_uiFrequency;
	a_rVoice.uiSampleStep = TUINT32(
	    ( TUINT64( a_uiFrequency ) << 16 ) / TUINT64( DEFAULT_FREQUENCY )
	);
	return TTRUE;
}

TBOOL ASoundMixer_PSP::IsListenerValid( const ASoundListener_PSP& a_rListener ) const
{
	if ( a_rListener.iX < -MAX_POSITION || a_rListener.iX > MAX_POSITION ||
	     a_rListener.iY < -MAX_POSITION || a_rListener.iY > MAX_POSITION ||
	     a_rListener.iZ < -MAX_POSITION || a_rListener.iZ > MAX_POSITION ||
	     a_rListener.iRightX < -LISTENER_AXIS_SCALE || a_rListener.iRightX > LISTENER_AXIS_SCALE ||
	     a_rListener.iRightZ < -LISTENER_AXIS_SCALE || a_rListener.iRightZ > LISTENER_AXIS_SCALE ||
	     a_rListener.iForwardX < -LISTENER_AXIS_SCALE || a_rListener.iForwardX > LISTENER_AXIS_SCALE ||
	     a_rListener.iForwardZ < -LISTENER_AXIS_SCALE || a_rListener.iForwardZ > LISTENER_AXIS_SCALE )
	{
		return TFALSE;
	}

	const TINT64 iExpectedLength = TINT64( LISTENER_AXIS_SCALE ) * LISTENER_AXIS_SCALE;
	const TINT64 iTolerance = iExpectedLength / 8;
	const TINT64 iRightLength = TINT64( a_rListener.iRightX ) * a_rListener.iRightX +
	                            TINT64( a_rListener.iRightZ ) * a_rListener.iRightZ;
	const TINT64 iForwardLength = TINT64( a_rListener.iForwardX ) * a_rListener.iForwardX +
	                              TINT64( a_rListener.iForwardZ ) * a_rListener.iForwardZ;
	const TINT64 iDot = TINT64( a_rListener.iRightX ) * a_rListener.iForwardX +
	                    TINT64( a_rListener.iRightZ ) * a_rListener.iForwardZ;
	return iRightLength >= iExpectedLength - iTolerance &&
	       iRightLength <= iExpectedLength + iTolerance &&
	       iForwardLength >= iExpectedLength - iTolerance &&
	       iForwardLength <= iExpectedLength + iTolerance &&
	       iDot >= -iTolerance && iDot <= iTolerance;
}
