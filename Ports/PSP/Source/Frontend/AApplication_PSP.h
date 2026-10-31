#pragma once

#include "AFrontendBootstrapTask_PSP.h"

#include <Toshi/THPTimer.h>
#include <Toshi/TScheduler.h>
#include <Platform/PSP/Input/TInputInterface_PSP.h>

enum AApplicationFlag_PSP
{
	AApplicationFlag_PSP_Created   = BITFLAG( 0 ),
	AApplicationFlag_PSP_Destroyed = BITFLAG( 1 ),
};

class AApplication_PSP
{
public:
	AApplication_PSP(
	    Toshi::TInputInterface_PSP* a_pInputInterface,
	    Toshi::TScheduler* a_pScheduler,
	    Toshi::THPTimer* a_pFrameTimer
	);
	~AApplication_PSP();

	TBOOL Create( const AFrontendRenderContext_PSP* a_pRenderContext );
	TBOOL Update();
	void  Destroy();

	TBOOL IsCreated() const { return ( m_uiFlags & AApplicationFlag_PSP_Created ) != 0; }
	TBOOL IsDestroyed() const { return ( m_uiFlags & AApplicationFlag_PSP_Destroyed ) != 0; }

	const AFrontendBootstrapState_PSP& GetFrontendState() const { return m_FrontendState; }

private:
	Toshi::TInputInterface_PSP* m_pInputInterface;
	Toshi::TScheduler*          m_pScheduler;
	Toshi::THPTimer*            m_pFrameTimer;
	AFrontendBootstrapState_PSP m_FrontendState;
	TUINT32                     m_uiFlags;
};
