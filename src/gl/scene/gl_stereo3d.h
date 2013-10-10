#ifndef GZDOOM_GL_STEREO3D_H_
#define GZDOOM_GL_STEREO3D_H_

#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_colormask.h"

class Stereo3D {
public:
	enum Mode {
		MONO,
		GREEN_MAGENTA,
		RED_CYAN,
		SIDE_BY_SIDE,
		LEFT_EYE_VIEW,
		RIGHT_EYE_VIEW,
		QUAD_BUFFERED,
		ROW_INTERLEAVED, // TODO
		CHECKERBOARD, // TODO
		OCULUS_RIFT // TODO
	};

	float iod; // interocular distance
	bool swap; // switch eyes

	Stereo3D() 
		: mode(MONO)
		, iod(2.0) // intraocular distance in doom units; 1 doom unit = about 3 cm
		, swap(false)
	{}

	void render(FGLRenderer& renderer, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool toscreen) 
	{
		angle_t a1 = renderer.FrustumAngle();

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
			GLboolean supportsStereo = false;
			GLboolean supportsBuffered = false;
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

	void setMode(int m) {
		mode = static_cast<Mode>(m);
	};

private:

	void setMonoView(FGLRenderer& renderer, float fov, float ratio, float fovratio) {
		renderer.SetProjection(fov, ratio, fovratio, 0);
	}

	void setLeftEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio) {
		renderer.SetProjection(fov, ratio, fovratio, swap ? +iod/2 : -iod/2);
	}

	void setRightEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio) {
		renderer.SetProjection(fov, ratio, fovratio, swap ? -iod/2 : +iod/2);
	}

	void setViewportFull(FGLRenderer& renderer, GL_IRECT * bounds) {
		renderer.SetViewport(bounds);
	}

	void setViewportLeft(FGLRenderer& renderer, GL_IRECT * bounds) {
		renderer.SetViewport(bounds, 2, 0);
	}

	void setViewportRight(FGLRenderer& renderer, GL_IRECT * bounds) {
		renderer.SetViewport(bounds, 2, 1);
	}

	Mode mode;
};

#endif // GZDOOM_GL_STEREO3D_H_

