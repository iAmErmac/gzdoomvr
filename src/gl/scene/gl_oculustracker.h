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
	ovrResult init();
	void destroy();
	void recenter_pose();

private:
	bool ovrInitialized;
#ifdef HAVE_OCULUS_API
	ovrHmd hmd;
#endif
};

#endif // GZDOOM_OCULUS_TRACKER_H_
