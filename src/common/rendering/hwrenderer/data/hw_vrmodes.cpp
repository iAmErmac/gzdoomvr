// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2015 Christopher Bruns
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_stereo_leftright.cpp
** Offsets for left and right eye views
**
*/

#include "vectors.h" // RAD2DEG
#include "hw_cvars.h"
#include "hw_vrmodes.h"
#include "v_video.h"
#include "gl/stereo3d/gl_openvr.h"
#include "version.h"
#include "i_interface.h"

// Set up 3D-specific console variables:
CVAR(Int, vr_mode, 10, CVAR_GLOBALCONFIG  | CVAR_ARCHIVE)
// switch left and right eye views
CVAR(Bool, vr_swap_eyes, false, CVAR_GLOBALCONFIG   | CVAR_ARCHIVE)
// intraocular distance in meters
CVAR(Float, vr_ipd, 0.062f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // METERS
// distance between viewer and the display screen
CVAR(Float, vr_screendist, 0.80f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // METERS

CVAR(Float, vr_kill_momentum, 0.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// default conversion between (vertical) DOOM units and meters
CVAR(Float, vr_vunits_per_meter, 32.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // METERS
CVAR(Float, vr_floor_offset, 0.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // METERS
CVAR(Bool, openvr_rightHanded, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, openvr_moveFollowsOffHand, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_teleport, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, openvr_drawControllers, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, openvr_weaponRotate, -40, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, openvr_weaponScale, 0.3f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CVAR(Bool, vr_enable_haptics, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_pickup_haptic_level, 0.25f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_quake_haptic_level, 0.8f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_missile_haptic_level, 0.6f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

//HUD control
CVAR(Float, vr_hud_scale, 0.35f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_hud_stereo, 0.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_hud_distance, 0.70f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_hud_rotate, 5.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_hud_fixed_pitch, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_hud_fixed_roll, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

//AutoMap control
CVAR(Bool, vr_automap_use_hud, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_automap_scale, 0.4f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_automap_stereo, 0.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_automap_distance, 1.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_automap_rotate, 13.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_automap_fixed_pitch, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_automap_fixed_roll, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

#define isqrt2 0.7071067812f

VRMode::VRMode(int eyeCount, float horizontalViewportScale,
	float verticalViewportScale, float weaponProjectionScale, VREyeInfo eyes[2])
{
	mEyeCount = eyeCount;
	mHorizontalViewportScale = horizontalViewportScale;
	mVerticalViewportScale = verticalViewportScale;
	mWeaponProjectionScale = weaponProjectionScale;
	mEyes[0] = &eyes[0];
	mEyes[1] = &eyes[1];

}

const VRMode *VRMode::GetVRMode(bool toscreen)
{
	static VREyeInfo vrmi_mono_eyes[2] = { VREyeInfo(0.f, 1.f), VREyeInfo(0.f, 0.f) };
	static VREyeInfo vrmi_stereo_eyes[2] = { VREyeInfo(-.5f, 1.f), VREyeInfo(.5f, 1.f) };
	static VREyeInfo vrmi_sbsfull_eyes[2] = { VREyeInfo(-.5f, .5f), VREyeInfo(.5f, .5f) };
	static VREyeInfo vrmi_sbssquished_eyes[2] = { VREyeInfo(-.5f, 1.f), VREyeInfo(.5f, 1.f) };
	static VREyeInfo vrmi_lefteye_eyes[2] = { VREyeInfo(-.5f, 1.f), VREyeInfo(0.f, 0.f) };
	static VREyeInfo vrmi_righteye_eyes[2] = { VREyeInfo(.5f, 1.f), VREyeInfo(0.f, 0.f) };
	static VREyeInfo vrmi_topbottom_eyes[2] = { VREyeInfo(-.5f, 1.f), VREyeInfo(.5f, 1.f) };
	static VREyeInfo vrmi_checker_eyes[2] = { VREyeInfo(-.5f, 1.f), VREyeInfo(.5f, 1.f) };
	static s3d::OpenVREyePose vrmi_openvr_eyes[2] = { s3d::OpenVREyePose(0, -.5f, 1.f), s3d::OpenVREyePose(1, .5f, 1.f) };

	static VRMode vrmi_mono(1, 1.f, 1.f, 1.f, vrmi_mono_eyes);
	static VRMode vrmi_stereo(2, 1.f, 1.f, 1.f, vrmi_stereo_eyes);
	static VRMode vrmi_sbsfull(2, .5f, 1.f, 2.f, vrmi_sbsfull_eyes);
	static VRMode vrmi_sbssquished(2, .5f, 1.f, 1.f, vrmi_sbssquished_eyes);
	static VRMode vrmi_lefteye(1, 1.f, 1.f, 1.f, vrmi_lefteye_eyes);
	static VRMode vrmi_righteye(1, 1.f, 1.f, 1.f, vrmi_righteye_eyes);
	static VRMode vrmi_topbottom(2, 1.f, .5f, 1.f, vrmi_topbottom_eyes);
	static VRMode vrmi_checker(2, isqrt2, isqrt2, 1.f, vrmi_checker_eyes);
	static s3d::OpenVRMode vrmi_openvr(vrmi_openvr_eyes);

	int mode = !toscreen || (sysCallbacks.DisableTextureFilter && sysCallbacks.DisableTextureFilter()) ? 0 : vr_mode;

	switch (mode)
	{
	default:
	case VR_MONO:
		return &vrmi_mono;

	case VR_GREENMAGENTA:
	case VR_REDCYAN:
	case VR_QUADSTEREO:
	case VR_AMBERBLUE:
	case VR_SIDEBYSIDELETTERBOX:
		return &vrmi_stereo;

	case VR_SIDEBYSIDESQUISHED:
	case VR_COLUMNINTERLEAVED:
		return &vrmi_sbssquished;

	case VR_SIDEBYSIDEFULL:
		return &vrmi_sbsfull;

	case VR_TOPBOTTOM:
	case VR_ROWINTERLEAVED:
		return &vrmi_topbottom;

	case VR_LEFTEYEVIEW:
		return &vrmi_lefteye;

	case VR_RIGHTEYEVIEW:
		return &vrmi_righteye;

	case VR_CHECKERINTERLEAVED:
		return &vrmi_checker;
	case VR_OPENVR:
		return vrmi_openvr.IsInitialized() ? &vrmi_openvr : &vrmi_mono;
	}
}

void VRMode::AdjustViewport(DFrameBuffer *screen) const
{
	screen->mSceneViewport.height = (int)(screen->mSceneViewport.height * mVerticalViewportScale);
	screen->mSceneViewport.top = (int)(screen->mSceneViewport.top * mVerticalViewportScale);
	screen->mSceneViewport.width = (int)(screen->mSceneViewport.width * mHorizontalViewportScale);
	screen->mSceneViewport.left = (int)(screen->mSceneViewport.left * mHorizontalViewportScale);

	screen->mScreenViewport.height = (int)(screen->mScreenViewport.height * mVerticalViewportScale);
	screen->mScreenViewport.top = (int)(screen->mScreenViewport.top * mVerticalViewportScale);
	screen->mScreenViewport.width = (int)(screen->mScreenViewport.width * mHorizontalViewportScale);
	screen->mScreenViewport.left = (int)(screen->mScreenViewport.left * mHorizontalViewportScale);
}

VSMatrix VRMode::GetHUDSpriteProjection() const
{
	VSMatrix mat;
	int w = screen->GetWidth();
	int h = screen->GetHeight();
	float scaled_w = w / mWeaponProjectionScale;
	float left_ofs = (w - scaled_w) / 2.f;
	mat.ortho(left_ofs, left_ofs + scaled_w, (float)h, 0, -1.0f, 1.0f);
	return mat;
}

VREyeInfo::VREyeInfo(float shiftFactor, float scaleFactor)
{
	mShiftFactor = shiftFactor;
	mScaleFactor = scaleFactor;
}

float VREyeInfo::getShift() const
{
	auto res = mShiftFactor * vr_ipd;
	return vr_swap_eyes ? -res : res;
}

VSMatrix VREyeInfo::GetProjection(float fov, float aspectRatio, float fovRatio) const
{
	VSMatrix result;

	if (mShiftFactor == 0)
	{
		float fovy = (float)(2 * RAD2DEG(atan(tan(DEG2RAD(fov) / 2) / fovRatio)));
		result.perspective(fovy, aspectRatio, screen->GetZNear(), screen->GetZFar());
		return result;
	}
	else
	{
		double zNear = screen->GetZNear();
		double zFar = screen->GetZFar();

		// For stereo 3D, use asymmetric frustum shift in projection matrix
		// Q: shouldn't shift vary with roll angle, at least for desktop display?
		// A: No. (lab) roll is not measured on desktop display (yet)
		double frustumShift = zNear * getShift() / vr_screendist; // meters cancel, leaving doom units
																  // double frustumShift = 0; // Turning off shift for debugging
		double fH = zNear * tan(DEG2RAD(fov) / 2) / fovRatio;
		double fW = fH * aspectRatio * mScaleFactor;
		double left = -fW - frustumShift;
		double right = fW - frustumShift;
		double bottom = -fH;
		double top = fH;

		VSMatrix result(1);
		result.frustum((float)left, (float)right, (float)bottom, (float)top, (float)zNear, (float)zFar);
		return result;
	}
}



/* virtual */
DVector3 VREyeInfo::GetViewShift(float yaw) const
{
	if (mShiftFactor == 0)
	{
		// pass-through for Mono view
		return { 0, 0, 0 };
	}
	else
	{
		double dx = -cos(DEG2RAD(yaw)) * vr_vunits_per_meter * getShift();
		double dy = sin(DEG2RAD(yaw)) * vr_vunits_per_meter * getShift();
		return { dx, dy, 0 };
	}
}

