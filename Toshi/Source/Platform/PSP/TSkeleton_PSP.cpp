#include "ToshiPCH.h"
#include "TSkeleton_PSP.h"

#include <cstring>

TOSHI_NAMESPACE_START

static TCHAR TSkeletonToLower_PSP( TCHAR a_cCharacter )
{
	return a_cCharacter >= 'A' && a_cCharacter <= 'Z' ?
	           TCHAR( a_cCharacter + ( 'a' - 'A' ) ) :
	           a_cCharacter;
}

static TBOOL TSkeletonNamesEqual_PSP( const TCHAR* a_pLeft, const TCHAR* a_pRight )
{
	if ( a_pLeft == TNULL || a_pRight == TNULL )
		return TFALSE;

	while ( *a_pLeft != '\0' && *a_pRight != '\0' )
	{
		if ( TSkeletonToLower_PSP( *a_pLeft ) != TSkeletonToLower_PSP( *a_pRight ) )
			return TFALSE;
		a_pLeft++;
		a_pRight++;
	}

	return *a_pLeft == *a_pRight;
}

static TBOOL TSkeletonBoneIsValid_PSP( const TSkeletonBone_PSP& a_rBone, TUINT32 a_uiBone )
{
	if ( a_rBone.uiReserved != 0 ||
	     a_rBone.uiNameLength == 0 ||
	     a_rBone.uiNameLength > TSkeletonBone_PSP::MAX_NAME_LENGTH ||
	     a_rBone.szName[ a_rBone.uiNameLength ] != '\0' )
	{
		return TFALSE;
	}

	for ( TUINT32 i = 0; i < a_rBone.uiNameLength; i++ )
	{
		if ( a_rBone.szName[ i ] == '\0' )
			return TFALSE;
	}
	for ( TUINT32 i = a_rBone.uiNameLength + 1; i < TARRAYSIZE( a_rBone.szName ); i++ )
	{
		if ( a_rBone.szName[ i ] != '\0' )
			return TFALSE;
	}

	TFLOAT fRotationLengthSquared = 0.0f;
	for ( TUINT32 i = 0; i < TARRAYSIZE( a_rBone.aRotation ); i++ )
	{
		const TFLOAT fValue = a_rBone.aRotation[ i ];
		if ( fValue != fValue || fValue < -1.001f || fValue > 1.001f )
			return TFALSE;
		fRotationLengthSquared += fValue * fValue;
	}
	if ( fRotationLengthSquared < 0.999f || fRotationLengthSquared > 1.001f )
		return TFALSE;

	for ( TUINT32 i = 0; i < TARRAYSIZE( a_rBone.aTranslation ); i++ )
	{
		const TFLOAT fValue = a_rBone.aTranslation[ i ];
		if ( fValue != fValue || fValue < -100000.0f || fValue > 100000.0f )
			return TFALSE;
	}

	if ( a_uiBone == 0 )
		return a_rBone.iParentBone == -1;

	return a_rBone.iParentBone >= 0 && TUINT32( a_rBone.iParentBone ) < a_uiBone;
}

TSkeleton_PSP::TSkeleton_PSP()
    : m_pData( TNULL )
    , m_uiDataSize( 0 )
    , m_uiBoneCount( 0 )
{
}

void TSkeleton_PSP::SetData( const void* a_pData, TUINT a_uiDataSize )
{
	m_pData      = a_pData;
	m_uiDataSize = a_uiDataSize;
}

void TSkeleton_PSP::GetData( const void*& a_rData, TUINT& a_rDataSize ) const
{
	a_rData     = m_pData;
	a_rDataSize = m_uiDataSize;
}

TBOOL TSkeleton_PSP::Load()
{
	Unload();
	if ( m_pData == TNULL || m_uiDataSize < sizeof( TSkeletonDataHeader_PSP ) )
		return TFALSE;

	const auto* pHeader = static_cast<const TSkeletonDataHeader_PSP*>( m_pData );
	if ( pHeader->uiMagic != TSkeletonDataHeader_PSP::MAGIC ||
	     pHeader->uiVersion != TSkeletonDataHeader_PSP::VERSION ||
	     pHeader->uiBoneCount == 0 ||
	     pHeader->uiBoneCount > TSkeletonDataHeader_PSP::MAX_BONE_COUNT ||
	     pHeader->uiBoneSize != sizeof( TSkeletonBone_PSP ) )
	{
		return TFALSE;
	}

	const TSIZE uiExpectedSize = sizeof( TSkeletonDataHeader_PSP ) +
	                             TSIZE( pHeader->uiBoneCount ) * sizeof( TSkeletonBone_PSP );
	if ( TSIZE( m_uiDataSize ) != uiExpectedSize )
		return TFALSE;

	const auto* pBones = reinterpret_cast<const TSkeletonBone_PSP*>(
	    static_cast<const TUINT8*>( m_pData ) + sizeof( TSkeletonDataHeader_PSP )
	);
	for ( TUINT32 i = 0; i < pHeader->uiBoneCount; i++ )
	{
		if ( !TSkeletonBoneIsValid_PSP( pBones[ i ], i ) )
			return TFALSE;

		for ( TUINT32 j = 0; j < i; j++ )
		{
			if ( TSkeletonNamesEqual_PSP( pBones[ i ].szName, pBones[ j ].szName ) )
				return TFALSE;
		}
	}

	std::memcpy( m_aBones, pBones, TSIZE( pHeader->uiBoneCount ) * sizeof( TSkeletonBone_PSP ) );
	m_uiBoneCount = pHeader->uiBoneCount;
	return TTRUE;
}

void TSkeleton_PSP::Unload()
{
	m_uiBoneCount = 0;
}

TINT TSkeleton_PSP::GetBoneID( const TCHAR* a_pName ) const
{
	for ( TUINT32 i = 0; i < m_uiBoneCount; i++ )
	{
		if ( TSkeletonNamesEqual_PSP( m_aBones[ i ].szName, a_pName ) )
			return TINT( i );
	}

	return -1;
}

const TSkeletonBone_PSP* TSkeleton_PSP::GetBone( TUINT32 a_uiBone ) const
{
	return a_uiBone < m_uiBoneCount ? &m_aBones[ a_uiBone ] : TNULL;
}

TOSHI_NAMESPACE_END
