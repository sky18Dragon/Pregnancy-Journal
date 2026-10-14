#include <cassert>

#include "refresh_policy.h"

int main() {
  PageRefreshTracker tracker;
  assert(tracker.needs_full(0));          // first frame
  assert(!tracker.needs_full(0));         // same page: partial
  assert(!tracker.needs_full(0));
  assert(tracker.needs_full(1));          // page change: full
  assert(!tracker.needs_full(1));
  assert(tracker.needs_full(1, true));    // forced (resume, orientation)
  assert(!tracker.needs_full(1));
  assert(tracker.needs_full(0));          // back to the first page: full again
  tracker.reset();
  assert(tracker.needs_full(0));          // reset forgets the page
  assert(tracker.needs_full(7, true));    // force also records the new page
  assert(!tracker.needs_full(7));
  return 0;
}
