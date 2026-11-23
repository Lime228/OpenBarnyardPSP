#include "pch.h"
#include "AMovieAudio_PSP.h"

#include <Core/TMemory.h>

#include <pspaudio.h>
#include <pspkernel.h>

#include <cstring>

TOSHI_NAMESPACE_USING

AMovieAudio_PSP::~AMovieAudio_PSP()
{
	Close();
}

TBOOL AMovieAudio_PSP::Open( TUINT32 a_uiMaxRecordFrames, TUINT32 a_uiBufferedRecords )
{
	Close();
	m_uiCapacity = TMath::Max( 8u * OUTPUT_FRAMES, a_uiBufferedRecords * a_uiMaxRecordFrames + OUTPUT_FRAMES );
	m_pRing = TSTATICCAST( TINT16, TMemalign( m_uiCapacity * CHANNELS * sizeof( TINT16 ), 64 ) );
	m_pOutput = TSTATICCAST( TINT16, TMemalign( 2 * OUTPUT_FRAMES * CHANNELS * sizeof( TINT16 ), 64 ) );
	if ( m_pRing == TNULL || m_pOutput == TNULL || !m_Lock.Create() || !m_Wake.Create( 0, 1 ) )
	{
		Close();
		return TFALSE;
	}
	m_iChannel = sceAudioChReserve( PSP_AUDIO_NEXT_CHANNEL, OUTPUT_FRAMES, PSP_AUDIO_FORMAT_STEREO );
	if ( m_iChannel < 0 )
	{
		Close();
		return TFALSE;
	}
	m_iThread = sceKernelCreateThread( "BarnyardMovieAudio", ThreadEntry, 0x13, 0x2000, PSP_THREAD_ATTR_USER, TNULL );
	AMovieAudio_PSP* pAudio = this;
	if ( m_iThread < 0 || sceKernelStartThread( m_iThread, sizeof( pAudio ), &pAudio ) < 0 )
	{
		if ( m_iThread >= 0 )
			sceKernelDeleteThread( m_iThread );
		m_iThread = -1;
		Close();
		return TFALSE;
	}
	return TTRUE;
}

void AMovieAudio_PSP::Close()
{
	if ( m_iThread >= 0 )
	{
		m_Lock.Lock();
		m_bClosed = TTRUE;
		m_Lock.Unlock();
		m_Wake.Signal();
		sceKernelWaitThreadEnd( m_iThread, TNULL );
		sceKernelDeleteThread( m_iThread );
	}
	if ( m_iChannel >= 0 )
	{
		// The hardware can still own the last submitted buffer after the writer exits.
		sceAudioChangeChannelVolume( m_iChannel, 0, 0 );
		while ( sceAudioGetChannelRestLen( m_iChannel ) > 0 )
			sceKernelDelayThread( 1000 );
		sceAudioChRelease( m_iChannel );
	}
	m_Wake.Destroy();
	m_Lock.Destroy();
	TFree( m_pRing );
	TFree( m_pOutput );
	m_pRing = TNULL;
	m_pOutput = TNULL;
	m_iChannel = -1;
	m_iThread = -1;
	m_uiCapacity = 0;
	m_uiWritten = 0;
	m_uiRead = 0;
	m_uiSubmitted = 0;
	m_uiPeak = 0;
	m_uiUnderruns = 0;
	m_bClosed = TFALSE;
	m_bPaused = TTRUE;
	m_bMuted = TFALSE;
	m_bInputFinished = TFALSE;
	m_bFailed = TFALSE;
}

TBOOL AMovieAudio_PSP::Queue( const TINT16* a_pPCM, TUINT32 a_uiFrames )
{
	m_Lock.Lock();
	if ( m_bFailed || a_uiFrames > m_uiCapacity - ( m_uiWritten - m_uiRead ) )
	{
		m_Lock.Unlock();
		return TFALSE;
	}
	const TUINT32 uiStart = m_uiWritten % m_uiCapacity;
	const TUINT32 uiFirst = TMath::Min( a_uiFrames, m_uiCapacity - uiStart );
	std::memcpy( m_pRing + CHANNELS * uiStart, a_pPCM, uiFirst * CHANNELS * sizeof( TINT16 ) );
	std::memcpy( m_pRing, a_pPCM + uiFirst * CHANNELS, ( a_uiFrames - uiFirst ) * CHANNELS * sizeof( TINT16 ) );
	m_uiWritten += a_uiFrames;
	m_Lock.Unlock();
	m_Wake.Signal();
	for ( TSIZE i = 0; i < a_uiFrames * CHANNELS; i++ )
		m_uiPeak = TMath::Max( m_uiPeak, TUINT32( TMath::Abs( TINT( a_pPCM[ i ] ) ) ) );
	return TTRUE;
}

void AMovieAudio_PSP::FinishInput()
{
	m_Lock.Lock();
	m_bInputFinished = TTRUE;
	m_Lock.Unlock();
	m_Wake.Signal();
}

void AMovieAudio_PSP::Pause( TBOOL a_bPause )
{
	m_Lock.Lock();
	m_bPaused = a_bPause;
	m_Lock.Unlock();
	m_Wake.Signal();
}

void AMovieAudio_PSP::Mute( TBOOL a_bMute )
{
	m_Lock.Lock();
	m_bMuted = a_bMute;
	m_Lock.Unlock();
}

TBOOL AMovieAudio_PSP::IsHealthy()
{
	m_Lock.Lock();
	const TBOOL bHealthy = !m_bFailed;
	m_Lock.Unlock();
	return bHealthy;
}

TUINT32 AMovieAudio_PSP::GetPlayedFrames()
{
	m_Lock.Lock();
	const TINT iRemaining = sceAudioGetChannelRestLen( m_iChannel );
	const TUINT32 uiPlayed = iRemaining >= 0 && TUINT32( iRemaining ) < m_uiSubmitted ? m_uiSubmitted - iRemaining : 0;
	const TUINT32 uiResult = TMath::Min( uiPlayed, m_uiWritten );
	m_Lock.Unlock();
	return uiResult;
}

TUINT32 AMovieAudio_PSP::GetUnderruns()
{
	m_Lock.Lock();
	const TUINT32 uiResult = m_uiUnderruns;
	m_Lock.Unlock();
	return uiResult;
}

TSIZE AMovieAudio_PSP::GetBufferBytes() const
{
	return IsOpen() ? ( m_uiCapacity + 2 * OUTPUT_FRAMES ) * CHANNELS * sizeof( TINT16 ) : 0;
}

TINT AMovieAudio_PSP::ThreadEntry( SceSize a_uiArgs, void* a_pArgs )
{
	if ( a_uiArgs != sizeof( AMovieAudio_PSP* ) || a_pArgs == TNULL )
		return -1;
	( *TSTATICCAST( AMovieAudio_PSP*, a_pArgs ) )->Output();
	return 0;
}

void AMovieAudio_PSP::Output()
{
	TUINT32 uiSlot = 0;
	TBOOL bWaitingForPCM = TFALSE;
	while ( TTRUE )
	{
		m_Lock.Lock();
		if ( m_bClosed || m_bFailed )
		{
			m_Lock.Unlock();
			return;
		}
		const TUINT32 uiAvailable = m_uiWritten - m_uiRead;
		if ( m_bPaused || uiAvailable == 0 || ( uiAvailable < OUTPUT_FRAMES && !m_bInputFinished ) )
		{
			bWaitingForPCM = !m_bPaused && !m_bInputFinished && m_uiSubmitted != 0;
			m_Lock.Unlock();
			m_Wake.Wait();
			continue;
		}
		if ( bWaitingForPCM && sceAudioGetChannelRestLen( m_iChannel ) == 0 )
			m_uiUnderruns++;
		bWaitingForPCM = TFALSE;
		TINT16* pBlock = m_pOutput + uiSlot * OUTPUT_FRAMES * CHANNELS;
		const TUINT32 uiFrames = TMath::Min( uiAvailable, OUTPUT_FRAMES );
		const TUINT32 uiStart = m_uiRead % m_uiCapacity;
		const TUINT32 uiFirst = TMath::Min( uiFrames, m_uiCapacity - uiStart );
		std::memcpy( pBlock, m_pRing + CHANNELS * uiStart, uiFirst * CHANNELS * sizeof( TINT16 ) );
		std::memcpy( pBlock + uiFirst * CHANNELS, m_pRing, ( uiFrames - uiFirst ) * CHANNELS * sizeof( TINT16 ) );
		std::memset( pBlock + uiFrames * CHANNELS, 0, ( OUTPUT_FRAMES - uiFrames ) * CHANNELS * sizeof( TINT16 ) );
		m_uiRead += uiFrames;
		const TINT iVolume = m_bMuted ? 0 : PSP_AUDIO_VOLUME_MAX;
		m_Lock.Unlock();
		sceKernelDcacheWritebackRange( pBlock, OUTPUT_FRAMES * CHANNELS * sizeof( TINT16 ) );
		const TINT iResult = sceAudioOutputBlocking( m_iChannel, iVolume, pBlock );
		m_Lock.Lock();
		m_bFailed = iResult < 0;
		if ( !m_bFailed )
			m_uiSubmitted += OUTPUT_FRAMES;
		m_Lock.Unlock();
		uiSlot ^= 1;
	}
}
