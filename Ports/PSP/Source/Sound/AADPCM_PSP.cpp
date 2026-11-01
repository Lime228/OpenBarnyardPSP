#include <ToshiPCH.h>
#include "AADPCM_PSP.h"

static constexpr TINT s_aiADPCMStepTable_PSP[] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
	34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
	143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
	494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
	1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
	4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
	11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
	27086, 29794, 32767,
};

static constexpr TINT s_aiADPCMIndexTable_PSP[] = {
	-1, -1, -1, -1, 2, 4, 6, 8,
	-1, -1, -1, -1, 2, 4, 6, 8,
};

static TINT16 ReadADPCMPredictor_PSP( const TUINT8* a_pData )
{
	return TINT16( TUINT16( a_pData[ 0 ] ) | ( TUINT16( a_pData[ 1 ] ) << 8 ) );
}

TBOOL AADPCMDecoder_PSP::DecodeBlock(
    const TUINT8* a_pBlock,
    TSIZE a_uiBlockBytes,
    TINT16* a_pOutputSamples,
    TUINT a_uiSampleFrames
)
{
	if ( a_pBlock == TNULL || a_pOutputSamples == TNULL ||
	     a_uiBlockBytes != AADPCMStreamHeader_PSP::BLOCK_BYTES ||
	     a_uiSampleFrames == 0 ||
	     a_uiSampleFrames > AADPCMStreamHeader_PSP::BLOCK_SAMPLE_FRAMES )
	{
		return TFALSE;
	}

	for ( TUINT uiChannel = 0; uiChannel < AWave_PSP::OUTPUT_CHANNELS; uiChannel++ )
	{
		const TUINT8* pChannel = a_pBlock +
		    uiChannel * ( AADPCMStreamHeader_PSP::CHANNEL_HEADER_BYTES +
		                  AADPCMStreamHeader_PSP::CHANNEL_DATA_BYTES );
		TINT iPredictor = ReadADPCMPredictor_PSP( pChannel );
		TINT iStepIndex = pChannel[ 2 ];
		if ( iStepIndex > 88 || pChannel[ 3 ] != 0 )
			return TFALSE;

		a_pOutputSamples[ uiChannel ] = TINT16( iPredictor );
		const TUINT8* pCodes = pChannel + AADPCMStreamHeader_PSP::CHANNEL_HEADER_BYTES;
		for ( TUINT uiFrame = 1; uiFrame < a_uiSampleFrames; uiFrame++ )
		{
			const TUINT uiCodeIndex = uiFrame - 1;
			const TUINT8 uiPacked = pCodes[ uiCodeIndex / 2 ];
			const TUINT8 uiCode = uiCodeIndex & 1 ? uiPacked >> 4 : uiPacked & 0x0f;
			const TINT iStep = s_aiADPCMStepTable_PSP[ iStepIndex ];
			TINT iDelta = iStep >> 3;
			if ( uiCode & 1 ) iDelta += iStep >> 2;
			if ( uiCode & 2 ) iDelta += iStep >> 1;
			if ( uiCode & 4 ) iDelta += iStep;
			iPredictor += uiCode & 8 ? -iDelta : iDelta;
			if ( iPredictor < -32768 ) iPredictor = -32768;
			if ( iPredictor > 32767 ) iPredictor = 32767;

			iStepIndex += s_aiADPCMIndexTable_PSP[ uiCode ];
			if ( iStepIndex < 0 ) iStepIndex = 0;
			if ( iStepIndex > 88 ) iStepIndex = 88;
			a_pOutputSamples[ uiFrame * AWave_PSP::OUTPUT_CHANNELS + uiChannel ] =
			    TINT16( iPredictor );
		}
	}

	return TTRUE;
}
