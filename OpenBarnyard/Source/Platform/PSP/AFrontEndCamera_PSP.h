#pragma once

#include <Platform/PSP/TRenderInterface_PSP.h>

struct AFrontEndCameraFrame_PSP
{
	TFLOAT aPosition[ 3 ];
	TFLOAT aDirection[ 3 ];
	TFLOAT aUp[ 3 ];
};

class AFrontEndCamera_PSP
{
public:
	static constexpr TUINT MAX_FRAMES_PSP = 64;

public:
	AFrontEndCamera_PSP();

	TBOOL Load( const TCHAR* a_szPath = "local:frontend_camera.pcam", TFLOAT a_fFOV = 60.0f, TFLOAT a_fNear = 1.0f );
	void  Unload();
	void  Update( TFLOAT a_fDeltaTime );

	TBOOL IsLoaded() const { return m_uiFrameCount_PSP != 0; }
	TUINT GetFrameCount() const { return m_uiFrameCount_PSP; }
	TUINT GetSampleRate() const { return m_uiSampleRate_PSP; }
	const Toshi::TRenderCamera_PSP& GetRenderCamera() const { return m_RenderCamera_PSP; }
	const TCHAR* GetFailedPath() const { return m_szFailedPath_PSP; }

private:
	TBOOL ApplyFrame_PSP( TUINT a_uiFrame0, TUINT a_uiFrame1, TFLOAT a_fProgress );

private:
	AFrontEndCameraFrame_PSP    m_aFrames_PSP[ MAX_FRAMES_PSP ];
	Toshi::TRenderCamera_PSP    m_RenderCamera_PSP;
	TUINT                       m_uiFrameCount_PSP;
	TUINT                       m_uiSampleRate_PSP;
	TUINT                       m_uiFlags_PSP;
	TFLOAT                      m_fTime_PSP;
	TCHAR                       m_szFailedPath_PSP[ 64 ];
};
