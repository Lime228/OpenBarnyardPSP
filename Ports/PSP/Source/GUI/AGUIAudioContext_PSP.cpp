#include <ToshiPCH.h>
#include "AGUIAudioContext_PSP.h"

AGUIAudioContext_PSP::AGUIAudioContext_PSP()
    : m_pWaveBank( TNULL )
    , m_pCommandQueue( TNULL )
    , m_bLastPlayQueued( TFALSE )
    , m_uiRejectedSounds( 0 )
{
}

TBOOL AGUIAudioContext_PSP::Create(
    const AWaveBank_PSP* a_pWaveBank,
    ASoundCommandQueue_PSP* a_pCommandQueue
)
{
	Destroy();
	if ( a_pWaveBank == TNULL || a_pCommandQueue == TNULL )
		return TFALSE;

	m_pWaveBank = a_pWaveBank;
	m_pCommandQueue = a_pCommandQueue;
	return TTRUE;
}

void AGUIAudioContext_PSP::Destroy()
{
	m_pWaveBank = TNULL;
	m_pCommandQueue = TNULL;
	m_bLastPlayQueued = TFALSE;
	m_uiRejectedSounds = 0;
}

void AGUIAudioContext_PSP::PlaySound( TINT16 a_iSoundID )
{
	m_bLastPlayQueued = TFALSE;
	const TCHAR* pWaveName = ResolveWaveName( a_iSoundID );
	if ( !IsCreated() || pWaveName == TNULL )
	{
		m_uiRejectedSounds++;
		return;
	}

	m_bLastPlayQueued = m_pCommandQueue->QueuePlayDetachedNamed( *m_pWaveBank, pWaveName );
	if ( !m_bLastPlayQueued )
		m_uiRejectedSounds++;
}

const TCHAR* AGUIAudioContext_PSP::ResolveWaveName( TINT16 a_iSoundID ) const
{
	switch ( a_iSoundID )
	{
		case ASoundId_PSP_UI_MENUCLICK:
		case ASoundId_PSP_UI_MENUBACK:
		case ASoundId_PSP_UI_MENUOK:
			return "BOOTSTRAP";
		default:
			return TNULL;
	}
}
