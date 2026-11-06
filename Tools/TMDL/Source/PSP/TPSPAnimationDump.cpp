#include "pch.h"
#include "TPSPAnimationDump.h"

#include <File/TTRB.h>
#include <Plugins/PTRB.h>
#include <Render/TAnimation.h>
#include <Render/TTMDBase.h>
#include <Render/TSkeleton.h>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

TOSHI_NAMESPACE_USING

static void WriteJSONString_PSPAnimation( std::ostream& a_rOutput, const TCHAR* a_pValue, TSIZE a_uiLength )
{
	a_rOutput << '"';
	for ( TSIZE i = 0; a_pValue != TNULL && i < a_uiLength; i++ )
	{
		const TCHAR cCharacter = a_pValue[ i ];
		switch ( cCharacter )
		{
			case '"': a_rOutput << "\\\""; break;
			case '\\': a_rOutput << "\\\\"; break;
			case '\n': a_rOutput << "\\n"; break;
			case '\r': a_rOutput << "\\r"; break;
			case '\t': a_rOutput << "\\t"; break;
			default:
				if ( TUINT8( cCharacter ) < 0x20 )
					a_rOutput << '?';
				else
					a_rOutput << cCharacter;
				break;
		}
	}
	a_rOutput << '"';
}

static TBOOL IsKeyRangeValid_PSPAnimation( TINT a_iBase, TINT a_iCount, TINT a_iTotal )
{
	return a_iBase >= 0 && a_iCount >= 0 && a_iBase <= a_iTotal && a_iCount <= a_iTotal - a_iBase;
}

static TBOOL IsPoseFinite_PSPAnimation( const TQuaternion& a_rRotation, const TVector3& a_rTranslation )
{
	return std::isfinite( a_rRotation.x ) && std::isfinite( a_rRotation.y ) &&
	       std::isfinite( a_rRotation.z ) && std::isfinite( a_rRotation.w ) &&
	       std::isfinite( a_rTranslation.x ) && std::isfinite( a_rTranslation.y ) && std::isfinite( a_rTranslation.z ) &&
	       a_rRotation.MagnitudeSq() > 0.000001f;
}

static TBOOL ValidateSequenceBone_PSPAnimation(
    TSkeletonSequenceBone& a_rSequenceBone,
    TINT                   a_iTranslationCount,
    TINT                   a_iQuaternionCount
)
{
	const TUINT16 uiKeyCount = a_rSequenceBone.GetKeyCount();
	if ( uiKeyCount == 0 )
		return TTRUE;
	const TUINT8 uiMinimumKeySize = a_rSequenceBone.IsTranslateAnimated() ? 6 : 4;
	if ( a_rSequenceBone.m_pData == TNULL || a_rSequenceBone.m_iKeySize < uiMinimumKeySize )
		return TFALSE;

	TUINT16 uiPreviousTime = 0;
	for ( TUINT16 i = 0; i < uiKeyCount; i++ )
	{
		const TUINT16* pKey = a_rSequenceBone.GetKey( i );
		if ( ( i != 0 && pKey[ 0 ] <= uiPreviousTime ) || pKey[ 1 ] >= a_iQuaternionCount )
			return TFALSE;
		if ( a_rSequenceBone.IsTranslateAnimated() && pKey[ 2 ] >= a_iTranslationCount )
			return TFALSE;
		uiPreviousTime = pKey[ 0 ];
	}
	return TTRUE;
}

static TBOOL SampleSequenceBone_PSPAnimation(
    TSkeletonSequenceBone& a_rSequenceBone,
    TSkeletonBone&         a_rBone,
    TAnimVector*           a_pTranslations,
    TAnimQuaternion*       a_pQuaternions,
    TFLOAT                 a_fProgress,
    TQuaternion&           a_rRotation,
    TVector3&              a_rTranslation
)
{
	if ( a_rSequenceBone.GetKeyCount() == 0 )
	{
		a_rRotation = a_rBone.GetRotation();
		a_rTranslation = a_rBone.GetPosition();
	}
	else
	{
		TUINT16 uiCurrentKey = 0;
		TUINT16 uiFromKey;
		TUINT16 uiToKey;
		const TFLOAT fLerp = a_rSequenceBone.GetKeyPair( TINT( a_fProgress * 65535.0f ), uiCurrentKey, uiFromKey, uiToKey );
		const TUINT16* pFromKey = a_rSequenceBone.GetKey( uiFromKey );
		const TUINT16* pToKey = a_rSequenceBone.GetKey( uiToKey );
		TQuaternion::Slerp( a_rRotation, a_pQuaternions[ pFromKey[ 1 ] ], a_pQuaternions[ pToKey[ 1 ] ], fLerp );

		if ( a_rSequenceBone.IsTranslateAnimated() )
			a_rTranslation.Lerp( a_pTranslations[ pFromKey[ 2 ] ], a_pTranslations[ pToKey[ 2 ] ], fLerp );
		else
			a_rTranslation = a_rBone.GetPosition();
	}

	return IsPoseFinite_PSPAnimation( a_rRotation, a_rTranslation );
}

TBOOL ExportPSPAnimationDump(
    const TCHAR* a_pModelPath,
    const TCHAR* a_pKeyLibraryPath,
    const TCHAR* a_pSequenceName,
    TINT         a_iSampleRate,
    const TCHAR* a_pOutputPath
)
{
	if ( a_pModelPath == TNULL || a_pKeyLibraryPath == TNULL || a_pSequenceName == TNULL ||
	     a_pOutputPath == TNULL || a_iSampleRate < 1 || a_iSampleRate > 60 )
	{
		return TFALSE;
	}

	PTRB oModelTRB;
	PTRB oKeyLibraryTRB;
	if ( !oModelTRB.ReadFromFile( a_pModelPath ) || !oKeyLibraryTRB.ReadFromFile( a_pKeyLibraryPath ) )
		return TFALSE;
	auto oSkeleton = oModelTRB.GetSymbols()->Find<TSkeleton>( oModelTRB.GetSections(), "Skeleton" );
	auto oSkeletonHeader = oModelTRB.GetSymbols()->Find<TTMDBase::SkeletonHeader>( oModelTRB.GetSections(), "SkeletonHeader" );
	auto oKeyLibrary = oKeyLibraryTRB.GetSymbols()->Find<TKeyframeLibrary::TRBHeader>( oKeyLibraryTRB.GetSections(), "keylib" );
	if ( !oSkeleton || !oSkeletonHeader || !oKeyLibrary )
		return TFALSE;

	TTMDBase::SkeletonHeader& rSkeletonHeader = *oSkeletonHeader.get();
	TKeyframeLibrary::TRBHeader& rKeyLibrary = *oKeyLibrary.get();
	if ( !IsKeyRangeValid_PSPAnimation( rSkeletonHeader.m_iTBaseIndex, rSkeletonHeader.m_iTKeyCount, rKeyLibrary.m_iNumTranslations ) ||
	     !IsKeyRangeValid_PSPAnimation( rSkeletonHeader.m_iQBaseIndex, rSkeletonHeader.m_iQKeyCount, rKeyLibrary.m_iNumQuaternions ) ||
	     ( rSkeletonHeader.m_iTKeyCount != 0 && rKeyLibrary.m_pTranslations == TNULL ) ||
	     ( rSkeletonHeader.m_iQKeyCount != 0 && rKeyLibrary.m_pQuaternions == TNULL ) )
	{
		TERROR(
		    "PSP animation dump rejected key ranges (translations=%d+%d/%d quaternions=%d+%d/%d)\n",
		    rSkeletonHeader.m_iTBaseIndex,
		    rSkeletonHeader.m_iTKeyCount,
		    rKeyLibrary.m_iNumTranslations,
		    rSkeletonHeader.m_iQBaseIndex,
		    rSkeletonHeader.m_iQKeyCount,
		    rKeyLibrary.m_iNumQuaternions
		);
		return TFALSE;
	}

	TSkeleton& rSkeleton = *oSkeleton.get();
	const TINT iBoneCount = rSkeleton.GetBoneCount();
	const TINT iAutoBoneCount = rSkeleton.GetAutoBoneCount();
	if ( iBoneCount <= 0 || iBoneCount > 64 || rSkeleton.GetSequenceCount() <= 0 ||
	     iAutoBoneCount < 0 || iAutoBoneCount > iBoneCount ||
	     rSkeleton.m_pBones == TNULL || rSkeleton.m_SkeletonSequences == TNULL )
	{
		return TFALSE;
	}
	for ( TINT bone = 0; bone < iBoneCount; bone++ )
	{
		TSkeletonBone* pBone = rSkeleton.GetBone( bone );
		const TUINT8 uiNameLength = pBone->GetNameLength();
		const TINT iParentBone = pBone->GetParentBone();
		if ( uiNameLength == 0 || uiNameLength > 31 ||
		     ( bone == 0 && iParentBone != -1 ) ||
		     ( bone != 0 && ( iParentBone < 0 || iParentBone >= bone ) ) ||
		     rSkeleton.GetBoneID( pBone->GetName(), uiNameLength ) != bone )
		{
			return TFALSE;
		}
	}
	const TSIZE uiSequenceNameLength = TStringManager::String8Length( a_pSequenceName );
	const TINT iSequenceIndex = rSkeleton.GetSequenceID( a_pSequenceName, TUINT32( uiSequenceNameLength ) );
	if ( iSequenceIndex < 0 || iSequenceIndex >= rSkeleton.GetSequenceCount() )
		return TFALSE;
	TSkeletonSequence& rSequence = *rSkeleton.GetSequence( iSequenceIndex );
	if ( rSequence.IsOverlay() ||
	     ( rSequence.GetMode() != TSkeletonSequence::MODE_LOOPED && rSequence.GetMode() != TSkeletonSequence::MODE_CLAMPED ) ||
	     rSequence.GetDuration() <= 0.0f || !std::isfinite( rSequence.GetDuration() ) )
		return TFALSE;

	if ( rSequence.m_pSeqBones == TNULL )
		return TFALSE;
	for ( TINT bone = 0; bone < iAutoBoneCount; bone++ )
	{
		if ( !ValidateSequenceBone_PSPAnimation( *rSequence.GetBone( bone ), rSkeletonHeader.m_iTKeyCount, rSkeletonHeader.m_iQKeyCount ) )
			return TFALSE;
	}

	const TFLOAT fIntervalCount = rSequence.GetDuration() * TFLOAT( a_iSampleRate );
	if ( !std::isfinite( fIntervalCount ) || fIntervalCount > 64.0f )
		return TFALSE;
	const TINT iIntervals = TINT( std::ceil( fIntervalCount ) );
	const TINT iFrameCount = rSequence.GetMode() == TSkeletonSequence::MODE_LOOPED ? iIntervals : iIntervals + 1;
	if ( iFrameCount <= 0 || iFrameCount > 64 )
		return TFALSE;

	TAnimVector* pTranslations = rKeyLibrary.m_pTranslations != TNULL
	    ? rKeyLibrary.m_pTranslations + rSkeletonHeader.m_iTBaseIndex
	    : TNULL;
	TAnimQuaternion* pQuaternions = rKeyLibrary.m_pQuaternions + rSkeletonHeader.m_iQBaseIndex;
	std::ostringstream oOutput;
	oOutput.imbue( std::locale::classic() );
	oOutput << std::setprecision( 9 );
	oOutput << "{\n  \"sequence\": ";
	WriteJSONString_PSPAnimation( oOutput, rSequence.GetName(), rSequence.GetNameLength() );
	oOutput << ",\n  \"sample_rate\": " << a_iSampleRate
	        << ",\n  \"loop\": " << ( rSequence.GetMode() == TSkeletonSequence::MODE_LOOPED ? "true" : "false" )
	        << ",\n  \"frames\": [\n";

	for ( TINT frame = 0; frame < iFrameCount; frame++ )
	{
		const TFLOAT fTime = frame + 1 == iFrameCount && rSequence.GetMode() == TSkeletonSequence::MODE_CLAMPED
		    ? rSequence.GetDuration()
		    : TFLOAT( frame ) / TFLOAT( a_iSampleRate );
		const TFLOAT fProgress = TMath::Min( fTime / rSequence.GetDuration(), 1.0f );
		oOutput << "    {\n";
		for ( TINT bone = 0; bone < iBoneCount; bone++ )
		{
			TSkeletonBone& rBone = *rSkeleton.GetBone( bone );
			TQuaternion oRotation;
			TVector3 oTranslation;
			TBOOL bSampled;
			if ( bone < iAutoBoneCount )
			{
				bSampled = SampleSequenceBone_PSPAnimation(
				    *rSequence.GetBone( bone ), rBone, pTranslations, pQuaternions, fProgress, oRotation, oTranslation
				);
			}
			else
			{
				oRotation = rBone.GetRotation();
				oTranslation = rBone.GetPosition();
				bSampled = IsPoseFinite_PSPAnimation( oRotation, oTranslation );
			}
			if ( !bSampled )
			{
				return TFALSE;
			}
			oOutput << "      ";
			WriteJSONString_PSPAnimation( oOutput, rBone.GetName(), rBone.GetNameLength() );
			oOutput << ": { \"rotation\": [" << oRotation.x << ", " << oRotation.y << ", " << oRotation.z << ", " << oRotation.w
			        << "], \"translation\": [" << oTranslation.x << ", " << oTranslation.y << ", " << oTranslation.z << "] }";
			if ( bone + 1 != iBoneCount ) oOutput << ',';
			oOutput << '\n';
		}
		oOutput << "    }";
		if ( frame + 1 != iFrameCount ) oOutput << ',';
		oOutput << '\n';
	}
	oOutput << "  ]\n}\n";
	if ( !oOutput.good() )
		return TFALSE;

	std::ofstream oFile( a_pOutputPath, std::ios::out | std::ios::trunc );
	if ( !oFile )
		return TFALSE;
	oFile << oOutput.str();
	return oFile.good();
}
