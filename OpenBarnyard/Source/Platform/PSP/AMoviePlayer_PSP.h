#pragma once

#include "Movie/AMoviePlayer.h"

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
	virtual TBOOL IsMoviePlaying() override;
	virtual TBOOL IsMoviePaused() override;
	virtual void  Render( TFLOAT a_fDeltaTime ) override;

private:
	TBOOL m_bPlaying;
	TBOOL m_bPaused;
	TCHAR m_szMoviePath[ 64 ];
};
