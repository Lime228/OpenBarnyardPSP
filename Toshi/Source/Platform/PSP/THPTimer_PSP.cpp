#include <Toshi/Defines.h>
#include <Toshi/THPTimer.h>

#include <psprtc.h>

TOSHI_NAMESPACE_START

THPTimer::THPTimer()
{
	Reset();
}

THPTimer::~THPTimer() = default;

TUINT32 THPTimer::GetRaw32()
{
	return static_cast<TUINT32>( GetRaw64() );
}

TINT64 THPTimer::GetRaw64()
{
	TUINT64 uiTick = 0;
	sceRtcGetCurrentTick( &uiTick );
	return static_cast<TINT64>( uiTick );
}

void THPTimer::Reset()
{
	m_iFrequency = sceRtcGetTickResolution();
	sceRtcGetCurrentTick( &m_iCurrentTime );
	m_iOldTime        = m_iCurrentTime;
	m_fCurrentSeconds = 0.0f;
	m_Unused          = 0.0f;
	m_fDelta           = 0.0f;
}

void THPTimer::Update()
{
	m_iOldTime = m_iCurrentTime;
	sceRtcGetCurrentTick( &m_iCurrentTime );

	if ( m_iFrequency == 0 )
	{
		m_fDelta = 0.0f;
		return;
	}

	const TDOUBLE fSecondsPerTick = 1.0 / static_cast<TDOUBLE>( m_iFrequency );
	m_fDelta                     = static_cast<TFLOAT>( ( m_iCurrentTime - m_iOldTime ) * fSecondsPerTick );
	m_fCurrentSeconds            = static_cast<TFLOAT>( m_iCurrentTime * fSecondsPerTick );
}

TOSHI_NAMESPACE_END
