#pragma once

#include "Input/TInputDeviceController.h"

#include <pspctrl.h>

TOSHI_NAMESPACE_START

class TInputDeviceController_PSP : public TInputDeviceController
{
public:
	TDECLARE_CLASS( TInputDeviceController_PSP, TInputDeviceController );

	static constexpr TINT STICK_CENTRE   = 128;
	static constexpr TINT STICK_DEADZONE = 16;
	static constexpr TINT STICK_SATURATE = 127;

public:
	TInputDeviceController_PSP();
	virtual ~TInputDeviceController_PSP() override = default;

	virtual TBOOL Acquire() override;
	virtual TBOOL Unacquire() override;
	virtual void  Release() override;
	virtual void  Update( TFLOAT a_fDeltaTime ) override;
	virtual TBOOL Flush() override;
	virtual TINT  ProcessEvents( EventEmitter& a_rEmitter, TFLOAT a_fDeltaTime ) override;
	virtual TINT  GetButtonCount() const override;
	virtual TINT  GetAxisCount() const override;
	virtual TBOOL IsDown( Doodad a_iDoodad ) const override;
	virtual TINT  GetAxisInt( Doodad a_iDoodad, TINT a_iAxis ) const override;
	virtual TINT  GetAxisInt2( Doodad a_iDoodad, TINT a_iAxis ) const override;
	virtual TFLOAT GetAxisFloat( Doodad a_iDoodad, TINT a_iAxis ) const override;
	virtual TFLOAT GetAxisFloat2( Doodad a_iDoodad, TINT a_iAxis ) const override;
	virtual TBOOL IsEnabled() const override;
	TUINT GetCurrentButtons_PSP() const { return m_oState.Buttons; }
	TUINT GetObservedButtons_PSP() const { return m_uiObservedButtonsPSP; }
	TUINT GetNonZeroButtonSamples_PSP() const { return m_uiNonZeroButtonSamplesPSP; }

	virtual TBOOL WasDown( Doodad a_iDoodad ) const override;
	virtual TINT  GetPortIndex() override;

private:
	static TUINT TranslateDoodadToPSPButton( Doodad a_iDoodad );

private:
	SceCtrlData m_oState;
	SceCtrlData m_oOldState;
	TUINT       m_uiObservedButtonsPSP;
	TUINT       m_uiNonZeroButtonSamplesPSP;
	TBOOL       m_bPolled;
};

TOSHI_NAMESPACE_END
