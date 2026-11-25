#pragma once

#include <File/TTRB.h>
#include <Math/TVector4.h>
#include <Render/TTMDBase.h>

class ATerrainCollision_PSP
{
public:
	static TBOOL HasInitialPack();
	TBOOL LoadInitial();
	void Unload();

	TBOOL CollideVertical( TFLOAT a_fX, TFLOAT a_fZ, TFLOAT a_fOriginY, TBOOL a_bDown, TFLOAT& a_rfDistance ) const;
	TBOOL ResolveGroundedStep( Toshi::TVector4& a_rTarget, const Toshi::TVector4& a_rFacing, TFLOAT a_fRadius ) const;
	TUINT GetPackCount() const { return m_uiPacks; }
	TUINT GetTriangleCount() const { return m_uiTriangles; }
	const TCHAR* GetFailedPath() const { return m_szPath; }

	static TBOOL ContainsTRBData( const Toshi::TTRB& a_rTRB, const void* a_pData, TSIZE a_uiBytes );

private:
	static constexpr TUINT MAX_PACKS = 6;
	static constexpr TUINT MAX_MESHES_PER_PACK = 32;

	struct MeshBounds
	{
		TFLOAT fMinX;
		TFLOAT fMaxX;
		TFLOAT fMinZ;
		TFLOAT fMaxZ;
	};

	struct Pack
	{
		Toshi::TTRB oTRB;
		const Toshi::TTMDBase::CollisionHeader* pCollision = TNULL;
		MeshBounds aBounds[ MAX_MESHES_PER_PACK ];
	};

	TBOOL LoadPack( const TCHAR* a_szName );

	Pack m_aPacks[ MAX_PACKS ];
	TUINT m_uiPacks = 0;
	TUINT m_uiTriangles = 0;
	TCHAR m_szPath[ 88 ] = {};
};
