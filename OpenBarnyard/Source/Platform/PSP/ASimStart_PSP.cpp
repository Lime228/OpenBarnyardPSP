#include "pch.h"
#include "ASimStart_PSP.h"
#include "AGameRuntimeReport_PSP.h"
#include "ATerrainCollision_PSP.h"

#include "UnitMovement/PathfindingNetworks/AInteriorNetwork.h"

#include <Math/TMathInline.h>
#include <Toshi/TStringManager.h>

TOSHI_NAMESPACE_USING

static constexpr TCHAR s_szSimNetwork_PSP[] = "local:Data/Networks/park2.trb";
static constexpr TCHAR s_szSimStartGoal_PSP[] = "park2_goals_playerstart";

static_assert( sizeof( AInteriorNetwork::INetwork ) == 48 );
static_assert( sizeof( AInteriorGoal ) == 64 );
static_assert( sizeof( AInteriorArea ) == 64 );

static const AInteriorGoal* AFindSimGoal_PSP( const TTRB& a_rTRB, const AInteriorNetwork::INetwork& a_rNetwork, const TCHAR* a_szGoal )
{
	if ( a_rNetwork.m_iNumGoals <= 0 || a_rNetwork.m_iNumGoals > 1024 ||
	     !ATerrainCollision_PSP::ContainsTRBData( a_rTRB, a_rNetwork.m_pGoals, a_rNetwork.m_iNumGoals * sizeof( AInteriorGoal ) ) )
		return TNULL;

	const TSIZE uiNameBytes = TStringManager::String8Length( a_szGoal ) + 1;
	for ( TINT i = 0; i < a_rNetwork.m_iNumGoals; i++ )
	{
		const AInteriorGoal& rGoal = a_rNetwork.m_pGoals[ i ];
		const TCHAR* szName = rGoal.m_szName_Internal;
		if ( ATerrainCollision_PSP::ContainsTRBData( a_rTRB, szName, uiNameBytes ) &&
		     TStringManager::String8Compare( szName, a_szGoal, uiNameBytes ) == 0 )
			return &rGoal;
	}
	return TNULL;
}

static TBOOL AReadSimGoal_PSP( ASimStart_PSP& a_rStart, const TCHAR* a_szGoal,
                              TFLOAT& a_rfAreaHeight, AInteriorArea::FLAGS& a_reAreaFlags )
{
	TTRB oNetwork;
	if ( oNetwork.Load( s_szSimNetwork_PSP ) != TTRB::ERROR_OK || oNetwork.GetSymbolTable() == TNULL )
		return TFALSE;

	const auto* pNetwork = oNetwork.CastSymbol<AInteriorNetwork::INetwork>( "INetwork" );
	if ( !ATerrainCollision_PSP::ContainsTRBData( oNetwork, pNetwork, sizeof( *pNetwork ) ) ||
	     pNetwork->m_iNumAreas <= 0 || pNetwork->m_iNumAreas > 2048 ||
	     !ATerrainCollision_PSP::ContainsTRBData( oNetwork, pNetwork->m_pAreas, pNetwork->m_iNumAreas * sizeof( AInteriorArea ) ) )
		return TFALSE;

	const AInteriorGoal* pGoal = AFindSimGoal_PSP( oNetwork, *pNetwork, a_szGoal );
	if ( pGoal == TNULL || pGoal->m_iAreaIndex < 0 || pGoal->m_iAreaIndex >= pNetwork->m_iNumAreas )
		return TFALSE;

	const AInteriorArea& rArea = pNetwork->m_pAreas[ pGoal->m_iAreaIndex ];
	a_rStart.vPosition.Set( pGoal->m_fPositionX, pGoal->m_fPositionY, pGoal->m_fPositionZ, 1.0f );
	a_rStart.vFacing = pGoal->m_vForward;
	a_rStart.iArea = pGoal->m_iAreaIndex;
	a_rfAreaHeight = rArea.m_fBaseHeight;
	a_reAreaFlags = rArea.m_eFlags;
	return TMath::IsFinite( a_rfAreaHeight ) && TMath::IsFinite( a_rStart.vPosition.x ) &&
	       TMath::IsFinite( a_rStart.vPosition.y ) && TMath::IsFinite( a_rStart.vPosition.z ) &&
	       TMath::IsFinite( a_rStart.vFacing.MagnitudeSq() );
}

TBOOL AReadSimGoal_PSP( ASimStart_PSP& a_rStart, const TCHAR* a_szGoal )
{
	TFLOAT fAreaHeight;
	AInteriorArea::FLAGS eAreaFlags;
	return AReadSimGoal_PSP( a_rStart, a_szGoal, fAreaHeight, eAreaFlags );
}

TBOOL AReadSimStart_PSP( ASimStart_PSP& a_rStart, ATerrainCollision_PSP& a_rTerrain )
{
	TFLOAT fAreaHeight;
	AInteriorArea::FLAGS eAreaFlags;
	if ( !AReadSimGoal_PSP( a_rStart, s_szSimStartGoal_PSP, fAreaHeight, eAreaFlags ) ||
	     !HASANYFLAG( eAreaFlags, AInteriorArea::FLAGS_HEIGHT_RAYCAST_SHORT ) )
	{
		ASetGameRuntimeFailedPath_PSP( s_szSimNetwork_PSP );
		return TFALSE;
	}

	if ( !a_rTerrain.LoadInitial() )
	{
		ASetGameRuntimeFailedPath_PSP( a_rTerrain.GetFailedPath() );
		return TFALSE;
	}

	// Match the short terrain ray and the bottom-ray fallback in AInteriorNetwork::ResolvePointHeight.
	const TFLOAT fRayY = -fAreaHeight - 0.25f;
	const TVector4& rPosition = a_rStart.vPosition;
	TFLOAT fDistance = 40.0f;
	if ( a_rTerrain.CollideVertical( rPosition.x, rPosition.z, fRayY, TTRUE, fDistance ) && fDistance < 39.0f )
		a_rStart.fGroundY = fRayY + fDistance;
	else
	{
		fDistance = 80.0f;
		if ( !a_rTerrain.CollideVertical( rPosition.x, rPosition.z, 40.0f, TFALSE, fDistance ) || fDistance >= 79.0f )
		{
			ASetGameRuntimeFailedPath_PSP( "envmain-start-ground" );
			return TFALSE;
		}
		a_rStart.fGroundY = 40.0f - fDistance;
	}
	if ( a_rStart.vPosition.y == TMath::MAXFLOAT )
		a_rStart.vPosition.y = a_rStart.fGroundY;
	if ( a_rStart.vFacing.MagnitudeSq() < 0.5f )
		a_rStart.vFacing = TVector4::VEC_NEGZ;
	return TTRUE;
}
