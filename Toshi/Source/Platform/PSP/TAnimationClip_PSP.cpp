#include "ToshiPCH.h"
#include "TAnimationClip_PSP.h"

#include "Toshi/T2Allocator.h"

#include <cstring>
#include <math.h>

TOSHI_NAMESPACE_START

static TBOOL TAnimationSampleIsValid_PSP( const TAnimationSample_PSP& a_rSample )
{
	if ( a_rSample.iReserved != 0 )
		return TFALSE;

	TINT64 iLengthSquared = 0;
	for ( TUINT32 i = 0; i < TARRAYSIZE( a_rSample.aRotation ); i++ )
	{
		const TINT64 iValue = a_rSample.aRotation[ i ];
		iLengthSquared += iValue * iValue;
	}

	static constexpr TINT64 MIN_LENGTH_SQUARED = 1020054735;
	static constexpr TINT64 MAX_LENGTH_SQUARED = 1127429028;
	return iLengthSquared >= MIN_LENGTH_SQUARED && iLengthSquared <= MAX_LENGTH_SQUARED;
}

static void TAnimationDecodeRotation_PSP( TFLOAT* a_pOutput, const TINT16* a_pInput )
{
	static constexpr TFLOAT QUANTIZATION_SCALE = 1.0f / 32767.0f;
	TFLOAT fLengthSquared = 0.0f;
	for ( TUINT32 i = 0; i < 4; i++ )
	{
		a_pOutput[ i ] = TFLOAT( a_pInput[ i ] ) * QUANTIZATION_SCALE;
		fLengthSquared += a_pOutput[ i ] * a_pOutput[ i ];
	}

	const TFLOAT fInverseLength = 1.0f / sqrtf( fLengthSquared );
	for ( TUINT32 i = 0; i < 4; i++ )
		a_pOutput[ i ] *= fInverseLength;
}

TAnimationClip_PSP::TAnimationClip_PSP()
    : m_pData( TNULL )
    , m_uiDataSize( 0 )
    , m_pSkeleton( TNULL )
    , m_pSamples( TNULL )
    , m_uiBoneCount( 0 )
    , m_uiFrameCount( 0 )
    , m_uiSampleRate( 0 )
    , m_fTranslationScale( 0.0f )
    , m_uiFlags( TANIMATIONCLIPFLAGS_PSP_NONE )
{
}

TAnimationClip_PSP::~TAnimationClip_PSP()
{
	Unload();
}

void TAnimationClip_PSP::SetData( const void* a_pData, TUINT a_uiDataSize )
{
	m_pData      = a_pData;
	m_uiDataSize = a_uiDataSize;
}

void TAnimationClip_PSP::SetSkeleton( const TSkeleton_PSP* a_pSkeleton )
{
	m_pSkeleton = a_pSkeleton;
}

TBOOL TAnimationClip_PSP::Load()
{
	Unload();
	if ( m_pData == TNULL || m_pSkeleton == TNULL || !m_pSkeleton->IsLoaded() ||
	     m_uiDataSize < sizeof( TAnimationClipDataHeader_PSP ) )
	{
		return TFALSE;
	}

	const auto* pHeader = static_cast<const TAnimationClipDataHeader_PSP*>( m_pData );
	if ( pHeader->uiMagic != TAnimationClipDataHeader_PSP::MAGIC ||
	     pHeader->uiVersion != TAnimationClipDataHeader_PSP::VERSION ||
	     pHeader->uiBoneCount != m_pSkeleton->GetBoneCount() ||
	     pHeader->uiFrameCount == 0 ||
	     pHeader->uiFrameCount > TAnimationClipDataHeader_PSP::MAX_FRAME_COUNT ||
	     pHeader->uiSampleRate == 0 ||
	     pHeader->uiSampleRate > TAnimationClipDataHeader_PSP::MAX_SAMPLE_RATE ||
	     pHeader->uiSampleSize != sizeof( TAnimationSample_PSP ) ||
	     pHeader->fTranslationScale != pHeader->fTranslationScale ||
	     pHeader->fTranslationScale <= 0.0f ||
	     pHeader->fTranslationScale > 100000.0f ||
	     ( pHeader->uiFlags & ~TANIMATIONCLIPFLAGS_PSP_LOOP ) != 0 )
	{
		return TFALSE;
	}

	const TSIZE uiSampleCount = TSIZE( pHeader->uiBoneCount ) * pHeader->uiFrameCount;
	const TSIZE uiSampleBytes = uiSampleCount * sizeof( TAnimationSample_PSP );
	if ( TSIZE( m_uiDataSize ) != sizeof( TAnimationClipDataHeader_PSP ) + uiSampleBytes )
		return TFALSE;

	const auto* pSamples = reinterpret_cast<const TAnimationSample_PSP*>(
	    static_cast<const TUINT8*>( m_pData ) + sizeof( TAnimationClipDataHeader_PSP )
	);
	for ( TSIZE i = 0; i < uiSampleCount; i++ )
	{
		if ( !TAnimationSampleIsValid_PSP( pSamples[ i ] ) )
			return TFALSE;
	}

	m_pSamples = static_cast<TAnimationSample_PSP*>( GetGlobalAllocator()->Malloc( uiSampleBytes, 16 ) );
	if ( m_pSamples == TNULL )
		return TFALSE;

	std::memcpy( m_pSamples, pSamples, uiSampleBytes );
	m_uiBoneCount       = pHeader->uiBoneCount;
	m_uiFrameCount      = pHeader->uiFrameCount;
	m_uiSampleRate      = pHeader->uiSampleRate;
	m_fTranslationScale = pHeader->fTranslationScale;
	m_uiFlags           = pHeader->uiFlags;
	return TTRUE;
}

void TAnimationClip_PSP::Unload()
{
	if ( m_pSamples != TNULL )
		GetGlobalAllocator()->Free( m_pSamples );
	m_pSamples           = TNULL;
	m_uiBoneCount        = 0;
	m_uiFrameCount       = 0;
	m_uiSampleRate       = 0;
	m_fTranslationScale  = 0.0f;
	m_uiFlags            = TANIMATIONCLIPFLAGS_PSP_NONE;
}

const TAnimationSample_PSP* TAnimationClip_PSP::GetSample( TUINT32 a_uiFrame, TUINT32 a_uiBone ) const
{
	if ( m_pSamples == TNULL || a_uiFrame >= m_uiFrameCount || a_uiBone >= m_uiBoneCount )
		return TNULL;
	return &m_pSamples[ TSIZE( a_uiFrame ) * m_uiBoneCount + a_uiBone ];
}

TBOOL TAnimationClip_PSP::SampleBone( TUINT32 a_uiBone, TUINT a_uiTimeMilliseconds, TSkeletonTransform_PSP& a_rOutput ) const
{
	if ( m_pSamples == TNULL || a_uiBone >= m_uiBoneCount )
		return TFALSE;

	const TUINT64 uiLastFramePosition = TUINT64( m_uiFrameCount - 1 ) * 1000;
	TUINT64 uiFramePosition = TUINT64( a_uiTimeMilliseconds ) * m_uiSampleRate;
	if ( IsLooping() && uiLastFramePosition != 0 )
		uiFramePosition %= uiLastFramePosition;
	else if ( uiFramePosition > uiLastFramePosition )
		uiFramePosition = uiLastFramePosition;

	const TUINT32 uiFrame = TUINT32( uiFramePosition / 1000 );
	const TUINT32 uiNextFrame = uiFrame + 1 < m_uiFrameCount ? uiFrame + 1 : uiFrame;
	const TFLOAT fProgress = TFLOAT( uiFramePosition % 1000 ) * 0.001f;
	const TAnimationSample_PSP* pFrom = GetSample( uiFrame, a_uiBone );
	const TAnimationSample_PSP* pTo = GetSample( uiNextFrame, a_uiBone );
	if ( pFrom == TNULL || pTo == TNULL )
		return TFALSE;

	TFLOAT aFromRotation[ 4 ];
	TFLOAT aToRotation[ 4 ];
	TAnimationDecodeRotation_PSP( aFromRotation, pFrom->aRotation );
	TAnimationDecodeRotation_PSP( aToRotation, pTo->aRotation );
	TFLOAT fDot = 0.0f;
	for ( TUINT32 i = 0; i < 4; i++ )
		fDot += aFromRotation[ i ] * aToRotation[ i ];

	TFLOAT fLengthSquared = 0.0f;
	for ( TUINT32 i = 0; i < 4; i++ )
	{
		const TFLOAT fTo = fDot < 0.0f ? -aToRotation[ i ] : aToRotation[ i ];
		a_rOutput.aRotation[ i ] = aFromRotation[ i ] + ( fTo - aFromRotation[ i ] ) * fProgress;
		fLengthSquared += a_rOutput.aRotation[ i ] * a_rOutput.aRotation[ i ];
	}
	const TFLOAT fInverseLength = 1.0f / sqrtf( fLengthSquared );
	for ( TUINT32 i = 0; i < 4; i++ )
		a_rOutput.aRotation[ i ] *= fInverseLength;

	const TFLOAT fTranslationFactor = m_fTranslationScale / 32767.0f;
	for ( TUINT32 i = 0; i < 3; i++ )
	{
		const TFLOAT fFrom = TFLOAT( pFrom->aTranslation[ i ] ) * fTranslationFactor;
		const TFLOAT fTo = TFLOAT( pTo->aTranslation[ i ] ) * fTranslationFactor;
		a_rOutput.aTranslation[ i ] = fFrom + ( fTo - fFrom ) * fProgress;
	}
	return TTRUE;
}

TOSHI_NAMESPACE_END
