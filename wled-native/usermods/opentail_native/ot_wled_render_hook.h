#pragma once
#include <cstddef>
#include <cstdint>

// Called from the pinned WLED frame compositor after native segment blending
// and before WLED gamma/color correction and bus mapping. Colors use WLED's
// packed RGBW32 representation, matching the compositor's logical frame.
extern "C" void opentailNativePostRender(uint32_t* pixels, std::size_t count);
