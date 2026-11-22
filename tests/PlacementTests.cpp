#include "taskbar/Placement.h"
#include <iostream>

int main() {
    const RECT primary{0, 0, 1920, 1080};
    const RECT bottom{0, 1032, 1920, 1080};
    const RECT secondary{-2560, -200, 0, 1240};
    const RECT top{-2560, -200, 0, -128};
    int failures = 0;
    auto check = [&](const char* name, RECT bar, RECT monitor, UINT dpi, int inset, bool right,
                     std::optional<RECT> expected) {
        const auto actual = prototype::placement(bar, monitor, dpi, inset, right);
        if (actual.has_value() != expected.has_value() ||
            (actual && !EqualRect(&*actual, &*expected))) {
            std::cerr << name << " failed\n";
            ++failures;
        }
    };
    check("100 percent left", bottom, primary, 96, 192, false, RECT{192,1045,292,1067});
    check("125 percent right", bottom, primary, 120, 16, true, RECT{1775,1042,1900,1070});
    check("150 percent negative origin", top, secondary, 144, 192, false, RECT{-2272,-181,-2122,-148});
    check("200 percent clamped", bottom, primary, 192, 16384, false, RECT{1720,1034,1920,1078});
    check("sliding off screen", RECT{0,1078,1920,1126}, primary, 96, 0, false, {});
    check("hidden sliver", RECT{0,1078,1920,1080}, primary, 96, 0, false, {});
    check("vertical", RECT{0,0,48,1080}, primary, 96, 0, false, {});
    check("too narrow", RECT{0,0,90,42}, primary, 96, 0, false, {});
    check("no dpi", bottom, primary, 0, 0, false, {});
    const auto middle=prototype::placement(top,secondary,144,352,false,240,32,true);
    if(!middle || middle->left!=-1460 || middle->right!=-1100) ++failures;
    check("flush left",bottom,primary,96,0,false,RECT{0,1045,100,1067});
    check("before notification area",RECT{0,1032,1500,1080},primary,96,0,true,RECT{1400,1045,1500,1067});
    return failures ? 1 : 0;
}
