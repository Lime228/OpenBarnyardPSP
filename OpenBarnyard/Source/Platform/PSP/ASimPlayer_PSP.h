#pragma once

#include "ASimModel_PSP.h"

#include <Math/TVector2.h>

struct ASimStart_PSP;
class ATerrainCollision_PSP;

class ASimPlayer_PSP
{
public:
	TBOOL Load( const ASimStart_PSP& a_rStart, TUINT a_uiGender, TUINT a_uiCostume );
	void Unload();
	TBOOL Update( TFLOAT a_fDeltaTime, const Toshi::TVector2& a_rStick, const Toshi::TRenderCamera_PSP& a_rCamera,
	              const ATerrainCollision_PSP& a_rCollision );
	TUINT Render( Toshi::TRenderInterface_PSP& a_rRenderer, const Toshi::TRenderCamera_PSP& a_rCamera ) const;
	const TCHAR* GetFailedPath() const { return m_szPath; }
	const Toshi::TVector4& GetPosition() const { return m_vPosition; }
	TFLOAT GetMeasuredSpeed() const { return m_fMeasuredSpeed; }
	TUINT GetMotion() const { return m_eMotion; }
	TUINT GetTerrainContactCount() const { return m_uiTerrainContacts; }
	TUINT GetTerrainSlideCount() const { return m_uiTerrainSlides; }
	TBOOL HasSteering() const { return m_bHasSteering; }

private:
	enum Motion : TUINT
	{
		MOTION_IDLE,
		MOTION_WALK,
		MOTION_RUN,
		NUM_MOTIONS
	};

	struct SteerTraits
	{
		TFLOAT fRadius;
		TFLOAT fAcc;
		TFLOAT fDec;
		TFLOAT fTurningAcc;
		TFLOAT fTurningDec;
		TFLOAT fNHolonomicRate;
		TFLOAT fLogicalWalkSpeed;
		TFLOAT fLogicalRunSpeed;
		TFLOAT fAnimatedWalkSpeed;
		TFLOAT fAnimatedRunSpeed;
		TFLOAT fMaxAesWalkSpeed;
	};

	TBOOL LoadAssets( const TCHAR* a_szRoot, TUINT a_uiCostume );
	void UpdateMovement( TFLOAT a_fDeltaTime, const Toshi::TVector2& a_rStick, const Toshi::TRenderCamera_PSP& a_rCamera,
	                     const ATerrainCollision_PSP& a_rCollision );
	void UpdateTurning( TFLOAT a_fStep );
	TBOOL UpdatePose();

	ASimModel_PSP m_Model;
	Toshi::TAnimationClip_PSP m_aClips[ NUM_MOTIONS ];
	Toshi::TVector4 m_vPosition;
	Toshi::TVector4 m_vFacing;
	Toshi::TVector4 m_vDesiredFacing;
	SteerTraits m_SteerTraits = {};
	TFLOAT m_fSpeed = 0.0f;
	TFLOAT m_fTurnSpeed = 0.0f;
	TFLOAT m_fMeasuredSpeed = 0.0f;
	TUINT m_uiTerrainContacts = 0;
	TUINT m_uiTerrainSlides = 0;
	TFLOAT m_fAnimationTime = 0.0f;
	Motion m_eMotion = MOTION_IDLE;
	TBOOL m_bHasSteering = TFALSE;
	TCHAR m_szPath[ 88 ] = {};
};
