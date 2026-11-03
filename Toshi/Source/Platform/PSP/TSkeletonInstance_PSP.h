#pragma once

#include "TSkeleton_PSP.h"

TOSHI_NAMESPACE_START

class TAnimationClip_PSP;

class TSkeletonInstance_PSP
{
public:
	TSkeletonInstance_PSP();
	~TSkeletonInstance_PSP();

	TBOOL Create( const TSkeleton_PSP* a_pSkeleton );
	void  Destroy();
	TBOOL SetBindPose();
	TBOOL SetAnimationPose( const TAnimationClip_PSP& a_rClip, TUINT a_uiTimeMilliseconds );

	const TSkeleton_PSP* GetSkeleton() const { return m_pSkeleton; }
	const TSkeletonTransform_PSP* GetBoneTransform( TUINT32 a_uiBone ) const;
	const TSkeletonTransform_PSP* GetBoneTransform( const TCHAR* a_pName ) const;

private:
	const TSkeleton_PSP*   m_pSkeleton;
	TSkeletonTransform_PSP m_aTransforms[ TSkeletonDataHeader_PSP::MAX_BONE_COUNT ];
};

TOSHI_NAMESPACE_END
