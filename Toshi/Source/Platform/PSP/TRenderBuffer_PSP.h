#pragma once

#include "Toshi/Defines.h"
#include "Render/TIndexFactoryFormat.h"

TOSHI_NAMESPACE_START

class TVertexPoolResource_PSP
{
public:
	TVertexPoolResource_PSP();
	~TVertexPoolResource_PSP();

	TBOOL Create( TUINT16 a_uiVertexSize, TUINT16 a_uiMaxVertices );
	void  Destroy();

	TBYTE* Lock();
	void   Unlock( TUINT16 a_uiNumVertices );

	const void* GetData() const { return m_pData; }
	TUINT16     GetVertexSize() const { return m_uiVertexSize; }
	TUINT16     GetMaxVertices() const { return m_uiMaxVertices; }
	TUINT16     GetNumVertices() const { return m_uiNumVertices; }
	TBOOL       IsLocked() const { return m_bLocked; }

private:
	TBYTE*  m_pData;
	TUINT16 m_uiVertexSize;
	TUINT16 m_uiMaxVertices;
	TUINT16 m_uiNumVertices;
	TBOOL   m_bLocked;
};

class TIndexPoolResource_PSP
{
public:
	TIndexPoolResource_PSP();
	~TIndexPoolResource_PSP();

	TBOOL Create( TUINT16 a_uiMaxIndices );
	void  Destroy();

	TIndexType* Lock();
	void        Unlock( TUINT16 a_uiNumIndices );

	const TIndexType* GetData() const { return m_pData; }
	TUINT16           GetMaxIndices() const { return m_uiMaxIndices; }
	TUINT16           GetNumIndices() const { return m_uiNumIndices; }
	TBOOL             IsLocked() const { return m_bLocked; }

private:
	TIndexType* m_pData;
	TUINT16     m_uiMaxIndices;
	TUINT16     m_uiNumIndices;
	TBOOL       m_bLocked;
};

TOSHI_NAMESPACE_END
