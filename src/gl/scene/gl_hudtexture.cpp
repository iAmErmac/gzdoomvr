#include "gl/scene/gl_hudtexture.h"
#include "gl/system/gl_system.h"
#include <cstring>

using namespace std;

HudTexture::HudTexture(int width, int height)
	: w(width), h(height)
	, frameBuffer(0)
	, renderedTexture(0)
{
	// Framebuffer
	glGenFramebuffers(1, &frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	glGenTextures(1, &renderedTexture);
	glBindTexture(GL_TEXTURE_2D, renderedTexture);
	init(width, height);
	// clean up
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

bool HudTexture::checkSize(int width, int height) {
	if ((w == width) && (h == height))
		return true; // no change
	return false;
}

void HudTexture::destroy() {
	glDeleteTextures(1, &renderedTexture);
	renderedTexture = 0;
	glDeleteFramebuffers(1, &frameBuffer);
	frameBuffer = 0;
}

void HudTexture::init(int width, int height) {
	// Framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	// Image texture
	glBindTexture(GL_TEXTURE_2D, renderedTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	// Poor filtering. Needed !
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);	
	//
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderedTexture, 0);
	// clean up
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void HudTexture::bindToFrameBuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderedTexture, 0);
	glViewport(0, 0, w, h);
}

void HudTexture::renderToScreen() {
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
	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2d(0, 1); glVertex3f(-1,  1, 0.5);
		glTexCoord2d(1, 1); glVertex3f( 1,  1, 0.5);
		glTexCoord2d(0, 0); glVertex3f(-1, -1, 0.5);
		glTexCoord2d(1, 0); glVertex3f( 1, -1, 0.5);
	glEnd();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glBindTexture(GL_TEXTURE_2D, 0);
}

void HudTexture::unbind() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

