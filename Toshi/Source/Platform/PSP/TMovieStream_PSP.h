#pragma once

#include <Toshi/Defines.h>

TOSHI_NAMESPACE_START

class TFile;

class TMovieStream_PSP
{
public:
	static constexpr TUINT32 WIDTH = 480;
	static constexpr TUINT32 HEIGHT = 272;
	static constexpr TSIZE PIXEL_BYTES = WIDTH * HEIGHT * sizeof( TUINT32 );

	TMovieStream_PSP() = default;
	~TMovieStream_PSP();
	TMovieStream_PSP( const TMovieStream_PSP& ) = delete;
	TMovieStream_PSP& operator=( const TMovieStream_PSP& ) = delete;

	// The caller keeps the file alive and owns closing it.
	TBOOL Open( TFile& a_rFile );
	void Close();
	TBOOL DecodeNextFrame();
	TUINT32 GetFrameCount() const { return m_uiFrameCount; }
	TUINT32 GetRateNumerator() const { return m_uiRateNumerator; }
	TUINT32 GetRateDenominator() const { return m_uiRateDenominator; }
	const TUINT32* GetPixels() const { return m_pPixels; }
	TSIZE GetBufferBytes() const { return m_pPixels != TNULL ? PIXEL_BYTES + m_uiMaxJPEG : 0; }
	const TCHAR* GetStage() const { return m_pStage; }

private:
	TFile* m_pFile = TNULL;
	TUINT8* m_pJPEG = TNULL;
	TUINT32* m_pPixels = TNULL;
	TSIZE m_uiBytesLeft = 0;
	TUINT32 m_uiFrameCount = 0;
	TUINT32 m_uiFrame = 0;
	TUINT32 m_uiRateNumerator = 0;
	TUINT32 m_uiRateDenominator = 0;
	TUINT32 m_uiMaxJPEG = 0;
	TUINT32 m_uiAudioTracks = 0;
	TUINT64 m_uiAudioPhase = 0;
	const TCHAR* m_pStage = "closed";
	TBOOL m_bModuleLoaded = TFALSE;
	TBOOL m_bJPEGInitialised = TFALSE;
	TBOOL m_bJPEGCreated = TFALSE;
};

TOSHI_NAMESPACE_END
