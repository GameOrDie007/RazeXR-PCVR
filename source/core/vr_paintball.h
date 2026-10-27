#pragma once

class DCoreActor;

// PC branch: Paintball (VR Options > Hands and Weapons). Off, Modern or Classic.
bool VR_Paintball();
bool VR_IsPaintTile(int tile);
// Turns a mark a shot has just left on a wall into a paint splat, in place.
// Returns false, leaving the mark as it was, when Paintball is off.
bool VR_PaintMark(DCoreActor* mark);
