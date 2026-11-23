#include <ToshiPCH.h>
#include "AWaveFactory_PSP.h"

#include <File/TFile.h>

TBOOL AWaveFactory_PSP::CreateWaveFromFile( AWave_PSP& a_rWave, const TCHAR* a_pFilePath ) const
{
	Toshi::TFile* pFile = Toshi::TFile::Create( a_pFilePath, Toshi::TFILEMODE_READ | Toshi::TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;

	const TBOOL bLoaded = a_rWave.Create( *pFile );
	pFile->Destroy();
	return bLoaded;
}

TBOOL AWaveFactory_PSP::CreateWaveFromMemory( AWave_PSP& a_rWave, const void* a_pData, TUINT a_uiDataSize ) const
{
	return a_rWave.Create( a_pData, a_uiDataSize );
}
