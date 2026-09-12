/*
	TBXR_PC.cpp

	PC counterpart of Team Beef's RazeXR/TBXR_Common.cpp.

	Everything OpenXR here is theirs: the same extensions where they exist on
	PC, the same spaces, the same swapchain format, the same frame structure,
	the same layer composition, the same maths. What changes is only the
	platform seam:

	  - EGL and the JNI app thread become the window and GL context Raze's
	    Win32 video backend already creates, bound through
	    XrGraphicsBindingOpenGLWin32KHR.
	  - XR_KHR_opengl_es_enable becomes XR_KHR_opengl_enable, and swapchain
	    images become XrSwapchainImageOpenGLKHR.
	  - Their eye framebuffer uses GL_EXT_multisampled_render_to_texture, a
	    GLES extension that resolves implicitly. Desktop GL has no equivalent,
	    so the same sample count is reached with an explicit multisample
	    framebuffer and a resolve blit.
	  - Their Android-only extensions, thread hinting and Java lifecycle drop
	    out. The message queue and app thread go with them, because on PC the
	    engine's own main thread runs the loop.
	  - Theirs keeps one framebuffer holding a two-layer texture array and
	    renders both eyes in a single GL_OVR_multiview2 pass. This port renders
	    one pass per eye into FrameBuffer[eye]. See PROGRESS.md.
	  - GL entry points past 1.1 come from Raze's own gl_load pointers rather
	    than a second loader.

	This file began as the Quake 1 PCVR port's vr_xr.c, itself a PC port of the
	same Team Beef framework. The OpenXR half is unchanged between them because
	TBXR_Common is the same framework in both games.

	Copyright (C) 2023 Simon Brown (Team Beef)
	Copyright (C) 2026 RazeXR PCVR port

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation; either version 2 of the License, or (at your option)
	any later version.
*/

/*
	Raze's headers first, deliberately. Their mathlib.h, reached through
	VrCommon.h, defines DEG2RAD and RAD2DEG as macros; Raze's basics.h declares
	them as inline overloads. Whichever comes second loses, so the engine's
	must come first.
*/
#include "printf.h"
#include "c_cvars.h"
#include "i_time.h"
#include "v_video.h"
#include "c_dispatch.h"

#include "VrCommon.h"

#include <math.h>

bool VR_MenuInWorld();	// hw_vrmodes.cpp
float VR_MenuScale();
float VR_MenuDistance();
float VR_MenuAspect();
void VR_Get2DMetrics(int* canvasW, int* canvasH, int* viewW, int* viewH);
float VR_MenuDepth();
float vr_hunits_per_meter();
void VR_GetWorldEyePos(float* x, float* y, float* z);

/*
	The pause menu as a layer of its own.

	Painting the menu into the eye texture cannot hold it still. That texture
	is submitted as a projection layer and the compositor reprojects it to the
	head pose at display time - a correction that is right for the world,
	because the world is at the depth the projection assumes, and wrong for a
	panel a metre away that was placed with an older pose. It came out almost
	right and jittering on every small movement, which is what was reported
	twice and what no adjustment to the matrix could fix, because the error is
	added after the matrix has had its say.

	A quad layer is placed by the compositor itself, in stage space, from a
	pose we give it. It is then as stable as the runtime can make it, and the
	pose needs no Euler angles: the head pose at the moment the menu opened is
	already an XrPosef in stage space, so the panel is that pose pushed forward
	along its own facing. This is what the Quake II PCVR port does.
*/
static ovrFramebuffer gMenuBuffer;
static bool gMenuBufferReady = false;
static bool gMenuLayerDrawn = false;
static XrPosef gMenuAnchorPose;
static bool gMenuAnchored = false;
static float gMenuAnchorYawDeg = 0.0f;
static float gMenuAnchorWorld[3] = {0, 0, 0};	// eye-0 camera at pause, map units
static bool gMenuLayerAcquired = false;	// Begin succeeded; End owes a Release
static bool gMenuStatsPending = false;	// one readback per menu opening
static bool gMenuGeomPending = false;	// one geometry line per menu opening

// Square, and large enough that the menu is not the thing losing detail.
// The 2D canvas is one eye wide, so this is a mild reduction, not a blur.
#define MENU_LAYER_SIZE 2048

#ifndef GL_FRAMEBUFFER_SRGB
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#endif
#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif

ovrApp gAppState;

/*
	Their command line sets these; ours keeps the same defaults, which is what
	the owner's install runs with - his commandline.txt is just "quake".
*/
/*
	Theirs, with their values, not the Quake port's.

	RazeXR's TBXR_Common.cpp sets REFRESH = 72 and SS_MULTIPLIER = 1.0f, creates
	its swapchain with sampleCount = 1 and sets frameBuffer->Multisamples = 0.
	So the 1:1 build renders at exactly the resolution the runtime recommends,
	with no supersampling and no MSAA. Anything nicer belongs on the PC branch.
*/
int NUM_MULTI_SAMPLES = 0;
int REFRESH = 72;
float SS_MULTIPLIER = 1.0f;

/*
	The desktop mirror. In VR the engine sizes itself to one eye buffer, so the
	real window size has to come from the framebuffer object rather than from
	the engine's notion of the screen.
*/
static int TBXR_MirrorWidth()
{
	RECT r;
	if (gAppState.Hwnd && GetClientRect(gAppState.Hwnd, &r)) return r.right - r.left;
	return 0;
}

static int TBXR_MirrorHeight()
{
	RECT r;
	if (gAppState.Hwnd && GetClientRect(gAppState.Hwnd, &r)) return r.bottom - r.top;
	return 0;
}

static GLboolean stageSupported = GL_FALSE;

// Diagnostic for the desktop mirror. See TBXR_MirrorToWindow.
CVAR(Bool, vr_mirror_probe, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// strlcpy is a BSD extension the Quake engine supplied and MSVC does not.
static void TBXR_strcopy(char *dst, const char *src, size_t size)
{
	if (size == 0) return;
	strncpy(dst, src, size - 1);
	dst[size - 1] = 0;
}

/*
	Theirs, from TBXR_Common.cpp. fov_x is produced at the end of
	TBXR_submitFrame and handed to the engine through RazeXR_GetFOV(), which
	hw_entrypoint.cpp and hw_vrmodes.cpp call for culling.

	Note the consequence, which is theirs and is reproduced rather than fixed:
	because it is written after the frame is submitted, the engine culls frame
	N against frame N-1's field of view, and against 0 on the very first frame.
*/
float fov_x = 0;

float RazeXR_GetFOV()
{
	return fov_x;
}

/* ------------------------------------------------------------------------ */
/* Error reporting                                                          */
/* ------------------------------------------------------------------------ */

/*
	The instance, the system and the eye resolution are all established before
	Host_Init, because the engine has to size itself to one eye buffer before
	it opens its window. That is also before the console exists, so anything
	printed there would vanish - including the reason a headset was not found,
	which is exactly what one wants to read. Buffer it and replay it once the
	console is up.
*/
static char vr_earlylog[8192];
static bool vr_consoleready = false;

static void VR_Log(const char *fmt, ...);
static void VR_Log(const char *fmt, ...)
{
	va_list argptr;
	char msg[1024];

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	/*
		PCVR port. Everything up to and including session creation happens
		before the Raze console exists, and Raze is a GUI subsystem binary, so
		there is no stdout either - which means the reason a headset was not
		found would otherwise be invisible. Mirror every line to a file next to
		the executable so a failed startup can be read after the fact. Testing
		rounds in a headset are the scarce resource; this is what makes one
		round worth something.
	*/
	{
		FILE *f = fopen("razexr_vr.log", "a");
		if (f)
		{
			fputs(msg, f);
			fclose(f);
		}
	}

	if (vr_consoleready)
	{
		Printf("%s", msg);
		return;
	}

	strncat(vr_earlylog, msg, sizeof(vr_earlylog) - strlen(vr_earlylog) - 1);
}

void VR_FlushEarlyLog(void)
{
	vr_consoleready = true;

	if (vr_earlylog[0])
	{
		Printf("%s", vr_earlylog);
		vr_earlylog[0] = 0;
	}
}

/*
	Theirs uses clock_gettime(CLOCK_MONOTONIC). The engine already has a
	monotonic clock of its own, and using it keeps the haptics timing on the
	same clock as everything else.
*/
double TBXR_GetTimeInMilliSeconds(void)
{
	return (double)I_nsTime() / 1000000.0;
}

XrInstance TBXR_GetXrInstance(void)
{
	return gAppState.Instance;
}

void TBXR_CheckErrors(XrResult result, const char *function)
{
	if (XR_FAILED(result))
	{
		char buffer[XR_MAX_RESULT_STRING_SIZE];

		if (gAppState.Instance != XR_NULL_HANDLE &&
				XR_SUCCEEDED(xrResultToString(gAppState.Instance, result, buffer)))
		{
			VR_Log("VR: OpenXR error: %s: %s\n", function, buffer);
		}
		else
		{
			VR_Log("VR: OpenXR error: %s: %d\n", function, (int)result);
		}
	}
}


/* ------------------------------------------------------------------------ */
/* Maths - theirs, verbatim                                                  */
/* ------------------------------------------------------------------------ */

ovrMatrix4f ovrMatrix4f_CreateFromQuaternion(const XrQuaternionf *q)
{
	const float ww = q->w * q->w;
	const float xx = q->x * q->x;
	const float yy = q->y * q->y;
	const float zz = q->z * q->z;

	ovrMatrix4f out;
	out.M[0][0] = ww + xx - yy - zz;
	out.M[0][1] = 2 * (q->x * q->y - q->w * q->z);
	out.M[0][2] = 2 * (q->x * q->z + q->w * q->y);
	out.M[0][3] = 0;

	out.M[1][0] = 2 * (q->x * q->y + q->w * q->z);
	out.M[1][1] = ww - xx + yy - zz;
	out.M[1][2] = 2 * (q->y * q->z - q->w * q->x);
	out.M[1][3] = 0;

	out.M[2][0] = 2 * (q->x * q->z - q->w * q->y);
	out.M[2][1] = 2 * (q->y * q->z + q->w * q->x);
	out.M[2][2] = ww - xx - yy + zz;
	out.M[2][3] = 0;

	out.M[3][0] = 0;
	out.M[3][1] = 0;
	out.M[3][2] = 0;
	out.M[3][3] = 1;
	return out;
}

ovrMatrix4f ovrMatrix4f_Multiply(const ovrMatrix4f *a, const ovrMatrix4f *b)
{
	ovrMatrix4f out;
	int i, j;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			out.M[i][j] = a->M[i][0] * b->M[0][j] +
					a->M[i][1] * b->M[1][j] +
					a->M[i][2] * b->M[2][j] +
					a->M[i][3] * b->M[3][j];
		}
	}

	return out;
}

ovrMatrix4f ovrMatrix4f_CreateRotation(const float radiansX, const float radiansY, const float radiansZ)
{
	const float sinX = sinf(radiansX);
	const float cosX = cosf(radiansX);
	const float sinY = sinf(radiansY);
	const float cosY = cosf(radiansY);
	const float sinZ = sinf(radiansZ);
	const float cosZ = cosf(radiansZ);
	ovrMatrix4f rotationX = {{{1, 0, 0, 0}, {0, cosX, -sinX, 0}, {0, sinX, cosX, 0}, {0, 0, 0, 1}}};
	ovrMatrix4f rotationY = {{{cosY, 0, sinY, 0}, {0, 1, 0, 0}, {-sinY, 0, cosY, 0}, {0, 0, 0, 1}}};
	ovrMatrix4f rotationZ = {{{cosZ, -sinZ, 0, 0}, {sinZ, cosZ, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
	ovrMatrix4f rotationXY = ovrMatrix4f_Multiply(&rotationY, &rotationX);
	return ovrMatrix4f_Multiply(&rotationZ, &rotationXY);
}

XrVector4f XrVector4f_MultiplyMatrix4f(const ovrMatrix4f *a, const XrVector4f *v)
{
	XrVector4f out;
	out.x = a->M[0][0] * v->x + a->M[0][1] * v->y + a->M[0][2] * v->z + a->M[0][3] * v->w;
	out.y = a->M[1][0] * v->x + a->M[1][1] * v->y + a->M[1][2] * v->z + a->M[1][3] * v->w;
	out.z = a->M[2][0] * v->x + a->M[2][1] * v->y + a->M[2][2] * v->z + a->M[2][3] * v->w;
	out.w = a->M[3][0] * v->x + a->M[3][1] * v->y + a->M[3][2] * v->z + a->M[3][3] * v->w;
	return out;
}

static XrVector3f normalizeVec(XrVector3f vec)
{
	float xxyyzz = vec.x * vec.x + vec.y * vec.y + vec.z * vec.z;
	float reciprocalLength = 1.0f / sqrtf(xxyyzz);

	XrVector3f result;
	result.x = vec.x * reciprocalLength;
	result.y = vec.y * reciprocalLength;
	result.z = vec.z * reciprocalLength;
	return result;
}

void NormalizeAngles(vec3_t angles)
{
	while (angles[0] >= 90) angles[0] -= 180;
	while (angles[1] >= 180) angles[1] -= 360;
	while (angles[2] >= 180) angles[2] -= 360;
	while (angles[0] < -90) angles[0] += 180;
	while (angles[1] < -180) angles[1] += 360;
	while (angles[2] < -180) angles[2] += 360;
}

#ifndef EPSILON
#define EPSILON 0.001f
#endif

void GetAnglesFromVectors(const XrVector3f forward, const XrVector3f right, const XrVector3f up, vec3_t angles)
{
	float sr, sp, sy, cr, cp, cy;

	sp = -forward.z;

	float cp_x_cy = forward.x;
	float cp_x_sy = forward.y;
	float cp_x_sr = -right.z;
	float cp_x_cr = up.z;

	float yaw = atan2(cp_x_sy, cp_x_cy);
	float roll = atan2(cp_x_sr, cp_x_cr);

	cy = cos(yaw);
	sy = sin(yaw);
	cr = cos(roll);
	sr = sin(roll);

	if (fabs(cy) > EPSILON)
	{
		cp = cp_x_cy / cy;
	}
	else if (fabs(sy) > EPSILON)
	{
		cp = cp_x_sy / sy;
	}
	else if (fabs(sr) > EPSILON)
	{
		cp = cp_x_sr / sr;
	}
	else if (fabs(cr) > EPSILON)
	{
		cp = cp_x_cr / cr;
	}
	else
	{
		cp = cos(asin(sp));
	}

	float pitch = atan2(sp, cp);

	angles[0] = pitch / (M_PI * 2.f / 360.f);
	angles[1] = yaw / (M_PI * 2.f / 360.f);
	angles[2] = roll / (M_PI * 2.f / 360.f);

	NormalizeAngles(angles);
}

void QuatToYawPitchRoll(XrQuaternionf q, vec3_t rotation, vec3_t out)
{
	ovrMatrix4f mat = ovrMatrix4f_CreateFromQuaternion(&q);

	if (rotation[0] != 0.0f || rotation[1] != 0.0f || rotation[2] != 0.0f)
	{
		/*
			Theirs negates the rotation here. The Quake fork of this framework
			does not, and taking its version applied vr_weaponPitchAdjust and
			vr_weaponYawAdjust with the wrong sign - a 40 degree pitch error at
			the default of 20, which put the crosshair on screen only with the
			controller aimed at the floor.
		*/
		ovrMatrix4f rot = ovrMatrix4f_CreateRotation(DEG2RAD(-rotation[0]), DEG2RAD(-rotation[1]), DEG2RAD(-rotation[2]));
		mat = ovrMatrix4f_Multiply(&mat, &rot);
	}

	XrVector4f v1 = {0, 0, -1, 0};
	XrVector4f v2 = {1, 0, 0, 0};
	XrVector4f v3 = {0, 1, 0, 0};

	XrVector4f forwardInVRSpace = XrVector4f_MultiplyMatrix4f(&mat, &v1);
	XrVector4f rightInVRSpace = XrVector4f_MultiplyMatrix4f(&mat, &v2);
	XrVector4f upInVRSpace = XrVector4f_MultiplyMatrix4f(&mat, &v3);

	XrVector3f forward = {-forwardInVRSpace.z, -forwardInVRSpace.x, forwardInVRSpace.y};
	XrVector3f right = {-rightInVRSpace.z, -rightInVRSpace.x, rightInVRSpace.y};
	XrVector3f up = {-upInVRSpace.z, -upInVRSpace.x, upInVRSpace.y};

	XrVector3f forwardNormal = normalizeVec(forward);
	XrVector3f rightNormal = normalizeVec(right);
	XrVector3f upNormal = normalizeVec(up);

	GetAnglesFromVectors(forwardNormal, rightNormal, upNormal, out);
}

/* ------------------------------------------------------------------------ */
/* Eye framebuffers                                                          */
/* ------------------------------------------------------------------------ */

static void ovrFramebuffer_Clear(ovrFramebuffer *frameBuffer)
{
	memset(frameBuffer, 0, sizeof(*frameBuffer));
	frameBuffer->ColorSwapChain.Handle = XR_NULL_HANDLE;
}

static bool ovrFramebuffer_Create(
		XrSession session,
		ovrFramebuffer *frameBuffer,
		const GLenum colorFormat,
		const int width,
		const int height,
		const int multisamples)
{
	uint32_t i;

	frameBuffer->Width = width;
	frameBuffer->Height = height;
	frameBuffer->Multisamples = multisamples;

	XrSwapchainCreateInfo swapChainCreateInfo;
	memset(&swapChainCreateInfo, 0, sizeof(swapChainCreateInfo));
	swapChainCreateInfo.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
	swapChainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCreateInfo.format = colorFormat;
	swapChainCreateInfo.sampleCount = 1;
	swapChainCreateInfo.width = width;
	swapChainCreateInfo.height = height;
	swapChainCreateInfo.faceCount = 1;
	swapChainCreateInfo.arraySize = 1;
	swapChainCreateInfo.mipCount = 1;

	frameBuffer->ColorSwapChain.Width = swapChainCreateInfo.width;
	frameBuffer->ColorSwapChain.Height = swapChainCreateInfo.height;

	OXR(xrCreateSwapchain(session, &swapChainCreateInfo, &frameBuffer->ColorSwapChain.Handle));
	OXR(xrEnumerateSwapchainImages(
			frameBuffer->ColorSwapChain.Handle, 0, &frameBuffer->TextureSwapChainLength, NULL));

	frameBuffer->ColorSwapChainImage = (XrSwapchainImageOpenGLKHR *)malloc(
			frameBuffer->TextureSwapChainLength * sizeof(XrSwapchainImageOpenGLKHR));

	for (i = 0; i < frameBuffer->TextureSwapChainLength; i++)
	{
		frameBuffer->ColorSwapChainImage[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
		frameBuffer->ColorSwapChainImage[i].next = NULL;
	}

	OXR(xrEnumerateSwapchainImages(
			frameBuffer->ColorSwapChain.Handle,
			frameBuffer->TextureSwapChainLength,
			&frameBuffer->TextureSwapChainLength,
			(XrSwapchainImageBaseHeader *)frameBuffer->ColorSwapChainImage));

	frameBuffer->FrameBuffers =
			(GLuint *)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));

	/*
		One plain framebuffer per swapchain image. Theirs attaches the colour
		texture with glFramebufferTexture2DMultisampleEXT and renders straight
		into it; ours only ever receives a resolve blit, so it is single
		sample and carries no depth.
	*/
	for (i = 0; i < frameBuffer->TextureSwapChainLength; i++)
	{
		const GLuint colorTexture = frameBuffer->ColorSwapChainImage[i].image;
		GLenum status;

		glBindTexture(GL_TEXTURE_2D, colorTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);

		glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]);
		glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer->FrameBuffers[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
		status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			VR_Log("VR: incomplete eye framebuffer: 0x%x\n", status);
			return false;
		}
	}

	/*
		The target the engine actually renders into. One per eye is enough
		because it is resolved into the swapchain image before the eye ends.

		sRGB, matching the swapchain image, so the resolve is between two
		buffers of the same encoding - which is what their implicit resolve
		effectively is too.
	*/
	glGenRenderbuffers(1, &frameBuffer->MsaaColour);
	glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->MsaaColour);

	if (frameBuffer->Multisamples > 1 && glRenderbufferStorageMultisample)
	{
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, frameBuffer->Multisamples,
				GL_SRGB8_ALPHA8, width, height);
	}
	else
	{
		frameBuffer->Multisamples = 1;
		glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8_ALPHA8, width, height);
	}

	glGenRenderbuffers(1, &frameBuffer->MsaaDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->MsaaDepth);

	if (frameBuffer->Multisamples > 1)
	{
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, frameBuffer->Multisamples,
				GL_DEPTH_COMPONENT24, width, height);
	}
	else
	{
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
	}

	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glGenFramebuffers(1, &frameBuffer->MsaaFrameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer->MsaaFrameBuffer);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_RENDERBUFFER, frameBuffer->MsaaColour);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
			GL_RENDERBUFFER, frameBuffer->MsaaDepth);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		VR_Log("VR: incomplete multisample framebuffer\n");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	VR_Log("VR: eye framebuffer %dx%d, %d samples, %d swapchain images\n",
			width, height, frameBuffer->Multisamples, frameBuffer->TextureSwapChainLength);
	return true;
}

static void ovrFramebuffer_Destroy(ovrFramebuffer *frameBuffer)
{
	if (frameBuffer->FrameBuffers)
		glDeleteFramebuffers(frameBuffer->TextureSwapChainLength, frameBuffer->FrameBuffers);
	if (frameBuffer->MsaaFrameBuffer)
		glDeleteFramebuffers(1, &frameBuffer->MsaaFrameBuffer);
	if (frameBuffer->MsaaColour)
		glDeleteRenderbuffers(1, &frameBuffer->MsaaColour);
	if (frameBuffer->MsaaDepth)
		glDeleteRenderbuffers(1, &frameBuffer->MsaaDepth);
	if (frameBuffer->ColorSwapChain.Handle != XR_NULL_HANDLE)
		OXR(xrDestroySwapchain(frameBuffer->ColorSwapChain.Handle));

	free(frameBuffer->ColorSwapChainImage);
	free(frameBuffer->FrameBuffers);
	ovrFramebuffer_Clear(frameBuffer);
}

static void ovrFramebuffer_SetCurrent(ovrFramebuffer *frameBuffer)
{
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer->MsaaFrameBuffer);
}

static void ovrFramebuffer_SetNone(void)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/*
	The step their GLES extension performed implicitly.

	sRGB encoding on write is disabled deliberately: the conversion that
	matters happens on read, and is decided by the multisample buffer's
	format.
*/
static void ovrFramebuffer_Resolve(ovrFramebuffer *frameBuffer)
{
	glDisable(GL_FRAMEBUFFER_SRGB);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, frameBuffer->MsaaFrameBuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[frameBuffer->TextureSwapChainIndex]);
	glBlitFramebuffer(0, 0, frameBuffer->Width, frameBuffer->Height,
			0, 0, frameBuffer->Width, frameBuffer->Height,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

static void ovrFramebuffer_Acquire(ovrFramebuffer *frameBuffer)
{
	XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, NULL};
	XrSwapchainImageWaitInfo waitInfo;
	XrResult res;

	OXR(xrAcquireSwapchainImage(
			frameBuffer->ColorSwapChain.Handle, &acquireInfo, &frameBuffer->TextureSwapChainIndex));

	waitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
	waitInfo.next = NULL;
	waitInfo.timeout = 1000000000; /* nanoseconds */

	res = xrWaitSwapchainImage(frameBuffer->ColorSwapChain.Handle, &waitInfo);
	while (res == XR_TIMEOUT_EXPIRED)
	{
		DPrintf(DMSG_NOTIFY, "VR: retrying xrWaitSwapchainImage after XR_TIMEOUT_EXPIRED\n");
		res = xrWaitSwapchainImage(frameBuffer->ColorSwapChain.Handle, &waitInfo);
	}
}

static void ovrFramebuffer_Release(ovrFramebuffer *frameBuffer)
{
	XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
	OXR(xrReleaseSwapchainImage(frameBuffer->ColorSwapChain.Handle, &releaseInfo));
}

static void ovrRenderer_Create(XrSession session, ovrRenderer *renderer, int width, int height)
{
	int eye;

	for (eye = 0; eye < ovrMaxNumEyes; eye++)
	{
		ovrFramebuffer_Create(session, &renderer->FrameBuffer[eye],
				GL_SRGB8_ALPHA8, width, height, NUM_MULTI_SAMPLES);
	}
}

static void ovrRenderer_Destroy(ovrRenderer *renderer)
{
	int eye;

	for (eye = 0; eye < ovrMaxNumEyes; eye++)
		ovrFramebuffer_Destroy(&renderer->FrameBuffer[eye]);
}

/* ------------------------------------------------------------------------ */
/* Session state and events - theirs, with the Android hints removed          */
/* ------------------------------------------------------------------------ */

static void ovrApp_HandleSessionStateChanges(ovrApp *app, XrSessionState state)
{
	if (state == XR_SESSION_STATE_READY)
	{
		XrSessionBeginInfo sessionBeginInfo;
		XrResult result;

		memset(&sessionBeginInfo, 0, sizeof(sessionBeginInfo));
		sessionBeginInfo.type = XR_TYPE_SESSION_BEGIN_INFO;
		sessionBeginInfo.next = NULL;
		sessionBeginInfo.primaryViewConfigurationType = app->ViewportConfig.viewConfigurationType;

		OXR(result = xrBeginSession(app->Session, &sessionBeginInfo));
		app->SessionActive = (result == XR_SUCCESS);
		VR_Log("VR: session begun (%s)\n", app->SessionActive ? "active" : "failed");
	}
	else if (state == XR_SESSION_STATE_STOPPING)
	{
		OXR(xrEndSession(app->Session));
		app->SessionActive = false;
		VR_Log("VR: session stopping\n");
	}
}

static GLboolean ovrApp_HandleXrEvents(ovrApp *app)
{
	XrEventDataBuffer eventDataBuffer = {};
	GLboolean recenter = GL_FALSE;

	for (;;)
	{
		XrEventDataBaseHeader *baseEventHeader = (XrEventDataBaseHeader *)(&eventDataBuffer);
		XrResult r;

		baseEventHeader->type = XR_TYPE_EVENT_DATA_BUFFER;
		baseEventHeader->next = NULL;

		r = xrPollEvent(app->Instance, &eventDataBuffer);
		if (r != XR_SUCCESS)
			break;

		switch (baseEventHeader->type)
		{
		case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
			VR_Log("VR: instance loss pending\n");
			break;

		case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
			recenter = GL_TRUE;
			break;

		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
			{
				const XrEventDataSessionStateChanged *ev =
						(XrEventDataSessionStateChanged *)(baseEventHeader);

				switch (ev->state)
				{
				case XR_SESSION_STATE_FOCUSED:
					app->Focused = true;
					break;
				case XR_SESSION_STATE_VISIBLE:
					app->Visible = true;
					break;
				case XR_SESSION_STATE_READY:
				case XR_SESSION_STATE_STOPPING:
					ovrApp_HandleSessionStateChanges(app, ev->state);
					break;
				case XR_SESSION_STATE_EXITING:
				case XR_SESSION_STATE_LOSS_PENDING:
					VR_Log("VR: runtime asked the session to exit\n");
					AddCommandString("quit");
					break;
				default:
					break;
				}
			}
			break;

		default:
			break;
		}
	}

	return recenter;
}

/* ------------------------------------------------------------------------ */
/* Instance, system and eye resolution                                       */
/* ------------------------------------------------------------------------ */

static bool TBXR_AddExtensionIfAvailable(
		const XrExtensionProperties *available,
		uint32_t availableCount,
		const char *name,
		const char **enabled,
		uint32_t *enabledCount)
{
	uint32_t i;

	for (i = 0; i < availableCount; i++)
	{
		if (!strcmp(available[i].extensionName, name))
		{
			enabled[(*enabledCount)++] = name;
			return true;
		}
	}

	return false;
}

static void TBXR_InitialiseResolution(void)
{
	uint32_t viewCount = 0;

	gAppState.ViewportConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
	OXR(xrGetViewConfigurationProperties(gAppState.Instance, gAppState.SystemId,
			XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, &gAppState.ViewportConfig));

	OXR(xrEnumerateViewConfigurationViews(gAppState.Instance, gAppState.SystemId,
			XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, NULL));

	if (viewCount != ovrMaxNumEyes)
	{
		VR_Log("VR: expected %d views, runtime reports %u\n", ovrMaxNumEyes, viewCount);
		return;
	}

	{
		uint32_t e;

		for (e = 0; e < viewCount; e++)
		{
			gAppState.ViewConfigurationView[e].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
			gAppState.ViewConfigurationView[e].next = NULL;
		}

		OXR(xrEnumerateViewConfigurationViews(gAppState.Instance, gAppState.SystemId,
				XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount,
				gAppState.ViewConfigurationView));
	}

	// Theirs, including the multiplier and the use of eye 0 for both. The
	// cvar cannot be read here - this runs before Host_Init, so no cvar
	// exists yet - so this is the window-sizing estimate and VR_Startup
	// settles the real figure.
	gAppState.Width = gAppState.ViewConfigurationView[0].recommendedImageRectWidth * SS_MULTIPLIER;
	gAppState.Height = gAppState.ViewConfigurationView[0].recommendedImageRectHeight * SS_MULTIPLIER;

	VR_Log("VR: runtime recommends %ux%u per eye, x%.2f supersampling gives %dx%d\n",
			gAppState.ViewConfigurationView[0].recommendedImageRectWidth,
			gAppState.ViewConfigurationView[0].recommendedImageRectHeight,
			SS_MULTIPLIER, (int)gAppState.Width, (int)gAppState.Height);
}

bool TBXR_InitialiseInstance(void)
{
	XrApplicationInfo appInfo;
	XrInstanceCreateInfo instanceCreateInfo;
	XrExtensionProperties *availableExtensions;
	const char *enabledExtensions[16];
	uint32_t enabledExtensionCount = 0;
	uint32_t availableExtensionCount = 0;
	XrInstanceProperties instanceInfo;
	XrSystemGetInfo systemGetInfo;
	XrResult initResult;
	uint32_t i;
	PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = NULL;
	XrGraphicsRequirementsOpenGLKHR graphicsRequirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};

	/*
		The window and GL context are the engine's, handed over by
		TBXR_SetGraphicsBinding before any of this runs, so they have to survive
		the clear. The session itself does not notice - it asks wgl for the
		current DC and context directly - but the desktop mirror needs the
		window, and losing it left the monitor black while VR ran fine.
	*/
	HWND savedHwnd = gAppState.Hwnd;
	HDC savedHdc = gAppState.Hdc;
	HGLRC savedHglrc = gAppState.Hglrc;

	memset(&gAppState, 0, sizeof(gAppState));

	gAppState.Hwnd = savedHwnd;
	gAppState.Hdc = savedHdc;
	gAppState.Hglrc = savedHglrc;
	gAppState.OpenXRHMD = "";

	OXR(xrEnumerateInstanceExtensionProperties(NULL, 0, &availableExtensionCount, NULL));
	availableExtensions = (XrExtensionProperties *)calloc(availableExtensionCount, sizeof(XrExtensionProperties));
	for (i = 0; i < availableExtensionCount; ++i)
		availableExtensions[i].type = XR_TYPE_EXTENSION_PROPERTIES;
	OXR(xrEnumerateInstanceExtensionProperties(NULL, availableExtensionCount,
			&availableExtensionCount, availableExtensions));

	/*
		Their list, minus everything Android-only. XR_KHR_opengl_enable takes
		the place of XR_KHR_opengl_es_enable and is the one extension that must
		be present; the rest are optional there and here.
	*/
	if (!TBXR_AddExtensionIfAvailable(availableExtensions, availableExtensionCount,
			XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, enabledExtensions, &enabledExtensionCount))
	{
		/*
			Named rather than just logged, because this is the one failure a
			player can fix themselves and it says nothing about their setup
			being broken. Meta's PC runtime supports D3D and Vulkan only; Raze
			is OpenGL, so it can never provide a session here.
		*/
		VR_Log("VR: the OpenXR runtime does not support " XR_KHR_OPENGL_ENABLE_EXTENSION_NAME "\n");
		Printf("VR: this OpenXR runtime cannot give an OpenGL application a headset.\n");
		Printf("VR: Meta's PC runtime (Link and Air Link) is Direct3D and Vulkan only.\n");
		Printf("VR: use Virtual Desktop, or make SteamVR the active OpenXR runtime -\n");
		Printf("VR: SteamVR supports OpenGL and works over Link.\n");
		free(availableExtensions);
		return false;
	}

	TBXR_AddExtensionIfAvailable(availableExtensions, availableExtensionCount,
			XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME, enabledExtensions, &enabledExtensionCount);
	TBXR_AddExtensionIfAvailable(availableExtensions, availableExtensionCount,
			XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME, enabledExtensions, &enabledExtensionCount);
	TBXR_AddExtensionIfAvailable(availableExtensions, availableExtensionCount,
			XR_FB_COLOR_SPACE_EXTENSION_NAME, enabledExtensions, &enabledExtensionCount);

	memset(&appInfo, 0, sizeof(appInfo));
	TBXR_strcopy(appInfo.applicationName, "RazeXR", sizeof(appInfo.applicationName));
	appInfo.applicationVersion = 0;
	TBXR_strcopy(appInfo.engineName, "RazeXR", sizeof(appInfo.engineName));
	appInfo.engineVersion = 0;
	appInfo.apiVersion = XR_CURRENT_API_VERSION;

	memset(&instanceCreateInfo, 0, sizeof(instanceCreateInfo));
	instanceCreateInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.next = NULL;
	instanceCreateInfo.createFlags = 0;
	instanceCreateInfo.applicationInfo = appInfo;
	instanceCreateInfo.enabledApiLayerCount = 0;
	instanceCreateInfo.enabledApiLayerNames = NULL;
	instanceCreateInfo.enabledExtensionCount = enabledExtensionCount;
	instanceCreateInfo.enabledExtensionNames = enabledExtensions;

	initResult = xrCreateInstance(&instanceCreateInfo, &gAppState.Instance);
	free(availableExtensions);

	if (initResult != XR_SUCCESS)
	{
		VR_Log("VR: failed to create an OpenXR instance (%d) - is a runtime installed and running?\n",
				(int)initResult);
		gAppState.Instance = XR_NULL_HANDLE;
		return false;
	}

	instanceInfo.type = XR_TYPE_INSTANCE_PROPERTIES;
	instanceInfo.next = NULL;
	OXR(xrGetInstanceProperties(gAppState.Instance, &instanceInfo));
	VR_Log("VR: runtime %s %u.%u.%u\n", instanceInfo.runtimeName,
			XR_VERSION_MAJOR(instanceInfo.runtimeVersion),
			XR_VERSION_MINOR(instanceInfo.runtimeVersion),
			XR_VERSION_PATCH(instanceInfo.runtimeVersion));

	// Theirs, and still useful: the game half keys a couple of behaviours off it.
	if (strstr(instanceInfo.runtimeName, "PICO") || strstr(instanceInfo.runtimeName, "Pico") ||
			strstr(instanceInfo.runtimeName, "pico"))
		gAppState.OpenXRHMD = "pico";
	else if (strstr(instanceInfo.runtimeName, "Oculus") || strstr(instanceInfo.runtimeName, "Meta") ||
			strstr(instanceInfo.runtimeName, "meta"))
		gAppState.OpenXRHMD = "meta";

	memset(&systemGetInfo, 0, sizeof(systemGetInfo));
	systemGetInfo.type = XR_TYPE_SYSTEM_GET_INFO;
	systemGetInfo.next = NULL;
	systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

	initResult = xrGetSystem(gAppState.Instance, &systemGetInfo, &gAppState.SystemId);
	if (initResult != XR_SUCCESS)
	{
		VR_Log("VR: no head mounted display available (%d)\n", (int)initResult);
		xrDestroyInstance(gAppState.Instance);
		gAppState.Instance = XR_NULL_HANDLE;
		return false;
	}

	/*
		Required before xrCreateSession, and it is the call that tells the
		runtime we mean desktop GL rather than GLES. The reported version
		range is not enforced here, exactly as on their side.
	*/
	OXR(xrGetInstanceProcAddr(gAppState.Instance, "xrGetOpenGLGraphicsRequirementsKHR",
			(PFN_xrVoidFunction *)(&pfnGetOpenGLGraphicsRequirementsKHR)));
	if (pfnGetOpenGLGraphicsRequirementsKHR)
		OXR(pfnGetOpenGLGraphicsRequirementsKHR(gAppState.Instance, gAppState.SystemId, &graphicsRequirements));

	TBXR_InitialiseResolution();
	return true;
}

void TBXR_GetEyeResolution(int *width, int *height)
{
	*width = (int)gAppState.Width;
	*height = (int)gAppState.Height;
}

/* ------------------------------------------------------------------------ */
/* Session and spaces                                                        */
/* ------------------------------------------------------------------------ */

void TBXR_Recenter(void)
{
	XrReferenceSpaceCreateInfo spaceCreateInfo = {};

	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;

	if (gAppState.StageSpace != XR_NULL_HANDLE)
		OXR(xrDestroySpace(gAppState.StageSpace));
	if (gAppState.FakeStageSpace != XR_NULL_HANDLE)
		OXR(xrDestroySpace(gAppState.FakeStageSpace));

	gAppState.StageSpace = XR_NULL_HANDLE;
	gAppState.FakeStageSpace = XR_NULL_HANDLE;

	// A default stage to use when STAGE is unsupported. Their 1.675m offset.
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	spaceCreateInfo.poseInReferenceSpace.position.y = -1.6750f;
	OXR(xrCreateReferenceSpace(gAppState.Session, &spaceCreateInfo, &gAppState.FakeStageSpace));
	gAppState.CurrentSpace = gAppState.FakeStageSpace;

	if (stageSupported)
	{
		spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
		spaceCreateInfo.poseInReferenceSpace.position.y = 0.0f;
		OXR(xrCreateReferenceSpace(gAppState.Session, &spaceCreateInfo, &gAppState.StageSpace));
		gAppState.CurrentSpace = gAppState.StageSpace;
	}
}

bool TBXR_EnterVR(void)
{
	XrGraphicsBindingOpenGLWin32KHR graphicsBinding = {};
	XrSessionCreateInfo sessionCreateInfo = {};
	XrReferenceSpaceCreateInfo spaceCreateInfo = {};
	XrResult initResult;

	if (gAppState.Session)
		return true;

	/*
		No instance means TBXR_InitialiseInstance refused, and the only thing
		that makes it refuse is a runtime without XR_KHR_opengl_enable.

		Without this the next line called xrCreateSession with XR_NULL_HANDLE
		and the loader dereferenced it - a crash a moment after the shaders
		finish compiling, which is where anyone would look for it and where it
		is not. Reported on Meta Link, whose PC runtime offers D3D and Vulkan
		but no OpenGL.
	*/
	if (gAppState.Instance == XR_NULL_HANDLE)
	{
		Printf("VR: no OpenXR instance, so there is nothing to start.\n");
		Printf("VR: see the runtime note above; the game will run on the monitor.\n");
		return false;
	}

	/*
		The whole platform seam, in four lines. Their EGL display, surface and
		context become the device context and render context SDL already made
		for the engine's window. Both must be current on this thread, which
		they are: the engine creates them during Host_Init and never makes
		them current anywhere else.
	*/
	graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
	graphicsBinding.next = NULL;
	graphicsBinding.hDC = wglGetCurrentDC();
	graphicsBinding.hGLRC = wglGetCurrentContext();

	if (!graphicsBinding.hDC || !graphicsBinding.hGLRC)
	{
		Printf("VR: no current OpenGL context to bind the session to\n");
		return false;
	}

	sessionCreateInfo.type = XR_TYPE_SESSION_CREATE_INFO;
	sessionCreateInfo.next = &graphicsBinding;
	sessionCreateInfo.createFlags = 0;
	sessionCreateInfo.systemId = gAppState.SystemId;

	initResult = xrCreateSession(gAppState.Instance, &sessionCreateInfo, &gAppState.Session);
	if (initResult != XR_SUCCESS)
	{
		Printf("VR: failed to create the OpenXR session (%d)\n", (int)initResult);
		gAppState.Session = XR_NULL_HANDLE;
		return false;
	}

	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
	OXR(xrCreateReferenceSpace(gAppState.Session, &spaceCreateInfo, &gAppState.HeadSpace));

	return true;
}

void TBXR_LeaveVR(void)
{
	if (gAppState.Session)
	{
		ovrRenderer_Destroy(&gAppState.Renderer);
		free(gAppState.Projections);
		gAppState.Projections = NULL;

		if (gAppState.HeadSpace != XR_NULL_HANDLE)
			OXR(xrDestroySpace(gAppState.HeadSpace));
		if (gAppState.StageSpace != XR_NULL_HANDLE)
			OXR(xrDestroySpace(gAppState.StageSpace));
		if (gAppState.FakeStageSpace != XR_NULL_HANDLE)
			OXR(xrDestroySpace(gAppState.FakeStageSpace));

		gAppState.CurrentSpace = XR_NULL_HANDLE;
		OXR(xrDestroySession(gAppState.Session));
		gAppState.Session = XR_NULL_HANDLE;
	}

	if (gAppState.Instance != XR_NULL_HANDLE)
	{
		OXR(xrDestroyInstance(gAppState.Instance));
		gAppState.Instance = XR_NULL_HANDLE;
	}
}

void TBXR_InitRenderer(void)
{
	uint32_t numOutputSpaces = 0;
	XrReferenceSpaceType *referenceSpaces;
	uint32_t i;
	int eye;

	OXR(xrEnumerateReferenceSpaces(gAppState.Session, 0, &numOutputSpaces, NULL));
	referenceSpaces = (XrReferenceSpaceType *)malloc(numOutputSpaces * sizeof(XrReferenceSpaceType));
	OXR(xrEnumerateReferenceSpaces(gAppState.Session, numOutputSpaces, &numOutputSpaces, referenceSpaces));

	for (i = 0; i < numOutputSpaces; i++)
	{
		if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE)
		{
			stageSupported = GL_TRUE;
			break;
		}
	}

	free(referenceSpaces);

	if (gAppState.CurrentSpace == XR_NULL_HANDLE)
		TBXR_Recenter();

	/*
		Settle the eye buffer size from the cvar now that one exists. Only the
		eye framebuffers and the engine's idea of its own resolution change;
		the window and the GL context the session is bound to are untouched,
		which is why this can happen after xrCreateSession.
	*/
	{
		/*
			Theirs, unchanged: the resolution the runtime recommends, with
			SS_MULTIPLIER at 1.0 and no MSAA. Raze sizes its own framebuffer
			from TBXR_GetScreenRes, so there is no vid struct to poke here the
			way the Quake fork did.
		*/
		gAppState.Width = gAppState.ViewConfigurationView[0].recommendedImageRectWidth * SS_MULTIPLIER;
		gAppState.Height = gAppState.ViewConfigurationView[0].recommendedImageRectHeight * SS_MULTIPLIER;

		Printf("VR: %dx%d per eye (x%.2f supersampling), %dx MSAA\n",
				(int)gAppState.Width, (int)gAppState.Height, SS_MULTIPLIER, NUM_MULTI_SAMPLES);
	}

	gAppState.Projections = (XrView *)malloc(ovrMaxNumEyes * sizeof(XrView));
	for (eye = 0; eye < ovrMaxNumEyes; eye++)
	{
		memset(&gAppState.Projections[eye], 0, sizeof(XrView));
		gAppState.Projections[eye].type = XR_TYPE_VIEW;
	}

	ovrRenderer_Create(gAppState.Session, &gAppState.Renderer,
			(int)gAppState.Width, (int)gAppState.Height);

	// Its own swapchain, so the menu can be handed over as its own layer.
	// A failure here is not fatal: the menu falls back to the eye texture.
	gMenuBufferReady = ovrFramebuffer_Create(gAppState.Session, &gMenuBuffer,
			GL_SRGB8_ALPHA8, MENU_LAYER_SIZE, MENU_LAYER_SIZE, 0);
	if (!gMenuBufferReady)
		VR_Log("VR: no menu layer - the pause menu will be drawn into the eye\n");
}

void TBXR_WaitForSessionActive(void)
{
	// Theirs waits forever, which is safe on Android where the app cannot be
	// running without a headset. Here a runtime that never sends
	// XR_SESSION_STATE_READY would hang the process with nothing on screen,
	// so give up after twenty seconds and let the frame loop keep trying.
	int waited;

	for (waited = 0; !gAppState.SessionActive && waited < 20000; waited++)
	{
		if (ovrApp_HandleXrEvents(&gAppState))
			TBXR_Recenter();

		Sleep(1);
	}

	if (!gAppState.SessionActive)
		Printf("VR: the session did not become active within 20 seconds\n");
}

int TBXR_GetRefresh(void)
{
	// Their value comes from XR_FB_display_refresh_rate, which desktop
	// runtimes generally do not offer. REFRESH is their command line
	// override; 72 is what a Quest reports and what stock's sys_ticrate
	// default of 0.0138889 works out to.
	if (gAppState.currentDisplayRefreshRate > 1.0f)
		return (int)gAppState.currentDisplayRefreshRate;

	return REFRESH > 0 ? REFRESH : 72;
}

/* ------------------------------------------------------------------------ */
/* The frame                                                                 */
/* ------------------------------------------------------------------------ */

static void TBXR_GetHMDOrientation(void)
{
	XrSpaceLocation loc = {};
	vec3_t rotation = {0, 0, 0};
	vec3_t orientation = {0, 0, 0};

	if (gAppState.FrameState.predictedDisplayTime == 0)
		return;

	loc.type = XR_TYPE_SPACE_LOCATION;
	OXR(xrLocateSpace(gAppState.HeadSpace, gAppState.CurrentSpace,
			gAppState.FrameState.predictedDisplayTime, &loc));
	gAppState.xfStageFromHead = loc.pose;

	QuatToYawPitchRoll(gAppState.xfStageFromHead.orientation, rotation, orientation);
	VR_SetHMDPosition(gAppState.xfStageFromHead.position.x,
			gAppState.xfStageFromHead.position.y,
			gAppState.xfStageFromHead.position.z);
	VR_SetHMDOrientation(orientation[0], orientation[1], orientation[2]);
}

void TBXR_FrameSetup(void)
{
	XrFrameBeginInfo beginFrameDesc = {};

	if (gAppState.FrameSetup)
		return;

	for (;;)
	{
		if (ovrApp_HandleXrEvents(&gAppState))
			TBXR_Recenter();

		if (gAppState.SessionActive == GL_FALSE)
		{
			// Their loop spins here; on PC that would peg a core, and the
			// engine still wants to run so the desktop window stays alive.
			Sleep(1);
			continue;
		}

		break;
	}

	memset(&(gAppState.FrameState), 0, sizeof(XrFrameState));
	gAppState.FrameState.type = XR_TYPE_FRAME_STATE;
	OXR(xrWaitFrame(gAppState.Session, NULL, &gAppState.FrameState));

	beginFrameDesc.type = XR_TYPE_FRAME_BEGIN_INFO;
	beginFrameDesc.next = NULL;
	OXR(xrBeginFrame(gAppState.Session, &beginFrameDesc));

	VR_FrameSetup();

	TBXR_GetHMDOrientation();
	VR_HandleControllerInput();
	TBXR_ProcessHaptics();

	gAppState.FrameSetup = true;
}

static void TBXR_ClearFrameBuffer(int width, int height)
{
	glEnable(GL_SCISSOR_TEST);
	glViewport(0, 0, width, height);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	glScissor(0, 0, width, height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glScissor(0, 0, 0, 0);
	glDisable(GL_SCISSOR_TEST);

	// Theirs: the engine works in linear RGB, so never encode on write.
	glDisable(GL_FRAMEBUFFER_SRGB);
}

void TBXR_prepareEyeBuffer(int eye)
{
	ovrFramebuffer *frameBuffer = &(gAppState.Renderer.FrameBuffer[eye]);

	ovrFramebuffer_Acquire(frameBuffer);
	ovrFramebuffer_SetCurrent(frameBuffer);
	TBXR_ClearFrameBuffer(frameBuffer->Width, frameBuffer->Height);
}

/*
	The menu layer is drawn after both eyes are copied, from the same 2D
	drawer, and cleared to transparent so only what the menu draws is composited
	over the world.
*/
bool TBXR_BeginMenuLayer(void)
{
	if (!gMenuBufferReady || !gAppState.SessionActive)
	{
		if (gMenuStatsPending)
		{
			gMenuStatsPending = false;
			VR_Log("VR: menu layer refused - buffer ready %d, session active %d\n",
					(int)gMenuBufferReady, (int)gAppState.SessionActive);
		}
		return false;
	}

	gMenuLayerAcquired = true;
	ovrFramebuffer_Acquire(&gMenuBuffer);
	ovrFramebuffer_SetCurrent(&gMenuBuffer);

	glViewport(0, 0, gMenuBuffer.Width, gMenuBuffer.Height);
	glScissor(0, 0, gMenuBuffer.Width, gMenuBuffer.Height);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	return true;
}

void TBXR_EndMenuLayer(void)
{
	// Paired with the acquire, not with the session: were the session to drop
	// between the two calls, the image would still be held and every later
	// frame would wait on it.
	if (!gMenuLayerAcquired) return;
	gMenuLayerAcquired = false;

	ovrFramebuffer_Resolve(&gMenuBuffer);
	ovrFramebuffer_Release(&gMenuBuffer);
	ovrFramebuffer_SetNone();
	gMenuLayerDrawn = true;
}

int TBXR_MenuLayerSize(void)
{
	/*
		Must answer the same question TBXR_BeginMenuLayer will: the eye path
		skips drawing 2D whenever this is non-zero. Report a usable layer that
		Begin then refuses - the session inactive, on lifting the headset or
		a Virtual Desktop reconnect - and the menu is drawn into neither.
	*/
	return (gMenuBufferReady && gAppState.SessionActive) ? MENU_LAYER_SIZE : 0;
}

void TBXR_finishEyeBuffer(int eye)
{
	ovrFramebuffer *frameBuffer = &(gAppState.Renderer.FrameBuffer[eye]);

	// Theirs: without a solid alpha channel the runtime will not take the
	// whole image.
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	ovrFramebuffer_Resolve(frameBuffer);
	ovrFramebuffer_Release(frameBuffer);
	ovrFramebuffer_SetNone();
}

static void TBXR_updateProjections(void)
{
	XrViewLocateInfo projectionInfo = {};
	XrViewState viewState = {XR_TYPE_VIEW_STATE, NULL};
	uint32_t projectionCapacityInput = ovrMaxNumEyes;
	uint32_t projectionCountOutput = projectionCapacityInput;

	projectionInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
	projectionInfo.viewConfigurationType = gAppState.ViewportConfig.viewConfigurationType;
	projectionInfo.displayTime = gAppState.FrameState.predictedDisplayTime;
	projectionInfo.space = gAppState.HeadSpace;

	OXR(xrLocateViews(gAppState.Session, &projectionInfo, &viewState,
			projectionCapacityInput, &projectionCountOutput, gAppState.Projections));
}

/*
	Mirror the left eye into the desktop window.

	Their build has no desktop window at all, so this has no counterpart on
	their side. It exists because a PC game that shows nothing on the monitor
	is unpleasant to run, and because it is the only way to see what the
	headset sees while debugging.
*/
static void TBXR_MirrorToWindow(void)
{
	ovrFramebuffer *frameBuffer = &gAppState.Renderer.FrameBuffer[0];

	const int mirrorWidth = TBXR_MirrorWidth();
	const int mirrorHeight = TBXR_MirrorHeight();

	static bool reported = false;
	if (!reported)
	{
		reported = true;
		VR_Log("VR: mirror %dx%d from fbo %u (%dx%d), hwnd %p\n",
			mirrorWidth, mirrorHeight, frameBuffer->MsaaFrameBuffer,
			frameBuffer->Width, frameBuffer->Height, (void*)gAppState.Hwnd);
	}

	if (mirrorWidth <= 0 || mirrorHeight <= 0)
		return;

	/*
		glBlitFramebuffer is clipped by the scissor test, and the engine leaves
		scissoring enabled and set to the eye viewport after rendering the
		scene. That is why the mirror showed the menu - drawn with scissoring
		off - and almost nothing in a level.
	*/
	// Drain any error left by earlier engine calls, so what is checked after
	// the blit is the blit's own.
	static bool checking = false;
	GLenum stale = GL_NO_ERROR;
	if (!checking) { while (glGetError() != GL_NO_ERROR) stale = 1; }

	GLboolean scissorWas = glIsEnabled(GL_SCISSOR_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_FRAMEBUFFER_SRGB);
	/*
		vr_mirror_probe paints the window magenta before the blit. If the
		monitor turns magenta, presentation works and the blit content is at
		fault; if it does not change, nothing drawn here reaches the screen.
		One run separates two very different problems.
	*/
	if (vr_mirror_probe)
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	glBindFramebuffer(GL_READ_FRAMEBUFFER, frameBuffer->MsaaFrameBuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, frameBuffer->Width, frameBuffer->Height,
			0, 0, mirrorWidth, mirrorHeight,
			GL_COLOR_BUFFER_BIT, GL_LINEAR);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	if (scissorWas) glEnable(GL_SCISSOR_TEST);

	// One-shot: says whether the blit itself was accepted. A GL error here
	// means the mirror is being rejected; no error but a black monitor means
	// the problem is downstream, in presentation rather than in this blit.
	static bool checked = false;
	if (!checked)
	{
		checked = true;
		checking = true;
		GLenum err = glGetError();
		VR_Log("VR: mirror blit %s (scissor was %d, stale errors %d)\n",
			err == GL_NO_ERROR ? "ok" : "REJECTED", (int)scissorWas, (int)stale);
		if (err != GL_NO_ERROR)
		{
			GLint readSamples = 0, drawSamples = 0;
			glBindFramebuffer(GL_READ_FRAMEBUFFER, frameBuffer->MsaaFrameBuffer);
			glGetIntegerv(GL_SAMPLES, &readSamples);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glGetIntegerv(GL_SAMPLES, &drawSamples);
			VR_Log("VR: mirror gl error 0x%x, read samples %d, window samples %d\n",
				err, readSamples, drawSamples);
		}
	}
}

void TBXR_submitFrame(void)
{
	XrPosef stageFromEye[2];
	XrCompositionLayerProjectionView projection_layer_elements[2] = {};
	const XrCompositionLayerBaseHeader *layers[ovrMaxLayerCount] = {};
	XrFrameEndInfo endFrameInfo = {};
	int eye, i;

	if (gAppState.SessionActive == GL_FALSE)
		return;

	TBXR_updateProjections();

	/*
		Frame-period check, one line per pause. The paused loop once slept to
		the 30 Hz game tick (mainloop.cpp, TryRunTics): 10.85 ms a frame in
		play, 32.78 paused, and the world reached the compositor three frames
		stale under a panel drawn fresh - which is a sway. The two numbers here
		must agree.
	*/
	{
		static XrTime lastDisplay = 0;
		static double playPeriodMs = 0.0, menuSumMs = 0.0, menuMaxMs = 0.0;
		static float menuPeakHeadM = 0.0f, menuPeakWorldM = 0.0f, menuPeakGapM = 0.0f;
		static float menuPeakHeadV = 0.0f, menuPeakWorldV = 0.0f, menuPeakGapV = 0.0f;
		static int menuFrames = 0;
		static bool wasInWorld = false;

		const XrTime now = gAppState.FrameState.predictedDisplayTime;
		const double dtMs = lastDisplay ? (now - lastDisplay) / 1.0e6 : 0.0;
		lastDisplay = now;

		const bool inWorld = VR_MenuInWorld();
		if (!inWorld)
		{
			if (dtMs > 0.0)
				playPeriodMs = playPeriodMs == 0.0 ? dtMs : playPeriodMs * 0.95 + dtMs * 0.05;
		}
		else
		{
			if (!wasInWorld)
			{
				menuFrames = 0;
				menuSumMs = menuMaxMs = 0.0;
				menuPeakHeadM = menuPeakWorldM = menuPeakGapM = 0.0f;
				menuPeakHeadV = menuPeakWorldV = menuPeakGapV = 0.0f;
			}
			if (menuFrames < 300)
			{
				if (dtMs > 0.0) { menuSumMs += dtMs; if (dtMs > menuMaxMs) menuMaxMs = dtMs; }
				if (gMenuAnchored)
				{
					/*
						Head travel since the pause, in metres, against the
						world camera's travel in metre-equivalents.

						These two must be equal. A stage-fixed panel and a
						world that translates by a different amount cannot
						agree, and the disagreement appears as the panel
						drifting - largest when the head moves most, which is
						why the peak is what gets reported.
					*/
					float wx, wy, wz;
					VR_GetWorldEyePos(&wx, &wy, &wz);
					const float hx = gAppState.xfStageFromHead.position.x - gMenuAnchorPose.position.x;
					const float hz = gAppState.xfStageFromHead.position.z - gMenuAnchorPose.position.z;
					const float headM = sqrtf(hx * hx + hz * hz);
					const float dx = wx - gMenuAnchorWorld[0], dy = wy - gMenuAnchorWorld[1];
					const float worldM = sqrtf(dx * dx + dy * dy) / vr_hunits_per_meter();

					/*
						And the vertical, separately. A nod or a spoken word
						moves the head mostly up and down, and the horizontal
						check above cannot see any of it: Build's up axis is z
						where the runtime's is y, so the two never met.
					*/
					const float headV = fabsf(gAppState.xfStageFromHead.position.y - gMenuAnchorPose.position.y);
					const float worldV = fabsf(wz - gMenuAnchorWorld[2]) / vr_hunits_per_meter();
					if (headV > menuPeakHeadV)
					{
						menuPeakHeadV = headV;
						menuPeakWorldV = worldV;
					}
					const float gapV = fabsf(headV - worldV);
					if (gapV > menuPeakGapV) menuPeakGapV = gapV;
					if (headM > menuPeakHeadM)
					{
						menuPeakHeadM = headM;
						menuPeakWorldM = worldM;
					}
					const float gap = fabsf(headM - worldM);
					if (gap > menuPeakGapM) menuPeakGapM = gap;
				}
				if (++menuFrames == 300)
				{
					const double slipDeg = RAD2DEG(atan2((double)menuPeakGapM,
							(double)(VR_MenuDepth() > 0.01f ? VR_MenuDepth() : 3.5f)));
					VR_Log("VR: pause frame period mean %.2f ms, max %.2f ms over 300 frames (play %.2f ms)\n",
							menuSumMs / 300.0, menuMaxMs, playPeriodMs);
					const double slipDegV = RAD2DEG(atan2((double)menuPeakGapV,
							(double)(VR_MenuDepth() > 0.01f ? VR_MenuDepth() : 3.5f)));
					VR_Log("VR: pause travel flat - head %.3f m, world %.3f m, ratio %.3f, gap %.3f m = %.2f deg\n",
							menuPeakHeadM, menuPeakWorldM,
							menuPeakHeadM > 0.001f ? menuPeakWorldM / menuPeakHeadM : 0.0f,
							menuPeakGapM, slipDeg);
					VR_Log("VR: pause travel vert - head %.3f m, world %.3f m, ratio %.3f, gap %.3f m = %.2f deg\n",
							menuPeakHeadV, menuPeakWorldV,
							menuPeakHeadV > 0.001f ? menuPeakWorldV / menuPeakHeadV : 0.0f,
							menuPeakGapV, slipDegV);
					VR_Log("VR: pause panel %.2f m away, %.2f m wide = %.1f deg across\n",
							VR_MenuDepth(), 2.0f * VR_MenuScale() * VR_MenuDepth()
									/ (VR_MenuDistance() > 0.01f ? VR_MenuDistance() : 1.0f),
							2.0 * RAD2DEG(atan2((double)(VR_MenuScale() / (VR_MenuDistance() > 0.01f ? VR_MenuDistance() : 1.0f)), 1.0)));
				}
			}
		}
		wasInWorld = inWorld;
	}

	/*
		Theirs, exactly. RazeXR does not build a per eye asymmetric frustum -
		it builds one frustum that is the union of the two eyes (the left
		eye's leftmost angle, the right eye's rightmost, and the first eye's
		up and down) and renders both eyes with it. fov_x is what
		RazeXR_GetFOV() hands the engine for culling.

		Note this is the union and not the average. The Quake port this file
		came from averaged the two eyes, which is that game's answer, not this
		one's.
	*/
	XrFovf fov = {
		gAppState.Projections[0].fov.angleLeft,
		gAppState.Projections[1].fov.angleRight,
		gAppState.Projections[0].fov.angleUp,
		gAppState.Projections[0].fov.angleDown
	};

	fov_x = (fabs(fov.angleLeft) + fabs(fov.angleRight)) * 180.0f / M_PI;

	for (eye = 0; eye < ovrMaxNumEyes; eye++)
	{
		XrPosef xfHeadFromEye = gAppState.Projections[eye].pose;
		stageFromEye[eye] = XrPosef_Multiply(gAppState.xfStageFromHead, xfHeadFromEye);
	}

	TBXR_MirrorToWindow();

	gAppState.LayerCount = 0;
	memset(gAppState.Layers, 0, sizeof(xrCompositorLayer_Union) * ovrMaxLayerCount);

	/*
		PCVR port: a menu opened inside a level keeps the projection layers.
		The virtual screen is only used where there is no world to stand in -
		the title menus, intermissions, the remote camera. The flag itself is
		left as theirs set it, because it also freezes playerYaw and resets the
		positional origin while a menu is up, and both of those still want to
		happen; only the choice of layer changes.
	*/
	/*
		The pose the panel hangs at: the head's, at the moment the menu came
		up, pushed forward along its own facing. Taken from the runtime's own
		XrPosef rather than rebuilt from angles, so there are no conventions
		to get wrong and nothing left to drift.
	*/
	if (VR_MenuInWorld())
	{
		if (!gMenuAnchored)
		{
			/*
				Yaw only, as the Quake ports place theirs: a panel that
				inherited the pitch and roll of wherever you were looking when
				you paused would hang at that angle. Heading and height are
				the head's; the tilt is not. QuatToYawPitchRoll writes pitch,
				yaw, roll - index 1 is yaw - and the forward vector rotates
				(0,0,-1) by it, the pairing the screen layer already relies on.
			*/
			vec3_t rot = {0, 0, 0}, ang = {0, 0, 0};
			const XrVector3f up = {0.0f, 1.0f, 0.0f};
			QuatToYawPitchRoll(gAppState.xfStageFromHead.orientation, rot, ang);
			gMenuAnchorPose.position = gAppState.xfStageFromHead.position;
			gMenuAnchorPose.orientation = XrQuaternionf_CreateFromVectorAngle(up, DEG2RAD(ang[1]));
			gMenuAnchorYawDeg = ang[1];
			VR_GetWorldEyePos(&gMenuAnchorWorld[0], &gMenuAnchorWorld[1], &gMenuAnchorWorld[2]);
			gMenuAnchored = true;
			gMenuStatsPending = true;
			gMenuGeomPending = true;
		}
	}
	else gMenuAnchored = false;

	if (!VR_UseScreenLayer() || VR_MenuInWorld())
	{
		XrCompositionLayerProjection projection_layer = {};

		projection_layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
		projection_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		projection_layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
		projection_layer.space = gAppState.CurrentSpace;
		projection_layer.viewCount = ovrMaxNumEyes;
		projection_layer.views = projection_layer_elements;

		for (eye = 0; eye < ovrMaxNumEyes; eye++)
		{
			ovrFramebuffer *frameBuffer = &gAppState.Renderer.FrameBuffer[eye];

			memset(&projection_layer_elements[eye], 0, sizeof(XrCompositionLayerProjectionView));
			projection_layer_elements[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
			/*
				Theirs: the head pose and the union fov for both views. The
				eye separation is not in the pose - the engine bakes it into
				the projection matrix in VREyeInfo::GetStereoProjection. Feed
				the compositor anything else and the world shears.
			*/
			projection_layer_elements[eye].pose = gAppState.xfStageFromHead;
			projection_layer_elements[eye].fov = fov;
			projection_layer_elements[eye].subImage.swapchain = frameBuffer->ColorSwapChain.Handle;
			projection_layer_elements[eye].subImage.imageRect.offset.x = 0;
			projection_layer_elements[eye].subImage.imageRect.offset.y = 0;
			projection_layer_elements[eye].subImage.imageRect.extent.width = frameBuffer->ColorSwapChain.Width;
			projection_layer_elements[eye].subImage.imageRect.extent.height = frameBuffer->ColorSwapChain.Height;
			projection_layer_elements[eye].subImage.imageArrayIndex = 0;
		}

		gAppState.Layers[gAppState.LayerCount++].Projection = projection_layer;
	}
	else
	{
		XrCompositionLayerQuad quad_layer = {};
		int width = gAppState.Renderer.FrameBuffer[0].ColorSwapChain.Width;
		int height = gAppState.Renderer.FrameBuffer[0].ColorSwapChain.Height;
		const XrVector3f axis = {0.0f, 1.0f, 0.0f};
		/*
			Centred at eye height, and shaped like the buffer it shows.

			Theirs fixed the centre at 1.0 m - eyes sit around 1.6 - and the
			size at 5.0 x 4.5 whatever the swapchain's shape. On this 3072x3264
			buffer that stretched the picture 18% wide and hung its bottom rows
			well below comfortable view.
		*/
		XrVector3f pos = {
				gAppState.xfStageFromHead.position.x - sin(DEG2RAD(playerYaw)) * VR_GetScreenLayerDistance(),
				gAppState.xfStageFromHead.position.y,
				gAppState.xfStageFromHead.position.z - cos(DEG2RAD(playerYaw)) * VR_GetScreenLayerDistance()
		};
		const float screenHeight = 4.5f;
		XrExtent2Df size = {screenHeight * (float)width / (float)height, screenHeight};

		/*
			One line, once per change of shape, to settle the virtual screen's
			aspect without costing a testing round.

			Welz reported the game menus as "very tall and thin" on a 3440x1440
			ultrawide, fixed by forcing 4:3 per game. The pause panel's own
			stretch is understood and corrected - it was a square texture on a
			square quad - but the main menu is not that panel. It is this
			layer, and which of the three numbers below disagrees decides
			whether the fix is the same one:

			  canvas   what the 2D was laid out for, in pixels
			  viewport where Draw2D put it inside the eye buffer
			  buffer   the eye buffer this quad shows, and the quad's shape

			If canvas and buffer differ in aspect while the viewport covers the
			buffer, the picture is stretched and the quad wants the canvas
			aspect, exactly as the pause panel did. If the viewport is smaller
			than the buffer, nothing is stretched and the picture is sitting in
			a corner instead, which is a different repair.
		*/
		{
			static int lastW = -1, lastH = -1, lastCW = -1;
			int cw = 0, ch = 0, vw = 0, vh = 0;
			VR_Get2DMetrics(&cw, &ch, &vw, &vh);
			if (lastW != width || lastH != height || lastCW != cw)
			{
				lastW = width; lastH = height; lastCW = cw;
				VR_Log("VR: virtual screen - canvas %dx%d (%.2f), viewport %dx%d, buffer %dx%d (%.2f), quad %.2f x %.2f m\n",
						cw, ch, ch > 0 ? (float)cw / (float)ch : 0.f,
						vw, vh, width, height, height > 0 ? (float)width / (float)height : 0.f,
						size.width, size.height);
			}
		}

		quad_layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
		quad_layer.next = NULL;
		quad_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		quad_layer.space = gAppState.CurrentSpace;
		quad_layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
		quad_layer.subImage.swapchain = gAppState.Renderer.FrameBuffer[0].ColorSwapChain.Handle;
		quad_layer.subImage.imageRect.offset.x = 0;
		quad_layer.subImage.imageRect.offset.y = 0;
		quad_layer.subImage.imageRect.extent.width = width;
		quad_layer.subImage.imageRect.extent.height = height;
		quad_layer.subImage.imageArrayIndex = 0;
		quad_layer.pose.orientation = XrQuaternionf_CreateFromVectorAngle(axis, DEG2RAD(playerYaw));
		quad_layer.pose.position = pos;
		quad_layer.size = size;

		gAppState.Layers[gAppState.LayerCount++].Quad = quad_layer;
	}

	/*
		After the world, so it composites on top of it. Its height is taken
		from vr_menu_scale so the existing slider still sizes it, and its
		width from the aspect of the screen that was painted into the square
		texture, so the picture comes out the shape it went in.
	*/
	if (gMenuLayerDrawn && VR_MenuInWorld() && gAppState.LayerCount < ovrMaxLayerCount)
	{
		XrCompositionLayerQuad menu_layer = {};

		/*
			Placed exactly as the main menu's screen layer places itself -
			the one panel confirmed rock-steady on this machine - from the
			head position and yaw captured when the menu opened: back along
			(sin yaw, cos yaw), facing the head. At vr_menu_depth, with the
			width grown in proportion so the panel subtends the same angle it
			did at one metre, where its size was approved.
		*/
		const float dist = VR_MenuDepth();
		// Grown with the distance so it subtends the angle the approved panel
		// did at vr_menu_distance.
		const float ref = VR_MenuDistance() > 0.01f ? VR_MenuDistance() : 1.0f;
		const float height = 2.0f * VR_MenuScale() * dist / ref;
		/*
			Height is the size that was signed off, so height is what is kept.
			The width carries the screen's aspect, which is the squeeze the paint
			put into the square texture - see VR_MenuAspect. Square until a paint
			reports one, which is what this always did.
		*/
		const float aspect = VR_MenuAspect() > 0.01f ? VR_MenuAspect() : 1.0f;
		const float width = height * aspect;
		const float yaw = DEG2RAD(gMenuAnchorYawDeg);
		const XrVector3f axis = {0.0f, 1.0f, 0.0f};

		menu_layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
		menu_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		menu_layer.space = gAppState.CurrentSpace;
		menu_layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
		menu_layer.subImage.swapchain = gMenuBuffer.ColorSwapChain.Handle;
		menu_layer.subImage.imageRect.offset.x = 0;
		menu_layer.subImage.imageRect.offset.y = 0;
		menu_layer.subImage.imageRect.extent.width = gMenuBuffer.Width;
		menu_layer.subImage.imageRect.extent.height = gMenuBuffer.Height;
		menu_layer.subImage.imageArrayIndex = 0;
		menu_layer.pose.orientation = XrQuaternionf_CreateFromVectorAngle(axis, yaw);
		menu_layer.pose.position.x = gMenuAnchorPose.position.x - sinf(yaw) * dist;
		menu_layer.pose.position.y = gMenuAnchorPose.position.y;
		menu_layer.pose.position.z = gMenuAnchorPose.position.z - cosf(yaw) * dist;
		menu_layer.size.width = width;
		menu_layer.size.height = height;

		if (gMenuGeomPending)
		{
			gMenuGeomPending = false;
			VR_Log("VR: menu quad at (%.2f %.2f %.2f) size %.2f x %.2f m (aspect %.2f), head at (%.2f %.2f %.2f), layer %d of %d\n",
					menu_layer.pose.position.x, menu_layer.pose.position.y, menu_layer.pose.position.z,
					menu_layer.size.width, menu_layer.size.height, aspect,
					gAppState.xfStageFromHead.position.x, gAppState.xfStageFromHead.position.y,
					gAppState.xfStageFromHead.position.z,
					gAppState.LayerCount + 1, ovrMaxLayerCount);
		}
		gAppState.Layers[gAppState.LayerCount++].Quad = menu_layer;
	}
	else if (gMenuGeomPending && VR_MenuInWorld())
	{
		gMenuGeomPending = false;
		VR_Log("VR: menu quad NOT submitted - drawn %d, layers %d/%d\n",
				(int)gMenuLayerDrawn, gAppState.LayerCount, ovrMaxLayerCount);
	}
	gMenuLayerDrawn = false;

	for (i = 0; i < gAppState.LayerCount; i++)
		layers[i] = (const XrCompositionLayerBaseHeader *)&gAppState.Layers[i];

	endFrameInfo.type = XR_TYPE_FRAME_END_INFO;
	endFrameInfo.displayTime = gAppState.FrameState.predictedDisplayTime;
	endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	endFrameInfo.layerCount = gAppState.LayerCount;
	endFrameInfo.layers = layers;

	OXR(xrEndFrame(gAppState.Session, &endFrameInfo));

	gAppState.FrameSetup = false;
}

/* ------------------------------------------------------------------------ */
/* RazeXR-named entry points                                                 */
/*                                                                           */
/* Their TBXR_Common.h names a few things differently from the Quake fork of  */
/* the same framework this file came from. These are thin adapters rather     */
/* than reimplementations, so their call sites need no edit.                  */
/* ------------------------------------------------------------------------ */

extern "C" void TBXR_LogError(const char *fmt, ...)
{
	va_list argptr;
	char msg[1024];

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	VR_Log("%s", msg);
}

extern "C" void TBXR_LogVerbose(const char *fmt, ...)
{
	va_list argptr;
	char msg[1024];

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	VR_Log("%s", msg);
}

// Theirs. The Quake fork calls the same thing TBXR_GetEyeResolution.
void TBXR_GetScreenRes(int *width, int *height)
{
	TBXR_GetEyeResolution(width, height);
}

/*
	Theirs does instance creation and resolution selection in one call. The
	Quake fork splits them because DarkPlaces needs the eye size before it
	opens its window; Raze does not, so put them back together.
*/
void TBXR_InitialiseOpenXR()
{
	if (!TBXR_InitialiseInstance())
		return;

	TBXR_InitialiseResolution();
}

bool TBXR_IsFrameSetup()
{
	return gAppState.FrameSetup;
}

/*
	The Win32 video backend hands us the window and GL context it created.
	Theirs gets the equivalent from EGL during the JNI surface callbacks.
*/
void TBXR_SetGraphicsBinding(HWND hwnd, HDC hdc, HGLRC hglrc)
{
	gAppState.Hwnd = hwnd;
	gAppState.Hdc = hdc;
	gAppState.Hglrc = hglrc;
}

bool TBXR_VREnabled()
{
	return gAppState.Session != XR_NULL_HANDLE && gAppState.SessionActive;
}
