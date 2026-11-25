#include "ToshiPCH.h"
#include "TSkinningBuffer_PSP.h"
#include "TAnimationMemory_PSP.h"

#include "Math/TMatrix44.h"
#include "Math/TQuaternion.h"

#include <pspkernel.h>

TOSHI_NAMESPACE_START

static void TSkinningMakeMatrix_PSP( TMatrix44& a_rMatrix, const TSkeletonTransform_PSP& a_rPose )
{
	const TFLOAT* pRotation = a_rPose.aRotation;
	const TFLOAT* pTranslation = a_rPose.aTranslation;
	a_rMatrix.SetFromQuaternion( TQuaternion( pRotation[ 0 ], pRotation[ 1 ], pRotation[ 2 ], pRotation[ 3 ] ) );
	a_rMatrix.SetTranslation( TVector4( pTranslation[ 0 ], pTranslation[ 1 ], pTranslation[ 2 ], 1.0f ) );
}

static void TSkinningMakeInverse_PSP( TMatrix44& a_rMatrix, const TSkeletonTransform_PSP& a_rPose )
{
	const TFLOAT* pRotation = a_rPose.aRotation;
	const TFLOAT* pTranslation = a_rPose.aTranslation;
	a_rMatrix.SetFromQuaternion( TQuaternion( -pRotation[ 0 ], -pRotation[ 1 ], -pRotation[ 2 ], pRotation[ 3 ] ) );
	TVector3 vecPosition;
	TMatrix44::RotateVector( vecPosition, a_rMatrix, TVector3( -pTranslation[ 0 ], -pTranslation[ 1 ], -pTranslation[ 2 ] ) );
	a_rMatrix.SetTranslation( TVector4( vecPosition.x, vecPosition.y, vecPosition.z, 1.0f ) );
}

TSkinningBuffer_PSP::TSkinningBuffer_PSP()
    : m_pModel( TNULL )
    , m_pSkeleton( TNULL )
    , m_uiActiveBuffer( 2 )
    , m_uiBoneCount( 0 )
    , m_uiReservedVertices( 0 )
{
}

TSkinningBuffer_PSP::~TSkinningBuffer_PSP()
{
	Destroy();
}

TBOOL TSkinningBuffer_PSP::Create( const TModel_PSP* a_pModel, const TSkeleton_PSP* a_pSkeleton )
{
	Destroy();
	if ( a_pModel == TNULL || a_pSkeleton == TNULL || !a_pModel->IsLoaded() || !a_pSkeleton->IsLoaded() ||
	     a_pModel->GetFormat() != TMODELFORMAT_PSP_SKIN_TEXTURE_COLOUR_XYZ ||
	     !m_BindPose.Create( a_pSkeleton ) )
	{
		return TFALSE;
	}

	const TVertexPoolResource_PSP& rSource = a_pModel->GetVertices();
	const auto* pVertices = static_cast<const TModelSkinTextureVertex_PSP*>( rSource.GetData() );
	for ( TUINT16 i = 0; i < rSource.GetNumVertices(); i++ )
	{
		for ( TUINT32 influence = 0; influence < 2; influence++ )
		{
			if ( pVertices[ i ].aBoneWeights[ influence ] != 0 &&
			     pVertices[ i ].aBoneIndices[ influence ] >= a_pSkeleton->GetBoneCount() )
			{
				Destroy();
				return TFALSE;
			}
		}
	}
	if ( !TAnimationMemory_PSP::ReserveSkinning( rSource.GetNumVertices() ) )
	{
		Destroy();
		return TFALSE;
	}
	m_uiReservedVertices = rSource.GetNumVertices();

	if ( !m_aVertices[ 0 ].Create( sizeof( TModelTextureVertex_PSP ), rSource.GetNumVertices() ) ||
	     !m_aVertices[ 1 ].Create( sizeof( TModelTextureVertex_PSP ), rSource.GetNumVertices() ) )
	{
		Destroy();
		return TFALSE;
	}

	m_pModel = a_pModel;
	m_pSkeleton = a_pSkeleton;
	m_uiBoneCount = TUINT8( a_pSkeleton->GetBoneCount() );
	return TTRUE;
}

void TSkinningBuffer_PSP::Destroy()
{
	m_aVertices[ 1 ].Destroy();
	m_aVertices[ 0 ].Destroy();
	m_BindPose.Destroy();
	if ( m_uiReservedVertices != 0 )
		TAnimationMemory_PSP::ReleaseSkinning( m_uiReservedVertices );
	m_pModel = TNULL;
	m_pSkeleton = TNULL;
	m_uiActiveBuffer = 2;
	m_uiBoneCount = 0;
	m_uiReservedVertices = 0;
}

TBOOL TSkinningBuffer_PSP::Skin( const TSkeletonInstance_PSP& a_rInstance )
{
	if ( !IsCreated() || a_rInstance.GetSkeleton() != m_pSkeleton || m_pSkeleton->GetBoneCount() != m_uiBoneCount )
		return TFALSE;
	const TUINT64 uiStartTime = sceKernelGetSystemTimeWide();

	// One current-pose times inverse-bind matrix per bone for all vertices in the mesh.
	TMatrix44 aSkinMatrices[ TSkeletonDataHeader_PSP::MAX_BONE_COUNT ];
	for ( TUINT32 i = 0; i < m_pSkeleton->GetBoneCount(); i++ )
	{
		TMatrix44 oCurrent, oInverseBind;
		TSkinningMakeMatrix_PSP( oCurrent, *a_rInstance.GetBoneTransform( i ) );
		TSkinningMakeInverse_PSP( oInverseBind, *m_BindPose.GetBoneTransform( i ) );
		aSkinMatrices[ i ].Multiply( oCurrent, oInverseBind );
	}

	const TUINT8 uiTargetBuffer = m_uiActiveBuffer == 0 ? 1 : 0;
	TModelTextureVertex_PSP* pOutput = reinterpret_cast<TModelTextureVertex_PSP*>(
	    m_aVertices[ uiTargetBuffer ].Lock()
	);
	if ( pOutput == TNULL )
		return TFALSE;

	const TVertexPoolResource_PSP& rSource = m_pModel->GetVertices();
	const auto* pInput = static_cast<const TModelSkinTextureVertex_PSP*>( rSource.GetData() );
	for ( TUINT16 i = 0; i < rSource.GetNumVertices(); i++ )
	{
		pOutput[ i ].fU = pInput[ i ].fU;
		pOutput[ i ].fV = pInput[ i ].fV;
		pOutput[ i ].uiColour = pInput[ i ].uiColour;
		pOutput[ i ].fX = 0.0f;
		pOutput[ i ].fY = 0.0f;
		pOutput[ i ].fZ = 0.0f;
		const TVector3 vecBindPoint( pInput[ i ].fX, pInput[ i ].fY, pInput[ i ].fZ );
		for ( TUINT32 influence = 0; influence < 2; influence++ )
		{
			const TUINT8 uiWeight = pInput[ i ].aBoneWeights[ influence ];
			if ( uiWeight == 0 )
				continue;
			const TUINT8 uiBone = pInput[ i ].aBoneIndices[ influence ];
			TVector3 vecPoint;
			TMatrix44::TransformVector( vecPoint, aSkinMatrices[ uiBone ], vecBindPoint );
			const TFLOAT fWeight = TFLOAT( uiWeight ) * ( 1.0f / 255.0f );
			pOutput[ i ].fX += vecPoint.x * fWeight;
			pOutput[ i ].fY += vecPoint.y * fWeight;
			pOutput[ i ].fZ += vecPoint.z * fWeight;
		}
	}

	m_aVertices[ uiTargetBuffer ].Unlock( rSource.GetNumVertices() );
	m_uiActiveBuffer = uiTargetBuffer;
	TAnimationMemory_PSP::RecordSkinTime( sceKernelGetSystemTimeWide() - uiStartTime );
	return TTRUE;
}

const TVertexPoolResource_PSP* TSkinningBuffer_PSP::GetVertices() const
{
	return m_uiActiveBuffer < 2 ? &m_aVertices[ m_uiActiveBuffer ] : TNULL;
}

TOSHI_NAMESPACE_END
