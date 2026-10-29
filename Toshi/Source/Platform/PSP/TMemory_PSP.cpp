#include "Toshi/Defines.h"
#include "Toshi/Endianness.h"
#include "Toshi/Helpers.h"
#include "Core/TMemory.h"

#include <cstdlib>
#include <malloc.h>

static TSIZE NormalizeAlignment_PSP( TSIZE a_uiAlignment )
{
	TSIZE uiAlignment = a_uiAlignment < sizeof( TUINTPTR ) ?
	                            sizeof( TUINTPTR ) :
	                            a_uiAlignment;

	if ( ( uiAlignment & ( uiAlignment - 1 ) ) != 0 )
	{
		TSIZE uiPowerOfTwo = sizeof( TUINTPTR );
		while ( uiPowerOfTwo < uiAlignment )
		{
			uiPowerOfTwo <<= 1;
		}

		uiAlignment = uiPowerOfTwo;
	}

	return uiAlignment;
}

void* TMalloc( TSIZE a_uiSize )
{
	return std::malloc( a_uiSize );
}

void* TMalloc( TSIZE a_uiSize, const TCHAR*, TINT )
{
	return TMalloc( a_uiSize );
}

void* TMalloc( TSIZE a_uiSize, Toshi::TMemory::MemBlock*, const TCHAR*, TINT )
{
	return TMalloc( a_uiSize );
}

void* TRealloc( void* a_pMem, TSIZE a_uiSize, Toshi::TMemory::MemBlock* )
{
	return std::realloc( a_pMem, a_uiSize );
}

void* TMemalign( TSIZE a_uiSize, TSIZE a_uiAlignment )
{
	return memalign( NormalizeAlignment_PSP( a_uiAlignment ), a_uiSize );
}

void* TMemalign( TSIZE a_uiAlignment, TSIZE a_uiSize, Toshi::TMemory::MemBlock* )
{
	return TMemalign( a_uiSize, a_uiAlignment );
}

void TFree( void* a_pMem )
{
	std::free( a_pMem );
}
