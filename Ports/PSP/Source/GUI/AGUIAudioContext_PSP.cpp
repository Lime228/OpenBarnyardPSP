#include <ToshiPCH.h>
#include "AGUIAudioContext_PSP.h"

AGUIAudioContext_PSP::AGUIAudioContext_PSP()
    : m_pSoundManager( TNULL )
    , m_bLastPlayQueued( TFALSE )
    , m_uiRejectedSounds( 0 )
{
}

TBOOL AGUIAudioContext_PSP::Create( ASoundManager_PSP* a_pSoundManager )
{
	Destroy();
	if ( a_pSoundManager == TNULL || !a_pSoundManager->IsCueBankBound() )
		return TFALSE;

	m_pSoundManager = a_pSoundManager;
	return TTRUE;
}

void AGUIAudioContext_PSP::Destroy()
{
	m_pSoundManager = TNULL;
	m_bLastPlayQueued = TFALSE;
	m_uiRejectedSounds = 0;
}

void AGUIAudioContext_PSP::PlaySound( TINT16 a_iSoundID )
{
	m_bLastPlayQueued = TFALSE;
	if ( !IsCreated() || a_iSoundID < 0 )
	{
		m_uiRejectedSounds++;
		return;
	}

	m_bLastPlayQueued = m_pSoundManager->PlayCue( ASoundId_PSP( a_iSoundID ) ) >= 0;
	if ( !m_bLastPlayQueued )
		m_uiRejectedSounds++;
}
