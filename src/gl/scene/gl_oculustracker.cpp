#include "gl/scene/gl_oculustracker.h"
// #include "doomtype.h" // Printf
#include <string>
#include "OVR_CAPI.h"

OculusTracker::OculusTracker() 
	: pitch(0)
	, roll(0)
	, yaw(0)
	, deviceId(1)
	, frameIndex(0)
{
#ifdef HAVE_OCULUS_API
	originPosition = OVR::Vector3f(0,0,0);
	position = OVR::Vector3f(0,0,0);
	ovr_Initialize(); // OVR::System::Init();
	hmd = ovrHmd_Create(0);
	if (hmd) {
		// ovrHmd_GetDesc(hmd, &hmdDesc);
		setLowPersistence(true);
		ovrHmd_ConfigureTracking(hmd,
			ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position, // supported
			ovrTrackingCap_Orientation); // required
		if ( hmd->Type == ovrHmd_DK2 ) {
			deviceId = 2;
		}
		else {
			deviceId = 1;
		}
	}

#endif
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
	frameIndex ++;
	if (hmd)
		ovrHmd_BeginFrameTiming(hmd, frameIndex);
#endif
}

void OculusTracker::endFrame() {
#ifdef HAVE_OCULUS_API
	if (hmd)
		ovrHmd_EndFrameTiming(hmd);
#endif
}

void OculusTracker::update() {
#ifdef HAVE_OCULUS_API
	const float pixelRatio = 1.20;

	ovrFrameTiming frameTiming = ovrHmd_GetFrameTiming(hmd, frameIndex);
	ovrTrackingState sensorState = ovrHmd_GetTrackingState(hmd, frameTiming.ScanoutMidpointSeconds);

	// Rotation tracking
	if (sensorState.StatusFlags & (ovrStatus_OrientationTracked) ) {
		ovrPosef pose = sensorState.HeadPose.ThePose; 
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

		// ovrPosef pose = sensorState.HeadPose.ThePose;
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
