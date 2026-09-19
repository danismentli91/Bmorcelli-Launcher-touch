#pragma once
#include <stdint.h>

namespace waveshare183 {
enum class Action { None, Select, Back, Next, Previous };
struct Point { int16_t x, y; };

inline Point rotateTouch(Point p, uint8_t rotation) {
    switch (rotation & 3) {
    case 1: return {p.y, int16_t(239 - p.x)};
    case 2: return {int16_t(239 - p.x), int16_t(283 - p.y)};
    case 3: return {int16_t(283 - p.y), p.x};
    default: return p;
    }
}

// No Arduino dependencies: exercise the actual gesture logic in host tests.
// Unsigned elapsed times also work across millis() rollover.
class InputState {
public:
    static constexpr uint32_t doubleMs = 300;
    explicit InputState(bool touch) : touch_(touch) {}

    void cancel() { pending_ = false; suppressed_ = down_; }
    bool down() const { return down_; }

    Action update(uint32_t now, bool pressed, Point p = {0, 0}, bool suppress = false) {
        Action result = Action::None;
        if (suppress) { pending_ = false; suppressed_ = true; }
        // A second press owns the pending click until it is released. Never
        // emit a single while that second press is still held.
        if (!down_ && pending_ && uint32_t(now - releasedAt_) >= doubleMs) {
            pending_ = false;
            result = touch_ ? Action::Select : Action::Next;
        }
        if (pressed && !down_) {
            down_ = true;
            downAt_ = now;
            start_ = p;
            last_ = p;
            moved_ = false;
            second_ = pending_;
        } else if (pressed && down_) {
            last_ = p;
            int dx = p.x - start_.x, dy = p.y - start_.y;
            if (dx > 30 || dx < -30 || dy > 30 || dy < -30) moved_ = true;
        } else if (!pressed && down_) {
            down_ = false;
            const uint32_t held = now - downAt_;
            if (suppressed_) {
                suppressed_ = false;
                pending_ = false;
                return result;
            }
            if (touch_ && moved_) {
                pending_ = false;
                int dx = last_.x - start_.x, dy = last_.y - start_.y;
                int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
                if (ax < 30 && ay < 30) return result;
                return (ay >= ax ? dy < 0 : dx < 0) ? Action::Next : Action::Previous;
            }
            if (held >= (touch_ ? 700U : 550U)) {
                pending_ = false;
                return touch_ || held >= 1200 ? Action::Back : Action::Select;
            }
            if (second_) {
                pending_ = false;
                return touch_ ? Action::Back : Action::Previous;
            }
            pending_ = true;
            releasedAt_ = now;
        }
        return result;
    }

private:
    bool touch_, down_ = false, pending_ = false, second_ = false;
    bool suppressed_ = false, moved_ = false;
    uint32_t downAt_ = 0, releasedAt_ = 0;
    Point start_{0, 0}, last_{0, 0};
};

class Debounce {
public:
    bool update(uint32_t now, bool raw) {
        if (raw != raw_) { raw_ = raw; changedAt_ = now; }
        if (uint32_t(now - changedAt_) >= 25) stable_ = raw_;
        return stable_;
    }
private:
    bool raw_ = false, stable_ = false;
    uint32_t changedAt_ = 0;
};
} // namespace waveshare183
