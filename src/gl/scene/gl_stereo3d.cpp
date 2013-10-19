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

EXTERN_CVAR(Bool, gl_draw_sync)
//
CVAR(Int, st3d_mode, 0, CVAR_GLOBALCONFIG)
CVAR(Bool, st3d_swap, false, CVAR_GLOBALCONFIG)
// intraocular distance in doom units; 1 doom unit = about 3 cm
CVAR(Float, st3d_iod, 0.062f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // METERS

Stereo3D::Stereo3D() 
	: mode(MONO)
	, oculusTexture(NULL)
	, hudTexture(NULL)
	, oculusTracker(NULL)
{}

void Stereo3D::render(FGLRenderer& renderer, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool toscreen, sector_t * viewsector) 
{
	setMode(st3d_mode);
	if (hudTexture)
		hudTexture->unbind();

	GLboolean supportsStereo = false;
	GLboolean supportsBuffered = false;
	// Calibrate oculusFov by slowly yawing view. 
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
			// Flush previous render
			if (!gl_draw_sync && toscreen)
			{
				All.Unclock();
				static_cast<OpenGLFrameBuffer*>(screen)->Swap();
				All.Clock();
			}
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
			float fullWidth = SCREENWIDTH / 2.0;
			float weaponScale = 0.33;
			viewwidth = weaponScale * fullWidth;
			float left = (1.0 - weaponScale) * fullWidth * 0.5; // left edge of scaled viewport
			// TODO Sprite needs some offset to appear at correct distance, rather than at infinity.
			int spriteOffsetX = (int)(0.021*fullWidth); // kludge to set weapon distance
			viewwindowx = left + fullWidth - spriteOffsetX;
			int oldViewwindowy = viewwindowy;
			int spriteOffsetY = (int)(-0.00*viewheight); // nudge gun up/down
			// Counteract effect of status bar on weapon position
			if (oldScreenBlocks <= 10) { // lower weapon in status mode
				spriteOffsetY += 0.305 * viewwidth; // empirical
			}
			viewwindowy += spriteOffsetY;
			renderer.EndDrawScene(viewsector); // right view
			viewwindowx = left + spriteOffsetX;
			renderer.EndDrawScene(viewsector); // left view
			//
			// Third pass HUD
			glEnable(GL_TEXTURE_2D);
			float hudScale = weaponScale;
			float h = SCREENHEIGHT * hudScale * 1.20 / 2.0; // 1.20 pixel aspect
			float w = SCREENWIDTH/2 * hudScale;
			float x = (SCREENWIDTH/2-w)*0.5;
			float hudOffsetY = 0.01 * h; // nudge crosshair up
			if (oldScreenBlocks <= 10)
				hudOffsetY -= 0.080 * h; // lower crosshair when status bar is on
			float y = (SCREENHEIGHT-h)*0.5 + hudOffsetY; // offset to move cross hair up to correct spot
			glViewport(x+spriteOffsetX/5, y, w, h); // Not infinity, but not as close as the weapon.
			hudTexture->renderToScreen();
			x += SCREENWIDTH/2;
			glViewport(x-spriteOffsetX/5, y, w, h);
			hudTexture->renderToScreen();
			//
			// restore global state
			viewwidth = oldViewwidth;
			viewwindowx = oldViewwindowx;
			viewwindowy = oldViewwindowy;
			// Warp offscreen framebuffer to screen
			oculusTexture->unbind();
			glViewport(0, 0, SCREENWIDTH, SCREENHEIGHT);
			oculusTexture->renderToScreen();
			// Update orientation for NEXT frame, after expensive render has occurred this frame
			setViewDirection(renderer);
			// TODO - experiment with setting size of status bar, menus, etc.
			h = SCREENHEIGHT;
			w = SCREENWIDTH/2;
			hudTexture->bindToFrameBuffer();
			glClearColor(0, 0, 0, 0);
			glClear(GL_COLOR_BUFFER_BIT);
			// glViewport((1.0-hudScale)*0.5*SCREENWIDTH, (SCREENHEIGHT-sh)*0.5, sw, sh); // Render HUD stuff to left eye, smaller
			glViewport(0, 0, w, h);
			screenblocks = max(oldScreenBlocks, 10); // Don't vignette main 3D view
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

void Stereo3D::setMode(int m) {
	mode = static_cast<Mode>(m);
};

void Stereo3D::setMonoView(FGLRenderer& renderer, float fov, float ratio, float fovratio) {
	renderer.SetProjection(fov, ratio, fovratio, 0);
}

void Stereo3D::setLeftEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio, bool frustumShift) {
	renderer.SetProjection(fov, ratio, fovratio, st3d_swap ? +st3d_iod/2 : -st3d_iod/2, frustumShift);
}

void Stereo3D::setRightEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio, bool frustumShift) {
	renderer.SetProjection(fov, ratio, fovratio, st3d_swap ? -st3d_iod/2 : +st3d_iod/2, frustumShift);
}

void Stereo3D::setViewDirection(FGLRenderer& renderer) {
	// Set HMD angle parameters for NEXT frame
	static float previousYaw = 0;
	if (mode == OCULUS_RIFT) {
		if (oculusTracker == NULL) {
			oculusTracker = new OculusTracker();
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


