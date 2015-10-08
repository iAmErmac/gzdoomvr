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
	void destroy(); // release all resources
	ovrResult init_tracking();
	ovrResult init_graphics();
	bool bindToSceneFrameBufferAndUpdate();
	void bindToSceneFrameBuffer();

	void paintHudQuad(float hudScale, float pitchAngle);
	void paintCrosshairQuad(const ovrPosef& eyePose, const ovrPosef& otherEyePose);
	void paintWeaponQuad(const ovrPosef& eyePose, const ovrPosef& otherEyePose, float weaponDist);

	ovrPosef& setSceneEyeView(int eye, float zNear, float zFar);
	ovrResult submitFrame(float metersPerSceneUnit);
	void recenter_pose();
	const ovrPosef& getCurrentEyePose() const {return currentEyePose;}

private:
	ovrResult init_scene_texture();

	unsigned int sceneFrameBuffer;
	unsigned int depthBuffer;
	unsigned int frameIndex;

#ifdef HAVE_OCULUS_API
	ovrSwapTextureSet * sceneTextureSet;
	ovrTexture * mirrorTexture;
	ovrHmd hmd;
	ovrVector3f hmdToEyeViewOffset[2];
	ovrLayerEyeFov sceneLayer;
	ovrPosef currentEyePose;
#endif
};

extern RiftHmd* sharedRiftHmd;

#endif // GZDOOM_GL_RIFTHMD_H_
