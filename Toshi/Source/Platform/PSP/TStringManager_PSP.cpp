#include "ToshiPCH.h"
#include "Toshi/TStringManager.h"

TOSHI_NAMESPACE_START

TWCHAR* TStringManager::StringCharToUnicode( TWCHAR* a_pUnicodeString, const TCHAR* a_pCharString, TUINT32 a_uiLength )
{
	TASSERT( a_pUnicodeString != TNULL && a_pCharString != TNULL );

	const TSIZE uiSourceLength = T2String8::Length( a_pCharString );
	if ( uiSourceLength < a_uiLength || a_uiLength == TUINT32( -1 ) )
		a_uiLength = TUINT32( uiSourceLength );

	for ( TSIZE i = 0; i < a_uiLength; i++ )
		a_pUnicodeString[ i ] = TWCHAR( a_pCharString[ i ] );

	a_pUnicodeString[ a_uiLength ] = L'\0';
	return a_pUnicodeString;
}

TCHAR* TStringManager::StringUnicodeToChar( TCHAR* a_pCharString, const TWCHAR* const& a_pUnicodeString, TUINT32 a_uiLength )
{
	TASSERT( a_pUnicodeString != TNULL && a_pCharString != TNULL );

	TSIZE uiSourceLength = 0;
	while ( a_pUnicodeString[ uiSourceLength ] != L'\0' )
		uiSourceLength += 1;

	if ( uiSourceLength < a_uiLength || a_uiLength == TUINT32( -1 ) )
		a_uiLength = TUINT32( uiSourceLength );

	for ( TSIZE i = 0; i < a_uiLength; i++ )
		a_pCharString[ i ] = TCHAR( a_pUnicodeString[ i ] );

	a_pCharString[ a_uiLength ] = '\0';
	return a_pCharString;
}

TOSHI_NAMESPACE_END
