#ifndef GZDOOM_GL_STEREO3D_H_
#define GZDOOM_GL_STEREO3D_H_

class Stereo3D {
public:
	enum Mode {
		MONO,
		GREEN_MAGENTA
	};

	Stereo3D() 
		: Mode(MONO)
	{}

	void render() {
		switch(mode) {
		case MONO:
			{
				setMonoView();
				renderScene();
			}
			break;
		case GREEN_MAGENTA:
			{
			}
			break;
		}
	}

private:
	Mode mode;
};

#endif // GZDOOM_GL_STEREO3D_H_

