#include <ToshiPCH.h>
#include "ASoundUpdateTask_PSP.h"

TDEFINE_CLASS( ASoundUpdateTask_PSP );

ASoundUpdateTask_PSP::ASoundUpdateTask_PSP()
    : m_pSoundManager( TNULL )
    , m_pSoundMixer( TNULL )
    , m_pCommandQueue( TNULL )
    , m_pDelayedEventQueue( TNULL )
{
}

TBOOL ASoundUpdateTask_PSP::OnCreate()
{
	return m_pSoundManager != TNULL && m_pSoundManager->IsCreated() &&
	       m_pSoundMixer != TNULL && m_pCommandQueue != TNULL &&
	       m_pDelayedEventQueue != TNULL;
}

TBOOL ASoundUpdateTask_PSP::OnUpdate( TFLOAT a_fDeltaTime )
{
	(void)a_fDeltaTime;
	m_pCommandQueue->Process( *m_pSoundMixer );
	TINT16* pOutput = m_pSoundManager->TryLockOutputBuffer();
	if ( pOutput == TNULL )
		return TTRUE;

	m_pDelayedEventQueue->Update( ASoundManager_PSP::BUFFER_SAMPLE_FRAMES, *m_pCommandQueue );
	m_pCommandQueue->Process( *m_pSoundMixer );
	m_pSoundMixer->Mix( pOutput, ASoundManager_PSP::BUFFER_SAMPLE_FRAMES );
	return m_pSoundManager->UnlockOutputBuffer();
}
