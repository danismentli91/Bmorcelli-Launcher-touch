#include "../boards/waveshare-esp32-s3-touch-lcd-183/input_state.h"
#include <assert.h>
#include <stdio.h>
using namespace waveshare183;

int main() {
    // A tap entering Settings must yield exactly one Select after 300 ms.
    InputState tap(true);
    assert(tap.update(10, true) == Action::None);
    assert(tap.update(60, false) == Action::None);
    assert(tap.update(359, false) == Action::None);
    assert(tap.update(360, false) == Action::Select);
    assert(tap.update(361, false) == Action::None);
    assert(tap.update(900, false) == Action::None);

    // No Select is sent before or after a double tap, even if its second
    // contact straddles the first tap's deadline.
    InputState dbl(true);
    dbl.update(100, true);
    dbl.update(150, false);
    assert(dbl.update(400, true) == Action::None);
    assert(dbl.update(460, true) == Action::None);
    assert(dbl.update(480, false) == Action::Back);
    assert(dbl.update(1000, false) == Action::None);

    InputState separate(true);
    separate.update(10, true);
    separate.update(50, false);
    assert(separate.update(350, true) == Action::Select);
    assert(separate.update(400, false) == Action::None);
    assert(separate.update(700, false) == Action::Select);

    InputState wake(true);
    wake.update(100, true, {0, 0}, true);
    assert(wake.update(200, false) == Action::None);
    assert(wake.update(600, false) == Action::None);
    wake.update(700, true);
    wake.update(750, false);
    assert(wake.update(1050, false) == Action::Select);

    InputState fault(true);
    fault.update(100, true);
    fault.cancel(); // Missing release / I2C failure must not create a tap.
    assert(fault.update(400, false) == Action::None);
    assert(fault.update(900, false) == Action::None);

    InputState swipe(true);
    swipe.update(10, true, {100, 200});
    swipe.update(50, true, {100, 140});
    assert(swipe.update(80, false) == Action::Next);
    assert(swipe.update(500, false) == Action::None);
    swipe.update(600, true, {100, 100});
    swipe.update(650, true, {100, 160});
    assert(swipe.update(680, false) == Action::Previous);

    InputState boot(false);
    boot.update(100, true);
    assert(boot.update(200, false) == Action::None);
    assert(boot.update(499, false) == Action::None);
    assert(boot.update(500, false) == Action::Next);
    boot.update(1000, true);
    boot.update(1100, false);
    boot.update(1200, true);
    assert(boot.update(1250, false) == Action::Previous);
    assert(boot.update(1600, false) == Action::None);
    boot.update(2000, true);
    assert(boot.update(2550, true) == Action::None);
    assert(boot.update(2700, false) == Action::Select);
    boot.update(3000, true);
    assert(boot.update(4300, false) == Action::Back);

    // A hold after a pending short click supersedes that short click.
    boot.update(5000, true);
    boot.update(5050, false);
    boot.update(5100, true);
    assert(boot.update(5450, true) == Action::None);
    assert(boot.update(5800, false) == Action::Select);
    assert(boot.update(6200, false) == Action::None);

    InputState rollover(true);
    rollover.update(UINT32_MAX - 100, true);
    rollover.update(UINT32_MAX - 50, false);
    assert(rollover.update(248, false) == Action::None);
    assert(rollover.update(249, false) == Action::Select);

    Debounce debounce;
    assert(!debounce.update(100, true));
    assert(!debounce.update(105, false));
    assert(!debounce.update(110, true));
    assert(!debounce.update(134, true));
    assert(debounce.update(135, true));
    assert(debounce.update(200, false));
    assert(debounce.update(205, true));
    assert(debounce.update(210, false));
    assert(!debounce.update(235, false));

    Point p = rotateTouch({0, 0}, 0);
    assert(p.x == 0 && p.y == 0);
    p = rotateTouch({239, 283}, 0);
    assert(p.x == 239 && p.y == 283);
    p = rotateTouch({0, 0}, 1);
    assert(p.x == 0 && p.y == 239);
    p = rotateTouch({0, 0}, 2);
    assert(p.x == 239 && p.y == 283);
    p = rotateTouch({0, 0}, 3);
    assert(p.x == 283 && p.y == 0);
    puts("Waveshare 1.83 input regression tests passed");
}
