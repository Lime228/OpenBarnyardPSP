#pragma once

class ADisplayModes_PSP : public Toshi::TObject
{
public:
	TDECLARE_CLASS( ADisplayModes_PSP, Toshi::TObject );

	struct Mode
	{
		TUINT32 m_uiWidth;
		TUINT32 m_uiHeight;
		TUINT32 m_uiColourDepth;
	};

public:
	ADisplayModes_PSP() = default;
	~ADisplayModes_PSP() = default;

	void Initialise();
	TBOOL DoesModeExist( const TUINT32& a_rWidth, const TUINT32& a_rHeight, const TUINT32& a_rColourDepth );

private:
	static constexpr Mode s_Mode = { 480, 272, 32 };
};
