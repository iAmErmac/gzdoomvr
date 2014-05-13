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
	pFusionResult->SetPredictionEnabled(true);
	pFusionResult->SetPrediction(0.020, true); // Never hurts to be 20 ms in future?
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
	bool usePredicted = false;
	OVR::Quatf quaternion;
	if (usePredicted)
		quaternion = pFusionResult->GetPredictedOrientation();
	else
		quaternion = pFusionResult->GetOrientation();

	// Compress head tracking orientation in Y, to compensate for Doom pixel aspect ratio
	/*
	const double pixelRatio = 1.00;
	OVR::Vector3<float> axis;
	float angle;
	quaternion.GetAxisAngle(&axis, &angle);
	axis.y *= 1.0/pixelRatio; // 1) squish direction in Y
	axis.Normalize();
	float angleFactor = 1.0 + sqrt(1.0 - axis.y*axis.y) * (pixelRatio - 1.0);
	angle = atan2(angleFactor * sin(angle), cos(angle)); // 2) Expand angle in Y
	OVR::Quatf squishedQuat(axis, angle);
	squishedQuat.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&yaw, &pitch, &roll);
	*/

	quaternion.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&yaw, &pitch, &roll);
#endif
}
