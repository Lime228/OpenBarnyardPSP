#pragma once

#include "AWave_PSP.h"

#include <Thread/TMutex.h>
#include <Thread/TSemaphore.h>
#include <Thread/TThread.h>

TOSHI_NAMESPACE_START
class TFile;
TOSHI_NAMESPACE_END

class AStreamWave_PSP;

class AStreamPrefetchThread_PSP : public Toshi::TThread
{
public:
	AStreamPrefetchThread_PSP();

	virtual void Main() OVERRIDE;
	void SetStreamWave( AStreamWave_PSP* a_pStreamWave ) { m_pStreamWave = a_pStreamWave; }

private:
	AStreamWave_PSP* m_pStreamWave;
};

class AStreamWave_PSP
{
public:
	static constexpr TUINT CACHE_SAMPLE_FRAMES = 1024;
	static constexpr TUINT MAX_STREAM_SAMPLE_FRAMES = 44100 * 300;
	static constexpr TUINT CACHE_PAGE_COUNT = 2;
	static constexpr TUINT INVALID_SAMPLE_FRAME = 0xffffffff;

public:
	AStreamWave_PSP();
	~AStreamWave_PSP();

	TBOOL Create( const TCHAR* a_pFilePath );
	void Destroy();
	TBOOL Prepare( TUINT a_uiStartSampleFrame );
	const TINT16* GetFrame( TUINT a_uiSampleFrame ) const;

	TUINT GetSampleFrames() const { return m_oHeader.uiSampleFrames; }
	TBOOL IsCreated() const { return m_pFile != TNULL; }

private:
	struct CachePage
	{
		TUINT uiStart;
		TUINT uiFrames;
		TBOOL bReady;
		alignas( 64 ) TINT16 aSamples[ CACHE_SAMPLE_FRAMES * AWave_PSP::OUTPUT_CHANNELS ];
	};

	TBOOL ReadPage( TUINT a_uiStartSampleFrame, TUINT a_uiPage );
	void RequestPrefetch( TUINT a_uiStartSampleFrame );
	void RunPrefetchThread();
	friend class AStreamPrefetchThread_PSP;

	Toshi::TMutex m_oStateMutex;
	Toshi::TSemaphore m_oPrefetchRequested;
	Toshi::TSemaphore m_oPrefetchStopped;
	AStreamPrefetchThread_PSP m_oPrefetchThread;
	Toshi::TFile* m_pFile;
	AWaveDataHeader_PSP m_oHeader;
	CachePage m_aPages[ CACHE_PAGE_COUNT ];
	TUINT m_uiCurrentPage;
	TUINT m_uiPendingStart;
	TUINT m_uiLoadingStart;
	volatile TBOOL m_bPrefetchRunning;
	TBOOL m_bSyncReady;
};
