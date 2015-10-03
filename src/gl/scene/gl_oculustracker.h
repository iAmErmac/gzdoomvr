#ifndef GZDOOM_OCULUS_TRACKER_H_
#define GZDOOM_OCULUS_TRACKER_H_

#define OCULUS_SDK_VERSION 0700

#ifdef HAVE_OCULUS_API
extern "C" {
#include "OVR_CAPI.h"
}
#endif

class OculusTexture;

class OculusTracker {
public:
	OculusTracker();
	~OculusTracker();
	float getPositionX();
	float getPositionY();
	float getPositionZ();
	void resetPosition();
	void configureTexture(OculusTexture*);
#ifdef HAVE_OCULUS_API
	// const OVR::HMDInfo& getInfo() const {return Info;}
	float getRiftInterpupillaryDistance() const {
		const_cast<OculusTracker&>(*this).checkConfiguration();
		return ovr_GetFloat(hmd, OVR_KEY_IPD, 0.062f);
	}
	ovrQuatf quaternion;
#endif
	bool isGood() const;
	void update();
	int getDeviceId() const {return deviceId;}

	// Head orientation state, refreshed by call to update();
	float pitch, roll, yaw;
	void setLowPersistence(bool setLow);
	void checkInitialized();
	void checkConfiguration();
	float* getProjection(int eye);
	float getLeftEyeOffset();

private:
	bool trackingConfigured;
	bool renderingConfigured;
	bool ovrInitialized;
	unsigned int frameIndex;
#ifdef HAVE_OCULUS_API
	ovrHmd hmd;
	ovrHmdDesc hmdDesc;
	// ovrSensorDesc sensorDesc;
	int deviceId;
	ovrVector3f position;
	ovrVector3f originPosition;
	ovrPosef eyePose;
#endif
};

extern OculusTracker * sharedOculusTracker;

#endif // GZDOOM_OCULUS_TRACKER_H_
