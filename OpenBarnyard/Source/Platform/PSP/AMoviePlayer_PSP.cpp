#include "pch.h"
#include "AMoviePlayer_PSP.h"
#include "ARenderer_PSP.h"

#include <Core/TMemory.h>
#include <File/TFile.h>
#include <Platform/PSP/TRenderInterface_PSP.h>
#include <Toshi/T2String8.h>

#include <pspkernel.h>

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( AMoviePlayer_PSP );

static constexpr TCHAR MOVIE_REPORT_PATH_PSP[] = "local:openbarnyard_psp_movies.log";
static constexpr TSIZE MOVIE_STEREO_FRAME_BYTES_PSP = TMovieStream_PSP::AUDIO_CHANNELS * sizeof( TINT16 );

static TBOOL IsLocalisedAttract_PSP( const TCHAR* a_szName )
{
	return T2String8::Compare( a_szName, "AttractD" ) == 0 || T2String8::Compare( a_szName, "AttractI" ) == 0 ||
	       T2String8::Compare( a_szName, "AttractE" ) == 0 || T2String8::Compare( a_szName, "AttractF" ) == 0 ||
	       T2String8::Compare( a_szName, "AttractN" ) == 0;
}

AMoviePlayer_PSP::AMoviePlayer_PSP() = default;

AMoviePlayer_PSP::~AMoviePlayer_PSP()
{
	StopMovie();
}

TBOOL AMoviePlayer_PSP::OnCreate()
{
	TFile* pReport = TFile::Create( MOVIE_REPORT_PATH_PSP, TFILEMODE_WRITE | TFILEMODE_CREATENEW | TFILEMODE_NOBUFFER );
	if ( pReport != TNULL )
		pReport->Destroy();
	return AMoviePlayer::OnCreate();
}

TBOOL AMoviePlayer_PSP::OnUpdate( TFLOAT )
{
	if ( !m_bPlaying || m_bPaused || m_uiPresentedFrame == 0 )
		return TTRUE;
	const TUINT64 uiNow = sceKernelGetSystemTimeWide();
	if ( m_uiStartTime == 0 )
	{
		m_uiStartTime = uiNow;
		if ( m_Audio.IsOpen() )
			m_Audio.Pause( TFALSE );
		return TTRUE;
	}

	const TUINT32 uiCount = m_Movie.GetFrameCount();
	TBOOL bFinished;
	TUINT64 uiFrameOffset;
	if ( m_Audio.IsOpen() )
	{
		if ( !m_Audio.IsHealthy() )
		{
			CloseMovie( "audio-failed" );
			return TTRUE;
		}
		m_uiAudioClock = TMath::Max( m_uiAudioClock, m_Audio.GetPlayedFrames() );
		uiFrameOffset = (( TUINT64( m_uiAudioClock ) + 1 ) * m_Movie.GetRateNumerator() - 1) /
		                ( TUINT64( TMovieStream_PSP::AUDIO_RATE ) * m_Movie.GetRateDenominator() );
		bFinished = m_uiReadFrames == uiCount && m_uiAudioClock >= m_Audio.GetQueuedFrames();
	}
	else
	{
		uiFrameOffset = ( uiNow - m_uiStartTime ) * m_Movie.GetRateNumerator() /
		                ( 1000000ull * m_Movie.GetRateDenominator() );
		bFinished = uiFrameOffset >= uiCount;
	}
	if ( bFinished && m_uiPresentedFrame == uiCount )
	{
		CloseMovie( "finished" );
		return TTRUE;
	}

	const TUINT32 uiTarget = uiFrameOffset >= uiCount ? uiCount : TUINT32( uiFrameOffset ) + 1;
	while ( m_uiCurrentFrame < uiTarget )
	{
		m_uiCurrentFrame++;
		// Read expired frames to keep their PCM, then decode the frame due for display.
		if ( ( m_uiCurrentFrame == uiTarget && !m_Movie.DecodeFrame() ) || !ReadAhead() )
		{
			CloseMovie( "stream-failed" );
			break;
		}
	}
	return TTRUE;
}

void AMoviePlayer_PSP::OnDestroy()
{
	StopMovie();
	AMoviePlayer::OnDestroy();
}

TBOOL AMoviePlayer_PSP::PlayMovie( const TCHAR* a_szFileName, PLAYFLAGS )
{
	StopMovie();
	if ( a_szFileName == TNULL || a_szFileName[ 0 ] == '\0' )
		return TFALSE;
	TStringManager::String8Format( m_szMoviePath, sizeof( m_szMoviePath ), "local:Data/Movies/%s.pmvf", a_szFileName );
	m_pMovieFile = TFile::Create( m_szMoviePath, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( m_pMovieFile == TNULL && IsLocalisedAttract_PSP( a_szFileName ) )
	{
		TStringManager::String8Copy( m_szMoviePath, "local:Data/Movies/Attract.pmvf", sizeof( m_szMoviePath ) );
		m_pMovieFile = TFile::Create( m_szMoviePath, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	}
	if ( m_pMovieFile == TNULL || !m_Movie.Open( *m_pMovieFile ) )
	{
		CloseMovie( m_pMovieFile == TNULL ? "missing" : "open-failed" );
		return TFALSE;
	}
	const TSIZE uiMaxPCM = m_Movie.GetMaxPCMBytes();
	if ( uiMaxPCM != 0 )
	{
		m_pPCM = TSTATICCAST( TINT16, TMemalign( uiMaxPCM, 64 ) );
		if ( m_pPCM == TNULL || !m_Audio.Open( uiMaxPCM / MOVIE_STEREO_FRAME_BYTES_PSP ) )
		{
			CloseMovie( "audio-open-failed" );
			return TFALSE;
		}
		m_Audio.Mute( m_bMuted );
	}
	if ( !ReadAhead() || !m_Movie.DecodeFrame() || !ReadAhead() )
	{
		CloseMovie( "stream-failed" );
		return TFALSE;
	}
	m_uiCurrentFrame = 1;
	m_bPlaying = TTRUE;
	WriteMovieReport( "started" );
	return TTRUE;
}

TBOOL AMoviePlayer_PSP::ReadAhead()
{
	if ( m_uiReadFrames == m_Movie.GetFrameCount() )
		return TTRUE;
	if ( !m_Movie.ReadNextFrame( m_pPCM, m_Movie.GetMaxPCMBytes() ) ||
	     ( m_Audio.IsOpen() && !m_Audio.Queue( m_pPCM, m_Movie.GetPCMFrames() ) ) )
		return TFALSE;
	m_uiReadFrames++;
	if ( m_uiReadFrames == m_Movie.GetFrameCount() && m_Audio.IsOpen() )
		m_Audio.FinishInput();
	return TTRUE;
}

void AMoviePlayer_PSP::StopMovie()
{
	CloseMovie( "stopped" );
}

void AMoviePlayer_PSP::CloseMovie( const TCHAR* a_szResult )
{
	if ( m_szMoviePath[ 0 ] != '\0' )
		WriteMovieReport( a_szResult );
	m_Audio.Close();
	m_Movie.Close();
	TFree( m_pPCM );
	if ( m_pMovieFile != TNULL )
		m_pMovieFile->Destroy();
	m_pMovieFile = TNULL;
	m_pPCM = TNULL;
	m_uiStartTime = 0;
	m_uiPauseTime = 0;
	m_uiAudioClock = 0;
	m_uiReadFrames = 0;
	m_uiCurrentFrame = 0;
	m_uiPresentedFrame = 0;
	m_uiPresentedFrames = 0;
	m_bPlaying = TFALSE;
	m_bPaused = TFALSE;
	m_szMoviePath[ 0 ] = '\0';
}

void AMoviePlayer_PSP::PauseMovie( TBOOL a_bPause )
{
	if ( !m_bPlaying || m_bPaused == a_bPause )
		return;
	m_bPaused = a_bPause;
	if ( m_uiStartTime == 0 )
		return;
	const TUINT64 uiNow = sceKernelGetSystemTimeWide();
	if ( a_bPause )
		m_uiPauseTime = uiNow;
	else
		m_uiStartTime += uiNow - m_uiPauseTime;
	if ( m_Audio.IsOpen() )
		m_Audio.Pause( a_bPause );
}

void AMoviePlayer_PSP::Mute( TBOOL a_bMute )
{
	AMoviePlayer::Mute( a_bMute );
	m_bMuted = a_bMute;
	if ( m_Audio.IsOpen() )
		m_Audio.Mute( a_bMute );
}

TBOOL AMoviePlayer_PSP::IsMoviePlaying()
{
	return m_bPlaying;
}

TBOOL AMoviePlayer_PSP::IsMoviePaused()
{
	return m_bPaused;
}

void AMoviePlayer_PSP::Render( TFLOAT )
{
	if ( !m_bPlaying )
		return;
	AGetRenderInterface_PSP().DrawMovieFrame( m_Movie.GetPixels() );
	if ( m_uiPresentedFrame != m_uiCurrentFrame )
	{
		m_uiPresentedFrame = m_uiCurrentFrame;
		m_uiPresentedFrames++;
	}
}

void AMoviePlayer_PSP::WriteMovieReport( const TCHAR* a_szResult )
{
	TFile* pReport = TFile::Create( MOVIE_REPORT_PATH_PSP, TFILEMODE_WRITE | TFILEMODE_NOBUFFER );
	if ( pReport == TNULL )
		return;
	if ( pReport->Seek( 0, TSEEK_END ) )
	{
		pReport->CPrintf( "movie=%s result=%s stage=%s rate=%u/%u frames=%u read=%u presented=%u last=%u pcm_queued=%u pcm_played=%u pcm_peak=%u buffers=%u\n",
		    m_szMoviePath, a_szResult, m_Movie.GetStage(), m_Movie.GetRateNumerator(), m_Movie.GetRateDenominator(),
		    m_Movie.GetFrameCount(), m_uiReadFrames, m_uiPresentedFrames, m_uiPresentedFrame, m_Audio.GetQueuedFrames(),
		    m_Audio.IsOpen() ? TMath::Max( m_uiAudioClock, m_Audio.GetPlayedFrames() ) : 0, m_Audio.GetPeak(),
		    TUINT( m_Movie.GetBufferBytes() + m_Audio.GetBufferBytes() + m_Movie.GetMaxPCMBytes() ) );
	}
	pReport->Destroy();
}
