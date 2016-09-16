/*
** gl_openvr.cpp
** Stereoscopic virtual reality mode for the HTC Vive headset
**
**---------------------------------------------------------------------------
** Copyright 2016 Christopher Bruns
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifdef USE_OPENVR

#include "gl_openvr.h"
#include "openvr.h"
#include <string>
#include "gl/system/gl_system.h"
#include "doomtype.h" // Printf
#include "d_player.h"
#include "g_game.h" // G_Add...
#include "p_local.h" // P_TryMove
#include "r_utility.h" // viewpitch
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/data/gl_data.h"
#include "math/cmath.h"
#include "c_cvars.h"
#include "LSMatrix.h"

// For conversion between real-world and doom units
#define VERTICAL_DOOM_UNITS_PER_METER 27.0f

EXTERN_CVAR(Int, screenblocks);

using namespace vr;

namespace s3d 
{

/* static */
const Stereo3DMode& OpenVRMode::getInstance()
{
		static OpenVRMode instance;
		if (! instance.hmdWasFound)
			return  MonoView::getInstance();
		return instance;
}

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

OpenVREyePose::OpenVREyePose(vr::EVREye eye)
	: ShiftedEyePose( 0.0f )
	, eye(eye)
	, eyeTexture(nullptr)
	, verticalDoomUnitsPerMeter(VERTICAL_DOOM_UNITS_PER_METER)
	, currentPose(nullptr)
{
}


/* virtual */
OpenVREyePose::~OpenVREyePose() 
{
	dispose();
}

static void vSMatrixFromHmdMatrix34(VSMatrix& m1, const vr::HmdMatrix34_t& m2)
{
	float tmp[16];
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 4; ++j) {
			tmp[4 * i + j] = m2.m[i][j];
		}
	}
	int i = 3;
	for (int j = 0; j < 4; ++j) {
		tmp[4 * i + j] = 0;
	}
	tmp[15] = 1;
	m1.loadMatrix(&tmp[0]);
}


/* virtual */
void OpenVREyePose::GetViewShift(FLOATTYPE yaw, FLOATTYPE outViewShift[3]) const
{
	if (currentPose == nullptr)
		return;
	const vr::TrackedDevicePose_t& hmd = *currentPose;
	if (! hmd.bDeviceIsConnected)
		return;
	if (! hmd.bPoseIsValid)
		return;
	const vr::HmdMatrix34_t& hmdPose = hmd.mDeviceToAbsoluteTracking;

	// Pitch and Roll are identical between OpenVR and Doom worlds.
	// But yaw can differ, depending on starting state, and controller movement.
	float doomYawDegrees = GLRenderer->mAngles.Yaw.Degrees;
	float openVrYawDegrees = RAD2DEG(-eulerAnglesFromMatrix(hmdPose).v[0]);
	float deltaYawDegrees = doomYawDegrees - openVrYawDegrees;
	while (deltaYawDegrees > 180)
		deltaYawDegrees -= 360;
	while (deltaYawDegrees < -180)
		deltaYawDegrees += 360;

	// Printf("delta yaw = %.0f; yaw = %.0f\n", deltaYawDegrees, openVrYawDegrees);

	// extract rotation component from hmd transform
	LSMatrix44 hmdLs(hmdPose);
	LSMatrix44 hmdRot = hmdLs.getWithoutTranslation(); // .transpose();


	/// In these eye methods, just get local inter-eye stereoscopic shift, not full position shift ///

	// compute local eye shift
	LSMatrix44 eyeShift2;
	eyeShift2.loadIdentity();
	eyeShift2 = eyeShift2 * eyeToHeadTransform; // eye to head
	eyeShift2 = eyeShift2 * hmdRot; // head to openvr

	if (eye == vr::Eye_Left) { // debugging here...
		LSVec3 eye_EyePos = LSVec3(0, 0, 0); // eye position in eye frame
		// Printf("Eye position in eye = %.2f, %.2f, %.2f\n", eye_EyePos.x, eye_EyePos.y, eye_EyePos.z);
		LSVec3 hmd_EyePos = LSMatrix44(eyeToHeadTransform) * eye_EyePos;
		// Printf("Eye position in hmd = %.3f, %.3f, %.3f\n", hmd_EyePos.x, hmd_EyePos.y, hmd_EyePos.z);
		LSVec3 openvr_EyePos = hmdLs * hmd_EyePos;
		// Printf("Eye position in office = %.3f, %.3f, %.3f\n", openvr_EyePos.x, openvr_EyePos.y, openvr_EyePos.z);
		LSVec3 hmd_OtherEyePos = LSMatrix44(otherEyeToHeadTransform) * eye_EyePos;
		// Printf("Eye position in office = %.3f, %.3f, %.3f\n", hmd_OtherEyePos.x, hmd_OtherEyePos.y, hmd_OtherEyePos.z);
		LSVec3 openvr_OtherEyePos = hmdLs * hmd_OtherEyePos;
		// Printf("Eye position in office = %.3f, %.3f, %.3f\n", openvr_OtherEyePos.x, openvr_OtherEyePos.y, openvr_OtherEyePos.z);
		LSVec3 openvr_EyeOffset = 0.5 * (openvr_EyePos - openvr_OtherEyePos);
		Printf("Relative eye position in office = %.3f, %.3f, %.3f\n", openvr_EyeOffset.x, openvr_EyeOffset.y, openvr_EyeOffset.z);
	}

	// OpenVR knows exactly where the floor is, so...
	// ...set viewz to exact absolute height above the floor
	// float openvrHeightMeters = hmdLs[1][3] + eyeShift2[1][3];
	// float doomFloorZ = FIXED2FLOAT(viewsector->floorplane.ZatPoint(viewx, viewy));
	// float doomViewZDoomUnits = verticalDoomUnitsPerMeter * openvrHeightMeters + doomFloorZ;
	// float deltaViewZ = doomViewZDoomUnits - FIXED2FLOAT(viewz);

	float horizontalDoomUnitsPerMeter = verticalDoomUnitsPerMeter * glset.pixelstretch;

	// output per-eye shifts, for stereoscopic rendering by caller of this method
	outViewShift[0] = eyeShift2[0][3] * horizontalDoomUnitsPerMeter;
	outViewShift[1] = eyeShift2[2][3] * horizontalDoomUnitsPerMeter; // "viewy" is horizontal direction in doom
	// outViewShift[2] = deltaViewZ; // Set absolute height above; "viewz" is height in doom
	outViewShift[2] = eyeShift2[1][3] * verticalDoomUnitsPerMeter;

	// Printf("viewshift = %2.1f, %2.1f\n", outViewShift[0], outViewShift[1]);
}

/* virtual */
VSMatrix OpenVREyePose::GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio) const
{
	// Ignore those arguments and get the projection from the SDK
	VSMatrix vs1 = ShiftedEyePose::GetProjection(fov, aspectRatio, fovRatio);
	return projectionMatrix;
}

void OpenVREyePose::initialize(vr::IVRSystem& vrsystem)
{
	float zNear = 5.0;
	float zFar = 65536.0;
	vr::HmdMatrix44_t projection = vrsystem.GetProjectionMatrix(
			eye, zNear, zFar, vr::API_OpenGL);
	vr::HmdMatrix44_t proj_transpose;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			proj_transpose.m[i][j] = projection.m[j][i];
		}
	}
	projectionMatrix.loadIdentity();
	projectionMatrix.multMatrix(&proj_transpose.m[0][0]);

	vr::HmdMatrix34_t eyeToHead = vrsystem.GetEyeToHeadTransform(eye);
	vSMatrixFromHmdMatrix34(eyeToHeadTransform, eyeToHead);
	vr::HmdMatrix34_t otherEyeToHead = vrsystem.GetEyeToHeadTransform(eye == Eye_Left ? Eye_Right : Eye_Left);
	vSMatrixFromHmdMatrix34(otherEyeToHeadTransform, otherEyeToHead);

	if (eyeTexture == nullptr)
		eyeTexture = new vr::Texture_t();
	eyeTexture->handle = nullptr; // TODO: populate this at resolve time
	eyeTexture->eType = vr::API_OpenGL;
	eyeTexture->eColorSpace = vr::ColorSpace_Linear;
}

void OpenVREyePose::dispose()
{
	if (eyeTexture) {
		delete eyeTexture;
		eyeTexture = nullptr;
	}
}

bool OpenVREyePose::submitFrame() const
{
	if (eyeTexture == nullptr)
		return false;
	if (vr::VRCompositor() == nullptr)
		return false;
	eyeTexture->handle = (void *)GLRenderer->mBuffers->GetEyeTextureGLHandle((int)eye);
	vr::VRCompositor()->Submit(eye, eyeTexture);
	return true;
}


OpenVRMode::OpenVRMode() 
	: vrSystem(nullptr)
	, leftEyeView(vr::Eye_Left)
	, rightEyeView(vr::Eye_Right)
	, hmdWasFound(false)
	, sceneWidth(0), sceneHeight(0)
{
	eye_ptrs.Push(&leftEyeView); // default behavior to Mono non-stereo rendering

	EVRInitError eError;
	if (VR_IsHmdPresent())
	{
		vrSystem = VR_Init(&eError, VRApplication_Scene);
		if (eError != vr::VRInitError_None) {
			std::string errMsg = VR_GetVRInitErrorAsEnglishDescription(eError);
			vrSystem = nullptr;
			return;
			// TODO: report error
		}
		vrSystem->GetRecommendedRenderTargetSize(&sceneWidth, &sceneHeight);

		// OK
		leftEyeView.initialize(*vrSystem);
		rightEyeView.initialize(*vrSystem);

		if (!vr::VRCompositor())
			return;

		eye_ptrs.Push(&rightEyeView); // NOW we render to two eyes
		hmdWasFound = true;
	}
}

/* virtual */
// AdjustViewports() is called from within FLGRenderer::SetOutputViewport(...)
void OpenVRMode::AdjustViewports() const
{
	// Draw the 3D scene into the entire framebuffer
	GLRenderer->mSceneViewport.width = sceneWidth;
	GLRenderer->mSceneViewport.height = sceneHeight;
	GLRenderer->mSceneViewport.left = 0;
	GLRenderer->mSceneViewport.top = 0;

	GLRenderer->mScreenViewport.width = sceneWidth;
	GLRenderer->mScreenViewport.height = sceneHeight;
}

/* virtual */
void OpenVRMode::Present() const {
	// TODO: For performance, don't render to the desktop screen here
	const bool renderToDesktop = true;
	if (renderToDesktop) {
		GLRenderer->mBuffers->BindOutputFB();
		GLRenderer->ClearBorders();

		// Compute screen regions to use for left and right eye views
		int leftWidth = GLRenderer->mOutputLetterbox.width / 2;
		int rightWidth = GLRenderer->mOutputLetterbox.width - leftWidth;
		GL_IRECT leftHalfScreen = GLRenderer->mOutputLetterbox;
		leftHalfScreen.width = leftWidth;
		GL_IRECT rightHalfScreen = GLRenderer->mOutputLetterbox;
		rightHalfScreen.width = rightWidth;
		rightHalfScreen.left += leftWidth;

		GLRenderer->mBuffers->BindEyeTexture(0, 0);
		GLRenderer->DrawPresentTexture(leftHalfScreen, true);
		GLRenderer->mBuffers->BindEyeTexture(1, 0);
		GLRenderer->DrawPresentTexture(rightHalfScreen, true);
	}

	const bool renderToHmd = true;
	if (renderToHmd) 
	{
		leftEyeView.submitFrame();
		rightEyeView.submitFrame();
	}
}

static int mAngleFromRadians(double radians) 
{
	double m = std::round(65535.0 * radians / (2.0 * M_PI));
	return int(m);
}

void OpenVRMode::updateHmdPose(
	double hmdYawRadians, 
	double hmdPitchRadians, 
	double hmdRollRadians) const 
{
	double hmdyaw = hmdYawRadians;
	double hmdpitch = hmdPitchRadians;
	double hmdroll = hmdRollRadians;

	// Set HMD angle game state parameters for NEXT frame
	static double previousYaw = 0;
	static bool havePreviousYaw = false;
	if (!havePreviousYaw) {
		previousYaw = hmdyaw;
		havePreviousYaw = true;
	}
	double dYaw = hmdyaw - previousYaw;
	G_AddViewAngle(mAngleFromRadians(-dYaw));
	previousYaw = hmdyaw;

	/* */
	// Pitch
	double hmdPitchInDoom = -atan(tan(hmdpitch) / glset.pixelstretch);
	double viewPitchInDoom = GLRenderer->mAngles.Pitch.Radians();
	double dPitch = hmdPitchInDoom - viewPitchInDoom;
	G_AddViewPitch(mAngleFromRadians(-dPitch));
	/* */

	// Roll can be local, because it doesn't affect gameplay.
	GLRenderer->mAngles.Roll = RAD2DEG(-hmdroll);

	// Late-schedule update to renderer angles directly, too
	bool doLateScheduledRotationTracking = true;
	if (doLateScheduledRotationTracking) {
		GLRenderer->mAngles.Pitch = RAD2DEG(-hmdpitch);
		GLRenderer->mAngles.Yaw += RAD2DEG(dYaw); // "plus" is the correct direction
	}
	/* */
}

/* virtual */
void OpenVRMode::SetUp() const
{
	super::SetUp();

	cachedScreenBlocks = screenblocks;
	screenblocks = 12; // always be full-screen during 3D scene render

	if (vr::VRCompositor() == nullptr)
		return;

	static vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
	vr::VRCompositor()->WaitGetPoses(
		poses, vr::k_unMaxTrackedDeviceCount, // current pose
		nullptr, 0 // future pose?
	);

	TrackedDevicePose_t& hmdPose0 = poses[vr::k_unTrackedDeviceIndex_Hmd];

	if (hmdPose0.bPoseIsValid) {
		const vr::HmdMatrix34_t& hmdPose = hmdPose0.mDeviceToAbsoluteTracking;
		HmdVector3d_t eulerAngles = eulerAnglesFromMatrix(hmdPose);
		// Printf("%.1f %.1f %.1f\n", eulerAngles.v[0], eulerAngles.v[1], eulerAngles.v[2]);
		updateHmdPose(eulerAngles.v[0], eulerAngles.v[1], eulerAngles.v[2]);
		leftEyeView.setCurrentHmdPose(&hmdPose0);
		rightEyeView.setCurrentHmdPose(&hmdPose0);
		// TODO: position tracking
	}
}

/* virtual */
void OpenVRMode::TearDown() const
{
	screenblocks = cachedScreenBlocks;
	super::TearDown();
}

/* virtual */
OpenVRMode::~OpenVRMode() 
{
	if (vrSystem != nullptr) {
		VR_Shutdown();
		vrSystem = nullptr;
		leftEyeView.dispose();
		rightEyeView.dispose();
	}
}

} /* namespace s3d */

#endif

