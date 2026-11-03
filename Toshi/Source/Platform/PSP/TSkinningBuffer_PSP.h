#pragma once

#include "TModel_PSP.h"
#include "TSkeletonInstance_PSP.h"

TOSHI_NAMESPACE_START

class TSkinningBuffer_PSP
{
public:
	TSkinningBuffer_PSP();
	~TSkinningBuffer_PSP();

	TBOOL Create( const TModel_PSP* a_pModel, const TSkeleton_PSP* a_pSkeleton );
	void  Destroy();
	TBOOL Skin( const TSkeletonInstance_PSP& a_rInstance );

	const TVertexPoolResource_PSP* GetVertices() const;
	TBOOL IsCreated() const { return m_pModel != TNULL && m_pSkeleton != TNULL; }

private:
	const TModel_PSP*      m_pModel;
	const TSkeleton_PSP*   m_pSkeleton;
	TSkeletonInstance_PSP  m_BindPose;
	TVertexPoolResource_PSP m_aVertices[ 2 ];
	TUINT8                  m_uiActiveBuffer;
};

TOSHI_NAMESPACE_END
