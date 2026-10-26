#include "ToshiPCH.h"
#include "TNativeFile_PSP.h"

#include "Toshi/Platform.h"
#include "Toshi/T2String8.h"

#include <pspiofilemgr.h>

#include <cwchar>

TOSHI_NAMESPACE_START

TNativeFileSystem::TNativeFileSystem( const TCHAR* a_pName )
    : TFileSystem( a_pName )
    , m_iDirectoryHandle( -1 )
{
	m_NextFileHandle = TNULL;
	TFileManager::GetSingletonSafe()->MountFileSystem( this );
}

TNativeFileSystem::~TNativeFileSystem()
{
	if ( m_iDirectoryHandle >= 0 )
		sceIoDclose( m_iDirectoryHandle );

	UnmountFileSystem();
}

TFile* TNativeFileSystem::CreateFile( const TString8& a_rFileName, TFILEMODE a_eFileMode )
{
	TNativeFile* pFile = new TNativeFile( this );
	if ( !pFile->Open( MakeInternalPath( a_rFileName ), a_eFileMode ) )
	{
		delete pFile;
		return TNULL;
	}

	return pFile;
}

void TNativeFileSystem::DestroyFile( TFile* a_pFile )
{
	delete a_pFile;
}

TBOOL TNativeFileSystem::RemoveFile( const TString8& a_rFileName )
{
	const TString8 oInternalPath = MakeInternalPath( a_rFileName );
	return sceIoRemove( oInternalPath.GetString() ) >= 0;
}

TString8 TNativeFileSystem::MakeInternalPath( const TString8& a_rPath )
{
	TString8 oPath = a_rPath;
	FixPathSlashes( oPath );

	if ( oPath.Find( ':' ) >= 0 )
		return oPath;

	TString8 oInternalPath = m_Prefix;
	if ( oInternalPath.Length() > 0 && oInternalPath[ oInternalPath.Length() - 1 ] == '/' )
		oInternalPath.Truncate( oInternalPath.Length() - 1 );

	oInternalPath += TFileManager::GetSingletonSafe()->MakeAbsolutePath( oPath );
	FixPathSlashes( oInternalPath );
	return oInternalPath;
}

TBOOL TNativeFileSystem::MakeDirectory( const TString8& a_rDirectory )
{
	const TString8 oInternalPath = MakeInternalPath( a_rDirectory );
	return sceIoMkdir( oInternalPath.GetString(), 0777 ) >= 0;
}

TBOOL TNativeFileSystem::GetFirstFile( const TString8& a_rPath, TString8& a_rOutFileName, TFINDFILE a_ui8Flags )
{
	if ( m_iDirectoryHandle >= 0 )
		sceIoDclose( m_iDirectoryHandle );

	const TString8 oInternalPath = MakeInternalPath( a_rPath );
	m_iDirectoryHandle = sceIoDopen( oInternalPath.GetString() );
	return m_iDirectoryHandle >= 0 && GetNextFile( a_rOutFileName, a_ui8Flags );
}

TBOOL TNativeFileSystem::GetNextFile( TString8& a_rOutFileName, TFINDFILE a_ui8Flags )
{
	if ( m_iDirectoryHandle < 0 )
		return TFALSE;

	SceIoDirent oEntry = {};
	while ( sceIoDread( m_iDirectoryHandle, &oEntry ) > 0 )
	{
		if ( T2String8::Compare( oEntry.d_name, "." ) == 0 ||
		     T2String8::Compare( oEntry.d_name, ".." ) == 0 )
		{
			continue;
		}

		const TBOOL bDirectory = FIO_S_ISDIR( oEntry.d_stat.st_mode );
		if ( ( bDirectory && ( a_ui8Flags & TFINDFILE_DIRECTORY ) ) ||
		     ( !bDirectory && ( a_ui8Flags & TFINDFILE_FILE ) ) )
		{
			a_rOutFileName = oEntry.d_name;
			return TTRUE;
		}
	}

	sceIoDclose( m_iDirectoryHandle );
	m_iDirectoryHandle = -1;
	return TFALSE;
}

TNativeFile::TNativeFile( TNativeFileSystem* a_pFileSystem )
    : TFile( a_pFileSystem )
    , m_iHandle( -1 )
{
}

TNativeFile::TNativeFile( const TNativeFile& a_rOther )
    : TFile( a_rOther )
    , m_iHandle( a_rOther.m_iHandle )
{
}

TNativeFile::~TNativeFile()
{
	Close();
}

TSIZE TNativeFile::Read( void* a_pDst, TSIZE a_uiSize )
{
	const TINT iRead = sceIoRead( m_iHandle, a_pDst, a_uiSize );
	return iRead >= 0 ? TSIZE( iRead ) : 0;
}

TSIZE TNativeFile::Write( const void* a_pSrc, TSIZE a_uiSize )
{
	const TINT iWritten = sceIoWrite( m_iHandle, a_pSrc, a_uiSize );
	return iWritten >= 0 ? TSIZE( iWritten ) : 0;
}

TBOOL TNativeFile::Seek( TINT a_iOffset, TSEEK a_eOrigin )
{
	static const TINT s_Origins[] = { PSP_SEEK_SET, PSP_SEEK_CUR, PSP_SEEK_END };
	return sceIoLseek32( m_iHandle, a_iOffset, s_Origins[ a_eOrigin ] ) >= 0;
}

TSIZE TNativeFile::Tell()
{
	const TINT iPosition = sceIoLseek32( m_iHandle, 0, PSP_SEEK_CUR );
	return iPosition >= 0 ? TSIZE( iPosition ) : 0;
}

void TNativeFile::FlushBuffers()
{
	sceIoSync( TNULL, 0 );
}

TSIZE TNativeFile::GetSize()
{
	const TINT iPosition = sceIoLseek32( m_iHandle, 0, PSP_SEEK_CUR );
	const TINT iSize     = sceIoLseek32( m_iHandle, 0, PSP_SEEK_END );
	if ( iPosition >= 0 )
		sceIoLseek32( m_iHandle, iPosition, PSP_SEEK_SET );

	return iSize >= 0 ? TSIZE( iSize ) : 0;
}

TUINT64 TNativeFile::GetDate()
{
	return 0;
}

TCHAR TNativeFile::GetCChar()
{
	TCHAR cValue = TCHAR( -1 );
	return Read( &cValue, sizeof( cValue ) ) == sizeof( cValue ) ? cValue : TCHAR( -1 );
}

TWCHAR TNativeFile::GetWChar()
{
	TWCHAR wcValue = TWCHAR( -1 );
	return Read( &wcValue, sizeof( wcValue ) ) == sizeof( wcValue ) ? wcValue : TWCHAR( -1 );
}

TINT TNativeFile::PutCChar( TCHAR a_cCharacter )
{
	return Write( &a_cCharacter, sizeof( a_cCharacter ) ) == sizeof( a_cCharacter ) ?
	           TINT( TUINT8( a_cCharacter ) ) :
	           -1;
}

TINT TNativeFile::PutWChar( TWCHAR a_wcCharacter )
{
	return Write( &a_wcCharacter, sizeof( a_wcCharacter ) ) == sizeof( a_wcCharacter ) ?
	           TINT( a_wcCharacter ) :
	           -1;
}

TINT TNativeFile::VCPrintf( const TCHAR* a_pFormat, va_list a_Args )
{
	TCHAR aBuffer[ 512 ];
	TINT  iLength = T2String8::FormatV( aBuffer, TINT( TARRAYSIZE( aBuffer ) ), a_pFormat, a_Args );
	if ( iLength < 0 )
		iLength = TINT( TARRAYSIZE( aBuffer ) - 1 );

	return TINT( Write( aBuffer, TSIZE( iLength ) ) );
}

TINT TNativeFile::VWPrintf( const TWCHAR* a_pFormat, va_list a_Args )
{
	TWCHAR aBuffer[ 512 ];
	TINT   iLength = vswprintf( aBuffer, TARRAYSIZE( aBuffer ), a_pFormat, a_Args );
	if ( iLength < 0 )
		iLength = TINT( TARRAYSIZE( aBuffer ) - 1 );

	const TSIZE uiWritten = Write( aBuffer, TSIZE( iLength ) * sizeof( TWCHAR ) );
	return TINT( uiWritten / sizeof( TWCHAR ) );
}

TBOOL TNativeFile::Open( const TString8& a_rFileName, TFILEMODE a_eMode )
{
	TINT iFlags = 0;
	if ( ( a_eMode & TFILEMODE_READWRITE ) ||
	     ( ( a_eMode & TFILEMODE_READ ) && ( a_eMode & TFILEMODE_WRITE ) ) )
		iFlags |= PSP_O_RDWR;
	else if ( a_eMode & TFILEMODE_READ )
		iFlags |= PSP_O_RDONLY;
	else
		iFlags |= PSP_O_WRONLY;

	if ( a_eMode & TFILEMODE_CREATENEW )
		iFlags |= PSP_O_CREAT | PSP_O_TRUNC;

	m_iHandle = sceIoOpen( a_rFileName.GetString(), iFlags, 0777 );
	return m_iHandle >= 0;
}

void TNativeFile::Close()
{
	if ( m_iHandle >= 0 )
	{
		sceIoClose( m_iHandle );
		m_iHandle = -1;
	}
}

TOSHI_NAMESPACE_END
