#pragma once

#include <Platform/PSP/T2LocaleFactory_PSP.h>

class ALocaleManager_PSP
{
public:
	using Lang = TINT32;

	enum Lang_ : Lang
	{
		Lang_UNKNOWN = -1,
		Lang_English,
		Lang_EnglishUK,
		Lang_Japanese,
		Lang_German,
		Lang_Italian,
		Lang_Spanish,
		Lang_French,
		Lang_Netherlandic,
		Lang_NUMOF
	};

public:
	ALocaleManager_PSP();

	TBOOL SetLanguage( Lang a_eLanguage );
	void Destroy();

	const TCHAR* GetLanguageFilename( Lang a_eLanguage ) const;
	const TCHAR* GetCurrentLanguageName() const;
	const TWCHAR* GetString( TINT a_iString ) const;
	TUINT GetNumStrings() const { return m_oLocale.GetNumStrings(); }
	Lang GetLanguage() const { return m_eLanguage; }
	TBOOL IsCreated() const { return m_oLocale.IsCreated(); }

	static TBOOL IsLanguageSupported( Lang a_eLanguage );
	static Lang GetDefaultLanguage() { return Lang_EnglishUK; }
	static const TCHAR* GetLanguageName( Lang a_eLanguage );

private:
	Toshi::T2Locale_PSP        m_oLocale;
	Toshi::T2LocaleFactory_PSP m_oFactory;
	Lang                       m_eLanguage;
};
