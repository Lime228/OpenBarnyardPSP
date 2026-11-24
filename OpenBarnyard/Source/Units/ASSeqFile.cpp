#include "pch.h"
#include "ASSeqFile.h"

#include <File/TFile.h>
#include <cstring>

#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

struct ASSeqScriptHeader
{
	TUINT32 uiName;
	TINT32  iUnknown;
	TINT32  iTrackCount;
	TUINT32 uiTrackActors;
	TUINT32 uiTrackOffsets;
};

static TUINT32 AReadSSeqWord( const TUINT8* a_pData )
{
	TUINT32 uiValue;
	std::memcpy( &uiValue, a_pData, sizeof( uiValue ) );
	return uiValue;
}

static TBOOL AValidateSSeqScript( const ASSeqFile::Header& a_rHeader, const TUINT8* a_pData, TSIZE a_uiSize, TINT& a_riTrackCount )
{
	const TSIZE uiStringRelocs = a_rHeader.uiStringRelocsOffset;
	const TSIZE uiStrings      = a_rHeader.uiStringsOffset;
	const TSIZE uiPointerRelocs = a_rHeader.uiPointerRelocsOffset;
	const TSIZE uiPointers     = a_rHeader.uiPointersOffset;
	if ( uiStringRelocs < sizeof( ASSeqScriptHeader ) || uiStringRelocs > uiStrings ||
	     uiStrings > uiPointerRelocs || uiPointerRelocs > uiPointers || uiPointers > a_uiSize ||
	     a_rHeader.uiStringRelocsSize != uiStrings - uiStringRelocs ||
	     a_rHeader.uiPointerRelocsSize != 2 * sizeof( TUINT32 ) ||
	     uiPointers - uiPointerRelocs != a_rHeader.uiPointerRelocsSize ||
	     uiStringRelocs % sizeof( TUINT32 ) || uiStrings % sizeof( TUINT32 ) || uiPointerRelocs % sizeof( TUINT32 ) )
		return TFALSE;

	const TUINT32 uiActorField  = offsetof( ASSeqScriptHeader, uiTrackActors );
	const TUINT32 uiOffsetField = offsetof( ASSeqScriptHeader, uiTrackOffsets );
	const TUINT32 uiFirstReloc  = AReadSSeqWord( a_pData + uiPointerRelocs );
	const TUINT32 uiSecondReloc = AReadSSeqWord( a_pData + uiPointerRelocs + sizeof( TUINT32 ) );
	if ( !( ( uiFirstReloc == uiActorField && uiSecondReloc == uiOffsetField ) ||
	        ( uiFirstReloc == uiOffsetField && uiSecondReloc == uiActorField ) ) )
		return TFALSE;

	ASSeqScriptHeader oScript;
	std::memcpy( &oScript, a_pData, sizeof( oScript ) );
	const TSIZE uiStringBytes = uiPointerRelocs - uiStrings;
	TBOOL bNamedScript = TFALSE;
	for ( TSIZE i = uiStringRelocs; i < uiStrings; i += sizeof( TUINT32 ) )
	{
		const TSIZE uiField = AReadSSeqWord( a_pData + i );
		if ( ( uiField != 0 && uiField < sizeof( ASSeqScriptHeader ) ) ||
		     uiField % sizeof( TUINT32 ) || uiField > uiStringRelocs - sizeof( TUINT32 ) )
			return TFALSE;
		const TSIZE uiString = AReadSSeqWord( a_pData + uiField );
		if ( uiString >= uiStringBytes || std::memchr( a_pData + uiStrings + uiString, '\0', uiStringBytes - uiString ) == TNULL )
			return TFALSE;
		if ( uiField == 0 )
			bNamedScript = a_pData[ uiStrings + uiString ] != '\0';
	}
	if ( !bNamedScript || oScript.iTrackCount < 0 )
		return TFALSE;

	const TSIZE uiPointerBytes = a_uiSize - uiPointers;
	if ( oScript.uiTrackActors > uiPointerBytes || oScript.uiTrackOffsets > uiPointerBytes ||
	     oScript.uiTrackActors % sizeof( TUINT32 ) || oScript.uiTrackOffsets % sizeof( TUINT32 ) ||
	     TSIZE( oScript.iTrackCount ) > ( uiPointerBytes - oScript.uiTrackActors ) / sizeof( TINT32 ) ||
	     TSIZE( oScript.iTrackCount ) > ( uiPointerBytes - oScript.uiTrackOffsets ) / sizeof( TUINT32 ) )
		return TFALSE;

	for ( TINT i = 0; i < oScript.iTrackCount; i++ )
	{
		TSIZE uiInstruction = AReadSSeqWord( a_pData + uiPointers + oScript.uiTrackOffsets + i * sizeof( TUINT32 ) );
		if ( uiInstruction < sizeof( ASSeqScriptHeader ) || uiInstruction % sizeof( TUINT32 ) )
			return TFALSE;
		// Every instruction, including END, has a 12-byte common header.
		constexpr TSIZE uiInstructionHeaderSize = 3 * sizeof( TUINT32 );
		while ( uiInstruction <= uiStringRelocs && uiStringRelocs - uiInstruction >= uiInstructionHeaderSize )
		{
			const TUINT32 uiOpcode = AReadSSeqWord( a_pData + uiInstruction );
			if ( uiOpcode == ASSeqFile::END_INSTRUCTION )
				break;
			if ( uiOpcode > ASSeqFile::END_INSTRUCTION || TSIZE( ASSeqFile::INSTRUCTION_SIZES[ uiOpcode ] ) > uiStringRelocs - uiInstruction )
				return TFALSE;
			uiInstruction += ASSeqFile::INSTRUCTION_SIZES[ uiOpcode ];
		}
		if ( uiInstruction > uiStringRelocs || uiStringRelocs - uiInstruction < uiInstructionHeaderSize )
			return TFALSE;
	}

	a_riTrackCount = oScript.iTrackCount;
	return TTRUE;
}

TUINT8* ASSeqFile::Read( const TCHAR* a_szPath, TINT& a_riTrackCount )
{
	a_riTrackCount = 0;
	TFile* pFile = TFile::Create( a_szPath, TFILEMODE_READ );
	if ( !pFile )
		return TNULL;

	const TSIZE uiSize = pFile->GetSize();
	TUINT8* pData = TNULL;
#ifdef TOSHI_SKU_PSP
	// Reserve the game heap for scene resources. The largest retail TSO is under 19 KiB.
	static constexpr TSIZE s_uiMaxScriptSize_PSP = 256 * 1024;
	if ( uiSize > s_uiMaxScriptSize_PSP )
	{
		pFile->Destroy();
		return TNULL;
	}
#endif
	if ( uiSize >= sizeof( Header ) + sizeof( ASSeqScriptHeader ) )
		pData = TSTATICCAST( TUINT8, TMalloc( uiSize ) );
	const TBOOL bRead = pData != TNULL && pFile->Read( pData, uiSize ) == uiSize;
	pFile->Destroy();

	if ( bRead )
	{
		Header oHeader;
		std::memcpy( &oHeader, pData, sizeof( oHeader ) );
		if ( AValidateSSeqScript( oHeader, pData + sizeof( oHeader ), uiSize - sizeof( oHeader ), a_riTrackCount ) )
			return pData;
	}
	if ( pData != TNULL )
		TFree( pData );
	return TNULL;
}
