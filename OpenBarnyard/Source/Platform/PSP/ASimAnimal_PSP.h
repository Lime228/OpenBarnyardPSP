#pragma once

#include "ASimModel_PSP.h"

class ASimAnimal_PSP
{
public:
	TBOOL Load();
	void Unload();
	TBOOL Update( TFLOAT a_fDeltaTime, const Toshi::TVector4& a_rPlayerPosition );
	TUINT Render( Toshi::TRenderInterface_PSP& a_rRenderer, const Toshi::TRenderCamera_PSP& a_rCamera ) const;
	const TCHAR* GetFailedPath() const { return m_szPath; }
	TBOOL IsLoaded() const { return m_bLoaded; }
	const Toshi::TVector4& GetPosition() const { return m_vPosition; }
	TUINT GetPoseUpdates() const { return m_uiPoseUpdates; }

private:
	ASimModel_PSP m_Model;
	Toshi::TAnimationClip_PSP m_Idle;
	Toshi::TVector4 m_vPosition = Toshi::TVector4::VEC_ZERO;
	TFLOAT m_fIdleTime = 0.0f;
	TUINT m_uiIdleFrame = 0;
	TUINT m_uiPoseUpdates = 0;
	TBOOL m_bLoaded = TFALSE;
	TBOOL m_bNearby = TFALSE;
	TCHAR m_szPath[ 88 ] = {};
};
