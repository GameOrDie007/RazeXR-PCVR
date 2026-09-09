//---------------------------------------------------------------------------
//
// The RazeXR PCVR splash, shown once at startup ahead of whatever the game
// itself opens with.
//
// ImageScreen is not reused for this. It draws with FSMode_ScaleToFit43, which
// is right for the games' own 320x200 art and wrong for a 16:9 plate - it would
// stretch this one to 4:3. Everything else about it is what we want, so this is
// the same job with one draw call changed.
//
//---------------------------------------------------------------------------

class VRSplashScreen : SkippableScreenJob
{
	int waittime;			// in ms
	bool cleared;
	TextureID texid;

	ScreenJob Init(String tex, int fade, int wait)
	{
		Super.Init(fade);
		waittime = wait;
		texid = TexMan.CheckForTexture(tex, TexMan.Type_Any, TexMan.TryAny | TexMan.ForceLookup);
		cleared = false;
		return self;
	}

	override void OnTick()
	{
		if (cleared)
		{
			int span = ticks * 1000 / GameTicRate;
			if (span > waittime) jobstate = finished;
		}
	}

	override void Draw(double smooth)
	{
		if (texid.IsValid())
		{
			Screen.DrawTexture(texid, true, 0, 0,
				DTA_FullscreenEx, FSMode_ScaleToFit,
				DTA_LegacyRenderStyle, STYLE_Normal);
		}
		cleared = true;
	}
}

class VRSplash ui   // must be a ui class, and a class not a struct, so
{                   // CallCreateFunction can look the method up from C++
	// Named by PlayLogos. A missing or unreadable image must not cost anyone
	// their intro, so an invalid texture appends nothing at all rather than
	// holding a black screen for three seconds.
	static void Create(ScreenJobRunner runner)
	{
		let job = new("VRSplashScreen").Init("graphics/vrsplash.jpg",
			ScreenJob.fadein | ScreenJob.fadeout, 2600);
		let splash = VRSplashScreen(job);
		if (splash && splash.texid.IsValid()) runner.Append(job);
		else Console.Printf("VR splash: graphics/vrsplash.jpg not found, skipped");
	}
}
