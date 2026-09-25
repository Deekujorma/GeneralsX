#pragma once

#include "Common/SubsystemInterface.h"

// GeneralsX @feature OpenAI 25/09/2026 Pure, canonical HUD geometry transformations shared by runtime code and tests.
inline Real ClampControlBarScale(Real scale)
{
	return clamp(0.60f, scale, 1.00f);
}

inline Int ScaleControlBarCoordinate(Int value, Int pivot, Real scale)
{
	return pivot + REAL_TO_INT((value - pivot) * ClampControlBarScale(scale));
}

inline Int ScaleControlBarExtent(Int value, Real scale)
{
	return REAL_TO_INT(value * ClampControlBarScale(scale));
}

inline Int CalculateControlBarArtworkOffset(Int currentMarker, Int canonicalMarker, Real scale)
{
	// The marker has already moved with the scaled windows. Add only the canonical-origin correction.
	return currentMarker - REAL_TO_INT(canonicalMarker * ClampControlBarScale(scale));
}

inline Real CalculateControlBarViewportScale(Real baseScale, Real hudScale)
{
	return 1.0f - (1.0f - baseScale) * ClampControlBarScale(hudScale);
}
