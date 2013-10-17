#ifndef GZDOOM_GL_HudTEXTURE_H_
#define GZDOOM_GL_HudTEXTURE_H_

// Framebuffer texture for intermediate rendering of Hud image
class HudTexture {
public:
	HudTexture(int width, int height);
	~HudTexture() {destroy();}
	void bindToFrameBuffer();
	bool checkSize(int width, int height); // is the size still the same?
	void renderToScreen();
	void unbind();

private:
	void init(int width, int height);
	void destroy(); // release all opengl resources

	unsigned int w, h;
	unsigned int frameBuffer;
	unsigned int renderedTexture;
};

#endif // GZDOOM_GL_HudTEXTURE_H_
