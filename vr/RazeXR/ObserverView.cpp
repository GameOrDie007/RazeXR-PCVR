/*
	PC branch: the observer view - the picture on the desktop window, for
	streams and recordings.

	It shows everything the player sees. In a level it is a cut-out of the
	left eye that follows the head's turns but not its wobble, turned so the
	horizon stays level: the raw eye shakes with every small movement of the
	head and tilts with every lean, and nobody can watch that for long. With
	the pause menu up the cut-out sits still and upright, and the menu is laid
	over it, as the headset shows it. The main menu, the splash and every
	other full-screen picture fill the window at the shape they were laid out
	in.

	vr_observer_steady 0 keeps the plain, centred cut-out in play too.

	Drawn as a textured quad, never blitted: a desktop window with
	multisampling refuses a blit into it without a visible error, and a
	rotated cut-out is not a blit anyway. The eye and the menu are resolved
	into textures of our own first, because both are multisample renderbuffers
	and the swapchain images belong to the runtime once released. The GL state
	the engine tracks is saved and put back around all of it.

	The window itself is the engine's: borderless and the size of the monitor
	by default (vid_fullscreen), Alt+Enter for a window and back, remembered.
	Neither rebuilds the GL context the session holds - both only restyle the
	window - so the headset is untouched.

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation; either version 2 of the License, or (at your option)
	any later version.
*/

#include "printf.h"
#include "c_cvars.h"

#include "VrCommon.h"

#include <math.h>
#include <chrono>

CVAR(Bool, vr_observer_steady, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

void TBXR_Log(const char *fmt, ...);	// TBXR_PC.cpp, into razexr_vr.log
#define VR_Log TBXR_Log

#ifndef GL_TEXTURE_SRGB_DECODE_EXT
#define GL_TEXTURE_SRGB_DECODE_EXT 0x8A48
#define GL_SKIP_DECODE_EXT 0x8A4A
#endif
float VR_MenuAspect();
void VR_Get2DMetrics(int* canvasW, int* canvasH, int* viewW, int* viewH);

#include "ObserverCrop.h"

namespace {

using namespace observer;

Steady gSteady;

//==========================================================================
//
// Drawing.
//
//==========================================================================

GLuint gProgram = 0, gVao = 0;
GLint gURect = -1, gUUV = -1, gUTex = -1;
bool gBroken = false;	// the shader would not build: the caller keeps its blit

struct Target { GLuint fbo = 0, tex = 0; int w = 0, h = 0; };
Target gEyeCopy, gMenuCopy;

const char* kVertex =
	"#version 330 core\n"
	"uniform vec4 uRect;\n"			// x0 y0 x1 y1, normalised device coordinates
	"uniform vec2 uUV[4];\n"		// top left, top right, bottom right, bottom left
	"out vec2 vUV;\n"
	"void main()\n"
	"{\n"
	"	int i = gl_VertexID;\n"		// a strip: top left, top right, bottom left, bottom right
	"	vec2 p = vec2((i == 1 || i == 3) ? uRect.z : uRect.x, (i < 2) ? uRect.w : uRect.y);\n"
	"	int c = (i == 0) ? 0 : (i == 1) ? 1 : (i == 2) ? 3 : 2;\n"
	"	vUV = uUV[c];\n"
	"	gl_Position = vec4(p, 0.0, 1.0);\n"
	"}\n";

const char* kFragment =
	"#version 330 core\n"
	"uniform sampler2D uTex;\n"
	"in vec2 vUV;\n"
	"out vec4 fragColor;\n"
	"void main() { fragColor = texture(uTex, vUV); }\n";

GLuint Compile(GLenum type, const char* src)
{
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		char log[1024] = {};
		glGetShaderInfoLog(s, sizeof(log), nullptr, log);
		VR_Log("VR: observer view shader failed: %s\n", log);
		glDeleteShader(s);
		return 0;
	}
	return s;
}

bool Ready()
{
	if (gProgram) return true;
	if (gBroken) return false;
	const GLuint vs = Compile(GL_VERTEX_SHADER, kVertex);
	const GLuint fs = Compile(GL_FRAGMENT_SHADER, kFragment);
	if (!vs || !fs) { gBroken = true; return false; }
	gProgram = glCreateProgram();
	glAttachShader(gProgram, vs);
	glAttachShader(gProgram, fs);
	glLinkProgram(gProgram);
	glDeleteShader(vs);
	glDeleteShader(fs);
	GLint ok = 0;
	glGetProgramiv(gProgram, GL_LINK_STATUS, &ok);
	if (!ok)
	{
		VR_Log("VR: observer view program would not link\n");
		glDeleteProgram(gProgram);
		gProgram = 0;
		gBroken = true;
		return false;
	}
	gURect = glGetUniformLocation(gProgram, "uRect");
	gUUV = glGetUniformLocation(gProgram, "uUV");
	gUTex = glGetUniformLocation(gProgram, "uTex");
	glGenVertexArrays(1, &gVao);
	VR_Log("VR: observer view ready\n");
	return true;
}

// A texture of our own the size of the source, and the multisample source
// resolved into it.
bool CopyFrom(Target& t, GLuint srcFbo, int w, int h)
{
	if (t.w != w || t.h != h)
	{
		if (t.fbo) glDeleteFramebuffers(1, &t.fbo);
		if (t.tex) glDeleteTextures(1, &t.tex);
		t = Target();
		glGenTextures(1, &t.tex);
		glBindTexture(GL_TEXTURE_2D, t.tex);
		/*
			The source's own format: a resolve from multisample needs the two
			to match exactly. The engine writes its colours into these sRGB
			buffers unconverted, so they are read back unconverted too.
		*/
		glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SRGB_DECODE_EXT, GL_SKIP_DECODE_EXT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glGenFramebuffers(1, &t.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
		const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			VR_Log("VR: observer view copy incomplete: 0x%x\n", status);
			glDeleteFramebuffers(1, &t.fbo);
			glDeleteTextures(1, &t.tex);
			t = Target();
			return false;
		}
		t.w = w; t.h = h;
	}
	glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, t.fbo);
	glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	return true;
}

// One textured quad: the picture's corners (y down) onto a rectangle of the
// window, in pixels from its top left.
void Quad(GLuint tex, const Crop& c, float x0, float y0, float x1, float y1, int winW, int winH)
{
	float uv[8];
	for (int i = 0; i < 4; i++)
	{
		uv[i * 2] = c.x[i];
		uv[i * 2 + 1] = 1.0f - c.y[i];	// GL textures run bottom up
	}
	glUniform4f(gURect, x0 / winW * 2.0f - 1.0f, 1.0f - y1 / winH * 2.0f,
		x1 / winW * 2.0f - 1.0f, 1.0f - y0 / winH * 2.0f);
	glUniform2fv(gUUV, 4, uv);
	glBindTexture(GL_TEXTURE_2D, tex);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// The engine caches its GL state; everything touched here is put back.
struct SavedState
{
	GLint program, vao, activeTex, tex0, sampler0, drawFbo, readFbo;
	GLint viewport[4], scissorBox[4];
	GLint blendSrcRGB, blendDstRGB, blendSrcA, blendDstA, blendEqRGB, blendEqA;
	GLboolean blend, depth, cull, stencil, scissor, srgb, colorMask[4], depthMask;

	void Save()
	{
		glGetIntegerv(GL_CURRENT_PROGRAM, &program);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);
		glActiveTexture(GL_TEXTURE0);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex0);
		glGetIntegerv(GL_SAMPLER_BINDING, &sampler0);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
		glGetIntegerv(GL_VIEWPORT, viewport);
		glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
		glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRGB);
		glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRGB);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcA);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstA);
		glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEqRGB);
		glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEqA);
		blend = glIsEnabled(GL_BLEND);
		depth = glIsEnabled(GL_DEPTH_TEST);
		cull = glIsEnabled(GL_CULL_FACE);
		stencil = glIsEnabled(GL_STENCIL_TEST);
		scissor = glIsEnabled(GL_SCISSOR_TEST);
		srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
		glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
	}

	static void Set(GLenum cap, GLboolean on) { if (on) glEnable(cap); else glDisable(cap); }

	void Restore()
	{
		glUseProgram(program);
		glBindVertexArray(vao);
		glBindTexture(GL_TEXTURE_2D, tex0);
		glBindSampler(0, sampler0);
		glActiveTexture(activeTex);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
		glBlendFuncSeparate(blendSrcRGB, blendDstRGB, blendSrcA, blendDstA);
		glBlendEquationSeparate(blendEqRGB, blendEqA);
		Set(GL_BLEND, blend);
		Set(GL_DEPTH_TEST, depth);
		Set(GL_CULL_FACE, cull);
		Set(GL_STENCIL_TEST, stencil);
		Set(GL_SCISSOR_TEST, scissor);
		Set(GL_FRAMEBUFFER_SRGB, srgb);
		glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
		glDepthMask(depthMask);
	}
};

} // namespace

/*
	Called once a frame from TBXR_submitFrame, with the pose and field of view
	submitted with the left eye's picture. screen: the 2D canvas is on the
	virtual screen (the main menu, the splash); menu: the pause menu was
	painted this frame and hangs over the world. False when the view could not
	be drawn, and the caller's plain blit takes over.
*/
bool TBXR_ObserverDraw(const XrQuaternionf& orientation, const XrFovf& fov, bool screen,
	ovrFramebuffer* eye, ovrFramebuffer* menu)
{
	RECT rc;
	if (!gAppState.Hwnd || !GetClientRect(gAppState.Hwnd, &rc)) return true;
	const int winW = rc.right - rc.left, winH = rc.bottom - rc.top;
	if (winW <= 0 || winH <= 0) return true;	// minimised: nothing to draw
	if (!Ready()) return false;

	SavedState saved;
	saved.Save();

	glDisable(GL_FRAMEBUFFER_SRGB);
	glDisable(GL_SCISSOR_TEST);
	const bool haveEye = CopyFrom(gEyeCopy, eye->MsaaFrameBuffer, eye->Width, eye->Height);
	const bool haveMenu = menu && CopyFrom(gMenuCopy, menu->MsaaFrameBuffer, menu->Width, menu->Height);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glViewport(0, 0, winW, winH);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(gProgram);
	glBindVertexArray(gVao);
	glActiveTexture(GL_TEXTURE0);
	glBindSampler(0, 0);
	glUniform1i(gUTex, 0);

	const Crop whole = { { 0, 1, 1, 0 }, { 0, 0, 1, 1 } };
	if (haveEye && screen)
	{
		// The 2D canvas fills the eye buffer; show it at the canvas's shape.
		int cw = 0, ch = 0;
		VR_Get2DMetrics(&cw, &ch, nullptr, nullptr);
		const float a = (cw > 0 && ch > 0) ? (float)cw / ch : (float)eye->Width / eye->Height;
		float w = (float)winW, h = w / a;
		if (h > winH) { h = (float)winH; w = h * a; }
		const float x0 = (winW - w) * 0.5f, y0 = (winH - h) * 0.5f;
		Quad(gEyeCopy.tex, whole, x0, y0, x0 + w, y0 + h, winW, winH);
	}
	else if (haveEye)
	{
		Eye e;
		e.q = { orientation.x, orientation.y, orientation.z, orientation.w };
		e.l = tanf(fov.angleLeft); e.r = tanf(fov.angleRight);
		e.u = tanf(fov.angleUp); e.d = tanf(fov.angleDown);
		e.W = (float)eye->Width; e.H = (float)eye->Height;
		const double now = std::chrono::duration<double>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
		// Pause menu up: still and upright, so the menu laid over it reads.
		const Crop c = gSteady.Update(e, (float)winW / winH, now, !vr_observer_steady || haveMenu);
		Quad(gEyeCopy.tex, c, 0, 0, (float)winW, (float)winH, winW, winH);
	}

	if (haveMenu)
	{
		/*
			The menu texture is square and holds the canvas squeezed into it;
			drawn at the canvas's shape it comes out right, as the quad in the
			headset does. Premultiplied, as the runtime takes it.
		*/
		const float a = VR_MenuAspect() > 0.01f ? VR_MenuAspect() : 1.0f;
		float w = winW * 0.8f, h = w / a;
		if (h > winH * 0.9f) { h = winH * 0.9f; w = h * a; }
		const float x0 = (winW - w) * 0.5f, y0 = (winH - h) * 0.5f;
		glEnable(GL_BLEND);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		Quad(gMenuCopy.tex, whole, x0, y0, x0 + w, y0 + h, winW, winH);
	}

	saved.Restore();

	static bool reported = false;
	if (!reported)
	{
		reported = true;
		VR_Log("VR: observer view %dx%d from a %dx%d eye, %s\n", winW, winH, eye->Width, eye->Height,
			vr_observer_steady ? "steady" : "plain");
	}
	return true;
}
