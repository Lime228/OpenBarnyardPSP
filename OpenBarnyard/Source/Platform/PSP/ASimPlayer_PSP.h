#pragma once

#include <Math/TMatrix44.h>
#include <Math/TVector2.h>
#include <Platform/PSP/TAnimationClip_PSP.h>
#include <Platform/PSP/TRenderInterface_PSP.h>
#include <Platform/PSP/TSkinningBuffer_PSP.h>

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
	TBOOL HasSteering() const { return m_bHasSteering; }

private:
	static constexpr TUINT NUM_MESHES = 2;

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
	void UpdateTransform();
	void UpdateMovement( TFLOAT a_fDeltaTime, const Toshi::TVector2& a_rStick, const Toshi::TRenderCamera_PSP& a_rCamera,
	                     const ATerrainCollision_PSP& a_rCollision );
	void UpdateTurning( TFLOAT a_fStep );
	TBOOL UpdatePose();

	Toshi::TSkeleton_PSP m_Skeleton;
	Toshi::TSkeletonInstance_PSP m_Pose;
	Toshi::TAnimationClip_PSP m_aClips[ NUM_MOTIONS ];
	Toshi::TModel_PSP m_aModels[ NUM_MESHES ];
	Toshi::TSkinningBuffer_PSP m_aSkins[ NUM_MESHES ];
	Toshi::T2Texture m_aTextures[ NUM_MESHES ];
	Toshi::TMatrix44 m_World;
	Toshi::TVector4 m_vPosition;
	Toshi::TVector4 m_vFacing;
	Toshi::TVector4 m_vDesiredFacing;
	SteerTraits m_SteerTraits = {};
	TFLOAT m_fSpeed = 0.0f;
	TFLOAT m_fTurnSpeed = 0.0f;
	TFLOAT m_fMeasuredSpeed = 0.0f;
	TFLOAT m_fAnimationTime = 0.0f;
	Motion m_eMotion = MOTION_IDLE;
	TBOOL m_bHasSteering = TFALSE;
	TCHAR m_szPath[ 88 ] = {};
};
