/*
** gl_openvr.h
** Stereoscopic 3D API
**
**---------------------------------------------------------------------------
** Copyright 2015 Christopher Bruns
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
**
*/

#ifndef GL_OPENVR_H_
#define GL_OPENVR_H_

#include "gl_stereo3d.h"
#include "gl_stereo_leftright.h"
#include "vr_framebuffer.h"

// forward declaration from openvr.h
namespace vr {
	class IVRSystem;
	struct HmdMatrix44_t;
	struct Texture_t;
	enum EVREye;
}

/* stereoscopic 3D API */
namespace s3d {

class OpenVREyePose : public ShiftedEyePose
{
public:
	OpenVREyePose(vr::EVREye eye);
	virtual ~OpenVREyePose();
	virtual VSMatrix GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio) const;
	virtual void SetUp() const;
	virtual void TearDown() const;
	void initialize(vr::IVRSystem& vrsystem);
	void dispose();
	bool submitFrame() const;

protected:
	VSMatrix projectionMatrix;
	VSMatrix eyeToHeadTransform;
	VrFramebuffer framebuffer;
	vr::Texture_t* eyeTexture;
	vr::EVREye eye;

private:
	typedef ShiftedEyePose super;
};

class OpenVRMode : public Stereo3DMode
{
public:
	static const OpenVRMode& getInstance();

	virtual ~OpenVRMode();
	virtual void SetUp() const;
	virtual void TearDown() const;

protected:
	OpenVRMode();
	void updateDoomViewDirection() const;
	void updateHmdPose(double hmdYawRadians, double hmdPitchRadians, double hmdRollRadians) const;

	OpenVREyePose leftEyeView;
	OpenVREyePose rightEyeView;

	vr::IVRSystem* ivrSystem;

private:
	typedef Stereo3DMode super;
};

} /* namespace st3d */


#endif /* GL_OPENVR_H_ */
