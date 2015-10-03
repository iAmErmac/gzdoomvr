#ifndef GZDOOM_GL_OCULUSTEXTURE_H_
#define GZDOOM_GL_OCULUSTEXTURE_H_

#ifdef HAVE_OCULUS_API
extern "C" {
#include "OVR_CAPI.h"
}
#endif

// Framebuffer texture for intermediate rendering of Oculus Rift image
class OculusTexture {
public:
	OculusTexture();
	~OculusTexture() {destroy();}
	void bindToFrameBuffer();
	void renderToScreen(unsigned int& frameIndex);
	void unbind();

private:
#ifdef HAVE_OCULUS_API
	void init(ovrHmd hmd);
#endif
	void destroy(); // release all opengl resources

	unsigned int frameBuffer;

#ifdef HAVE_OCULUS_API
	ovrSwapTextureSet * pTextureSet;
	ovrTexture * mirrorTexture;
	ovrHmd hmd;
	ovrVector3f hmdToEyeViewOffset[2];
	ovrLayerEyeFov layer;
#endif
};

#endif // GZDOOM_GL_OCULUSTEXTURE_H_
