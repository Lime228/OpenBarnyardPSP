#pragma once

#include "AFrontendBootstrapTask_PSP.h"

class AInputHandler_PSP;

class AGameState_PSP
{
public:
	virtual ~AGameState_PSP() = default;

	virtual AFrontendState_PSP GetStateID() const = 0;
	virtual void OnInsertion( AFrontendBootstrapState_PSP& a_rState );
	virtual void OnRemoval( AFrontendBootstrapState_PSP& a_rState );
	virtual AFrontendState_PSP OnUpdate(
	    AFrontendBootstrapState_PSP& a_rState,
	    const AInputHandler_PSP& a_rInputHandler,
	    TFLOAT a_fDeltaTime
	) = 0;
	virtual TUINT32 GetClearColor( const AFrontendBootstrapState_PSP& a_rState ) const = 0;
	virtual void OnRender(
	    const AFrontendBootstrapState_PSP& a_rState,
	    const AFrontendRenderContext_PSP& a_rContext
	) = 0;
};
