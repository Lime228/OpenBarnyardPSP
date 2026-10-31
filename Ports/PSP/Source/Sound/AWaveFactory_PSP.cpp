#include <ToshiPCH.h>
#include "AWaveFactory_PSP.h"

#include <File/TFile.h>
#include <Toshi/T2Allocator.h>

TBOOL AWaveFactory_PSP::CreateWaveFromFile( AWave_PSP& a_rWave, const TCHAR* a_pFilePath ) const
{
	Toshi::TFile* pFile = Toshi::TFile::Create( a_pFilePath, Toshi::TFILEMODE_READ | Toshi::TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;

	TBOOL bLoaded = TFALSE;
	const TSIZE uiFileSize = pFile->GetSize();
	if ( uiFileSize >= sizeof( AWaveDataHeader_PSP ) && uiFileSize <= MAX_WAVE_FILE_SIZE )
	{
		void* pData = Toshi::GetGlobalAllocator()->Malloc( uiFileSize, 16 );
		if ( pData != TNULL )
		{
			if ( pFile->Read( pData, uiFileSize ) == uiFileSize )
				bLoaded = CreateWaveFromMemory( a_rWave, pData, TUINT( uiFileSize ) );
			Toshi::GetGlobalAllocator()->Free( pData );
		}
	}

	pFile->Destroy();
	return bLoaded;
}

TBOOL AWaveFactory_PSP::CreateWaveFromMemory( AWave_PSP& a_rWave, const void* a_pData, TUINT a_uiDataSize ) const
{
	return a_rWave.Create( a_pData, a_uiDataSize );
}
