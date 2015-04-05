#include "gl/scene/gl_oculustracker.h"
// #include "doomtype.h" // Printf
#include <string>

OculusTracker::OculusTracker() 
	: pitch(0)
	, roll(0)
	, yaw(0)
	, deviceId(1)
{
#ifdef HAVE_OCULUS_API
	originPosition = OVR::Vector3f(0,0,0);
	position = OVR::Vector3f(0,0,0);
	ovr_Initialize();// OVR::System::Init();
	hmd = ovrHmd_Create(0);
	if (hmd) {
		ovrHmd_GetDesc(hmd, &hmdDesc);
		setLowPersistence(true);
		ovrHmd_StartSensor(hmd,
			ovrSensorCap_Orientation | ovrSensorCap_YawCorrection | ovrSensorCap_Position, // supported
			ovrSensorCap_Orientation); // required
		if ( hmdDesc.Type == ovrHmd_DK2 ) {
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

void OculusTracker::report() const {
}

void OculusTracker::update() {
#ifdef HAVE_OCULUS_API
	const float pixelRatio = 1.20;

	const bool usePredicted = true;
	double predictionTime = 0.00;
	if (usePredicted)
		predictionTime = 0.030; // 20 milliseconds - TODO setting to zero does not resolve shake

	ovrSensorState sensorState = ovrHmd_GetSensorState(hmd, predictionTime);

	// Rotation tracking
	if (sensorState.StatusFlags & (ovrStatus_OrientationTracked) ) {
		// Predicted is extremely unstable; at least in my initial experiments CMB
		ovrPosef pose = sensorState.Recorded.Pose; // = sensorState.Predicted.Pose;
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
	}

	// Neck-model-based position tracking
	if (sensorState.StatusFlags & (ovrStatus_PositionConnected)) 
	{
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

		ovrPosef pose = sensorState.Recorded.Pose; // = sensorState.Predicted.Pose;
		position = pose.Position;
		// TODO - should we apply pixelRatio?
	}

#endif
}
