#include "AApplication_PSP.h"

AApplication_PSP::AApplication_PSP(
    Toshi::TInputInterface_PSP* a_pInputInterface,
    Toshi::TScheduler* a_pScheduler,
    Toshi::THPTimer* a_pFrameTimer
)
    : m_pInputInterface( a_pInputInterface )
    , m_pScheduler( a_pScheduler )
    , m_pFrameTimer( a_pFrameTimer )
    , m_FrontendState{}
    , m_uiFlags( 0 )
{
}

AApplication_PSP::~AApplication_PSP()
{
	Destroy();
}

TBOOL AApplication_PSP::Create( const AFrontendRenderContext_PSP* a_pRenderContext )
{
	if ( IsCreated() || m_pInputInterface == TNULL || m_pScheduler == TNULL ||
	     m_pFrameTimer == TNULL || a_pRenderContext == TNULL )
	{
		return TFALSE;
	}

	m_FrontendState = {};

	// TScheduler inserts new root tasks at the head, so reverse creation keeps
	// the retail frame order: input, update, then render.
	auto* pRenderTask = m_pScheduler->CreateTask<AFrontendRenderTask_PSP>();
	auto* pUpdateTask = m_pScheduler->CreateTask<AFrontendUpdateTask_PSP>();
	auto* pInputTask  = m_pScheduler->CreateTask<AFrontendInputTask_PSP>();
	if ( pRenderTask == TNULL || pUpdateTask == TNULL || pInputTask == TNULL )
	{
		m_pScheduler->DestroyAllTasks();
		return TFALSE;
	}

	pRenderTask->SetState( &m_FrontendState );
	pRenderTask->SetContext( a_pRenderContext );
	pUpdateTask->SetState( &m_FrontendState );
	pInputTask->SetState( &m_FrontendState );

	if ( !pRenderTask->Create() || !pUpdateTask->Create() || !pInputTask->Create() )
	{
		m_pScheduler->DestroyAllTasks();
		return TFALSE;
	}

	m_uiFlags = AApplicationFlag_PSP_Created;
	return TTRUE;
}

TBOOL AApplication_PSP::Update()
{
	if ( !IsCreated() || IsDestroyed() )
		return TFALSE;

	m_pFrameTimer->Update();
	const TFLOAT fDeltaTime = m_pFrameTimer->GetDelta();
	m_pInputInterface->ProcessEvents( fDeltaTime );
	m_pScheduler->Update( fDeltaTime );
	return TTRUE;
}

void AApplication_PSP::Destroy()
{
	if ( IsDestroyed() )
		return;

	if ( m_pScheduler != TNULL )
		m_pScheduler->DestroyAllTasks();

	m_uiFlags |= AApplicationFlag_PSP_Destroyed;
}
