#pragma once

#include <File/TTRB.h>
#include <Render/TTMDBase.h>

class ATerrainCollision_PSP
{
public:
	static TBOOL HasInitialPack();
	TBOOL LoadInitial();
	void Unload();

	TBOOL CollideVertical( TFLOAT a_fX, TFLOAT a_fZ, TFLOAT a_fOriginY, TBOOL a_bDown, TFLOAT& a_rfDistance ) const;
	TUINT GetPackCount() const { return m_uiPacks; }
	TUINT GetTriangleCount() const { return m_uiTriangles; }
	const TCHAR* GetFailedPath() const { return m_szPath; }

	static TBOOL ContainsTRBData( const Toshi::TTRB& a_rTRB, const void* a_pData, TSIZE a_uiBytes );

private:
	static constexpr TUINT MAX_PACKS = 6;

	struct Pack
	{
		Toshi::TTRB oTRB;
		const Toshi::TTMDBase::CollisionHeader* pCollision = TNULL;
	};

	TBOOL LoadPack( const TCHAR* a_szName );

	Pack m_aPacks[ MAX_PACKS ];
	TUINT m_uiPacks = 0;
	TUINT m_uiTriangles = 0;
	TCHAR m_szPath[ 88 ] = {};
};
