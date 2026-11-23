#pragma once

#include "Movie/AMoviePlayer.h"
#include "AMovieAudio_PSP.h"

#include <Platform/PSP/TMovieStream_PSP.h>

class AMoviePlayer_PSP : public AMoviePlayer
{
public:
	TDECLARE_CLASS( AMoviePlayer_PSP, AMoviePlayer );

public:
	AMoviePlayer_PSP();
	~AMoviePlayer_PSP();

	virtual TBOOL OnCreate() override;
	virtual TBOOL OnUpdate( TFLOAT a_fDeltaTime ) override;
	virtual void  OnDestroy() override;

	virtual TBOOL PlayMovie( const TCHAR* a_szFileName, PLAYFLAGS a_eFlags ) override;
	virtual void  StopMovie() override;
	virtual void  PauseMovie( TBOOL a_bPause ) override;
	virtual void  Mute( TBOOL a_bMute = TTRUE ) override;
	virtual TBOOL IsMoviePlaying() override;
	virtual TBOOL IsMoviePaused() override;
	virtual void  Render( TFLOAT a_fDeltaTime ) override;

private:
	TBOOL ReadAhead();
	void CloseMovie( const TCHAR* a_szResult );
	void WriteMovieReport( const TCHAR* a_szResult );

	Toshi::TMovieStream_PSP m_Movie;
	AMovieAudio_PSP m_Audio;
	Toshi::TFile* m_pMovieFile = TNULL;
	TINT16* m_pPCM = TNULL;
	TUINT64 m_uiStartTime = 0;
	TUINT64 m_uiPauseTime = 0;
	TUINT32 m_uiAudioClock = 0;
	TUINT32 m_uiReadFrames = 0;
	TUINT32 m_uiCurrentFrame = 0;
	TUINT32 m_uiPresentedFrame = 0;
	TUINT32 m_uiPresentedFrames = 0;
	TBOOL m_bPlaying = TFALSE;
	TBOOL m_bPaused = TFALSE;
	TBOOL m_bMuted = TFALSE;
	TCHAR m_szMoviePath[ 64 ] = {};
};
