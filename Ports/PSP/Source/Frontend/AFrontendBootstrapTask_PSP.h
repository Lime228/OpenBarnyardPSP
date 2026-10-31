#pragma once

#include <Toshi/TTask.h>
#include <Render/TBackend.h>

class AGameState_PSP;

enum AFrontendState_PSP
{
	AFrontendState_PSP_Boot,
	AFrontendState_PSP_Menu,
	AFrontendState_PSP_Probe,
	AFrontendState_PSP_Info,
	AFrontendState_PSP_Error,
};

struct AFrontendBootstrapState_PSP
{
	TFLOAT fXOffset;
	TFLOAT fModelRotation;
	TFLOAT fStateTime;
	TBOOL  bConfirmDown;
	TUINT  uiFrameCount;
	TUINT  uiTransitionCount;
	AFrontendState_PSP eState;
	AFrontendState_PSP eRequestedState;
	AGameState_PSP* pActiveState;
};

struct AFrontendRenderContext_PSP
{
	Toshi::TRenderInterface_PSP*      pRenderInterface;
	Toshi::TVertexPoolResource_PSP*   pVertices;
	Toshi::TIndexPoolResource_PSP*    pIndices;
	Toshi::T2Texture*                 pTexture;
	Toshi::T2Texture*                 pFontTexture;
	Toshi::TModel_PSP*                pModel;
	const Toshi::TRenderMaterial_PSP* pMaterial;
	TBOOL                             bBootstrapReady;
	TBOOL                             bMeshReady;
};

class AFrontendUpdateTask_PSP : public Toshi::TTask
{
public:
	TDECLARE_CLASS( AFrontendUpdateTask_PSP, Toshi::TTask );

public:
	AFrontendUpdateTask_PSP();

	virtual TBOOL OnCreate() override;
	virtual TBOOL OnUpdate( TFLOAT a_fDeltaTime ) override;

	void SetState( AFrontendBootstrapState_PSP* a_pState ) { m_pState = a_pState; }

private:
	AFrontendBootstrapState_PSP* m_pState;
};

class AFrontendRenderTask_PSP : public Toshi::TTask
{
public:
	TDECLARE_CLASS( AFrontendRenderTask_PSP, Toshi::TTask );

public:
	AFrontendRenderTask_PSP();

	virtual TBOOL OnCreate() override;
	virtual TBOOL OnUpdate( TFLOAT a_fDeltaTime ) override;

	void SetState( AFrontendBootstrapState_PSP* a_pState ) { m_pState = a_pState; }
	void SetContext( const AFrontendRenderContext_PSP* a_pContext ) { m_pContext = a_pContext; }

private:
	AFrontendBootstrapState_PSP*      m_pState;
	const AFrontendRenderContext_PSP* m_pContext;
};
