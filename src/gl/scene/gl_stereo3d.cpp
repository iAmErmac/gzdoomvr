#include "gl/system/gl_system.h"
#include "gl/scene/gl_stereo3d.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_colormask.h"
#include "doomstat.h"

CVAR(Int, st3d_mode, 0, CVAR_GLOBALCONFIG)
CVAR(Bool, st3d_swap, false, CVAR_GLOBALCONFIG)
// intraocular distance in doom units; 1 doom unit = about 3 cm
CVAR(Float, st3d_iod, 0.062f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // METERS

Stereo3D::Stereo3D() 
	: mode(MONO)
	, oculusTexture(NULL)
{}

void Stereo3D::render(FGLRenderer& renderer, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool toscreen, sector_t * viewsector) 
{
	setMode(st3d_mode);

	GLboolean supportsStereo = false;
	GLboolean supportsBuffered = false;
	float oculusFov = 90 * fovratio; // Hard code probably wider fov for oculus
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
				oculusTexture = new OculusTexture(SCREENWIDTH, SCREENHEIGHT);
			}
			// Render unwarped image to offscreen frame buffer
			oculusTexture->bindToFrameBuffer();
			// FIRST PASS - 3D
			// Temporarily modify global variables, so HUD could draw correctly
			// each view is half width
			int oldViewwidth = viewwidth;
			viewwidth = viewwidth/2;
			// left
			setViewportLeft(renderer, bounds);
			setLeftEyeView(renderer, oculusFov, ratio/2, fovratio, false);
			renderer.RenderOneEye(a1, toscreen);
			// right
			// right view is offset to right
			int oldViewwindowx = viewwindowx;
			viewwindowx += viewwidth;
			setViewportRight(renderer, bounds);
			setRightEyeView(renderer, oculusFov, ratio/2, fovratio, false);
			renderer.RenderOneEye(a1, false);
			//
			// SECOND PASS weapon sprite
			// TODO Sprite needs some offset to appear at correct distance
			renderer.EndDrawScene(viewsector); // right view
			viewwindowx -= viewwidth;
			renderer.EndDrawScene(viewsector); // left view
			//
			// restore global state
			viewwidth = oldViewwidth;
			viewwindowx = oldViewwindowx;
			// Warp offscreen framebuffer to screen
			oculusTexture->unbind();
			oculusTexture->renderToScreen();
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


