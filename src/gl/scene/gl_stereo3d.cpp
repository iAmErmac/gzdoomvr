#include "gl/system/gl_system.h"
#include "gl/scene/gl_stereo3d.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_colormask.h"
#include "doomstat.h"

CVAR(Int, st3d_mode, 0, CVAR_GLOBALCONFIG)
CVAR(Bool, st3d_swap, false, CVAR_GLOBALCONFIG)
CVAR(Float, st3d_screendist, 20.0f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
// intraocular distance in doom units; 1 doom unit = about 3 cm
CVAR(Float, st3d_iod, 2.0f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

Stereo3D::Stereo3D() 
	: mode(MONO)
{}

void Stereo3D::render(FGLRenderer& renderer, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool toscreen) 
{
	setMode(st3d_mode);

	angle_t a1 = renderer.FrustumAngle();
	GLboolean supportsStereo = false;
	GLboolean supportsBuffered = false;

	switch(mode) 
	{

	case MONO:
		setViewportFull(renderer, bounds);
		setMonoView(renderer, fov, ratio, fovratio);
		renderer.RenderOneEye(a1, toscreen);			
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
			break;
		} // close scope to auto-revert glColorMask

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
			break;
		} // close scope to auto-revert glColorMask

	case SIDE_BY_SIDE:
		// TODO - hud needs to be split too...
		// left
		setViewportLeft(renderer, bounds);
		setLeftEyeView(renderer, fov, ratio/2, fovratio);
		renderer.RenderOneEye(a1, toscreen);
		// right
		setViewportRight(renderer, bounds);
		setRightEyeView(renderer, fov, ratio/2, fovratio);
		renderer.RenderOneEye(a1, toscreen);
		//
		break;

	case LEFT_EYE_VIEW:
		setViewportFull(renderer, bounds);
		setLeftEyeView(renderer, fov, ratio, fovratio);
		renderer.RenderOneEye(a1, toscreen);
		break;

	case RIGHT_EYE_VIEW:
		setViewportFull(renderer, bounds);
		setRightEyeView(renderer, fov, ratio, fovratio);
		renderer.RenderOneEye(a1, toscreen);
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
			break;
		} else { // mono view, in case hardware stereo is not supported
			setMonoView(renderer, fov, ratio, fovratio);
			renderer.RenderOneEye(a1, toscreen);			
			break;
		}

	default:
		setViewportFull(renderer, bounds);
		setMonoView(renderer, fov, ratio, fovratio);
		renderer.RenderOneEye(a1, toscreen);			
		break;

	}
}

void Stereo3D::setMode(int m) {
	mode = static_cast<Mode>(m);
};

void Stereo3D::setMonoView(FGLRenderer& renderer, float fov, float ratio, float fovratio) {
	renderer.SetProjection(fov, ratio, fovratio, 0);
}

void Stereo3D::setLeftEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio) {
	renderer.SetProjection(fov, ratio, fovratio, st3d_swap ? +st3d_iod/2 : -st3d_iod/2);
}

void Stereo3D::setRightEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio) {
	renderer.SetProjection(fov, ratio, fovratio, st3d_swap ? -st3d_iod/2 : +st3d_iod/2);
}

void Stereo3D::setViewportFull(FGLRenderer& renderer, GL_IRECT * bounds) {
	renderer.viewport_offsetx = 0;
	renderer.viewport_scalex = 1;
	renderer.SetViewport(bounds);
}

void Stereo3D::setViewportLeft(FGLRenderer& renderer, GL_IRECT * bounds) {
	renderer.viewport_offsetx = 0;
	renderer.viewport_scalex = 2;
	renderer.SetViewport(bounds);
}

void Stereo3D::setViewportRight(FGLRenderer& renderer, GL_IRECT * bounds) {
	renderer.viewport_offsetx = 1;
	renderer.viewport_scalex = 2;
	renderer.SetViewport(bounds);
}


