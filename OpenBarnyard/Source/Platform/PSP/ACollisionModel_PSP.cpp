#include "pch.h"
#include "ACollisionModel_PSP.h"

ACollisionModel_PSP::ACollisionModel_PSP()
    : m_fAverageExtent( 0.0f ), m_pModel( TNULL ), m_iMeshIndex( -1 )
{
	m_vecTreeCenter.Set( 0.0f, 0.0f, 0.0f, 0.0f );
	m_aIgnoreGroups[ 0 ] = 0;
	m_aIgnoreGroups[ 1 ] = 0;
}

ACollisionModel_PSP::~ACollisionModel_PSP() = default;

TBOOL ACollisionModel_PSP::Create( TINT a_iMeshIndex, Toshi::TModel* a_pModel )
{
	m_iMeshIndex = a_iMeshIndex;
	m_pModel     = a_pModel;
	return a_pModel != TNULL;
}

TBOOL ACollisionModel_PSP::CollideRay( ACollisionModelInstance*, const Toshi::TVector4&, const Toshi::TVector4&, TFLOAT&, Toshi::TCollisionCommon::TOSHICGROUPFLAG, TBOOL )
{
	return TFALSE;
}

TBOOL ACollisionModel_PSP::CollideRay( ACollisionModelInstance*, const Toshi::TVector4&, const Toshi::TVector4&, TFLOAT&, ACollisionModelRayHitList*, Toshi::TCollisionCommon::TOSHICGROUPFLAG, TBOOL )
{
	return TFALSE;
}

TBOOL ACollisionModel_PSP::CollideSphere( ACollisionModelInstance*, const Toshi::TVector4&, TFLOAT&, Toshi::TVector4&, TINT, Toshi::TCollisionCommon::TOSHICGROUPFLAG )
{
	return TFALSE;
}

TBOOL ACollisionModel_PSP::CollideSphere( ACollisionModelInstance*, const Toshi::TVector4&, ACollisionModelRayHitList*, TINT, Toshi::TCollisionCommon::TOSHICGROUPFLAG )
{
	return TFALSE;
}

TBOOL ACollisionModel_PSP::CollideBox( ACollisionModelInstance*, const Toshi::TOBBox&, ACollisionModelRayHitList*, TINT, Toshi::TCollisionCommon::TOSHICGROUPFLAG )
{
	return TFALSE;
}

TBOOL ACollisionModel_PSP::CollideSweptBox( ACollisionModelInstance*, const Toshi::TOBBox&, const Toshi::TOBBox&, ACollisionModelRayHitList*, TINT, Toshi::TCollisionCommon::TOSHICGROUPFLAG )
{
	return TFALSE;
}

TBOOL ACollisionModel_PSP::CollideSweptSphere( ACollisionModelInstance*, const Toshi::TVector4&, TFLOAT, const Toshi::TVector4&, TFLOAT&, Toshi::TVector4&, TFLOAT, TFLOAT&, TFLOAT&, TBOOL&, Toshi::TCollisionCommon::TOSHICGROUPFLAG )
{
	return TFALSE;
}

TBOOL ACollisionModel_PSP::CollideRayFast( const Toshi::TVector4&, const Toshi::TVector4&, TFLOAT, const Toshi::TMatrix44&, TFLOAT )
{
	return TFALSE;
}

TBOOL ACollisionModel_PSP::CollideSphereFast( const Toshi::TVector4&, const Toshi::TMatrix44&, TFLOAT )
{
	return TFALSE;
}

Toshi::TModelCollisionData* ACollisionModel_PSP::GetCollisionData() const
{
	return TNULL;
}

void ACollisionModel_PSP::CreateStaticCache()
{
}

void ACollisionModel_PSP::DestroyStaticCache()
{
}
