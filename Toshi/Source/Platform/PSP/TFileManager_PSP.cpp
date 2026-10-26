#include "ToshiPCH.h"
#include "TNativeFile_PSP.h"

TOSHI_NAMESPACE_START

TBOOL TFileManager::Create()
{
	CreateCommon();
	TFileManager* pFileManager = TFileManager::GetSingletonSafe();

	( new TNativeFileSystem( "local" ) )->SetPrefix( "." );
	( new TNativeFileSystem( "abs" ) )->SetPrefix( "" );
	pFileManager->SetSystemPath( "local" );

	return TTRUE;
}

TOSHI_NAMESPACE_END
