#include "ToshiPCH.h"
#include "TModelFactory_PSP.h"

#include "File/TFile.h"
#include "Toshi/T2Allocator.h"

TOSHI_NAMESPACE_START

TBOOL TModelFactory_PSP::CreateModelFromFile( TModel_PSP& a_rModel, const TCHAR* a_pFilePath ) const
{
	TFile* pFile = TFile::Create(
	    a_pFilePath,
	    TFILEMODE_READ | TFILEMODE_NOBUFFER
	);
	if ( pFile == TNULL )
		return TFALSE;

	TBOOL       bLoaded    = TFALSE;
	const TSIZE uiFileSize = pFile->GetSize();
	if ( uiFileSize >= sizeof( TModelDataHeader_PSP ) && uiFileSize <= MAX_MODEL_FILE_SIZE )
	{
		void* pData = GetGlobalAllocator()->Malloc( uiFileSize, 16 );
		if ( pData != TNULL )
		{
			if ( pFile->Read( pData, uiFileSize ) == uiFileSize )
				bLoaded = CreateModelFromMemory( a_rModel, pData, TUINT( uiFileSize ) );
			GetGlobalAllocator()->Free( pData );
		}
	}

	pFile->Destroy();
	return bLoaded;
}

TBOOL TModelFactory_PSP::CreateModelFromMemory( TModel_PSP& a_rModel, const void* a_pData, TUINT a_uiDataSize ) const
{
	a_rModel.SetData( a_pData, a_uiDataSize );
	const TBOOL bLoaded = a_rModel.Load();
	a_rModel.SetData( TNULL, 0 );
	return bLoaded;
}

TOSHI_NAMESPACE_END
