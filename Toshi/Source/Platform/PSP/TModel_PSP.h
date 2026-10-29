#pragma once

#include "TRenderBuffer_PSP.h"

TOSHI_NAMESPACE_START

enum TModelFormat_PSP : TUINT32
{
	TMODELFORMAT_PSP_COLOUR_XYZ = 0,
};

struct TModelDataHeader_PSP
{
	static constexpr TUINT32 MAGIC   = 0x53454D50;
	static constexpr TUINT32 VERSION = 1;

	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiVertexCount;
	TUINT32 uiIndexCount;
	TUINT32 uiVertexSize;
	TUINT32 uiFormat;
};

struct TModelVertex_PSP
{
	TUINT32 uiColour;
	TFLOAT  fX;
	TFLOAT  fY;
	TFLOAT  fZ;
};

static_assert( sizeof( TModelDataHeader_PSP ) == 24, "Unexpected PSP model header layout" );
static_assert( sizeof( TModelVertex_PSP ) == 16, "Unexpected PSP model vertex layout" );

class TModel_PSP
{
public:
	TModel_PSP();
	~TModel_PSP();

	void SetData( const void* a_pData, TUINT a_uiDataSize );
	void GetData( const void*& a_rData, TUINT& a_rDataSize ) const;

	TBOOL Load();
	void  Unload();

	const TVertexPoolResource_PSP& GetVertices() const { return m_Vertices; }
	const TIndexPoolResource_PSP&  GetIndices() const { return m_Indices; }
	TBOOL IsLoaded() const { return m_Vertices.GetData() != TNULL && m_Indices.GetData() != TNULL; }

private:
	const void*             m_pData;
	TUINT                   m_uiDataSize;
	TVertexPoolResource_PSP m_Vertices;
	TIndexPoolResource_PSP  m_Indices;
};

TOSHI_NAMESPACE_END
