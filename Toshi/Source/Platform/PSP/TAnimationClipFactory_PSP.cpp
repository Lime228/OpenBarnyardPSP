#include "ToshiPCH.h"
#include "TAnimationClipFactory_PSP.h"

#include "File/TFile.h"
#include "Toshi/T2Allocator.h"

TOSHI_NAMESPACE_START

TBOOL TAnimationClipFactory_PSP::CreateAnimationFromFile(
    TAnimationClip_PSP& a_rClip,
    const TSkeleton_PSP* a_pSkeleton,
    const TCHAR* a_pFilePath
) const
{
	TFile* pFile = TFile::Create( a_pFilePath, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;

	TBOOL bLoaded = TFALSE;
	const TSIZE uiFileSize = pFile->GetSize();
	if ( uiFileSize >= sizeof( TAnimationClipDataHeader_PSP ) && uiFileSize <= MAX_ANIMATION_FILE_SIZE )
	{
		void* pData = GetGlobalAllocator()->Malloc( uiFileSize, 16 );
		if ( pData != TNULL )
		{
			if ( pFile->Read( pData, uiFileSize ) == uiFileSize )
				bLoaded = CreateAnimationFromMemory( a_rClip, a_pSkeleton, pData, TUINT( uiFileSize ) );
			GetGlobalAllocator()->Free( pData );
		}
	}

	pFile->Destroy();
	return bLoaded;
}

TBOOL TAnimationClipFactory_PSP::CreateAnimationFromMemory(
    TAnimationClip_PSP& a_rClip,
    const TSkeleton_PSP* a_pSkeleton,
    const void* a_pData,
    TUINT a_uiDataSize
) const
{
	a_rClip.SetData( a_pData, a_uiDataSize );
	a_rClip.SetSkeleton( a_pSkeleton );
	const TBOOL bLoaded = a_rClip.Load();
	a_rClip.SetData( TNULL, 0 );
	return bLoaded;
}

TOSHI_NAMESPACE_END
