#include "AProbeState_PSP.h"
#include "../GUI/AGUIAudioContext_PSP.h"
#include "../Input/AInputHandler_PSP.h"
#include "../World/AWorld_PSP.h"

void AProbeState_PSP::OnInsertion( AFrontendBootstrapState_PSP& a_rState )
{
	a_rState.fXOffset = 0.0f;
	a_rState.fModelRotation = 0.0f;
	a_rState.fWorldViewX = 0.0f;
	a_rState.fWorldViewZ = 0.0f;
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
	a_rState.fWorldViewX -= a_rInputHandler.GetMoveX() * 1.5f * a_fDeltaTime;
	a_rState.fWorldViewZ += a_rInputHandler.GetMoveY() * 1.5f * a_fDeltaTime;
	if ( a_rState.fWorldViewX < -2.0f ) a_rState.fWorldViewX = -2.0f;
	if ( a_rState.fWorldViewX > 2.0f ) a_rState.fWorldViewX = 2.0f;
	if ( a_rState.fWorldViewZ < -1.5f ) a_rState.fWorldViewZ = -1.5f;
	if ( a_rState.fWorldViewZ > 1.5f ) a_rState.fWorldViewZ = 1.5f;

	a_rState.fModelRotation += 0.9f * a_fDeltaTime;
	a_rState.uiFrameCount += 1;
	if ( !a_rInputHandler.IsActionPressed( AInputAction_PSP_Cancel ) )
		return AFrontendState_PSP_Probe;

	if ( a_rState.pGUIAudioContext != TNULL )
		a_rState.pGUIAudioContext->PlaySound( ASoundId_PSP_UI_MENUBACK );
	return AFrontendState_PSP_Menu;
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
	if ( a_rContext.pWorld != TNULL &&
	     a_rContext.pWorld->IsLoaded() )
	{
		TUINT16 aVisibleInstances[ AWorld_PSP::MAX_INSTANCES ];
		const TUINT uiVisibleCount = a_rContext.pWorld->CollectVisibleInstances(
		    a_rState.fWorldViewX,
		    a_rState.fWorldViewZ,
		    aVisibleInstances,
		    TARRAYSIZE( aVisibleInstances )
		);
		for ( TUINT i = 0; i < uiVisibleCount; i++ )
		{
			const AWorldInstance_PSP* pInstance = a_rContext.pWorld->GetInstance( aVisibleInstances[ i ] );
			const AWorldModelResource_PSP* pResource = pInstance != TNULL
			    ? a_rContext.pWorld->GetModelResource( pInstance->uiModelIndex )
			    : TNULL;
			if ( pInstance == TNULL || pResource == TNULL )
				continue;
			const Toshi::TRenderSceneTransform_PSP oTransform = {
				{
				    pInstance->aPosition[ 0 ] + a_rState.fWorldViewX,
				    pInstance->aPosition[ 1 ],
				    pInstance->aPosition[ 2 ] + a_rState.fWorldViewZ,
				},
				{
				    pInstance->aRotation[ 0 ],
				    pInstance->aRotation[ 1 ] + a_rState.fModelRotation,
				    pInstance->aRotation[ 2 ],
				},
				pInstance->fScale,
			};
			a_rContext.pRenderInterface->DrawModel(
			    *pResource->pModel,
			    oTransform,
			    *pResource->pMaterial,
			    pResource->pTexture
			);
		}
	}
	if ( a_rContext.pSkinnedModel != TNULL &&
	     a_rContext.pSkinningBuffer != TNULL &&
	     a_rContext.pSkeletonInstance != TNULL &&
	     a_rContext.pAnimation != TNULL &&
	     a_rContext.pTexture != TNULL &&
	     a_rContext.pMaterial != TNULL )
	{
		TUINT uiAnimationTime = TUINT( a_rState.fStateTime * 1000.0f );
		if ( !a_rContext.pAnimation->IsLooping() )
		{
			const TUINT uiLastFrameTime =
			    ( a_rContext.pAnimation->GetFrameCount() - 1 ) * 1000 / a_rContext.pAnimation->GetSampleRate();
			if ( uiLastFrameTime == 0 )
			{
				uiAnimationTime = 0;
			}
			else
			{
				const TUINT uiPingPongTime = uiLastFrameTime * 2;
				uiAnimationTime %= uiPingPongTime;
				if ( uiAnimationTime > uiLastFrameTime )
					uiAnimationTime = uiPingPongTime - uiAnimationTime;
			}
		}
		if ( a_rContext.pSkeletonInstance->SetAnimationPose( *a_rContext.pAnimation, uiAnimationTime ) &&
		     a_rContext.pSkinningBuffer->Skin( *a_rContext.pSkeletonInstance ) )
		{
			const Toshi::TVertexPoolResource_PSP* pVertices = a_rContext.pSkinningBuffer->GetVertices();
			if ( pVertices != TNULL )
			{
				a_rContext.pRenderInterface->DrawSkinnedModel(
				    *a_rContext.pSkinnedModel,
				    *pVertices,
				    a_rState.fModelRotation,
				    *a_rContext.pMaterial,
				    a_rContext.pTexture,
				    a_rContext.oSkinnedModelTransform
				);
			}
		}
	}
	else if ( a_rContext.pModel != TNULL && a_rContext.pTexture != TNULL && a_rContext.pMaterial != TNULL )
	{
		a_rContext.pRenderInterface->DrawModel( *a_rContext.pModel, a_rState.fModelRotation, *a_rContext.pMaterial, a_rContext.pTexture );
	}
}
