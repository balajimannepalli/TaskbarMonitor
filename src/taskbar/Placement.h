#pragma once
#include <windows.h>
#include <optional>

namespace prototype {
// Input rectangles use virtual-screen physical pixels. The inset is in DIPs.
std::optional<RECT> placement(RECT bar, RECT monitor, UINT dpi, int inset, bool right,
                              int widthDip=100, int heightDip=22, bool centered=false);
}
