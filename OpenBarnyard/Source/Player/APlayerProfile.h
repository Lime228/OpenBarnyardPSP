#pragma once
#include <Toshi/TObject.h>
#include <Toshi/T2DList.h>

#include "GameInterface/AHackedQuickSave.h"

// Base of the player profiles (APlayerSimProfile is the only one used by the game).
class MODAPI APlayerProfile
    : public Toshi::TObject
    , public Toshi::T2DList<APlayerProfile>::Node
{
public:
	TDECLARE_CLASS( APlayerProfile, Toshi::TObject );

	using GENDER = TINT;
	enum GENDER_ : GENDER
	{
		GENDER_MALE   = 0,
		GENDER_FEMALE = 1,
		GENDER_NONE   = 2, // NOTE: also means "use the profile's gender" when passed to the getters
	};

public:
	// $Barnyard: FUNCTION 00620060
	APlayerProfile()
	    : m_eGender( GENDER_NONE )
	{}

	// $Barnyard: FUNCTION 00620090
	// $Barnyard: FUNCTION 00620050
	~APlayerProfile() {}

	virtual void  Reset() {}                                                    // +0x0c, DOES_NOTHING
	virtual TBOOL Update( TFLOAT a_fDeltaTime ) { return TFALSE; }              // +0x10, RETURNS_FALSE
	virtual TBOOL Save( AHackedQuickSave::Buffer* a_pBuffer ) { return TTRUE; } // +0x14, RETURNS_TRUE
	virtual TBOOL Load( AHackedQuickSave::Buffer* a_pBuffer ) { return TTRUE; } // +0x18, RETURNS_TRUE

	GENDER GetGender() const { return m_eGender; }
	void   SetGender( GENDER a_eGender ) { m_eGender = a_eGender; }

	static const TCHAR* GetHideTextureNameForGender( TINT a_iCostume, GENDER a_eGender )
	{
		static const TCHAR* s_aBoyHides[] = {
			"Hide_American",
			"Hide_Angus",
			"Hide_Beefmaster",
			"Hide_Spaniard",
			"Hide_Longhorn",
			"Hide_Brahman",
			"Hide_Holsien"
		}; // 0x00778ccc

		static const TCHAR* s_aGirlHides[] = {
			"Hide_Girl_American",
			"Hide_Girl_Angus",
			"Hide_Girl_Beefmaster",
			"Hide_Girl_Spaniard",
			"Hide_Girl_Longhorn",
			"Hide_Girl_Brahman",
			"Hide_Girl_Holsien"
		}; // 0x00778ce8
		return ( a_eGender != GENDER_MALE ) ? s_aGirlHides[ a_iCostume ] : s_aBoyHides[ a_iCostume ];
	}

protected:
	// 0x04: T2DList<APlayerProfile>::Node
	GENDER m_eGender; // 0x0c
};

TSTATICASSERT( sizeof( APlayerProfile ) == 0x10 );
