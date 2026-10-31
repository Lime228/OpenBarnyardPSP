#include "ToshiPCH.h"
#include "T2Locale_PSP.h"

#include "Toshi/TUtil.h"

TOSHI_NAMESPACE_START

T2Locale_PSP::T2Locale_PSP()
    : m_uiStringCount( 0 )
    , m_auiOffsets{}
    , m_aCharacters{}
{
}

TBOOL T2Locale_PSP::Create( const void* a_pData, TUINT a_uiDataSize )
{
	Destroy();
	if ( a_pData == TNULL || a_uiDataSize < sizeof( T2LocaleDataHeader_PSP ) )
		return TFALSE;

	const auto* pHeader = static_cast<const T2LocaleDataHeader_PSP*>( a_pData );
	if ( pHeader->uiMagic != T2LocaleDataHeader_PSP::MAGIC ||
	     pHeader->uiVersion != T2LocaleDataHeader_PSP::VERSION ||
	     pHeader->uiStringCount == 0 || pHeader->uiStringCount > MAX_STRINGS ||
	     pHeader->uiCodeUnitCount == 0 || pHeader->uiCodeUnitCount > MAX_CHARACTERS )
	{
		return TFALSE;
	}

	const TSIZE uiOffsetsSize = ( pHeader->uiStringCount + 1 ) * sizeof( TUINT32 );
	const TSIZE uiExpectedSize = sizeof( T2LocaleDataHeader_PSP ) + uiOffsetsSize +
	                             pHeader->uiCodeUnitCount * sizeof( TUINT16 );
	if ( a_uiDataSize != uiExpectedSize )
		return TFALSE;

	const auto* pOffsets = reinterpret_cast<const TUINT32*>( pHeader + 1 );
	const auto* pCodeUnits = reinterpret_cast<const TUINT16*>( reinterpret_cast<const TBYTE*>( pOffsets ) + uiOffsetsSize );
	if ( pOffsets[ 0 ] != 0 || pOffsets[ pHeader->uiStringCount ] != pHeader->uiCodeUnitCount )
		return TFALSE;

	TUINT uiOutput = 0;
	for ( TUINT i = 0; i < pHeader->uiStringCount; i++ )
	{
		const TUINT uiBegin = pOffsets[ i ];
		const TUINT uiEnd = pOffsets[ i + 1 ];
		if ( uiBegin >= uiEnd || uiEnd > pHeader->uiCodeUnitCount || pCodeUnits[ uiEnd - 1 ] != 0 )
		{
			Destroy();
			return TFALSE;
		}

		m_auiOffsets[ i ] = uiOutput;
		for ( TUINT j = uiBegin; j < uiEnd; j++ )
		{
			TUINT32 uiCharacter = pCodeUnits[ j ];
			if ( uiCharacter >= 0xd800 && uiCharacter <= 0xdbff )
			{
				if ( j + 1 >= uiEnd )
					return TFALSE;
				const TUINT32 uiLow = pCodeUnits[ ++j ];
				if ( uiLow < 0xdc00 || uiLow > 0xdfff )
					return TFALSE;
				uiCharacter = 0x10000 + ( ( uiCharacter - 0xd800 ) << 10 ) + ( uiLow - 0xdc00 );
			}
			else if ( uiCharacter >= 0xdc00 && uiCharacter <= 0xdfff )
				return TFALSE;

			if ( uiOutput >= MAX_CHARACTERS )
				return TFALSE;
			m_aCharacters[ uiOutput++ ] = TWCHAR( uiCharacter );
		}
	}

	m_uiStringCount = pHeader->uiStringCount;
	return TTRUE;
}

void T2Locale_PSP::Destroy()
{
	m_uiStringCount = 0;
	TUtil::MemClear( m_auiOffsets, sizeof( m_auiOffsets ) );
	TUtil::MemClear( m_aCharacters, sizeof( m_aCharacters ) );
}

const TWCHAR* T2Locale_PSP::GetString( TUINT a_uiString ) const
{
	return a_uiString < m_uiStringCount ? &m_aCharacters[ m_auiOffsets[ a_uiString ] ] : TNULL;
}

TOSHI_NAMESPACE_END
