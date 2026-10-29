#pragma once

#include "Toshi/Defines.h"

TOSHI_NAMESPACE_START

class TOSHI_API THPTimer
{
public:
	THPTimer();
	~THPTimer();

	TUINT32 GetRaw32();
	TINT64  GetRaw64();

	void Update();
	void Reset();

public:
	TFLOAT GetDelta() const { return m_fDelta; }
	TFLOAT GetCurrentSeconds() const { return m_fCurrentSeconds; }

private:
	TUINT64 m_iFrequency;
	TFLOAT  m_fCurrentSeconds;
	TFLOAT  m_Unused;
	TUINT64 m_iOldTime;
	TUINT64 m_iCurrentTime;
	TFLOAT  m_fDelta;
};

TOSHI_NAMESPACE_END
