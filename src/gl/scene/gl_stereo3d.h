#ifndef GZDOOM_GL_STEREO3D_H_
#define GZDOOM_GL_STEREO3D_H_

#include "gl/renderer/gl_renderer.h"

class Stereo3D {
public:
	enum Mode {
		MONO,
		GREEN_MAGENTA
	};

	Stereo3D() 
		: mode(GREEN_MAGENTA)
		, iod(2.0) // intraocular distance in doom units; 1 doom unit = about 3 cm
	{}

	void render(FGLRenderer& renderer, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool toscreen) 
	{
		angle_t a1 = renderer.FrustumAngle();
		renderer.SetViewport(bounds);

		switch(mode) {
		case MONO:
			{
				setMonoView(renderer, fov, ratio, fovratio);
				renderer.RenderOneEye(a1, toscreen);			}
			break;
		case GREEN_MAGENTA:
			{ // Local scope for color mask

				// Left eye green
				LocalScopeGLColorMask colorMask(false, true, false, true); // green
				renderer.SetProjection(fov, ratio, fovratio, -iod/2);	// switch to perspective mode and set up clipper
				renderer.RenderOneEye(a1, toscreen);

				// Right eye magenta
				colorMask.setColorMask(true, false, true, true); // magenta
				renderer.SetProjection(fov, ratio, fovratio, +iod/2);	// switch to perspective mode and set up clipper
				renderer.RenderOneEye(a1, toscreen);
				break;
			} // close scope to auto-revert glColorMask
		}
	}

private:

	void setMonoView(FGLRenderer& renderer, float fov, float ratio, float fovratio) {
		renderer.SetProjection(fov, ratio, fovratio, 0);
	}

	void setLeftEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio) {
		renderer.SetProjection(fov, ratio, fovratio, 0);
	}

	void setRightEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio) {
		renderer.SetProjection(fov, ratio, fovratio, 0);
	}

	Mode mode;
	float iod;
};

#endif // GZDOOM_GL_STEREO3D_H_

