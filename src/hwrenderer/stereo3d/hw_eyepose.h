#pragma once

#include "r_data/matrix.h"
#include "gl_load/gl_system.h"
#include "gl/renderer/gl_renderer.h"


/* Viewpoint of one eye */
class EyePose
{
public:
	EyePose() : m_isActive(false) {}
	virtual ~EyePose() {}
	virtual VSMatrix GetProjection(float fov, float aspectRatio, float fovRatio) const;
	//virtual Viewport GetViewport(const Viewport& fullViewport) const;
	virtual void GetViewShift(float yaw, float outViewShift[3]) const;
	virtual void SetUp() const { m_isActive = true; }
	virtual void TearDown() const { m_isActive = false; }
	virtual void AdjustHud() const {}
	virtual void AdjustBlend(FDrawInfo* di) const {}
	bool isActive() const { return m_isActive; }

private:
	mutable bool m_isActive;
};

class ShiftedEyePose : public EyePose
{
public:
	ShiftedEyePose(float shift) : shift(shift) {};
	float getShift() const;
	virtual VSMatrix GetProjection(float fov, float aspectRatio, float fovRatio) const;
	virtual void GetViewShift(float yaw, float outViewShift[3]) const;

protected:
	void setShift(float shift) { this->shift = shift; }

private:
	float shift;
};


class LeftEyePose : public ShiftedEyePose
{
public:
	LeftEyePose(float ipd) : ShiftedEyePose( -0.5f * ipd) {}
	void setIpd(float ipd) { setShift(-0.5f * ipd); }
};


class RightEyePose : public ShiftedEyePose
{
public:
	RightEyePose(float ipd) : ShiftedEyePose(0.5f * ipd) {}
	void setIpd(float ipd) { setShift(0.5f * ipd); }
};


class SBSFLeftEyePose : public LeftEyePose {
public:
	SBSFLeftEyePose(float ipdMeters) : LeftEyePose(ipdMeters) {}
	virtual VSMatrix GetProjection(float fov, float aspectRatio, float fovRatio) const override {
		return LeftEyePose::GetProjection(fov, 0.5f * aspectRatio, fovRatio);
	}
};

class SBSFRightEyePose : public RightEyePose {
public:
	SBSFRightEyePose(float ipdMeters) : RightEyePose(ipdMeters) {}
	virtual VSMatrix GetProjection(float fov, float aspectRatio, float fovRatio) const override {
		return RightEyePose::GetProjection(fov, 0.5f * aspectRatio, fovRatio);
	}
};
