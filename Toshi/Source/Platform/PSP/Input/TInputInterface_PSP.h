#pragma once

#include "Input/TInputInterface.h"
#include "TInputDeviceController_PSP.h"

TOSHI_NAMESPACE_START

class TInputInterface_PSP : public TInputInterface
{
public:
	TDECLARE_CLASS( TInputInterface_PSP, TInputInterface );

public:
	TInputInterface_PSP();
	virtual ~TInputInterface_PSP() override;

	virtual TBOOL Initialise() override;
	virtual TBOOL Deinitialise() override;

	TInputDeviceController_PSP& GetController() { return m_oController; }
	const TInputDeviceController_PSP& GetController() const { return m_oController; }

private:
	TInputDeviceController_PSP m_oController;
	TBOOL                      m_bInitialised;
};

TOSHI_NAMESPACE_END
