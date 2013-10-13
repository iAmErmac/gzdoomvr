#ifndef GZDOOM_OCULUS_TRACKER_H_
#define GZDOOM_OCULUS_TRACKER_H_

#ifdef HAVE_OCULUS_API
#include "OVR.h"
#endif

class OculusTracker {
public:
	OculusTracker();
	~OculusTracker();
	bool isGood();
	void report();
	void update();

	float pitch, roll, yaw;

private:
#ifdef HAVE_OCULUS_API
	OVR::Ptr<OVR::DeviceManager> pManager;
	OVR::Ptr<OVR::HMDDevice> pHMD;
	OVR::Ptr<OVR::SensorDevice> pSensor;
	OVR::SensorFusion* pFusionResult;
	OVR::HMDInfo Info;
	bool InfoLoaded;
#endif
};

#endif // GZDOOM_OCULUS_TRACKER_H_
