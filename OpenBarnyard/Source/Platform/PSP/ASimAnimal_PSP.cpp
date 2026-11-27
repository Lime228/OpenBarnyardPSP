#include "pch.h"
#include "ASimAnimal_PSP.h"
#include "ASimStart_PSP.h"
#include "ATerrainCollision_PSP.h"

#include "Units/AAnimalPopulationManager.h"
#include "Terrain/ATerrainLocator.h"

#include <File/TFile.h>
#include <Platform/PSP/TAnimationClipFactory_PSP.h>
#include <Platform/PSP/TModelFactory_PSP.h>
#include <Platform/PSP/TTextureFactory_PSP.h>
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

static TBOOL AReadMilesStart_PSP( TTRB& a_rTRB, ASimStart_PSP& a_rStart, TBOOL& a_rbHasMarker )
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
		a_rbHasMarker = HASANYFLAG( rSpawn.uiMarkerFlags, AnimalPopulation::MARKERTYPE_EXCLAMATION );
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
	TBOOL bHasMarker = TFALSE;
	TStringManager::String8Copy( m_szPath, s_szPopulationPath_PSP, TARRAYSIZE( m_szPath ) );
	{
		TTRB oPopulation;
		if ( oPopulation.Load( s_szPopulationPath_PSP ) != TTRB::ERROR_OK || oPopulation.GetSymbolTable() == TNULL ||
		     !AReadMilesStart_PSP( oPopulation, oStart, bHasMarker ) )
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
	     !m_Model.LoadMeshes( s_szMilesRoot_PSP, m_szPath, uiPathCapacity ) || !m_Model.UpdatePose( m_Idle, 0 ) ||
	     ( bHasMarker && !LoadMarker() ) )
	{
		Unload();
		return TFALSE;
	}
	m_vPosition = oStart.vPosition;
	m_vFacing = oStart.vFacing;
	m_Model.SetTransform( m_vPosition, m_vFacing );
	m_bLoaded = TTRUE;
	m_szPath[ 0 ] = '\0';
	return TTRUE;
}

TBOOL ASimAnimal_PSP::LoadMarker()
{
	const TSIZE uiPathCapacity = TARRAYSIZE( m_szPath );
	TStringManager::String8Format( m_szPath, uiPathCapacity, "%s/exclamation.psmk", s_szMilesRoot_PSP );
	TFile* pFile = TFile::Create( m_szPath, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TTRUE;

	TUINT32 aHeader[ 2 ];
	TFLOAT fHeight;
	const TBOOL bRead = pFile->GetSize() == sizeof( aHeader ) + sizeof( fHeight ) &&
	                    pFile->Read( aHeader, sizeof( aHeader ) ) == sizeof( aHeader ) &&
	                    aHeader[ 0 ] == 0x4b4d5350 && aHeader[ 1 ] == 1 &&
	                    pFile->Read( &fHeight, sizeof( fHeight ) ) == sizeof( fHeight ) && fHeight > 0.0f && fHeight < 8.0f;
	pFile->Destroy();
	if ( !bRead )
		return TFALSE;

	const TModelFactory_PSP oModelFactory;
	TStringManager::String8Format( m_szPath, uiPathCapacity, "%s/exclamation.pmes", s_szMilesRoot_PSP );
	if ( !oModelFactory.CreateModelFromFile( m_MarkerModel, m_szPath ) )
		return TFALSE;

	const TTextureFactory_PSP oTextureFactory;
	TStringManager::String8Format( m_szPath, uiPathCapacity, "%s/exclamation.ptex", s_szMilesRoot_PSP );
	if ( !oTextureFactory.CreateTextureFromFile( m_MarkerTexture, m_szPath ) )
		return TFALSE;

	m_fMarkerHeight = fHeight;
	return TTRUE;
}

void ASimAnimal_PSP::Unload()
{
	m_MarkerModel.Unload();
	m_MarkerTexture.Unload();
	m_Idle.Unload();
	m_Model.Unload();
	m_vPosition = TVector4::VEC_ZERO;
	m_fIdleTime = 0.0f;
	m_fMarkerTime = m_fMarkerHeight = 0.0f;
	m_uiIdleFrame = m_uiPoseUpdates = 0;
	m_bLoaded = m_bNearby = TFALSE;
}

TBOOL ASimAnimal_PSP::Update( TFLOAT a_fDeltaTime, const TVector4& a_rPlayerPosition )
{
	m_bNearby = m_bLoaded && TVector4::DistanceSqXZ( m_vPosition, a_rPlayerPosition ) <
	           s_fAnimalUpdateDistance_PSP * s_fAnimalUpdateDistance_PSP;
	if ( !m_bNearby )
		return TTRUE;

	m_fMarkerTime += a_fDeltaTime;
	m_fMarkerTime -= 2.0f * TMath::FloorToInt( m_fMarkerTime * 0.5f );

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

TBOOL ASimAnimal_PSP::RenderMarker( TRenderInterface_PSP& a_rRenderer, const TRenderCamera_PSP& a_rCamera ) const
{
	if ( !m_bNearby || m_fMarkerHeight == 0.0f )
		return TFALSE;

	// AAnimalPopulationManager::RenderMarkers uses one turn every two seconds and a 0.1 m bob.
	const TFLOAT fBob = TMath::Sin( m_fMarkerTime * TMath::TWO_PI ) * 0.1f;
	const TVector4 vPosition( m_vPosition.x, m_vPosition.y - m_fMarkerHeight - 1.0f + fBob, m_vPosition.z, 1.0f );
	const TVector4 vCamera( a_rCamera.aPosition[ 0 ], a_rCamera.aPosition[ 1 ], a_rCamera.aPosition[ 2 ], 1.0f );
	TFLOAT fDistance = TMath::Sqrt( TVector4::DistanceSq( vPosition, vCamera ) ) - 5.0f;
	TMath::Clip( fDistance, 0.0f, 40.0f );
	const TRenderSceneTransform_PSP oTransform = {
		{ vPosition.x, vPosition.y, vPosition.z },
		{ 0.0f, m_fMarkerTime * TMath::PI, 0.0f },
		0.5f + fDistance / 35.0f
	};
	const TRenderModelTransform_PSP oModelTransform = { 1.0f, { 0.0f, 0.0f, 0.0f } };
	static const TRenderMaterial_PSP s_Material = {
		TTRUE, TRENDERTEXTUREFUNCTION_PSP_MODULATE, TRENDERCULLMODE_PSP_NONE, TRENDERALPHAMODE_PSP_OPAQUE,
		TRENDERTEXTUREADDRESS_PSP_WRAP, TRENDERTEXTUREADDRESS_PSP_WRAP,
	};
	a_rRenderer.DrawModel( m_MarkerModel, oTransform, s_Material, &m_MarkerTexture, oModelTransform, &a_rCamera );
	return TTRUE;
}
