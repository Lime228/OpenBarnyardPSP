#pragma once

#include "AGameState_PSP.h"

class AFrontendInfoState_PSP : public AGameState_PSP
{
public:
	virtual AFrontendState_PSP GetStateID() const override { return AFrontendState_PSP_Info; }
	virtual AFrontendState_PSP OnUpdate(
	    AFrontendBootstrapState_PSP& a_rState,
	    const AInputHandler_PSP& a_rInputHandler,
	    TFLOAT a_fDeltaTime
	) override;
	virtual TUINT32 GetClearColor( const AFrontendBootstrapState_PSP& a_rState ) const override;
	virtual void OnRender(
	    const AFrontendBootstrapState_PSP& a_rState,
	    const AFrontendRenderContext_PSP& a_rContext
	) override;
};
