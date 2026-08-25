#if !defined(vrcommon_h)
#define vrcommon_h

#include "TBXR_Common.h"

#include "c_cvars.h"

EXTERN_CVAR(Int, vr_control_scheme)
EXTERN_CVAR(Bool, vr_move_use_offhand)
EXTERN_CVAR(Float, vr_snapTurn);
EXTERN_CVAR(Float, vr_ipd);
EXTERN_CVAR(Float, vr_weaponPitchAdjust);
EXTERN_CVAR(Float, vr_weaponYawAdjust);
EXTERN_CVAR(Bool, vr_allowPitchOverride);
EXTERN_CVAR(Bool, vr_switch_sticks);
EXTERN_CVAR(Bool, vr_secondary_button_mappings);
EXTERN_CVAR(Bool, vr_two_handed_weapons);
EXTERN_CVAR(Bool, vr_crouch_use_button);


// PCVR port: theirs logs to logcat here and repeats the ALOGE/ALOGV macros
// that TBXR_Common.h already defines. On PC that header routes them to the
// Raze console, so the duplicate definitions are simply dropped.

#include "mathlib.h"

extern float cinemamodeYaw;
extern float cinemamodePitch;

extern float playerYaw;
extern float vrYaw;

extern vec3_t hmdPosition;
extern vec3_t hmdOrigin;
extern vec3_t hmdorientation;
extern vec3_t positionDelta;

extern vec3_t rawcontrollerangles;
extern vec3_t weaponangles;
extern vec3_t weaponoffset;

extern vec3_t offhandangles;
extern vec3_t offhandoffset;

extern bool player_moving;

extern bool shutdown;
void shutdownVR();

bool isMultiplayer();
float length(float x, float y);
float nonLinearFilter(float in);
bool between(float min, float val, float max);
void rotateAboutOrigin(float v1, float v2, float rotation, vec2_t out);
void handleTrackedControllerButton(ovrInputStateTrackedRemote * trackedRemoteState, ovrInputStateTrackedRemote * prevTrackedRemoteState, uint32_t button, int key);
void QuatToYawPitchRoll(XrQuaternionf q, vec3_t rotation, vec3_t out);

//Called from engine code
void RazeXR_setUseScreenLayer(bool use);
void jni_shutdown();


#endif //vrcommon_h