#include "ToshiPCH.h"
#include "T2Locale.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include "Core/TMemoryDebugOn.h"

TOSHI_NAMESPACE_START

// $Barnyard: FUNCTION 006be7f0
T2Locale::T2Locale( TINT langCount, TSIZE bufferSize, void* buffer )
{
	// 00662d40
	m_pBuffer     = buffer;
	m_BufferPos   = buffer;
	m_StringTable = TNULL;
	m_LangId      = -1;
	m_bOwnsBuffer = TFALSE;
#ifdef TOSHI_SKU_PSP
	m_pStringTable_PSP = TNULL;
#endif

	if ( m_pBuffer == TNULL )
	{
		TSIZE symbolTableSize = TTRB::GetSymbolTableSize( 1 );
		TSIZE headerSize      = TTRB::GetHeaderSize( 1 );
		m_pBuffer             = TMalloc( headerSize + bufferSize + symbolTableSize );
		m_BufferPos           = m_pBuffer;
		m_bOwnsBuffer         = TTRUE;
	}

	m_Locale.SetMemoryFunctions( TRBAllocator, TRBDeallocator, this );
}

// $Barnyard: FUNCTION 006be710
// $Barnyard: FUNCTION 006be890
T2Locale::~T2Locale()
{
#ifdef TOSHI_SKU_PSP
	DestroyStringTable_PSP();
#endif
	m_Locale.Close();

	if ( m_bOwnsBuffer )
	{
		TFree( m_pBuffer );
	}
}

// $Barnyard: FUNCTION 006be7d0
void* __stdcall T2Locale::TRBAllocator( TTRB::AllocType alloctype, TUINT32 size, TINT16 unk, TUINT32 unk2, void* userData )
{
	return TSTATICCAST( T2Locale, userData )->TRBAlloc( size );
}

// $Barnyard: FUNCTION 00539d40
void __stdcall T2Locale::TRBDeallocator( TTRB::AllocType alloctype, void* ptr, TINT16 unk, TUINT32 unk2, void* userData )
{
	// T2Locale doesn't have deallocator
}

// $Barnyard: FUNCTION 006be750
void T2Locale::SetLanguage( Lang langid )
{
	// 00662e30
	if ( langid != m_LangId && ( m_Locale.Close(), -1 < langid ) )
	{
#ifdef TOSHI_SKU_PSP
		DestroyStringTable_PSP();
		m_StringTable = TNULL;
		m_LangId      = -1;
		m_BufferPos   = m_pBuffer;

		if ( m_Locale.Load( GetLanguageFilename( langid ) ) == TTRB::ERROR_OK )
		{
			LocaleStrings* pRetailStringTable = m_Locale.CastSymbol<LocaleStrings>( "LocaleStrings" );
			if ( ConvertStringTable_PSP( pRetailStringTable ) )
				m_LangId = langid;
		}
#else
		m_Locale.Load( GetLanguageFilename( langid ) );
		m_StringTable = m_Locale.CastSymbol<LocaleStrings>( "LocaleStrings" );

		m_BufferPos = m_pBuffer;
		m_LangId    = langid;
#endif
	}
}

#ifdef TOSHI_SKU_PSP

static constexpr TINT  T2LOCALE_MAX_RETAIL_STRINGS_PSP    = 4096;
static constexpr TSIZE T2LOCALE_MAX_RETAIL_CHARACTERS_PSP = 131072;

TBOOL T2Locale::ConvertStringTable_PSP( LocaleStrings* a_pRetailStringTable )
{
	if ( a_pRetailStringTable == TNULL ||
	     a_pRetailStringTable->m_numstrings <= 0 ||
	     a_pRetailStringTable->m_numstrings > T2LOCALE_MAX_RETAIL_STRINGS_PSP )
		return TFALSE;

	const TINT iNumStrings    = a_pRetailStringTable->m_numstrings;
	TSIZE      uiNumCharacters = 0;
	for ( TINT i = 0; i < iNumStrings; i++ )
	{
		const TUINT16* pString = TREINTERPRETCAST( const TUINT16*, a_pRetailStringTable->Strings[ i ] );
		if ( pString == TNULL )
		{
			uiNumCharacters++;
			continue;
		}

		while ( TTRUE )
		{
			const TUINT16 uiCharacter = *pString++;
			uiNumCharacters++;
			if ( uiNumCharacters > T2LOCALE_MAX_RETAIL_CHARACTERS_PSP )
				return TFALSE;
			if ( uiCharacter == 0 )
				break;
			if ( uiCharacter >= 0xd800 && uiCharacter <= 0xdbff )
			{
				const TUINT16 uiLowCharacter = *pString++;
				if ( uiLowCharacter < 0xdc00 || uiLowCharacter > 0xdfff )
					return TFALSE;
			}
			else if ( uiCharacter >= 0xdc00 && uiCharacter <= 0xdfff )
				return TFALSE;
		}
	}

	const TSIZE uiPointerTableSize  = TSIZE( iNumStrings ) * sizeof( T2LocalisedString );
	const TSIZE uiCharacterDataSize = uiNumCharacters * sizeof( TWCHAR );
	const TSIZE uiAllocationSize    = sizeof( LocaleStrings ) + uiPointerTableSize + uiCharacterDataSize;
	TBYTE*      pAllocation         = TSTATICCAST( TBYTE, TMalloc( uiAllocationSize ) );
	if ( pAllocation == TNULL )
		return TFALSE;

	LocaleStrings* pStringTable    = TREINTERPRETCAST( LocaleStrings*, pAllocation );
	pStringTable->m_numstrings = iNumStrings;
	pStringTable->Strings      = TREINTERPRETCAST( T2LocalisedString*, pAllocation + sizeof( LocaleStrings ) );
	TWCHAR* pDestination      = TREINTERPRETCAST( TWCHAR*, pAllocation + sizeof( LocaleStrings ) + uiPointerTableSize );

	for ( TINT i = 0; i < iNumStrings; i++ )
	{
		const TUINT16* pSource = TREINTERPRETCAST( const TUINT16*, a_pRetailStringTable->Strings[ i ] );
		pStringTable->Strings[ i ] = pDestination;
		if ( pSource == TNULL )
		{
			*pDestination++ = L'\0';
			continue;
		}

		while ( TTRUE )
		{
			TUINT32 uiCharacter = *pSource++;
			if ( uiCharacter >= 0xd800 && uiCharacter <= 0xdbff )
			{
				const TUINT32 uiLowCharacter = *pSource++;
				uiCharacter = 0x10000 + ( ( uiCharacter - 0xd800 ) << 10 ) + ( uiLowCharacter - 0xdc00 );
			}

			*pDestination++ = TWCHAR( uiCharacter );
			if ( uiCharacter == 0 )
				break;
		}
	}

	m_pStringTable_PSP = pAllocation;
	m_StringTable      = pStringTable;
	return TTRUE;
}

void T2Locale::DestroyStringTable_PSP()
{
	if ( m_pStringTable_PSP != TNULL )
	{
		TFree( m_pStringTable_PSP );
		m_pStringTable_PSP = TNULL;
	}
	m_StringTable = TNULL;
}

#endif

Toshi::T2Locale::Lang T2Locale::GetLanguage() const
{
	return m_LangId;
}

TINT T2Locale::GetNumStrings() const
{
	return m_StringTable->m_numstrings;
}

// $Barnyard: FUNCTION 006be7b0
T2LocalisedString T2Locale::GetString( TINT a_iNumString )
{
	TASSERT( a_iNumString >= 0 && a_iNumString < GetNumStrings() );
	return m_StringTable->Strings[ a_iNumString ];
}

TOSHI_NAMESPACE_END
