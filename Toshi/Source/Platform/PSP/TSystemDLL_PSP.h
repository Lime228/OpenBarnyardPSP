#pragma once

#include "Toshi/TString8.h"

TOSHI_NAMESPACE_START

class TSystemDLL_PSP
{
public:
	TSystemDLL_PSP();
	TSystemDLL_PSP( const TSystemDLL_PSP& a_rOther );
	~TSystemDLL_PSP();

	TBOOL Load( const TString8& a_rFileName );
	void  Unload();
	void* GetAddress( const TString8& a_rSymbolName );

	const TString8& GetFileName() const { return m_sFileName; }
	void*           GetDLL() const { return TNULL; }

	TSystemDLL_PSP& operator=( const TSystemDLL_PSP& a_rOther );

private:
	TString8 m_sFileName;
};

TOSHI_NAMESPACE_END
