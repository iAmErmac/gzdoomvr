#ifndef GZDOOM_GL_OCULUSTEXTURE_H_
#define GZDOOM_GL_OCULUSTEXTURE_H_

struct RiftShaderParams {
	float width_meters;
	float height_meters;
	float left_eye_center_u;
	float warp_k0;
	float warp_k1;
	float warp_k2;
	float warp_k3;
	float ab_r0;
	float ab_r1;
	float ab_b0;
	float ab_b1;
};

static RiftShaderParams dk1ShaderParams = {
	0.14976, // width_meters
	0.09360, // height_meters
	0.57599, // left eye center u// "(1 - 0.0635/0.14976)" // 1 - ipd(0.640)/width
	1.0, 0.220, 0.240, 0.000, // official parameters
	// Barrel (0.6,0) (0.5,0) | | (0.475,0) (0.45,0) (0.4,0) Pincushion
	// 1.0, 0.390, 0.100, 0.000, // warp parameters
	0.996, -0.004, 1.014, 0.0 // chromatic aberration parameters
};

static RiftShaderParams dk2ShaderParams = {
	0.12576, // width_meters
	0.07074, // height_meters
	0.49507, // left eye center u // "(1 - 0.0635/0.12576)" // 1 - ipd(0.640)/width
	// Deduced empirically
	// K1 Barrel 0.220 | 0.100 | 0.000 Pincushion
	// K2 Barrel 0.190 | 0.150 | 0.100 0.00 Pincushion
	1.0, 0.100, 0.150, 0.000, // warp parameters
	0.986, -0.012, 1.019, 0.01 // chromatic aberration parameters
};

// Framebuffer texture for intermediate rendering of Oculus Rift image
class OculusTexture {
public:
	OculusTexture(int width, int height, RiftShaderParams);
	~OculusTexture() {destroy();}
	void bindToFrameBuffer();
	bool checkSize(int width, int height); // is the size still the same?
	void renderToScreen();
	void unbind();

private:
	void init(int width, int height, RiftShaderParams);
	void destroy(); // release all opengl resources

	unsigned int w, h;
	unsigned int frameBuffer;
	unsigned int renderedTexture;
	unsigned int depthBuffer;
	unsigned int shader;
	unsigned int vertexShader;
	unsigned int fragmentShader;
};

#endif // GZDOOM_GL_OCULUSTEXTURE_H_
