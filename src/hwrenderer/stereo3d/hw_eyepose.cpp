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

#include "hw_eyepose.h"
#include "v_video.h"
#include "vectors.h" // RAD2DEG
#include "doomtype.h" // M_PI
#include "hwrenderer/utility/hw_cvars.h"
#include "gl_load/gl_system.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"

EXTERN_CVAR(Float, vr_screendist)
EXTERN_CVAR(Float, vr_vunits_per_meter)	
EXTERN_CVAR(Bool, vr_swap_eyes)


/* virtual */
VSMatrix EyePose::GetProjection(float fov, float aspectRatio, float fovRatio) const
{
	VSMatrix result;

	float fovy = (float)(2 * RAD2DEG(atan(tan(DEG2RAD(fov) / 2) / fovRatio)));
	result.perspective(fovy, aspectRatio, screen->GetZNear(), screen->GetZFar());

	return result;
}

/* virtual */
void EyePose::GetViewShift(float yaw, float outViewShift[3]) const
{
	// pass-through for Mono view
	outViewShift[0] = 0;
	outViewShift[1] = 0;
	outViewShift[2] = 0;
}

/* virtual */
VSMatrix ShiftedEyePose::GetProjection(float fov, float aspectRatio, float fovRatio) const
{
	double zNear = 5.0;
	double zFar = 65536.0;

	// For stereo 3D, use asymmetric frustum shift in projection matrix
	// Q: shouldn't shift vary with roll angle, at least for desktop display?
	// A: No. (lab) roll is not measured on desktop display (yet)
	double frustumShift = zNear * getShift() / vr_screendist; // meters cancel, leaving doom units
	// double frustumShift = 0; // Turning off shift for debugging
	double fH = zNear * tan(DEG2RAD(fov) / 2) / fovRatio;
	double fW = fH * aspectRatio;
	double left = -fW - frustumShift;
	double right = fW - frustumShift;
	double bottom = -fH;
	double top = fH;

	VSMatrix result(1);
	result.frustum(left, right, bottom, top, zNear, zFar);
	return result;
}


/* virtual */
void ShiftedEyePose::GetViewShift(float yaw, float outViewShift[3]) const
{
	double pixelstretch = level.info ? level.info->pixelstretch : 1.20;
	float dx = -cos(DEG2RAD(yaw)) * vr_vunits_per_meter * pixelstretch * getShift();
	float dy = sin(DEG2RAD(yaw)) * vr_vunits_per_meter * pixelstretch * getShift();
	outViewShift[0] = dx;
	outViewShift[1] = dy;
	outViewShift[2] = 0;
}

float ShiftedEyePose::getShift() const 
{
	return vr_swap_eyes ? -shift : shift;
}

