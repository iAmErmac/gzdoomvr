#include "gl/scene/gl_oculustexture.h"
#include "gl/system/gl_system.h"
#include "gl/system/gl_cvars.h"
#include <cstring>
#include <string>
#include <sstream>

using namespace std;

EXTERN_CVAR(Bool, vr_sdkwarp)

static const char* vertexProgramString = ""
"#version 120\n"
"void main() {\n"
"	gl_Position = ftransform();\n"
"	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
"}\n";

static std::string createRiftFragmentShaderString(RiftShaderParams p) {
	stringstream ss;

	ss << "#version 120 \n";
	ss << " \n";
	ss << "uniform sampler2D texture; \n";
	ss << " \n";
	ss << "const float aspectRatio = " << p.aspectRatio() << "; \n";
	ss << "const float distortionScale = " << p.distortionScale() << "; \n";
	ss << "const vec2 lensCenter = vec2(" << p.leftEyeCenterU() << ", 0.5); // left eye \n";
	ss << "const vec2 inputCenter = vec2(0.5, 0.5); // I rendered center at center of unwarped image \n";
	ss << "const vec2 scale = vec2(0.5/distortionScale, 0.5*aspectRatio/distortionScale); \n";
	ss << "const vec2 scaleIn = vec2(2.0, 2.0/aspectRatio); \n";
	ss << "const vec4 hmdWarpParam = vec4(" << p.warp_k0 << ", " << p.warp_k1 << ", " << p.warp_k2 << ", " << p.warp_k3 << "); \n";
	ss << "const vec4 chromAbParam = vec4(" << p.ab_r0 << ", " << p.ab_r1 << ", " << p.ab_b0 << ", " << p.ab_b1 << "); \n";
	ss << " \n";
	ss << "void main() { \n";
	ss << "   vec2 tcIn = gl_TexCoord[0].st; \n";
	ss << "   vec2 uv = vec2(tcIn.x*2, tcIn.y); // unwarped image coordinates (left eye) \n";
	ss << "   if (tcIn.x > 0.5) // right eye \n";
	ss << "       uv.x = 2 - 2*tcIn.x; \n";
	ss << "   vec2 theta = (uv - lensCenter) * scaleIn; \n";
	ss << "   float rSq = theta.x * theta.x + theta.y * theta.y; \n";
	ss << "   vec2 rvector = theta * ( hmdWarpParam.x + \n";
	ss << "                            hmdWarpParam.y * rSq + \n";
	ss << "                            hmdWarpParam.z * rSq * rSq + \n";
	ss << "                            hmdWarpParam.w * rSq * rSq * rSq); \n";
	ss << "   // Chromatic aberration correction \n";
	ss << "   vec2 thetaBlue = rvector * (chromAbParam.z + chromAbParam.w * rSq); \n";
	ss << "   vec2 tcBlue = inputCenter + scale * thetaBlue; \n";
	ss << "   // Blue is farthest out \n";
	ss << "   if ( (abs(tcBlue.x - 0.5) > 0.5) || (abs(tcBlue.y - 0.5) > 0.5) ) { \n";
	ss << "        gl_FragColor = vec4(0, 0, 0, 1); \n";
	ss << "        return; \n";
	ss << "   } \n";
	ss << "   vec2 thetaRed = rvector * (chromAbParam.x + chromAbParam.y * rSq); \n";
	ss << "   vec2 tcRed = inputCenter + scale * thetaRed; \n";
	ss << "   vec2 tcGreen = inputCenter + scale * rvector; // green \n";
	ss << "   tcRed.x *= 0.5; // because output only goes to 0-0.5 (left eye) \n";
	ss << "   tcGreen.x *= 0.5; // because output only goes to 0-0.5 (left eye) \n";
	ss << "   tcBlue.x *= 0.5; // because output only goes to 0-0.5 (left eye) \n";
	ss << "   if (tcIn.x > 0.5) { // right eye 0.5-1.0 \n";
	ss << "        tcRed.x = 1 - tcRed.x; \n";
	ss << "        tcGreen.x = 1 - tcGreen.x; \n";
	ss << "        tcBlue.x = 1 - tcBlue.x; \n";
	ss << "   } \n";
	ss << "   float red = texture2D(texture, tcRed).r; \n";
	ss << "   float green = texture2D(texture, tcGreen).g; \n";
	ss << "   float blue = texture2D(texture, tcBlue).b; \n";
	ss << "   \n";
	ss << "   // Set alpha to 1.0, to counteract hall of mirror problem in complex alpha-blending situations.\n";
	ss << "   gl_FragColor = vec4(red, green, blue, 1.0); \n";
	ss << "} \n";

	return ss.str();
}

OculusTexture::OculusTexture(int width, int height, RiftShaderParams shaderParams)
	: w(width), h(height)
	, frameBuffer(0)
	, renderedTexture(0)
	, depthBuffer(0)
{
	// Framebuffer
	glGenFramebuffers(1, &frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	glGenTextures(1, &renderedTexture);
	glBindTexture(GL_TEXTURE_2D, renderedTexture);
	glGenRenderbuffers(1, &depthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);

	init(width, height, shaderParams);

	// clean up
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

bool OculusTexture::checkSize(int width, int height) {
	if ((w == width) && (h == height))
		return true; // no change
	return false;
}

void OculusTexture::destroy() {
	glDeleteProgram(shader);
	shader = 0;
	glDeleteShader(vertexShader);
	vertexShader = 0;
	glDeleteShader(fragmentShader);
	fragmentShader = 0;
	glDeleteRenderbuffers(1, &depthBuffer);
	depthBuffer = 0;
	glDeleteTextures(1, &renderedTexture);
	renderedTexture = 0;
	glDeleteFramebuffers(1, &frameBuffer);
	frameBuffer = 0;
}

void OculusTexture::init(int width, int height, RiftShaderParams params) {
	// Framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	// Image texture
	glBindTexture(GL_TEXTURE_2D, renderedTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	// Poor filtering. Needed (?)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);	
	// Depth bufer
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
	//
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderedTexture, 0);
	// shader
	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexProgramString, NULL);
	glCompileShader(vertexShader);
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	// TODO - choose string based on attached device
	std::string fragmentProgramString = createRiftFragmentShaderString(params);
	const char* fragmentProgramCString = fragmentProgramString.c_str();
	glShaderSource(fragmentShader, 1, &fragmentProgramCString, NULL);

	glCompileShader(fragmentShader);
	shader = glCreateProgram();
	glAttachShader(shader, vertexShader);
	glAttachShader(shader, fragmentShader);
	glLinkProgram(shader);
	GLsizei infoLength;
	GLchar infoBuffer[1001];
	glGetProgramInfoLog(shader, 1000, &infoLength, infoBuffer);
	if (*infoBuffer) {
		fprintf(stderr, "%s", infoBuffer);
		// error... TODO
	}
	// clean up
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void OculusTexture::bindToFrameBuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderedTexture, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
	glViewport(0, 0, w, h);
}

void OculusTexture::renderToScreen() {
	bool useShader = true;
	// glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); // Makes HOM -> black May 2014
	// Load that texture we just rendered
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND); // Improves color of alpha-sprites in no-shader mode; but does not solve HOM defect.
	glBindTexture(GL_TEXTURE_2D, renderedTexture);
	// Very simple draw routine maps texture onto entire screen; no glOrtho or whatever!
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	if (useShader) {
		glUseProgram(shader);
	}
	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2d(0, 1); glVertex3f(-1,  1, 0.5);
		glTexCoord2d(1, 1); glVertex3f( 1,  1, 0.5);
		glTexCoord2d(0, 0); glVertex3f(-1, -1, 0.5);
		glTexCoord2d(1, 0); glVertex3f( 1, -1, 0.5);
	glEnd();
	if (useShader) {
		glUseProgram(0);
	}
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glBindTexture(GL_TEXTURE_2D, 0);
	glEnable(GL_BLEND);
}

void OculusTexture::unbind() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

