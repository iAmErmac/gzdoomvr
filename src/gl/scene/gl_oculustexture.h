#ifndef GZDOOM_GL_OCULUSTEXTURE_H_
#define GZDOOM_GL_OCULUSTEXTURE_H_

// Framebuffer texture for intermediate rendering of Oculus Rift image
class OculusTexture {
public:
	OculusTexture(int width, int height);
	void bindToFrameBuffer();
	bool checkSize(int width, int height);
	void renderToScreen();
	void unbind();

private:
	void init(int width, int height);
	unsigned int w, h;
	unsigned int frameBuffer;
	unsigned int renderedTexture;
	unsigned int depthBuffer;
	unsigned int shader;
};

#endif // GZDOOM_GL_OCULUSTEXTURE_H_
