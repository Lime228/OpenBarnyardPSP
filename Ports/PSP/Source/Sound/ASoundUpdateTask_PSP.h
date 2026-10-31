#pragma once

#include "ASoundManager_PSP.h"
#include "ASoundMixer_PSP.h"
#include "ASoundCommandQueue_PSP.h"

#include <Toshi/TTask.h>

class ASoundUpdateTask_PSP : public Toshi::TTask
{
public:
	TDECLARE_CLASS( ASoundUpdateTask_PSP, Toshi::TTask );

public:
	ASoundUpdateTask_PSP();

	virtual TBOOL OnCreate() override;
	virtual TBOOL OnUpdate( TFLOAT a_fDeltaTime ) override;

	void SetSoundManager( ASoundManager_PSP* a_pSoundManager ) { m_pSoundManager = a_pSoundManager; }
	void SetSoundMixer( ASoundMixer_PSP* a_pSoundMixer ) { m_pSoundMixer = a_pSoundMixer; }
	void SetCommandQueue( ASoundCommandQueue_PSP* a_pCommandQueue ) { m_pCommandQueue = a_pCommandQueue; }

private:
	ASoundManager_PSP* m_pSoundManager;
	ASoundMixer_PSP* m_pSoundMixer;
	ASoundCommandQueue_PSP* m_pCommandQueue;
};
