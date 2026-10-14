#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <unistd.h>

#include "sd_audio_usage.h"

namespace fs = std::filesystem;

int main()
{
    using namespace sticky_storage;
    assert(sd_audio_usage_probe(nullptr).status ==
           SdAudioUsageStatus::InvalidArgument);

    char temporary[] = "/tmp/sticky_audio_usage_XXXXXX";
    char *created = ::mkdtemp(temporary);
    assert(created != nullptr);
    const fs::path root(created);
    const fs::path missing = root / "MISSING";
    assert(sd_audio_usage_probe(missing.c_str()).status ==
           SdAudioUsageStatus::Missing);

    const fs::path audio = root / "AUDIO";
    fs::create_directory(audio);
    {
        std::ofstream(audio / "AUD00001.WAV", std::ios::binary)
            << "1234567890";
        std::ofstream(audio / "AUD00002.TMP", std::ios::binary)
            << "12345";
    }
    SdAudioUsageProbe usage = sd_audio_usage_probe(audio.c_str());
    assert(usage.status == SdAudioUsageStatus::Ready);
    assert(usage.items == 2U && usage.bytes == 15U);

    std::ofstream(audio / "lower.wav", std::ios::binary) << "x";
    usage = sd_audio_usage_probe(audio.c_str());
    assert(usage.status == SdAudioUsageStatus::UnexpectedEntry);
    assert(usage.items == 0U && usage.bytes == 0U);
    fs::remove(audio / "lower.wav");

    fs::create_directory(audio / "NESTED.WAV");
    usage = sd_audio_usage_probe(audio.c_str());
    assert(usage.status == SdAudioUsageStatus::UnexpectedEntry);
    fs::remove_all(root);
    return 0;
}
