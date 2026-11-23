#include "ToshiPCH.h"
#include "T2String16.h"

#include <cwchar>

#include "Core/TMemoryDebugOn.h"

TOSHI_NAMESPACE_START

// $Barnyard: FUNCTION 006bb3c0
TSIZE T2String16::Length( const TWCHAR* a_wszString )
{
	return a_wszString != TNULL ? std::wcslen( a_wszString ) : 0;
}

// $Barnyard: FUNCTION 006bb450
TWCHAR* T2String16::CreateCopy( const TWCHAR* a_wszString, TSIZE a_uiSize )
{
	const TSIZE uiSize = a_uiSize == TSIZE( -1 ) ? Length( a_wszString ) : a_uiSize;
	TWCHAR* wszBuffer = new TWCHAR[ uiSize + 1 ];
	if ( uiSize != 0 )
		TUtil::MemCopy( wszBuffer, a_wszString, uiSize * sizeof( TWCHAR ) );
	wszBuffer[ uiSize ] = L'\0';
	return wszBuffer;
}

TOSHI_NAMESPACE_END
