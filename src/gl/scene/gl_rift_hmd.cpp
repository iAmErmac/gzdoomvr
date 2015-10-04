#define NOMINMAX
#include "gl/scene/gl_rift_hmd.h"
#include "gl/system/gl_system.h"
#include "gl/system/gl_cvars.h"
#include <cstring>
#include <string>
#include <sstream>

extern "C" {
// #include "OVR.h"
#include "OVR_CAPI_GL.h"
}

using namespace std;

RiftHmd::RiftHmd()
	: hmd(nullptr)
	, sceneFrameBuffer(0)
	, hudFrameBuffer(0)
	, depthBuffer(0)
	, sceneTextureSet(nullptr)
	, mirrorTexture(nullptr)
	, frameIndex(0)
{
}

void RiftHmd::destroy() {
	if (sceneTextureSet) {
		ovr_DestroySwapTextureSet(hmd, sceneTextureSet);
		sceneTextureSet = nullptr;
	}
	glDeleteRenderbuffers(1, &depthBuffer);
	depthBuffer = 0;
	glDeleteFramebuffers(1, &sceneFrameBuffer);
	sceneFrameBuffer = 0;
	glDeleteFramebuffers(1, &hudFrameBuffer);
	hudFrameBuffer = 0;
	if (hmd) {
		ovr_Destroy(hmd);
		hmd = nullptr;
	}
	ovr_Shutdown();
}

ovrResult RiftHmd::init_tracking() 
{
	if (hmd) return ovrSuccess; // already initialized
	ovrResult result = ovr_Initialize(nullptr);
	if OVR_FAILURE(result)
		return result;
	ovrGraphicsLuid luid;
	ovr_Create(&hmd, &luid);
	ovr_ConfigureTracking(hmd,
			ovrTrackingCap_Orientation | // supported capabilities
			ovrTrackingCap_MagYawCorrection |
			ovrTrackingCap_Position, 
			0); // required capabilities
	return result;
}


ovrResult RiftHmd::init_graphics(int width, int height) 
{
    // NOTE: Initialize OpenGL first (elsewhere), before getting Rift textures here.

	// HMD
	ovrResult result = init_tracking();
	if OVR_FAILURE(result)
		return result;

	// 3D scene
	result = init_scene_texture();
	if OVR_FAILURE(result)
		return result;

	// HUD
	result = init_hud_texture(width, height);
	if OVR_FAILURE(result)
		return result;

	return result;
}

ovrResult RiftHmd::init_scene_texture()
{
	if (sceneTextureSet)
		return ovrSuccess;

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
            GL_SRGB8_ALPHA8, bufferSize.w, bufferSize.h, &sceneTextureSet);
	if OVR_FAILURE(result)
		return result;

    // Initialize VR structures, filling out description.
    // 1ba) Compute FOV
    ovrEyeRenderDesc eyeRenderDesc[2];
    // ovrVector3f hmdToEyeViewOffset[2];
    eyeRenderDesc[0] = ovr_GetRenderDesc(hmd, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
    eyeRenderDesc[1] = ovr_GetRenderDesc(hmd, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);
    hmdToEyeViewOffset[0] = eyeRenderDesc[0].HmdToEyeViewOffset;
    hmdToEyeViewOffset[1] = eyeRenderDesc[1].HmdToEyeViewOffset;

	// Stereo3D Layer for primary 3D scene
    // Initialize our single full screen Fov layer.
    // ovrLayerEyeFov layer;
    sceneLayer.Header.Type      = ovrLayerType_EyeFov;
    sceneLayer.Header.Flags     = ovrLayerFlag_TextureOriginAtBottomLeft; // OpenGL convention;
    sceneLayer.ColorTexture[0]  = sceneTextureSet; // single texture for both eyes;
    sceneLayer.ColorTexture[1]  = sceneTextureSet; // single texture for both eyes;
    sceneLayer.Fov[0]           = eyeRenderDesc[0].Fov;
    sceneLayer.Fov[1]           = eyeRenderDesc[1].Fov;
	sceneLayer.Viewport[0].Pos.x = 0;
	sceneLayer.Viewport[0].Pos.y = 0;
	sceneLayer.Viewport[0].Size.w = bufferSize.w / 2;
	sceneLayer.Viewport[0].Size.h = bufferSize.h;
	sceneLayer.Viewport[1].Pos.x = bufferSize.w / 2;
	sceneLayer.Viewport[1].Pos.y = 0;
	sceneLayer.Viewport[1].Size.w = bufferSize.w / 2;
	sceneLayer.Viewport[1].Size.h = bufferSize.h;

	// create OpenGL framebuffer for rendering to Rift
	glGenFramebuffers(1, &sceneFrameBuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneFrameBuffer);
	// color layer will be provided by Rift API at render time...
	// depth buffer
	glGenRenderbuffers(1, &depthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, bufferSize.w, bufferSize.h);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

	// clean up
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	return result;
}

ovrResult RiftHmd::init_hud_texture(int width, int height)
{
	if (hudTextureSet)
		return ovrSuccess;

    ovrResult result = ovr_CreateSwapTextureSetGL(hmd,
            GL_SRGB8_ALPHA8, width, height, &hudTextureSet);

	// Stereo3D Layer for primary 3D scene
	// Second Monoscopic Quad layer for menus
	// Create HUD layer, fixed to the player's torso
	hudLayer.Header.Type      = ovrLayerType_QuadHeadLocked; // Was non-existent "ovrLayerType_Quad" in the docs...
    hudLayer.Header.Flags     = ovrLayerFlag_TextureOriginAtBottomLeft | ovrLayerFlag_HighQuality;
    hudLayer.ColorTexture  = hudTextureSet; // single texture for both eyes;
	// 50cm in front and 20cm down from the player's nose,
	// fixed relative to their torso.
	float hudScale = 1.0f;
	hudLayer.QuadPoseCenter.Position.x =  0.00f * hudScale;
	hudLayer.QuadPoseCenter.Position.y = -0.20f * hudScale;
	hudLayer.QuadPoseCenter.Position.z = -0.50f * hudScale;
	hudLayer.QuadPoseCenter.Orientation = ovrQuatf();
	// HUD is 50cm wide, 30cm tall.
	hudLayer.QuadSize.x = 0.50f * hudScale;
	hudLayer.QuadSize.y = 0.30f * hudScale;
	// Display all of the HUD texture.
	hudLayer.Viewport.Pos.x = 0.0f;
	hudLayer.Viewport.Pos.y = 0.0f;
	hudLayer.Viewport.Size.w = 1.0f;
	hudLayer.Viewport.Size.h = 1.0f;

	// create OpenGL framebuffer for rendering to Rift
	glGenFramebuffers(1, &hudFrameBuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hudFrameBuffer);
	// color layer will be provided by Rift API at render time...
	// depth buffer

	// clean up
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    // ld.RenderPose is updated later per frame.
	return result;
}

bool RiftHmd::bindToSceneFrameBufferAndUpdate()
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneFrameBuffer);
    ovrFrameTiming ftiming  = ovr_GetFrameTiming(hmd, 0);
    ovrTrackingState hmdState = ovr_GetTrackingState(hmd, ftiming.DisplayMidpointSeconds);
    // print hmdState.HeadPose.ThePose
    ovr_CalcEyePoses(hmdState.HeadPose.ThePose, 
            hmdToEyeViewOffset,
            sceneLayer.RenderPose);
    // Increment to use next texture, just before writing
    // 2d) Advance CurrentIndex within each used texture set to target the next consecutive texture buffer for the following frame.
	int ix = sceneTextureSet->CurrentIndex + 1;
	ix = ix % sceneTextureSet->TextureCount;
    sceneTextureSet->CurrentIndex = ix;
    ovrGLTexture * texture = (ovrGLTexture*) &sceneTextureSet->Textures[ix];
    glFramebufferTexture2D(GL_FRAMEBUFFER, 
            GL_COLOR_ATTACHMENT0, 
            GL_TEXTURE_2D,
			texture->OGL.TexId,
            0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
	GLenum fbStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	GLenum desiredStatus = GL_FRAMEBUFFER_COMPLETE;
	if (fbStatus != GL_FRAMEBUFFER_COMPLETE)
		return false;
	return true;
}

bool RiftHmd::bindHudBuffer()
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hudFrameBuffer);
    // Increment to use next texture, just before writing
    // 2d) Advance CurrentIndex within each used texture set to target the next consecutive texture buffer for the following frame.
	int ix = hudTextureSet->CurrentIndex + 1;
	ix = ix % hudTextureSet->TextureCount;
    hudTextureSet->CurrentIndex = ix;
    ovrGLTexture * texture = (ovrGLTexture*) &hudTextureSet->Textures[ix];
    glFramebufferTexture2D(GL_FRAMEBUFFER, 
            GL_COLOR_ATTACHMENT0, 
            GL_TEXTURE_2D,
			texture->OGL.TexId,
            0);
	GLenum fbStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	GLenum desiredStatus = GL_FRAMEBUFFER_COMPLETE;
	if (fbStatus != GL_FRAMEBUFFER_COMPLETE)
		return false;

    // Set up eye viewport
	ovrSizei sz = texture->Texture.Header.TextureSize;
    glViewport(0, 0, sz.w, sz.h);
	glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, sz.w, sz.h);
    // glMatrixMode(GL_PROJECTION);
    // glLoadIdentity();
	return true;
}

ovrPosef& RiftHmd::setSceneEyeView(int eye, float zNear, float zFar) {
    // Set up eye viewport
    ovrRecti v = sceneLayer.Viewport[eye];
    glViewport(v.Pos.x, v.Pos.y, v.Size.w, v.Size.h);
	glEnable(GL_SCISSOR_TEST);
    glScissor(v.Pos.x, v.Pos.y, v.Size.w, v.Size.h);
    // Get projection matrix for the Rift camera
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    ovrMatrix4f proj = ovrMatrix4f_Projection(sceneLayer.Fov[eye], zNear, zFar,
                    ovrProjection_RightHanded | ovrProjection_ClipRangeOpenGL);
    glMultTransposeMatrixf(&proj.M[0][0]);

    // Get view matrix for the Rift camera
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    currentEyePose = sceneLayer.RenderPose[eye];
	return currentEyePose;
}

ovrResult RiftHmd::submitFrame(float metersPerSceneUnit) {
    // 2c) Call ovr_SubmitFrame, passing swap texture set(s) from the previous step within a ovrLayerEyeFov structure. Although a single layer is required to submit a frame, you can use multiple layers and layer types for advanced rendering. ovr_SubmitFrame passes layer textures to the compositor which handles distortion, timewarp, and GPU synchronization before presenting it to the headset. 
    ovrViewScaleDesc viewScale;
    viewScale.HmdSpaceToWorldScaleInMeters = metersPerSceneUnit;
    viewScale.HmdToEyeViewOffset[0] = hmdToEyeViewOffset[0];
    viewScale.HmdToEyeViewOffset[1] = hmdToEyeViewOffset[1];
	ovrLayerHeader* layerList[2];
	layerList[0] = &sceneLayer.Header;
	layerList[1] = &hudLayer.Header;
    ovrResult result = ovr_SubmitFrame(hmd, frameIndex, &viewScale, layerList, 1);
    frameIndex += 1;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return result;
}

void RiftHmd::recenter_pose() {
	ovr_RecenterPose(hmd);
}

static RiftHmd _sharedRiftHmd;
RiftHmd* sharedRiftHmd = &_sharedRiftHmd;
