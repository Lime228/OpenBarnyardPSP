#include "Toshi/Defines.h"
#include "Toshi/T2String16.h"

TOSHI_NAMESPACE_START

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
