#include "ToshiPCH.h"
#include "TSystemDLL_PSP.h"

TOSHI_NAMESPACE_START

TSystemDLL_PSP::TSystemDLL_PSP() = default;

TSystemDLL_PSP::TSystemDLL_PSP( const TSystemDLL_PSP& a_rOther )
    : m_sFileName( a_rOther.m_sFileName )
{
}

TSystemDLL_PSP::~TSystemDLL_PSP() = default;

TBOOL TSystemDLL_PSP::Load( const TString8& a_rFileName )
{
	(void)a_rFileName;
	return TFALSE;
}

void TSystemDLL_PSP::Unload()
{
	m_sFileName = "";
}

void* TSystemDLL_PSP::GetAddress( const TString8& a_rSymbolName )
{
	(void)a_rSymbolName;
	return TNULL;
}

TSystemDLL_PSP& TSystemDLL_PSP::operator=( const TSystemDLL_PSP& a_rOther )
{
	m_sFileName = a_rOther.m_sFileName;
	return *this;
}

TOSHI_NAMESPACE_END
