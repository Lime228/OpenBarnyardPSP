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

TBOOL ASoundUpdateTask_PSP::OnUpdate( TFLOAT )
{
	UpdateSound( *m_pSoundManager, *m_pSoundMixer, *m_pCommandQueue, *m_pDelayedEventQueue );
	return TTRUE;
}

TBOOL ASoundUpdateTask_PSP::UpdateSound(
    ASoundManager_PSP& a_rSoundManager,
    ASoundMixer_PSP& a_rSoundMixer,
    ASoundCommandQueue_PSP& a_rCommandQueue,
    ASoundDelayedEventQueue_PSP& a_rDelayedEventQueue,
    TBOOL* a_pNonSilent
)
{
	a_rCommandQueue.Process( a_rSoundMixer );
	a_rSoundManager.UpdateCues( a_rSoundMixer );
	a_rCommandQueue.Process( a_rSoundMixer );
	a_rSoundManager.UpdateCues( a_rSoundMixer );
	TINT16* pOutput = a_rSoundManager.TryLockOutputBuffer();
	if ( pOutput == TNULL )
		return TFALSE;

	a_rDelayedEventQueue.Update( ASoundManager_PSP::BUFFER_SAMPLE_FRAMES, a_rCommandQueue );
	a_rSoundManager.UpdateFades( ASoundManager_PSP::BUFFER_SAMPLE_FRAMES );
	a_rCommandQueue.Process( a_rSoundMixer );
	a_rSoundManager.UpdateCues( a_rSoundMixer );
	a_rCommandQueue.Process( a_rSoundMixer );
	a_rSoundManager.UpdateCues( a_rSoundMixer );
	a_rSoundMixer.Mix( pOutput, ASoundManager_PSP::BUFFER_SAMPLE_FRAMES );
	a_rSoundManager.UpdateCues( a_rSoundMixer );

	if ( a_pNonSilent != TNULL )
	{
		*a_pNonSilent = TFALSE;
		for ( TSIZE i = 0; i < ASoundManager_PSP::BUFFER_SAMPLE_FRAMES * ASoundManager_PSP::OUTPUT_CHANNELS; i++ )
		{
			if ( pOutput[ i ] != 0 )
			{
				*a_pNonSilent = TTRUE;
				break;
			}
		}
	}

	return a_rSoundManager.UnlockOutputBuffer();
}
