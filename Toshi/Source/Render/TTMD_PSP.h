#pragma once

#include "TTMDBase.h"
#include "Math/TSphere.h"

TOSHI_NAMESPACE_START

namespace TTMD_PSP
{

struct TRBLODHeader
{
	TINT                 m_iMeshCount1;
	TINT                 m_iMeshCount2;
	TTMDBase::SHADERTYPE m_eShader;
	TSphere              m_RenderVolume;
};

struct TRBPSPHeader
{
	TINT          m_iNumLODs;
	TFLOAT        m_fLODDistance;
	TRBLODHeader* m_pLODs;

	TRBLODHeader* GetLOD( TINT a_iIndex )
	{
		TASSERT( a_iIndex >= 0 && a_iIndex < m_iNumLODs );
		return m_pLODs + a_iIndex;
	}
};

} // namespace TTMD_PSP

TOSHI_NAMESPACE_END
