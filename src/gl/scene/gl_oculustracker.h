#ifndef GZDOOM_OCULUS_TRACKER_H_
#define GZDOOM_OCULUS_TRACKER_H_

#ifdef HAVE_OCULUS_API
#include "OVR.h"
#endif

class OculusTracker {
public:
	OculusTracker();
	~OculusTracker();
	float getPositionX();
	float getPositionY();
	float getPositionZ();
	void resetPosition();
	void beginFrame();
	void endFrame();
#ifdef HAVE_OCULUS_API
	// const OVR::HMDInfo& getInfo() const {return Info;}
	float getRiftInterpupillaryDistance() const {return ovrHmd_GetFloat(hmd, OVR_KEY_IPD, 0.062f);}
	OVR::Quatf quaternion;
#endif
	bool isGood() const;
	void update();
	int getDeviceId() const {return deviceId;}

	// Head orientation state, refreshed by call to update();
	float pitch, roll, yaw;
	void setLowPersistence(bool setLow);

private:
#ifdef HAVE_OCULUS_API
	ovrHmd hmd;
	// ovrHmdDesc hmdDesc;
	// ovrSensorDesc sensorDesc;
	int deviceId;
	OVR::Vector3f position;
	OVR::Vector3f originPosition;
	ovrPosef eyePose;
	int frameIndex;

	// OVR::Ptr<OVR::DeviceManager> pManager;
	// OVR::Ptr<OVR::HMDDevice> pHMD;
	// OVR::Ptr<OVR::SensorDevice> pSensor;
	// OVR::SensorFusion* pFusionResult;
	// OVR::HMDInfo Info;
	// bool InfoLoaded;
#endif
};

#endif // GZDOOM_OCULUS_TRACKER_H_
