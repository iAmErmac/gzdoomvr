#ifndef GZDOOM_GL_STEREO3D_H_
#define GZDOOM_GL_STEREO3D_H_

#include "gl/renderer/gl_renderer.h"

/**
 * Stereo3D class controls OpenGL viewport and projection matrix,
 * to drive various stereoscopic modes.
 */
class Stereo3D {
public:
	enum Mode {
		MONO = 0,
		GREEN_MAGENTA = 1,
		RED_CYAN = 2,
		SIDE_BY_SIDE = 3,
		LEFT_EYE_VIEW = 4,
		RIGHT_EYE_VIEW = 5,
		QUAD_BUFFERED = 6,
		ROW_INTERLEAVED = 7, // TODO
		CHECKERBOARD = 8, // TODO
		OCULUS_RIFT = 9 // TODO
	};

	Stereo3D();

	void render(FGLRenderer& renderer, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool toscreen);

	void setMode(int m);

private:

	void setMonoView(FGLRenderer& renderer, float fov, float ratio, float fovratio);
	void setLeftEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio);
	void setRightEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio);
	void setViewportFull(FGLRenderer& renderer, GL_IRECT * bounds);
	void setViewportLeft(FGLRenderer& renderer, GL_IRECT * bounds);
	void setViewportRight(FGLRenderer& renderer, GL_IRECT * bounds);

	Mode mode;
};

#endif // GZDOOM_GL_STEREO3D_H_

