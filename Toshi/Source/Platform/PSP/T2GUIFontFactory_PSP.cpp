#include "ToshiPCH.h"
#include "T2GUIFontFactory_PSP.h"

#include "File/TFile.h"
#include "Toshi/T2Allocator.h"

TOSHI_NAMESPACE_START

TBOOL T2GUIFontFactory_PSP::CreateFontFromFile(
    T2GUIFont_PSP& a_rFont,
    TRenderInterface_PSP* a_pRenderInterface,
    const T2Texture* a_pTexture,
    const TCHAR* a_pFilePath
) const
{
	TFile* pFile = TFile::Create( a_pFilePath, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;

	TBOOL bLoaded = TFALSE;
	const TSIZE uiFileSize = pFile->GetSize();
	if ( uiFileSize >= sizeof( T2GUIFontDataHeader_PSP ) && uiFileSize <= MAX_FONT_FILE_SIZE )
	{
		void* pData = GetGlobalAllocator()->Malloc( uiFileSize, 16 );
		if ( pData != TNULL )
		{
			if ( pFile->Read( pData, uiFileSize ) == uiFileSize )
			{
				bLoaded = CreateFontFromMemory(
				    a_rFont,
				    a_pRenderInterface,
				    a_pTexture,
				    pData,
				    TUINT( uiFileSize )
				);
			}
			GetGlobalAllocator()->Free( pData );
		}
	}

	pFile->Destroy();
	return bLoaded;
}

TBOOL T2GUIFontFactory_PSP::CreateFontFromMemory(
    T2GUIFont_PSP& a_rFont,
    TRenderInterface_PSP* a_pRenderInterface,
    const T2Texture* a_pTexture,
    const void* a_pData,
    TUINT a_uiDataSize
) const
{
	return a_rFont.Create( a_pRenderInterface, a_pTexture, a_pData, a_uiDataSize );
}

TOSHI_NAMESPACE_END
