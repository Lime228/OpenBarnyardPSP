#pragma once

TOSHI_NAMESPACE_START

class TDebugConsole
{
public:
	TDebugConsole();
	~TDebugConsole();

	TBOOL Show( TBOOL a_bState );
	TBOOL Toggle();

	TBOOL IsVisible() const { return m_bVisible; }
	TBOOL IsCreated() const { return m_bCreated; }

private:
	TBOOL m_bVisible;
	TBOOL m_bCreated;
};

TOSHI_NAMESPACE_END
