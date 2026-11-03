#include "ToshiPCH.h"
#include "TAnimationMemory_PSP.h"

TOSHI_NAMESPACE_START

TSIZE TAnimationMemory_PSP::s_uiSampleCurrent = 0;
TSIZE TAnimationMemory_PSP::s_uiSamplePeak = 0;
TUINT TAnimationMemory_PSP::s_uiInstanceCount = 0;
TUINT TAnimationMemory_PSP::s_uiSkinnedModelCount = 0;
TUINT TAnimationMemory_PSP::s_uiAnimatedVertices = 0;
TUINT TAnimationMemory_PSP::s_uiAnimatedVertexPeak = 0;
TUINT32 TAnimationMemory_PSP::s_uiLastPoseMicroseconds = 0;
TUINT32 TAnimationMemory_PSP::s_uiMaxPoseMicroseconds = 0;
TUINT32 TAnimationMemory_PSP::s_uiLastSkinMicroseconds = 0;
TUINT32 TAnimationMemory_PSP::s_uiMaxSkinMicroseconds = 0;
TUINT32 TAnimationMemory_PSP::s_uiProfiledPoseCount = 0;
TUINT32 TAnimationMemory_PSP::s_uiProfiledSkinCount = 0;

static TUINT32 TAnimationClampTime_PSP( TUINT64 a_uiMicroseconds )
{
	return a_uiMicroseconds > 0xffffffffULL ? 0xffffffffU : TUINT32( a_uiMicroseconds );
}

TBOOL TAnimationMemory_PSP::ReserveSamples( TSIZE a_uiBytes )
{
	if ( a_uiBytes == 0 || a_uiBytes > SAMPLE_BUDGET - s_uiSampleCurrent )
		return TFALSE;
	s_uiSampleCurrent += a_uiBytes;
	if ( s_uiSamplePeak < s_uiSampleCurrent )
		s_uiSamplePeak = s_uiSampleCurrent;
	return TTRUE;
}

void TAnimationMemory_PSP::ReleaseSamples( TSIZE a_uiBytes )
{
	TASSERT( a_uiBytes <= s_uiSampleCurrent );
	s_uiSampleCurrent = a_uiBytes <= s_uiSampleCurrent ? s_uiSampleCurrent - a_uiBytes : 0;
}

TBOOL TAnimationMemory_PSP::ReserveInstance()
{
	if ( s_uiInstanceCount >= INSTANCE_BUDGET )
		return TFALSE;
	s_uiInstanceCount++;
	return TTRUE;
}

void TAnimationMemory_PSP::ReleaseInstance()
{
	TASSERT( s_uiInstanceCount != 0 );
	if ( s_uiInstanceCount != 0 )
		s_uiInstanceCount--;
}

TBOOL TAnimationMemory_PSP::ReserveSkinning( TUINT a_uiVertices )
{
	if ( a_uiVertices == 0 || s_uiSkinnedModelCount >= SKINNED_MODEL_BUDGET ||
	     a_uiVertices > ANIMATED_VERTEX_BUDGET - s_uiAnimatedVertices )
	{
		return TFALSE;
	}
	s_uiSkinnedModelCount++;
	s_uiAnimatedVertices += a_uiVertices;
	if ( s_uiAnimatedVertexPeak < s_uiAnimatedVertices )
		s_uiAnimatedVertexPeak = s_uiAnimatedVertices;
	return TTRUE;
}

void TAnimationMemory_PSP::ReleaseSkinning( TUINT a_uiVertices )
{
	TASSERT( s_uiSkinnedModelCount != 0 && a_uiVertices <= s_uiAnimatedVertices );
	if ( s_uiSkinnedModelCount != 0 )
		s_uiSkinnedModelCount--;
	s_uiAnimatedVertices = a_uiVertices <= s_uiAnimatedVertices ? s_uiAnimatedVertices - a_uiVertices : 0;
}

void TAnimationMemory_PSP::RecordPoseTime( TUINT64 a_uiMicroseconds )
{
	s_uiLastPoseMicroseconds = TAnimationClampTime_PSP( a_uiMicroseconds );
	if ( s_uiMaxPoseMicroseconds < s_uiLastPoseMicroseconds )
		s_uiMaxPoseMicroseconds = s_uiLastPoseMicroseconds;
	s_uiProfiledPoseCount++;
}

void TAnimationMemory_PSP::RecordSkinTime( TUINT64 a_uiMicroseconds )
{
	s_uiLastSkinMicroseconds = TAnimationClampTime_PSP( a_uiMicroseconds );
	if ( s_uiMaxSkinMicroseconds < s_uiLastSkinMicroseconds )
		s_uiMaxSkinMicroseconds = s_uiLastSkinMicroseconds;
	s_uiProfiledSkinCount++;
}

TAnimationMemoryStats_PSP TAnimationMemory_PSP::GetStats()
{
	return {
		s_uiSampleCurrent,
		s_uiSamplePeak,
		SAMPLE_BUDGET,
		s_uiInstanceCount,
		INSTANCE_BUDGET,
		s_uiSkinnedModelCount,
		SKINNED_MODEL_BUDGET,
		s_uiAnimatedVertices,
		s_uiAnimatedVertexPeak,
		ANIMATED_VERTEX_BUDGET,
		s_uiLastPoseMicroseconds,
		s_uiMaxPoseMicroseconds,
		s_uiLastSkinMicroseconds,
		s_uiMaxSkinMicroseconds,
		s_uiProfiledPoseCount,
		s_uiProfiledSkinCount,
	};
}

TOSHI_NAMESPACE_END
