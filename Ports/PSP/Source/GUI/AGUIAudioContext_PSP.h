#pragma once

#include "../Sound/ASoundCommandQueue_PSP.h"

enum ASoundId_PSP : TINT16
{
	ASoundId_PSP_UI_MENUCLICK = 166,
	ASoundId_PSP_UI_MENUBACK  = 167,
	ASoundId_PSP_UI_MENUOK    = 168,
};

class AGUIAudioContext_PSP
{
public:
	AGUIAudioContext_PSP();

	TBOOL Create( const AWaveBank_PSP* a_pWaveBank, ASoundCommandQueue_PSP* a_pCommandQueue );
	void Destroy();
	void PlaySound( TINT16 a_iSoundID );

	TBOOL WasLastPlayQueued() const { return m_bLastPlayQueued; }
	TUINT GetRejectedSoundCount() const { return m_uiRejectedSounds; }
	TBOOL IsCreated() const { return m_pWaveBank != TNULL && m_pCommandQueue != TNULL; }

private:
	const TCHAR* ResolveWaveName( TINT16 a_iSoundID ) const;

private:
	const AWaveBank_PSP* m_pWaveBank;
	ASoundCommandQueue_PSP* m_pCommandQueue;
	TBOOL m_bLastPlayQueued;
	TUINT m_uiRejectedSounds;
};
