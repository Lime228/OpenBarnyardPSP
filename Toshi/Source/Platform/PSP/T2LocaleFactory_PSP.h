#pragma once

#include "T2Locale_PSP.h"

TOSHI_NAMESPACE_START

class T2LocaleFactory_PSP
{
public:
	static constexpr TSIZE MAX_LOCALE_FILE_SIZE = sizeof( T2LocaleDataHeader_PSP ) +
	                                                 ( T2Locale_PSP::MAX_STRINGS + 1 ) * sizeof( TUINT32 ) +
	                                                 T2Locale_PSP::MAX_CHARACTERS * sizeof( TUINT16 );

public:
	TBOOL CreateLocaleFromFile( T2Locale_PSP& a_rLocale, const TCHAR* a_pFilePath ) const;
	TBOOL CreateLocaleFromMemory( T2Locale_PSP& a_rLocale, const void* a_pData, TUINT a_uiDataSize ) const;
};

TOSHI_NAMESPACE_END
