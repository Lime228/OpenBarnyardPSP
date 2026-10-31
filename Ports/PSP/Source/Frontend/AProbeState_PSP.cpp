#include "AProbeState_PSP.h"
#include "../Input/AInputHandler_PSP.h"

void AProbeState_PSP::OnInsertion( AFrontendBootstrapState_PSP& a_rState )
{
	a_rState.fXOffset = 0.0f;
	a_rState.fModelRotation = 0.0f;
}

AFrontendState_PSP AProbeState_PSP::OnUpdate(
    AFrontendBootstrapState_PSP& a_rState,
    const AInputHandler_PSP& a_rInputHandler,
    TFLOAT a_fDeltaTime
)
{
	a_rState.bConfirmDown = a_rInputHandler.IsActionDown( AInputAction_PSP_Confirm );
	a_rState.fXOffset += a_rInputHandler.GetMoveX() * 120.0f * a_fDeltaTime;
	if ( a_rState.fXOffset < -100.0f ) a_rState.fXOffset = -100.0f;
	if ( a_rState.fXOffset > 100.0f ) a_rState.fXOffset = 100.0f;

	a_rState.fModelRotation += 0.9f * a_fDeltaTime;
	a_rState.uiFrameCount += 1;
	return a_rInputHandler.IsActionPressed( AInputAction_PSP_Cancel )
	    ? AFrontendState_PSP_Menu
	    : AFrontendState_PSP_Probe;
}

TUINT32 AProbeState_PSP::GetClearColor( const AFrontendBootstrapState_PSP& a_rState ) const
{
	return a_rState.bConfirmDown ? 0xff402010 : 0xff201810;
}

void AProbeState_PSP::OnRender(
    const AFrontendBootstrapState_PSP& a_rState,
    const AFrontendRenderContext_PSP& a_rContext
)
{
	a_rContext.pRenderInterface->DrawColourTriangle( a_rState.fXOffset );
	if ( a_rContext.bMeshReady && a_rContext.pVertices != TNULL && a_rContext.pIndices != TNULL )
		a_rContext.pRenderInterface->DrawIndexedColourTriangles( *a_rContext.pVertices, *a_rContext.pIndices );
	if ( a_rContext.pTexture != TNULL )
		a_rContext.pRenderInterface->DrawTextureSprite( *a_rContext.pTexture, 176.0f + a_rState.fXOffset, 72.0f, 128.0f, 128.0f );
	if ( a_rContext.pModel != TNULL && a_rContext.pTexture != TNULL && a_rContext.pMaterial != TNULL )
		a_rContext.pRenderInterface->DrawModel( *a_rContext.pModel, a_rState.fModelRotation, *a_rContext.pMaterial, a_rContext.pTexture );
}
