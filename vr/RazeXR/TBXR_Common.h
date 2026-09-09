/*
	TBXR_Common.h - PCVR port.

	Team Beef's original targets Android: EGL, JNI, an ANativeWindow and
	XR_KHR_opengl_es_enable. This one targets Win32 and desktop OpenGL.

	The file keeps its original name, and every struct, enum, field and
	function name theirs declares is kept with the same meaning, so their
	OpenXrInput.cpp, VrInputCommon.cpp, VrInputDefault.cpp and the game half of
	RazeXR_OpenXR.cpp compile against it unchanged.

	What differs, and why:

	- GLES3/EGL/JNI headers are replaced by <windows.h> and Raze's own
	  gl_load.h, which is already on the include path and supplies the desktop
	  GL entry points under their normal names.
	- ovrFramebuffer carries an explicit multisample colour/depth renderbuffer
	  and blits into the swapchain image. Theirs attaches the colour texture
	  through GL_EXT_multisampled_render_to_texture, which resolves implicitly
	  on a tiler and has no desktop equivalent.
	- ovrRenderer holds FrameBuffer[ovrMaxNumEyes] rather than a single
	  FrameBuffer. Theirs renders both eyes in one pass into a two-layer
	  texture array using GL_OVR_multiview2; this port renders two passes, one
	  per eye, so each eye needs its own target. See PROGRESS.md.
	- The Android app thread, its surface message queue and the JNI lifecycle
	  are gone. On PC the engine owns the main thread.

	Copyright (C) 2023 Simon Brown (Team Beef)
	Copyright (C) 2026 RazeXR PCVR port

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation; either version 2 of the License, or (at your option)
	any later version.
*/

#if !defined(tbxr_common_h)
#define tbxr_common_h

//OpenXR
#define XR_USE_GRAPHICS_API_OPENGL 1
#define XR_USE_PLATFORM_WIN32 1

// WIN32_LEAN_AND_MEAN keeps winsock out of the translation unit. Without it
// its shutdown() collides with the global of the same name that VrCommon.h
// declares and their code writes to.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// openxr_platform.h declares the D3D graphics bindings over IUnknown, which
// WIN32_LEAN_AND_MEAN otherwise leaves undefined.
#include <unknwn.h>

// Raze's own loader. It defines the desktop GL entry points as macros over
// function pointers under their standard names, so their GL calls carry over
// unchanged.
#include "gl_load.h"

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr_helpers.h>

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef NDEBUG
#define DEBUG 1
#endif

#define LOG_TAG "RazeXR"

// Theirs go to logcat. Ours go to the Raze console, which is where a PC player
// would look for them. Declared rather than included to keep this header free
// of engine headers, which their .cpp files do not expect to see.
#ifdef __cplusplus
extern "C" {
#endif
void TBXR_LogError(const char *fmt, ...);
void TBXR_LogVerbose(const char *fmt, ...);
#ifdef __cplusplus
}
#endif

#define ALOGE(...) TBXR_LogError(__VA_ARGS__)

#if DEBUG
#define ALOGV(...) TBXR_LogVerbose(__VA_ARGS__)
#else
#define ALOGV(...)
#endif

// 1 upstream: theirs submits the world or the virtual screen, never both.
// The world-space pause menu is a second layer over the projection, and with
// one slot it was refused every frame. xrEndFrame is given LayerCount, so the
// spare slots cost nothing.
enum { ovrMaxLayerCount = 4 };
enum { ovrMaxNumEyes = 2 };

// Theirs, unchanged - OpenXrInput.cpp fills these in and the game half reads
// them by name.
typedef enum xrButton_ {
    xrButton_A = 0x00000001,
    xrButton_B = 0x00000002,
    xrButton_RThumb = 0x00000004,
    xrButton_RShoulder = 0x00000008,
    xrButton_X = 0x00000100,
    xrButton_Y = 0x00000200,
    xrButton_LThumb = 0x00000400,
    xrButton_LShoulder = 0x00000800,
    xrButton_Up = 0x00010000,
    xrButton_Down = 0x00020000,
    xrButton_Left = 0x00040000,
    xrButton_Right = 0x00080000,
    xrButton_Enter = 0x00100000,
    xrButton_Back = 0x00200000,
    xrButton_GripTrigger = 0x04000000,
    xrButton_Trigger = 0x20000000,
    xrButton_Joystick = 0x80000000,

    //Define additional controller touch points (not button presses)
    xrButton_ThumbRest = 0x00000010,

    xrButton_EnumSize = 0x7fffffff
} xrButton;

typedef struct {
    uint32_t Buttons;
    uint32_t Touches;
    float IndexTrigger;
    float GripTrigger;
    XrVector2f Joystick;
} ovrInputStateTrackedRemote;

typedef struct {
    GLboolean Active;
    XrPosef Pose;
    XrSpaceVelocity Velocity;
} ovrTrackedController;

typedef enum control_scheme {
    RIGHT_HANDED_DEFAULT = 0,
    LEFT_HANDED_DEFAULT = 10,
    LEFT_HANDED_ALT = 11
} control_scheme_t;

typedef struct {
    float M[4][4];
} ovrMatrix4f;

typedef struct {
    XrSwapchain Handle;
    uint32_t Width;
    uint32_t Height;
} ovrSwapChain;

/*
	One eye's render target.

	Theirs holds a per-swapchain-image framebuffer with the colour texture
	attached through glFramebufferTexture2DMultisampleEXT, so multisampling
	resolves for free on the tiler. Desktop GL has no such path, so we keep one
	multisample colour and depth renderbuffer for the eye and blit into
	whichever swapchain image the runtime handed us. The per-image framebuffers
	below are therefore plain single-sample blit targets.
*/
typedef struct {
    int Width;
    int Height;
    int Multisamples;
    uint32_t TextureSwapChainLength;
    uint32_t TextureSwapChainIndex;
    ovrSwapChain ColorSwapChain;
    XrSwapchainImageOpenGLKHR* ColorSwapChainImage;
    GLuint* FrameBuffers;

    GLuint MsaaFrameBuffer;
    GLuint MsaaColour;
    GLuint MsaaDepth;
} ovrFramebuffer;

/*
================================================================================

ovrRenderer

================================================================================
*/

typedef struct
{
    ovrFramebuffer	FrameBuffer[ovrMaxNumEyes];
} ovrRenderer;

/*
================================================================================

ovrApp

================================================================================
*/

typedef union {
    XrCompositionLayerProjection Projection;
    XrCompositionLayerQuad Quad;
} xrCompositorLayer_Union;

#define GL(func) func;

// Forward declarations
XrInstance TBXR_GetXrInstance();

#if defined(DEBUG)
static void
OXR_CheckErrors(XrInstance instance, XrResult result, const char* function, bool failOnError) {
    if (XR_FAILED(result)) {
        char errorBuffer[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(instance, result, errorBuffer);
        if (failOnError) {
            ALOGE("OpenXR error: %s: %s\n", function, errorBuffer);
        } else {
            ALOGV("OpenXR error: %s: %s\n", function, errorBuffer);
        }
    }
}
#endif

#if defined(DEBUG)
#define OXR(func) OXR_CheckErrors(TBXR_GetXrInstance(), func, #func, true);
#else
#define OXR(func) func;
#endif

typedef struct
{
    // Win32 replaces their ovrEgl and ovrJava. These are the engine's own
    // window device context and GL context, handed to
    // XrGraphicsBindingOpenGLWin32KHR at session creation.
    HDC                 Hdc;
    HGLRC               Hglrc;
    HWND                Hwnd;

    bool				Resumed;
    bool				Focused;
    // PC only: their Android build learns visibility from the JNI surface
    // callbacks. On PC it comes from the OpenXR session state.
    bool				Visible;
    bool                FrameSetup;
    const char*         OpenXRHMD;

    float               Width;
    float               Height;

    XrInstance Instance;
    XrSession Session;
    XrViewConfigurationProperties ViewportConfig;
    XrViewConfigurationView ViewConfigurationView[ovrMaxNumEyes];
    XrSystemId SystemId;
    XrSpace HeadSpace;
    XrSpace StageSpace;
    XrSpace FakeStageSpace;
    XrSpace CurrentSpace;
    GLboolean SessionActive;
    XrPosef xfStageFromHead;
    XrView* Projections;
    XrMatrix4x4f ProjectionMatrices[2];

    float currentDisplayRefreshRate;
    float* SupportedDisplayRefreshRates;
    uint32_t RequestedDisplayRefreshRateIndex;
    uint32_t NumSupportedDisplayRefreshRates;
    PFN_xrGetDisplayRefreshRateFB pfnGetDisplayRefreshRate;
    PFN_xrRequestDisplayRefreshRateFB pfnRequestDisplayRefreshRate;

    XrFrameState        FrameState;
    int					SwapInterval;
    xrCompositorLayer_Union		Layers[ovrMaxLayerCount];
    int					LayerCount;
    ovrRenderer			Renderer;
    ovrTrackedController TrackedController[2];
} ovrApp;

extern ovrApp gAppState;

void ovrTrackedController_Clear(ovrTrackedController* controller);

//Functions that need to be implemented by the game specific code
void VR_FrameSetup();
bool VR_UseScreenLayer();
float VR_GetScreenLayerDistance();
bool VR_GetVRProjection(int eye, float zNear, float zFar, float* projection);
void VR_HandleControllerInput();
void VR_SetHMDOrientation(float pitch, float yaw, float roll );
void VR_SetHMDPosition(float x, float y, float z );
void VR_HapticEvent(const char* event, int position, int intensity, float angle, float yHeight );
void VR_HapticUpdateEvent(const char* event, int intensity, float angle );
void VR_HapticEndFrame();
void VR_HapticStopEvent(const char* event);
void VR_HapticEnable();
void VR_HapticDisable();
extern "C" void VR_Shutdown();


//Reusable Team Beef OpenXR stuff (in TBXR_PC.cpp on this platform)
double TBXR_GetTimeInMilliSeconds();
int TBXR_GetRefresh();
void TBXR_Recenter();
void TBXR_InitialiseOpenXR();
void TBXR_WaitForSessionActive();
void TBXR_InitRenderer();
bool TBXR_EnterVR();
void TBXR_LeaveVR( );
void TBXR_GetScreenRes(int *width, int *height);
void TBXR_InitActions( void );
void TBXR_Vibrate(int duration, int channel, float intensity );
void TBXR_ProcessHaptics();
void TBXR_FrameSetup();
bool TBXR_IsFrameSetup();
void TBXR_updateProjections();
void TBXR_UpdateControllers( );
void TBXR_prepareEyeBuffer(int eye );
void TBXR_finishEyeBuffer(int eye );
void TBXR_submitFrame();

// PC only. The Win32 video backend hands the OpenXR layer the window and GL
// context it created, before the session is made.
void TBXR_SetGraphicsBinding(HWND hwnd, HDC hdc, HGLRC hglrc);

// PC only. True once an OpenXR session is live. Every VR-specific behaviour in
// the engine is gated on this at run time rather than at compile time, so one
// binary serves both the headset and the desktop.
bool TBXR_VREnabled();

// Maths, theirs, verbatim - defined in TBXR_PC.cpp.
ovrMatrix4f ovrMatrix4f_CreateFromQuaternion(const XrQuaternionf *q);
ovrMatrix4f ovrMatrix4f_CreateRotation(const float radiansX, const float radiansY, const float radiansZ);
ovrMatrix4f ovrMatrix4f_Multiply(const ovrMatrix4f *a, const ovrMatrix4f *b);
XrVector4f XrVector4f_MultiplyMatrix4f(const ovrMatrix4f *a, const XrVector4f *v);

#endif //tbxr_common_h
