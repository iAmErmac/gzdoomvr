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
	float fov_degrees;
	// float up_tan, down_tan, left_tan, right_tan; // could be look up in SDK 0.3.2

	float leftEyeCenterU() {
		return left_eye_center_u;
	}

	float lensSeparation() {
		return 0.0635;
	}

	float distortionScale() {
		float edgeRadius = 2.0 * (1.0 - lensSeparation() / width_meters);
		if (edgeRadius < 1) edgeRadius = 2.0 - edgeRadius; // other edge...
		float rSq = edgeRadius * edgeRadius;
		float scale = warp_k0 + 
			warp_k1 * rSq + 
			warp_k2 * rSq * rSq +
			warp_k3 * rSq * rSq * rSq;
		return scale;
	}

	float aspectRatio() {
		return 0.5 * width_meters / height_meters;
	}
};

// TODO - non-A cups
static RiftShaderParams dk1ShaderParams = {
	0.14976, // width_meters
	0.09360, // height_meters
	0.57599, // left eye center u// "(1 - 0.0635/0.14976)" // 1 - ipd(0.640)/width
	1.0, 0.220, 0.240, 0.000, // official parameters
	// Barrel (0.6,0) (0.5,0) | | (0.475,0) (0.45,0) (0.4,0) Pincushion
	// 1.0, 0.390, 0.100, 0.000, // warp parameters
	0.996, -0.004, 1.014, 0.0, // chromatic aberration parameters
	117.4 // horizontal fov
	// 2.1382618, 2.1382618, 2.1933062, 0.97073942
};

static RiftShaderParams dk2ShaderParams = {
	0.12576, // width_meters
	0.07074, // height_meters
	0.49507, // left eye center u // "(1 - 0.0635/0.12576)" // 1 - ipd(0.640)/width
	// Deduced empirically
	// K1/K2 Barrel .22/.24 |  |  Pincushion
	1.0, 0.090, 0.140, 0.000, // warp parameters
	0.986, -0.012, 1.019, 0.01, // chromatic aberration parameters
	94.3 // horizontal fov (106.1 vertical fov)
	// 1.3292863, 1.3292863, 1.0586575, 1.0923680
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
