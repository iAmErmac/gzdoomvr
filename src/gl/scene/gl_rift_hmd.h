#ifndef GZDOOM_GL_RIFTHMD_H_
#define GZDOOM_GL_RIFTHMD_H_

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
	ovrResult init_tracking();
	ovrResult init_graphics(int width, int height);
	bool bindToSceneFrameBufferAndUpdate();
	bool bindHudBuffer();
	ovrPosef& setSceneEyeView(int eye, float zNear, float zFar);
	ovrResult submitFrame(float metersPerSceneUnit);
	void recenter_pose();
	const ovrPosef& getCurrentEyePose() const {return currentEyePose;}
	void destroy(); // release all resources

private:
	ovrResult init_scene_texture();
	ovrResult init_hud_texture(int width, int height);

	unsigned int hudFrameBuffer;
	unsigned int sceneFrameBuffer;
	unsigned int depthBuffer;
	unsigned int frameIndex;

#ifdef HAVE_OCULUS_API
	ovrSwapTextureSet * sceneTextureSet;
	ovrSwapTextureSet * hudTextureSet;
	ovrTexture * mirrorTexture;
	ovrHmd hmd;
	ovrVector3f hmdToEyeViewOffset[2];
	ovrLayerEyeFov sceneLayer;
	ovrLayerQuad hudLayer;
	ovrPosef currentEyePose;
#endif
};

extern RiftHmd* sharedRiftHmd;

#endif // GZDOOM_GL_RIFTHMD_H_
