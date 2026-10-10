/*
	Desk proof of the observer view's cut-out (vr/RazeXR/ObserverCrop.h).

	    cl /EHsc /O2 /I vr\RazeXR tools\observer-crop-test.cpp

	Checks, against a Quest 3-shaped eye with real, uneven tangents:
	  - the horizon is level: at the cut-out's middle, its own left-right
	    looks along the world's horizontal, with the head rolled and pitched;
	  - every corner stays inside the eye;
	  - a 2 degree, 6 Hz nod reaches the view at well under half strength,
	    standing still and while turning at 20 degrees a second;
	  - a turn is caught up within a second of the head stopping.
	Prints each measurement and exits 1 if any check fails.
*/

#include "ObserverCrop.h"

#include <stdio.h>

using namespace observer;

static const float kDeg = (float)M_PI / 180.0f;

// Yaw about +Y, then pitch about +X, then roll about -Z (the view's forward).
static Q FromYawPitchRoll(float yawDeg, float pitchDeg, float rollDeg)
{
	auto axis = [](float x, float y, float z, float deg) {
		const float h = deg * kDeg * 0.5f, s = sinf(h);
		return Q{ x * s, y * s, z * s, cosf(h) };
	};
	auto mul = [](Q a, Q b) {
		return Q{ a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
				  a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
				  a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
				  a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z };
	};
	return mul(mul(axis(0, 1, 0, yawDeg), axis(1, 0, 0, pitchDeg)), axis(0, 0, 1, rollDeg));
}

static Eye MakeEye(Q q)
{
	Eye e;
	e.q = q;
	e.l = tanf(-0.942f); e.r = tanf(0.698f);	// a Quest 3 left eye through VDXR
	e.u = tanf(0.768f); e.d = tanf(-0.890f);
	e.W = 3072; e.H = 3264;
	return e;
}

static float Elevation(V3 d) { return asinf(Clampf(d.y, -1, 1)) / kDeg; }
static float Bearing(V3 d) { return atan2f(d.x, -d.z) / kDeg; }

// Where the middle of the cut-out looks, in the world.
static V3 CentreDir(const Eye& e, const Crop& c)
{
	float x = 0, y = 0;
	for (int i = 0; i < 4; i++) { x += c.x[i] * 0.25f; y += c.y[i] * 0.25f; }
	return Unproject(e, x * e.W, y * e.H);
}

static int failures = 0;
static void Check(bool ok, const char* what) { printf("  %s  %s\n", ok ? "ok  " : "FAIL", what); if (!ok) failures++; }

int main()
{
	const float aspect = 16.0f / 9.0f;
	const double dt = 1.0 / 90.0;

	printf("Horizon and corners, settled on a still head:\n");
	float worstTilt = 0, worstOut = 0;
	for (float roll = -25; roll <= 25; roll += 12.5f)
		for (float pitch = -35; pitch <= 35; pitch += 17.5f)
		{
			Steady st;
			const Eye e = MakeEye(FromYawPitchRoll(30, pitch, roll));
			Crop c{};
			for (int f = 0; f < 180; f++) c = st.Update(e, aspect, f * dt, false);
			// A short step each way from the middle along the cut-out's own
			// right: level means the two look out at the same height. (The
			// far ends of the row are not equally far round from the middle
			// in a flat projection, so they are not the test.)
			float cx = 0, cy = 0;
			for (int i = 0; i < 4; i++) { cx += c.x[i] * 0.25f; cy += c.y[i] * 0.25f; }
			float ux = (c.x[1] - c.x[0]) * e.W, uy = (c.y[1] - c.y[0]) * e.H;
			const float ul = sqrtf(ux * ux + uy * uy); ux /= ul; uy /= ul;
			const float step = 0.01f * e.W;
			const V3 a = Unproject(e, cx * e.W - ux * step, cy * e.H - uy * step);
			const V3 b = Unproject(e, cx * e.W + ux * step, cy * e.H + uy * step);
			const float across = acosf(Clampf(Dot(a, b), -1, 1)) / kDeg;
			const float tilt = atan2f(fabsf(Elevation(a) - Elevation(b)), across) / kDeg;
			if (tilt > worstTilt) worstTilt = tilt;
			for (int i = 0; i < 4; i++)
			{
				const float out = fmaxf(fmaxf(-c.x[i], c.x[i] - 1), fmaxf(-c.y[i], c.y[i] - 1));
				if (out > worstOut) worstOut = out;
			}
		}
	printf("  worst horizon tilt %.3f deg, worst corner outside %.5f\n", worstTilt, worstOut);
	Check(worstTilt < 0.3f, "horizon level within 0.3 degrees");
	Check(worstOut < 0.001f, "every corner inside the eye");

	// A nod: 2 degrees at 6 Hz on the pitch, at rest and while turning.
	for (int turning = 0; turning < 2; turning++)
	{
		Steady st;
		const float rate = turning ? 20.0f : 0.0f;
		float lo = 1e9f, hi = -1e9f;
		for (int f = 0; f < 540; f++)
		{
			const double t = f * dt;
			const float nod = 2.0f * sinf((float)(2 * M_PI * 6 * t));
			const Eye e = MakeEye(FromYawPitchRoll((float)(rate * t), nod, 0));
			const Crop c = st.Update(e, aspect, t, false);
			if (t > 3.0)	// settled
			{
				const float el = Elevation(CentreDir(e, c));
				lo = fminf(lo, el); hi = fmaxf(hi, el);
			}
		}
		const float share = (hi - lo) / 4.0f;	// the head swings 4 degrees end to end
		printf("  nod reaching the view %s: %.0f%%\n", turning ? "while turning 20 deg/s" : "at rest", share * 100);
		Check(share < (turning ? 0.5f : 0.25f), turning ? "nod damped while turning" : "nod damped at rest");
	}

	// A 40 degree turn at 40 degrees a second, then still: caught up in a second.
	{
		Steady st;
		float lag = 0;
		for (int f = 0; f < 270; f++)
		{
			const double t = f * dt;
			const float yaw = (float)(t < 1.0 ? 40 * t : 40);
			const Eye e = MakeEye(FromYawPitchRoll(yaw, 0, 0));
			const Crop c = st.Update(e, aspect, t, false);
			if (t > 1.0 && t <= 2.0) lag = fabsf(Bearing(CentreDir(e, c)) - Bearing(Unproject(e, 0.5f * e.W, 0.5f * e.H)));
		}
		// The eye's middle is not its forward axis (uneven tangents); compare
		// against where the forward axis lands instead.
		Steady still;
		const Eye e = MakeEye(FromYawPitchRoll(40, 0, 0));
		Crop c{};
		for (int f = 0; f < 270; f++) c = still.Update(e, aspect, f * dt, false);
		const float settled = fabsf(Bearing(CentreDir(e, c)) - Bearing(Unproject(e, 0.5f * e.W, 0.5f * e.H)));
		printf("  one second after the turn stops: %.2f deg from settled\n", fabsf(lag - settled));
		Check(fabsf(lag - settled) < 1.0f, "turn caught up within a second");
	}

	printf(failures ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", failures);
	return failures ? 1 : 0;
}
