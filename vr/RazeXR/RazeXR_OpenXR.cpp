/*
	RazeXR_OpenXR.cpp - PCVR port.

	The game half of Team Beef's VR layer. Everything above the "Activity
	lifecycle" banner is theirs and is untouched: the HMD pose, VR_GetMove,
	VR_GetVRProjection, the screen layer and the haptic event names.

	What is replaced is the Android lifecycle below that banner - JNI_OnLoad,
	the Java_com_drbeef_razexr_GLES3JNILib_* entry points, the app thread and
	the Java haptics service. On PC the engine owns the main thread, so the
	startup sequence their AppThreadFunction performed is split in two around
	the point where Raze creates its window and GL context.

	Copyright (C) 2023 Simon Brown (Team Beef)
	Copyright (C) 2026 RazeXR PCVR port

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation; either version 2 of the License, or (at your option)
	any later version.
*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "VrInput.h"


//#define ENABLE_GL_DEBUG
#define ENABLE_GL_DEBUG_VERBOSE 1

//Let's go to the maximum!
extern int REFRESH	         ;
extern float SS_MULTIPLIER    ;


/* global arg_xxx structs */
struct arg_dbl *ss;
struct arg_int *cpu;
struct arg_int *gpu;
struct arg_int *msaa;
struct arg_int *refresh;
struct arg_end *end;

char **argv;
int argc=0;


//Define all variables here that were externs in the VrCommon.h
long long global_time;
float playerYaw;
float vrYaw, vrPitch;
vec3_t hmdPosition;
vec3_t hmdOrigin;
vec3_t hmdorientation;
vec3_t positionDelta;
vec3_t rawcontrollerangles;
vec3_t weaponangles;
vec3_t weaponoffset;
bool weaponStabilised;

vec3_t offhandangles;
vec3_t offhandoffset;
bool player_moving;
bool shutdown;

//This is now controlled by the engine
static bool useVirtualScreen = true;

static bool hasIWADs = false;
static bool hasLauncher = false;

/*
================================================================================

QuestZDoom Stuff

================================================================================
*/

void RazeXR_setUseScreenLayer(bool use)
{
	useVirtualScreen = use;
}

int RazeXR_SetRefreshRate(int refreshRate)
{
	if (strstr(gAppState.OpenXRHMD, "meta") != NULL)
	{
		gAppState.currentDisplayRefreshRate = refreshRate;
		OXR(gAppState.pfnRequestDisplayRefreshRate(gAppState.Session, (float)refreshRate));
		return refreshRate;
	}

	return 0;
}

int RazeXR_GetRefreshRate()
{
	return (int)gAppState.currentDisplayRefreshRate;
}

void RazeXR_GetScreenRes(uint32_t *width, uint32_t *height)
{
	int iWidth, iHeight;
	TBXR_GetScreenRes(&iWidth, &iHeight);
	*width = iWidth;
	*height = iHeight;
}

bool VR_UseScreenLayer()
{
	return useVirtualScreen;
}

float VR_GetScreenLayerDistance()
{
	return 4.0f;
}

static void UnEscapeQuotes( char *arg )
{
	char *last = NULL;
	while( *arg ) {
		if( *arg == '"' && *last == '\\' ) {
			char *c_curr = arg;
			char *c_last = last;
			while( *c_curr ) {
				*c_last = *c_curr;
				c_last = c_curr;
				c_curr++;
			}
			*c_last = '\0';
		}
		last = arg;
		arg++;
	}
}

static int ParseCommandLine(char *cmdline, char **argv)
{
	char *bufp;
	char *lastp = NULL;
	int argc, last_argc;
	argc = last_argc = 0;
	for ( bufp = cmdline; *bufp; ) {
		while ( isspace(*bufp) ) {
			++bufp;
		}
		if ( *bufp == '"' ) {
			++bufp;
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			while ( *bufp && ( *bufp != '"' || *lastp == '\\' ) ) {
				lastp = bufp;
				++bufp;
			}
		} else {
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			while ( *bufp && ! isspace(*bufp) ) {
				++bufp;
			}
		}
		if ( *bufp ) {
			if ( argv ) {
				*bufp = '\0';
			}
			++bufp;
		}
		if( argv && last_argc != argc ) {
			UnEscapeQuotes( argv[last_argc] );
		}
		last_argc = argc;
	}
	if ( argv ) {
		argv[argc] = NULL;
	}
	return(argc);
}


void VR_SetHMDOrientation(float pitch, float yaw, float roll)
{
	VectorSet(hmdorientation, pitch, yaw, roll);

	if (!VR_UseScreenLayer() || playerYaw == 0.0f)
    {
    	playerYaw = yaw;
	}
}

void VR_SetHMDPosition(float x, float y, float z )
{
 	VectorSet(hmdPosition, x, y, z);

	if (VR_UseScreenLayer() || hmdOrigin[0] == 0.0f)
	{
		VectorSet(hmdOrigin, x, y, z);
	}

	VectorSubtract(hmdPosition, hmdOrigin, positionDelta);
}

void VR_GetMove(float *joy_forward, float *joy_side, float *hmd_forward, float *hmd_side, float *up,
				float *yaw, float *pitch, float *roll)
{
    *joy_forward = remote_movementForward;
    *hmd_forward = positional_movementForward;
    *up = remote_movementUp;
    *joy_side = remote_movementSideways;
    *hmd_side = positional_movementSideways;
	*yaw = hmdorientation[YAW] + snapTurn;
	*pitch = hmdorientation[PITCH];
	*roll = hmdorientation[ROLL];
}

void VR_Init()
{
	//Initialise all our variables
	playerYaw = 0.0f;
	VectorClear(hmdOrigin);
	remote_movementSideways = 0.0f;
	remote_movementForward = 0.0f;
	remote_movementUp = 0.0f;
	positional_movementSideways = 0.0f;
	positional_movementForward = 0.0f;
	snapTurn = 0.0f;

	//init randomiser
	srand(time(NULL));

	shutdown = false;

	// Theirs moves to /sdcard/RazeXR here. On PC the engine already runs from
	// its own directory and Raze finds its data through the search paths.
}

int raze_main (int argc, char **argv);

/*
	Theirs runs the whole startup on an app thread it creates from JNI. On PC
	the engine owns the main thread, so their sequence lives in
	RazeXR_PC_PreInit / RazeXR_PC_StartVR below instead.
*/

//All the stuff we want to do each frame specifically for this game
void VR_FrameSetup()
{
	RazeXR_SetRefreshRate(REFRESH);
}

bool VR_GetVRProjection(int eye, float zNear, float zFar, float* projection)
{

    XrFovf fov = {
            gAppState.Projections[0].fov.angleLeft,
            gAppState.Projections[1].fov.angleRight,
            gAppState.Projections[0].fov.angleUp,
            gAppState.Projections[0].fov.angleDown
    };

    XrMatrix4x4f_CreateProjectionFov(
            &(gAppState.ProjectionMatrices[eye]), GRAPHICS_OPENGL_ES,
            fov, zNear, zFar);
	
	memcpy(projection, gAppState.ProjectionMatrices[eye].m, 16 * sizeof(float));
	return true;
}


extern "C" {
void jni_haptic_event(const char *event, int position, int intensity, float angle, float yHeight);
void jni_haptic_updateevent(const char *event, int intensity, float angle);
void jni_haptic_stopevent(const char *event);
void jni_haptic_endframe();
void jni_haptic_enable();
void jni_haptic_disable();
};

void VR_ExternalHapticEvent(const char* event, int position, int flags, int intensity, float angle, float yHeight )
{
	jni_haptic_event(event, position, intensity, angle, yHeight);
}

void VR_HapticStopEvent(const char* event)
{
	jni_haptic_stopevent(event);
}

void VR_HapticEnable()
{
	static bool firstTime = true;
	if (firstTime) {
		jni_haptic_enable();
		firstTime = false;
		jni_haptic_event("fire_pistol", 0, 100, 0, 0);
	}
}

void VR_HapticDisable()
{
	jni_haptic_disable();
}

void VR_HapticEvent(const char* event, int position, int intensity, float angle, float yHeight )
{
    static char buffer[256];

    memset(buffer, 0, 256);
    for(int i = 0; event[i]; i++)
    {
        buffer[i] = tolower(event[i]);
    }

    jni_haptic_event(buffer, position, intensity, angle, yHeight);
}

void RazeXR_Vibrate(float duration, int channel, float intensity )
{
	TBXR_Vibrate(duration, channel+1, intensity);
}

void VR_HandleControllerInput() {
	TBXR_UpdateControllers();

    //Call additional control schemes here
    switch (vr_control_scheme)
    {
            case RIGHT_HANDED_DEFAULT:
    	        HandleInput_Default(vr_control_scheme,
    	                &rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
                                &leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
                                xrButton_A, xrButton_B, xrButton_X, xrButton_Y);
                    break;
            case LEFT_HANDED_DEFAULT:
			case LEFT_HANDED_ALT:
	            HandleInput_Default(vr_control_scheme,
	                                &leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
                                        &rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
                                        xrButton_X, xrButton_Y, xrButton_A, xrButton_B);
                    break;
    }
}

/*
================================================================================

PC lifecycle

Replaces their JNI_OnLoad, the GLES3JNILib entry points and the app thread.

================================================================================
*/

/*
	Theirs does all of this on the app thread before calling raze_main, because
	on Android the GL context already exists by then - EGL created it during the
	surface callbacks. On PC the context is Raze's own and does not exist until
	the engine has opened its window, so the sequence splits:

	  RazeXR_PC_PreInit   before the engine starts - instance, system and eye
	                      resolution, none of which need a GL context.
	  RazeXR_PC_StartVR   once the Win32 backend has a context and has called
	                      TBXR_SetGraphicsBinding - session, renderer, actions.

	Their SS_MULTIPLIER clamp is kept where they put it.
*/
void RazeXR_PC_PreInit()
{
	//Set device defaults
	if (SS_MULTIPLIER == 0.0f)
	{
		SS_MULTIPLIER = 1.0f;
	}
	else if (SS_MULTIPLIER > 1.5f)
	{
		SS_MULTIPLIER = 1.5f;
	}

	VR_Init();

	TBXR_InitialiseOpenXR();
}

void RazeXR_PC_StartVR()
{
	if (!TBXR_EnterVR())
		return;

	TBXR_InitRenderer();
	TBXR_InitActions();

	TBXR_WaitForSessionActive();

	if (REFRESH != 0)
	{
		RazeXR_SetRefreshRate(REFRESH);
	}
}

void RazeXR_PC_StopVR()
{
	TBXR_LeaveVR();
}

/*
	Theirs asks Java to finish the activity. On PC there is nothing above us to
	ask - the engine's own quit path runs.
*/
void jni_shutdown()
{
	ALOGV("jni_shutdown");
}

extern "C" void VR_Shutdown()
{
	jni_shutdown();
}

/*
	Their haptics are authored patterns played by a Java service, addressed by
	name ("fire_pistol" and so on). There is no PC equivalent of that service,
	so the named event becomes a plain controller pulse through the OpenXR
	haptic action. This is a divergence and is recorded as one in PROGRESS.md:
	the events fire in the same places with the same intensities, but the
	texture of the pattern is lost.
*/
static bool vr_hapticsEnabled = false;

void jni_haptic_event(const char *event, int position, int intensity, float angle, float yHeight)
{
	if (!vr_hapticsEnabled)
		return;

	// position: 0 both, 1 right, 2 left - as their Java service reads it.
	const float level = (float)intensity / 100.0f;

	if (position == 0 || position == 1) TBXR_Vibrate(100, 1, level);
	if (position == 0 || position == 2) TBXR_Vibrate(100, 2, level);
}

void jni_haptic_stopevent(const char *event)
{
	TBXR_Vibrate(0, 1, 0.0f);
	TBXR_Vibrate(0, 2, 0.0f);
}

void jni_haptic_enable()
{
	vr_hapticsEnabled = true;
}

void jni_haptic_disable()
{
	vr_hapticsEnabled = false;
}
