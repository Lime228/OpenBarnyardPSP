#include "ToshiPCH.h"
#include "TSkeletonInstance_PSP.h"

TOSHI_NAMESPACE_START

static void TSkeletonMultiplyQuaternion_PSP(
    TFLOAT* a_pOutput,
    const TFLOAT* a_pLeft,
    const TFLOAT* a_pRight
)
{
	const TFLOAT fLeftX = a_pLeft[ 0 ];
	const TFLOAT fLeftY = a_pLeft[ 1 ];
	const TFLOAT fLeftZ = a_pLeft[ 2 ];
	const TFLOAT fLeftW = a_pLeft[ 3 ];
	const TFLOAT fRightX = a_pRight[ 0 ];
	const TFLOAT fRightY = a_pRight[ 1 ];
	const TFLOAT fRightZ = a_pRight[ 2 ];
	const TFLOAT fRightW = a_pRight[ 3 ];
	a_pOutput[ 0 ] = fLeftW * fRightX + fLeftX * fRightW + fLeftY * fRightZ - fLeftZ * fRightY;
	a_pOutput[ 1 ] = fLeftW * fRightY - fLeftX * fRightZ + fLeftY * fRightW + fLeftZ * fRightX;
	a_pOutput[ 2 ] = fLeftW * fRightZ + fLeftX * fRightY - fLeftY * fRightX + fLeftZ * fRightW;
	a_pOutput[ 3 ] = fLeftW * fRightW - fLeftX * fRightX - fLeftY * fRightY - fLeftZ * fRightZ;
}

static void TSkeletonRotateVector_PSP(
    TFLOAT* a_pOutput,
    const TFLOAT* a_pRotation,
    const TFLOAT* a_pVector
)
{
	const TFLOAT fX = a_pRotation[ 0 ];
	const TFLOAT fY = a_pRotation[ 1 ];
	const TFLOAT fZ = a_pRotation[ 2 ];
	const TFLOAT fW = a_pRotation[ 3 ];
	const TFLOAT fVectorX = a_pVector[ 0 ];
	const TFLOAT fVectorY = a_pVector[ 1 ];
	const TFLOAT fVectorZ = a_pVector[ 2 ];
	const TFLOAT fCrossX = fY * fVectorZ - fZ * fVectorY;
	const TFLOAT fCrossY = fZ * fVectorX - fX * fVectorZ;
	const TFLOAT fCrossZ = fX * fVectorY - fY * fVectorX;
	const TFLOAT fSecondCrossX = fY * fCrossZ - fZ * fCrossY;
	const TFLOAT fSecondCrossY = fZ * fCrossX - fX * fCrossZ;
	const TFLOAT fSecondCrossZ = fX * fCrossY - fY * fCrossX;
	a_pOutput[ 0 ] = fVectorX + 2.0f * ( fW * fCrossX + fSecondCrossX );
	a_pOutput[ 1 ] = fVectorY + 2.0f * ( fW * fCrossY + fSecondCrossY );
	a_pOutput[ 2 ] = fVectorZ + 2.0f * ( fW * fCrossZ + fSecondCrossZ );
}

TSkeletonInstance_PSP::TSkeletonInstance_PSP()
    : m_pSkeleton( TNULL )
{
}

TBOOL TSkeletonInstance_PSP::Create( const TSkeleton_PSP* a_pSkeleton )
{
	Destroy();
	if ( a_pSkeleton == TNULL || !a_pSkeleton->IsLoaded() )
		return TFALSE;

	m_pSkeleton = a_pSkeleton;
	if ( !SetBindPose() )
	{
		Destroy();
		return TFALSE;
	}

	return TTRUE;
}

void TSkeletonInstance_PSP::Destroy()
{
	m_pSkeleton = TNULL;
}

TBOOL TSkeletonInstance_PSP::SetBindPose()
{
	if ( m_pSkeleton == TNULL || !m_pSkeleton->IsLoaded() )
		return TFALSE;

	for ( TUINT32 i = 0; i < m_pSkeleton->GetBoneCount(); i++ )
	{
		const TSkeletonBone_PSP* pBone = m_pSkeleton->GetBone( i );
		if ( pBone == TNULL )
			return TFALSE;

		TSkeletonTransform_PSP& rOutput = m_aTransforms[ i ];
		if ( pBone->iParentBone < 0 )
		{
			for ( TUINT32 component = 0; component < 4; component++ )
				rOutput.aRotation[ component ] = pBone->aRotation[ component ];
			for ( TUINT32 component = 0; component < 3; component++ )
				rOutput.aTranslation[ component ] = pBone->aTranslation[ component ];
			continue;
		}

		const TSkeletonTransform_PSP& rParent = m_aTransforms[ pBone->iParentBone ];
		TFLOAT aRotatedTranslation[ 3 ];
		TSkeletonMultiplyQuaternion_PSP( rOutput.aRotation, rParent.aRotation, pBone->aRotation );
		TSkeletonRotateVector_PSP( aRotatedTranslation, rParent.aRotation, pBone->aTranslation );
		for ( TUINT32 component = 0; component < 3; component++ )
			rOutput.aTranslation[ component ] = rParent.aTranslation[ component ] + aRotatedTranslation[ component ];
	}

	return TTRUE;
}

const TSkeletonTransform_PSP* TSkeletonInstance_PSP::GetBoneTransform( TUINT32 a_uiBone ) const
{
	return m_pSkeleton != TNULL && a_uiBone < m_pSkeleton->GetBoneCount() ?
	           &m_aTransforms[ a_uiBone ] :
	           TNULL;
}

const TSkeletonTransform_PSP* TSkeletonInstance_PSP::GetBoneTransform( const TCHAR* a_pName ) const
{
	if ( m_pSkeleton == TNULL )
		return TNULL;

	const TINT iBone = m_pSkeleton->GetBoneID( a_pName );
	return iBone >= 0 ? GetBoneTransform( TUINT32( iBone ) ) : TNULL;
}

TOSHI_NAMESPACE_END
