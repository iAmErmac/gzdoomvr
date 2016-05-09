#include "gl/stereo3d/gl_openvr.h"
#include "openvr.h"
#include <string>
#include "doomtype.h" // Printf
#include "g_game.h" // G_Add...
#include "r_utility.h" // viewpitch
#include "gl/renderer/gl_renderer.h"

using namespace vr;

namespace s3d 
{

static HmdVector3d_t eulerAnglesFromQuat(HmdQuaternion_t quat) {
	double q0 = quat.w;
	// permute axes to make "Y" up/yaw
	double q2 = quat.x;
	double q3 = quat.y;
	double q1 = quat.z;

	// http://stackoverflow.com/questions/18433801/converting-a-3x3-matrix-to-euler-tait-bryan-angles-pitch-yaw-roll
	double roll = atan2(2 * (q0*q1 + q2*q3), 1 - 2 * (q1*q1 + q2*q2));
	double pitch = asin(2 * (q0*q2 - q3*q1));
	double yaw = atan2(2 * (q0*q3 + q1*q2), 1 - 2 * (q2*q2 + q3*q3));

	return HmdVector3d_t{ yaw, pitch, roll };
}

static HmdQuaternion_t quatFromMatrix(HmdMatrix34_t matrix) {
	HmdQuaternion_t q;
	typedef float f34[3][4];
	f34& a = matrix.m;
	// http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
	float trace = a[0][0] + a[1][1] + a[2][2]; // I removed + 1.0f; see discussion with Ethan
	if (trace > 0) {// I changed M_EPSILON to 0
		float s = 0.5f / sqrtf(trace + 1.0f);
		q.w = 0.25f / s;
		q.x = (a[2][1] - a[1][2]) * s;
		q.y = (a[0][2] - a[2][0]) * s;
		q.z = (a[1][0] - a[0][1]) * s;
	}
	else {
		if (a[0][0] > a[1][1] && a[0][0] > a[2][2]) {
			float s = 2.0f * sqrtf(1.0f + a[0][0] - a[1][1] - a[2][2]);
			q.w = (a[2][1] - a[1][2]) / s;
			q.x = 0.25f * s;
			q.y = (a[0][1] + a[1][0]) / s;
			q.z = (a[0][2] + a[2][0]) / s;
		}
		else if (a[1][1] > a[2][2]) {
			float s = 2.0f * sqrtf(1.0f + a[1][1] - a[0][0] - a[2][2]);
			q.w = (a[0][2] - a[2][0]) / s;
			q.x = (a[0][1] + a[1][0]) / s;
			q.y = 0.25f * s;
			q.z = (a[1][2] + a[2][1]) / s;
		}
		else {
			float s = 2.0f * sqrtf(1.0f + a[2][2] - a[0][0] - a[1][1]);
			q.w = (a[1][0] - a[0][1]) / s;
			q.x = (a[0][2] + a[2][0]) / s;
			q.y = (a[1][2] + a[2][1]) / s;
			q.z = 0.25f * s;
		}
	}

	return q;
}

static HmdVector3d_t eulerAnglesFromMatrix(HmdMatrix34_t mat) {
	return eulerAnglesFromQuat(quatFromMatrix(mat));
}


/* static */
const OpenVRMode& OpenVRMode::getInstance(FLOATTYPE ipd)
{
	static OpenVRMode instance(ipd);
	return instance;
}

OpenVRMode::OpenVRMode(FLOATTYPE ipd) 
	: ivrSystem(nullptr)
	, leftEyeView(ipd)
	, rightEyeView(ipd)
{
	eye_ptrs.Push(&leftEyeView); // default behavior to Mono non-stereo rendering

	EVRInitError eError;
	if (VR_IsHmdPresent())
	{
		ivrSystem = VR_Init(&eError, VRApplication_Scene);
		std::string errMsg = VR_GetVRInitErrorAsEnglishDescription(eError);
		HmdMatrix34_t mat1 = ivrSystem->GetEyeToHeadTransform(Eye_Left);
	}
}

void OpenVRMode::updateDoomViewDirection() const
{
	if (ivrSystem == nullptr)
		return;
	// Compute how far in the future to predict HMD pose
	float secondsSinceLastVsync = 0;
	ivrSystem->GetTimeSinceLastVsync(&secondsSinceLastVsync, nullptr);
	float displayFrequency = ivrSystem->GetFloatTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, Prop_DisplayFrequency_Float);
	float frameDuration = 1.0f / displayFrequency;
	float vsyncToPhotons = ivrSystem->GetFloatTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, Prop_SecondsFromVsyncToPhotons_Float);
	float predictedSecondsFromNow = frameDuration - secondsSinceLastVsync + vsyncToPhotons;

	// Compute HMD pose, in terms of yaw, pitch, and roll
	TrackedDevicePose_t trackedDevicePoses[k_unMaxTrackedDeviceCount];
	ivrSystem->GetDeviceToAbsoluteTrackingPose(
		TrackingUniverseStanding,
		predictedSecondsFromNow,
		trackedDevicePoses,
		k_unMaxTrackedDeviceCount
	);
	TrackedDevicePose_t& hmdPose = trackedDevicePoses[k_unTrackedDeviceIndex_Hmd];
	HmdVector3d_t eulerAngles = eulerAnglesFromMatrix(hmdPose.mDeviceToAbsoluteTracking);
	// Printf("%.1f %.1f %.1f\n", eulerAngles.v[0], eulerAngles.v[1], eulerAngles.v[2]);

	double hmdyaw = eulerAngles.v[0];
	double hmdpitch = eulerAngles.v[1];
	double hmdroll = -eulerAngles.v[2];

	// Set HMD angle game state parameters for NEXT frame
	static double previousYaw = 0;
	static bool havePreviousYaw = false;
	if (!havePreviousYaw) {
		previousYaw = hmdyaw;
		havePreviousYaw = true;
	}
	double dYaw = hmdyaw - previousYaw;
	G_AddViewAngle((int)(-32768.0*dYaw / 3.14159)); // determined empirically
	previousYaw = hmdyaw;

	// Pitch
	int pitch = (int)(-32768 / 3.14159*hmdpitch);
	int dPitch = (pitch - viewpitch / 65536); // empirical
	G_AddViewPitch(-dPitch);

	// Roll can be local, because it doesn't affect gameplay.
	GLRenderer->mAngles.Roll = (float)(hmdroll * 180.0 / 3.14159);

	// Late-schedule update to renderer angles directly, too
	GLRenderer->mAngles.Pitch = -hmdpitch * 180.0 / 3.14159;
	GLRenderer->mAngles.Yaw += dYaw * 180.0 / 3.14159; // TODO: Is this correct? Maybe minus?
}

/* virtual */
void OpenVRMode::SetUp() const
{
	updateDoomViewDirection();	
}

/* virtual */
OpenVRMode::~OpenVRMode() 
{
	if (ivrSystem != nullptr) {
		VR_Shutdown();
		ivrSystem = nullptr;
	}
}

} /* namespace s3d */
