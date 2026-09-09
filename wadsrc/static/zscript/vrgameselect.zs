/*
	The Switch Game menu, with the highlighted game's cover beside the list.

	PC branch. The list itself is built in C++ by BuildVRGameSelectMenu, one
	row per launcher found beside raze.exe; this only adds the picture. The
	cover for a row is asked for by index rather than derived from the row's
	text, because the menu shows the name grpinfo gave the game and the image
	is named after the launcher file - "BLOOD: Cryptic Passage" against
	"BLOOD - Cryptic Passage VR".

	Covers live in boxart.pk3 under boxart/, which the launchers load when it
	is present. A missing pack, or a game with no cover, draws nothing and
	leaves an ordinary text menu - so this degrades to what it replaced rather
	than to an error.

	Copyright (C) 2026 RazeXR PCVR port
	GPL-2.0-or-later, see package/common/gpl-2.0.txt.
*/

class VRGameSelectMenu : OptionMenu
{
	// Fraction of the screen the cover occupies, and its margin from the right
	// edge. The list is drawn around the centre, so the right quarter is free.
	const COVER_HEIGHT = 0.62;
	const COVER_MARGIN = 0.04;
	const COVER_ASPECT = 2.0 / 3.0;	// every cover is 600x900

	TextureID lastTex;
	int lastItem;

	override void Init(Menu parent, OptionMenuDescriptor desc)
	{
		Super.Init(parent, desc);
		lastItem = -1;
	}

	/*
		Looked up once per change of selection rather than once per frame.
		CheckForTexture on a path builds the texture the first time it is
		asked for, and this menu is redrawn ninety times a second.
	*/
	TextureID CoverFor(int item)
	{
		if (item == lastItem) return lastTex;
		lastItem = item;
		lastTex.SetInvalid();

		// Already resolved, alias and all - see VRLaunchers_BoxartForItem.
		String path = Raze.VRBoxartName(item);
		if (path.Length() > 0)
		{
			lastTex = TexMan.CheckForTexture(path, TexMan.Type_Any);
		}
		return lastTex;
	}

	override void Drawer()
	{
		Super.Drawer();

		TextureID tex = CoverFor(mDesc.mSelectedItem);
		if (!tex.IsValid()) return;

		int sw = Screen.GetWidth();
		int sh = Screen.GetHeight();

		double h = sh * COVER_HEIGHT;
		double w = h * COVER_ASPECT;
		double x = sw - w - sw * COVER_MARGIN;
		double y = (sh - h) * 0.5;

		// A dark plate a little larger than the cover, so the art has an edge
		// against whatever is behind it rather than floating on the scene.
		double b = sh * 0.008;
		Screen.Dim(0x000000, 0.6, int(x - b), int(y - b), int(w + b * 2), int(h + b * 2));

		Screen.DrawTexture(tex, false, x, y,
			DTA_DestWidthF, w,
			DTA_DestHeightF, h);
	}
}
