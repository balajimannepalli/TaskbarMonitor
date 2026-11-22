#include "Placement.h"
#include <algorithm>

namespace prototype {
std::optional<RECT> placement(RECT bar, RECT monitor, UINT dpi, int inset, bool right,int widthDip,int heightDip,bool centered) {
    if (!dpi || bar.left < monitor.left || bar.top < monitor.top ||
        bar.right > monitor.right || bar.bottom > monitor.bottom) return {};

    const LONG width = MulDiv(widthDip, dpi, 96);
    const LONG height = MulDiv(heightDip, dpi, 96);
    const LONG freeWidth = bar.right - bar.left - width;
    const LONG freeHeight = bar.bottom - bar.top - height;
    if (freeWidth < 0 || freeHeight < 0 || bar.right - bar.left <= bar.bottom - bar.top)
        return {};
    const LONG offset = std::clamp<LONG>(MulDiv(inset, dpi, 96), 0, freeWidth);
    const LONG x = centered ? bar.left + freeWidth/2 : right ? bar.right - width - offset : bar.left + offset;
    const LONG y = bar.top + freeHeight / 2;
    return RECT{x, y, x + width, y + height};
}
}
