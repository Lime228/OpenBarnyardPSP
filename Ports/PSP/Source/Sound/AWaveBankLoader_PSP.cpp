#include <ToshiPCH.h>
#include "AWaveBankLoader_PSP.h"

#include "AWaveFactory_PSP.h"

#include <File/TFile.h>

TBOOL AWaveBankLoader_PSP::Load(
    AWaveBank_PSP& a_rBank,
    AWave_PSP* a_pWaveStorage,
    TUINT a_uiWaveCapacity,
    const TCHAR* a_pManifestPath,
    TUINT& a_rLoadedWaves
) const
{
	a_rLoadedWaves = 0;
	if ( a_pWaveStorage == TNULL || a_pManifestPath == TNULL ||
	     a_uiWaveCapacity == 0 || a_uiWaveCapacity > AWaveBank_PSP::MAX_WAVES ||
	     a_rBank.GetCount() != 0 )
	{
		return TFALSE;
	}

	Toshi::TFile* pFile = Toshi::TFile::Create(
	    a_pManifestPath,
	    Toshi::TFILEMODE_READ | Toshi::TFILEMODE_NOBUFFER
	);
	if ( pFile == TNULL )
		return TFALSE;

	AWaveBankDataHeader_PSP oHeader = {};
	TBOOL bLoaded = pFile->Read( &oHeader, sizeof( oHeader ) ) == sizeof( oHeader ) &&
	                oHeader.uiMagic == AWaveBankDataHeader_PSP::MAGIC &&
	                oHeader.uiVersion == AWaveBankDataHeader_PSP::VERSION &&
	                oHeader.uiWaveCount != 0 &&
	                oHeader.uiWaveCount <= a_uiWaveCapacity &&
	                pFile->GetSize() == sizeof( oHeader ) +
	                                        oHeader.uiWaveCount * sizeof( AWaveBankDataEntry_PSP );

	AWaveFactory_PSP oWaveFactory;
	for ( TUINT i = 0; bLoaded && i < oHeader.uiWaveCount; i++ )
	{
		AWaveBankDataEntry_PSP oEntry = {};
		bLoaded = pFile->Read( &oEntry, sizeof( oEntry ) ) == sizeof( oEntry ) &&
		          oEntry.szName[ 0 ] != '\0' && oEntry.szPath[ 0 ] != '\0' &&
		          oEntry.szName[ AWaveBank_PSP::MAX_WAVE_NAME - 1 ] == '\0' &&
		          oEntry.szPath[ sizeof( oEntry.szPath ) - 1 ] == '\0' &&
		          !a_pWaveStorage[ i ].IsCreated();
		if ( !bLoaded )
			continue;

		bLoaded = oWaveFactory.CreateWaveFromFile( a_pWaveStorage[ i ], oEntry.szPath );
		if ( bLoaded && a_rBank.RegisterWave( oEntry.szName, &a_pWaveStorage[ i ] ) )
		{
			a_rLoadedWaves++;
		}
		else
		{
			a_pWaveStorage[ i ].Destroy();
			bLoaded = TFALSE;
		}
	}

	pFile->Destroy();
	if ( !bLoaded )
		Unload( a_rBank, a_pWaveStorage, a_rLoadedWaves );
	return bLoaded;
}

void AWaveBankLoader_PSP::Unload(
    AWaveBank_PSP& a_rBank,
    AWave_PSP* a_pWaveStorage,
    TUINT a_uiLoadedWaves
) const
{
	a_rBank.Clear();
	if ( a_pWaveStorage == TNULL )
		return;

	for ( TUINT i = 0; i < a_uiLoadedWaves; i++ )
		a_pWaveStorage[ i ].Destroy();
}
