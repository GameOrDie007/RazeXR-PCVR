#pragma once

#include "matrix.h"

class DFrameBuffer;

enum
{
	VR_MONO = 0,
	VR_GREENMAGENTA = 1,
	VR_REDCYAN = 2,
	VR_SIDEBYSIDEFULL = 3,
	VR_SIDEBYSIDESQUISHED = 4,
	VR_LEFTEYEVIEW = 5,
	VR_RIGHTEYEVIEW = 6,
	VR_QUADSTEREO = 7,
	VR_SIDEBYSIDELETTERBOX = 8,
	VR_AMBERBLUE = 9,
	VR_TOPBOTTOM = 11,
	VR_ROWINTERLEAVED = 12,
	VR_COLUMNINTERLEAVED = 13,
	VR_CHECKERINTERLEAVED = 14,
	VR_OPENXR = 15
};

struct VREyeInfo
{
	float mShiftFactor;
	float mScaleFactor;

	VSMatrix GetCenterProjection(float fov, float aspectRatio, float fovRatio) const;
	VSMatrix GetStereoProjection(float fov, float aspectRatio, float fovRatio) const;
	DVector3 GetViewShift(FRotator angles) const;
	VSMatrix GetHUDProjection(int width, int height) const;
	VSMatrix GetMenuProjection(int width, int height) const;
	VSMatrix GetPlayerSpriteProjection(int width, int height) const;

private:
	float getShift() const;
	int getEye() const;

    float getStereoSeparation(double stereoLevel) const;
};

struct VRMode
{
	int mEyeCount;
	float mHorizontalViewportScale;
	float mVerticalViewportScale;
	float mWeaponProjectionScale;
	VREyeInfo mEyes[2];

	static const VRMode *GetVRMode(bool toscreen = true);
	void AdjustViewport(DFrameBuffer *fb) const;
};

// PCVR port: a menu opened inside a level is drawn as a panel in the world.
// True only while that is happening - the compositor, the 2D projection and
// the scissor test all key off it, so they agree by construction.
bool VR_MenuInWorld();
void VR_MenuAnchorUpdate();
// Hidden for a screenshot: the 2D layer is not drawn while a menu is up.
bool VR_MenuHidden();
float VR_MenuScale();
float VR_MenuDistance();
float VR_MenuDepth();

/*
	The shape of the panel, width over height.

	The panel's texture is a square, and the whole 2D screen is painted across
	it - so a 16:9 screen is squeezed to 1:1 on the way in, and an ultrawide
	3440x1440 is squeezed by 2.39. A square quad then shows that squeeze, and
	everything on the panel stands too tall and too narrow. Reported by Welz,
	12 September 2026: "the game menus were in a funny resolution (very tall
	and thin)", with forcing the game to 4:3 as the workaround - which is
	exactly what you would expect, because 4:3 is the aspect closest to the
	square the panel actually is.

	This is the aspect of the pixels that went in, recorded during the paint,
	so making the quad this shape undoes precisely the squeeze and nothing
	else. The quad keeps the height it has always had - the size signed off in
	the headset - and grows sideways, because a panel that keeps its text size
	is easier to read than one that shrinks to fit a fixed width.

	Zero until the first paint, and the caller falls back to square.
*/
void VR_Set2DMetrics(int canvasW, int canvasH, int viewW, int viewH);
float VR_MenuAspect();

// The same numbers, for the one-shot diagnostic in the virtual screen layer.
// See VR_MenuAspect for why they are worth having.
void VR_Get2DMetrics(int* canvasW, int* canvasH, int* viewW, int* viewH);

/*
	Set while the pause panel's own texture is being painted.

	The panel is a compositor quad, and the quad is what puts it in the world.
	The texture behind it must therefore be painted flat - a plain ortho, the
	same one the main menu uses. Painting it with GetMenuProjection as well
	placed the menu twice: the quad hung still while the picture inside it
	slid, tilted and receded with the head, which is exactly what a panel
	stuck to your face looks like.
*/
void VR_SetMenuLayerPainting(bool on);
bool VR_MenuLayerPainting();
void VR_MenuSetHidden(bool hidden);
