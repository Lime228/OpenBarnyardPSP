#pragma once

#include "../Sound/ASoundManager_PSP.h"

class AGUIAudioContext_PSP
{
public:
	AGUIAudioContext_PSP();

	TBOOL Create( ASoundManager_PSP* a_pSoundManager );
	void Destroy();
	void PlaySound( TINT16 a_iSoundID );

	TBOOL WasLastPlayQueued() const { return m_bLastPlayQueued; }
	TUINT GetRejectedSoundCount() const { return m_uiRejectedSounds; }
	TBOOL IsCreated() const { return m_pSoundManager != TNULL; }

private:
	ASoundManager_PSP* m_pSoundManager;
	TBOOL m_bLastPlayQueued;
	TUINT m_uiRejectedSounds;
};
