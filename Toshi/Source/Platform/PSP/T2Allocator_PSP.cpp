#include <Toshi/Defines.h>
#include <Toshi/T2Allocator.h>

#include <cstdlib>
#include <malloc.h>

TOSHI_NAMESPACE_START

T2GlobalAllocator ms_oGlobalAllocator;

void* T2GlobalAllocator::Malloc( TSIZE a_uiSize )
{
	return std::malloc( a_uiSize );
}

void* T2GlobalAllocator::Malloc( TSIZE a_uiSize, TSIZE a_uiAlignment )
{
	const TSIZE uiAlignment = a_uiAlignment < sizeof( TUINTPTR ) ?
	                              sizeof( TUINTPTR ) :
	                              a_uiAlignment;

	return memalign( uiAlignment, a_uiSize );
}

void T2GlobalAllocator::Free( void* a_pPtr )
{
	std::free( a_pPtr );
}

TBOOL T2GlobalAllocator::CanAllocate( TSIZE ) const
{
	return TTRUE;
}

void* T2GlobalAllocator::TryMalloc( TSIZE a_uiSize )
{
	return Malloc( a_uiSize );
}

void* T2GlobalAllocator::TryMalloc( TSIZE a_uiSize, TSIZE a_uiAlignment )
{
	return Malloc( a_uiSize, a_uiAlignment );
}

TSIZE T2GlobalAllocator::GetUsedSize() const
{
	return 0;
}

TSIZE T2GlobalAllocator::GetCapacity() const
{
	return 0;
}

TOSHI_NAMESPACE_END
