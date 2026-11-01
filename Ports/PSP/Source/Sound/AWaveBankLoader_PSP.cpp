#include <ToshiPCH.h>
#include "AWaveBankLoader_PSP.h"

#include "AWaveFactory_PSP.h"

#include <File/TFile.h>

TBOOL AWaveBankLoader_PSP::Load(
    AWaveBank_PSP& a_rBank,
    AWave_PSP* a_pWaveStorage,
    TUINT a_uiWaveCapacity,
    AStreamWave_PSP* a_pStreamStorage,
    TUINT a_uiStreamCapacity,
    const TCHAR* a_pManifestPath,
    TUINT& a_rLoadedWaves,
    TUINT& a_rLoadedStreams
) const
{
	a_rLoadedWaves = 0;
	a_rLoadedStreams = 0;
	if ( a_pWaveStorage == TNULL || a_pStreamStorage == TNULL || a_pManifestPath == TNULL ||
	     a_uiWaveCapacity == 0 || a_uiStreamCapacity == 0 ||
	     a_uiWaveCapacity + a_uiStreamCapacity > AWaveBank_PSP::MAX_WAVES ||
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
	                oHeader.uiWaveCount <= a_uiWaveCapacity + a_uiStreamCapacity &&
	                pFile->GetSize() == sizeof( oHeader ) +
	                                        oHeader.uiWaveCount * sizeof( AWaveBankDataEntry_PSP );

	AWaveFactory_PSP oWaveFactory;
	for ( TUINT i = 0; bLoaded && i < oHeader.uiWaveCount; i++ )
	{
		AWaveBankDataEntry_PSP oEntry = {};
		bLoaded = pFile->Read( &oEntry, sizeof( oEntry ) ) == sizeof( oEntry ) &&
		          oEntry.uiType <= AWaveBankEntryType_PSP_Stream &&
		          oEntry.szName[ 0 ] != '\0' && oEntry.szPath[ 0 ] != '\0' &&
		          oEntry.szName[ AWaveBank_PSP::MAX_WAVE_NAME - 1 ] == '\0' &&
		          oEntry.szPath[ sizeof( oEntry.szPath ) - 1 ] == '\0';
		if ( !bLoaded )
			continue;

		if ( oEntry.uiType == AWaveBankEntryType_PSP_Resident )
		{
			bLoaded = a_rLoadedWaves < a_uiWaveCapacity &&
			          !a_pWaveStorage[ a_rLoadedWaves ].IsCreated() &&
			          oWaveFactory.CreateWaveFromFile(
			              a_pWaveStorage[ a_rLoadedWaves ],
			              oEntry.szPath
			          ) &&
			          a_rBank.RegisterWave(
			              oEntry.szName,
			              &a_pWaveStorage[ a_rLoadedWaves ]
			          );
			if ( bLoaded )
				a_rLoadedWaves++;
			else if ( a_rLoadedWaves < a_uiWaveCapacity )
				a_pWaveStorage[ a_rLoadedWaves ].Destroy();
		}
		else
		{
			bLoaded = a_rLoadedStreams < a_uiStreamCapacity &&
			          !a_pStreamStorage[ a_rLoadedStreams ].IsCreated() &&
			          a_pStreamStorage[ a_rLoadedStreams ].Create( oEntry.szPath ) &&
			          a_rBank.RegisterStream(
			              oEntry.szName,
			              &a_pStreamStorage[ a_rLoadedStreams ]
			          );
			if ( bLoaded )
				a_rLoadedStreams++;
			else if ( a_rLoadedStreams < a_uiStreamCapacity )
				a_pStreamStorage[ a_rLoadedStreams ].Destroy();
		}
	}

	pFile->Destroy();
	if ( !bLoaded )
		Unload(
		    a_rBank,
		    a_pWaveStorage,
		    a_rLoadedWaves,
		    a_pStreamStorage,
		    a_rLoadedStreams
		);
	return bLoaded;
}

void AWaveBankLoader_PSP::Unload(
    AWaveBank_PSP& a_rBank,
    AWave_PSP* a_pWaveStorage,
    TUINT a_uiLoadedWaves,
    AStreamWave_PSP* a_pStreamStorage,
    TUINT a_uiLoadedStreams
) const
{
	a_rBank.Clear();
	if ( a_pWaveStorage == TNULL )
		return;

	for ( TUINT i = 0; i < a_uiLoadedWaves; i++ )
		a_pWaveStorage[ i ].Destroy();
	if ( a_pStreamStorage == TNULL )
		return;

	for ( TUINT i = 0; i < a_uiLoadedStreams; i++ )
		a_pStreamStorage[ i ].Destroy();
}
