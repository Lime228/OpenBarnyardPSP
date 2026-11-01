#pragma once

#include <Toshi/Typedefs.h>
#include <Thread/TThread.h>
#include <Thread/TSemaphore.h>

class ASoundManager_PSP;
class ASoundCommandQueue_PSP;
class AWaveBank_PSP;

using ASoundId_PSP = TINT32;

static constexpr ASoundId_PSP ASOUNDID_PSP_INVALID = -1;
static constexpr ASoundId_PSP ASoundId_PSP_UI_MENUCLICK = 166;
static constexpr ASoundId_PSP ASoundId_PSP_UI_MENUBACK  = 167;
static constexpr ASoundId_PSP ASoundId_PSP_UI_MENUOK    = 168;

class ASoundOutputThread_PSP : public Toshi::TThread
{
public:
	ASoundOutputThread_PSP();

	virtual void Main() override;
	void SetSoundManager( ASoundManager_PSP* a_pSoundManager ) { m_pSoundManager = a_pSoundManager; }

private:
	ASoundManager_PSP* m_pSoundManager;
};

class ASoundManager_PSP
{
public:
	static constexpr TUINT SAMPLE_RATE = 44100;
	static constexpr TUINT BUFFER_SAMPLE_FRAMES = 1024;
	static constexpr TUINT OUTPUT_CHANNELS = 2;

public:
	ASoundManager_PSP();
	~ASoundManager_PSP();

	TBOOL Create();
	void Destroy();
	TBOOL BindCueBank( const AWaveBank_PSP* a_pWaveBank, ASoundCommandQueue_PSP* a_pCommandQueue );
	void UnbindCueBank();
	TINT PlayCue( ASoundId_PSP a_eSoundID );

	TBOOL SubmitStereo( const TINT16* a_pSamples, TUINT a_uiSampleFrames );
	TBOOL SubmitSilence();
	TBOOL WaitForSubmittedBuffer();
	TINT16* LockOutputBuffer();
	TINT16* TryLockOutputBuffer();
	TBOOL UnlockOutputBuffer();

	TBOOL IsCreated() const { return m_iChannel >= 0; }
	TBOOL IsCueBankBound() const { return m_pWaveBank != TNULL && m_pCommandQueue != TNULL; }
	TINT GetChannel() const { return m_iChannel; }
	TUINT GetRejectedCueCount() const { return m_uiRejectedCues; }

private:
	const TCHAR* ResolveCueName( ASoundId_PSP a_eSoundID ) const;
	void RunOutputThread();
	friend class ASoundOutputThread_PSP;

	static constexpr TUINT OUTPUT_BUFFER_COUNT = 2;

	Toshi::TSemaphore m_FreeBuffers;
	Toshi::TSemaphore m_ReadyBuffers;
	Toshi::TSemaphore m_CompletedBuffers;
	Toshi::TSemaphore m_ThreadStopped;
	ASoundOutputThread_PSP m_OutputThread;
	alignas( 64 ) TINT16 m_aOutputBuffers[ 2 ][ BUFFER_SAMPLE_FRAMES * OUTPUT_CHANNELS ];
	TINT m_iChannel;
	volatile TBOOL m_bRunning;
	volatile TBOOL m_bOutputHealthy;
	TBOOL m_bOutputLocked;
	TUINT m_uiWriteBuffer;
	TUINT m_uiReadBuffer;
	const AWaveBank_PSP* m_pWaveBank;
	ASoundCommandQueue_PSP* m_pCommandQueue;
	TUINT m_uiRejectedCues;
};
