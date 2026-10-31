#pragma once

#include "Toshi/Defines.h"

TOSHI_NAMESPACE_START

struct T2LocaleDataHeader_PSP
{
	static constexpr TUINT32 MAGIC = 0x434f4c50;
	static constexpr TUINT32 VERSION = 1;

	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiStringCount;
	TUINT32 uiCodeUnitCount;
};

static_assert( sizeof( T2LocaleDataHeader_PSP ) == 16, "Unexpected PSP locale header layout" );

class T2Locale_PSP
{
public:
	static constexpr TUINT MAX_STRINGS = 32;
	static constexpr TUINT MAX_CHARACTERS = 1024;

public:
	T2Locale_PSP();

	TBOOL Create( const void* a_pData, TUINT a_uiDataSize );
	void Destroy();

	const TWCHAR* GetString( TUINT a_uiString ) const;
	TUINT GetNumStrings() const { return m_uiStringCount; }
	TBOOL IsCreated() const { return m_uiStringCount != 0; }

private:
	TUINT  m_uiStringCount;
	TUINT  m_auiOffsets[ MAX_STRINGS ];
	TWCHAR m_aCharacters[ MAX_CHARACTERS ];
};

TOSHI_NAMESPACE_END
