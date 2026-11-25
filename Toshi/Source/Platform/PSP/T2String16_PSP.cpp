#include "Toshi/Defines.h"
#include "Toshi/T2String16.h"
#include <cwctype>

TOSHI_NAMESPACE_START

TINT T2String16::Compare( const TWCHAR* a_pLeft, const TWCHAR* a_pRight, TSIZE a_uiSize )
{
	TSIZE uiIndex = 0;
	while ( ( a_uiSize == TSIZE( -1 ) || uiIndex < a_uiSize ) &&
	        a_pLeft[ uiIndex ] == a_pRight[ uiIndex ] )
	{
		if ( a_pLeft[ uiIndex ] == L'\0' )
			return 0;

		uiIndex++;
	}

	if ( a_uiSize != TSIZE( -1 ) && uiIndex == a_uiSize )
		return 0;

	return a_pLeft[ uiIndex ] < a_pRight[ uiIndex ] ? -1 : 1;
}

TINT T2String16::CompareNoCase( const TWCHAR* a_pLeft, const TWCHAR* a_pRight, TSIZE a_uiSize )
{
	for ( TSIZE i = 0; i < a_uiSize; i++ )
	{
		const auto wLeft = std::towlower( a_pLeft[ i ] );
		const auto wRight = std::towlower( a_pRight[ i ] );
		if ( wLeft != wRight )
			return wLeft < wRight ? -1 : 1;
		if ( wLeft == L'\0' )
			break;
	}
	return 0;
}

TSIZE T2String16::Length( const TWCHAR* a_pString )
{
	if ( a_pString == TNULL )
		return 0;

	const TWCHAR* pCursor = a_pString;
	while ( *pCursor != L'\0' )
		pCursor++;

	return TSIZE( pCursor - a_pString );
}

TWCHAR* T2String16::Copy( TWCHAR* a_pDestination, const TWCHAR* a_pSource, TSIZE a_uiSize )
{
	if ( a_uiSize == TSIZE( -1 ) )
	{
		TWCHAR* pDestination = a_pDestination;
		while ( ( *pDestination++ = *a_pSource++ ) != L'\0' )
		{
		}

		return a_pDestination;
	}

	TSIZE uiIndex = 0;
	for ( ; uiIndex < a_uiSize && a_pSource[ uiIndex ] != L'\0'; uiIndex++ )
	{
		a_pDestination[ uiIndex ] = a_pSource[ uiIndex ];
	}

	for ( ; uiIndex < a_uiSize; uiIndex++ )
	{
		a_pDestination[ uiIndex ] = L'\0';
	}

	return a_pDestination;
}

TOSHI_NAMESPACE_END
