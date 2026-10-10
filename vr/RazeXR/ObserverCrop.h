#pragma once

/*
	PC branch: the observer view's cut-out - where in the left eye's picture
	the desktop window's picture comes from. Maths only, no GL and no engine,
	so tools/observer-crop-test.cpp can prove it at the desk. Drawn by
	ObserverView.cpp.

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation; either version 2 of the License, or (at your option)
	any later version.
*/

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//==========================================================================
//
// The cut-out: where in the eye picture the window's picture comes from.
//
// Fractions of the eye picture, x right and y DOWN from its top left; the
// corners are the window's top left, top right, bottom right, bottom left.
//
//==========================================================================

namespace observer {

struct Q { float x, y, z, w; };	// an orientation, as OpenXR gives it


struct V3 { float x, y, z; };

inline V3 Add(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 Sub(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 Mul(V3 a, float k) { return { a.x * k, a.y * k, a.z * k }; }
inline float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 Cross(V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline float Len(V3 a) { return sqrtf(Dot(a, a)); }
inline V3 Norm(V3 a) { const float l = Len(a); return l > 1e-9f ? Mul(a, 1.0f / l) : V3{ 0, 0, -1 }; }
inline float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline V3 Rotate(const Q& q, V3 v)
{
	const V3 u = { q.x, q.y, q.z };
	const V3 t = Mul(Cross(u, v), 2.0f);
	return Add(Add(v, Mul(t, q.w)), Cross(u, t));
}

inline V3 Unrotate(const Q& q, V3 v)
{
	const Q c = { -q.x, -q.y, -q.z, q.w };
	return Rotate(c, v);
}

struct Eye
{
	Q q;
	float l, r, u, d;	// tangents of the field of view
	float W, H;			// the picture, in pixels
};

// Where a direction in the tracking space lands in the picture, in pixels.
inline bool Project(const Eye& e, V3 dir, float& px, float& py)
{
	const V3 v = Unrotate(e.q, dir);
	if (v.z > -1e-4f) return false;		// behind: the view looks down -Z
	const float tx = v.x / -v.z, ty = v.y / -v.z;
	px = (tx - e.l) / (e.r - e.l) * e.W;
	py = (e.u - ty) / (e.u - e.d) * e.H;
	return true;
}

// The direction a pixel of the picture shows.
inline V3 Unproject(const Eye& e, float px, float py)
{
	const float tx = e.l + px / e.W * (e.r - e.l);
	const float ty = e.u - py / e.H * (e.u - e.d);
	return Norm(Rotate(e.q, V3{ tx, ty, -1.0f }));
}

/*
	The world's horizontal through a point, as the picture draws it: nudge
	the point along the horizontal both ways and see which way it moves on
	the picture. Levelling the horizon, not "up": away from the middle of the
	picture the two are not at right angles. Straight up or down there is no
	horizontal, and the last angle is kept.
*/
inline float HorizonAt(const Eye& e, float px, float py, float fallback)
{
	const V3 d = Unproject(e, px, py);
	V3 h = Cross(d, V3{ 0, 1, 0 });
	if (Len(h) < 0.05f) return fallback;
	h = Mul(Norm(h), 0.002f);
	float ax, ay, bx, by;
	if (!Project(e, Add(d, h), ax, ay) || !Project(e, Sub(d, h), bx, by)) return fallback;
	return atan2f(ay - by, ax - bx);
}

// How far a first-order filter at hz moves in dt.
inline float Alpha(float hz, double dt)
{
	const double tau = 1.0 / (2.0 * M_PI * (hz > 1e-3f ? hz : 1e-3f));
	return (float)(1.0 / (1.0 + tau / dt));
}

struct Crop { float x[4], y[4]; };

/*
	Follows where the head points with a speed-adaptive (one-euro) filter:
	heavy at rest, light while turning, so a slow turn is not lagged off the
	edge of the eye and a wobble is not let through. The turn rate is filtered
	as a vector before its size is taken - the size of a wobble's rate is
	always positive, and filtered on its own it reads as a steady turn. A jump
	of more than 45 degrees (a recenter, a snap turn) is taken at once.
*/
struct Steady
{
	bool has = false;
	V3 followed = { 0, 0, -1 }, lastHead = { 0, 0, -1 }, rate = { 0, 0, 0 };
	double lastT = 0;
	float lastRoll = 0;

	const float widthFraction = 0.78f;
	const float minCutoffHz = 0.5f, beta = 2.0f, rateCutoffHz = 0.5f, jumpDeg = 45.0f;

	Crop Update(const Eye& e, float outAspect, double seconds, bool plain)
	{
		const V3 head = Norm(Rotate(e.q, V3{ 0, 0, -1 }));
		float cx = e.W * 0.5f, cy = e.H * 0.5f, roll = 0;

		if (plain)
		{
			has = false;	// steady picks the head up afresh when it resumes
			float px, py;
			if (Project(e, head, px, py)) { cx = px; cy = py; }
		}
		else
		{
			double dt = seconds - lastT;
			dt = dt < 1e-4 ? 1e-4 : (dt > 0.1 ? 0.1 : dt);
			if (!has || Dot(head, followed) < cosf(jumpDeg * (float)M_PI / 180.0f))
			{
				followed = head;
				rate = { 0, 0, 0 };
			}
			else
			{
				const V3 raw = Mul(Sub(head, lastHead), (float)(1.0 / dt));
				rate = Add(rate, Mul(Sub(raw, rate), Alpha(rateCutoffHz, dt)));
				const float cutoff = minCutoffHz + beta * Len(rate);
				followed = Norm(Add(followed, Mul(Sub(head, followed), Alpha(cutoff, dt))));
			}
			has = true;
			lastHead = head;
			lastT = seconds;

			float px, py;
			if (!Project(e, followed, px, py))
			{
				followed = head;
				Project(e, head, px, py);
			}
			cx = px; cy = py;
		}

		/*
			The window's shape, turned to the horizon, shrunk if its turned
			corners would leave the eye, then kept inside it. Held at the edge,
			the followed direction becomes the one drawn, or it winds up past
			the edge and the view stays pinned there after the head stops.
		*/
		float cw = widthFraction * e.W, ch = cw / outAspect;
		if (ch > e.H) { ch = e.H; cw = ch * outAspect; }
		const float cw0 = cw, ch0 = ch;
		for (int pass = 0; pass < 3; pass++)
		{
			if (!plain) roll = HorizonAt(e, cx, cy, lastRoll);
			const float c = fabsf(cosf(roll)), s = fabsf(sinf(roll));
			float k = 1.0f;
			k = fminf(k, e.W / (cw0 * c + ch0 * s));
			k = fminf(k, e.H / (cw0 * s + ch0 * c));
			cw = cw0 * k; ch = ch0 * k;
			const float hx = 0.5f * (cw * c + ch * s), hy = 0.5f * (cw * s + ch * c);
			const float kx = Clampf(cx, hx, e.W - hx), ky = Clampf(cy, hy, e.H - hy);
			const bool moved = fabsf(kx - cx) > 0.25f || fabsf(ky - cy) > 0.25f;
			cx = kx; cy = ky;
			if (!moved) break;
			if (!plain) followed = Unproject(e, cx, cy);
		}
		if (!plain) lastRoll = roll;

		const float rc = cosf(roll), rs = sinf(roll);
		const float axx = rc * 0.5f * cw, axy = rs * 0.5f * cw;		// to the right edge
		const float ayx = -rs * 0.5f * ch, ayy = rc * 0.5f * ch;	// to the bottom edge
		Crop out;
		const float px[4] = { cx - axx - ayx, cx + axx - ayx, cx + axx + ayx, cx - axx + ayx };
		const float py[4] = { cy - axy - ayy, cy + axy - ayy, cy + axy + ayy, cy - axy + ayy };
		for (int i = 0; i < 4; i++) { out.x[i] = px[i] / e.W; out.y[i] = py[i] / e.H; }
		return out;
	}
};

} // namespace observer
