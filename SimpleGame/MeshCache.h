#pragma once
#include "RenderTypes.h"
#include <array>

// Versioned shared topology; per-instance positions/colors never enter the disk cache.
std::array<Point, 6> LoadUnitQuad();
