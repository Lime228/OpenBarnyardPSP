#pragma once

#include <Thread/TMutex.h>
#include <Thread/TSemaphore.h>

#include <pspthreadman.h>

class AMovieAudio_PSP
{
public:
	AMovieAudio_PSP() = default;
	~AMovieAudio_PSP();
	AMovieAudio_PSP( const AMovieAudio_PSP& ) = delete;
	AMovieAudio_PSP& operator=( const AMovieAudio_PSP& ) = delete;

	TBOOL Open( TUINT32 a_uiMaxRecordFrames );
	void Close();
	TBOOL Queue( const TINT16* a_pPCM, TUINT32 a_uiFrames );
	void FinishInput();
	void Pause( TBOOL a_bPause );
	void Mute( TBOOL a_bMute );
	TBOOL IsOpen() const { return m_iChannel >= 0; }
	TBOOL IsHealthy();
	TUINT32 GetPlayedFrames();
	TUINT32 GetQueuedFrames() const { return m_uiWritten; }
	TUINT32 GetPeak() const { return m_uiPeak; }
	TSIZE GetBufferBytes() const;

private:
	static constexpr TUINT32 CHANNELS = 2;
	static constexpr TUINT32 OUTPUT_FRAMES = 1024;
	static TINT ThreadEntry( SceSize a_uiArgs, void* a_pArgs );
	void Output();

	Toshi::TMutex m_Lock;
	Toshi::TSemaphore m_Wake;
	TINT16* m_pRing = TNULL;
	TINT16* m_pOutput = TNULL;
	TINT m_iChannel = -1;
	TINT m_iThread = -1;
	TUINT32 m_uiCapacity = 0;
	TUINT32 m_uiWritten = 0;
	TUINT32 m_uiRead = 0;
	TUINT32 m_uiSubmitted = 0;
	TUINT32 m_uiPeak = 0;
	TBOOL m_bClosed = TFALSE;
	TBOOL m_bPaused = TTRUE;
	TBOOL m_bMuted = TFALSE;
	TBOOL m_bInputFinished = TFALSE;
	TBOOL m_bFailed = TFALSE;
};
