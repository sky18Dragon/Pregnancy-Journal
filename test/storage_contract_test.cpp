#include <cassert>

#include "storage_contract.h"

int main()
{
    using namespace sticky_storage;
    assert(kWorstCaseRecordingBytes == 960044ULL);
    assert(can_start_audio_recording(0U, 0U,
                                     kBusinessReserveBytes +
                                         kWorstCaseRecordingBytes));
    assert(!can_start_audio_recording(kAudioItemLimit, 0U, UINT64_MAX));
    assert(!can_start_audio_recording(0U, kAudioByteLimit, UINT64_MAX));
    assert(!can_start_audio_recording(0U, 0U,
                                      kBusinessReserveBytes +
                                          kWorstCaseRecordingBytes - 1U));
}
