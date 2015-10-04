#ifndef GZDOOM_GL_OCULUSTEXTURE_H_
#define GZDOOM_GL_OCULUSTEXTURE_H_

#ifdef HAVE_OCULUS_API
extern "C" {
#include "OVR_CAPI.h"
}
#endif

// Framebuffer texture for intermediate rendering of Oculus Rift image
class RiftHmd {
public:
	RiftHmd();
	~RiftHmd() {destroy();}
	ovrResult init();
	void bindToFrameBufferAndUpdate();
	ovrPosef setEyeView(int eye, float zNear, float zFar);
	void submitFrame();
	void recenter_pose();
	const ovrPosef& getCurrentEyePose() const {return currentEyePose;}
	void destroy(); // release all resources

private:
#ifdef HAVE_OCULUS_API
#endif

	unsigned int frameBuffer;
	unsigned int frameIndex;

#ifdef HAVE_OCULUS_API
	ovrSwapTextureSet * pTextureSet;
	ovrTexture * mirrorTexture;
	ovrHmd hmd;
	ovrVector3f hmdToEyeViewOffset[2];
	ovrLayerEyeFov sceneLayer;
	ovrPosef currentEyePose;
#endif
};

extern RiftHmd* sharedRiftHmd;

#endif // GZDOOM_GL_OCULUSTEXTURE_H_
