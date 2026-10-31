#include "ALocaleManager_PSP.h"

static const TCHAR* s_apszLanguageFiles_PSP[ ALocaleManager_PSP::Lang_NUMOF ] = {
	TNULL,
	"local:bootstrap_locale.ploc",
	TNULL,
	TNULL,
	TNULL,
	TNULL,
	TNULL,
	TNULL,
};

ALocaleManager_PSP::ALocaleManager_PSP()
    : m_eLanguage( Lang_UNKNOWN )
{
}

TBOOL ALocaleManager_PSP::SetLanguage( Lang a_eLanguage )
{
	if ( !IsLanguageSupported( a_eLanguage ) )
		return TFALSE;

	if ( m_eLanguage == a_eLanguage && m_oLocale.IsCreated() )
		return TTRUE;

	if ( !m_oFactory.CreateLocaleFromFile( m_oLocale, GetLanguageFilename( a_eLanguage ) ) )
	{
		m_eLanguage = Lang_UNKNOWN;
		return TFALSE;
	}

	m_eLanguage = a_eLanguage;
	return TTRUE;
}

void ALocaleManager_PSP::Destroy()
{
	m_oLocale.Destroy();
	m_eLanguage = Lang_UNKNOWN;
}

const TCHAR* ALocaleManager_PSP::GetLanguageFilename( Lang a_eLanguage ) const
{
	return IsLanguageSupported( a_eLanguage ) ? s_apszLanguageFiles_PSP[ a_eLanguage ] : TNULL;
}

const TCHAR* ALocaleManager_PSP::GetCurrentLanguageName() const
{
	return GetLanguageName( m_eLanguage );
}

const TWCHAR* ALocaleManager_PSP::GetString( TINT a_iString ) const
{
	return a_iString >= 0 ? m_oLocale.GetString( TUINT( a_iString ) ) : TNULL;
}

TBOOL ALocaleManager_PSP::IsLanguageSupported( Lang a_eLanguage )
{
	return a_eLanguage == Lang_EnglishUK;
}

const TCHAR* ALocaleManager_PSP::GetLanguageName( Lang a_eLanguage )
{
	return IsLanguageSupported( a_eLanguage ) ? "ENG" : "";
}
