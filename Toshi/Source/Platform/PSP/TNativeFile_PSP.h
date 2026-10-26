#pragma once

#include "File/TFile.h"

TOSHI_NAMESPACE_START

class TNativeFileSystem : public TFileSystem
{
public:
	TNativeFileSystem( const TCHAR* a_pName );
	virtual ~TNativeFileSystem();

	virtual TFile*   CreateFile( const TString8& a_rFileName, TFILEMODE a_eFileMode ) OVERRIDE;
	virtual void     DestroyFile( TFile* a_pFile ) OVERRIDE;
	virtual TBOOL    RemoveFile( const TString8& a_rFileName ) OVERRIDE;
	virtual TString8 MakeInternalPath( const TString8& a_rPath ) OVERRIDE;

	virtual TBOOL MakeDirectory( const TString8& a_rDirectory );
};

class TNativeFile : public TFile
{
public:
	TNativeFile( TNativeFileSystem* a_pFileSystem );
	TNativeFile( const TNativeFile& a_rOther );
	virtual ~TNativeFile();

	virtual TSIZE   Read( void* a_pDst, TSIZE a_uiSize ) OVERRIDE;
	virtual TSIZE   Write( const void* a_pSrc, TSIZE a_uiSize ) OVERRIDE;
	virtual TBOOL   Seek( TINT a_iOffset, TSEEK a_eOrigin = TSEEK_CUR ) OVERRIDE;
	virtual TSIZE   Tell() OVERRIDE;
	virtual void    FlushBuffers() OVERRIDE;
	virtual TSIZE   GetSize() OVERRIDE;
	virtual TUINT64 GetDate() OVERRIDE;
	virtual TCHAR   GetCChar() OVERRIDE;
	virtual TWCHAR  GetWChar() OVERRIDE;
	virtual TINT    PutCChar( TCHAR a_cCharacter ) OVERRIDE;
	virtual TINT    PutWChar( TWCHAR a_wcCharacter ) OVERRIDE;
	virtual TINT    VCPrintf( const TCHAR* a_pFormat, va_list a_Args ) OVERRIDE;
	virtual TINT    VWPrintf( const TWCHAR* a_pFormat, va_list a_Args ) OVERRIDE;

	TBOOL Open( const TString8& a_rFileName, TFILEMODE a_eMode );
	void  Close();

private:
	TINT m_iHandle;
};

TOSHI_NAMESPACE_END
