#pragma once

#include "Toshi/Defines.h"

TOSHI_NAMESPACE_START

struct TAnimationMemoryStats_PSP
{
	TSIZE  uiSampleCurrent;
	TSIZE  uiSamplePeak;
	TSIZE  uiSampleBudget;
	TUINT  uiInstanceCount;
	TUINT  uiInstanceBudget;
	TUINT  uiSkinnedModelCount;
	TUINT  uiSkinnedModelBudget;
	TUINT  uiAnimatedVertices;
	TUINT  uiAnimatedVertexPeak;
	TUINT  uiAnimatedVertexBudget;
	TUINT32 uiLastPoseMicroseconds;
	TUINT32 uiMaxPoseMicroseconds;
	TUINT32 uiLastSkinMicroseconds;
	TUINT32 uiMaxSkinMicroseconds;
	TUINT32 uiProfiledPoseCount;
	TUINT32 uiProfiledSkinCount;
};

class TAnimationMemory_PSP
{
public:
	static constexpr TSIZE SAMPLE_BUDGET = 2 * 1024 * 1024;
	static constexpr TUINT INSTANCE_BUDGET = 8;
	static constexpr TUINT SKINNED_MODEL_BUDGET = 4;
	static constexpr TUINT ANIMATED_VERTEX_BUDGET = 8192;

public:
	static TBOOL ReserveSamples( TSIZE a_uiBytes );
	static void ReleaseSamples( TSIZE a_uiBytes );
	static TBOOL ReserveInstance();
	static void ReleaseInstance();
	static TBOOL ReserveSkinning( TUINT a_uiVertices );
	static void ReleaseSkinning( TUINT a_uiVertices );
	static void RecordPoseTime( TUINT64 a_uiMicroseconds );
	static void RecordSkinTime( TUINT64 a_uiMicroseconds );
	static TAnimationMemoryStats_PSP GetStats();

private:
	static TSIZE s_uiSampleCurrent;
	static TSIZE s_uiSamplePeak;
	static TUINT s_uiInstanceCount;
	static TUINT s_uiSkinnedModelCount;
	static TUINT s_uiAnimatedVertices;
	static TUINT s_uiAnimatedVertexPeak;
	static TUINT32 s_uiLastPoseMicroseconds;
	static TUINT32 s_uiMaxPoseMicroseconds;
	static TUINT32 s_uiLastSkinMicroseconds;
	static TUINT32 s_uiMaxSkinMicroseconds;
	static TUINT32 s_uiProfiledPoseCount;
	static TUINT32 s_uiProfiledSkinCount;
};

TOSHI_NAMESPACE_END
