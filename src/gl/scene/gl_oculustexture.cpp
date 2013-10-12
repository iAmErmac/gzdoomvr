#include "gl/scene/gl_oculustexture.h"
#include "gl/system/gl_system.h"
#include <cstring>

using namespace std;

static const char* vertexProgramString = ""
"#version 120\n"
"void main() {\n"
"	gl_Position = ftransform();\n"
"	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
"}\n";

static const char* fragmentProgramString = ""
"#version 120 \n"
" \n"
"uniform sampler2D texture; \n"
" \n"
"const float aspectRatio = 1.0; \n"
"const float distortionScale = 1.714; // TODO check this \n"
"const vec2 screenSize = vec2(0.14976, 0.0936);"
"const vec2 screenCenter = 0.5 * screenSize; \n"
"const vec2 lensCenter = vec2(0.57265, 0.5); // left eye \n"
"const vec2 scale = vec2(0.5/distortionScale, 0.5*aspectRatio/distortionScale); \n"
"const vec2 scaleIn = vec2(2.0, 2.0/aspectRatio); \n"
"const vec4 hmdWarpParam = vec4(1.0, 0.220, 0.240, 0.000); \n"
" \n"
"void main() { \n"
"   vec2 tcIn = gl_TexCoord[0].st; \n"
"   vec2 uv = vec2(tcIn.x*2, tcIn.y); // unwarped image coordinates (left eye) \n"
"   if (tcIn.x > 0.5) // right eye \n"
"       uv.x = 2 - 2*tcIn.x; \n"
"   vec2 theta = (uv - lensCenter) * scaleIn; \n"
"   float rSq = theta.x * theta.x + theta.y * theta.y; \n"
"   vec2 rvector = theta * ( hmdWarpParam.x + \n"
"                           hmdWarpParam.y * rSq + \n"
"                           hmdWarpParam.z * rSq * rSq + \n"
"                           hmdWarpParam.w * rSq * rSq * rSq); \n"
"   vec2 tc = vec2(0.5,0.5) + scale * rvector; \n"
"   if ( (abs(tc.x - 0.5) > 0.5) || (abs(tc.y - 0.5) > 0.5) ) { \n"
"        gl_FragColor = vec4(0, 0, 0, 1); \n"
"        return; \n"
"   } \n"
"   tc.x = 0.5 * tc.x; // because output only goes to 0-0.5 (left eye) \n"
"   if (tcIn.x > 0.5) // right eye 0.5-1.0 \n"
"        tc.x = 1 - tc.x; \n"
"    \n"
"   gl_FragColor = texture2D(texture, tc); \n"
"} \n";

OculusTexture::OculusTexture(int width, int height)
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
	init(width, height);
	// shader
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexProgramString, NULL);
	glCompileShader(vertexShader);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentProgramString, NULL);
	glCompileShader(fragmentShader);
	shader = glCreateProgram();
	glAttachShader(shader, vertexShader);
	glAttachShader(shader, fragmentShader);
	glLinkProgram(shader);
	GLsizei infoLength;
	GLchar infoBuffer[1001];
	glGetProgramInfoLog(shader, 1000, &infoLength, infoBuffer);
	if (*infoBuffer) {
		// error... TODO
	}
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

void OculusTexture::init(int width, int height) {
	// Framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	// Image texture
	glBindTexture(GL_TEXTURE_2D, renderedTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	// Poor filtering. Needed !
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);	
	// Depth bufer
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
	//
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderedTexture, 0);
	// shader - simple initially for testing
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexProgramString, NULL);
	glCompileShader(vertexShader);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentProgramString, NULL);
	glCompileShader(fragmentShader);
	shader = glCreateProgram();
	glAttachShader(shader, vertexShader);
	glAttachShader(shader, fragmentShader);
	glLinkProgram(shader);
	GLsizei infoLength;
	GLchar infoBuffer[1001];
	glGetProgramInfoLog(shader, 1000, &infoLength, infoBuffer);
	if (*infoBuffer) {
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
	// Load that texture we just rendered
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, renderedTexture);
	// Very simple draw routine maps texture onto entire screen; no glOrtho or whatever!
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glUseProgram(shader);
	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2d(0, 1); glVertex3f(-1,  1, 0.5);
		glTexCoord2d(1, 1); glVertex3f( 1,  1, 0.5);
		glTexCoord2d(0, 0); glVertex3f(-1, -1, 0.5);
		glTexCoord2d(1, 0); glVertex3f( 1, -1, 0.5);
	glEnd();
	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glBindTexture(GL_TEXTURE_2D, 0);
}

void OculusTexture::unbind() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

