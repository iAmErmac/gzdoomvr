#include "v_video.h"
#include "gl/system/gl_system.h"
#include "gl/system/gl_cvars.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/scene/gl_stereo3d.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_colormask.h"
#include "gl/scene/gl_hudtexture.h"
#include "gl/utility/gl_clock.h"
#include "doomstat.h"
#include "r_utility.h" // viewpitch
#include "g_game.h"
#include "c_console.h"
#include "sbar.h"
#include "am_map.h"

#define RIFT_HUDSCALE 0.40

EXTERN_CVAR(Bool, gl_draw_sync)
//
CVAR(Int, vr_mode, 0, CVAR_GLOBALCONFIG)
CVAR(Bool, vr_swap, false, CVAR_GLOBALCONFIG)
// intraocular distance in meters
CVAR(Float, vr_ipd, 0.062f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // METERS


// Global shared Stereo3DMode object
Stereo3D Stereo3DMode;


// Delegated screen functions from v_video.h
int getStereoScreenWidth() {
	return Stereo3DMode.getScreenWidth();
}
int getStereoScreenHeight() {
	return Stereo3DMode.getScreenHeight();
}
void stereoScreenUpdate() {
	Stereo3DMode.updateScreen();
}



Stereo3D::Stereo3D() 
	: mode(MONO)
	, oculusTexture(NULL)
	, hudTexture(NULL)
	, oculusTracker(NULL)
	, adaptScreenSize(false)
{}

void Stereo3D::render(FGLRenderer& renderer, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool toscreen, sector_t * viewsector) 
{
	setMode(vr_mode);

	// Restore actual screen, instead of offscreen single-eye buffer,
	// in case we just exited Rift mode.
	if (hudTexture)
		hudTexture->unbind();
	adaptScreenSize = false;

	GLboolean supportsStereo = false;
	GLboolean supportsBuffered = false;
	// Task: manually calibrate oculusFov by slowly yawing view. 
	// If subjects approach center of view too fast, oculusFov is too small.
	// If subjects approach center of view too slowly, oculusFov is too large.
	// If subjects approach correctly , oculusFov is just right.
	// 90 is too large, 80 is too small.
	float oculusFov = 85 * fovratio; // Hard code probably wider fov for oculus
	if (mode == OCULUS_RIFT)
		renderer.mCurrentFoV = oculusFov; // needed for Frustum angle calculation
	angle_t a1 = renderer.FrustumAngle();

	switch(mode) 
	{

	case MONO:
		setViewportFull(renderer, bounds);
		setMonoView(renderer, fov, ratio, fovratio);
		renderer.RenderOneEye(a1, toscreen);
		renderer.EndDrawScene(viewsector);
		break;

	case GREEN_MAGENTA:
		setViewportFull(renderer, bounds);
		{ // Local scope for color mask
			// Left eye green
			LocalScopeGLColorMask colorMask(0,1,0,1); // green
			setLeftEyeView(renderer, fov, ratio, fovratio);
			renderer.RenderOneEye(a1, toscreen);

			// Right eye magenta
			colorMask.setColorMask(1,0,1,1); // magenta
			setRightEyeView(renderer, fov, ratio, fovratio);
			renderer.RenderOneEye(a1, toscreen);
		} // close scope to auto-revert glColorMask
		renderer.EndDrawScene(viewsector);
		break;

	case RED_CYAN:
		setViewportFull(renderer, bounds);
		{ // Local scope for color mask
			// Left eye red
			LocalScopeGLColorMask colorMask(1,0,0,1); // red
			setLeftEyeView(renderer, fov, ratio, fovratio);
			renderer.RenderOneEye(a1, toscreen);

			// Right eye cyan
			colorMask.setColorMask(0,1,1,1); // cyan
			setRightEyeView(renderer, fov, ratio, fovratio);
			renderer.RenderOneEye(a1, toscreen);
		} // close scope to auto-revert glColorMask
		renderer.EndDrawScene(viewsector);
		break;

	case SIDE_BY_SIDE:
		{
			// FIRST PASS - 3D
			// Temporarily modify global variables, so HUD could draw correctly
			// each view is half width
			int oldViewwidth = viewwidth;
			viewwidth = viewwidth/2;
			// left
			setViewportLeft(renderer, bounds);
			setLeftEyeView(renderer, fov, ratio/2, fovratio);
			renderer.RenderOneEye(a1, false); // False, to not swap yet
			// right
			// right view is offset to right
			int oldViewwindowx = viewwindowx;
			viewwindowx += viewwidth;
			setViewportRight(renderer, bounds);
			setRightEyeView(renderer, fov, ratio/2, fovratio);
			renderer.RenderOneEye(a1, toscreen);
			//
			// SECOND PASS weapon sprite
			renderer.EndDrawScene(viewsector); // right view
			viewwindowx -= viewwidth;
			renderer.EndDrawScene(viewsector); // left view
			//
			// restore global state
			viewwidth = oldViewwidth;
			viewwindowx = oldViewwindowx;
			break;
		}

	case SIDE_BY_SIDE_SQUISHED:
		{
			// FIRST PASS - 3D
			// Temporarily modify global variables, so HUD could draw correctly
			// each view is half width
			int oldViewwidth = viewwidth;
			viewwidth = viewwidth/2;
			// left
			setViewportLeft(renderer, bounds);
			setLeftEyeView(renderer, fov, ratio, fovratio*2);
			renderer.RenderOneEye(a1, toscreen); // False, to not swap yet
			// right
			// right view is offset to right
			int oldViewwindowx = viewwindowx;
			viewwindowx += viewwidth;
			setViewportRight(renderer, bounds);
			setRightEyeView(renderer, fov, ratio, fovratio*2);
			renderer.RenderOneEye(a1, false);
			//
			// SECOND PASS weapon sprite
			renderer.EndDrawScene(viewsector); // right view
			viewwindowx -= viewwidth;
			renderer.EndDrawScene(viewsector); // left view
			//
			// restore global state
			viewwidth = oldViewwidth;
			viewwindowx = oldViewwindowx;
			break;
		}

	case OCULUS_RIFT:
		{
			if ( (oculusTexture == NULL) || (! oculusTexture->checkSize(SCREENWIDTH, SCREENHEIGHT)) ) {
				if (oculusTexture)
					delete(oculusTexture);
				oculusTexture = new OculusTexture(SCREENWIDTH, SCREENHEIGHT);
			}
			if ( (hudTexture == NULL) || (! hudTexture->checkSize(SCREENWIDTH/2, SCREENHEIGHT)) ) {
				if (hudTexture)
					delete(hudTexture);
				hudTexture = new HudTexture(SCREENWIDTH/2, SCREENHEIGHT);
				hudTexture->bindToFrameBuffer();
				glClearColor(0, 0, 0, 0);
				glClear(GL_COLOR_BUFFER_BIT);
				hudTexture->unbind();
			}
			// Flush previous render - for some reason this way always has a good hud image
			/*
			if (!gl_draw_sync && toscreen)
			{
				All.Unclock();
				static_cast<OpenGLFrameBuffer*>(screen)->Swap();
				All.Clock();
			}
			*/
			// Render unwarped image to offscreen frame buffer
			oculusTexture->bindToFrameBuffer();
			// FIRST PASS - 3D
			// Temporarily modify global variables, so HUD could draw correctly
			// each view is half width
			int oldViewwidth = viewwidth;
			viewwidth = viewwidth/2;
			int oldScreenBlocks = screenblocks;
			screenblocks = 12; // full screen
			// left
			GL_IRECT riftBounds; // Always use full screen with Oculus Rift
			riftBounds.width = SCREENWIDTH;
			riftBounds.height = SCREENHEIGHT;
			riftBounds.left = 0;
			riftBounds.top = 0;
			setViewportLeft(renderer, &riftBounds);
			setLeftEyeView(renderer, oculusFov, ratio/2, fovratio, false);
			glEnable(GL_DEPTH_TEST);
			renderer.RenderOneEye(a1, false);
			// right
			// right view is offset to right
			int oldViewwindowx = viewwindowx;
			viewwindowx += viewwidth;
			setViewportRight(renderer, &riftBounds);
			setRightEyeView(renderer, oculusFov, ratio/2, fovratio, false);
			renderer.RenderOneEye(a1, false);
			//
			// SECOND PASS weapon sprite
			glEnable(GL_TEXTURE_2D);
			screenblocks = 12;
			float fullWidth = SCREENWIDTH / 2.0;
			viewwidth = RIFT_HUDSCALE * fullWidth;
			float left = (1.0 - RIFT_HUDSCALE) * fullWidth * 0.5; // left edge of scaled viewport
			// TODO Sprite needs some offset to appear at correct distance, rather than at infinity.
			int spriteOffsetX = (int)(0.021*fullWidth); // kludge to set weapon distance
			viewwindowx = left + fullWidth - spriteOffsetX;
			int oldViewwindowy = viewwindowy;
			int spriteOffsetY = (int)(0.00*viewheight); // nudge gun up/down
			// Counteract effect of status bar on weapon position
			if (oldScreenBlocks <= 10) { // lower weapon in status mode
				spriteOffsetY += 0.227 * viewwidth; // empirical - lines up brutal doom down sight in 1920x1080 Rift mode
			}
			viewwindowy += spriteOffsetY;
			renderer.EndDrawScene(viewsector); // right view
			viewwindowx = left + spriteOffsetX;
			renderer.EndDrawScene(viewsector); // left view
			// Third pass HUD
			screenblocks = max(oldScreenBlocks, 10); // Don't vignette main 3D view
			// Draw HUD again, to avoid flashing? - and render to screen
			blitHudTextureToScreen(true); // HUD pass now occurs in main doom loop! Since I delegated screen->Update to stereo3d.updateScreen().
			//
			// restore global state
			viewwidth = oldViewwidth;
			viewwindowx = oldViewwindowx;
			viewwindowy = oldViewwindowy;
			// Update orientation for NEXT frame, after expensive render has occurred this frame
			setViewDirection(renderer);
			// Set up 2D rendering to write to our hud renderbuffer
			hudTexture->bindToFrameBuffer();
			
			glClearColor(0, 0, 0, 0);
			glClear(GL_COLOR_BUFFER_BIT);
			
			int h = hudTexture->getHeight();
			int w = hudTexture->getWidth();
			glViewport(0, 0, w, h);
			/*
			// Experiment...
			adaptScreenSize = true;
			C_NewModeAdjust ();
			ST_LoadCrosshair (true);
			AM_NewResolution ();
			*/ // not helping~
			//
			break;
		}

	case LEFT_EYE_VIEW:
		setViewportFull(renderer, bounds);
		setLeftEyeView(renderer, fov, ratio, fovratio);
		renderer.RenderOneEye(a1, toscreen);
		renderer.EndDrawScene(viewsector);
		break;

	case RIGHT_EYE_VIEW:
		setViewportFull(renderer, bounds);
		setRightEyeView(renderer, fov, ratio, fovratio);
		renderer.RenderOneEye(a1, toscreen);
		renderer.EndDrawScene(viewsector);
		break;

	case QUAD_BUFFERED: // TODO - NOT TESTED! Run on a Quadro system with gl stereo enabled...
		setViewportFull(renderer, bounds);
		glGetBooleanv(GL_STEREO, &supportsStereo);
		glGetBooleanv(GL_DOUBLEBUFFER, &supportsBuffered);
		if (supportsStereo && supportsBuffered && toscreen)
		{ 
			// Right first this time, so more generic GL_BACK_LEFT will remain for other modes
			glDrawBuffer(GL_BACK_RIGHT);
			setRightEyeView(renderer, fov, ratio, fovratio);
			renderer.RenderOneEye(a1, toscreen);
			// Left
			glDrawBuffer(GL_BACK_LEFT);
			setLeftEyeView(renderer, fov, ratio, fovratio);
			renderer.RenderOneEye(a1, toscreen);
		} else { // mono view, in case hardware stereo is not supported
			setMonoView(renderer, fov, ratio, fovratio);
			renderer.RenderOneEye(a1, toscreen);			
		}
		renderer.EndDrawScene(viewsector);
		break;

	default:
		setViewportFull(renderer, bounds);
		setMonoView(renderer, fov, ratio, fovratio);
		renderer.RenderOneEye(a1, toscreen);			
		renderer.EndDrawScene(viewsector);
		break;

	}
}

void Stereo3D::updateScreen() {
	screen->Update();
	HudTexture * ht = Stereo3DMode.hudTexture;
	if ( (ht != NULL) && (ht->isBound()) ) {
		ht->unbind(); // restore drawing to real screen
		blitHudTextureToScreen();
		//
		ht->bindToFrameBuffer();
		viewwidth = ht->getWidth();
		viewwindowx = 0;
		viewwindowy = 0;
		glViewport(0, 0, ht->getWidth(), ht->getHeight());
	}
}

int Stereo3D::getScreenWidth() {
	if (adaptScreenSize && hudTexture && hudTexture->isBound())
		return hudTexture->getWidth();
	return screen->GetWidth();
}
int Stereo3D::getScreenHeight() {
	if (adaptScreenSize && hudTexture && hudTexture->isBound())
		return hudTexture->getHeight();
	return screen->GetHeight();
}

void Stereo3D::blitHudTextureToScreen(bool toscreen) {
	glEnable(GL_TEXTURE_2D);
	if (mode == OCULUS_RIFT) {
		bool useOculusTexture = true;
		if (oculusTexture)
			useOculusTexture = true;
		// First pass blit unwarped hudTexture into oculusTexture, in two places
		if (useOculusTexture)
			oculusTexture->bindToFrameBuffer();
		// Compute viewport coordinates
		float h = SCREENHEIGHT * RIFT_HUDSCALE * 1.20 / 2.0; // 1.20 pixel aspect
		float w = SCREENWIDTH/2 * RIFT_HUDSCALE;
		float x = (SCREENWIDTH/2-w)*0.5;
		float hudOffsetY = 0.01 * h; // nudge crosshair up
		hudOffsetY -= 0.005 * SCREENHEIGHT; // reverse effect of oculus head raising.
		if (screenblocks <= 10)
			hudOffsetY -= 0.080 * h; // lower crosshair when status bar is on
		float y = (SCREENHEIGHT-h)*0.5 + hudOffsetY; // offset to move cross hair up to correct spot
		int hudOffsetX = (int)(0.004*SCREENWIDTH/2); // kludge to set hud distance
		glViewport(x+hudOffsetX, y, w, h); // Not infinity, but not as close as the weapon.
		if (useOculusTexture || toscreen)
			hudTexture->renderToScreen();
		x += SCREENWIDTH/2;
		glViewport(x-hudOffsetX, y, w, h);
		if (useOculusTexture || toscreen)
			hudTexture->renderToScreen();
		// Second pass blit warped oculusTexture to screen
		if (oculusTexture && toscreen) {
			oculusTexture->unbind();
			glViewport(0, 0, SCREENWIDTH, SCREENHEIGHT);
			oculusTexture->renderToScreen();
			if (!gl_draw_sync && toscreen) {
			// if (gamestate != GS_LEVEL) { // TODO avoids flash by swapping at beginning of 3D render
				All.Unclock();
				static_cast<OpenGLFrameBuffer*>(screen)->Swap();
				All.Clock();
			}
		}
	}
	else { // TODO what else?
		if (toscreen) {
			glViewport(0, 0, SCREENWIDTH, SCREENHEIGHT);
			hudTexture->renderToScreen();
			// if (gamestate != GS_LEVEL) {
				static_cast<OpenGLFrameBuffer*>(screen)->Swap();
			// }
		}
	}
}

void Stereo3D::setMode(int m) {
	mode = static_cast<Mode>(m);
};

void Stereo3D::setMonoView(FGLRenderer& renderer, float fov, float ratio, float fovratio) {
	renderer.SetProjection(fov, ratio, fovratio, 0);
}

void Stereo3D::setLeftEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio, bool frustumShift) {
	renderer.SetProjection(fov, ratio, fovratio, vr_swap ? +vr_ipd/2 : -vr_ipd/2, frustumShift);
}

void Stereo3D::setRightEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio, bool frustumShift) {
	renderer.SetProjection(fov, ratio, fovratio, vr_swap ? -vr_ipd/2 : +vr_ipd/2, frustumShift);
}

void Stereo3D::setViewDirection(FGLRenderer& renderer) {
	// Set HMD angle parameters for NEXT frame
	static float previousYaw = 0;
	if (mode == OCULUS_RIFT) {
		if (oculusTracker == NULL) {
			oculusTracker = new OculusTracker();
			if (oculusTracker->isGood()) {
				// update cvars TODO
				const OVR::HMDInfo& info = oculusTracker->getInfo();
				vr_ipd = info.InterpupillaryDistance;
			}
		}
		if (oculusTracker->isGood()) {
			oculusTracker->update(); // get new orientation from headset.
			// Roll can be local, because it doesn't affect gameplay.
			renderer.mAngles.Roll = -oculusTracker->roll * 180.0 / 3.14159;
			// Yaw
			float yaw = oculusTracker->yaw;
			float dYaw = yaw - previousYaw;
			G_AddViewAngle(-32768.0*dYaw/3.14159); // determined empirically
			previousYaw = yaw;
			// Pitch
			int pitch = -32768/3.14159*oculusTracker->pitch;
			int dPitch = (pitch - viewpitch/65536); // empirical
			G_AddViewPitch(-dPitch);
			int x = 3;
		}
	}
}

// Normal full screen viewport
void Stereo3D::setViewportFull(FGLRenderer& renderer, GL_IRECT * bounds) {
	renderer.SetViewport(bounds);
}

// Left half of screen
void Stereo3D::setViewportLeft(FGLRenderer& renderer, GL_IRECT * bounds) {
	if (bounds) {
		GL_IRECT leftBounds;
		leftBounds.width = bounds->width / 2;
		leftBounds.height = bounds->height;
		leftBounds.left = bounds->left;
		leftBounds.top = bounds->top;
		renderer.SetViewport(&leftBounds);
	}
	else {
		renderer.SetViewport(bounds);
	}
}

// Right half of screen
void Stereo3D::setViewportRight(FGLRenderer& renderer, GL_IRECT * bounds) {
	if (bounds) {
		GL_IRECT rightBounds;
		rightBounds.width = bounds->width / 2;
		rightBounds.height = bounds->height;
		rightBounds.left = bounds->left + rightBounds.width;
		rightBounds.top = bounds->top;
		renderer.SetViewport(&rightBounds);
	}
	else {
		renderer.SetViewport(bounds);
	}
}


