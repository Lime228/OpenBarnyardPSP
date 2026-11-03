#pragma once

#include "Toshi/Typedefs.h"

TOSHI_NAMESPACE_START

struct TSkeletonDataHeader_PSP
{
	static constexpr TUINT32 MAGIC          = 0x4C4B5350;
	static constexpr TUINT32 VERSION        = 1;
	static constexpr TUINT32 MAX_BONE_COUNT = 64;

	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiBoneCount;
	TUINT32 uiBoneSize;
};

struct TSkeletonBone_PSP
{
	static constexpr TUINT32 MAX_NAME_LENGTH = 31;

	TINT16 iParentBone;
	TUINT8 uiNameLength;
	TUINT8 uiReserved;
	TCHAR  szName[ 32 ];
	TFLOAT aRotation[ 4 ];
	TFLOAT aTranslation[ 3 ];
};

static_assert( sizeof( TSkeletonDataHeader_PSP ) == 16, "Unexpected PSP skeleton header layout" );
static_assert( sizeof( TSkeletonBone_PSP ) == 64, "Unexpected PSP skeleton bone layout" );

class TSkeleton_PSP
{
public:
	TSkeleton_PSP();

	void SetData( const void* a_pData, TUINT a_uiDataSize );
	void GetData( const void*& a_rData, TUINT& a_rDataSize ) const;

	TBOOL Load();
	void  Unload();

	TINT GetBoneID( const TCHAR* a_pName ) const;
	const TSkeletonBone_PSP* GetBone( TUINT32 a_uiBone ) const;
	TUINT32 GetBoneCount() const { return m_uiBoneCount; }
	TBOOL IsLoaded() const { return m_uiBoneCount != 0; }

private:
	const void*       m_pData;
	TUINT             m_uiDataSize;
	TUINT32           m_uiBoneCount;
	TSkeletonBone_PSP m_aBones[ TSkeletonDataHeader_PSP::MAX_BONE_COUNT ];
};

TOSHI_NAMESPACE_END
