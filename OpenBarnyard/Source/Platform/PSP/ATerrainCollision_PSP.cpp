#include "pch.h"
#include "ATerrainCollision_PSP.h"

#include <File/TFile.h>
#include <Math/TMath.h>
#include <Math/TMathInline.h>
#include <Math/TPlane.h>
#include <Math/TVector3.h>
#include <Toshi/TStringManager.h>

TOSHI_NAMESPACE_USING

static constexpr TCHAR s_szInitialCollision_PSP[] = "local:Data/Terrain/EnvMain/initial.pscl";
static constexpr TSIZE s_uiCollisionNameBytes_PSP = 48;

static_assert( sizeof( TTMDBase::CollisionHeader ) == 8 );
static_assert( sizeof( TTMDBase::CollisionMesh ) == 28 );
static_assert( sizeof( TVector3 ) == 12 );

void ATerrainCollision_PSP::Bounds::Include( const TVector3& a_rPoint )
{
	vMin.Set( TMath::Min( vMin.x, a_rPoint.x ), TMath::Min( vMin.y, a_rPoint.y ), TMath::Min( vMin.z, a_rPoint.z ) );
	vMax.Set( TMath::Max( vMax.x, a_rPoint.x ), TMath::Max( vMax.y, a_rPoint.y ), TMath::Max( vMax.z, a_rPoint.z ) );
}

TBOOL ATerrainCollision_PSP::Bounds::Overlaps( const Bounds& a_rBounds ) const
{
	return vMin.x <= a_rBounds.vMax.x && vMax.x >= a_rBounds.vMin.x &&
	       vMin.y <= a_rBounds.vMax.y && vMax.y >= a_rBounds.vMin.y &&
	       vMin.z <= a_rBounds.vMax.z && vMax.z >= a_rBounds.vMin.z;
}

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
	m_uiActivePackMask = GetLoadedPackMask();
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
	     pCollision->m_iNumMeshes <= 0 || TUINT( pCollision->m_iNumMeshes ) > MAX_MESHES_PER_PACK ||
	     !ContainsTRBData( rPack.oTRB, pCollision->m_pMeshes, pCollision->m_iNumMeshes * sizeof( TTMDBase::CollisionMesh ) ) )
		return TFALSE;

	for ( TINT i = 0; i < pCollision->m_iNumMeshes; i++ )
	{
		const TTMDBase::CollisionMesh& rMesh = pCollision->m_pMeshes[ i ];
		if ( rMesh.m_uiNumVertices < 3 || rMesh.m_uiNumVertices > 65536 ||
		     rMesh.m_uiNumIndices < 3 || rMesh.m_uiNumIndices % 3 != 0 ||
		     rMesh.m_uiNumIndices / 3 > MAX_TRIANGLES - m_uiTriangles ||
		     !ContainsTRBData( rPack.oTRB, rMesh.m_pVertices, rMesh.m_uiNumVertices * sizeof( TVector3 ) ) ||
		     !ContainsTRBData( rPack.oTRB, rMesh.m_pIndices, rMesh.m_uiNumIndices * sizeof( TUINT16 ) ) )
			return TFALSE;
		if ( rMesh.m_uiNumCollTypes > rMesh.m_uiNumIndices / 3 ||
		     ( rMesh.m_uiNumCollTypes > 0 && !ContainsTRBData( rPack.oTRB, rMesh.m_pCollGroups, rMesh.m_uiNumCollTypes * sizeof( TTMDBase::CollisionGroup ) ) ) )
			return TFALSE;
		const TVector3& rFirst = rMesh.m_pVertices[ 0 ];
		MeshBounds& rBounds = rPack.aBounds[ i ];
		rBounds = { { rFirst, rFirst }, m_uiTriangleBlocks };
		for ( TUINT j = 0; j < rMesh.m_uiNumVertices; j++ )
		{
			const TVector3& rVertex = rMesh.m_pVertices[ j ];
			if ( !TMath::IsFinite( rVertex.x ) || !TMath::IsFinite( rVertex.y ) || !TMath::IsFinite( rVertex.z ) )
				return TFALSE;
			rBounds.oBounds.Include( rVertex );
		}
		for ( TUINT j = 0; j < rMesh.m_uiNumIndices; j += INDICES_PER_BLOCK )
		{
			if ( m_uiTriangleBlocks == MAX_TRIANGLE_BLOCKS )
				return TFALSE;
			const TUINT uiEnd = TMath::Min<TUINT>( rMesh.m_uiNumIndices, j + INDICES_PER_BLOCK );
			if ( rMesh.m_pIndices[ j ] >= rMesh.m_uiNumVertices )
				return TFALSE;
			const TVector3& rBlockFirst = rMesh.m_pVertices[ rMesh.m_pIndices[ j ] ];
			Bounds& rBlock = m_aTriangleBounds[ m_uiTriangleBlocks++ ];
			rBlock = { rBlockFirst, rBlockFirst };
			for ( TUINT k = j + 1; k < uiEnd; k++ )
			{
				const TUINT16 uiVertex = rMesh.m_pIndices[ k ];
				if ( uiVertex >= rMesh.m_uiNumVertices )
					return TFALSE;
				rBlock.Include( rMesh.m_pVertices[ uiVertex ] );
			}
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
	m_uiActivePackMask = 0;
	m_uiTriangles = 0;
	m_uiTriangleBlocks = 0;
}

static TBOOL AGetTerrainTriangleHeight_PSP( const TVector3& a_rA, const TVector3& a_rB, const TVector3& a_rC,
                                           TFLOAT a_fX, TFLOAT a_fZ, TFLOAT& a_rfY )
{
	// ATerrainInterface::CreateModelInstance rotates terrain by +90 degrees around X.
	// The stored triangle's XY plane is world XZ and its Z becomes world -Y.
	if ( ( a_fX < a_rA.x && a_fX < a_rB.x && a_fX < a_rC.x ) || ( a_fX > a_rA.x && a_fX > a_rB.x && a_fX > a_rC.x ) ||
	     ( a_fZ < a_rA.y && a_fZ < a_rB.y && a_fZ < a_rC.y ) || ( a_fZ > a_rA.y && a_fZ > a_rB.y && a_fZ > a_rC.y ) )
		return TFALSE;
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
	return CollideVerticalInPacks( a_fX, a_fZ, a_fOriginY, a_bDown, a_rfDistance, m_uiActivePackMask );
}

TINT ATerrainCollision_PSP::FindSection( const TVector4& a_rCameraPosition, TUINT a_uiPrevious ) const
{
	// GetSectionPlayerIsIn first tests the current section, then the resident sections in VIS order.
	TFLOAT fDistance = 100.0f;
	const TUINT uiPreviousMask = a_uiPrevious < m_uiPacks ? ( 1U << a_uiPrevious ) & m_uiActivePackMask : 0;
	if ( uiPreviousMask != 0 && CollideVerticalInPacks( a_rCameraPosition.x, a_rCameraPosition.z, a_rCameraPosition.y, TTRUE, fDistance, uiPreviousMask ) )
		return TINT( a_uiPrevious );
	for ( TUINT i = 0; i < m_uiPacks; i++ )
	{
		const TUINT uiMask = ( 1U << i ) & m_uiActivePackMask & ~uiPreviousMask;
		if ( uiMask != 0 && CollideVerticalInPacks( a_rCameraPosition.x, a_rCameraPosition.z, a_rCameraPosition.y, TTRUE, fDistance, uiMask ) )
			return TINT( i );
	}
	return -1;
}

TBOOL ATerrainCollision_PSP::CollideVerticalInPacks( TFLOAT a_fX, TFLOAT a_fZ, TFLOAT a_fOriginY, TBOOL a_bDown, TFLOAT& a_rfDistance,
                                                    TUINT a_uiMask ) const
{
	const TFLOAT fStartZ = -a_fOriginY;
	const TFLOAT fEndZ = fStartZ + ( a_bDown ? -a_rfDistance : a_rfDistance );
	const Bounds oRay{ { a_fX, a_fZ, TMath::Min( fStartZ, fEndZ ) }, { a_fX, a_fZ, TMath::Max( fStartZ, fEndZ ) } };
	TBOOL bHit = TFALSE;
	for ( TUINT i = 0; i < m_uiPacks; i++ )
	{
		if ( ( a_uiMask & ( 1U << i ) ) == 0 )
			continue;
		const TTMDBase::CollisionHeader& rCollision = *m_aPacks[ i ].pCollision;
		for ( TINT mesh = 0; mesh < rCollision.m_iNumMeshes; mesh++ )
		{
			const MeshBounds& rMeshBounds = m_aPacks[ i ].aBounds[ mesh ];
			if ( !rMeshBounds.oBounds.Overlaps( oRay ) )
				continue;
			const TTMDBase::CollisionMesh& rMesh = rCollision.m_pMeshes[ mesh ];
			TUINT uiBlock = rMeshBounds.uiFirstBlock;
			for ( TUINT j = 0; j < rMesh.m_uiNumIndices; j += INDICES_PER_BLOCK, uiBlock++ )
			{
				if ( !m_aTriangleBounds[ uiBlock ].Overlaps( oRay ) )
					continue;
				const TUINT uiEnd = TMath::Min<TUINT>( rMesh.m_uiNumIndices, j + INDICES_PER_BLOCK );
				for ( TUINT k = j; k < uiEnd; k += 3 )
				{
					const TUINT16* pFace = &rMesh.m_pIndices[ k ];
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
	}
	return bHit;
}

static TBOOL ACameraIgnoresTriangle_PSP( const TTRB& a_rTRB, const TTMDBase::CollisionMesh& a_rMesh, TUINT a_uiFace )
{
	if ( a_rMesh.m_uiNumCollTypes == 0 )
		return TFALSE;

	// TModelCollisionData groups consecutive triangles and falls back to group zero.
	const TCHAR* szGroup = a_rMesh.m_pCollGroups[ 0 ].pszName;
	for ( TUINT i = 0; i < a_rMesh.m_uiNumCollTypes; i++ )
	{
		const auto& rGroup = a_rMesh.m_pCollGroups[ i ];
		if ( a_uiFace < rGroup.uiNumFaces )
		{
			szGroup = rGroup.pszName;
			break;
		}
		a_uiFace -= rGroup.uiNumFaces;
	}

	// AGTAStyleCameraHelper's default mask is WATERWALL | CAMERA.
	static constexpr TCHAR s_szWaterWall[] = "coll_waterwall";
	static constexpr TCHAR s_szCamera[] = "collision_camera";
	return ( ATerrainCollision_PSP::ContainsTRBData( a_rTRB, szGroup, sizeof( s_szWaterWall ) ) &&
	         TStringManager::String8Compare( szGroup, s_szWaterWall, sizeof( s_szWaterWall ) ) == 0 ) ||
	       ( ATerrainCollision_PSP::ContainsTRBData( a_rTRB, szGroup, sizeof( s_szCamera ) ) &&
	         TStringManager::String8Compare( szGroup, s_szCamera, sizeof( s_szCamera ) ) == 0 );
}

static TBOOL ASweepTerrainTriangle_PSP( const TVector3& a_rA, const TVector3& a_rB, const TVector3& a_rC,
                                       const TVector4& a_rCenter, TFLOAT a_fRadius, const TVector4& a_rDirection,
                                       TFLOAT& a_rfDistance, TVector4& a_rNormal, TBOOL a_bCamera )
{
	TVector3 vNormal;
	vNormal.CrossProduct( a_rB - a_rA, a_rC - a_rA );
	const TFLOAT fNormalSq = vNormal.MagnitudeSq();
	if ( fNormalSq <= TMath::TFLOAT_EPSILON )
		return TFALSE;
	vNormal.Normalize();
	// The grounded path resolves upward-facing walkable surfaces with the two foot rays.
	if ( !a_bCamera && vNormal.z >= 0.55f )
		return TFALSE;

	const TPlane oPlane( vNormal, a_rA );
	const TVector3* const apVertices[] = { &a_rA, &a_rB, &a_rC };
	TFLOAT fDistance = a_rfDistance;
	TVector4 vHitNormal( vNormal );
	if ( TMath::SweptSphereVsTriangle( a_rCenter, a_fRadius, a_rDirection, apVertices, fDistance, oPlane, vHitNormal ) == 0 ||
	     !( fDistance < a_rfDistance && TVector4::DotProduct3( vHitNormal, a_rDirection ) < -0.0001f ) )
		return TFALSE;
	a_rfDistance = fDistance;
	a_rNormal = vHitNormal;
	return TTRUE;
}

TBOOL ATerrainCollision_PSP::CollideSweptSphere( const TVector4& a_rCenter, TFLOAT a_fRadius, const TVector4& a_rDirection,
                                                 TFLOAT& a_rfDistance, TVector4& a_rNormal, TBOOL a_bCamera ) const
{
	// Inverse of the original terrain transform, used by the foot rays as well.
	const TVector4 vCenter( a_rCenter.x, a_rCenter.z, -a_rCenter.y, 1.0f );
	const TVector4 vDirection( a_rDirection.x, a_rDirection.z, -a_rDirection.y, 0.0f );
	const TVector3 vPadding( a_fRadius, a_fRadius, a_fRadius );
	Bounds oSweep{ vCenter.AsVector3(), vCenter.AsVector3() };
	const TVector4 vEnd = vCenter + vDirection * a_rfDistance;
	oSweep.Include( vEnd.AsVector3() );
	oSweep.vMin = oSweep.vMin - vPadding;
	oSweep.vMax = oSweep.vMax + vPadding;

	TBOOL bHit = TFALSE;
	TVector4 vNormal;
	for ( TUINT i = 0; i < m_uiPacks; i++ )
	{
		if ( ( m_uiActivePackMask & ( 1U << i ) ) == 0 )
			continue;
		const TTMDBase::CollisionHeader& rCollision = *m_aPacks[ i ].pCollision;
		for ( TINT mesh = 0; mesh < rCollision.m_iNumMeshes; mesh++ )
		{
			const MeshBounds& rMeshBounds = m_aPacks[ i ].aBounds[ mesh ];
			if ( !rMeshBounds.oBounds.Overlaps( oSweep ) )
				continue;
			const TTMDBase::CollisionMesh& rMesh = rCollision.m_pMeshes[ mesh ];
			TUINT uiBlock = rMeshBounds.uiFirstBlock;
			for ( TUINT j = 0; j < rMesh.m_uiNumIndices; j += INDICES_PER_BLOCK, uiBlock++ )
			{
				if ( !m_aTriangleBounds[ uiBlock ].Overlaps( oSweep ) )
					continue;
				const TUINT uiEnd = TMath::Min<TUINT>( rMesh.m_uiNumIndices, j + INDICES_PER_BLOCK );
				for ( TUINT k = j; k < uiEnd; k += 3 )
				{
					const TUINT16* pFace = &rMesh.m_pIndices[ k ];
					const TVector3& rA = rMesh.m_pVertices[ pFace[ 0 ] ];
					const TVector3& rB = rMesh.m_pVertices[ pFace[ 1 ] ];
					const TVector3& rC = rMesh.m_pVertices[ pFace[ 2 ] ];
					Bounds oFace{ rA, rA };
					oFace.Include( rB );
					oFace.Include( rC );
					if ( !oFace.Overlaps( oSweep ) || ( a_bCamera && ACameraIgnoresTriangle_PSP( m_aPacks[ i ].oTRB, rMesh, k / 3 ) ) )
						continue;
					if ( ASweepTerrainTriangle_PSP( rA, rB, rC, vCenter, a_fRadius, vDirection, a_rfDistance, vNormal, a_bCamera ) )
						bHit = TTRUE;
				}
			}
		}
	}
	if ( bHit )
		a_rNormal.Set( vNormal.x, -vNormal.z, vNormal.y, 0.0f );
	return bHit;
}

ATerrainCollision_PSP::GroundedMove ATerrainCollision_PSP::MoveGrounded( const TVector4& a_rPosition, const TVector4& a_rFacing,
                                                                        TFLOAT a_fDistance, TFLOAT a_fRadius, TVector4& a_rTarget ) const
{
	a_rTarget = a_rPosition;
	TVector4 vDirection = a_rFacing;
	TFLOAT fRemaining = a_fDistance;
	GroundedMove eMove = GROUNDED_ADVANCED;
	static constexpr TUINT uiMaxContacts = 3;
	for ( TUINT i = 0; i < uiMaxContacts && fRemaining > 0.0001f; i++ )
	{
		// APlayerSteer::SweepAndSlide uses a lower sphere and a second sphere 1.8 radii above it.
		const TVector4 vLower( a_rTarget.x, a_rTarget.y - a_fRadius, a_rTarget.z, 1.0f );
		const TVector4 vUpper( vLower.x, vLower.y - a_fRadius * 1.8f, vLower.z, 1.0f );
		TFLOAT fTravel = fRemaining;
		TVector4 vNormal;
		const TBOOL bLower = CollideSweptSphere( vLower, a_fRadius, vDirection, fTravel, vNormal, TFALSE );
		const TBOOL bUpper = CollideSweptSphere( vUpper, a_fRadius, vDirection, fTravel, vNormal, TFALSE );
		a_rTarget.x += vDirection.x * fTravel;
		a_rTarget.z += vDirection.z * fTravel;
		if ( fTravel > 0.0001f && eMove == GROUNDED_CONTACT )
			eMove = GROUNDED_SLID;
		if ( !bLower && !bUpper )
			break;
		if ( eMove == GROUNDED_ADVANCED )
			eMove = GROUNDED_CONTACT;

		// Keep the slide in the horizontal plane so the foot rays own the vertical motion.
		if ( vNormal.x * vNormal.x + vNormal.z * vNormal.z <= TMath::TFLOAT_EPSILON )
			break;
		vNormal.NormaliseXZ();
		TFLOAT fSlide = 0.0f;
		TVector4 vSlide;
		TMath::SlideAlongPlane( vDirection, vNormal, 1.0f, fRemaining, fTravel, fSlide, vSlide, 0.1f, 0.004f, 1.0f );
		fRemaining = fSlide;
		if ( fRemaining > 0.0f )
			vDirection = vSlide;
	}
	if ( a_rTarget.x == a_rPosition.x && a_rTarget.z == a_rPosition.z )
		return eMove;
	return ResolveGroundedHeight( a_rTarget, a_rFacing, a_fRadius ) ? eMove : GROUNDED_UNSUPPORTED;
}

TFLOAT ATerrainCollision_PSP::GetCameraDistance( const TVector4& a_rPivot, const TVector4& a_rDirection, TFLOAT a_fDistance ) const
{
	TFLOAT fDistance = a_fDistance;
	TVector4 vNormal;
	// AGTAStyleCameraHelper tries its smaller sphere only when the camera is close to the pivot.
	if ( CollideSweptSphere( a_rPivot, 0.43f, a_rDirection, fDistance, vNormal, TTRUE ) && fDistance < 1.0f )
	{
		fDistance = a_fDistance;
		CollideSweptSphere( a_rPivot, 0.2f, a_rDirection, fDistance, vNormal, TTRUE );
		fDistance = TMath::Min( fDistance, 1.0f );
	}
	return TMath::Max( fDistance, 0.01f );
}

TBOOL ATerrainCollision_PSP::ResolveGroundedHeight( TVector4& a_rTarget, const TVector4& a_rFacing, TFLOAT a_fRadius ) const
{
	// APlayerSteer::GetGroundHeight uses two rays at half the unit radius and keeps the higher contact.
	const TFLOAT fSideX = a_rFacing.z * a_fRadius * 0.5f;
	const TFLOAT fSideZ = -a_rFacing.x * a_fRadius * 0.5f;
	const TFLOAT fOriginY = a_rTarget.y - 1.0f;
	TFLOAT fLeft = 40.0f;
	TFLOAT fRight = 40.0f;
	if ( !CollideVertical( a_rTarget.x + fSideX, a_rTarget.z + fSideZ, fOriginY, TTRUE, fRight ) ||
	     !CollideVertical( a_rTarget.x - fSideX, a_rTarget.z - fSideZ, fOriginY, TTRUE, fLeft ) )
		return TFALSE;

	// This path keeps both feet supported until the airborne states are available.
	if ( TMath::Max( TMath::Abs( fLeft - 1.0f ), TMath::Abs( fRight - 1.0f ) ) > a_fRadius )
		return TFALSE;
	a_rTarget.y = fOriginY + TMath::Min( fLeft, fRight );
	return TTRUE;
}
