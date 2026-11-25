#include "pch.h"
#include "ATerrainCollision_PSP.h"

#include <File/TFile.h>
#include <Math/TMathInline.h>
#include <Math/TVector3.h>
#include <Toshi/TStringManager.h>

TOSHI_NAMESPACE_USING

static constexpr TCHAR s_szInitialCollision_PSP[] = "local:Data/Terrain/EnvMain/initial.pscl";
static constexpr TSIZE s_uiCollisionNameBytes_PSP = 48;
static constexpr TUINT s_uiMaxTerrainTriangles_PSP = 16 * 1024;

static_assert( sizeof( TTMDBase::CollisionHeader ) == 8 );
static_assert( sizeof( TTMDBase::CollisionMesh ) == 28 );
static_assert( sizeof( TVector3 ) == 12 );

TBOOL ATerrainCollision_PSP::ContainsTRBData( const TTRB& a_rTRB, const void* a_pData, TSIZE a_uiBytes )
{
	const TUINTPTR uiAddress = TUINTPTR( a_pData );
	if ( a_pData == TNULL )
		return TFALSE;
	for ( TINT i = 0; i < a_rTRB.GetNumSections(); i++ )
	{
		const TTRB::SecInfo* pSection = a_rTRB.GetSectionInfo( i );
		const TUINTPTR uiStart = TUINTPTR( pSection->m_Data );
		if ( pSection->m_Data != TNULL && uiAddress >= uiStart && a_uiBytes <= pSection->m_Size &&
		     uiAddress - uiStart <= pSection->m_Size - a_uiBytes )
			return TTRUE;
	}
	return TFALSE;
}

TBOOL ATerrainCollision_PSP::HasInitialPack()
{
	TFile* pFile = TFile::Create( s_szInitialCollision_PSP, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;
	pFile->Destroy();
	return TTRUE;
}

static TBOOL AIsTerrainCollisionName_PSP( const TCHAR* a_szName )
{
	TSIZE uiLength = 0;
	while ( uiLength < s_uiCollisionNameBytes_PSP && a_szName[ uiLength ] != '\0' )
	{
		const TCHAR cChar = a_szName[ uiLength++ ];
		if ( !( ( cChar >= 'a' && cChar <= 'z' ) || ( cChar >= 'A' && cChar <= 'Z' ) ||
		        ( cChar >= '0' && cChar <= '9' ) || cChar == '_' || cChar == '.' ) )
			return TFALSE;
	}
	static constexpr TCHAR s_szSuffix[] = "_col.trb";
	return uiLength >= sizeof( s_szSuffix ) && uiLength < s_uiCollisionNameBytes_PSP &&
	       TStringManager::String8Compare( a_szName + uiLength - ( sizeof( s_szSuffix ) - 1 ), s_szSuffix ) == 0;
}

TBOOL ATerrainCollision_PSP::LoadInitial()
{
	Unload();
	TStringManager::String8Copy( m_szPath, s_szInitialCollision_PSP, TARRAYSIZE( m_szPath ) );
	TFile* pFile = TFile::Create( m_szPath, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;

	TUINT32 aHeader[ 3 ];
	TCHAR aszFiles[ MAX_PACKS ][ s_uiCollisionNameBytes_PSP ];
	TBOOL bRead = pFile->Read( aHeader, sizeof( aHeader ) ) == sizeof( aHeader ) &&
	              aHeader[ 0 ] == 0x4c435350 && aHeader[ 1 ] == 1 && aHeader[ 2 ] > 0 && aHeader[ 2 ] <= MAX_PACKS;
	if ( bRead )
	{
		const TSIZE uiBytes = aHeader[ 2 ] * s_uiCollisionNameBytes_PSP;
		bRead = pFile->GetSize() == sizeof( aHeader ) + uiBytes && pFile->Read( aszFiles, uiBytes ) == uiBytes;
	}
	pFile->Destroy();
	if ( !bRead )
		return TFALSE;

	for ( TUINT i = 0; i < aHeader[ 2 ]; i++ )
	{
		if ( !AIsTerrainCollisionName_PSP( aszFiles[ i ] ) || !LoadPack( aszFiles[ i ] ) )
		{
			Unload();
			return TFALSE;
		}
	}
	m_szPath[ 0 ] = '\0';
	return TTRUE;
}

TBOOL ATerrainCollision_PSP::LoadPack( const TCHAR* a_szName )
{
	TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "local:Data/Terrain/EnvMain/%s", a_szName );
	Pack& rPack = m_aPacks[ m_uiPacks ];
	if ( rPack.oTRB.Load( m_szPath ) != TTRB::ERROR_OK || rPack.oTRB.GetSymbolTable() == TNULL )
		return TFALSE;

	const auto* pCollision = rPack.oTRB.CastSymbol<TTMDBase::CollisionHeader>( "Collision" );
	if ( !ContainsTRBData( rPack.oTRB, pCollision, sizeof( *pCollision ) ) ||
	     pCollision->m_iNumMeshes <= 0 || pCollision->m_iNumMeshes > 32 ||
	     !ContainsTRBData( rPack.oTRB, pCollision->m_pMeshes, pCollision->m_iNumMeshes * sizeof( TTMDBase::CollisionMesh ) ) )
		return TFALSE;

	for ( TINT i = 0; i < pCollision->m_iNumMeshes; i++ )
	{
		const TTMDBase::CollisionMesh& rMesh = pCollision->m_pMeshes[ i ];
		if ( rMesh.m_uiNumVertices < 3 || rMesh.m_uiNumVertices > 65536 ||
		     rMesh.m_uiNumIndices < 3 || rMesh.m_uiNumIndices % 3 != 0 ||
		     rMesh.m_uiNumIndices / 3 > s_uiMaxTerrainTriangles_PSP - m_uiTriangles ||
		     !ContainsTRBData( rPack.oTRB, rMesh.m_pVertices, rMesh.m_uiNumVertices * sizeof( TVector3 ) ) ||
		     !ContainsTRBData( rPack.oTRB, rMesh.m_pIndices, rMesh.m_uiNumIndices * sizeof( TUINT16 ) ) )
			return TFALSE;
		for ( TUINT j = 0; j < rMesh.m_uiNumIndices; j++ )
		{
			if ( rMesh.m_pIndices[ j ] >= rMesh.m_uiNumVertices )
				return TFALSE;
		}
		m_uiTriangles += rMesh.m_uiNumIndices / 3;
	}
	rPack.pCollision = pCollision;
	m_uiPacks++;
	return TTRUE;
}

void ATerrainCollision_PSP::Unload()
{
	for ( Pack& rPack : m_aPacks )
	{
		rPack.oTRB.Close();
		rPack.pCollision = TNULL;
	}
	m_uiPacks = 0;
	m_uiTriangles = 0;
}

static TBOOL AGetTerrainTriangleHeight_PSP( const TVector3& a_rA, const TVector3& a_rB, const TVector3& a_rC,
                                           TFLOAT a_fX, TFLOAT a_fZ, TFLOAT& a_rfY )
{
	// ATerrainInterface::CreateModelInstance rotates terrain by +90 degrees around X.
	// The stored triangle's XY plane is world XZ and its Z becomes world -Y.
	const TFLOAT fABX = a_rB.x - a_rA.x;
	const TFLOAT fABY = a_rB.y - a_rA.y;
	const TFLOAT fACX = a_rC.x - a_rA.x;
	const TFLOAT fACY = a_rC.y - a_rA.y;
	const TFLOAT fDet = fABX * fACY - fABY * fACX;
	if ( TMath::Abs( fDet ) < TMath::TFLOAT_EPSILON )
		return TFALSE;
	const TFLOAT fAX = a_fX - a_rA.x;
	const TFLOAT fAY = a_fZ - a_rA.y;
	const TFLOAT fU = ( fAX * fACY - fAY * fACX ) / fDet;
	const TFLOAT fV = ( fABX * fAY - fABY * fAX ) / fDet;
	if ( !( fU >= 0.0f && fV >= 0.0f && fU + fV <= 1.0f ) )
		return TFALSE;
	a_rfY = -( a_rA.z + fU * ( a_rB.z - a_rA.z ) + fV * ( a_rC.z - a_rA.z ) );
	return TMath::IsFinite( a_rfY );
}

TBOOL ATerrainCollision_PSP::CollideVertical( TFLOAT a_fX, TFLOAT a_fZ, TFLOAT a_fOriginY, TBOOL a_bDown, TFLOAT& a_rfDistance ) const
{
	TBOOL bHit = TFALSE;
	for ( TUINT i = 0; i < m_uiPacks; i++ )
	{
		const TTMDBase::CollisionHeader& rCollision = *m_aPacks[ i ].pCollision;
		for ( TINT mesh = 0; mesh < rCollision.m_iNumMeshes; mesh++ )
		{
			const TTMDBase::CollisionMesh& rMesh = rCollision.m_pMeshes[ mesh ];
			for ( TUINT j = 0; j < rMesh.m_uiNumIndices; j += 3 )
			{
				const TUINT16* pFace = &rMesh.m_pIndices[ j ];
				const TVector3* pVertices = rMesh.m_pVertices;
				TFLOAT fY;
				if ( !AGetTerrainTriangleHeight_PSP( pVertices[ pFace[ 0 ] ], pVertices[ pFace[ 1 ] ], pVertices[ pFace[ 2 ] ], a_fX, a_fZ, fY ) )
					continue;
				const TFLOAT fDistance = a_bDown ? fY - a_fOriginY : a_fOriginY - fY;
				if ( fDistance >= 0.0f && fDistance < a_rfDistance )
				{
					a_rfDistance = fDistance;
					bHit = TTRUE;
				}
			}
		}
	}
	return bHit;
}
