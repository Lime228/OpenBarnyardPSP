#pragma once

#include "Physics/ACollisionCommon.h"

#include <Math/TOBBox.h>
#include <Render/TModel.h>

class ACollisionModelInstance;

class ACollisionModel_PSP
{
public:
	ACollisionModel_PSP();
	virtual ~ACollisionModel_PSP();

	TBOOL Create( TINT a_iMeshIndex, Toshi::TModel* a_pModel );

	TBOOL CollideRay(
	    ACollisionModelInstance*                 a_pModelInstance,
	    const Toshi::TVector4&                   a_rcRayOrigin,
	    const Toshi::TVector4&                   a_rcRayDir,
	    TFLOAT&                                  a_rfMaxDistance,
	    Toshi::TCollisionCommon::TOSHICGROUPFLAG a_eCGroupsMask,
	    TBOOL                                    a_bCulling
	);

	TBOOL CollideRay(
	    ACollisionModelInstance*                 a_pModelInstance,
	    const Toshi::TVector4&                   a_rcRayOrigin,
	    const Toshi::TVector4&                   a_rcRayDir,
	    TFLOAT&                                  a_rfMaxDistance,
	    ACollisionModelRayHitList*               a_pHitList,
	    Toshi::TCollisionCommon::TOSHICGROUPFLAG a_eIgnoreGroups,
	    TBOOL                                    a_bCulling
	);

	TBOOL CollideSphere(
	    ACollisionModelInstance*                 a_pModelInstance,
	    const Toshi::TVector4&                   a_rcSphere,
	    TFLOAT&                                  a_rfDistance,
	    Toshi::TVector4&                         a_rNormal,
	    TINT                                     a_iCollGroup,
	    Toshi::TCollisionCommon::TOSHICGROUPFLAG a_eIgnoreGroups
	);

	TBOOL CollideSphere(
	    ACollisionModelInstance*                 a_pModelInstance,
	    const Toshi::TVector4&                   a_rcSphere,
	    ACollisionModelRayHitList*               a_pHitList,
	    TINT                                     a_iCollGroup,
	    Toshi::TCollisionCommon::TOSHICGROUPFLAG a_eIgnoreGroups
	);

	TBOOL CollideBox(
	    ACollisionModelInstance*                 a_pModelInstance,
	    const Toshi::TOBBox&                     a_rcBox,
	    ACollisionModelRayHitList*               a_pHitList,
	    TINT                                     a_iCollGroup,
	    Toshi::TCollisionCommon::TOSHICGROUPFLAG a_eIgnoreGroups
	);

	TBOOL CollideSweptBox(
	    ACollisionModelInstance*                 a_pModelInstance,
	    const Toshi::TOBBox&                     a_rcPrevBox,
	    const Toshi::TOBBox&                     a_rcBox,
	    ACollisionModelRayHitList*               a_pHitList,
	    TINT                                     a_iCollGroup,
	    Toshi::TCollisionCommon::TOSHICGROUPFLAG a_eIgnoreGroups
	);

	TBOOL CollideSweptSphere(
	    ACollisionModelInstance*                 a_pModelInstance,
	    const Toshi::TVector4&                   a_rcCenter,
	    TFLOAT                                   a_fRadius,
	    const Toshi::TVector4&                   a_rcDirection,
	    TFLOAT&                                  a_rfDistance,
	    Toshi::TVector4&                         a_rNormal,
	    TFLOAT                                   a_fMaxStepHeight,
	    TFLOAT&                                  a_rfStepHeight,
	    TFLOAT&                                  a_rfPenetration,
	    TBOOL&                                   a_rbEdgeHit,
	    Toshi::TCollisionCommon::TOSHICGROUPFLAG a_eIgnoreGroups
	);

	TBOOL CollideRayFast(
	    const Toshi::TVector4&  a_rcRayOrigin,
	    const Toshi::TVector4&  a_rcRayDir,
	    TFLOAT                  a_fMaxDistance,
	    const Toshi::TMatrix44& a_rcModelTransform,
	    TFLOAT                  a_fModelScale
	);

	TBOOL CollideSphereFast( const Toshi::TVector4& a_rcSphere, const Toshi::TMatrix44& a_rcModelTransform, TFLOAT a_fModelScale );

	Toshi::TModelCollisionData* GetCollisionData() const;

	static void CreateStaticCache();
	static void DestroyStaticCache();

private:
	Toshi::TVector4 m_vecTreeCenter;
	TFLOAT          m_fAverageExtent;
	Toshi::TModel*  m_pModel;
	TINT            m_iMeshIndex;
	TINT            m_aIgnoreGroups[ 2 ];
};

using ACollisionModel = ACollisionModel_PSP;
