#include "ToshiPCH.h"
#include "T2LocaleFactory_PSP.h"

#include "File/TFile.h"
#include "Toshi/T2Allocator.h"

TOSHI_NAMESPACE_START

TBOOL T2LocaleFactory_PSP::CreateLocaleFromFile( T2Locale_PSP& a_rLocale, const TCHAR* a_pFilePath ) const
{
	TFile* pFile = TFile::Create( a_pFilePath, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;

	TBOOL bLoaded = TFALSE;
	const TSIZE uiFileSize = pFile->GetSize();
	if ( uiFileSize >= sizeof( T2LocaleDataHeader_PSP ) && uiFileSize <= MAX_LOCALE_FILE_SIZE )
	{
		void* pData = GetGlobalAllocator()->Malloc( uiFileSize, 16 );
		if ( pData != TNULL )
		{
			if ( pFile->Read( pData, uiFileSize ) == uiFileSize )
				bLoaded = CreateLocaleFromMemory( a_rLocale, pData, TUINT( uiFileSize ) );
			GetGlobalAllocator()->Free( pData );
		}
	}

	pFile->Destroy();
	return bLoaded;
}

TBOOL T2LocaleFactory_PSP::CreateLocaleFromMemory( T2Locale_PSP& a_rLocale, const void* a_pData, TUINT a_uiDataSize ) const
{
	return a_rLocale.Create( a_pData, a_uiDataSize );
}

TOSHI_NAMESPACE_END
