#pragma once

#include "TSkeleton_PSP.h"

TOSHI_NAMESPACE_START

class TSkeletonFactory_PSP
{
public:
	static constexpr TSIZE MAX_SKELETON_FILE_SIZE =
	    sizeof( TSkeletonDataHeader_PSP ) +
	    TSkeletonDataHeader_PSP::MAX_BONE_COUNT * sizeof( TSkeletonBone_PSP );

public:
	TBOOL CreateSkeletonFromFile( TSkeleton_PSP& a_rSkeleton, const TCHAR* a_pFilePath ) const;
	TBOOL CreateSkeletonFromMemory( TSkeleton_PSP& a_rSkeleton, const void* a_pData, TUINT a_uiDataSize ) const;
};

TOSHI_NAMESPACE_END
