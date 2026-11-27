#pragma once

#include <Math/TVector4.h>
#include <Platform/PSP/TRenderInterface_PSP.h>

class ATerrainCollision_PSP;

class AShadowManager_PSP
{
public:
	struct Shadow
	{
		Toshi::TVector4 vPosition = Toshi::TVector4::VEC_ZERO;
		Toshi::TVector3 aCorners[ 4 ];
		TUINT uiMask = 0;
		TBOOL bReady = TFALSE;
	};

	TBOOL Load();
	void Unload();
	void Update( Shadow& a_rShadow, const Toshi::TVector4& a_rPosition, const Toshi::TVector4& a_rFacing,
	             const ATerrainCollision_PSP& a_rTerrain ) const;
	TBOOL Render( const Shadow& a_rShadow, Toshi::TRenderInterface_PSP& a_rRenderer,
	              const Toshi::TRenderCamera_PSP& a_rCamera ) const;
	const TCHAR* GetFailedPath() const { return m_szPath; }

private:
	Toshi::T2Texture m_Texture;
	TFLOAT m_fHalfSize = 0.0f;
	const TCHAR* m_szPath = TNULL;
};
