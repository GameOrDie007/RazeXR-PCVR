/*
	PC branch: crouch for real.

	The head going down about 30 cm below standing height crouches the body,
	so it fits wherever the game's crouch fits. It stands again once the head
	is back above about 22 cm; the gap stops it flickering at the edge.

	The crouch goes through the normal input path - the same SB_CROUCH the
	crouch button sends - so each game does exactly what its own crouch does.

	The game's crouch lowers its camera, and the head has already gone down,
	so the view would drop twice. Each game says where its eye would be if the
	player stood (GameInterface::VRStandingEyeZ), and the eyes are lifted back
	by the difference. The lift carries on after the head comes up, until the
	game's camera is back at standing height, or standing up drops the view by
	the whole crouch for as long as the game takes to rise.

	Only on foot: swimming, flying, falling and jetpacks keep their own up and
	down. Only in the headset, with positional tracking on.

	And only once the head has been seen at standing height. A player sitting
	down who never recentered has a head well below the game's standing
	height for the whole game; without this they would play crouched.
*/

#include "vr_crouch.h"
#include "c_cvars.h"
#include "gamestate.h"
#include "gamestruct.h"
#include "hw_vrmodes.h"

CVAR(Bool, vr_crouch_real, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

EXTERN_CVAR(Float, vr_height_adjust)
EXTERN_CVAR(Bool, vr_positional_tracking)
extern float hmdPosition[3];
extern float playerHeight;
extern float heightScaler;
float vr_hunits_per_meter();

static const double kDown = 0.30;	// metres below standing height that crouch
static const double kUp = 0.22;		// and above which the body stands again

static bool armed = false;		// the head has been up at standing height
static bool held = false;		// the head is down
static bool lifting = false;	// the view is being put back
static double lift = 0;			// map units, this frame

static bool InHeadset()
{
	return vr_crouch_real && vr_positional_tracking && gamestate == GS_LEVEL &&
		playerHeight != 0 && heightScaler == 1.0f &&
		VRMode::GetVRMode(true)->mEyeCount == 2;
}

bool VRCrouch_Held(bool buttonCrouch)
{
	if (!InHeadset())
	{
		armed = held = lifting = false;
		return false;
	}

	// Standing height is where vr_recenter put it: the game's player height,
	// less the adjust that recenter took.
	const double upm = vr_hunits_per_meter();
	const double below = playerHeight / upm - (hmdPosition[1] + vr_height_adjust);
	if (below < kUp) armed = true;
	if (!armed) return false;
	if (!held && below > kDown) held = true;
	else if (held && below < kUp) held = false;

	double z;
	const bool onFoot = gi->VRStandingEyeZ(1., z);
	if (held && onFoot) lifting = true;
	// The button's own crouch is meant to drop the view.
	else if (buttonCrouch) lifting = false;
	return held && onFoot;
}

void VRCrouch_Frame(const DVector3& eye, double interpfrac)
{
	lift = 0;
	if (!lifting) return;
	double standZ;
	if (!InHeadset() || !gi->VRStandingEyeZ(interpfrac, standZ))
	{
		if (!held) lifting = false;
		return;
	}
	const double drop = eye.Z - standZ;	// Build Z runs downwards
	if (drop > 0.25) lift = drop;
	else if (!held) lifting = false;	// back up: done
}

double VRCrouch_Lift()
{
	return lift;
}
