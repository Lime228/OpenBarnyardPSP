#pragma once
#include <bit>
#include <cstring>
#include <type_traits>

typedef TUINT8 Endianess;
enum Endianess_ : Endianess
{
	Endianess_Little,
	Endianess_Big
};

inline constexpr Endianess CURRENT_ENDIANESS =
#if defined( TOSHI_SKU_WINDOWS ) || defined( TOSHI_SKU_PSP )
    Endianess_Little;
#else
#  error Unsupported platform endianness
#endif

TFORCEINLINE constexpr TUINT32 SWAP32( const TUINT8 bytes[ 4 ] )
{
	return ( bytes[ 3 ] << 0 ) | ( bytes[ 2 ] << 8 ) | ( bytes[ 1 ] << 16 ) | ( bytes[ 0 ] << 24 );
}

TFORCEINLINE constexpr TUINT16 SWAP16( const TUINT8 bytes[ 2 ] )
{
	return ( bytes[ 1 ] << 0 ) | ( bytes[ 0 ] << 8 );
}

TFORCEINLINE TFLOAT SWAPFLOAT( const TUINT8 bytes[ 4 ] )
{
	const TUINT32 uiResult = SWAP32( bytes );
	return std::bit_cast<TFLOAT>( uiResult );
}

TFORCEINLINE TUINT32 PARSEDWORD( const TUINT32 value )
{
	if constexpr ( std::endian::native == std::endian::little ) return value;
	else return SWAP32( (const TUINT8*)&value );
}

TFORCEINLINE TUINT32 PARSEDWORD( const TUINT8* value )
{
	if constexpr ( std::endian::native == std::endian::little )
	{
		TUINT32 uiResult;
		std::memcpy( &uiResult, value, sizeof( uiResult ) );
		return uiResult;
	}
	else return SWAP32( value );
}

TFORCEINLINE TUINT32 PARSEDWORD_BIG( const TUINT32 value )
{
	if constexpr ( std::endian::native == std::endian::little ) return SWAP32( (const TUINT8*)&value );
	else return value;
}

TFORCEINLINE TUINT32 PARSEDWORD_BIG( const TUINT8* value )
{
	if constexpr ( std::endian::native == std::endian::little ) return SWAP32( value );
	else
	{
		TUINT32 uiResult;
		std::memcpy( &uiResult, value, sizeof( uiResult ) );
		return uiResult;
	}
}

TFORCEINLINE TUINT16 PARSEWORD( TUINT16 value )
{
	if constexpr ( std::endian::native == std::endian::little ) return value;
	else return SWAP16( (const TUINT8*)&value );
}

TFORCEINLINE TUINT16 PARSEWORD( const TUINT8* value )
{
	if constexpr ( std::endian::native == std::endian::little )
	{
		TUINT16 uiResult;
		std::memcpy( &uiResult, value, sizeof( uiResult ) );
		return uiResult;
	}
	else return SWAP16( value );
}

TFORCEINLINE TUINT32 PARSEWORD_BIG( TUINT32 value )
{
	if constexpr ( std::endian::native == std::endian::little ) return SWAP16( (const TUINT8*)&value );
	else return value;
}

TFORCEINLINE TUINT32 PARSEWORD_BIG( const TUINT8* value )
{
	if constexpr ( std::endian::native == std::endian::little ) return SWAP16( value );
	else
	{
		TUINT16 uiResult;
		std::memcpy( &uiResult, value, sizeof( uiResult ) );
		return uiResult;
	}
}

TFORCEINLINE TFLOAT PARSEFLOAT( TFLOAT value )
{
	if constexpr ( std::endian::native == std::endian::little ) return value;
	else return SWAPFLOAT( (const TUINT8*)&value );
}

TFORCEINLINE TFLOAT PARSEFLOAT( const TUINT8* value )
{
	if constexpr ( std::endian::native == std::endian::little )
	{
		TFLOAT fResult;
		std::memcpy( &fResult, value, sizeof( fResult ) );
		return fResult;
	}
	else return SWAPFLOAT( value );
}

TFORCEINLINE TFLOAT PARSEFLOAT_BIG( TFLOAT value )
{
	if constexpr ( std::endian::native == std::endian::little ) return SWAPFLOAT( (const TUINT8*)&value );
	else return value;
}

TFORCEINLINE TFLOAT PARSEFLOAT_BIG( const TUINT8* value )
{
	if constexpr ( std::endian::native == std::endian::little ) return SWAPFLOAT( value );
	else
	{
		TFLOAT fResult;
		std::memcpy( &fResult, value, sizeof( fResult ) );
		return fResult;
	}
}

template <typename T>
TFORCEINLINE T CONVERTENDIANESS( Endianess a_eEndianess, T a_numValue )
{
	switch ( a_eEndianess )
	{
		case Endianess_Little:
			if constexpr ( std::is_same<T, TFLOAT>::value )
				return PARSEFLOAT( a_numValue );
			else if constexpr ( sizeof( T ) == 4 )
				return (T)PARSEDWORD( *(TUINT32*)( TREINTERPRETCAST( void*, &a_numValue ) ) );
			else if constexpr ( sizeof( T ) == 2 )
				return (T)PARSEWORD( *(TUINT16*)( TREINTERPRETCAST( void*, &a_numValue ) ) );
			break;
		case Endianess_Big:
			if constexpr ( std::is_same<T, TFLOAT>::value )
				return PARSEFLOAT_BIG( a_numValue );
			else if constexpr ( sizeof( T ) == 4 )
				return (T)PARSEDWORD_BIG( *(TUINT32*)( TREINTERPRETCAST( void*, &a_numValue ) ) );
			else if constexpr ( sizeof( T ) == 2 )
				return (T)PARSEWORD_BIG( *(TUINT16*)( TREINTERPRETCAST( void*, &a_numValue ) ) );
			break;
	}

	return a_numValue;
}
