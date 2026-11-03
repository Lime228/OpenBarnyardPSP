#include "ToshiPCH.h"
#include "TSkeletonFactory_PSP.h"

#include "File/TFile.h"
#include "Toshi/T2Allocator.h"

TOSHI_NAMESPACE_START

TBOOL TSkeletonFactory_PSP::CreateSkeletonFromFile( TSkeleton_PSP& a_rSkeleton, const TCHAR* a_pFilePath ) const
{
	TFile* pFile = TFile::Create(
	    a_pFilePath,
	    TFILEMODE_READ | TFILEMODE_NOBUFFER
	);
	if ( pFile == TNULL )
		return TFALSE;

	TBOOL       bLoaded    = TFALSE;
	const TSIZE uiFileSize = pFile->GetSize();
	if ( uiFileSize >= sizeof( TSkeletonDataHeader_PSP ) && uiFileSize <= MAX_SKELETON_FILE_SIZE )
	{
		void* pData = GetGlobalAllocator()->Malloc( uiFileSize, 16 );
		if ( pData != TNULL )
		{
			if ( pFile->Read( pData, uiFileSize ) == uiFileSize )
				bLoaded = CreateSkeletonFromMemory( a_rSkeleton, pData, TUINT( uiFileSize ) );
			GetGlobalAllocator()->Free( pData );
		}
	}

	pFile->Destroy();
	return bLoaded;
}

TBOOL TSkeletonFactory_PSP::CreateSkeletonFromMemory( TSkeleton_PSP& a_rSkeleton, const void* a_pData, TUINT a_uiDataSize ) const
{
	a_rSkeleton.SetData( a_pData, a_uiDataSize );
	const TBOOL bLoaded = a_rSkeleton.Load();
	a_rSkeleton.SetData( TNULL, 0 );
	return bLoaded;
}

TOSHI_NAMESPACE_END
