/*
	PC branch: point at menus.

	Hold the gun hand's grip in any menu and its aim becomes a pointer: a dot
	on the panel where the aim lands, and the trigger clicks there. Let go of
	the grip and the dot goes; a tap of the grip keeps its old job, and the
	sticks and buttons work the menu as before. Always-on pointing fought the
	stick's own selection, so it is only while the grip is held.

	The VR layer finds where the aim meets the panel (VrInputDefault.cpp, from
	the quad TBXR_PC.cpp submitted). Here that becomes a point on the 2D
	canvas - the whole canvas is what each panel shows, edge to edge - and the
	menus get it as ordinary mouse events, which they already understand.

	A click is released where it was pressed. Menus act on a release over the
	row that was pressed, and a hand drifts while it pulls a trigger; a real
	release a few pixels off would do nothing. A hand moved further than that
	is dragging (a slider) and is released where it is.
*/

#include "vr_pointer.h"
#include "d_eventbase.h"
#include "menustate.h"
#include "v_draw.h"
#include "v_2ddrawer.h"

void VR_Get2DMetrics(int* canvasW, int* canvasH, int* viewW, int* viewH);

static bool shown = false;		// the dot is up
static bool down = false;		// the trigger is held on the menu
static bool dragging = false;	// and the hand has moved away since
static int dotX = 0, dotY = 0;
static int pressX = 0, pressY = 0;

static void Post(int subtype, int x, int y)
{
	event_t ev = {};
	ev.type = EV_GUI_Event;
	ev.subtype = subtype;
	ev.data1 = x;
	ev.data2 = y;
	D_PostEvent(&ev);
}

void VRPointer_Update(bool on, float u, float v, bool trigger)
{
	int cw = 0, ch = 0;
	VR_Get2DMetrics(&cw, &ch, nullptr, nullptr);
	if (!on || cw <= 0 || ch <= 0 || menuactive == MENU_Off)
	{
		if (down) Post(EV_GUI_LButtonUp, pressX, pressY);
		down = dragging = shown = false;
		return;
	}

	const int x = clamp((int)(u * cw), 0, cw - 1);
	const int y = clamp((int)(v * ch), 0, ch - 1);
	const int slop = cw / 30;	// about 1 degree of hand at the default size

	if (down && !dragging && (abs(x - pressX) > slop || abs(y - pressY) > slop)) dragging = true;
	if ((!shown || x != dotX || y != dotY) && (!down || dragging)) Post(EV_GUI_MouseMove, x, y);

	if (trigger && !down)
	{
		Post(EV_GUI_LButtonDown, x, y);
		down = true;
		dragging = false;
		pressX = x;
		pressY = y;
	}
	else if (!trigger && down)
	{
		if (dragging) Post(EV_GUI_LButtonUp, x, y);
		else Post(EV_GUI_LButtonUp, pressX, pressY);
		down = dragging = false;
	}
	shown = true;
	dotX = x;
	dotY = y;
}

// A round dot: a dark rim, then the colour, in one-pixel rows.
static void Disc(int cx, int cy, int r, PalEntry color)
{
	for (int dy = -r; dy <= r; dy++)
	{
		const int w = (int)sqrt((double)(r * r - dy * dy));
		twod->AddColorOnlyQuad(cx - w, cy + dy, 2 * w + 1, 1, color);
	}
}

void VRPointer_Draw()
{
	if (!shown || menuactive == MENU_Off) return;
	int cw = 0, ch = 0;
	VR_Get2DMetrics(&cw, &ch, nullptr, nullptr);
	const int r = max(4, ch / 120);
	Disc(dotX, dotY, r + max(2, r / 3), PalEntry(255, 0, 0, 0));
	// Red while pressed.
	Disc(dotX, dotY, r, down ? PalEntry(255, 255, 40, 40) : PalEntry(255, 3, 193, 203));
}
