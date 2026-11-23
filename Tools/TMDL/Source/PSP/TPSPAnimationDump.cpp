#include "pch.h"
#include "TPSPAnimationDump.h"

#include <File/TTRB.h>
#include <Plugins/PTRB.h>
#include <Render/TAnimation.h>
#include <Render/TTMDBase.h>
#include <Render/TSkeleton.h>

#include <cmath>
#include <cstdio>
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

static TSIZE GetBytesRemaining_PSPAnimation( PTRBSections& a_rSections, const void* a_pValue )
{
	const TUINTPTR uiAddress = TREINTERPRETCAST( TUINTPTR, a_pValue );
	for ( TUINT i = 0; i < a_rSections.GetStackCount(); i++ )
	{
		auto pStack = a_rSections.GetStack( i );
		const TUINTPTR uiStart = TREINTERPRETCAST( TUINTPTR, pStack->GetBuffer() );
		const TSIZE uiSize = pStack->GetUsedSize();
		if ( uiAddress >= uiStart && uiAddress - uiStart < uiSize )
			return uiSize - ( uiAddress - uiStart );
	}
	return 0;
}

static TSIZE GetNameLength_PSPAnimation( const TCHAR* a_pName, TSIZE a_uiMaxLength )
{
	TSIZE uiLength = 0;
	while ( uiLength < a_uiMaxLength && a_pName[ uiLength ] != '\0' )
		uiLength++;
	return uiLength;
}

TBOOL ExportPSPAnimationInfo( const TCHAR* a_pInputPath, const TCHAR* a_pOutputPath )
{
	PTRB oTRB;
	if ( a_pInputPath == TNULL || a_pOutputPath == TNULL || !oTRB.ReadFromFile( a_pInputPath ) )
		return TFALSE;

	auto pSections = oTRB.GetSections();
	auto pSymbols = oTRB.GetSymbols();
	auto oKeyLibrary = pSymbols->Find<TKeyframeLibrary::TRBHeader>( pSections, "keylib" );
	std::ostringstream oOutput;
	oOutput.imbue( std::locale::classic() );
	oOutput << std::setprecision( 9 ) << "{\n  \"version\": 1,\n  \"library\": ";
	if ( oKeyLibrary )
	{
		if ( GetBytesRemaining_PSPAnimation( *pSections, oKeyLibrary.get() ) < sizeof( *oKeyLibrary ) )
			return TFALSE;
		const TKeyframeLibrary::TRBHeader& rLibrary = *oKeyLibrary;
		const TSIZE uiAvailable = TMath::Min( GetBytesRemaining_PSPAnimation( *pSections, rLibrary.m_szName ), TSIZE( 32 ) );
		const TSIZE uiLength = GetNameLength_PSPAnimation( rLibrary.m_szName, uiAvailable );
		if ( uiLength == 0 || uiLength == uiAvailable || rLibrary.m_iNumTranslations < 0 ||
		     rLibrary.m_iNumQuaternions < 0 || rLibrary.m_iNumScales < 0 )
			return TFALSE;
		WriteJSONString_PSPAnimation( oOutput, rLibrary.m_szName, uiLength );
		oOutput << ",\n  \"translations\": " << rLibrary.m_iNumTranslations
		        << ",\n  \"quaternions\": " << rLibrary.m_iNumQuaternions
		        << ",\n  \"scales\": " << rLibrary.m_iNumScales;
	}
	else
	{
		auto oSkeleton = pSymbols->Find<TSkeleton>( pSections, "Skeleton" );
		auto oHeader = pSymbols->Find<TTMDBase::SkeletonHeader>( pSections, "SkeletonHeader" );
		if ( !oSkeleton || !oHeader || GetBytesRemaining_PSPAnimation( *pSections, oSkeleton.get() ) < sizeof( TSkeleton ) ||
		     GetBytesRemaining_PSPAnimation( *pSections, oHeader.get() ) < sizeof( TTMDBase::SkeletonHeader ) )
			return TFALSE;
		TSkeleton& rSkeleton = *oSkeleton;
		const TTMDBase::SkeletonHeader& rHeader = *oHeader;
		const TINT iCount = rSkeleton.GetSequenceCount();
		if ( iCount < 0 || ( iCount > 0 && TSIZE( iCount ) > GetBytesRemaining_PSPAnimation( *pSections, rSkeleton.GetSequences() ) / sizeof( TSkeletonSequence ) ) )
			return TFALSE;
		WriteJSONString_PSPAnimation( oOutput, rHeader.m_szTKLName, GetNameLength_PSPAnimation( rHeader.m_szTKLName, sizeof( rHeader.m_szTKLName ) ) );
		oOutput << ",\n  \"bones\": " << rSkeleton.GetBoneCount()
		        << ",\n  \"translations\": [" << rHeader.m_iTBaseIndex << ", " << rHeader.m_iTKeyCount << ']'
		        << ",\n  \"quaternions\": [" << rHeader.m_iQBaseIndex << ", " << rHeader.m_iQKeyCount << ']'
		        << ",\n  \"scales\": [" << rHeader.m_iSBaseIndex << ", " << rHeader.m_iSKeyCount << ']'
		        << ",\n  \"sequences\": [";
		for ( TINT i = 0; i < iCount; i++ )
		{
			const TSkeletonSequence& rSequence = *rSkeleton.GetSequence( i );
			if ( rSequence.GetNameLength() > sizeof( rSequence.m_szName ) || !std::isfinite( rSequence.GetDuration() ) )
				return TFALSE;
			oOutput << ( i == 0 ? "\n" : ",\n" ) << "    { \"name\": ";
			WriteJSONString_PSPAnimation( oOutput, rSequence.GetName(), rSequence.GetNameLength() );
			oOutput << ", \"duration\": " << rSequence.GetDuration() << ", \"mode\": " << rSequence.GetMode()
			        << ", \"overlay\": " << ( rSequence.IsOverlay() ? "true" : "false" ) << " }";
		}
		oOutput << ( iCount == 0 ? "]" : "\n  ]" );
	}

	std::ofstream oFile( a_pOutputPath, std::ios::out | std::ios::trunc );
	oFile << oOutput.str() << "\n}\n";
	return oFile.good();
}

static TBOOL IsPoseFinite_PSPAnimation( const TQuaternion& a_rRotation, const TVector3& a_rTranslation )
{
	return std::isfinite( a_rRotation.x ) && std::isfinite( a_rRotation.y ) &&
	       std::isfinite( a_rRotation.z ) && std::isfinite( a_rRotation.w ) &&
	       std::isfinite( a_rTranslation.x ) && std::isfinite( a_rTranslation.y ) && std::isfinite( a_rTranslation.z ) &&
	       a_rRotation.MagnitudeSq() > 0.000001f;
}

static TBOOL ValidateSequenceBone_PSPAnimation(
    PTRBSections&          a_rSections,
    TSkeletonSequenceBone& a_rSequenceBone,
    TINT                   a_iTranslationCount,
    TINT                   a_iQuaternionCount
)
{
	const TUINT16 uiKeyCount = a_rSequenceBone.GetKeyCount();
	if ( uiKeyCount == 0 )
		return TTRUE;
	const TUINT8 uiMinimumKeySize = a_rSequenceBone.IsTranslateAnimated() ? 6 : 4;
	const TUINT8 uiKeySize = a_rSequenceBone.m_iKeySize;
	if ( uiKeySize < uiMinimumKeySize || ( uiKeySize & 1 ) != 0 ||
	     TSIZE( uiKeyCount ) * uiKeySize > GetBytesRemaining_PSPAnimation( a_rSections, a_rSequenceBone.m_pData ) )
	{
		std::fprintf( stderr, "Invalid animation key storage: %u keys, %u bytes per key\n", TUINT( uiKeyCount ), TUINT( uiKeySize ) );
		return TFALSE;
	}

	TUINT16 uiPreviousTime = 0;
	for ( TUINT16 i = 0; i < uiKeyCount; i++ )
	{
		const TUINT16* pKey = a_rSequenceBone.GetKey( i );
		// GetKeyPair skips all keys at or before the sample time when searching forward.
		if ( ( i != 0 && pKey[ 0 ] < uiPreviousTime ) || pKey[ 1 ] >= a_iQuaternionCount ||
		     ( a_rSequenceBone.IsTranslateAnimated() && pKey[ 2 ] >= a_iTranslationCount ) )
		{
			std::fprintf( stderr, "Invalid animation key %u: time=%u previous=%u quaternion=%u/%d translation=%d/%d\n",
			    TUINT( i ), TUINT( pKey[ 0 ] ), TUINT( uiPreviousTime ), TUINT( pKey[ 1 ] ), a_iQuaternionCount,
			    a_rSequenceBone.IsTranslateAnimated() ? TINT( pKey[ 2 ] ) : -1, a_iTranslationCount );
			return TFALSE;
		}
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
    const TCHAR* a_pKeyLibrarySymbol,
    const TCHAR* a_pSequenceName,
    TINT         a_iSampleRate,
    const TCHAR* a_pOutputPath
)
{
	if ( a_pModelPath == TNULL || a_pKeyLibraryPath == TNULL || a_pKeyLibrarySymbol == TNULL ||
	     a_pSequenceName == TNULL ||
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
	auto oKeyLibrary = oKeyLibraryTRB.GetSymbols()->Find<TKeyframeLibrary::TRBHeader>(
	    oKeyLibraryTRB.GetSections(), a_pKeyLibrarySymbol
	);
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
		std::fprintf( stderr, "Invalid animation skeleton: %d bones, %d automatic bones, %d sequences\n",
		    iBoneCount, iAutoBoneCount, TINT( rSkeleton.GetSequenceCount() ) );
		return TFALSE;
	}
	for ( TINT bone = 0; bone < iBoneCount; bone++ )
	{
		TSkeletonBone* pBone = rSkeleton.GetBone( bone );
		const TUINT8 uiNameLength = pBone->GetNameLength();
		const TINT iParentBone = pBone->GetParentBone();
		if ( uiNameLength == 0 || uiNameLength > 31 || iParentBone < -1 || iParentBone >= bone )
		{
			std::fprintf( stderr, "Invalid animation bone %d: name length=%u, parent=%d\n", bone, TUINT( uiNameLength ), iParentBone );
			return TFALSE;
		}
		const TINT iLookup = rSkeleton.GetBoneID( pBone->GetName(), uiNameLength );
		if ( iLookup != bone )
		{
			std::fprintf( stderr, "Animation bone %d aliases bone %d: %.*s\n", bone, iLookup, TINT( uiNameLength ), pBone->GetName() );
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

	if ( TSIZE( iAutoBoneCount ) * sizeof( TSkeletonSequenceBone ) > GetBytesRemaining_PSPAnimation( *oModelTRB.GetSections(), rSequence.m_pSeqBones ) )
		return TFALSE;
	for ( TINT bone = 0; bone < iAutoBoneCount; bone++ )
	{
		if ( !ValidateSequenceBone_PSPAnimation( *oModelTRB.GetSections(), *rSequence.GetBone( bone ), rSkeletonHeader.m_iTKeyCount, rSkeletonHeader.m_iQKeyCount ) )
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
	TAnimQuaternion* pQuaternions = rKeyLibrary.m_pQuaternions != TNULL
	    ? rKeyLibrary.m_pQuaternions + rSkeletonHeader.m_iQBaseIndex
	    : TNULL;
	std::ostringstream oOutput;
	oOutput.imbue( std::locale::classic() );
	oOutput << std::setprecision( 9 );
	oOutput << "{\n  \"sequence\": ";
	WriteJSONString_PSPAnimation( oOutput, rSequence.GetName(), rSequence.GetNameLength() );
	oOutput << ",\n  \"sample_rate\": " << a_iSampleRate
	        << ",\n  \"loop\": " << ( rSequence.GetMode() == TSkeletonSequence::MODE_LOOPED ? "true" : "false" )
	        << ",\n  \"bones\": [\n";
	for ( TINT bone = 0; bone < iBoneCount; bone++ )
	{
		TSkeletonBone& rBone = *rSkeleton.GetBone( bone );
		oOutput << "    { \"name\": ";
		WriteJSONString_PSPAnimation( oOutput, rBone.GetName(), rBone.GetNameLength() );
		oOutput << ", \"parent\": ";
		if ( rBone.GetParentBone() < 0 ) oOutput << "null";
		else oOutput << rBone.GetParentBone();
		oOutput << " }";
		if ( bone + 1 != iBoneCount ) oOutput << ',';
		oOutput << '\n';
	}
	oOutput << "  ],\n  \"frames\": [\n";

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
				std::fprintf( stderr, "Invalid pose at animation frame %d, bone %d\n", frame, bone );
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
