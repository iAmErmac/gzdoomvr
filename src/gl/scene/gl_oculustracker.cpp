#include "gl/scene/gl_oculustracker.h"

OculusTracker::OculusTracker() 
	: pitch(0)
	, roll(0)
	, yaw(0)
{
#ifdef HAVE_OCULUS_API
	OVR::System::Init();
	pFusionResult = new OVR::SensorFusion();
	pManager = *OVR::DeviceManager::Create();
	pHMD = *pManager->EnumerateDevices<OVR::HMDDevice>().CreateDevice();
	if(pHMD)
	{
		InfoLoaded = pHMD->GetDeviceInfo(&Info);
		pSensor = pHMD->GetSensor();
	}
	else
	{
		pSensor = *pManager->EnumerateDevices<OVR::SensorDevice>().CreateDevice();
	}

	if (pSensor)
	{
		pFusionResult->AttachToSensor(pSensor);
	}
#endif
}

OculusTracker::~OculusTracker() {
#ifdef HAVE_OCULUS_API
	pSensor.Clear();
	pHMD.Clear();
	pManager.Clear();
	delete pFusionResult;
	OVR::System::Destroy();
#endif
}

bool OculusTracker::isGood() const {
#ifdef HAVE_OCULUS_API
	return pSensor.GetPtr() != NULL;
#else
	return false;
#endif
}

void OculusTracker::report() const {
}

void OculusTracker::update() {
#ifdef HAVE_OCULUS_API
	OVR::Quatf quaternion = pFusionResult->GetOrientation();
	quaternion.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&yaw, &pitch, &roll);
#endif
}
