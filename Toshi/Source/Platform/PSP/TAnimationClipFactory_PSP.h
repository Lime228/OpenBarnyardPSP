#pragma once

#include "TAnimationClip_PSP.h"

TOSHI_NAMESPACE_START

class TAnimationClipFactory_PSP
{
public:
	static constexpr TSIZE MAX_ANIMATION_FILE_SIZE =
	    sizeof( TAnimationClipDataHeader_PSP ) +
	    TSkeletonDataHeader_PSP::MAX_BONE_COUNT *
	        TAnimationClipDataHeader_PSP::MAX_FRAME_COUNT *
	        sizeof( TAnimationSample_PSP );

public:
	TBOOL CreateAnimationFromFile(
	    TAnimationClip_PSP& a_rClip,
	    const TSkeleton_PSP* a_pSkeleton,
	    const TCHAR* a_pFilePath
	) const;
	TBOOL CreateAnimationFromMemory(
	    TAnimationClip_PSP& a_rClip,
	    const TSkeleton_PSP* a_pSkeleton,
	    const void* a_pData,
	    TUINT a_uiDataSize
	) const;
};

TOSHI_NAMESPACE_END
