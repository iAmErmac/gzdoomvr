#define USE_WINDOWS_DWORD
#include <Windows.h> // error C2371: 'DWORD' : redefinition; different basic types
#include "gl/scene/gl_oculustracker.h"
#include "gl/scene/gl_oculustexture.h"
#include <string>
#include "gl/system/gl_cvars.h"
#include "OVR_CAPI.h"
#include "OVR_CAPI_GL.h"

extern "C" {
void ovrhmd_EnableHSWDisplaySDKRender(ovrHmd hmd, ovrBool enabled);
}

#ifdef _WIN32
extern HWND Window;
#endif

CVAR(Bool, vr_sdkwarp, false, CVAR_GLOBALCONFIG)

// TODO - static globals are uncool
static ovrPosef eyePoses[2];
static ovrGLTexture ovrEyeTexture[2];
static ovrEyeRenderDesc eyeRenderDesc[2];
static OVR::Matrix4f projectionMatrix;
static ovrVector3f hmdToEyeViewOffset;

OculusTracker::OculusTracker() 
	: pitch(0)
	, roll(0)
	, yaw(0)
	, deviceId(1)
	, frameIndex(0)
	, trackingConfigured(false)
	, renderingConfigured(false)
	, ovrInitialized(false)
	, hmd(NULL)
	, texWidth(1920)
	, texHeight(1080)
	, textureId(0)
{
#ifdef HAVE_OCULUS_API
	originPosition = OVR::Vector3f(0,0,0);
	position = OVR::Vector3f(0,0,0);
	// checkInitialized(); // static initialization order crash CMB
#endif
}

float * OculusTracker::getProjection(int eye) {
	projectionMatrix = ovrMatrix4f_Projection(hmd->DefaultEyeFov[eye], 5.0, 655536, 1);
	return projectionMatrix.M[0];
}

void OculusTracker::checkHealthAndSafety() {
#ifdef HAVE_OCULUS_API
	static bool dismissed = false;
	if (dismissed) return;
	if (frameIndex > 100) {
		ovrHmd_DismissHSWDisplay(hmd);
		dismissed = true;
	}
	return;
	// Health and Safety Warning display state.
	ovrHSWDisplayState hswDisplayState;
	ovrHmd_GetHSWDisplayState(hmd, &hswDisplayState);
	if (hswDisplayState.Displayed)
	{
		// Dismiss the warning if the user pressed the appropriate key or if the user
		// is tapping the side of the HMD.
		// If the user has requested to dismiss the warning via keyboard or controller input...
		if (frameIndex > 5)
			ovrHmd_DismissHSWDisplay(hmd);
		else
		{
			// Detect a moderate tap on the side of the HMD.
			ovrTrackingState ts = ovrHmd_GetTrackingState(hmd, ovr_GetTimeInSeconds());
			if (ts.StatusFlags & ovrStatus_OrientationTracked)
			{
				const OVR::Vector3f v(ts.RawSensorData.Accelerometer.x,
				ts.RawSensorData.Accelerometer.y,
				ts.RawSensorData.Accelerometer.z);
				// Arbitrary value and representing moderate tap on the side of the DK2 Rift.
				if (v.LengthSq() > 250.f)
					ovrHmd_DismissHSWDisplay(hmd);
			}
		}
	}
#endif
}

void OculusTracker::checkInitialized() {
#ifdef HAVE_OCULUS_API
	if (! ovrInitialized) {
		ovr_Initialize();
		ovrInitialized = true;
	}
#endif
}

void OculusTracker::checkConfiguration() {
#ifdef HAVE_OCULUS_API
	checkInitialized();
	if ( hmd == NULL ) {
		hmd = ovrHmd_Create(0);
		if (hmd == NULL) {
			hmd = ovrHmd_CreateDebug(ovrHmd_DK1);
			ovrHmd_ResetFrameTiming(hmd, 0);
		}
		// Cannot use Oculus Rift SDK for warping, if we couldn't even initialize an HMD.
		if ( (hmd == NULL) && (vr_sdkwarp) ) {
			vr_sdkwarp = false;
		}
	}
	if ( hmd && (! trackingConfigured) ) {
		ovrHmd_ConfigureTracking(hmd,
			ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position, // supported
			0); // required
		if ( hmd->Type == ovrHmd_DK2 ) {
			deviceId = 2;
		}
		else {
			deviceId = 1;
		}
		trackingConfigured = true;
	}
	if ( hmd && (! renderingConfigured) && vr_sdkwarp ) {
		ovrGLConfig cfg;
		memset(&cfg, 0, sizeof cfg);
		cfg.OGL.Header.API = ovrRenderAPI_OpenGL;
		cfg.OGL.Header.BackBufferSize.w = texWidth;
		cfg.OGL.Header.BackBufferSize.h = texHeight;
		cfg.OGL.Header.Multisample = 1;
#ifdef _WIN32
		cfg.OGL.Window = GetActiveWindow();
		cfg.OGL.DC = wglGetCurrentDC();
#endif
		if (hmd->HmdCaps & ovrHmdCap_ExtendDesktop) {
			// extended desktop
		}
		else {
			// Direct to rift mode
#ifdef WIN32
			ovrHmd_AttachToWindow(hmd, cfg.OGL.Window, 0, 0);
#elif defined(OVR_OS_LINUX)
			ovrHmd_AttachToWindow(hmd, (void*)glXGetCurrentDrawable(), 0, 0);
#endif
		}
		ovrBool result = ovrHmd_ConfigureRendering(hmd, &cfg.Config
			, ovrDistortionCap_TimeWarp
#if OCULUS_SDK_VERSION <= 0440
			  | ovrDistortionCap_Chromatic
#endif
			  | ovrDistortionCap_Overdrive
			, hmd->DefaultEyeFov
			, eyeRenderDesc); // output
		if (result)
			renderingConfigured = true;
#if OCULUS_SDK_VERSION <= 0440
		ovrhmd_EnableHSWDisplaySDKRender(hmd, false); // for debugging, or avoiding crash in SDK 0.4.4
#endif
	}
#endif
}

void OculusTracker::configureTexture(OculusTexture* oculusTexture) 
{
	texWidth = oculusTexture->getWidth();
	texHeight = oculusTexture->getHeight();
	textureId = oculusTexture->getHandle();
}

float OculusTracker::getPositionX()
{
#ifdef HAVE_OCULUS_API
	return position.x - originPosition.x;
#else
	return 0;
#endif
}

float OculusTracker::getPositionY()
{
#ifdef HAVE_OCULUS_API
	return position.y - originPosition.y;
#else
	return 0;
#endif
}

float OculusTracker::getPositionZ()
{
#ifdef HAVE_OCULUS_API
	return position.z - originPosition.z;
#else
	return 0;
#endif
}

void OculusTracker::resetPosition()
{
#ifdef HAVE_OCULUS_API
	originPosition = position;
#endif
}

void OculusTracker::setLowPersistence(bool setLow) {
#ifdef HAVE_OCULUS_API
	if (hmd) {
		int hmdCaps = ovrHmd_GetEnabledCaps(hmd);
		if (setLow)
			ovrHmd_SetEnabledCaps(hmd, hmdCaps | ovrHmdCap_LowPersistence);
		else
			ovrHmd_SetEnabledCaps(hmd, hmdCaps & ~ovrHmdCap_LowPersistence);
	}
#endif
}

OculusTracker::~OculusTracker() {
#ifdef HAVE_OCULUS_API
	ovrHmd_Destroy(hmd);
	ovr_Shutdown();
#endif
}

bool OculusTracker::isGood() const {
#ifdef HAVE_OCULUS_API
	if (hmd == NULL) return false;
	return true;
#else
	return false;
#endif
}

void OculusTracker::beginFrame() {
#ifdef HAVE_OCULUS_API
	checkConfiguration();
	frameIndex ++;
	if (hmd) {
		if (vr_sdkwarp) {
			// ovrHmd_BeginFrameTiming(hmd, frameIndex);
			ovrHmd_BeginFrame(hmd, frameIndex);
		}
		else {
			ovrHmd_BeginFrameTiming(hmd, frameIndex);
		}
	}
#endif
}


void OculusTracker::endFrame() {
#ifdef HAVE_OCULUS_API
	if (hmd) {
		if (vr_sdkwarp) {
			ovrHmd_EndFrame(hmd, eyePoses, (ovrTexture*)ovrEyeTexture);
		}
		else {
			ovrHmd_EndFrameTiming(hmd);
		}
	}
#endif
}

void OculusTracker::update() {
#ifdef HAVE_OCULUS_API
	checkConfiguration();
	const float pixelRatio = 1.20;

	ovrTrackingState sensorState;
	if ( vr_sdkwarp ) {
		ovrSizei renderTargetSize = {texWidth, texHeight};
		ovrRecti leftViewport = {0, 0, texWidth/2, texHeight};
		// ovrRecti rightViewport = {0, 0, texWidth/2, texHeight};
		ovrRecti rightViewport = {(texWidth+1)/2, 0, texWidth/2, texHeight};

		ovrEyeTexture[0].OGL.Header.API = ovrRenderAPI_OpenGL;
		ovrEyeTexture[0].OGL.Header.TextureSize = renderTargetSize;
		ovrEyeTexture[0].OGL.Header.RenderViewport = leftViewport;
		ovrEyeTexture[0].OGL.TexId = textureId;

		ovrEyeTexture[1] = ovrEyeTexture[0];
		ovrEyeTexture[1].OGL.Header.RenderViewport = rightViewport;

		ovrHmd_GetEyePoses(hmd, frameIndex, &hmdToEyeViewOffset, eyePoses, &sensorState);
		// eyePoses[0] = ovrHmd_GetHmdPosePerEye(hmd, ovrEye_Left);
		// eyePoses[1] = ovrHmd_GetHmdPosePerEye(hmd, ovrEye_Right);

		// TODO - projection matrix
	}
	else {
		ovrFrameTiming frameTiming = ovrHmd_GetFrameTiming(hmd, frameIndex);
		sensorState = ovrHmd_GetTrackingState(hmd, frameTiming.ScanoutMidpointSeconds);
	}

	// Rotation tracking
	if (sensorState.StatusFlags & (ovrStatus_OrientationTracked) ) {
		ovrPosef pose;
		if (vr_sdkwarp) {
			pose = eyePoses[0];
		}
		else {
			pose = sensorState.HeadPose.ThePose; 
		}
		quaternion = pose.Orientation;
		OVR::Vector3<float> axis;
		float angle;
		quaternion.GetAxisAngle(&axis, &angle);
		axis.y *= 1.0f/pixelRatio; // 1) squish direction in Y
		axis.Normalize();
		float angleFactor = 1.0f + sqrt(1.0f - axis.y*axis.y) * (pixelRatio - 1.0f);
		angle = atan2(angleFactor * sin(angle), cos(angle)); // 2) Expand angle in Y
		OVR::Quatf squishedQuat(axis, angle);
		squishedQuat.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&yaw, &pitch, &roll);

		// Always use position tracking.
		// Uses neck model, if ovrStatus_PositionConnected is false
		// (unlike in SDK 0.3.2, where ovrStatus_PositionConnected is true for DK1 in neck-model-only)
	// }

	// Neck-model-based position tracking
	// if (sensorState.StatusFlags & (ovrStatus_PositionConnected)) 
	// {
		// Sanity check neck model, which might be nonsense, especially on DK1
		float neckEye[2] = {0, 0};
		ovrHmd_GetFloatArray(hmd, OVR_KEY_NECK_TO_EYE_DISTANCE, neckEye, 2);
		bool bChanged = false;
		if ((neckEye[0] < 0.05) || (neckEye[0] > 0.50)) {
			neckEye[0] = OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL;
			bChanged = true;
		}
		if ((neckEye[1] < 0.05) || (neckEye[1] > 0.50)) {
			neckEye[1] = OVR_DEFAULT_NECK_TO_EYE_VERTICAL;
			bChanged = true;
		}
		if (bChanged) {
			ovrHmd_SetFloatArray(hmd, OVR_KEY_NECK_TO_EYE_DISTANCE, neckEye, 2);
		}

		position = pose.Position;

		// Adjust position by yaw angle, to convert from real-life frame to headset frame
		float cy = std::cos(yaw);
		float sy = std::sin(yaw);
		float new_x = position.x * cy - position.z * sy; // TODO
		float new_z = position.z * cy + position.x * sy; // TODO
		position.x = new_x;
		position.z = new_z;

		// Q: Should we apply pixelRatio? A: No, PositionTracker takes care of that
	}

#endif
}

OculusTracker * sharedOculusTracker = new OculusTracker();
