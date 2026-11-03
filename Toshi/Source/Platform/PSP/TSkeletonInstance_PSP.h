#pragma once

#include "TSkeleton_PSP.h"

TOSHI_NAMESPACE_START

struct TSkeletonTransform_PSP
{
	TFLOAT aRotation[ 4 ];
	TFLOAT aTranslation[ 3 ];
};

class TSkeletonInstance_PSP
{
public:
	TSkeletonInstance_PSP();

	TBOOL Create( const TSkeleton_PSP* a_pSkeleton );
	void  Destroy();
	TBOOL SetBindPose();

	const TSkeleton_PSP* GetSkeleton() const { return m_pSkeleton; }
	const TSkeletonTransform_PSP* GetBoneTransform( TUINT32 a_uiBone ) const;
	const TSkeletonTransform_PSP* GetBoneTransform( const TCHAR* a_pName ) const;

private:
	const TSkeleton_PSP*   m_pSkeleton;
	TSkeletonTransform_PSP m_aTransforms[ TSkeletonDataHeader_PSP::MAX_BONE_COUNT ];
};

TOSHI_NAMESPACE_END
