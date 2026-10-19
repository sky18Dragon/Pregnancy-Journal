#include "side_key_tracker.h"

bool SideKeyTracker::on_edge(SideKey key, bool pressed, int64_t at_us,
                             SideKeyEvent &event)
{
    KeyState &self = keys_[index_of(key)];
    KeyState &other = keys_[1U - index_of(key)];

    if (pressed) {
        if (self.down) {
            return false;
        }
        self.down = true;
        self.pressed_at_us = at_us;
        self.tainted = other.down;
        if (other.down) {
            other.tainted = true;
        }
        return false;
    }

    if (!self.down) {
        return false;
    }
    self.down = false;
    if (!keys_[0].down || !keys_[1].down) {
        chord_latched_ = false;
    }

    const int64_t held_us = at_us - self.pressed_at_us;
    if (self.tainted || held_us >= kSingleMaxHoldUs) {
        return false;
    }
    event.type = SideKeyEventType::Single;
    event.side = key;
    event.at_us = at_us;
    return true;
}

bool SideKeyTracker::poll(int64_t now_us, SideKeyEvent &event)
{
    if (!keys_[0].down || !keys_[1].down || chord_latched_) {
        return false;
    }
    const int64_t first = keys_[0].pressed_at_us;
    const int64_t second = keys_[1].pressed_at_us;
    const int64_t gap_us = first > second ? first - second : second - first;
    const int64_t later_us = first > second ? first : second;
    if (gap_us > kChordPressWindowUs || now_us - later_us < kChordHoldUs) {
        return false;
    }
    chord_latched_ = true;
    event.type = SideKeyEventType::SleepChord;
    event.at_us = now_us;
    return true;
}
