#ifndef GZDOOM_GL_STEREO3D_H_
#define GZDOOM_GL_STEREO3D_H_

#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_hudtexture.h"
#include "gl/scene/gl_oculustexture.h"
#include "gl/scene/gl_oculustracker.h"

/**
 * Stereo3D class controls OpenGL viewport and projection matrix,
 * to drive various stereoscopic modes.
 */
class Stereo3D {
public:
	enum Mode {
		MONO = 0,
		GREEN_MAGENTA,
		RED_CYAN,
		SIDE_BY_SIDE,
		SIDE_BY_SIDE_SQUISHED,
		LEFT_EYE_VIEW,
		RIGHT_EYE_VIEW,
		QUAD_BUFFERED,
		OCULUS_RIFT,
		ROW_INTERLEAVED, // TODO
		CHECKERBOARD // TODO
	};

	Stereo3D();

	void render(FGLRenderer& renderer, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool toscreen, sector_t * viewsector);
	void setViewDirection(FGLRenderer& renderer);
	void setMode(int m);

private:

	void setMonoView(FGLRenderer& renderer, float fov, float ratio, float fovratio);
	void setLeftEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio, bool frustumShift=true);
	void setRightEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio, bool frustumShift=true);
	void setViewportFull(FGLRenderer& renderer, GL_IRECT * bounds);
	void setViewportLeft(FGLRenderer& renderer, GL_IRECT * bounds);
	void setViewportRight(FGLRenderer& renderer, GL_IRECT * bounds);

	Mode mode;
	HudTexture* hudTexture;
	OculusTexture* oculusTexture;
	OculusTracker* oculusTracker;
};

#endif // GZDOOM_GL_STEREO3D_H_

