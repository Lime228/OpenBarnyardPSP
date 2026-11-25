#pragma once

#include <Platform/PSP/TRenderInterface_PSP.h>

class ASkyDome_PSP
{
public:
	TBOOL Load( const TCHAR* a_szRoot = TNULL );
	void Unload();
	TUINT Render( Toshi::TRenderInterface_PSP& a_rRenderer, const Toshi::TRenderCamera_PSP& a_rCamera, TFLOAT a_fDeltaTime );
	const TCHAR* GetFailedPath() const { return m_szFailedPath; }

private:
	Toshi::TModel_PSP m_Base;
	Toshi::TModel_PSP m_Clouds;
	Toshi::T2Texture m_CloudTexture;
	TFLOAT m_fRotation = 0.0f;
	TFLOAT m_fRotationSpeed = 0.0f;
	TCHAR m_szFailedPath[ 80 ] = {};
};
