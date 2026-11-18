#include "pch.h"

#include "Memory/AMemory.h"
#include "Sound/ASoundManager.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static constexpr TINT s_iSoundPoolCapacity_PSP = 1;
static TFLOAT         s_fGlobalVolume_PSP      = 1.0f;

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
	return TTRUE;
}

TBOOL ASoundManager::OnUpdate( TFLOAT a_fDeltaTime )
{
	m_fCurrentTime += a_fDeltaTime;
	return TTRUE;
}

void ASoundManager::OnDestroy()
{
	Reset();
}

TINT ASoundManager::PlayCue( ASoundId a_iSound )
{
	return PlayCueEx( a_iSound, 1.0f, TFALSE, 0.0f, -1 );
}

TINT ASoundManager::PlayCueEx( ASoundId, TFLOAT, TBOOL, TFLOAT, TINT )
{
	// The original frontend may request UI cues before the PSP mixer owns the
	// retail sound banks.  Returning an invalid cue is the original API's safe
	// failure path and keeps that limitation explicit.
	return -1;
}

void ASoundManager::StopCue( TINT& a_rCueIndex )
{
	a_rCueIndex = -1;
}

void ASoundManager::StopCueAsync( TINT& a_rCueIndex, TFLOAT )
{
	a_rCueIndex = -1;
}

void ASoundManager::PauseAllCues( TBOOL )
{
}

void ASoundManager::PauseCue( TINT, TBOOL )
{
}

TINT ASoundManager::GetAvailableCueIndex()
{
	return -1;
}

TBOOL ASoundManager::IsCuePlaying( TINT )
{
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

void ASoundManager::SetCategoryVolume( TINT a_iCategory, TFLOAT a_fVolume, TFLOAT )
{
	if ( a_iCategory < 0 || a_iCategory >= TINT( MAX_NUM_CATEGORIES ) )
		return;

	if ( a_fVolume < 0.0f ) a_fVolume = 0.0f;
	if ( a_fVolume > 1.0f ) a_fVolume = 1.0f;

	Category& rCategory             = m_aCategories[ a_iCategory ];
	rCategory.fPrevVolumeMultiplier = rCategory.fVolumeMultiplier;
	rCategory.fVolumeMultiplier     = a_fVolume;
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
