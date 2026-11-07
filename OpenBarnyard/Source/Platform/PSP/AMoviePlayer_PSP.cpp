#include "pch.h"
#include "AMoviePlayer_PSP.h"

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( AMoviePlayer_PSP );

AMoviePlayer_PSP::AMoviePlayer_PSP()
    : m_bPlaying( TFALSE ), m_bPaused( TFALSE )
{
	m_szMoviePath[ 0 ] = '\0';
}

AMoviePlayer_PSP::~AMoviePlayer_PSP() = default;

TBOOL AMoviePlayer_PSP::OnCreate()
{
	return AMoviePlayer::OnCreate();
}

TBOOL AMoviePlayer_PSP::OnUpdate( TFLOAT a_fDeltaTime )
{
	return TTRUE;
}

void AMoviePlayer_PSP::OnDestroy()
{
	StopMovie();
	AMoviePlayer::OnDestroy();
}

TBOOL AMoviePlayer_PSP::PlayMovie( const TCHAR* a_szFileName, PLAYFLAGS a_eFlags )
{
	TStringManager::String8Format( m_szMoviePath, sizeof( m_szMoviePath ), "Data/Movies/%s.pmvf", a_szFileName );
	m_bPlaying = TFALSE;
	m_bPaused  = TFALSE;
	return TFALSE;
}

void AMoviePlayer_PSP::StopMovie()
{
	m_bPlaying = TFALSE;
	m_bPaused  = TFALSE;
}

void AMoviePlayer_PSP::PauseMovie( TBOOL a_bPause )
{
	m_bPaused = m_bPlaying && a_bPause;
}

TBOOL AMoviePlayer_PSP::IsMoviePlaying()
{
	return m_bPlaying;
}

TBOOL AMoviePlayer_PSP::IsMoviePaused()
{
	return m_bPaused;
}

void AMoviePlayer_PSP::Render( TFLOAT a_fDeltaTime )
{
}
