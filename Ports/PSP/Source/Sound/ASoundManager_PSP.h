#pragma once

#include <Toshi/Typedefs.h>
#include <Thread/TThread.h>
#include <Thread/TSemaphore.h>

class ASoundManager_PSP;

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

	TBOOL SubmitStereo( const TINT16* a_pSamples, TUINT a_uiSampleFrames );
	TBOOL SubmitSilence();
	TBOOL WaitForSubmittedBuffer();
	TINT16* LockOutputBuffer();
	TINT16* TryLockOutputBuffer();
	TBOOL UnlockOutputBuffer();

	TBOOL IsCreated() const { return m_iChannel >= 0; }
	TINT GetChannel() const { return m_iChannel; }

private:
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
};
