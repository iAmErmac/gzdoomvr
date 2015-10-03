#define NOMINMAX
#include "gl/scene/gl_oculustexture.h"
#include "gl/system/gl_system.h"
#include "gl/system/gl_cvars.h"
#include <cstring>
#include <string>
#include <sstream>

extern "C" {
#include "OVR_CAPI_GL.h"
}

using namespace std;

OculusTexture::OculusTexture()
	: hmd(nullptr)
	, frameBuffer(0)
	, pTextureSet(nullptr)
	, mirrorTexture(nullptr)
{
}

void OculusTexture::destroy() {
	if (pTextureSet) {
		ovr_DestroySwapTextureSet(hmd, pTextureSet);
		pTextureSet = nullptr;
	}
	glDeleteFramebuffers(1, &frameBuffer);
	frameBuffer = 0;
}

void OculusTexture::init(ovrHmd hmd) 
{
    // NOTE: Initialize OpenGL first (elsewhere), before getting Rift textures here.
    // Configure Stereo settings.
    // Use a single shared texture for simplicity
    // 1bb) Compute texture sizes
	ovrHmdDesc hmdDesc = ovr_GetHmdDesc(hmd);
    ovrSizei recommendedTex0Size = ovr_GetFovTextureSize(hmd, ovrEye_Left, 
            hmdDesc.DefaultEyeFov[0], 1.0);
    ovrSizei recommendedTex1Size = ovr_GetFovTextureSize(hmd, ovrEye_Right,
            hmdDesc.DefaultEyeFov[1], 1.0);
    ovrSizei bufferSize;
    bufferSize.w  = recommendedTex0Size.w + recommendedTex1Size.w;
    bufferSize.h = std::max( recommendedTex0Size.h, recommendedTex1Size.h );
    // print "Recommended buffer size = ", bufferSize, bufferSize.w, bufferSize.h
    // NOTE: We need to have set up OpenGL context before this point...
    // 1c) Allocate SwapTextureSets
    ovrResult result = ovr_CreateSwapTextureSetGL(hmd,
            GL_SRGB8_ALPHA8, bufferSize.w, bufferSize.h, &pTextureSet);

    // Initialize VR structures, filling out description.
    // 1ba) Compute FOV
    ovrEyeRenderDesc eyeRenderDesc[2];
    // ovrVector3f hmdToEyeViewOffset[2];
    eyeRenderDesc[0] = ovr_GetRenderDesc(hmd, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
    eyeRenderDesc[1] = ovr_GetRenderDesc(hmd, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);
    hmdToEyeViewOffset[0] = eyeRenderDesc[0].HmdToEyeViewOffset;
    hmdToEyeViewOffset[1] = eyeRenderDesc[1].HmdToEyeViewOffset;
    // Initialize our single full screen Fov layer.
    // ovrLayerEyeFov layer;
    layer.Header.Type      = ovrLayerType_EyeFov;
    layer.Header.Flags     = ovrLayerFlag_TextureOriginAtBottomLeft; // OpenGL convention;
    layer.ColorTexture[0]  = pTextureSet; // single texture for both eyes;
    layer.ColorTexture[1]  = pTextureSet; // single texture for both eyes;
    layer.Fov[0]           = eyeRenderDesc[0].Fov;
    layer.Fov[1]           = eyeRenderDesc[1].Fov;
	layer.Viewport[0].Pos.x = 0;
	layer.Viewport[0].Pos.y = 0;
	layer.Viewport[0].Size.w = bufferSize.w / 2;
	layer.Viewport[0].Size.h = bufferSize.h;
	layer.Viewport[1].Pos.x = bufferSize.w / 2;
	layer.Viewport[1].Pos.y = 0;
	layer.Viewport[1].Size.w = bufferSize.w / 2;
	layer.Viewport[1].Size.h = bufferSize.h;
    // ld.RenderPose is updated later per frame.

	glGenFramebuffers(1, &frameBuffer);
}

void OculusTexture::bindToFrameBuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    ovrFrameTiming ftiming  = ovr_GetFrameTiming(hmd, 0);
    ovrTrackingState hmdState = ovr_GetTrackingState(hmd, ftiming.DisplayMidpointSeconds);
    // print hmdState.HeadPose.ThePose
    ovr_CalcEyePoses(hmdState.HeadPose.ThePose, 
            hmdToEyeViewOffset,
            layer.RenderPose);
    // Increment to use next texture, just before writing
    // 2d) Advance CurrentIndex within each used texture set to target the next consecutive texture buffer for the following frame.
	int ix = pTextureSet->CurrentIndex + 1;
	ix = ix % pTextureSet->TextureCount;
    pTextureSet->CurrentIndex = ix;
    ovrGLTexture * texture = (ovrGLTexture*) &pTextureSet->Textures[ix];
    glFramebufferTexture2D(GL_FRAMEBUFFER, 
            GL_COLOR_ATTACHMENT0, 
            GL_TEXTURE_2D,
			texture->OGL.TexId,
            0);
	// TODO viewport...
}

void OculusTexture::renderToScreen(unsigned int& frameIndex) {
    // 2c) Call ovr_SubmitFrame, passing swap texture set(s) from the previous step within a ovrLayerEyeFov structure. Although a single layer is required to submit a frame, you can use multiple layers and layer types for advanced rendering. ovr_SubmitFrame passes layer textures to the compositor which handles distortion, timewarp, and GPU synchronization before presenting it to the headset. 
    ovrViewScaleDesc viewScale;
    viewScale.HmdSpaceToWorldScaleInMeters = 1.0;
    viewScale.HmdToEyeViewOffset[0] = hmdToEyeViewOffset[0];
    viewScale.HmdToEyeViewOffset[1] = hmdToEyeViewOffset[1];
	ovrLayerHeader* layers = &layer.Header;
    ovrResult result = ovr_SubmitFrame(hmd, frameIndex, &viewScale, &layers, 1);
    frameIndex += 1;
}

void OculusTexture::unbind() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

