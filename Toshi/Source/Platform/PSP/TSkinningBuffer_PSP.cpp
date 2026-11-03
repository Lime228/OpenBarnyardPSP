#include "ToshiPCH.h"
#include "TSkinningBuffer_PSP.h"
#include "TAnimationMemory_PSP.h"

#include <pspkernel.h>

TOSHI_NAMESPACE_START

static void TSkinningRotatePoint_PSP(
    TFLOAT* a_pOutput,
    const TFLOAT* a_pRotation,
    const TFLOAT* a_pPoint
)
{
	const TFLOAT fX = a_pRotation[ 0 ];
	const TFLOAT fY = a_pRotation[ 1 ];
	const TFLOAT fZ = a_pRotation[ 2 ];
	const TFLOAT fW = a_pRotation[ 3 ];
	const TFLOAT fCrossX = fY * a_pPoint[ 2 ] - fZ * a_pPoint[ 1 ];
	const TFLOAT fCrossY = fZ * a_pPoint[ 0 ] - fX * a_pPoint[ 2 ];
	const TFLOAT fCrossZ = fX * a_pPoint[ 1 ] - fY * a_pPoint[ 0 ];
	const TFLOAT fSecondCrossX = fY * fCrossZ - fZ * fCrossY;
	const TFLOAT fSecondCrossY = fZ * fCrossX - fX * fCrossZ;
	const TFLOAT fSecondCrossZ = fX * fCrossY - fY * fCrossX;
	a_pOutput[ 0 ] = a_pPoint[ 0 ] + 2.0f * ( fW * fCrossX + fSecondCrossX );
	a_pOutput[ 1 ] = a_pPoint[ 1 ] + 2.0f * ( fW * fCrossY + fSecondCrossY );
	a_pOutput[ 2 ] = a_pPoint[ 2 ] + 2.0f * ( fW * fCrossZ + fSecondCrossZ );
}

static TBOOL TSkinningTransformPoint_PSP(
    TFLOAT* a_pOutput,
    const TSkeletonTransform_PSP* a_pBindTransform,
    const TSkeletonTransform_PSP* a_pCurrentTransform,
    const TFLOAT* a_pPoint
)
{
	if ( a_pBindTransform == TNULL || a_pCurrentTransform == TNULL )
		return TFALSE;

	const TFLOAT aBindRelative[ 3 ] = {
		a_pPoint[ 0 ] - a_pBindTransform->aTranslation[ 0 ],
		a_pPoint[ 1 ] - a_pBindTransform->aTranslation[ 1 ],
		a_pPoint[ 2 ] - a_pBindTransform->aTranslation[ 2 ],
	};
	const TFLOAT aInverseBindRotation[ 4 ] = {
		-a_pBindTransform->aRotation[ 0 ],
		-a_pBindTransform->aRotation[ 1 ],
		-a_pBindTransform->aRotation[ 2 ],
		a_pBindTransform->aRotation[ 3 ],
	};
	TFLOAT aLocalPoint[ 3 ];
	TSkinningRotatePoint_PSP( aLocalPoint, aInverseBindRotation, aBindRelative );
	TSkinningRotatePoint_PSP( a_pOutput, a_pCurrentTransform->aRotation, aLocalPoint );
	for ( TUINT32 i = 0; i < 3; i++ )
		a_pOutput[ i ] += a_pCurrentTransform->aTranslation[ i ];
	return TTRUE;
}

TSkinningBuffer_PSP::TSkinningBuffer_PSP()
    : m_pModel( TNULL )
    , m_pSkeleton( TNULL )
    , m_uiActiveBuffer( 2 )
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
	m_uiReservedVertices = 0;
}

TBOOL TSkinningBuffer_PSP::Skin( const TSkeletonInstance_PSP& a_rInstance )
{
	if ( !IsCreated() || a_rInstance.GetSkeleton() != m_pSkeleton )
		return TFALSE;
	const TUINT64 uiStartTime = sceKernelGetSystemTimeWide();

	const TUINT8 uiTargetBuffer = m_uiActiveBuffer == 0 ? 1 : 0;
	TModelTextureVertex_PSP* pOutput = reinterpret_cast<TModelTextureVertex_PSP*>(
	    m_aVertices[ uiTargetBuffer ].Lock()
	);
	if ( pOutput == TNULL )
		return TFALSE;

	const TVertexPoolResource_PSP& rSource = m_pModel->GetVertices();
	const auto* pInput = static_cast<const TModelSkinTextureVertex_PSP*>( rSource.GetData() );
	TBOOL bValid = TTRUE;
	for ( TUINT16 i = 0; i < rSource.GetNumVertices() && bValid; i++ )
	{
		pOutput[ i ].fU = pInput[ i ].fU;
		pOutput[ i ].fV = pInput[ i ].fV;
		pOutput[ i ].uiColour = pInput[ i ].uiColour;
		pOutput[ i ].fX = 0.0f;
		pOutput[ i ].fY = 0.0f;
		pOutput[ i ].fZ = 0.0f;
		const TFLOAT aBindPoint[ 3 ] = { pInput[ i ].fX, pInput[ i ].fY, pInput[ i ].fZ };
		for ( TUINT32 influence = 0; influence < 2; influence++ )
		{
			const TUINT8 uiWeight = pInput[ i ].aBoneWeights[ influence ];
			if ( uiWeight == 0 )
				continue;
			const TUINT8 uiBone = pInput[ i ].aBoneIndices[ influence ];
			TFLOAT aTransformedPoint[ 3 ];
			if ( !TSkinningTransformPoint_PSP(
			         aTransformedPoint,
			         m_BindPose.GetBoneTransform( uiBone ),
			         a_rInstance.GetBoneTransform( uiBone ),
			         aBindPoint
			     ) )
			{
				bValid = TFALSE;
				break;
			}
			const TFLOAT fWeight = TFLOAT( uiWeight ) * ( 1.0f / 255.0f );
			pOutput[ i ].fX += aTransformedPoint[ 0 ] * fWeight;
			pOutput[ i ].fY += aTransformedPoint[ 1 ] * fWeight;
			pOutput[ i ].fZ += aTransformedPoint[ 2 ] * fWeight;
		}
	}

	m_aVertices[ uiTargetBuffer ].Unlock( bValid ? rSource.GetNumVertices() : 0 );
	if ( !bValid )
		return TFALSE;
	m_uiActiveBuffer = uiTargetBuffer;
	TAnimationMemory_PSP::RecordSkinTime( sceKernelGetSystemTimeWide() - uiStartTime );
	return TTRUE;
}

const TVertexPoolResource_PSP* TSkinningBuffer_PSP::GetVertices() const
{
	return m_uiActiveBuffer < 2 ? &m_aVertices[ m_uiActiveBuffer ] : TNULL;
}

TOSHI_NAMESPACE_END
