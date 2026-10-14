#pragma once

// Dedicated S0 command-classification diagnostic; never called by production
// profiles. It does not mount MicroSD and does not persist captured audio.
void sticky_voice_classifier_test_run();
