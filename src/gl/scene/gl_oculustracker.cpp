#define USE_WINDOWS_DWORD
#include <Windows.h> // error C2371: 'DWORD' : redefinition; different basic types
#include "gl/scene/gl_oculustracker.h"
#include "gl/scene/gl_oculustexture.h"
#include <string>
#include "gl/system/gl_cvars.h"

extern "C" {
#include "OVR_CAPI.h"
#include "OVR_CAPI_GL.h"
}

#ifdef _WIN32
extern HWND Window;
#endif

CVAR(Bool, vr_sdkwarp, false, CVAR_GLOBALCONFIG)

OculusTracker::OculusTracker() 
	: ovrInitialized(false)
	, hmd(NULL)
{
	init();
}

ovrResult OculusTracker::init() {
	if (ovrInitialized) 
		return ovrSuccess;
	ovrResult result = ovr_Initialize(nullptr);
	if OVR_FAILURE(result)
		return result;
	ovrGraphicsLuid luid;
	ovr_Create(&hmd, &luid);
	ovr_ConfigureTracking(hmd,
			ovrTrackingCap_Orientation | // supported capabilities
			ovrTrackingCap_MagYawCorrection |
			ovrTrackingCap_Position, 
			0); // required capabilities
	ovrInitialized = true;
	return result;
}

void OculusTracker::destroy() {
	if (hmd) {
		ovr_Destroy(hmd);
		hmd = nullptr;
	}
	ovr_Shutdown();
}

void OculusTracker::recenter_pose() {
	ovr_RecenterPose(hmd);
}

OculusTracker::~OculusTracker() {
#ifdef HAVE_OCULUS_API
	destroy();
#endif
}
