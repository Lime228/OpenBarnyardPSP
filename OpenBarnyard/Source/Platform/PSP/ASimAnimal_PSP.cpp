#include "pch.h"
#include "ASimAnimal_PSP.h"
#include "ASimStart_PSP.h"
#include "ATerrainCollision_PSP.h"

#include "Units/AAnimalPopulationManager.h"
#include "Terrain/ATerrainLocator.h"

#include <File/TFile.h>
#include <Platform/PSP/TAnimationClipFactory_PSP.h>
#include <Toshi/TStringManager.h>

TOSHI_NAMESPACE_USING

using AnimalPopulation = AAnimalPopulationManager;

static constexpr TCHAR s_szPopulationPath_PSP[] = "local:Data/Units/AnimalSpawn.trb";
static constexpr TCHAR s_szLocatorPath_PSP[] = "local:Data/Terrain/EnvMain/envmain_skel.trb";
static constexpr TCHAR s_szMilesRoot_PSP[] = "local:Data/Units/miles";
static constexpr TFLOAT s_fAnimalUpdateDistance_PSP = 24.0f;

static const TCHAR* AGetSpawnString_PSP( const TTRB& a_rTRB, const AnimalPopulation::StringTable& a_rTable, TINT a_iIndex )
{
	if ( a_rTable.iNumStrings <= 0 || a_rTable.iNumStrings > 256 || a_iIndex < 0 || a_iIndex >= a_rTable.iNumStrings ||
	     !ATerrainCollision_PSP::ContainsTRBData( a_rTRB, a_rTable.ppStrings, a_rTable.iNumStrings * sizeof( const TCHAR* ) ) )
		return TNULL;

	const TCHAR* pString = a_rTable.ppStrings[ a_iIndex ];
	for ( TSIZE i = 0; i < 64 && ATerrainCollision_PSP::ContainsTRBData( a_rTRB, pString, i + 1 ); i++ )
	{
		if ( pString[ i ] == '\0' )
			return pString;
	}
	return TNULL;
}

static TBOOL AReadSimQuestLocator_PSP( ASimStart_PSP& a_rStart, const TCHAR* a_szName )
{
	TTRB oTRB;
	if ( oTRB.Load( s_szLocatorPath_PSP ) != TTRB::ERROR_OK || oTRB.GetSymbolTable() == TNULL )
		return TFALSE;

	const auto* pHeader = oTRB.CastSymbol<ATerrainLocatorTRBHeader>( "Locators" );
	if ( !ATerrainCollision_PSP::ContainsTRBData( oTRB, pHeader, sizeof( *pHeader ) ) ||
	     pHeader->iNumLocators <= 0 || pHeader->iNumLocators > 16384 || pHeader->uiNameCount > 16384 ||
	     !ATerrainCollision_PSP::ContainsTRBData( oTRB, pHeader->pLocators, pHeader->iNumLocators * sizeof( ATerrainLocator ) ) ||
	     !ATerrainCollision_PSP::ContainsTRBData( oTRB, pHeader->ppNames, pHeader->uiNameCount * sizeof( const TCHAR* ) ) )
		return TFALSE;

	const TSIZE uiNameBytes = TStringManager::String8Length( a_szName ) + 1;
	for ( TINT i = 0; i < pHeader->iNumLocators; i++ )
	{
		const ATerrainLocator& rLocator = pHeader->pLocators[ i ];
		if ( rLocator.uiNameId >= pHeader->uiNameCount )
			return TFALSE;
		const TCHAR* szName = pHeader->ppNames[ rLocator.uiNameId ];
		if ( !ATerrainCollision_PSP::ContainsTRBData( oTRB, szName, uiNameBytes ) ||
		     TStringManager::String8CompareNoCase( szName, a_szName, uiNameBytes ) != 0 )
			continue;

		// ATerrainLocatorList maps the authored Z-up transform into the game world.
		TMatrix44 oWorld = {
			1.0f,  0.0f, 0.0f, 0.0f,
			0.0f,  0.0f, 1.0f, 0.0f,
			0.0f, -1.0f, 0.0f, 0.0f,
			0.0f,  0.0f, 0.0f, 1.0f,
		};
		TMatrix44 oLocal;
		oLocal.SetFromQuaternion( rLocator.quatRotation );
		oLocal.AsBasisVector3( BASISVECTOR_TRANSLATION ) = rLocator.vecPosition;
		oWorld.Multiply( oLocal );
		a_rStart.vPosition = oWorld.GetTranslation();
		a_rStart.vFacing.Set( -oWorld.m_f21, 0.0f, -oWorld.m_f23, 1.0f );
		return TMath::IsFinite( a_rStart.vPosition.MagnitudeSq() ) &&
		       TMath::IsFinite( a_rStart.vFacing.MagnitudeSqXZ() ) && a_rStart.vFacing.MagnitudeSqXZ() > 0.5f;
	}
	return TFALSE;
}

static TBOOL AReadMilesStart_PSP( TTRB& a_rTRB, ASimStart_PSP& a_rStart )
{
	const auto* pSpawns = a_rTRB.CastSymbol<AnimalPopulation::Set<AnimalPopulation::QuestTierSpawn>>( "QuestTierSpawnSet" );
	const auto* pTables = a_rTRB.CastSymbol<AnimalPopulation::StringTable>( "StringMappingTables" );
	if ( !ATerrainCollision_PSP::ContainsTRBData( a_rTRB, pSpawns, sizeof( *pSpawns ) ) ||
	     !ATerrainCollision_PSP::ContainsTRBData( a_rTRB, pTables, AnimalPopulation::STRINGTABLE_NUMOF * sizeof( *pTables ) ) ||
	     pSpawns->iNumEntries <= 1 || pSpawns->iNumEntries > 512 ||
	     !ATerrainCollision_PSP::ContainsTRBData( a_rTRB, pSpawns->pEntries, pSpawns->iNumEntries * sizeof( *pSpawns->pEntries ) ) )
		return TFALSE;

	for ( TINT i = 1; i < pSpawns->iNumEntries; i++ )
	{
		const AnimalPopulation::QuestTierSpawn& rEntry = pSpawns->pEntries[ i ];
		if ( rEntry.uiTier != 1 )
			continue;

		const TCHAR* szQuest = AGetSpawnString_PSP( a_rTRB, pTables[ AnimalPopulation::STRINGTABLE_QUEST ], rEntry.uiQuest );
		if ( szQuest == TNULL || TStringManager::String8Compare( szQuest, "Miles Welcome" ) != 0 )
			continue;

		const AnimalPopulation::SpawnData& rSpawn = rEntry.oSpawn;
		const TCHAR* szUnit = AGetSpawnString_PSP( a_rTRB, pTables[ AnimalPopulation::STRINGTABLE_UNITTYPE ], rSpawn.iType );
		const TCHAR* szName = AGetSpawnString_PSP( a_rTRB, pTables[ AnimalPopulation::STRINGTABLE_UNITNAME ], rSpawn.iName );
		const TCHAR* szNetwork = AGetSpawnString_PSP( a_rTRB, pTables[ AnimalPopulation::STRINGTABLE_NETWORK ], rSpawn.iNetwork );
		const TCHAR* szGoal = AGetSpawnString_PSP( a_rTRB, pTables[ AnimalPopulation::STRINGTABLE_GOAL ], rSpawn.iGoal );
		if ( szUnit == TNULL || TStringManager::String8Compare( szUnit, "miles" ) != 0 ||
		     szName == TNULL || TStringManager::String8Compare( szName, "miles" ) != 0 ||
		     szNetwork == TNULL || TStringManager::String8Compare( szNetwork, "park2" ) != 0 )
			return TFALSE;

		TCHAR szLocator[ 32 ];
		TStringManager::String8Format( szLocator, TARRAYSIZE( szLocator ), "ql_a1t01_%s", szName );
		// Match PlaceUnit: the visible quest locator takes priority over the network goal.
		if ( AReadSimQuestLocator_PSP( a_rStart, szLocator ) )
			return TTRUE;
		return szGoal != TNULL && AReadSimGoal_PSP( a_rStart, szGoal ) &&
		       a_rStart.vPosition.y != TMath::MAXFLOAT && a_rStart.vFacing.MagnitudeSqXZ() > 0.5f;
	}
	return TFALSE;
}

TBOOL ASimAnimal_PSP::Load()
{
	Unload();
	// Older staged runtimes contain only the player and terrain.
	TFile* pFile = TFile::Create( s_szPopulationPath_PSP, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TTRUE;
	pFile->Destroy();

	ASimStart_PSP oStart;
	TStringManager::String8Copy( m_szPath, s_szPopulationPath_PSP, TARRAYSIZE( m_szPath ) );
	{
		TTRB oPopulation;
		if ( oPopulation.Load( s_szPopulationPath_PSP ) != TTRB::ERROR_OK || oPopulation.GetSymbolTable() == TNULL ||
		     !AReadMilesStart_PSP( oPopulation, oStart ) )
			return TFALSE;
	}

	const TSIZE uiPathCapacity = TARRAYSIZE( m_szPath );
	if ( !m_Model.LoadRig( s_szMilesRoot_PSP, m_szPath, uiPathCapacity ) )
	{
		Unload();
		return TFALSE;
	}
	const TAnimationClipFactory_PSP oFactory;
	TStringManager::String8Format( m_szPath, uiPathCapacity, "%s/idle.panm", s_szMilesRoot_PSP );
	if ( !oFactory.CreateAnimationFromFile( m_Idle, m_Model.GetSkeleton(), m_szPath ) || !m_Idle.IsLooping() ||
	     !m_Model.LoadMeshes( s_szMilesRoot_PSP, m_szPath, uiPathCapacity ) || !m_Model.UpdatePose( m_Idle, 0 ) )
	{
		Unload();
		return TFALSE;
	}
	m_vPosition = oStart.vPosition;
	m_Model.SetTransform( m_vPosition, oStart.vFacing );
	m_bLoaded = TTRUE;
	m_szPath[ 0 ] = '\0';
	return TTRUE;
}

void ASimAnimal_PSP::Unload()
{
	m_Idle.Unload();
	m_Model.Unload();
	m_vPosition = TVector4::VEC_ZERO;
	m_fIdleTime = 0.0f;
	m_uiIdleFrame = m_uiPoseUpdates = 0;
	m_bLoaded = m_bNearby = TFALSE;
}

TBOOL ASimAnimal_PSP::Update( TFLOAT a_fDeltaTime, const TVector4& a_rPlayerPosition )
{
	m_bNearby = m_bLoaded && TVector4::DistanceSqXZ( m_vPosition, a_rPlayerPosition ) <
	           s_fAnimalUpdateDistance_PSP * s_fAnimalUpdateDistance_PSP;
	if ( !m_bNearby )
		return TTRUE;

	const TUINT uiRate = m_Idle.GetSampleRate();
	const TFLOAT fDuration = TFLOAT( m_Idle.GetFrameCount() ) / uiRate;
	m_fIdleTime += a_fDeltaTime;
	m_fIdleTime -= TMath::FloorToInt( m_fIdleTime / fDuration ) * fDuration;
	const TUINT uiFrame = TUINT( m_fIdleTime * uiRate );
	if ( uiFrame == m_uiIdleFrame )
		return TTRUE;

	if ( !m_Model.UpdatePose( m_Idle, ( uiFrame * 1000 + uiRate - 1 ) / uiRate ) )
		return TFALSE;
	m_uiIdleFrame = uiFrame;
	m_uiPoseUpdates++;
	return TTRUE;
}

TUINT ASimAnimal_PSP::Render( TRenderInterface_PSP& a_rRenderer, const TRenderCamera_PSP& a_rCamera ) const
{
	return m_bNearby ? m_Model.Render( a_rRenderer, a_rCamera ) : 0;
}
