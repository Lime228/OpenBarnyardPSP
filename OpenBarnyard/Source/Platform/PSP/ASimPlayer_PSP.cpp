#include "pch.h"
#include "ASimPlayer_PSP.h"
#include "ASimStart_PSP.h"
#include "ATerrainCollision_PSP.h"

#include <File/TFile.h>
#include <Math/TMathInline.h>
#include <Platform/PSP/TAnimationClipFactory_PSP.h>
#include <Platform/PSP/TModelFactory_PSP.h>
#include <Platform/PSP/TSkeletonFactory_PSP.h>
#include <Platform/PSP/TTextureFactory_PSP.h>
#include <Toshi/TUtil.h>

TOSHI_NAMESPACE_USING

TBOOL ASimPlayer_PSP::Load( const ASimStart_PSP& a_rStart, TUINT a_uiGender, TUINT a_uiCostume )
{
	Unload();
	static const TCHAR* const s_aszRoots[] = { "local:Data/Player/male", "local:Data/Player/female" };
	if ( a_uiGender >= TARRAYSIZE( s_aszRoots ) || a_uiCostume >= 7 )
	{
		TStringManager::String8Copy( m_szPath, "envmain-player-profile", TARRAYSIZE( m_szPath ) );
		return TFALSE;
	}

	m_vPosition = a_rStart.vPosition;
	m_vFacing = m_vDesiredFacing = a_rStart.vFacing;
	UpdateTransform();

	if ( !LoadAssets( s_aszRoots[ a_uiGender ], a_uiCostume ) )
	{
		Unload();
		return TFALSE;
	}
	m_szPath[ 0 ] = '\0';
	return TTRUE;
}

void ASimPlayer_PSP::UpdateTransform()
{
	// AModelInstance::CreateRotationFromDirection for an upright unit.
	TVector4 vForward;
	vForward.Negate3( m_vFacing );
	vForward.NormaliseXZ();
	TVector4 vRight;
	vRight.CrossProduct( vForward, TVector4::VEC_NEGY );
	vRight.Normalise();
	TVector4 vUp;
	vUp.CrossProduct( vRight, vForward );
	vUp.Normalise();
	m_World.Identity();
	m_World.SetBasisVector4( BASISVECTOR_RIGHT, TVector4( vRight.x, vRight.y, vRight.z, 0.0f ) );
	m_World.SetBasisVector4( BASISVECTOR_UP, TVector4( vForward.x, vForward.y, vForward.z, 0.0f ) );
	m_World.SetBasisVector4( BASISVECTOR_FORWARD, TVector4( vUp.x, vUp.y, vUp.z, 0.0f ) );
	m_World.SetTranslation( m_vPosition );
}

TBOOL ASimPlayer_PSP::LoadAssets( const TCHAR* a_szRoot, TUINT a_uiCostume )
{
	TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/player.pspl", a_szRoot );
	TFile* pFile = TFile::Create( m_szPath, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;
	TUINT32 aConfig[ 3 ] = {};
	static_assert( sizeof( SteerTraits ) == 11 * sizeof( TFLOAT ) );
	TFLOAT aTraits[ 11 ] = {};
	TBOOL bRead = pFile->Read( aConfig, sizeof( aConfig ) ) == sizeof( aConfig ) &&
	              aConfig[ 0 ] == 0x4c505350 && aConfig[ 2 ] < NUM_MESHES;
	if ( bRead && aConfig[ 1 ] == 2 )
	{
		bRead = pFile->GetSize() == sizeof( aConfig ) + sizeof( aTraits ) &&
		        pFile->Read( aTraits, sizeof( aTraits ) ) == sizeof( aTraits );
		for ( TFLOAT fValue : aTraits )
			bRead = bRead && TMath::IsFinite( fValue ) && fValue > 0.0f;
	}
	else
		bRead = bRead && aConfig[ 1 ] == 1 && pFile->GetSize() == sizeof( aConfig );
	pFile->Destroy();
	if ( !bRead )
		return TFALSE;
	m_bHasSteering = aConfig[ 1 ] == 2;
	TUtil::MemCopy( &m_SteerTraits, aTraits, sizeof( aTraits ) );

	const TSkeletonFactory_PSP oSkeletonFactory;
	TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/skeleton.pskl", a_szRoot );
	if ( !oSkeletonFactory.CreateSkeletonFromFile( m_Skeleton, m_szPath ) || !m_Pose.Create( &m_Skeleton ) )
		return TFALSE;
	const TAnimationClipFactory_PSP oAnimationFactory;
	static const TCHAR* const s_aszLoops[ NUM_MOTIONS ] = { "idle", "walk", "run" };
	const TUINT uiLoopCount = m_bHasSteering ? NUM_MOTIONS : 1;
	for ( TUINT i = 0; i < uiLoopCount; i++ )
	{
		TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/%s.panm", a_szRoot, s_aszLoops[ i ] );
		if ( !oAnimationFactory.CreateAnimationFromFile( m_aClips[ i ], &m_Skeleton, m_szPath ) || !m_aClips[ i ].IsLooping() )
			return TFALSE;
	}

	const TModelFactory_PSP oModelFactory;
	const TTextureFactory_PSP oTextureFactory;
	for ( TUINT i = 0; i < NUM_MESHES; i++ )
	{
		TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/mesh-%03u.pmes", a_szRoot, i );
		if ( !oModelFactory.CreateModelFromFile( m_aModels[ i ], m_szPath ) || !m_aSkins[ i ].Create( &m_aModels[ i ], &m_Skeleton ) )
			return TFALSE;
		if ( i == aConfig[ 2 ] )
			TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/costume-%u.ptex", a_szRoot, a_uiCostume );
		else
			TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/mesh-%03u.ptex", a_szRoot, i );
		if ( !oTextureFactory.CreateTextureFromFile( m_aTextures[ i ], m_szPath ) )
			return TFALSE;
	}
	return UpdatePose();
}

void ASimPlayer_PSP::Unload()
{
	for ( TUINT i = 0; i < NUM_MESHES; i++ )
	{
		m_aSkins[ i ].Destroy();
		m_aModels[ i ].Unload();
		m_aTextures[ i ].Unload();
	}
	for ( TAnimationClip_PSP& rClip : m_aClips )
		rClip.Unload();
	m_Pose.Destroy();
	m_Skeleton.Unload();
	m_fSpeed = m_fTurnSpeed = m_fMeasuredSpeed = 0.0f;
	m_fAnimationTime = 0.0f;
	m_eMotion = MOTION_IDLE;
	m_bHasSteering = TFALSE;
}

void ASimPlayer_PSP::UpdateTurning( TFLOAT a_fStep )
{
	// Unobstructed turning in ASteerKinematics3D::UpdateTurning.
	const TFLOAT fDot = TVector4::DotProduct3( m_vDesiredFacing, m_vFacing );
	const TFLOAT fAngle = TMath::ATan2( m_vFacing.z * m_vDesiredFacing.x - m_vDesiredFacing.z * m_vFacing.x, fDot );
	if ( TMath::Abs( fAngle ) <= 0.017453292f )
	{
		m_vFacing = m_vDesiredFacing;
		m_fTurnSpeed = 0.0f;
		return;
	}
	const TFLOAT fSign = fAngle < 0.0f ? -1.0f : 1.0f;
	const TFLOAT fLimit = TMath::Sqrt( 2.0f * m_SteerTraits.fTurningDec * TMath::Abs( fAngle ) );
	const TFLOAT fTarget = TMath::Min( m_SteerTraits.fNHolonomicRate, fLimit );
	TFLOAT fSpeed = m_fTurnSpeed * fSign;
	if ( fSpeed <= fTarget )
		fSpeed = TMath::Min( fTarget, fSpeed + ( fSpeed < 0.0f ? m_SteerTraits.fTurningDec : m_SteerTraits.fTurningAcc ) * a_fStep );
	else
		fSpeed = TMath::Max( fTarget, fSpeed - m_SteerTraits.fTurningDec * a_fStep );
	m_fTurnSpeed = fSpeed * fSign;
	m_vFacing.AsVector3().RotateY( m_fTurnSpeed * a_fStep );
	m_vFacing.NormaliseXZ();
}

void ASimPlayer_PSP::UpdateMovement( TFLOAT a_fDeltaTime, const TVector2& a_rStick, const TRenderCamera_PSP& a_rCamera,
                                     const ATerrainCollision_PSP& a_rCollision )
{
	TFLOAT fTargetSpeed = 0.0f;
	const TFLOAT fStickSq = a_rStick.x * a_rStick.x + a_rStick.y * a_rStick.y;
	if ( fStickSq >= 0.05f )
	{
		// ASimplePlayerControlComponent::UpdateMovement with the default, unladen player traits.
		TVector4 vForward( a_rCamera.aDirection[ 0 ], 0.0f, a_rCamera.aDirection[ 2 ], 0.0f );
		vForward.NormaliseXZ();
		TVector4 vRight = vForward;
		vRight.AsVector3().RotateY( TMath::HALF_PI );
		vRight.Multiply( a_rStick.x );
		vForward.Multiply( a_rStick.y );
		m_vDesiredFacing = vForward + vRight;
		m_vDesiredFacing.NormaliseXZ();
		const TFLOAT fMagnitude = TMath::Min( 1.0f, TMath::Sqrt( fStickSq ) );
		if ( fMagnitude >= 0.9f )
			fTargetSpeed = fMagnitude * m_SteerTraits.fLogicalRunSpeed;
		else if ( fMagnitude >= 0.8f || TVector4::DotProduct3( m_vFacing, m_vDesiredFacing ) >= 0.97f )
			fTargetSpeed = TMath::Max( fMagnitude * m_SteerTraits.fLogicalRunSpeed * 0.4f, m_SteerTraits.fLogicalWalkSpeed );
	}
	else
		m_vDesiredFacing = m_vFacing;

	static constexpr TFLOAT fTimeStep = 1.0f / 30.0f;
	const TVector4 vPrevious = m_vPosition;
	TFLOAT fRemaining = a_fDeltaTime;
	while ( fRemaining >= 0.0001f )
	{
		const TFLOAT fStep = fRemaining > fTimeStep && fRemaining < fTimeStep * 1.3f ?
		    fRemaining * 0.5f : TMath::Min( fRemaining, fTimeStep );
		fRemaining -= fStep;
		UpdateTurning( fStep );
		if ( fTargetSpeed > m_fSpeed )
			m_fSpeed = TMath::Min( fTargetSpeed, m_fSpeed + m_SteerTraits.fAcc * fStep );
		else
			m_fSpeed = TMath::Max( fTargetSpeed, m_fSpeed - m_SteerTraits.fDec * fStep );
		if ( m_fSpeed <= 0.0f )
			continue;

		const TFLOAT fDistance = m_fSpeed * fStep;
		TVector4 vTarget( m_vPosition.x + m_vFacing.x * fDistance, m_vPosition.y, m_vPosition.z + m_vFacing.z * fDistance, 1.0f );
		if ( a_rCollision.ResolveGroundedStep( vTarget, m_vFacing, m_SteerTraits.fRadius ) )
			m_vPosition = vTarget;
		else
			m_fSpeed = 0.0f;
	}
	const TFLOAT fDX = m_vPosition.x - vPrevious.x;
	const TFLOAT fDZ = m_vPosition.z - vPrevious.z;
	const TFLOAT fDistanceSq = fDX * fDX + fDZ * fDZ;
	m_fMeasuredSpeed = fDistanceSq > 0.0f ? TMath::Sqrt( fDistanceSq ) / a_fDeltaTime : 0.0f;
	UpdateTransform();
}

TBOOL ASimPlayer_PSP::Update( TFLOAT a_fDeltaTime, const TVector2& a_rStick, const TRenderCamera_PSP& a_rCamera,
                             const ATerrainCollision_PSP& a_rCollision )
{
	if ( !TMath::IsFinite( a_fDeltaTime ) || a_fDeltaTime <= 0.0f )
		return TTRUE;
	if ( m_bHasSteering )
		UpdateMovement( TMath::Min( a_fDeltaTime, 4.0f / 30.0f ), a_rStick, a_rCamera, a_rCollision );

	// ASteer::GetMovementType thresholds and ACharacterAnimController playback rates.
	Motion eMotion = m_eMotion;
	if ( m_fMeasuredSpeed < 0.05f )
		eMotion = MOTION_IDLE;
	else if ( m_eMotion == MOTION_IDLE && m_fMeasuredSpeed > 0.1f )
		eMotion = MOTION_WALK;
	else if ( m_eMotion == MOTION_WALK && m_fMeasuredSpeed > m_SteerTraits.fMaxAesWalkSpeed )
		eMotion = MOTION_RUN;
	else if ( m_eMotion == MOTION_RUN && m_fMeasuredSpeed < m_SteerTraits.fMaxAesWalkSpeed * 0.9f )
		eMotion = MOTION_WALK;
	if ( eMotion != m_eMotion )
	{
		m_eMotion = eMotion;
		m_fAnimationTime = 0.0f;
	}
	TFLOAT fRate = 1.0f;
	if ( m_eMotion == MOTION_WALK )
		fRate = m_fMeasuredSpeed / m_SteerTraits.fAnimatedWalkSpeed;
	else if ( m_eMotion == MOTION_RUN )
		fRate = m_fMeasuredSpeed / m_SteerTraits.fAnimatedRunSpeed;
	const TAnimationClip_PSP& rClip = m_aClips[ m_eMotion ];
	const TFLOAT fDuration = TFLOAT( rClip.GetFrameCount() ) / rClip.GetSampleRate();
	m_fAnimationTime += a_fDeltaTime * fRate;
	m_fAnimationTime -= TMath::FloorToInt( m_fAnimationTime / fDuration ) * fDuration;
	return UpdatePose();
}

TBOOL ASimPlayer_PSP::UpdatePose()
{
	if ( !m_Pose.SetAnimationPose( m_aClips[ m_eMotion ], TUINT( m_fAnimationTime * 1000.0f ) ) )
		return TFALSE;
	for ( TSkinningBuffer_PSP& rSkin : m_aSkins )
	{
		if ( !rSkin.Skin( m_Pose ) )
			return TFALSE;
	}
	return TTRUE;
}

TUINT ASimPlayer_PSP::Render( TRenderInterface_PSP& a_rRenderer, const TRenderCamera_PSP& a_rCamera ) const
{
	static const TRenderMaterial_PSP s_Material = {
		TTRUE, TRENDERTEXTUREFUNCTION_PSP_MODULATE, TRENDERCULLMODE_PSP_NONE, TRENDERALPHAMODE_PSP_OPAQUE,
		TRENDERTEXTUREADDRESS_PSP_WRAP, TRENDERTEXTUREADDRESS_PSP_WRAP,
	};
	TUINT uiDraws = 0;
	for ( TUINT i = 0; i < NUM_MESHES; i++ )
	{
		const auto* pVertices = m_aSkins[ i ].GetVertices();
		if ( pVertices != TNULL && a_rRenderer.DrawSkinnedModel( m_aModels[ i ], *pVertices, m_World, s_Material, &m_aTextures[ i ], a_rCamera ) )
			uiDraws++;
	}
	return uiDraws;
}
