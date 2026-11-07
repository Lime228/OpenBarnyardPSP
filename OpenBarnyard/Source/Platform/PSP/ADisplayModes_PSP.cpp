#include "pch.h"
#include "ADisplayModes_PSP.h"

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( ADisplayModes_PSP );

void ADisplayModes_PSP::Initialise()
{
}

TBOOL ADisplayModes_PSP::DoesModeExist( const TUINT32& a_rWidth, const TUINT32& a_rHeight, const TUINT32& a_rColourDepth )
{
	return a_rWidth == s_Mode.m_uiWidth &&
	       a_rHeight == s_Mode.m_uiHeight &&
	       a_rColourDepth == s_Mode.m_uiColourDepth;
}
