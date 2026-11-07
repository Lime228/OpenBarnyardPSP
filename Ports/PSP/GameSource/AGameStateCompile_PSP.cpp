#include <pch.h>

static AGameState* GetOriginalGameStateCompileGate_PSP( AGameState* a_pState )
{
	return a_pState;
}

static_assert( sizeof( GetOriginalGameStateCompileGate_PSP( TNULL ) ) == sizeof( AGameState* ) );
