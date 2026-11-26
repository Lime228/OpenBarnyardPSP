#pragma once

#include <File/TTRB.h>
#include <Math/TVector4.h>
#include <Render/TTMDBase.h>

class ATerrainCollision_PSP
{
public:
	static constexpr TUINT MAX_PACKS = 6;

	enum GroundedMove
	{
		GROUNDED_UNSUPPORTED,
		GROUNDED_ADVANCED,
		GROUNDED_CONTACT,
		GROUNDED_SLID
	};

	static TBOOL HasInitialPack();
	TBOOL LoadInitial();
	void Unload();

	TBOOL CollideVertical( TFLOAT a_fX, TFLOAT a_fZ, TFLOAT a_fOriginY, TBOOL a_bDown, TFLOAT& a_rfDistance ) const;
	GroundedMove MoveGrounded( const Toshi::TVector4& a_rPosition, const Toshi::TVector4& a_rFacing, TFLOAT a_fDistance,
	                           TFLOAT a_fRadius, Toshi::TVector4& a_rTarget ) const;
	TFLOAT GetCameraDistance( const Toshi::TVector4& a_rPivot, const Toshi::TVector4& a_rDirection, TFLOAT a_fDistance ) const;
	TUINT GetPackCount() const { return m_uiPacks; }
	TUINT GetLoadedPackMask() const { return ( 1U << m_uiPacks ) - 1; }
	void SetActivePackMask( TUINT a_uiMask ) { m_uiActivePackMask = a_uiMask & GetLoadedPackMask(); }
	TINT FindSection( const Toshi::TVector4& a_rCameraPosition, TUINT a_uiPrevious ) const;
	TUINT GetTriangleCount() const { return m_uiTriangles; }
	const TCHAR* GetFailedPath() const { return m_szPath; }

	static TBOOL ContainsTRBData( const Toshi::TTRB& a_rTRB, const void* a_pData, TSIZE a_uiBytes );

private:
	static constexpr TUINT MAX_MESHES_PER_PACK = 32;
	static constexpr TUINT MAX_TRIANGLES = 16 * 1024;
	static constexpr TUINT TRIANGLES_PER_BLOCK = 32;
	static constexpr TUINT INDICES_PER_BLOCK = 3 * TRIANGLES_PER_BLOCK;
	static constexpr TUINT MAX_TRIANGLE_BLOCKS = MAX_TRIANGLES / TRIANGLES_PER_BLOCK + MAX_PACKS * MAX_MESHES_PER_PACK;

	struct Bounds
	{
		Toshi::TVector3 vMin;
		Toshi::TVector3 vMax;

		void Include( const Toshi::TVector3& a_rPoint );
		TBOOL Overlaps( const Bounds& a_rBounds ) const;
	};

	struct MeshBounds
	{
		Bounds oBounds;
		TUINT uiFirstBlock;
	};

	struct Pack
	{
		Toshi::TTRB oTRB;
		const Toshi::TTMDBase::CollisionHeader* pCollision = TNULL;
		MeshBounds aBounds[ MAX_MESHES_PER_PACK ];
	};

	TBOOL LoadPack( const TCHAR* a_szName );
	TBOOL CollideSweptSphere( const Toshi::TVector4& a_rCenter, TFLOAT a_fRadius, const Toshi::TVector4& a_rDirection,
	                         TFLOAT& a_rfDistance, Toshi::TVector4& a_rNormal, TBOOL a_bCamera ) const;
	TBOOL ResolveGroundedHeight( Toshi::TVector4& a_rTarget, const Toshi::TVector4& a_rFacing, TFLOAT a_fRadius ) const;
	TBOOL CollideVerticalInPacks( TFLOAT a_fX, TFLOAT a_fZ, TFLOAT a_fOriginY, TBOOL a_bDown, TFLOAT& a_rfDistance,
	                             TUINT a_uiMask ) const;

	Pack m_aPacks[ MAX_PACKS ];
	Bounds m_aTriangleBounds[ MAX_TRIANGLE_BLOCKS ];
	TUINT m_uiPacks = 0;
	TUINT m_uiActivePackMask = 0;
	TUINT m_uiTriangles = 0;
	TUINT m_uiTriangleBlocks = 0;
	TCHAR m_szPath[ 88 ] = {};
};
