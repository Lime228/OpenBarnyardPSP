#include "ToshiPCH.h"
#include "T2String16.h"

#include "Core/TMemoryDebugOn.h"

TOSHI_NAMESPACE_START

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
