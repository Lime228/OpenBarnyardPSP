#pragma once

#include <Toshi/TTask.h>
#include <Toshi/TSingleton.h>

enum AInputAction_PSP
{
	AInputAction_PSP_Confirm = BITFLAG( 0 ),
	AInputAction_PSP_Cancel  = BITFLAG( 1 ),
	AInputAction_PSP_Pause   = BITFLAG( 2 ),
};

class AInputHandler_PSP : public Toshi::TTask
    , public Toshi::TSingleton<AInputHandler_PSP>
{
public:
	TDECLARE_CLASS( AInputHandler_PSP, Toshi::TTask );

public:
	AInputHandler_PSP();

	virtual TBOOL OnCreate() override;
	virtual TBOOL OnUpdate( TFLOAT a_fDeltaTime ) override;

	TBOOL  IsActionDown( AInputAction_PSP a_eAction ) const { return ( m_uiActionsDown & a_eAction ) != 0; }
	TBOOL  IsActionPressed( AInputAction_PSP a_eAction ) const { return ( m_uiActionsPressed & a_eAction ) != 0; }
	TFLOAT GetMoveX() const { return m_fMoveX; }

private:
	TUINT32 m_uiActionsDown;
	TUINT32 m_uiActionsPressed;
	TFLOAT  m_fMoveX;
};
