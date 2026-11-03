#pragma once

#include "TSkeleton_PSP.h"

TOSHI_NAMESPACE_START

enum TAnimationClipFlags_PSP : TUINT32
{
	TANIMATIONCLIPFLAGS_PSP_NONE = 0,
	TANIMATIONCLIPFLAGS_PSP_LOOP = 1,
};

struct TAnimationClipDataHeader_PSP
{
	static constexpr TUINT32 MAGIC           = 0x4D4E4150;
	static constexpr TUINT32 VERSION         = 1;
	static constexpr TUINT32 MAX_FRAME_COUNT = 64;
	static constexpr TUINT32 MAX_SAMPLE_RATE = 60;

	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiBoneCount;
	TUINT32 uiFrameCount;
	TUINT32 uiSampleRate;
	TUINT32 uiSampleSize;
	TFLOAT  fTranslationScale;
	TUINT32 uiFlags;
};

struct TAnimationSample_PSP
{
	TINT16 aRotation[ 4 ];
	TINT16 aTranslation[ 3 ];
	TINT16 iReserved;
};

static_assert( sizeof( TAnimationClipDataHeader_PSP ) == 32, "Unexpected PSP animation header layout" );
static_assert( sizeof( TAnimationSample_PSP ) == 16, "Unexpected PSP animation sample layout" );

class TAnimationClip_PSP
{
public:
	TAnimationClip_PSP();
	~TAnimationClip_PSP();

	void SetData( const void* a_pData, TUINT a_uiDataSize );
	void SetSkeleton( const TSkeleton_PSP* a_pSkeleton );

	TBOOL Load();
	void  Unload();
	TBOOL SampleBone( TUINT32 a_uiBone, TUINT a_uiTimeMilliseconds, TSkeletonTransform_PSP& a_rOutput ) const;

	const TSkeleton_PSP* GetSkeleton() const { return m_pSkeleton; }
	TUINT32 GetBoneCount() const { return m_uiBoneCount; }
	TUINT32 GetFrameCount() const { return m_uiFrameCount; }
	TUINT32 GetSampleRate() const { return m_uiSampleRate; }
	TBOOL IsLooping() const { return ( m_uiFlags & TANIMATIONCLIPFLAGS_PSP_LOOP ) != 0; }
	TBOOL IsLoaded() const { return m_pSamples != TNULL; }

private:
	const TAnimationSample_PSP* GetSample( TUINT32 a_uiFrame, TUINT32 a_uiBone ) const;

private:
	const void*                m_pData;
	TUINT                      m_uiDataSize;
	const TSkeleton_PSP*       m_pSkeleton;
	TAnimationSample_PSP*      m_pSamples;
	TUINT32                    m_uiBoneCount;
	TUINT32                    m_uiFrameCount;
	TUINT32                    m_uiSampleRate;
	TFLOAT                     m_fTranslationScale;
	TUINT32                    m_uiFlags;
};

TOSHI_NAMESPACE_END
