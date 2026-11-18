#include "ToshiPCH.h"
#include "TTextureFactory_PSP.h"

#include "File/TFile.h"

TOSHI_NAMESPACE_START

TBOOL TTextureFactory_PSP::CreateTextureFromFile( T2Texture& a_rTexture, const TCHAR* a_pFilePath ) const
{
	TFile* pFile = TFile::Create(
	    a_pFilePath,
	    TFILEMODE_READ | TFILEMODE_NOBUFFER
	);
	if ( pFile == TNULL )
		return TFALSE;

	TBOOL       bLoaded    = TFALSE;
	const TSIZE uiFileSize = pFile->GetSize();
	if ( uiFileSize >= sizeof( T2TextureDataHeader_PSP ) && uiFileSize <= MAX_TEXTURE_FILE_SIZE )
		bLoaded = a_rTexture.LoadFromFile( *pFile );

	pFile->Destroy();
	return bLoaded;
}

TBOOL TTextureFactory_PSP::CreateTextureFromMemory( T2Texture& a_rTexture, const void* a_pData, TUINT a_uiDataSize ) const
{
	a_rTexture.SetData( a_pData, a_uiDataSize );
	const TBOOL bLoaded = a_rTexture.Load();
	a_rTexture.SetData( TNULL, 0 );
	return bLoaded;
}

TOSHI_NAMESPACE_END
