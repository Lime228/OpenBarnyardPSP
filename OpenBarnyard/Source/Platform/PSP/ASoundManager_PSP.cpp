#include "pch.h"

#include "AGameRuntimeReport_PSP.h"
#include "AVOStream_PSP.h"
#include "ARandom.h"
#include "Memory/AMemory.h"
#include "Sound/ASoundManager.h"
#include "SoundBank/music.h"
#include "SoundBank/ui.h"

#include <Source/Sound/ASoundUpdateTask_PSP.h>
#include <Source/Sound/AWaveBankLoader_PSP.h>
#include <Math/TMathInline.h>
#include <Toshi/TStringManager.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TPSTRING8_DECLARE( Ingame );
TPSTRING8_DECLARE( Music );
TPSTRING8_DECLARE( VO );

static constexpr TINT s_iSoundPoolCapacity_PSP = 1;
static TFLOAT         s_fGlobalVolume_PSP      = 1.0f;

enum AMenuWaveId_PSP : ASoundId
{
	AMenuWaveId_PSP_Harp02 = 10000,
	AMenuWaveId_PSP_Harp03,
};

static constexpr ASoundCueMapping_PSP s_aMenuCueMappings_PSP[] = {
	{ soundbank::UI_NEGATIVE, "negative" },
	{ soundbank::UI_MENUCLICK, "click" },
	{ soundbank::UI_MENUBACK, "back" },
	{ soundbank::UI_VO_CLICK, "click" },
	{ soundbank::UI_FE_SELECT, "harp_strum" },
	{ AMenuWaveId_PSP_Harp02, "harp02" },
	{ AMenuWaveId_PSP_Harp03, "harp03" },
	{ soundbank::MUSIC_START_SCREEN, "start_screen", TTRUE, TFALSE },
};

static AWave_PSP                   s_aMenuWaves_PSP[ 6 ];
static AStreamWave_PSP             s_StartScreenMusic_PSP;
static AWaveBank_PSP               s_MenuBank_PSP;
static AWaveBankLoader_PSP         s_MenuBankLoader_PSP;
static ASoundMixer_PSP              s_MenuMixer_PSP;
static ASoundCommandQueue_PSP       s_MenuCommands_PSP;
static ASoundDelayedEventQueue_PSP  s_MenuEvents_PSP;
static ASoundManager_PSP            s_MenuSoundManager_PSP;
static TINT                        s_iMenuMusicCue_PSP = -1;
static AStreamWave_PSP             s_SSeqVoiceStream_PSP;
static ASoundVoiceHandle_PSP        s_hSSeqVoice_PSP = ASoundMixer_PSP::INVALID_VOICE;

void AStopSSeqVO_PSP()
{
	s_MenuMixer_PSP.Stop( s_hSSeqVoice_PSP );
	s_hSSeqVoice_PSP = ASoundMixer_PSP::INVALID_VOICE;
	s_SSeqVoiceStream_PSP.Destroy();
}

TBOOL APlaySSeqVO_PSP( TINT a_iSubstream )
{
	AStopSSeqVO_PSP();
	if ( a_iSubstream < 0 || !s_MenuSoundManager_PSP.IsCreated() )
		return TFALSE;
	TCHAR szPath[ 72 ];
	TStringManager::String8Format( szPath, TARRAYSIZE( szPath ), "local:Data/ScriptedSequence/SS01/Voice/%d.padp", a_iSubstream );
	if ( !s_SSeqVoiceStream_PSP.Create( szPath ) )
		return TFALSE;
	s_hSSeqVoice_PSP = s_MenuMixer_PSP.PlayStream( &s_SSeqVoiceStream_PSP );
	if ( s_hSSeqVoice_PSP != ASoundMixer_PSP::INVALID_VOICE )
		return TTRUE;
	AStopSSeqVO_PSP();
	return TFALSE;
}

TINT AGetSSeqVOTime_PSP()
{
	TINT iTime;
	return s_MenuMixer_PSP.GetStreamTime( s_hSSeqVoice_PSP, iTime ) ? iTime : -1;
}

TUINT AGetSSeqVOUnderruns_PSP()
{
	return s_SSeqVoiceStream_PSP.GetUnderrunCount();
}

struct AMenuCueLayer_PSP
{
	TINT iHandle = -1;
	TINT iPrimaryHandle = -1;
	TSIZE uiCategory = 0;
	TFLOAT fVolume = 0.0f;
};

static AMenuCueLayer_PSP s_aMenuCueLayers_PSP[ ASoundManager_PSP::MAX_MANAGED_CUES ];

static void ATrackMenuCueLayer_PSP( TINT a_iHandle, TINT a_iPrimary, TSIZE a_uiCategory, TFLOAT a_fVolume )
{
	if ( a_iHandle < 0 )
		return;

	for ( AMenuCueLayer_PSP& rLayer : s_aMenuCueLayers_PSP )
	{
		if ( !s_MenuSoundManager_PSP.IsCueUsed( rLayer.iHandle ) )
		{
			rLayer = { a_iHandle, a_iPrimary, a_uiCategory, a_fVolume };
			return;
		}
	}
}

static void AUnloadMenuBank_PSP()
{
	AStopSSeqVO_PSP();
	s_MenuSoundManager_PSP.Destroy();
	s_MenuMixer_PSP.StopAll();
	s_MenuEvents_PSP.Clear();
	s_MenuCommands_PSP.Clear();
	s_MenuBankLoader_PSP.Unload( s_MenuBank_PSP, s_aMenuWaves_PSP, TARRAYSIZE( s_aMenuWaves_PSP ), TNULL, 0 );
	s_StartScreenMusic_PSP.Destroy();
	s_iMenuMusicCue_PSP = -1;
	for ( AMenuCueLayer_PSP& rLayer : s_aMenuCueLayers_PSP )
		rLayer = {};
	AGetGameRuntimeReport_PSP().bMenuAudioReady = TFALSE;
	AGetGameRuntimeReport_PSP().bMenuMusicReady = TFALSE;
	AGetGameRuntimeReport_PSP().iMenuMusicPositionMs = -1;
}

TDEFINE_CLASS( ASoundManager );

ASoundManager::Cue::Cue()
{
	bUsed           = TFALSE;
	fStartTime      = -1.0f;
	pSound          = TNULL;
	fStartTime2     = -1.0f;
	vecPosition     = TVector4::VEC_ZERO;
	fVolume         = 1.0f;
	fFrequency      = 1.0f;
	iNumChannelRefs = 0;
	oEventList      = TNULL;
}

ASoundManager::Cue::~Cue() = default;

ASoundManager::ASoundManager()
    : m_SoundEventPool( AMemory::GetAllocator( AMemory::POOL_Sound ), s_iSoundPoolCapacity_PSP )
    , m_StreamRefPool( AMemory::GetAllocator( AMemory::POOL_Sound ), s_iSoundPoolCapacity_PSP )
    , m_ChannelRefPool( AMemory::GetAllocator( AMemory::POOL_Sound ), s_iSoundPoolCapacity_PSP )
    , m_ChannelRefLegacyPool( AMemory::GetAllocator( AMemory::POOL_Sound ), s_iSoundPoolCapacity_PSP )
    , m_CategoryIndices( AMemory::GetAllocator( AMemory::POOL_Sound ) )
    , m_SoundIdToSound( AMemory::GetAllocator( AMemory::POOL_Sound ) )
    , m_SoundIdToSoundLegacy( AMemory::GetAllocator( AMemory::POOL_Sound ) )
{
	m_iLastAvailableSoundExSlot = -1;
	m_fCurrentTime              = 0.0f;
	m_bMuted                    = TFALSE;
	m_bUseMinHardwareChannels   = TFALSE;
	m_iMinHWChannels            = 0;
	m_iNumChannels              = 0;
	m_iGlobalFrequency          = 44100;
	m_pS4                       = TNULL;
}

ASoundManager::~ASoundManager() = default;

TBOOL ASoundManager::Reset()
{
	m_fCurrentTime = 0.0f;
	return TTRUE;
}

TBOOL ASoundManager::OnCreate()
{
	TUINT uiLoadedWaves;
	TUINT uiLoadedStreams;
	const TBOOL bBankLoaded = s_MenuBankLoader_PSP.Load(
		s_MenuBank_PSP, s_aMenuWaves_PSP, TARRAYSIZE( s_aMenuWaves_PSP ), TNULL, 0,
		"local:Data/Sound/Menu/ui.pbnk", uiLoadedWaves, uiLoadedStreams
	);
	const TBOOL bMusicReady = bBankLoaded &&
	    s_StartScreenMusic_PSP.Create( "local:Data/Sound/Menu/start_screen.padp" ) &&
	    s_StartScreenMusic_PSP.IsLoopAligned() &&
	    s_MenuBank_PSP.RegisterStream( "start_screen", &s_StartScreenMusic_PSP );
	if ( !bMusicReady )
		s_StartScreenMusic_PSP.Destroy();

	const TBOOL bReady = bBankLoaded && s_MenuSoundManager_PSP.Create() &&
		s_MenuSoundManager_PSP.BindCueBank(
			&s_MenuBank_PSP, &s_MenuMixer_PSP, &s_MenuCommands_PSP, &s_MenuEvents_PSP,
			s_aMenuCueMappings_PSP, TARRAYSIZE( s_aMenuCueMappings_PSP )
		);

	if ( !bReady )
		AUnloadMenuBank_PSP();
	else
		s_MenuSoundManager_PSP.SetGlobalVolume( s_fGlobalVolume_PSP );
	AGetGameRuntimeReport_PSP().bMenuAudioReady = bReady;
	AGetGameRuntimeReport_PSP().bMenuMusicReady = bReady && bMusicReady;

	return TTRUE;
}

TBOOL ASoundManager::OnUpdate( TFLOAT a_fDeltaTime )
{
	m_fCurrentTime += a_fDeltaTime;
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	const TBOOL bSSeqVoice = s_MenuMixer_PSP.IsVoiceActive( s_hSSeqVoice_PSP );
	if ( bSSeqVoice )
	{
		const TFLOAT fVolume = TMath::Min( 1.0f, TMath::Max( 0.0f, GetCategoryVolume( TPS8( VO ) ) ) );
		s_MenuMixer_PSP.SetVolume( s_hSSeqVoice_PSP, TUINT( fVolume * ASoundMixer_PSP::MAX_VOLUME + 0.5f ) );
	}
	// Four blocks cover a frame even when the front end runs at 15 Hz.
	for ( TUINT i = 0; i < ASoundManager_PSP::OUTPUT_BUFFER_COUNT &&
	     ( bSSeqVoice || s_MenuSoundManager_PSP.GetManagedCueCount() > 0 ); i++ )
	{
		TBOOL bNonSilent;
		if ( !ASoundUpdateTask_PSP::UpdateSound(
			     s_MenuSoundManager_PSP, s_MenuMixer_PSP, s_MenuCommands_PSP, s_MenuEvents_PSP, &bNonSilent
		     ) )
			break;
		if ( bNonSilent )
		{
			rReport.uiMenuAudioNonSilentBlocks++;
			if ( bSSeqVoice )
				rReport.uiNewGameVoiceNonSilentBlocks++;
		}
	}
	const TINT iMusicPosition = s_MenuSoundManager_PSP.GetStreamCueTime( s_iMenuMusicCue_PSP );
	if ( iMusicPosition >= 0 && rReport.iMenuMusicPositionMs > iMusicPosition )
		rReport.uiMenuMusicLoops++;
	rReport.iMenuMusicPositionMs = iMusicPosition;
	rReport.uiMenuMusicUnderruns = s_StartScreenMusic_PSP.GetUnderrunCount();
	rReport.uiMenuMusicGain = TUINT( s_MenuSoundManager_PSP.GetCueVolume( s_iMenuMusicCue_PSP ) * 1000.0f + 0.5f );
	return TTRUE;
}

void ASoundManager::OnDestroy()
{
	AUnloadMenuBank_PSP();
	Reset();
}

TINT ASoundManager::PlayCue( ASoundId a_iSound )
{
	return PlayCueEx( a_iSound, 1.0f, TFALSE, 0.0f, -1 );
}

TINT ASoundManager::PlayCueEx( ASoundId a_iSound, TFLOAT a_fVolume, TBOOL a_bPaused, TFLOAT a_fDelay, TINT a_iTrack )
{
	if ( !s_MenuSoundManager_PSP.IsCueBankBound() || !ARandom::IsSingletonCreated() )
		return -1;

	const TBOOL bMenuMusic = a_iSound == soundbank::MUSIC_START_SCREEN;
	const TPString8& rCategory = bMenuMusic ? TPS8( Music ) : a_iSound == soundbank::UI_VO_CLICK ? TPS8( VO ) : TPS8( Ingame );
	const auto pCategory = m_CategoryIndices.Find( rCategory );
	if ( !m_CategoryIndices.IsValid( pCategory ) )
		return -1;

	TMath::Clip( a_fVolume, 0.0f, 1.0f );
	const TSIZE uiCategory = pCategory->GetSecond();
	const TFLOAT fCategoryVolume = m_aCategories[ uiCategory ].fVolumeMultiplier;
	TRandom& rRandom = ARandom::GetSingleton()->m_oRandom;
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	if ( bMenuMusic )
	{
		if ( ( a_iTrack != -1 && a_iTrack != 0 ) || !rReport.bMenuMusicReady )
			return -1;
		const TINT iMusicCue = s_MenuSoundManager_PSP.PlayCueEx( a_iSound, a_fVolume * fCategoryVolume, a_bPaused, a_fDelay, -1 );
		ATrackMenuCueLayer_PSP( iMusicCue, iMusicCue, uiCategory, a_fVolume );
		if ( iMusicCue >= 0 )
		{
			s_iMenuMusicCue_PSP = iMusicCue;
			rReport.iMenuMusicPositionMs = -1;
			rReport.uiMenuMusicLoops = 0;
		}
		return iMusicCue;
	}
	TINT iCue = -1;

	// Sample volumes, pitch ranges and track timing from SoundBank_ui.trb.
	switch ( a_iSound )
	{
		case soundbank::UI_MENUCLICK:
		case soundbank::UI_MENUBACK:
		case soundbank::UI_NEGATIVE:
		{
			if ( a_iTrack != -1 && a_iTrack != 0 )
				break;
			const TFLOAT fVolume = a_fVolume * ( a_iSound == soundbank::UI_MENUCLICK ? 0.251188993f : 0.501187027f );
			iCue = s_MenuSoundManager_PSP.PlayCueEx( a_iSound, fCategoryVolume * fVolume, a_bPaused, a_fDelay, -1 );
			ATrackMenuCueLayer_PSP( iCue, iCue, uiCategory, fVolume );
			break;
		}
		case soundbank::UI_MENUOK:
		{
			if ( a_iTrack != -1 && a_iTrack != 0 && a_iTrack != 1 )
				break;
			// Each of the two original tracks independently picks one of the harps.
			for ( TINT i = 0; i < 2; i++ )
			{
				if ( a_iTrack != -1 && a_iTrack != i )
					continue;
				const ASoundId eWave = rRandom.GetInt( 2 ) == 0 ? AMenuWaveId_PSP_Harp02 : AMenuWaveId_PSP_Harp03;
				const TFLOAT fVariation = i == 0 ? 1.0f : rRandom.GetFloatMinMax( 0.707946002f, 1.41253805f );
				const TFLOAT fDelay = a_fDelay + ( i == 0 ? 0.0f : rRandom.GetFloatMinMax( 0.0f, 0.1f ) );
				const TFLOAT fVolume = a_fVolume * 0.177827999f * fVariation;
				const TINT iLayer = s_MenuSoundManager_PSP.PlayCueEx( eWave, fCategoryVolume * fVolume, a_bPaused, fDelay, -1 );
				if ( iCue < 0 )
					iCue = iLayer;
				ATrackMenuCueLayer_PSP( iLayer, iCue, uiCategory, fVolume );
			}
			break;
		}
		case soundbank::UI_FE_MOVE:
		case soundbank::UI_FE_SELECT:
		{
			if ( a_iTrack != -1 && a_iTrack != 0 )
				break;
			ASoundId eWave = a_iSound;
			if ( a_iSound == soundbank::UI_FE_MOVE )
				eWave = rRandom.GetInt( 2 ) == 0 ? AMenuWaveId_PSP_Harp02 : AMenuWaveId_PSP_Harp03;
			const TFLOAT fVolume = a_fVolume * 0.501187027f * rRandom.GetFloatMinMax( 0.501187027f, 0.707946002f );
			iCue = s_MenuSoundManager_PSP.PlayCueEx( eWave, fCategoryVolume * fVolume, a_bPaused, a_fDelay, -1 );
			ATrackMenuCueLayer_PSP( iCue, iCue, uiCategory, fVolume );
			break;
		}
		case soundbank::UI_VO_CLICK:
		{
			if ( a_iTrack != -1 && a_iTrack != 0 )
				break;
			const TFLOAT fVolume = a_fVolume * rRandom.GetFloatMinMax( 0.707946002f, 1.0f );
			iCue = s_MenuSoundManager_PSP.PlayCueEx( a_iSound, fCategoryVolume * fVolume, a_bPaused, a_fDelay, -1 );
			if ( iCue >= 0 )
				s_MenuSoundManager_PSP.SetCueFrequency( iCue, rRandom.GetFloatMinMax( 1.68082201f, 1.68179297f ) );
			ATrackMenuCueLayer_PSP( iCue, iCue, uiCategory, fVolume );
			break;
		}
		default:
			break;
	}

	if ( iCue >= 0 )
	{
		rReport.iMenuAudioLastCue = a_iSound;
		rReport.uiMenuAudioCueCount++;
		if ( a_iSound == soundbank::UI_VO_CLICK )
			rReport.uiMenuAudioVOCueCount++;
	}
	return iCue;
}

void ASoundManager::StopCue( TINT& a_rCueIndex )
{
	StopCueAsync( a_rCueIndex, 0.0f );
}

void ASoundManager::StopCueAsync( TINT& a_rCueIndex, TFLOAT a_fDelay )
{
	if ( a_rCueIndex < 0 )
		return;

	for ( const AMenuCueLayer_PSP& rLayer : s_aMenuCueLayers_PSP )
	{
		if ( rLayer.iPrimaryHandle == a_rCueIndex )
		{
			TINT iHandle = rLayer.iHandle;
			s_MenuSoundManager_PSP.StopCueAsync( iHandle, a_fDelay );
		}
	}
	a_rCueIndex = -1;
}

void ASoundManager::PauseAllCues( TBOOL a_bPause )
{
	s_MenuSoundManager_PSP.PauseAllCues( a_bPause );
	s_MenuMixer_PSP.SetPaused( s_hSSeqVoice_PSP, a_bPause );
}

void ASoundManager::PauseCue( TINT a_iCue, TBOOL a_bPause )
{
	if ( a_iCue >= 0 )
	{
		for ( const AMenuCueLayer_PSP& rLayer : s_aMenuCueLayers_PSP )
			if ( rLayer.iPrimaryHandle == a_iCue )
				s_MenuSoundManager_PSP.PauseCue( rLayer.iHandle, a_bPause );
	}
}

TINT ASoundManager::GetAvailableCueIndex()
{
	return -1;
}

TBOOL ASoundManager::IsCuePlaying( TINT a_iCue )
{
	if ( a_iCue < 0 )
		return TFALSE;

	for ( const AMenuCueLayer_PSP& rLayer : s_aMenuCueLayers_PSP )
		if ( rLayer.iPrimaryHandle == a_iCue && s_MenuSoundManager_PSP.IsCuePlaying( rLayer.iHandle ) )
			return TTRUE;
	return TFALSE;
}

void ASoundManager::SetCueFrequency( TINT, TFLOAT, TFLOAT )
{
}

void ASoundManager::SetCueVolume( TINT, TFLOAT, TFLOAT )
{
}

void ASoundManager::FadeCue( TINT, TFLOAT, TFLOAT, TFLOAT )
{
}

void ASoundManager::FadeOutCue( TINT, TFLOAT, TBOOL, TFLOAT )
{
}

void ASoundManager::SetGlobalVolume( TFLOAT a_fVolume )
{
	if ( a_fVolume < 0.0f ) a_fVolume = 0.0f;
	if ( a_fVolume > 1.0f ) a_fVolume = 1.0f;
	s_fGlobalVolume_PSP = a_fVolume;
	s_MenuSoundManager_PSP.SetGlobalVolume( a_fVolume );
}

void ASoundManager::SetCategoryVolume( const TPString8& a_rcName, TFLOAT a_fVolume, TFLOAT a_fDelay )
{
	auto pFoundCategory = m_CategoryIndices.Find( a_rcName );
	TINT iCategory;

	if ( m_CategoryIndices.IsValid( pFoundCategory ) )
	{
		iCategory = TINT( pFoundCategory->GetSecond() );
	}
	else
	{
		if ( m_CategoryIndices.Size() >= MAX_NUM_CATEGORIES )
			return;

		iCategory = TINT( m_CategoryIndices.Size() );
		m_CategoryIndices.Insert( a_rcName, TSIZE( iCategory ) );
	}

	SetCategoryVolume( iCategory, a_fVolume, a_fDelay );
}

void ASoundManager::SetCategoryVolume( TINT a_iCategory, TFLOAT a_fVolume, TFLOAT a_fDelay )
{
	if ( a_iCategory < 0 || a_iCategory >= TINT( MAX_NUM_CATEGORIES ) )
		return;

	if ( a_fVolume < 0.0f ) a_fVolume = 0.0f;
	if ( a_fVolume > 1.0f ) a_fVolume = 1.0f;

	Category& rCategory             = m_aCategories[ a_iCategory ];
	rCategory.fPrevVolumeMultiplier = rCategory.fVolumeMultiplier;
	rCategory.fVolumeMultiplier     = a_fVolume;

	for ( const AMenuCueLayer_PSP& rLayer : s_aMenuCueLayers_PSP )
		if ( rLayer.uiCategory == TSIZE( a_iCategory ) && s_MenuSoundManager_PSP.IsCueUsed( rLayer.iHandle ) )
			s_MenuSoundManager_PSP.SetCueVolume( rLayer.iHandle, rLayer.fVolume * a_fVolume, a_fDelay );
}

TFLOAT ASoundManager::GetCategoryVolume( const TPString8& a_rcName )
{
	auto pFoundCategory = m_CategoryIndices.Find( a_rcName );
	if ( !m_CategoryIndices.IsValid( pFoundCategory ) )
		return 0.0f;

	return m_aCategories[ pFoundCategory->GetSecond() ].fVolumeMultiplier * s_fGlobalVolume_PSP;
}

void ASoundManager::GetMemoryStats( MemoryStats* a_pStats )
{
	if ( a_pStats == TNULL )
		return;

	a_pStats->uiCurrentAlloced = 0;
	a_pStats->uiMaxAlloced     = 0;
	a_pStats->uiUnknown        = 0;
}
