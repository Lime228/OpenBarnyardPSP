#include "ToshiPCH.h"
#include "TMovieStream_PSP.h"

#include <Core/TMemory.h>
#include <File/TFile.h>

#include <pspjpeg.h>
#include <pspkernel.h>
#include <psputility_avmodules.h>

TOSHI_NAMESPACE_START

static constexpr TUINT32 MOVIE_MAGIC_PSP = 0x46564d50; // PMVF
static constexpr TUINT32 MOVIE_VERSION_PSP = 2;
static constexpr TUINT32 MOVIE_RATE_LIMIT_PSP = 30;
static constexpr TUINT32 MOVIE_MAX_JPEG_PSP = 64 * 1024;
static constexpr TUINT32 MOVIE_MAX_TRACKS_PSP = 4;
static constexpr TUINT32 MOVIE_AUDIO_RATE_PSP = 44100;
static constexpr TUINT32 MOVIE_PCM_SAMPLE_BYTES_PSP = 4;

struct TMovieHeader_PSP
{
	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiHeaderBytes;
	TUINT32 uiWidth;
	TUINT32 uiHeight;
	TUINT32 uiRateNumerator;
	TUINT32 uiRateDenominator;
	TUINT32 uiFrames;
	TUINT32 uiMaxJPEG;
	TUINT32 uiTracks;
	TUINT32 uiAudioRate;
	TUINT32 uiMaxPCM;
};

static_assert( sizeof( TMovieHeader_PSP ) == 48 );

static TBOOL IsMovieJPEG_PSP( const TUINT8* a_pData, TSIZE a_uiSize )
{
	if ( a_uiSize < 4 || a_pData[ 0 ] != 0xff || a_pData[ 1 ] != 0xd8 ||
	     a_pData[ a_uiSize - 2 ] != 0xff || a_pData[ a_uiSize - 1 ] != 0xd9 )
		return TFALSE;

	TBOOL bFrame = TFALSE;
	for ( TSIZE offset = 2; offset + 4 <= a_uiSize; )
	{
		const TUINT8* pMarker = a_pData + offset;
		const TSIZE uiLength = ( TSIZE( pMarker[ 2 ] ) << 8 ) | pMarker[ 3 ];
		if ( pMarker[ 0 ] != 0xff || uiLength < 2 || uiLength > a_uiSize - offset - 2 )
			return TFALSE;
		if ( pMarker[ 1 ] == 0xc0 )
		{
			if ( bFrame || uiLength != 17 || pMarker[ 4 ] != 8 || pMarker[ 9 ] != 3 ||
			     ( ( TUINT32( pMarker[ 5 ] ) << 8 ) | pMarker[ 6 ] ) != TMovieStream_PSP::HEIGHT ||
			     ( ( TUINT32( pMarker[ 7 ] ) << 8 ) | pMarker[ 8 ] ) != TMovieStream_PSP::WIDTH ||
			     pMarker[ 11 ] != 0x22 || pMarker[ 14 ] != 0x11 || pMarker[ 17 ] != 0x11 )
				return TFALSE;
			bFrame = TTRUE;
		}
		else if ( pMarker[ 1 ] == 0xda )
		{
			return bFrame;
		}
		offset += 2 + uiLength;
	}
	return TFALSE;
}

TMovieStream_PSP::~TMovieStream_PSP()
{
	Close();
}

TBOOL TMovieStream_PSP::Open( TFile& a_rFile )
{
	Close();
	m_pStage = "header";
	TMovieHeader_PSP oHeader = {};
	if ( !a_rFile.Seek( 0, TSEEK_SET ) || a_rFile.Read( &oHeader, sizeof( oHeader ) ) != sizeof( oHeader ) ||
	     oHeader.uiMagic != MOVIE_MAGIC_PSP || oHeader.uiVersion != MOVIE_VERSION_PSP || oHeader.uiHeaderBytes != sizeof( oHeader ) )
		return TFALSE;

	const TSIZE uiFileSize = a_rFile.GetSize();
	if ( oHeader.uiWidth != WIDTH || oHeader.uiHeight != HEIGHT || oHeader.uiRateDenominator == 0 ||
	     oHeader.uiRateNumerator < oHeader.uiRateDenominator ||
	     oHeader.uiRateNumerator > TUINT64( oHeader.uiRateDenominator ) * MOVIE_RATE_LIMIT_PSP ||
	     oHeader.uiFrames == 0 || uiFileSize < sizeof( oHeader ) || oHeader.uiFrames > ( uiFileSize - sizeof( oHeader ) ) / 5 ||
	     oHeader.uiMaxJPEG == 0 || oHeader.uiMaxJPEG > MOVIE_MAX_JPEG_PSP || oHeader.uiTracks > MOVIE_MAX_TRACKS_PSP )
		return TFALSE;
	const TUINT32 uiAudioRate = oHeader.uiTracks != 0 ? MOVIE_AUDIO_RATE_PSP : 0;
	const TUINT64 uiAudioStep = TUINT64( uiAudioRate ) * oHeader.uiRateDenominator;
	const TUINT32 uiPCM = ( ( uiAudioStep + oHeader.uiRateNumerator - 1 ) / oHeader.uiRateNumerator ) * MOVIE_PCM_SAMPLE_BYTES_PSP;
	if ( oHeader.uiAudioRate != uiAudioRate || oHeader.uiMaxPCM != uiPCM )
		return TFALSE;

	m_pStage = "buffers";
	m_pJPEG = TSTATICCAST( TUINT8, TMemalign( oHeader.uiMaxJPEG, 64 ) );
	m_pPixels = TSTATICCAST( TUINT32, TMemalign( PIXEL_BYTES, 64 ) );
	if ( m_pJPEG == TNULL || m_pPixels == TNULL )
	{
		Close();
		return TFALSE;
	}
	m_pStage = "av-module";
	m_bModuleLoaded = sceUtilityLoadAvModule( PSP_AV_MODULE_AVCODEC ) >= 0;
	m_bJPEGInitialised = m_bModuleLoaded && sceJpegInitMJpeg() >= 0;
	m_bJPEGCreated = m_bJPEGInitialised && sceJpegCreateMJpeg( WIDTH, HEIGHT ) >= 0;
	if ( !m_bJPEGCreated )
	{
		Close();
		return TFALSE;
	}

	m_pFile = &a_rFile;
	m_uiBytesLeft = uiFileSize - sizeof( oHeader );
	m_uiFrameCount = oHeader.uiFrames;
	m_uiRateNumerator = oHeader.uiRateNumerator;
	m_uiRateDenominator = oHeader.uiRateDenominator;
	m_uiMaxJPEG = oHeader.uiMaxJPEG;
	m_uiAudioTracks = oHeader.uiTracks;
	m_pStage = "ready";
	return TTRUE;
}

void TMovieStream_PSP::Close()
{
	if ( m_bJPEGCreated )
		sceJpegDeleteMJpeg();
	if ( m_bJPEGInitialised )
		sceJpegFinishMJpeg();
	if ( m_bModuleLoaded )
		sceUtilityUnloadAvModule( PSP_AV_MODULE_AVCODEC );
	TFree( m_pJPEG );
	TFree( m_pPixels );
	m_pFile = TNULL;
	m_pJPEG = TNULL;
	m_pPixels = TNULL;
	m_uiBytesLeft = 0;
	m_uiFrame = 0;
	m_uiFrameCount = 0;
	m_uiRateNumerator = 0;
	m_uiRateDenominator = 0;
	m_uiMaxJPEG = 0;
	m_uiAudioTracks = 0;
	m_uiAudioPhase = 0;
	m_bJPEGCreated = TFALSE;
	m_bJPEGInitialised = TFALSE;
	m_bModuleLoaded = TFALSE;
}

TBOOL TMovieStream_PSP::DecodeNextFrame()
{
	if ( m_pFile == TNULL || m_uiFrame >= m_uiFrameCount )
		return TFALSE;
	m_pStage = "frame-record";
	TUINT32 uiJPEGSize = 0;
	if ( m_uiBytesLeft < sizeof( uiJPEGSize ) || m_pFile->Read( &uiJPEGSize, sizeof( uiJPEGSize ) ) != sizeof( uiJPEGSize ) )
		return TFALSE;
	m_uiBytesLeft -= sizeof( uiJPEGSize );
	TSIZE uiAudioBytes = 0;
	if ( m_uiAudioTracks != 0 )
	{
		m_uiAudioPhase += TUINT64( MOVIE_AUDIO_RATE_PSP ) * m_uiRateDenominator;
		uiAudioBytes = ( m_uiAudioPhase / m_uiRateNumerator ) * MOVIE_PCM_SAMPLE_BYTES_PSP * m_uiAudioTracks;
		m_uiAudioPhase %= m_uiRateNumerator;
	}
	if ( uiJPEGSize == 0 || uiJPEGSize > m_uiMaxJPEG || uiJPEGSize + uiAudioBytes > m_uiBytesLeft ||
	     m_pFile->Read( m_pJPEG, uiJPEGSize ) != uiJPEGSize ||
	     ( uiAudioBytes != 0 && !m_pFile->Seek( TINT( uiAudioBytes ), TSEEK_CUR ) ) )
		return TFALSE;
	m_uiBytesLeft -= uiJPEGSize + uiAudioBytes;
	if ( m_uiFrame + 1 == m_uiFrameCount && m_uiBytesLeft != 0 )
		return TFALSE;

	m_pStage = "jpeg-profile";
	if ( !IsMovieJPEG_PSP( m_pJPEG, uiJPEGSize ) )
		return TFALSE;
	m_pStage = "jpeg-decode";
	sceKernelDcacheWritebackRange( m_pJPEG, uiJPEGSize );
	if ( sceJpegDecodeMJpeg( m_pJPEG, uiJPEGSize, TREINTERPRETCAST( TUINT8*, m_pPixels ), 0 ) != TINT( ( WIDTH << 16 ) | HEIGHT ) )
		return TFALSE;
	m_uiFrame++;
	m_pStage = "ready";
	return TTRUE;
}

TOSHI_NAMESPACE_END
