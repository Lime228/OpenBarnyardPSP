#pragma once

#include <Toshi/TTask.h>

class AFrontendBootstrapTask_PSP : public Toshi::TTask
{
public:
	TDECLARE_CLASS( AFrontendBootstrapTask_PSP, Toshi::TTask );

public:
	AFrontendBootstrapTask_PSP();
	virtual ~AFrontendBootstrapTask_PSP() override = default;

	virtual TBOOL OnCreate() override;
	virtual TBOOL OnUpdate( TFLOAT a_fDeltaTime ) override;

	TFLOAT GetXOffset() const { return m_fXOffset; }
	TFLOAT GetModelRotation() const { return m_fModelRotation; }
	TBOOL  IsConfirmDown() const { return m_bConfirmDown; }
	TUINT  GetFrameCount() const { return m_uiFrameCount; }

private:
	TFLOAT m_fXOffset;
	TFLOAT m_fModelRotation;
	TBOOL  m_bConfirmDown;
	TUINT  m_uiFrameCount;
};
