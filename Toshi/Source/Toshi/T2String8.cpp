#include "ToshiPCH.h"
#include "T2String8.h"
#include "T2CharTraits.h"

#if defined( TOSHI_SKU_PSP )
#  include <cstdio>
#  include <strings.h>
#endif

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include "Core/TMemoryDebugOn.h"

TOSHI_NAMESPACE_START

#if defined( TOSHI_SKU_PSP )
static void T2String8IntToString_PSP( TINT a_iValue, TCHAR* a_pDst, TINT a_iRadix )
{
	static const TCHAR s_Characters[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	if ( a_iRadix < 2 || a_iRadix > 36 )
	{
		*a_pDst = '\0';
		return;
	}

	TCHAR              aBuffer[ sizeof( TUINT ) * 8 + 1 ];
	TSIZE              uiLength  = 0;
	const TBOOL        bNegative = a_iValue < 0 && a_iRadix == 10;
	TUINT              uiValue   = bNegative ?
	                                   0U - static_cast<TUINT>( a_iValue ) :
	                                   static_cast<TUINT>( a_iValue );

	do
	{
		aBuffer[ uiLength++ ] = s_Characters[ uiValue % static_cast<TUINT>( a_iRadix ) ];
		uiValue /= static_cast<TUINT>( a_iRadix );
	} while ( uiValue != 0 );

	if ( bNegative )
		*a_pDst++ = '-';

	while ( uiLength != 0 )
		*a_pDst++ = aBuffer[ --uiLength ];

	*a_pDst = '\0';
}
#endif

TCHAR T2String8::ms_aScratchMem[ T2String8::SCRATCH_MEM_SIZE ];

TINT T2String8::FormatV( TCHAR* a_pcString, const TCHAR* a_pcFormat, va_list args )
{
	TINT iResult = vsprintf( a_pcString, a_pcFormat, args );
	TASSERT( iResult != -1, "PS2/GC/X360 do not correctly support _vsnprintf, this code will cause memory to be clobbered on those platforms! Increase the size of the destination string to avoid this problem" );
	return iResult;
}

// $Barnyard: FUNCTION 006bb1d0
TINT T2String8::FormatV( TCHAR* a_pcString, TINT size, const TCHAR* a_pcFormat, va_list args )
{
#if defined( TOSHI_SKU_PSP )
	TINT iResult = vsnprintf( a_pcString, static_cast<TSIZE>( size ), a_pcFormat, args );
#else
	TINT iResult = _vsnprintf( a_pcString, size, a_pcFormat, args );
#endif
	TASSERT( iResult != -1, "PS2/GC/X360 do not correctly support _vsnprintf, this code will cause memory to be clobbered on those platforms! Increase the size of the destination string to avoid this problem" );
	a_pcString[ size - 1 ] = '\0';
	return iResult;
}

// $Barnyard: FUNCTION 006bb180
TINT T2String8::Format( TCHAR* a_pcString, const TCHAR* a_pcFormat, ... )
{
	va_list args;

	va_start( args, a_pcFormat );
	TINT iResult = FormatV( a_pcString, a_pcFormat, args );
	va_end( args );

	return iResult;
}

TINT T2String8::Format( TCHAR* a_pcString, TINT size, const TCHAR* a_pcFormat, ... )
{
	va_list args;

	va_start( args, a_pcFormat );
	TINT iResult = FormatV( a_pcString, size, a_pcFormat, args );
	va_end( args );

	return iResult;
}

TINT T2String8::Compare( const TCHAR* str1, const TCHAR* str2, TSIZE size )
{
	if ( size != -1 )
		return strncmp( str1, str2, size );

	return strcmp( str1, str2 );
}

// $Barnyard: FUNCTION 006bb0e0
TINT T2String8::CompareNoCase( const TCHAR* str1, const TCHAR* str2, TSIZE size /*= -1*/ )
{
#if defined( TOSHI_SKU_PSP )
	if ( size != -1 )
		return strncasecmp( str1, str2, size );

	return strcasecmp( str1, str2 );
#else
	if ( size != -1 )
		return _strnicmp( str1, str2, size );

	return _stricmp( str1, str2 );
#endif
}

// $Barnyard: FUNCTION 006bb120
TCHAR* T2String8::Copy( TCHAR* dst, const TCHAR* src, TSIZE size /*= -1*/ )
{
	if ( size != -1 )
		return strncpy( dst, src, size );

	return strcpy( dst, src );
}

TCHAR* T2String8::Concat( TCHAR* dst, const TCHAR* src, TSIZE size /*= -1*/ )
{
	if ( size != -1 )
		return strncat( dst, src, size );

	return strcat( dst, src );
}

TCHAR* T2String8::CopySafe( TCHAR* dst, const TCHAR* src, TSIZE size )
{
	TSIZE srcLen = Length( src );
	size         = TMath::Min( size - 1, srcLen );
	strncpy( dst, src, size );
	dst[ size ] = '\0';
	return dst;
}

const TCHAR* T2String8::FindChar( const TCHAR* str, TCHAR character )
{
	while ( TTRUE )
	{
		if ( *str == '\0' ) return TNULL;
		if ( *str == character ) return str;
		str++;
	}

	return TNULL;
}

TCHAR* T2String8::FindChar( TCHAR* str, TCHAR character )
{
	while ( TTRUE )
	{
		if ( *str == '\0' ) return TNULL;
		if ( *str == character ) return str;
		str++;
	}

	return TNULL;
}

// $Barnyard: FUNCTION 006bb200
const TCHAR* T2String8::FindString( const TCHAR* str, const TCHAR* substr )
{
	return strstr( str, substr );
}

TCHAR* T2String8::FindString( TCHAR* str, const TCHAR* substr )
{
	return strstr( str, substr );
}

// $Barnyard: FUNCTION 006bb160
TSIZE T2String8::Length( const TCHAR* str )
{
	return strlen( str );
}

TBOOL T2String8::IsLowerCase( const TCHAR* str )
{
	while ( *str != '\0' && T2Char8::IsLowerCase( *str ) )
		str++;

	return *str == '\0';
}

TBOOL T2String8::IsUpperCase( const TCHAR* str )
{
	while ( *str != '\0' && T2Char8::IsUpperCase( *str ) )
		str++;

	return *str == '\0';
}

void T2String8::ToLowerCase( TCHAR* str )
{
	while ( *str )
	{
		*str = T2Char8::ToLowerCase( *str );
		str++;
	}
}

void T2String8::ToUpperCase( TCHAR* str )
{
	while ( *str )
	{
		*str = T2Char8::ToUpperCase( *str );
		str++;
	}
}

// $Barnyard: FUNCTION 006bb220
void T2String8::IntToString( TINT value, TCHAR* dst, TINT radix )
{
#if defined( TOSHI_SKU_PSP )
	T2String8IntToString_PSP( value, dst, radix );
#else
	_itoa( value, dst, radix );
#endif
}

void T2String8::IntToString( TINT value, TCHAR* dst, TINT unused, TINT radix )
{
#if defined( TOSHI_SKU_PSP )
	T2String8IntToString_PSP( value, dst, radix );
#else
	_itoa( value, dst, radix );
#endif
}

// $Barnyard: FUNCTION 006bb2a0
TINT T2String8::StringToInt( const TCHAR* src )
{
	return atoi( src );
}

TFLOAT T2String8::StringToFloat( const TCHAR* src )
{
	return TFLOAT( atof( src ) );
}

TCHAR* T2String8::SkipSpaces( TCHAR* str )
{
	TCHAR* pszCursor = str;
	while ( T2Char8::IsSpace( *pszCursor ) ) pszCursor++;

	return pszCursor;
}

const TCHAR* T2String8::SkipSpaces( const TCHAR* str )
{
	const TCHAR* pszCursor = str;
	while ( T2Char8::IsSpace( *pszCursor ) ) pszCursor++;

	return pszCursor;
}

TCHAR* T2String8::CreateCopy( const TCHAR* a_pcString, TSIZE a_uiSize /*= -1 */ )
{
	TSIZE uiSize = ( a_uiSize == -1 ) ? T2String8::Length( a_pcString ) : a_uiSize;

	TCHAR* pcBuffer = new TCHAR[ uiSize + 1 ];
	TUtil::MemCopy( pcBuffer, a_pcString, uiSize * sizeof( TCHAR ) );
	pcBuffer[ uiSize ] = '\0';

	return pcBuffer;
}

TOSHI_NAMESPACE_END
